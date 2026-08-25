#pragma once

#include "suns/game_event.hpp"

#include <cstdint>
#include <vector>

namespace suns {

// Record a basic scan for every system crossed by a fleet's sensor footprint.
// Physical observation is authoritative, but owner knowledge changes only
// when the resulting staged report reaches a communication relay.
void observe_fleet_sensor_sweep(
    GameState& state,
    const Fleet& fleet,
    Position start,
    Position end,
    std::uint64_t observationTurn);

// Observe stationary colony/fleet coverage at the end of the movement phase.
// Arrival promotes to orbital knowledge; an additional turn in place promotes
// to geology. Reports already in flight are dominance-coalesced deterministically.
void observe_current_sensor_coverage(GameState& state, std::uint64_t observationTurn);

// Deliver reports due at the current planning boundary. Delivery is the only
// operation that mutates Player::surveyedStars and emits briefing events.
[[nodiscard]] std::vector<GameEvent> deliver_due_survey_reports(GameState& state);

// Queue an operational report at the physical source. Delivery latency is
// calculated here so callers never need to duplicate fog-of-war policy.
void queue_player_report(
    GameState& state,
    PlayerId recipient,
    PlayerReportKind kind,
    Position sourcePosition,
    std::uint64_t observationTurn,
    StarId star = 0,
    PlanetId planet = 0,
    FleetId fleet = 0,
    ShipDesignId shipDesign = 0,
    ProductionKind productionKind = ProductionKind::ColonyShip,
    std::uint32_t quantity = 0);

[[nodiscard]] std::vector<GameEvent> deliver_due_player_reports(GameState& state);

} // namespace suns
