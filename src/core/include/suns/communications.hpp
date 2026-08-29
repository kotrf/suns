#pragma once

#include "suns/game_state.hpp"

#include <cstdint>

namespace suns {

// Round-trip infrastructure is intentionally simple in the first playable
// slice: friendly established colonies are relay nodes. Later station/ship
// communication modules can replace the node source without changing callers.
[[nodiscard]] std::uint32_t communication_delay_turns(
    const GameState& state,
    PlayerId player,
    Position position);

[[nodiscard]] bool fleet_has_instant_link(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint64_t fleet_telemetry_age(const GameState& state, const Fleet& fleet);
[[nodiscard]] FleetTelemetry confirmed_fleet_telemetry(const GameState& state, const Fleet& fleet);
[[nodiscard]] Position projected_fleet_position(const GameState& state, const Fleet& fleet);
[[nodiscard]] Fleet fleet_player_view(const GameState& state, const Fleet& fleet);

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
    bool repeatOrders = false);

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
