#pragma once

#include "suns/game_event.hpp"

#include <cstdint>
#include <vector>

namespace suns {

// Record every system crossed by a fleet's sensor footprint. Physical
// observation is authoritative, but owner knowledge changes only when the
// resulting report reaches a communication relay.
void observe_fleet_sensor_sweep(
    GameState& state,
    const Fleet& fleet,
    Position start,
    Position end,
    std::uint64_t observationTurn);

// Observe stationary colony/fleet coverage at the end of the movement phase.
// Duplicate reports already in flight are coalesced deterministically.
void observe_current_sensor_coverage(GameState& state, std::uint64_t observationTurn);

// Deliver reports due at the current planning boundary. Delivery is the only
// operation that mutates Player::surveyedStars and emits briefing events.
[[nodiscard]] std::vector<GameEvent> deliver_due_survey_reports(GameState& state);

} // namespace suns
