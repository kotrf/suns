#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>

namespace {
using namespace suns;

Planet& establish_test_colony(GameState& state)
{
    auto& colony = state.planets.at(1);
    colony.owner = 1;
    colony.population = 1000;
    colony.industry = 50;
    colony.minerals = {100.0, 100.0, 100.0};
    colony.productionQueue.clear();
    return colony;
}

void homeworld_starts_with_basic_orbital_services()
{
    const auto state = make_demo_game();
    assert(state.orbitalStations.size() == 1);
    assert(state.nextOrbitalStationId == 2);

    const auto* station = find_orbital_station_at_planet(state, 1);
    assert(station);
    assert(station->id == 1);
    assert(station->owner == 1);
    assert(orbital_station_has_module(*station, OrbitalStationModule::Shipyard));
    assert(orbital_station_has_module(*station, OrbitalStationModule::RefuelingDepot));

    const auto generated = generate_game({42, 12, 900.0, 650.0, 48.0});
    assert(generated.orbitalStations.size() == 1);
    assert(find_orbital_station_at_planet(generated, generated.planets.front().id));
}

void ship_production_waits_without_a_shipyard()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    auto& colony = establish_test_colony(state);
    const auto* design = find_ship_design(state, kColonyShipDesignId);
    assert(design);
    const auto cost = ship_design_cost(*design);
    colony.productionQueue = {{ProductionKind::ColonyShip, cost, design->id}};
    const auto fleetsBefore = state.fleets.size();

    auto result = processor.process_with_events(state, {});
    const auto& blocked = result.state.planets.at(1);
    assert(result.state.fleets.size() == fleetsBefore);
    assert(blocked.productionQueue.size() == 1);
    assert(blocked.productionQueue.front().remainingCost == cost);
    assert(blocked.productionWaitingForShipyard);
    assert(std::any_of(result.events.begin(), result.events.end(), [](const GameEvent& event) {
        return event.kind == GameEventKind::ProductionWaitingForShipyard;
    }));

    result = processor.process_with_events(result.state, {});
    assert(std::none_of(result.events.begin(), result.events.end(), [](const GameEvent& event) {
        return event.kind == GameEventKind::ProductionWaitingForShipyard;
    }));
}

void dock_then_ship_can_complete_in_queue_order()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    auto& colony = establish_test_colony(state);
    const auto* design = find_ship_design(state, kColonyShipDesignId);
    assert(design);
    colony.productionQueue = {
        {ProductionKind::OrbitalStation, kOrbitalDockCost, 0},
        {ProductionKind::ColonyShip, ship_design_cost(*design), design->id},
    };
    const auto fleetsBefore = state.fleets.size();

    const auto next = processor.process(state, {});
    const auto* station = find_orbital_station_at_planet(next, colony.id);
    assert(station);
    assert(station->id == 2);
    assert(next.nextOrbitalStationId == 3);
    assert(station->owner == colony.owner);
    assert(orbital_station_has_module(*station, OrbitalStationModule::Shipyard));
    assert(orbital_station_has_module(*station, OrbitalStationModule::RefuelingDepot));
    assert(next.fleets.size() == fleetsBefore + 1);
    assert(next.planets.at(1).productionQueue.empty());
}

void station_loss_removes_refueling_service()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    auto& fleet = state.fleets.front();
    fleet.fuel = 0.0;
    state.orbitalStations.clear();

    auto next = processor.process(state, {{1, {RefuelFleetOrder{1, fleet.id}}}});
    assert(next.fleets.front().fuel == 0.0);

    next.orbitalStations.push_back({
        next.nextOrbitalStationId++, 1, 1, "Replacement Dock",
        OrbitalStationHullType::OrbitalDock,
        {OrbitalStationModule::Shipyard, OrbitalStationModule::RefuelingDepot},
    });
    next = processor.process(next, {{1, {RefuelFleetOrder{1, fleet.id}}}});
    assert(next.fleets.front().fuel == fleet_fuel_capacity(next, next.fleets.front()));
}

void production_forecast_understands_shipyard_dependency()
{
    auto state = make_demo_game();
    auto& colony = establish_test_colony(state);
    const auto* design = find_ship_design(state, kColonyShipDesignId);
    assert(design);
    const ProductionItem ship{ProductionKind::ColonyShip, ship_design_cost(*design), design->id};

    const auto blocked = forecast_production_queue(state, colony, {ship}, 20);
    assert(blocked.size() == 1);
    assert(!blocked.front().completionTurn);
    assert(blocked.front().beyondForecastHorizon);

    const auto sequenced = forecast_production_queue(
        state, colony,
        {{ProductionKind::OrbitalStation, kOrbitalDockCost, 0}, ship}, 20);
    assert(sequenced.size() == 2);
    assert(sequenced[0].completionTurn == state.turn + 1);
    assert(sequenced[1].completionTurn == state.turn + 1);
}

} // namespace

int main()
{
    homeworld_starts_with_basic_orbital_services();
    ship_production_waits_without_a_shipyard();
    dock_then_ship_can_complete_in_queue_order();
    station_loss_removes_refueling_service();
    production_forecast_understands_shipyard_dependency();
}
