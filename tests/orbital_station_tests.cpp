#include "suns/game_state.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/production.hpp"
#include "suns/ship_refit.hpp"
#include "suns/turn_processor.hpp"
#include "suns/stations.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {
using namespace suns;

Planet& establish_test_colony(GameState& state)
{
    auto& colony = state.planets.at(1);
    colony.owner = 1;
    colony.population = 50'000; // Staff all 50 factories used for this queue-order test.
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

void ships_cannot_be_queued_without_a_shipyard()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    auto& colony = establish_test_colony(state);
    assert(!find_orbital_station_at_planet(state, colony.id));
    const auto next = processor.process(
        state, {{1, {QueueShipDesignOrder{colony.id, kScoutDesignId}}}});
    assert(next.planets.at(1).productionQueue.empty());
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
    assert(next.fleets.front().fuel == kOrbitalDockAnnualFuelMg);
    next = processor.process(next, {});
    assert(next.fleets.front().fuel == fleet_fuel_capacity(next, next.fleets.front()));
}

void station_refuels_before_departure_and_after_arrival()
{
    TurnProcessor processor;
    auto departure = make_demo_game();
    auto& departingFleet = departure.fleets.front();
    const auto departureCapacity = fleet_fuel_capacity(departure, departingFleet);
    departingFleet.fuel = 0.0;
    departingFleet.destination = Position{1.0, 0.0};
    departingFleet.warp = 1;

    const auto underway = processor.process(departure, {});
    const auto& moved = underway.fleets.front();
    assert(same_position(moved.position, Position{1.0, 0.0}));
    assert(moved.fuel > 0.0);
    assert(moved.fuel < departureCapacity);

    auto arrival = make_demo_game();
    auto& arrivingFleet = arrival.fleets.front();
    const auto arrivalCapacity = fleet_fuel_capacity(arrival, arrivingFleet);
    arrivingFleet.position = {1.0, 0.0};
    arrivingFleet.destination = arrival.stars.front().position;
    arrivingFleet.warp = 1;
    arrivingFleet.fuel = arrivalCapacity / 2.0;

    const auto docked = processor.process(arrival, {});
    assert(same_position(docked.fleets.front().position, arrival.stars.front().position));
    assert(!docked.fleets.front().destination);
    assert(std::abs(docked.fleets.front().fuel - arrivalCapacity) < 0.000001);
}

void station_without_refueling_module_does_not_refuel()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    state.orbitalStations.front().modules = {OrbitalStationModule::Shipyard};
    state.fleets.front().fuel = 12.0;

    const auto next = processor.process(state, {});
    assert(std::abs(next.fleets.front().fuel - 12.0) < 0.000001);
}

void dock_shares_fuel_between_departures_and_arrivals()
{
    for (const auto arrivalAction : {FleetArrivalActionKind::None, FleetArrivalActionKind::Refuel}) {
        auto state = make_demo_game();
        auto& departure = state.fleets.front();
        const auto capacity = fleet_fuel_capacity(state, departure);
        departure.fuel = capacity - 150;
        departure.destination = Position{1, 0};
        departure.warp = 1;
        auto arrival = departure;
        arrival.id = state.nextFleetId++;
        arrival.position = *departure.destination;
        arrival.destination = departure.position;
        arrival.arrivalAction = FleetArrivalAction{arrivalAction};
        arrival.fuel = 20;
        state.fleets.push_back(arrival);

        auto noDepot = state;
        noDepot.orbitalStations.clear();
        const auto unserviced = TurnProcessor{}.process(noDepot, {});
        const auto refueled = TurnProcessor{}.process(state, {});
        assert(same_position(refueled.fleets[0].position, arrival.position));
        assert(same_position(refueled.fleets[1].position, *arrival.destination));
        assert(std::abs(refueled.fleets[0].fuel - unserviced.fleets[0].fuel - 150) < 1e-8);
        assert(std::abs(refueled.fleets[1].fuel - unserviced.fleets[1].fuel - 50) < 1e-8);
        const auto next = TurnProcessor{}.process(refueled, {});
        assert(std::abs(next.fleets[1].fuel - refueled.fleets[1].fuel - 200) < 1e-8);
    }
}

void dock_refuels_in_order_and_reuses_the_quota_for_explicit_commands()
{
    auto state = make_demo_game();
    const auto capacity = fleet_fuel_capacity(state, state.fleets.front());
    state.fleets.front().fuel = capacity - 150;
    auto second = state.fleets.front();
    second.id = state.nextFleetId++;
    state.fleets.insert(state.fleets.begin(), second); // Higher ID first in storage.

    const auto estimated = estimated_orbital_departure_fuel(state, 1, second);
    assert(estimated == capacity - 100);
    second.fuel = estimated;
    assert(estimated_orbital_departure_fuel(state, 1, second) == estimated);
    const auto automatic = TurnProcessor{}.process(state, {});
    assert(automatic.fleets[0].fuel == estimated && automatic.fleets[1].fuel == capacity);

    // Filling before and after a transfer consumes the same station balance.
    const PlayerOrders orders{1, {RefuelFleetOrder{1, 2}, TransferFleetFuelOrder{2, 1, 100},
        RefuelFleetOrder{1, 2}, RefuelFleetOrder{1, 2}}};
    const auto commandBoundary = projected_cargo_order_state(state, orders);
    assert(commandBoundary.fleets[0].fuel == capacity - 50);
    assert(commandBoundary.fleets[1].fuel == capacity - 50);
    assert(projected_orbital_dock_departure_fuel(state, orders, 2) == capacity - 50);
    const auto explicitFuel = TurnProcessor{}.process(state, {orders});
    assert(explicitFuel.fleets[0].fuel == capacity - 50 && explicitFuel.fleets[1].fuel == capacity - 50);

    state.orbitalStations.front().hull = OrbitalStationHullType::Starbase;
    const auto advanced = TurnProcessor{}.process(state, {});
    assert(advanced.fleets[0].fuel == capacity && advanced.fleets[1].fuel == capacity);
}

void dock_budget_skips_fleets_with_a_live_refit()
{
    auto state = make_demo_game();
    auto& colony = state.planets.front();
    colony.minerals = {};
    colony.observedConcentration = MineralCargo{};
    auto& first = state.fleets.front();
    first.fuel = 0;
    first.telemetry = {};
    auto second = first;
    second.id = state.nextFleetId++;
    state.fleets.push_back(second);
    assert(estimated_orbital_departure_fuel(state, colony.id, second) == 0);

    auto design = *find_ship_design(state, kScoutDesignId);
    design.id = state.nextShipDesignId++;
    design.name = "Refitted scout";
    design.components.push_back(ShipComponentType::MoleSkinShield);
    design.placements.clear();
    state.shipDesigns.push_back(design);
    assert(queue_ship_refit(state, 1, {colony.id, 1, kScoutDesignId, design.id}));
    state.players.front().surveyKnowledge.clear();
    set_survey_level(state, 1, colony.star, SurveyLevel::DeepSurvey, state.turn);
    const auto received = make_player_view(state, 1);
    const auto& receivedFleet = received.state.fleets.back();
    assert(receivedFleet.id == second.id && !received.state.planets.front().productionQueue.empty());
    assert(estimated_orbital_departure_fuel(received.state, colony.id, receivedFleet) == kOrbitalDockAnnualFuelMg);
    assert(projected_orbital_dock_departure_fuel(received.state, {1, {}}, second.id) == kOrbitalDockAnnualFuelMg);
    const auto next = TurnProcessor{}.process(state, {});
    assert(!next.planets.front().productionQueue.empty() && next.planets.front().productionQueue.front().refit);
    assert(next.fleets.front().fuel == 0 && next.fleets.back().fuel == kOrbitalDockAnnualFuelMg);

    state.fleets.front().destination = Position{1, 0};
    assert(estimated_orbital_departure_fuel(state, colony.id, second) == 0);
}

void dock_budget_uses_fuel_after_onboard_generation()
{
    auto state = make_demo_game();
    const auto colony = state.planets.front().id;
    const auto tankerDesign = state.nextShipDesignId++;
    state.shipDesigns.push_back({tankerDesign, 1, "Tanker", ShipHullType::SuperFuelXport,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}});
    auto scout = state.fleets.front();
    scout.id = state.nextFleetId++;
    scout.fuel = 0;
    scout.warp = 5;
    state.fleets.front().design = tankerDesign;
    state.fleets.front().ships = {{tankerDesign, 1}};
    state.fleets.push_back(scout);
    assert(fleet_fuel_capacity(state, state.fleets.front()) == 2250);
    assert(fleet_fuel_generation(state, state.fleets.front()) == 200);

    const struct {
        double tankerBefore, tankerAfter, scoutAfter;
    } cases[] = {{2050, 2250, 200}, {1950, 2250, 100}, {1000, 1400, 0}};
    for (const auto& [tankerBefore, tankerAfter, scoutAfter] : cases) {
        state.fleets.front().fuel = tankerBefore;
        for (auto& fleet : state.fleets) {
            fleet.telemetry = {};
            fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
        }
        const auto received = make_player_view(state, 1);
        const auto& view = received.state;
        assert(estimated_orbital_departure_fuel(view, colony, view.fleets.back()) == scoutAfter);
        assert(projected_orbital_dock_departure_fuel(view, {1, {}}, scout.id) == scoutAfter);
        auto tanker = view.fleets.front();
        assert(estimated_orbital_departure_fuel(view, colony, tanker) == tankerAfter);
        tanker.fuel = tankerAfter;
        assert(estimated_orbital_departure_fuel(view, colony, tanker) == tankerAfter);
        const auto next = TurnProcessor{}.process(state, {});
        assert(next.fleets.front().fuel == tankerAfter && next.fleets.back().fuel == scoutAfter);
    }

    state.fleets.front().fuel = 2050;
    const Position destination{25, 0};
    const auto underway = TurnProcessor{}.process(state, {{1, {MoveFleetOrder{scout.id, destination, 5}}}});
    assert(same_position(underway.fleets.back().position, destination));
    assert(std::abs(underway.fleets.back().fuel - (200.0
        - fleet_fuel_consumption_for_distance(state, scout, 25))) < 1e-8);
}

void dock_checks_dry_mass_at_admission_and_before_spending()
{
    auto state = make_demo_game();
    state.players.front().technology.levels.fill(20);
    state.players.front().technology.researchActive = false;
    auto& colony = state.planets.front();
    colony.population = 250'000;
    colony.industry = 200;
    colony.minerals = {1000, 1000, 1000};
    colony.mines = 0;

    ShipDesign exact{90, 1, "200 kt", ShipHullType::Cruiser,
        {ShipComponentType::TransGalacticDrive, ShipComponentType::TransGalacticDrive,
            ShipComponentType::Tritanium}};
    auto heavy = exact;
    heavy.id = 91;
    heavy.name = "201 kt";
    heavy.components.push_back(ShipComponentType::MoleSkinShield);
    assert(ship_design_mass(exact) == 200 && ship_design_mass(heavy) == 201);
    assert(ship_design_valid(exact) && ship_design_valid(heavy));
    state.shipDesigns.push_back(exact);
    state.shipDesigns.push_back(heavy);
    state.nextShipDesignId = 92;

    const PlayerOrders orders{1, {QueueShipDesignOrder{1, heavy.id}, QueueShipDesignOrder{1, exact.id},
        QueueShipDesignOrder{1, exact.id}}};
    const auto plan = planned_production_state(state, orders);
    assert(plan.planets.front().productionQueue.size() == 2);
    const auto next = TurnProcessor{}.process(state, {orders});
    assert(next.planets.front().productionQueue.empty());
    assert(std::count_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& fleet) {
        return fleet.design == exact.id;
    }) == 2);
    assert(std::none_of(next.fleets.begin(), next.fleets.end(), [&](const Fleet& fleet) { return fleet.design == heavy.id; }));

    const auto idle = TurnProcessor{}.process(state, {});
    colony.productionQueue = {{ProductionKind::ColonyShip, 1, heavy.id}};
    const auto blocked = TurnProcessor{}.process(state, {});
    assert(blocked.planets.front().productionWaitingForShipyard);
    assert(blocked.planets.front().productionQueue.front().remainingCost == 1);
    assert(blocked.planets.front().minerals.ironium == idle.planets.front().minerals.ironium);
    assert(blocked.planets.front().minerals.boranium == idle.planets.front().minerals.boranium);
    assert(blocked.planets.front().minerals.germanium == idle.planets.front().minerals.germanium);
    assert(blocked.fleets.size() == state.fleets.size());

    const StationLoadout upgrade{OrbitalStationHullType::Starbase, {}, true, true};
    ProductionItem refit{ProductionKind::OrbitalStation, 0};
    refit.station = StationConstruction{upgrade, state.orbitalStations.front().id};
    colony.productionQueue.insert(colony.productionQueue.begin(), refit);
    const auto upgraded = TurnProcessor{}.process(state, {});
    assert(upgraded.orbitalStations.front().hull == OrbitalStationHullType::Starbase);
    assert(upgraded.planets.front().productionQueue.empty());
    assert(upgraded.fleets.back().design == heavy.id);
}

void shipyard_repairs_docked_fleets_once_per_year()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    state.orbitalStations.front().modules = {OrbitalStationModule::Shipyard};
    state.fleets.front().damagePercent = 100.0;

    const auto first = processor.process(state, {});
    assert(std::abs(first.fleets.front().damagePercent - 80.0) < 0.000001);
    assert(fleet_warp_valid(first, first.fleets.front(), 1));

    const auto second = processor.process(first, {});
    assert(std::abs(second.fleets.front().damagePercent - 60.0) < 0.000001);

    auto almostRepaired = state;
    almostRepaired.fleets.front().damagePercent = 13.5;
    const auto completed = processor.process(almostRepaired, {});
    assert(completed.fleets.front().damagePercent == 0.0);
}

void repair_requires_a_friendly_shipyard_at_the_fleet_position()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    state.fleets.front().damagePercent = 40.0;

    auto withoutShipyard = state;
    withoutShipyard.orbitalStations.front().modules = {OrbitalStationModule::RefuelingDepot};
    assert(processor.process(withoutShipyard, {}).fleets.front().damagePercent == 40.0);

    auto enemyStation = state;
    enemyStation.orbitalStations.front().owner = 2;
    assert(processor.process(enemyStation, {}).fleets.front().damagePercent == 40.0);

    auto awayFromColony = state;
    awayFromColony.fleets.front().position = {1.0, 0.0};
    assert(processor.process(awayFromColony, {}).fleets.front().damagePercent == 40.0);

    state.orbitalStations.clear();
    assert(processor.process(state, {}).fleets.front().damagePercent == 40.0);
}

void arriving_fleet_repairs_but_departing_fleet_does_not()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    state.fleets.front().damagePercent = 40.0;
    state.fleets.front().warp = 1;
    state.fleets.front().destination = Position{1.0, 0.0};

    const auto departed = processor.process(state, {});
    assert(same_position(departed.fleets.front().position, Position{1.0, 0.0}));
    assert(departed.fleets.front().damagePercent == 40.0);

    auto returning = departed;
    returning.fleets.front().destination = state.fleets.front().position;
    const auto arrived = processor.process(returning, {});
    assert(same_position(arrived.fleets.front().position, state.fleets.front().position));
    assert(arrived.fleets.front().damagePercent == 20.0);
}

void newly_built_dock_can_repair_a_fleet_that_year()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    auto& colony = establish_test_colony(state);
    colony.productionQueue = {{ProductionKind::OrbitalStation, kOrbitalDockCost, 0}};
    state.fleets.front().position = state.stars.at(1).position;
    state.fleets.front().damagePercent = 100.0;

    const auto next = processor.process(state, {});
    assert(find_orbital_station_at_planet(next, colony.id));
    assert(next.fleets.front().damagePercent == 80.0);
}

void field_bays_repair_remote_fleets_in_proportion_to_equipped_ships()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    state.shipDesigns.push_back({
        state.nextShipDesignId++, 1, "Repair Scout", ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::FieldRepairBay},
    });
    const auto repairDesign = state.shipDesigns.back().id;
    auto& fleet = state.fleets.front();
    fleet.position = {500.0, 500.0};
    fleet.damagePercent = 100.0;
    fleet.ships = {{repairDesign, 1}};

    const auto equipped = processor.process(state, {});
    assert(equipped.fleets.front().damagePercent == 92.0);
    assert(fleet_warp_valid(equipped, equipped.fleets.front(), 1));

    fleet.ships.push_back({kScoutDesignId, 1});
    const auto mixed = processor.process(state, {});
    assert(mixed.fleets.front().damagePercent == 96.0);

    fleet.position = state.stars.front().position;
    const auto docked = processor.process(state, {});
    assert(docked.fleets.front().damagePercent == 80.0);
}

void restored_mobility_is_reported_once_and_respects_communications_delay()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    state.fleets.front().damagePercent = 100.0;

    const auto first = processor.process_with_events(state, {});
    const auto report = std::find_if(first.events.begin(), first.events.end(), [](const GameEvent& event) {
        return event.kind == GameEventKind::FleetMobilityRestored;
    });
    assert(report != first.events.end());
    assert(report->fleet == state.fleets.front().id);
    assert(report->recipient == state.fleets.front().owner);
    assert(report->observedTurn == state.turn + 1);
    const auto replay = processor.process_with_events(state, {});
    const auto replayReport = std::find_if(replay.events.begin(), replay.events.end(), [](const GameEvent& event) {
        return event.kind == GameEventKind::FleetMobilityRestored;
    });
    assert(replayReport != replay.events.end() && replayReport->id == report->id);
    const auto second = processor.process_with_events(first.state, {});
    assert(std::none_of(second.events.begin(), second.events.end(), [](const GameEvent& event) {
        return event.kind == GameEventKind::FleetMobilityRestored;
    }));

    auto remote = state;
    remote.shipDesigns.push_back({
        remote.nextShipDesignId++, 1, "Repair Scout", ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::FieldRepairBay}});
    remote.fleets.front().ships = {{remote.shipDesigns.back().id, 1}};
    remote.fleets.front().position = {500.0, 500.0};
    const auto firstRemote = processor.process_with_events(remote, {});
    assert(std::none_of(firstRemote.events.begin(), firstRemote.events.end(), [](const GameEvent& event) {
        return event.kind == GameEventKind::FleetMobilityRestored;
    }));
    assert(firstRemote.state.players.front().pendingPlayerReports.size() == 1);
    const auto dueTurn = firstRemote.state.players.front().pendingPlayerReports.front().deliveryTurn;
    assert(dueTurn > firstRemote.state.turn);
    assert(firstRemote.state.players.front().pendingPlayerReports.front().kind
        == PlayerReportKind::FleetMobilityRestored);
    auto current = firstRemote.state;
    while (current.turn < dueTurn) {
        auto next = processor.process_with_events(current, {});
        if (next.state.turn == dueTurn) {
            const auto delayed = std::find_if(next.events.begin(), next.events.end(), [](const GameEvent& event) {
                return event.kind == GameEventKind::FleetMobilityRestored;
            });
            assert(delayed != next.events.end());
            assert(delayed->observedTurn == remote.turn + 1);
            assert(delayed->turn == dueTurn);
        }
        current = std::move(next.state);
    }
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

void production_forecast_carries_station_hulls_between_years()
{
    auto state = make_demo_game();
    state.players.front().technology.levels.fill(20);
    state.players.front().technology.researchActive = false;
    auto& colony = state.planets.front();
    colony.population = 75'000;
    colony.industry = colony.mines = 0;
    colony.minerals = {10000, 10000, 10000};
    const ShipDesign heavy{90, 1, "Heavy cruiser", ShipHullType::Cruiser,
        {ShipComponentType::TransGalacticDrive, ShipComponentType::TransGalacticDrive,
            ShipComponentType::Tritanium, ShipComponentType::MoleSkinShield}};
    assert(ship_design_valid(heavy) && ship_design_mass(heavy) == 201);
    state.shipDesigns.push_back(heavy);
    state.nextShipDesignId = 91;
    const ProductionItem ship{ProductionKind::ColonyShip, ship_design_cost(heavy), heavy.id};
    ProductionItem refit{ProductionKind::OrbitalStation, colony_output(state, colony)};
    refit.station = StationConstruction{{OrbitalStationHullType::Starbase},
        state.orbitalStations.front().id};
    colony.productionQueue = {refit, ship};
    constexpr unsigned horizon = 12;
    const auto upgraded = forecast_production_queue(state, colony, colony.productionQueue, horizon);
    TurnProcessor processor;
    auto hosted = processor.process(state, {});
    assert(hosted.orbitalStations.front().hull == OrbitalStationHullType::Starbase);
    assert(hosted.planets.front().productionQueue.size() == 1);
    for (unsigned year = 1; year < horizon && !hosted.planets.front().productionQueue.empty(); ++year)
        hosted = processor.process(hosted, {});
    assert(hosted.planets.front().productionQueue.empty() && hosted.fleets.back().design == heavy.id);
    assert(upgraded[1].completionTurn == hosted.turn);

    for (const bool explicitLoadout : {false, true}) {
        state.orbitalStations.clear();
        ProductionItem dock{ProductionKind::OrbitalStation, kOrbitalDockCost};
        if (explicitLoadout) dock.station = StationConstruction{};
        colony.productionQueue = {dock, ship};
        const auto forecast = forecast_production_queue(state, colony, colony.productionQueue, horizon);
        hosted = state;
        for (unsigned year = 0; year < horizon; ++year) hosted = processor.process(hosted, {});
        assert(forecast[0].completionTurn && hosted.orbitalStations.front().hull == OrbitalStationHullType::OrbitalDock);
        assert(hosted.planets.front().productionQueue.size() == 1);
        assert(hosted.planets.front().productionQueue.front().remainingCost == ship.remainingCost);
        assert(!forecast[1].completionTurn && forecast[1].beyondForecastHorizon);
    }
}


void paid_stargates_validate_technology_and_safe_transport()
{
    auto s = make_demo_game();
    auto& owner = s.players.front();
    auto& destination = s.planets.at(1);
    destination.owner = 1;
    destination.population = 2'000'000;
    destination.industry = 150;
    destination.minerals = {500, 500, 500};
    const StationLoadout gateFit{
        OrbitalStationHullType::Starbase, {ShipComponentType::JumpGate}, true, true};
    assert(station_loadout_valid(gateFit));
    assert(station_build_cost(gateFit) > station_hull_spec(OrbitalStationHullType::Starbase).cost);
    assert(!station_loadout_available(s, 1, gateFit));
    owner.technology.levels[std::size_t(ResearchField::Energy)] = kStargateEnergyLevel;
    owner.technology.levels[std::size_t(ResearchField::Electronics)] = kStargateElectronicsLevel;
    owner.technology.levels[std::size_t(ResearchField::Construction)] = 9;
    assert(station_loadout_available(s, 1, gateFit));
    auto twoGates = gateFit;
    twoGates.components.push_back(ShipComponentType::JumpGate);
    assert(!station_loadout_valid(twoGates));
    auto dock = gateFit;
    dock.hull = OrbitalStationHullType::OrbitalDock;
    assert(!station_loadout_valid(dock));

    // Construction is a paid normal colony project, not a free research reward.
    const QueueStationOrder build{destination.id, {gateFit, 0}};
    assert(queue_station_construction(s, 1, build));
    assert(!destination.productionQueue.empty());
    assert(destination.productionQueue.back().remainingCost == station_build_cost(gateFit));
    assert(complete_station_construction(s, destination, build.construction));
    destination.productionQueue.clear(); // Completion removed this paid work.
    assert(stargate_station_available(s, 1, destination.id));
    assert(!stargate_station_available(s, 1, 1));
    // Earth must be refitted; a stock shipyard/refueling dock is not a gate.
    auto& sourceGate = s.orbitalStations.front();
    sourceGate.hull = OrbitalStationHullType::Starbase;
    sourceGate.components = {ShipComponentType::JumpGate};
    assert(stargate_station_available(s, 1, 1));

    const auto id = s.fleets.front().id;
    const auto initialFuel = s.fleets.front().fuel;
    assert(stargate_transfer_valid(s, 1, id, 1, 2));
    auto tooFar = s;
    tooFar.stars.at(1).position = {1000, 1000};
    assert(!stargate_transfer_valid(tooFar, 1, id, 1, 2));
    auto loaded = s;
    loaded.fleets.front().colonists = 1000;
    assert(!stargate_transfer_valid(loaded, 1, id, 1, 2));
    loaded = s;
    loaded.fleets.front().minerals.ironium = 1;
    assert(!stargate_transfer_valid(loaded, 1, id, 1, 2));
    loaded = s;
    loaded.fleets.front().destination = s.stars.at(1).position;
    assert(!stargate_transfer_valid(loaded, 1, id, 1, 2));
    loaded = s;
    loaded.orbitalStations.back().components.clear();
    assert(!stargate_transfer_valid(loaded, 1, id, 1, 2));
    assert(!stargate_transfer_valid(s, 1, id, 2, 1));
    assert(!stargate_transfer_valid(s, 2, id, 1, 2));

    const auto arrived = TurnProcessor{}.process(s, {{1, {
        GateFleetOrder{id, 1, 2}, GateFleetOrder{id, 2, 1}
    }}});
    assert(same_position(arrived.fleets.front().position, arrived.stars.at(1).position));
    assert(arrived.fleets.front().fuel == initialFuel);
    // No same-year chain of arbitrary gates; the reciprocal order is ignored.
    assert(!same_position(arrived.fleets.front().position, s.stars.front().position));
}
} // namespace

int main()
{
    homeworld_starts_with_basic_orbital_services();
    ship_production_waits_without_a_shipyard();
    ships_cannot_be_queued_without_a_shipyard();
    dock_then_ship_can_complete_in_queue_order();
    station_loss_removes_refueling_service();
    station_refuels_before_departure_and_after_arrival();
    station_without_refueling_module_does_not_refuel();
    dock_shares_fuel_between_departures_and_arrivals();
    dock_refuels_in_order_and_reuses_the_quota_for_explicit_commands();
    dock_budget_skips_fleets_with_a_live_refit();
    dock_budget_uses_fuel_after_onboard_generation();
    dock_checks_dry_mass_at_admission_and_before_spending();
    shipyard_repairs_docked_fleets_once_per_year();
    repair_requires_a_friendly_shipyard_at_the_fleet_position();
    arriving_fleet_repairs_but_departing_fleet_does_not();
    newly_built_dock_can_repair_a_fleet_that_year();
    field_bays_repair_remote_fleets_in_proportion_to_equipped_ships();
    restored_mobility_is_reported_once_and_respects_communications_delay();
    production_forecast_understands_shipyard_dependency();
    production_forecast_carries_station_hulls_between_years();
    paid_stargates_validate_technology_and_safe_transport();
}
