#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace {

using namespace suns;

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

void local_report_updates_knowledge_and_emits_a_stable_event()
{
    const auto initial = survey_fixture({100.0, 0.0}, {150.0, 0.0});
    const TurnProcessor processor;

    const auto first = processor.process_with_events(initial, {});
    const auto replay = processor.process_with_events(initial, {});

    assert(first.state.turn == 2);
    assert(is_surveyed(first.state, 1, 2));
    assert(first.state.players.front().pendingSurveyReports.empty());
    assert(first.events.size() == 1);
    assert(first.events.front().kind == GameEventKind::SystemSurveyed);
    assert(first.events.front().recipient == 1);
    assert(first.events.front().star == 2);
    assert(first.events.front().planet == 2);
    assert(first.events.front().fleet == 1);
    assert(first.events.front().observedTurn == 2);
    assert(first.events.front().turn == 2);
    assert(first.events.front().id == replay.events.front().id);
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
    assert(pending.deliveryTurn == 13);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    assert(!is_surveyed(turn12.state, 1, 2));
    assert(turn12.events.empty());
    assert(turn12.state.players.front().pendingSurveyReports.size() == 1);
    assert(turn12.state.players.front().pendingSurveyReports.front().observedTurn == 11);

    const auto turn13 = processor.process_with_events(turn12.state, {});
    assert(is_surveyed(turn13.state, 1, 2));
    assert(turn13.state.players.front().pendingSurveyReports.empty());
    assert(turn13.events.size() == 1);
    assert(turn13.events.front().observedTurn == 11);
    assert(turn13.events.front().turn == 13);

    const auto turn14 = processor.process_with_events(turn13.state, {});
    assert(turn14.events.empty());
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
    assert(turn11.state.players.front().pendingPlayerReports.front().deliveryTurn == 13);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    assert(turn12.state.players.front().pendingPlayerReports.size() == 1);
    const auto turn13 = processor.process_with_events(turn12.state, {});
    const auto* warning = find_event(turn13.events, GameEventKind::FleetStalledForFuel);
    assert(warning);
    assert(warning->severity == GameEventSeverity::Warning);

    const auto turn14 = processor.process_with_events(turn13.state, {});
    assert(find_event(turn14.events, GameEventKind::FleetStalledForFuel) == nullptr);
    assert(turn14.state.players.front().pendingPlayerReports.empty());
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
}

} // namespace

int main()
{
    local_report_updates_knowledge_and_emits_a_stable_event();
    remote_report_remains_in_flight_until_delivery();
    remote_route_completion_obeys_communications_delay();
    intermediate_waypoint_emits_arrival_not_completion();
    fuel_stall_warns_once_and_only_after_delivery();
    local_production_completion_is_immediate_and_deterministic();
    std::cout << "player knowledge tests passed\n";
    return 0;
}
