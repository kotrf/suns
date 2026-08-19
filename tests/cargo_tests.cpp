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
        {suns::ShipComponentType::FusionDrive},
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
        suns::FleetArrivalActionKind::LoadColonistsToCapacity,
        1,
    };
    const auto dynamicallyLoaded = processor.process(dynamic, {});
    assert(fleet(dynamicallyLoaded, 2).colonists == 999); // keep one colonist at the source colony.

    // Colonization deposits carried minerals on the new world before the ship is consumed.
    auto colonization = suns::make_demo_game();
    colonization.shipDesigns.push_back({
        3,
        1,
        "Mineral Colonizer",
        suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::ColonyModule},
    });
    colonization.players.front().surveyedStars.push_back(2);
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
    assert(close(planet(expanded, 2).minerals.ironium, 7.0));
    assert(close(planet(expanded, 2).minerals.boranium, 5.0));
    assert(close(planet(expanded, 2).minerals.germanium, 3.0));

    return 0;
}
