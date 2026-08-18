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

    assert(std::abs(suns::fleet_cargo_capacity(state, state.fleets.front()) - 50.0) < 0.000001);
    assert(std::abs(suns::fleet_cargo_used(state, state.fleets.front()) - 10.0) < 0.000001);

    const auto emptyFuelBurn = suns::fleet_fuel_change_for_distance(state, state.fleets.front(), 36.0);

    suns::PlayerOrders loadMinerals{1, {}};
    loadMinerals.orders.emplace_back(
        suns::SetFleetMineralCargoOrder{1, 2, {20.0, 10.0, 10.0}});
    const suns::TurnProcessor processor;
    const auto loaded = processor.process(state, {loadMinerals});

    const auto& loadedFleet = fleet(loaded, 2);
    assert(std::abs(loadedFleet.minerals.ironium - 20.0) < 0.000001);
    assert(std::abs(loadedFleet.minerals.boranium - 10.0) < 0.000001);
    assert(std::abs(loadedFleet.minerals.germanium - 10.0) < 0.000001);
    assert(std::abs(suns::fleet_cargo_used(loaded, loadedFleet) - 50.0) < 0.000001);
    assert(std::abs(planet(loaded, 1).minerals.ironium - 80.0) < 0.000001);
    assert(std::abs(planet(loaded, 1).minerals.boranium - 90.0) < 0.000001);
    assert(std::abs(planet(loaded, 1).minerals.germanium - 90.0) < 0.000001);
    assert(suns::fleet_fuel_change_for_distance(loaded, loadedFleet, 36.0) > emptyFuelBurn);

    // A load that would overflow the shared hold is rejected atomically.
    suns::PlayerOrders overload{1, {}};
    overload.orders.emplace_back(
        suns::SetFleetMineralCargoOrder{1, 2, {21.0, 10.0, 10.0}});
    const auto rejected = processor.process(loaded, {overload});
    assert(std::abs(fleet(rejected, 2).minerals.ironium - 20.0) < 0.000001);

    // Unloading returns exact mineral amounts to the colony.
    suns::PlayerOrders unload{1, {}};
    unload.orders.emplace_back(
        suns::SetFleetMineralCargoOrder{1, 2, {0.0, 0.0, 0.0}});
    const auto unloaded = processor.process(loaded, {unload});
    assert(std::abs(suns::mineral_cargo_mass(fleet(unloaded, 2).minerals)) < 0.000001);
    assert(std::abs(planet(unloaded, 1).minerals.ironium - 100.0) < 0.000001);
    assert(std::abs(planet(unloaded, 1).minerals.boranium - 100.0) < 0.000001);
    assert(std::abs(planet(unloaded, 1).minerals.germanium - 100.0) < 0.000001);

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
    assert(std::abs(planet(expanded, 2).minerals.ironium - 7.0) < 0.000001);
    assert(std::abs(planet(expanded, 2).minerals.boranium - 5.0) < 0.000001);
    assert(std::abs(planet(expanded, 2).minerals.germanium - 3.0) < 0.000001);

    return 0;
}
