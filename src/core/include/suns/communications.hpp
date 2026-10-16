#pragma once

#include "suns/game_state.hpp"

#include <cstdint>
#include <limits>

namespace suns {

inline constexpr std::uint64_t kAwaitingTransmission = std::numeric_limits<std::uint64_t>::max();

// Shared host outbox order for dispatch and save metadata. Already dispatched
// packets have a finite deliveryTurn and transmissionFleet == 0.
template<class State, class Visit> void visit_fleet_report_outbox(State& state, Visit visit)
{
    for(auto& player:state.players) {
        for(auto& r:player.pendingSurveyReports) visit(player.id,r);
        for(auto& r:player.pendingPlayerReports) visit(player.id,r);
        for(auto& r:player.pendingWormholeReports) visit(player.id,r);
        for(auto& r:player.pendingStrategicIntel) visit(player.id,r);
        for(auto& r:player.pendingEnemyContacts) visit(player.id,r);
        for(auto& r:player.pendingFriendlyFleetContacts) visit(player.id,r);
        for(auto& r:player.pendingSensorStatus) visit(player.id,r);
        for(auto& r:player.fieldScience) visit(player.id,r);
    }
}

// Schedule an actual outgoing observation. A passive/silent source keeps it
// aboard until transmission or local exchange with a friendly populated colony;
// listening for incoming orders does not imply any outgoing packet.
[[nodiscard]] std::uint64_t fleet_report_delivery_turn(const GameState&, PlayerId,
    Position source, std::uint64_t observedTurn, FleetId observer=0);
[[nodiscard]] FleetId held_report_source(const GameState&, PlayerId, Position,
    std::uint64_t observedTurn, FleetId observer=0);
void dispatch_fleet_report_outbox(GameState&);

// Round-trip infrastructure is intentionally simple in the first playable
// slice: friendly established colonies are relay nodes. Later station/ship
// communication modules can replace the node source without changing callers.
[[nodiscard]] std::uint32_t communication_delay_turns(
    const GameState& state,
    PlayerId player,
    Position position, std::uint64_t atTurn = 0);

struct CommunicationRoute {
    SignalChannel channel{SignalChannel::Fast};
    Position transmitter, receiver;
    std::uint32_t propagationTurns{}, receiverDelay{};
};
[[nodiscard]] CommunicationRoute communication_route(const GameState&, PlayerId, Position,
    std::uint64_t atTurn = 0);
// Only call for a dispatched packet; pure delay/knowledge queries never emit.
void record_report_transmission(GameState&, PlayerId, Position, std::uint64_t, FleetId observer=0);
void record_command_transmission(GameState&, PlayerId, Position target, std::uint64_t);
[[nodiscard]] std::uint64_t dispatch_report_delivery_turn(GameState&, PlayerId, Position,
    std::uint64_t, FleetId observer=0);

// A fleet can also exchange data through its friendly orbital colony without
// enabling its own long-range transmitter. The colony's network delay applies.
[[nodiscard]] bool fleet_has_instant_link(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint64_t fleet_telemetry_age(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint64_t fleet_position_age(const GameState& state, const Fleet& fleet);
// Only received radar evidence newer than the fleet's own telemetry.
[[nodiscard]] std::optional<ReconContact> fleet_radar_position(const GameState&, const Fleet&);
[[nodiscard]] FleetTelemetry confirmed_fleet_telemetry(const GameState& state, const Fleet& fleet);
[[nodiscard]] Position projected_fleet_position(const GameState& state, const Fleet& fleet);
[[nodiscard]] Fleet fleet_player_view(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::vector<Fleet> missing_fleet_contacts(const GameState&, PlayerId);
[[nodiscard]] bool fleet_route_tasks_valid(const FleetRouteProgram&);

// Queue a route replacement for physical delivery. A zero-delay command is
// applied immediately and therefore preserves the legacy local-fleet behavior.
[[nodiscard]] bool submit_fleet_route_command(
    GameState& state,
    PlayerId player,
    FleetId fleet,
    Position destination,
    std::uint8_t warp,
    FleetArrivalAction arrivalAction,
    const std::vector<FleetWaypoint>& queuedWaypoints,
    bool repeatOrders = false,
    FleetId targetFleet = 0,
    bool clearRoute = false);

// Queue a stationary fleet task for the same physical delivery path as route
// programmes. Starting and stopping work therefore respect communication delay.
[[nodiscard]] bool submit_fleet_task_command(
    GameState& state,
    PlayerId player,
    FleetId fleet,
    FleetTask task);

void deliver_due_fleet_commands(GameState& state);
void deliver_due_fleet_telemetry(GameState& state);
void publish_fleet_telemetry(GameState& state, std::uint64_t observationTurn);

} // namespace suns
