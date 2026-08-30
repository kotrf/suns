#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace {

using namespace suns;

const GameEvent* find_event(const std::vector<GameEvent>& events, GameEventKind kind);

GameState survey_fixture(Position scoutPosition, Position targetPosition, std::uint64_t turn = 1)
{
    auto state = make_demo_game();
    state.turn = turn;
    state.stars.resize(2);
    state.planets.resize(2);
    state.fleets.resize(1);

    state.stars[0] = {1, "Home", {0.0, 0.0}, StarClass::Yellow};
    state.stars[1] = {2, "Target", targetPosition, StarClass::Orange};
    state.planets[0] = {1, 1, "Home I", 100, 1, 1000, 4, 0, {}};
    state.planets[1] = {2, 2, "Target II", 82, 0, 0, 1, 0, {}};
    state.players.front().surveyedStars = {1};
    state.players.front().pendingSurveyReports.clear();

    auto& scout = state.fleets.front();
    scout.position = scoutPosition;
    scout.destination.reset();
    scout.pendingCommands.clear();
    scout.telemetryInTransit.clear();
    return state;
}

void ordinary_scanner_reports_system_contact_without_planet_data()
{
    const auto initial = survey_fixture({100.0, 0.0}, {150.0, 0.0});
    const TurnProcessor processor;

    const auto first = processor.process_with_events(initial, {});
    const auto replay = processor.process_with_events(initial, {});

    assert(first.state.turn == 2);
    assert(!is_surveyed(first.state, 1, 2));
    assert(survey_level(first.state, 1, 2) == SurveyLevel::SystemScan);
    assert(!known_planet_habitability(first.state, 1, 2).has_value());
    assert(!planet_geology_known(first.state, 1, 2));
    assert(first.state.players.front().pendingSurveyReports.empty());
    assert(first.events.size() == 1);
    assert(first.events.front().kind == GameEventKind::SystemSurveyed);
    assert(first.events.front().recipient == 1);
    assert(first.events.front().star == 2);
    assert(first.events.front().planet == 2);
    assert(first.events.front().fleet == 1);
    assert(first.events.front().observedTurn == 2);
    assert(first.events.front().turn == 2);
    assert(first.events.front().surveyLevel == SurveyLevel::SystemScan);
    assert(first.events.front().quantity == 0);
    assert(first.events.front().id == replay.events.front().id);
}

void penetrating_scanner_estimates_planet_during_a_flyby()
{
    auto state = survey_fixture({0.0, 0.0}, {50.0, 65.0});
    state.shipDesigns.push_back({
        99,
        1,
        "Deep Surveyor",
        ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::PenetratingScanner},
    });
    state.fleets.front().design = 99;
    state.fleets.front().ships = {{99, 1}};
    state.fleets.front().destination = Position{200.0, 0.0};
    state.fleets.front().fuel = 300.0;
    const TurnProcessor processor;

    const auto observed = processor.process_with_events(state, {});
    assert(survey_level(observed.state, 1, 2) < SurveyLevel::BasicScan);
    assert(!observed.state.players.front().pendingSurveyReports.empty());

    const auto result = processor.process_with_events(observed.state, {});
    assert(survey_level(result.state, 1, 2) == SurveyLevel::BasicScan);
    assert(is_surveyed(result.state, 1, 2));
    assert(known_planet_habitability(result.state, 1, 2).has_value());
    assert(!planet_geology_known(result.state, 1, 2));
    assert(result.events.size() == 1);
    assert(result.events.front().surveyLevel == SurveyLevel::BasicScan);
    assert(result.events.front().quantity == *known_planet_habitability(result.state, 1, 2));
}

void remote_report_remains_in_flight_until_delivery()
{
    auto state = survey_fixture({420.0, 0.0}, {450.0, 0.0}, 10);
    const TurnProcessor processor;

    const auto turn11 = processor.process_with_events(state, {});
    assert(turn11.state.turn == 11);
    assert(!is_surveyed(turn11.state, 1, 2));
    assert(turn11.events.empty());
    assert(turn11.state.players.front().pendingSurveyReports.size() == 1);
    const auto& pending = turn11.state.players.front().pendingSurveyReports.front();
    assert(pending.star == 2);
    assert(pending.sourceFleet == 1);
    assert(pending.observedTurn == 11);
    assert(pending.deliveryTurn == 14);
    assert(pending.level == SurveyLevel::SystemScan);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    assert(!is_surveyed(turn12.state, 1, 2));
    assert(turn12.events.empty());
    assert(turn12.state.players.front().pendingSurveyReports.size() == 1);
    assert(turn12.state.players.front().pendingSurveyReports.front().observedTurn == 11);

    const auto turn13 = processor.process_with_events(turn12.state, {});
    assert(!is_surveyed(turn13.state, 1, 2));
    assert(turn13.events.empty());

    const auto turn14 = processor.process_with_events(turn13.state, {});
    assert(!is_surveyed(turn14.state, 1, 2));
    assert(survey_level(turn14.state, 1, 2) == SurveyLevel::SystemScan);
    assert(turn14.state.players.front().pendingSurveyReports.empty());
    assert(turn14.events.size() == 1);
    assert(turn14.events.front().observedTurn == 11);
    assert(turn14.events.front().turn == 14);

    const auto turn15 = processor.process_with_events(turn14.state, {});
    assert(turn15.events.empty());
}

void arrival_and_dwell_progress_through_orbital_and_geological_surveys()
{
    auto state = survey_fixture({0.0, 0.0}, {50.0, 0.0});
    auto& scout = state.fleets.front();
    scout.destination = Position{50.0, 0.0};
    scout.fuel = 300.0;
    const TurnProcessor processor;

    const auto arrival = processor.process_with_events(state, {});
    assert(survey_level(arrival.state, 1, 2) == SurveyLevel::OrbitalSurvey);
    assert(known_planet_habitability(arrival.state, 1, 2) == 82);
    assert(!planet_geology_known(arrival.state, 1, 2));
    const auto* orbital = find_event(arrival.events, GameEventKind::SystemSurveyed);
    assert(orbital);
    assert(orbital->surveyLevel == SurveyLevel::OrbitalSurvey);
    assert(orbital->quantity == 82);

    const auto dwell = processor.process_with_events(arrival.state, {});
    assert(survey_level(dwell.state, 1, 2) == SurveyLevel::GeologicalSurvey);
    assert(planet_geology_known(dwell.state, 1, 2));
    const auto* geological = find_event(dwell.events, GameEventKind::SystemSurveyed);
    assert(geological);
    assert(geological->surveyLevel == SurveyLevel::GeologicalSurvey);
}

const GameEvent* find_event(const std::vector<GameEvent>& events, GameEventKind kind)
{
    const auto it = std::find_if(events.begin(), events.end(), [&](const GameEvent& event) {
        return event.kind == kind;
    });
    return it == events.end() ? nullptr : &*it;
}

void remote_route_completion_obeys_communications_delay()
{
    auto state = survey_fixture({420.0, 0.0}, {450.0, 0.0}, 10);
    state.players.front().surveyedStars.push_back(2);
    auto& scout = state.fleets.front();
    scout.destination = Position{450.0, 0.0};
    scout.fuel = 300.0;

    const TurnProcessor processor;
    const auto turn11 = processor.process_with_events(state, {});
    assert(find_event(turn11.events, GameEventKind::RouteCompleted) == nullptr);
    assert(turn11.state.players.front().pendingPlayerReports.size() == 1);
    const auto& pending = turn11.state.players.front().pendingPlayerReports.front();
    assert(pending.kind == PlayerReportKind::RouteCompleted);
    assert(pending.observedTurn == 11);
    assert(pending.deliveryTurn == 14);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    const auto turn13 = processor.process_with_events(turn12.state, {});
    const auto turn14 = processor.process_with_events(turn13.state, {});
    const auto* event = find_event(turn14.events, GameEventKind::RouteCompleted);
    assert(event);
    assert(event->turn == 14);
    assert(event->observedTurn == 11);
    assert(event->fleet == scout.id);
    assert(event->star == 2);
    assert(same_position(event->position, {450.0, 0.0}));
    assert(turn14.state.players.front().pendingPlayerReports.empty());
}

void intermediate_waypoint_emits_arrival_not_completion()
{
    auto state = survey_fixture({0.0, 0.0}, {50.0, 0.0});
    state.players.front().surveyedStars.push_back(2);
    auto& scout = state.fleets.front();
    scout.destination = Position{50.0, 0.0};
    scout.waypointQueue.push_back({{100.0, 0.0}, 8, {}});
    scout.fuel = 300.0;

    const TurnProcessor processor;
    const auto result = processor.process_with_events(state, {});
    const auto* arrived = find_event(result.events, GameEventKind::FleetArrived);
    assert(arrived);
    assert(find_event(result.events, GameEventKind::RouteCompleted) == nullptr);
    assert(arrived->star == 2);
    assert(result.state.fleets.front().destination.has_value());
    assert(same_position(*result.state.fleets.front().destination, {100.0, 0.0}));
}

void fuel_stall_warns_once_and_only_after_delivery()
{
    auto state = survey_fixture({420.0, 0.0}, {500.0, 0.0}, 10);
    state.players.front().surveyedStars.push_back(2);
    auto& scout = state.fleets.front();
    scout.destination = Position{500.0, 0.0};
    scout.fuel = 0.0;

    const TurnProcessor processor;
    const auto turn11 = processor.process_with_events(state, {});
    assert(turn11.state.fleets.front().fuelStalled);
    assert(find_event(turn11.events, GameEventKind::FleetStalledForFuel) == nullptr);
    assert(turn11.state.players.front().pendingPlayerReports.size() == 1);
    assert(turn11.state.players.front().pendingPlayerReports.front().deliveryTurn == 14);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    assert(turn12.state.players.front().pendingPlayerReports.size() == 1);
    const auto turn13 = processor.process_with_events(turn12.state, {});
    assert(find_event(turn13.events, GameEventKind::FleetStalledForFuel) == nullptr);
    const auto turn14 = processor.process_with_events(turn13.state, {});
    const auto* warning = find_event(turn14.events, GameEventKind::FleetStalledForFuel);
    assert(warning);
    assert(warning->severity == GameEventSeverity::Warning);

    const auto turn15 = processor.process_with_events(turn14.state, {});
    assert(find_event(turn15.events, GameEventKind::FleetStalledForFuel) == nullptr);
    assert(turn15.state.players.front().pendingPlayerReports.empty());
}

void local_production_completion_is_immediate_and_deterministic()
{
    auto state = make_demo_game();
    state.planets.front().productionQueue.push_back({ProductionKind::Factory, 0, 0});
    const TurnProcessor processor;

    const auto first = processor.process_with_events(state, {});
    const auto replay = processor.process_with_events(state, {});
    const auto* event = find_event(first.events, GameEventKind::ProductionCompleted);
    const auto* replayEvent = find_event(replay.events, GameEventKind::ProductionCompleted);
    assert(event && replayEvent);
    assert(event->turn == 2);
    assert(event->observedTurn == 2);
    assert(event->recipient == 1);
    assert(event->planet == 1);
    assert(event->star == 1);
    assert(event->productionKind == ProductionKind::Factory);
    assert(event->quantity == 5);
    assert(event->id == replayEvent->id);

    auto shipState = make_demo_game();
    shipState.planets.front().productionQueue.push_back({
        ProductionKind::ColonyShip,
        0,
        kColonyShipDesignId,
    });
    const auto shipResult = processor.process_with_events(shipState, {});
    const auto* shipEvent = find_event(shipResult.events, GameEventKind::ProductionCompleted);
    assert(shipEvent);
    assert(shipEvent->productionKind == ProductionKind::ColonyShip);
    assert(shipEvent->shipDesign == kColonyShipDesignId);
    assert(shipEvent->fleet == 2);
    assert(shipEvent->quantity == 2);
}

void colony_founding_emits_a_player_event()
{
    auto state = make_demo_game();
    state.players.front().surveyedStars.push_back(2);
    const auto* target = find_star(state, 2);
    assert(target);
    auto& colonyShip = state.fleets.front();
    colonyShip.position = target->position;
    colonyShip.design = kColonyShipDesignId;
    colonyShip.ships = {{kColonyShipDesignId, 1}};
    colonyShip.role = FleetRole::ColonyShip;
    colonyShip.colonists = 500;

    PlayerOrders orders;
    orders.player = 1;
    orders.orders.push_back(ColonizePlanetOrder{colonyShip.id, 2});
    const TurnProcessor processor;
    const auto result = processor.process_with_events(state, {orders});

    const auto* event = find_event(result.events, GameEventKind::ColonyFounded);
    assert(event);
    assert(event->recipient == 1);
    assert(event->star == 2);
    assert(event->planet == 2);
    assert(event->fleet == 1);
    assert(event->shipDesign == kColonyShipDesignId);
    assert(result.state.planets[1].owner == 1);
    assert(result.state.fleets.empty());
}

void mineral_shortage_warns_once_per_blocked_transition()
{
    auto state = make_demo_game();
    auto& earth = state.planets.front();
    earth.population = 1;
    earth.industry = 1;
    earth.minerals = {};
    earth.productionQueue.push_back({ProductionKind::Factory, 0, 0});
    const TurnProcessor processor;

    const auto first = processor.process_with_events(state, {});
    const auto replay = processor.process_with_events(state, {});
    const auto* warning = find_event(first.events, GameEventKind::ProductionWaitingForMinerals);
    const auto* replayWarning = find_event(replay.events, GameEventKind::ProductionWaitingForMinerals);
    assert(warning && replayWarning);
    assert(warning->severity == GameEventSeverity::Warning);
    assert(warning->planet == 1);
    assert(warning->productionKind == ProductionKind::Factory);
    assert(warning->id == replayWarning->id);
    assert(first.state.planets.front().productionWaitingForMinerals);

    auto stillBlocked = first.state;
    stillBlocked.planets.front().minerals = {};
    const auto second = processor.process_with_events(stillBlocked, {});
    assert(find_event(second.events, GameEventKind::ProductionWaitingForMinerals) == nullptr);
    assert(second.state.planets.front().productionWaitingForMinerals);
}

} // namespace

int main()
{
    ordinary_scanner_reports_system_contact_without_planet_data();
    penetrating_scanner_estimates_planet_during_a_flyby();
    remote_report_remains_in_flight_until_delivery();
    arrival_and_dwell_progress_through_orbital_and_geological_surveys();
    remote_route_completion_obeys_communications_delay();
    intermediate_waypoint_emits_arrival_not_completion();
    fuel_stall_warns_once_and_only_after_delivery();
    local_production_completion_is_immediate_and_deterministic();
    colony_founding_emits_a_player_event();
    mineral_shortage_warns_once_per_blocked_transition();
    std::cout << "player knowledge tests passed\n";
    return 0;
}
