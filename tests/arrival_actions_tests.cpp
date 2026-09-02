#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>

namespace {

const suns::Fleet* fleet(const suns::GameState& state, suns::FleetId id)
{
    for (const auto& candidate : state.fleets) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

const suns::Planet* planet(const suns::GameState& state, suns::PlanetId id)
{
    for (const auto& candidate : state.planets) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

void verify_dynamic_load_uses_arrival_population()
{
    suns::TurnProcessor processor;
    auto state = suns::make_demo_game();

    // Alpha Centauri becomes a small friendly colony. It will grow while the
    // transport is still on its way, so the amount available on arrival differs
    // from the amount that was available when the order was issued.
    state.planets[1].owner = 1;
    state.planets[1].population = 300;

    const auto* design = suns::find_ship_design(state, suns::kColonyShipDesignId);
    assert(design != nullptr);
    const auto fuel = suns::ship_design_fuel_capacity(*design);
    state.fleets.push_back({
        2, 1, "Colony Ship 2", suns::FleetRole::ColonyShip, suns::kColonyShipDesignId,
        {0.0, 0.0}, std::nullopt, 8, fuel, 0,
    });
    state.nextFleetId = 3;

    suns::PlayerOrders route{1, {}};
    route.orders.emplace_back(suns::MoveFleetOrder{
        2,
        state.stars[1].position,
        8,
        {suns::FleetArrivalActionKind::LoadAllAvailable, 100},
    });

    auto turn2 = processor.process(state, {route});
    const auto* travelling = fleet(turn2, 2);
    assert(travelling != nullptr);
    assert(travelling->destination.has_value());
    assert(travelling->colonists == 0);
    const auto populationAfterOneGrowth = planet(turn2, 2)->population;
    assert(populationAfterOneGrowth > 300);

    auto turn3 = processor.process(turn2, {});
    const auto* arrived = fleet(turn3, 2);
    assert(arrived != nullptr);
    assert(!arrived->destination.has_value());
    assert(!arrived->arrivalAction.has_value());

    // Light Transport has room for 500 colonists. With a reserve of 100 and
    // only ~320 colonists present on arrival, the dynamic action loads more
    // than the 200 that would have been available at order time, but less than
    // the ship's full capacity.
    assert(arrived->colonists > 200);
    assert(arrived->colonists < 500);

    const auto* destination = planet(turn3, 2);
    assert(destination != nullptr);
    // Arrival loading leaves 100 before the normal end-of-turn growth phase.
    assert(destination->population >= 100);
}

void verify_unload_and_refuel_actions()
{
    suns::TurnProcessor processor;
    auto state = suns::make_demo_game();
    state.planets[1].owner = 1;
    state.planets[1].population = 1000;
    state.orbitalStations.push_back({
        state.nextOrbitalStationId++, 1, 2, "Centauri Orbital Dock",
        suns::OrbitalStationHullType::OrbitalDock,
        {suns::OrbitalStationModule::Shipyard, suns::OrbitalStationModule::RefuelingDepot},
    });

    const auto* design = suns::find_ship_design(state, suns::kColonyShipDesignId);
    assert(design != nullptr);
    const auto capacity = suns::ship_design_fuel_capacity(*design);
    state.fleets.push_back({
        2, 1, "Transport", suns::FleetRole::ColonyShip, suns::kColonyShipDesignId,
        state.stars[1].position, state.stars[1].position, 8, 10.0, 250,
        suns::FleetArrivalAction{suns::FleetArrivalActionKind::UnloadAll, 1},
    });

    auto unloaded = processor.process(state, {});
    const auto* transport = fleet(unloaded, 2);
    assert(transport != nullptr);
    assert(transport->colonists == 0);
    assert(!transport->arrivalAction.has_value());
    assert(planet(unloaded, 2)->population > 1250); // unload + normal growth

    auto refuelState = state;
    refuelState.fleets.back().colonists = 0;
    refuelState.fleets.back().fuel = 10.0;
    refuelState.fleets.back().arrivalAction = suns::FleetArrivalAction{suns::FleetArrivalActionKind::Refuel, 1};
    auto refuelled = processor.process(refuelState, {});
    assert(fleet(refuelled, 2)->fuel == capacity);
}

} // namespace

int main()
{
    verify_dynamic_load_uses_arrival_population();
    verify_unload_and_refuel_actions();
    return 0;
}
