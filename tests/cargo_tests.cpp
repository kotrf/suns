#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>

namespace {

const suns::Planet& planet(const suns::GameState& state, suns::PlanetId id)
{
    for (const auto& candidate : state.planets) {
        if (candidate.id == id) return candidate;
    }
    assert(false);
    return state.planets.front();
}

const suns::Fleet& fleet(const suns::GameState& state, suns::FleetId id)
{
    for (const auto& candidate : state.fleets) {
        if (candidate.id == id) return candidate;
    }
    assert(false);
    return state.fleets.front();
}

bool close(double a, double b)
{
    return std::abs(a - b) < 0.000001;
}

} // namespace

int main()
{
    suns::GameState state = suns::make_demo_game();
    state.shipDesigns.push_back({
        3,
        1,
        "Mineral Hauler",
        suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive},
    });

    suns::Fleet hauler;
    hauler.id = 2;
    hauler.owner = 1;
    hauler.name = "Mineral Hauler 2";
    hauler.role = suns::FleetRole::Scout;
    hauler.design = 3;
    hauler.position = {0.0, 0.0};
    hauler.warp = 6;
    hauler.fuel = suns::ship_design_fuel_capacity(state.shipDesigns.back());
    hauler.colonists = 1000; // 10 cargo units.
    state.fleets = {hauler};

    assert(close(suns::fleet_cargo_capacity(state, state.fleets.front()), 50.0));
    assert(close(suns::fleet_cargo_used(state, state.fleets.front()), 10.0));

    const auto emptyFuelBurn = suns::fleet_fuel_change_for_distance(state, state.fleets.front(), 36.0);
    const auto firstMining = suns::projected_mineral_mining(state, state.planets.front());

    suns::PlayerOrders loadMinerals{1, {}};
    loadMinerals.orders.emplace_back(
        suns::SetFleetMineralCargoOrder{1, 2, {20.0, 10.0, 10.0}});
    const suns::TurnProcessor processor;
    const auto loaded = processor.process(state, {loadMinerals});

    const auto& loadedFleet = fleet(loaded, 2);
    assert(close(loadedFleet.minerals.ironium, 20.0));
    assert(close(loadedFleet.minerals.boranium, 10.0));
    assert(close(loadedFleet.minerals.germanium, 10.0));
    assert(close(suns::fleet_cargo_used(loaded, loadedFleet), 50.0));
    assert(close(planet(loaded, 1).minerals.ironium, 80.0 + firstMining.ironium));
    assert(close(planet(loaded, 1).minerals.boranium, 90.0 + firstMining.boranium));
    assert(close(planet(loaded, 1).minerals.germanium, 90.0 + firstMining.germanium));
    assert(suns::fleet_fuel_change_for_distance(loaded, loadedFleet, 36.0) > emptyFuelBurn);

    // A load that would overflow the shared hold is rejected atomically.
    suns::PlayerOrders overload{1, {}};
    overload.orders.emplace_back(
        suns::SetFleetMineralCargoOrder{1, 2, {21.0, 10.0, 10.0}});
    const auto rejected = processor.process(loaded, {overload});
    assert(close(fleet(rejected, 2).minerals.ironium, 20.0));

    // Unloading returns exact cargo while turn-start extraction remains in the colony.
    const auto secondMining = suns::projected_mineral_mining(loaded, planet(loaded, 1));
    suns::PlayerOrders unload{1, {}};
    unload.orders.emplace_back(
        suns::SetFleetMineralCargoOrder{1, 2, {0.0, 0.0, 0.0}});
    const auto unloaded = processor.process(loaded, {unload});
    assert(close(suns::mineral_cargo_mass(fleet(unloaded, 2).minerals), 0.0));
    assert(close(planet(unloaded, 1).minerals.ironium, 100.0 + firstMining.ironium + secondMining.ironium));
    assert(close(planet(unloaded, 1).minerals.boranium, 100.0 + firstMining.boranium + secondMining.boranium));
    assert(close(planet(unloaded, 1).minerals.germanium, 100.0 + firstMining.germanium + secondMining.germanium));

    // Minerals already aboard reduce how many colonists Load All may take.
    auto dynamic = state;
    dynamic.fleets.front().colonists = 0;
    dynamic.fleets.front().minerals = {20.0, 10.0, 10.0}; // 40/50 hold used.
    dynamic.fleets.front().destination = suns::Position{0.0, 0.0};
    dynamic.fleets.front().arrivalAction = suns::FleetArrivalAction{
        suns::FleetArrivalActionKind::LoadAllAvailable,
        1,
    };
    const auto dynamicallyLoaded = processor.process(dynamic, {});
    assert(fleet(dynamicallyLoaded, 2).colonists == 999); // keep one colonist at the source colony.

    // Waypoint mineral policies use the real surface stockpile at arrival and
    // fill only the space left in the shared hold.
    auto mineralPolicy = state;
    mineralPolicy.fleets.front().colonists = 1000; // 10/50 cargo units used.
    mineralPolicy.fleets.front().destination = suns::Position{0.0, 0.0};
    mineralPolicy.fleets.front().arrivalAction = suns::FleetArrivalAction{
        suns::FleetArrivalActionKind::LoadAllAvailable,
        1,
        suns::FleetCargoKind::Ironium,
    };
    const auto policyLoaded = processor.process(mineralPolicy, {});
    assert(close(fleet(policyLoaded, 2).minerals.ironium, 40.0));

    auto mineralUnload = policyLoaded;
    const auto surfaceBeforeUnload = planet(mineralUnload, 1).minerals.ironium;
    mineralUnload.fleets.front().destination = suns::Position{0.0, 0.0};
    mineralUnload.fleets.front().arrivalAction = suns::FleetArrivalAction{
        suns::FleetArrivalActionKind::UnloadAll,
        1,
        suns::FleetCargoKind::Ironium,
    };
    const auto policyUnloaded = processor.process(mineralUnload, {});
    assert(close(fleet(policyUnloaded, 2).minerals.ironium, 0.0));
    assert(planet(policyUnloaded, 1).minerals.ironium > surfaceBeforeUnload + 39.9);

    // The source/destination transfer primitive moves mixed cargo atomically
    // between a planetary surface and any friendly fleet at that system.
    auto transferState = suns::make_demo_game();
    transferState.shipDesigns.push_back({
        3,
        1,
        "Heavy Source",
        suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::CargoPod},
    });
    transferState.shipDesigns.push_back({
        4,
        1,
        "Receiver",
        suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive},
    });
    suns::Fleet sourceFleet;
    sourceFleet.id = 2;
    sourceFleet.owner = 1;
    sourceFleet.name = "Heavy Source 2";
    sourceFleet.design = 3;
    sourceFleet.position = {0.0, 0.0};
    sourceFleet.fuel = 100.0;
    sourceFleet.colonists = 300;
    sourceFleet.minerals = {10.0, 6.0, 4.0};
    suns::Fleet receiver = sourceFleet;
    receiver.id = 3;
    receiver.name = "Receiver 3";
    receiver.design = 4;
    receiver.colonists = 0;
    receiver.minerals = {};
    transferState.fleets = {sourceFleet, receiver};

    const auto transferControl = processor.process(transferState, {});
    suns::PlayerOrders surfaceTransfer{1, {}};
    surfaceTransfer.orders.emplace_back(suns::TransferCargoOrder{
        {1, 0},
        {0, 3},
        200,
        {5.0, 4.0, 3.0},
    });
    const auto fromSurface = processor.process(transferState, {surfaceTransfer});
    auto sourceAfterTransfer = planet(transferState, 1);
    sourceAfterTransfer.population -= 200;
    const auto expectedSourcePopulation = sourceAfterTransfer.population
        + suns::projected_population_growth(sourceAfterTransfer);
    assert(planet(fromSurface, 1).population == expectedSourcePopulation);
    assert(fleet(fromSurface, 3).colonists == 200);
    assert(close(fleet(fromSurface, 3).minerals.ironium, 5.0));
    assert(close(
        planet(fromSurface, 1).minerals.ironium + 5.0,
        planet(transferControl, 1).minerals.ironium));

    suns::PlayerOrders fleetTransfer{1, {}};
    fleetTransfer.orders.emplace_back(suns::TransferCargoOrder{
        {0, 2},
        {0, 3},
        100,
        {3.0, 2.0, 1.0},
    });
    const auto betweenFleets = processor.process(transferState, {fleetTransfer});
    assert(fleet(betweenFleets, 2).colonists == 200);
    assert(fleet(betweenFleets, 3).colonists == 100);
    assert(close(fleet(betweenFleets, 2).minerals.ironium, 7.0));
    assert(close(fleet(betweenFleets, 3).minerals.ironium, 3.0));

    // Capacity, location and unowned-world population rules reject the whole
    // mixed transfer without partially moving another cargo type.
    auto overloadTransferState = transferState;
    overloadTransferState.fleets[0].colonists = 6000;
    suns::PlayerOrders transferOverload{1, {}};
    transferOverload.orders.emplace_back(suns::TransferCargoOrder{
        {0, 2},
        {0, 3},
        6000,
        {1.0, 0.0, 0.0},
    });
    const auto transferRejected = processor.process(overloadTransferState, {transferOverload});
    assert(fleet(transferRejected, 2).colonists == 6000);
    assert(fleet(transferRejected, 3).colonists == 0);
    assert(close(fleet(transferRejected, 2).minerals.ironium, 10.0));

    auto separated = transferState;
    separated.fleets[1].position = {100.0, 0.0};
    const auto separatedResult = processor.process(separated, {fleetTransfer});
    assert(fleet(separatedResult, 2).colonists == 300);
    assert(fleet(separatedResult, 3).colonists == 0);

    auto unownedDestination = transferState;
    const auto* unownedStar = suns::find_star(unownedDestination, 2);
    assert(unownedStar != nullptr);
    unownedDestination.fleets[0].position = unownedStar->position;
    suns::PlayerOrders illegalPopulationDrop{1, {}};
    illegalPopulationDrop.orders.emplace_back(suns::TransferCargoOrder{
        {0, 2},
        {2, 0},
        100,
        {},
    });
    const auto populationDropRejected = processor.process(unownedDestination, {illegalPopulationDrop});
    assert(fleet(populationDropRejected, 2).colonists == 300);
    assert(planet(populationDropRejected, 2).population == 0);

    // Colonization deposits cargo in full and adds one-third ship salvage.
    auto colonization = suns::make_demo_game();
    colonization.shipDesigns.push_back({
        3,
        1,
        "Mineral Colonizer",
        suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::ColonyModule},
    });
    suns::set_survey_level(
        colonization, 1, 2, suns::SurveyLevel::OrbitalSurvey, colonization.turn);
    const auto* alpha = suns::find_star(colonization, 2);
    assert(alpha != nullptr);

    suns::Fleet colonizer;
    colonizer.id = 2;
    colonizer.owner = 1;
    colonizer.name = "Mineral Colonizer 2";
    colonizer.role = suns::FleetRole::ColonyShip;
    colonizer.design = 3;
    colonizer.position = alpha->position;
    colonizer.warp = 6;
    colonizer.fuel = 100.0;
    colonizer.colonists = 500;
    colonizer.minerals = {7.0, 5.0, 3.0};
    colonization.fleets = {colonizer};

    suns::PlayerOrders colonize{1, {}};
    colonize.orders.emplace_back(suns::ColonizePlanetOrder{2, 2});
    const auto expanded = processor.process(colonization, {colonize});
    assert(planet(expanded, 2).owner == 1);
    assert(close(planet(expanded, 2).minerals.ironium, 12.0));
    assert(close(planet(expanded, 2).minerals.boranium, 8.0));
    assert(close(planet(expanded, 2).minerals.germanium, 5.0));

    return 0;
}
