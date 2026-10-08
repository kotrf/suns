#include "suns/wormholes.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace suns;
namespace {
GameState fixture(Position entry = {})
{
    auto state = make_demo_game();
    state.wormholeRules.spawnChancePerTurn = 0;
    state.wormholeRules.driftPerTurn = 0;
    state.wormholeRules.relocationChance = 0;
    state.wormholeRules.minimumLossChance = 0;
    state.wormholeRules.instabilityLossChance = 0;
    state.wormholes.push_back({{{{10, entry, WormholeSignature::Strong},
        {11, {400, 100}, WormholeSignature::Weak}}}, 1, 500, 0.8});
    state.nextWormholeEndpointId = 12;
    state.fleets.front().position = entry;
    state.fleets.front().telemetry.position = entry;
    state.fleets.front().telemetry.observedTurn = state.turn;
    state.players.front().wormholeKnowledge.push_back({10, entry, 1, WormholeStability::Stable});
    return state;
}
FleetArrivalAction entry_action(WormholeEndpointId id = 10)
{
    FleetArrivalAction action;
    action.kind = FleetArrivalActionKind::EnterWormhole;
    action.wormholeEndpoint = id;
    return action;
}
bool has_event(const std::vector<GameEvent>& events, GameEventKind kind)
{
    return std::any_of(events.begin(), events.end(), [=](const auto& e) { return e.kind == kind; });
}

void explicit_entry_and_delayed_exit()
{
    auto state = fixture();
    const auto ordinary = TurnProcessor{}.process(state, {{1, {MoveFleetOrder{1, {}, 8}}}});
    assert(same_position(ordinary.fleets.front().position, {}));
    assert(ordinary.wormholeTransits.empty());
    state.players.front().wormholeKnowledge.clear();
    assert(!submit_fleet_route_command(state, 1, 1, {}, 8, entry_action(), {}));
    state.players.front().wormholeKnowledge.push_back({10, {}, 1});
    assert(!submit_fleet_route_command(state, 1, 1, {}, 8, entry_action(), {}));
    state.players.front().wormholeKnowledge.front().stability = WormholeStability::Stable;
    assert(!submit_fleet_route_command(state, 1, 1, {}, 8, entry_action(), {}, false, 1));
    assert(!submit_fleet_route_command(state, 1, 1, {}, 8, entry_action(), {{{20, 0}, 8}}));
    auto result = TurnProcessor{}.process_with_events(state, {{1, {MoveFleetOrder{1, {}, 8, entry_action()}}}});
    assert(same_position(result.state.fleets.front().position, {400, 100}));
    assert(has_event(result.events, GameEventKind::WormholeEntered));
    assert(!has_event(result.events, GameEventKind::WormholeEmerged));
    assert(!known_wormhole(result.state, 1, 11));
    auto packet = make_player_view(result.state, 1).state;
    assert(packet.wormholes.empty() && packet.galaxySeed == 0);
    assert(packet.players.front().pendingWormholeReports.empty());
    assert(!known_wormhole(packet, 1, 11));
    assert(!same_position(packet.fleets.front().position, {400, 100}));
    for (int i = 0; i < 5; ++i) result = TurnProcessor{}.process_with_events(result.state, {});
    assert(known_wormhole(result.state, 1, 11));
    assert(known_wormhole(result.state, 1, 10)->linkedEndpoint == 11);
    assert(result.state.wormholeTransits.front().status == WormholeTransitStatus::EmergenceConfirmed);
}

void hidden_drift_and_detection()
{
    auto a = fixture({300, 100});
    a.wormholeRules.driftPerTurn = 5;
    auto b = a;
    for (int i = 0; i < 5; ++i) { advance_wormholes(a); advance_wormholes(b); ++a.turn; ++b.turn; }
    for (int side = 0; side < 2; ++side) {
        assert(same_position(a.wormholes[0].endpoints[side].position, b.wormholes[0].endpoints[side].position));
        assert(distance_between(a.wormholes[0].endpoints[side].position, fixture({300, 100}).wormholes[0].endpoints[side].position) > 5);
    }
    assert(same_position(known_wormhole(a, 1, 10)->lastPosition, {300, 100}));
    auto local = fixture();
    local.players.front().wormholeKnowledge.clear();
    local.wormholes.front().endpoints[0].position = {60, 0};
    local.fleets.front().position = {20, 0};
    observe_current_wormholes(local, 1);
    auto events = deliver_wormhole_reports(local);
    assert(known_wormhole(local, 1, 10));
    assert(known_wormhole(local, 1, 10)->stability == WormholeStability::Unknown);
    assert(!known_wormhole(local, 1, 11));
    local.wormholes.front().endpoints[1].position = {5, 0};
    observe_current_wormholes(local, 1);
    (void)deliver_wormhole_reports(local);
    assert(!known_wormhole(local, 1, 11)); // Even close weak signatures require equipment.
    local.shipDesigns.front().components.push_back(ShipComponentType::AnomalyDetector);
    observe_current_wormholes(local, 1);
    (void)deliver_wormhole_reports(local);
    assert(known_wormhole(local, 1, 11));
    assert(known_wormhole(local, 1, 11)->stability == WormholeStability::Stable);
    assert(!component_available_to_player(local, 1, ShipComponentType::AnomalyDetector));
    local.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Electronics)] = 6;
    assert(component_available_to_player(local, 1, ShipComponentType::AnomalyDetector));
    // An old coordinate is not an automatic portal or omniscient homing beacon.
    auto missed = fixture();
    missed.wormholes.front().endpoints.front().position = {300, 0};
    auto failure = TurnProcessor{}.process_with_events(missed, {{1, {MoveFleetOrder{1, {}, 8, entry_action()}}}});
    assert(has_event(failure.events, GameEventKind::WormholeEntryMissed));
    assert(failure.state.wormholeTransits.empty());
    auto reacquired = fixture();
    reacquired.wormholes.front().endpoints.front().position = {20, 0};
    auto success = TurnProcessor{}.process(reacquired, {{1, {MoveFleetOrder{1, {}, 8, entry_action()}}}});
    assert(same_position(success.fleets.front().position, {400, 100}));
}

void loss_is_physical_but_not_omniscient()
{
    auto state = fixture({300, 0});
    state.wormholeRules.minimumLossChance = 1;
    // Use an already active onboard programme so command latency is not confused with entry latency.
    state.fleets.front().destination = Position{300, 0};
    state.fleets.front().arrivalAction = entry_action();
    auto result = TurnProcessor{}.process_with_events(state, {});
    assert(result.state.fleets.empty());
    assert(!has_event(result.events, GameEventKind::WormholeEntered));
    assert(!has_event(result.events, GameEventKind::WormholePresumedLost));
    assert(empire_turn_statistics(result.state, 1).fleets == 1);
    const auto packet = make_player_view(result.state, 1).state;
    assert(packet.fleets.size() == 1 && packet.wormholeTransits.empty());
    assert(packet.players.front().pendingWormholeReports.empty());
    const auto overdue = result.state.wormholeTransits.front().overdueTurn;
    const auto presumed = result.state.wormholeTransits.front().presumedLostTurn;
    bool entered = false, overdueSeen = false, lossSeen = false;
    while (result.state.turn < presumed) {
        result = TurnProcessor{}.process_with_events(result.state, {});
        if (has_event(result.events, GameEventKind::WormholeEntered)) entered = true;
        if (has_event(result.events, GameEventKind::WormholeOverdue)) { assert(result.state.turn == overdue); overdueSeen = true; }
        if (has_event(result.events, GameEventKind::WormholePresumedLost)) { assert(result.state.turn == presumed); lossSeen = true; }
        if (result.state.turn < presumed) assert(make_player_view(result.state, 1).state.fleets.size() == 1);
    }
    assert(entered && overdueSeen && lossSeen);
    assert(make_player_view(result.state, 1).state.fleets.empty());
    assert(empire_turn_statistics(result.state, 1).fleets == 0);
}

void lifecycle_is_seeded_and_collapse_observed()
{
    auto a = fixture();
    a.wormholes.clear();
    a.turn = 5;
    a.galaxySeed = 42;
    a.wormholeRules.spawnChancePerTurn = 1;
    a.wormholeRules.maximumPairs = 1;
    auto b = a;
    advance_wormholes(a); advance_wormholes(b);
    assert(a.wormholes.size() == 1 && b.wormholes.size() == 1);
    assert(a.wormholes.front().endpoints.front().id == b.wormholes.front().endpoints.front().id);
    assert(same_position(a.wormholes.front().endpoints.front().position, b.wormholes.front().endpoints.front().position));
    assert(a.wormholes.front().collapseTurn > a.turn);
    auto collapsed = fixture();
    collapsed.wormholes.front().collapseTurn = 2;
    auto result = TurnProcessor{}.process_with_events(collapsed, {});
    assert(result.state.wormholes.empty());
    assert(has_event(result.events, GameEventKind::WormholeCollapsed));
    assert(known_wormhole(result.state, 1, 10)->collapsed);
    auto unseen = fixture({300, 0});
    unseen.fleets.front().position = {};
    unseen.wormholes.front().collapseTurn = 2;
    result = TurnProcessor{}.process_with_events(unseen, {});
    assert(result.state.wormholes.empty());
    assert(!has_event(result.events, GameEventKind::WormholeCollapsed));
    assert(!known_wormhole(result.state, 1, 10)->collapsed);
}

void seeded_risk_and_boundary_motion()
{
    std::uint32_t losses = 0, equippedLosses = 0;
    for (FleetId id = 100; id < 600; ++id) {
        auto baseline = fixture();
        baseline.wormholes.front().stability = 0.1;
        baseline.wormholeRules.minimumLossChance = 0.01;
        baseline.wormholeRules.instabilityLossChance = 0.12;
        auto replay = baseline, equipped = baseline;
        auto fleet = baseline.fleets.front(), repeatedFleet = fleet, equippedFleet = fleet;
        fleet.id = repeatedFleet.id = equippedFleet.id = id;
        equipped.shipDesigns.front().components.push_back(ShipComponentType::AnomalyDetector);
        const auto lost = enter_wormhole(baseline, fleet, 10);
        assert(lost == enter_wormhole(replay, repeatedFleet, 10));
        const auto equippedLost = enter_wormhole(equipped, equippedFleet, 10);
        assert(!equippedLost || lost); // Same sample, smaller threshold, never a new loss.
        losses += lost;
        equippedLosses += equippedLost;
    }
    assert(losses > 0 && equippedLosses > 0 && equippedLosses < losses);
    auto moving = fixture();
    auto& mouth = moving.wormholes.front().endpoints.front();
    mouth.position = {449, 324};
    mouth.driftDirection = {0.8, 0.6};
    moving.wormholeRules.driftPerTurn = 10;
    advance_wormholes(moving);
    assert(mouth.driftDirection.x < 0 && mouth.driftDirection.y < 0);
    const auto edge = mouth.position;
    ++moving.turn;
    advance_wormholes(moving);
    assert(mouth.position.x < edge.x && mouth.position.y < edge.y);
}
} // namespace

int main()
{
    explicit_entry_and_delayed_exit();
    hidden_drift_and_detection();
    loss_is_physical_but_not_omniscient();
    lifecycle_is_seeded_and_collapse_observed();
    seeded_risk_and_boundary_motion();
    std::cout << "wormhole tests passed\n";
}
