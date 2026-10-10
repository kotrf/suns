#pragma once
#include "suns/game_event.hpp"
#include <span>

namespace suns {
[[nodiscard]] double minefield_radius(const Minefield&);
[[nodiscard]] std::uint8_t minefield_safe_warp(std::uint8_t kind);
[[nodiscard]] double fleet_cloak_fraction(const GameState&, const Fleet&);
[[nodiscard]] double fleet_tachyon_fraction(const GameState&, const Fleet&);
[[nodiscard]] double strategic_sensor_range(const GameState&, const Fleet&);
[[nodiscard]] double fleet_detection_range(const GameState&, const Fleet&, double scannerRange,
    double tachyon = 0, bool passive = false);
[[nodiscard]] bool colony_detects_fleet(const GameState&, const Planet&, const Fleet&);
[[nodiscard]] bool detector_detects_fleet(const GameState&, const Fleet&, const Fleet&);
[[nodiscard]] bool strategic_task_available(const GameState&, const Fleet&, FleetTask);
[[nodiscard]] std::string fleet_task_name(FleetTask);
void resolve_bombardments(GameState&, std::uint64_t);
void advance_minefields(GameState&);
// Called on the actual travelled segment, before arrival actions and combat.
// Stops at the first dangerous field boundary; consumes mines and damages armor.
[[nodiscard]] bool apply_minefield_crossing(GameState&, Fleet&, Position start, Position& end,
    std::span<const Minefield> navigationFields = {});
[[nodiscard]] Position minefield_navigation_endpoint(const GameState&, const Fleet&, Position end);
void run_strategic_tasks(GameState&, std::uint64_t);
void observe_strategic_objects(GameState&, std::uint64_t);
void deliver_strategic_intel(GameState&);
void apply_scientific_data(GameState&, Player&, const PendingPlayerReport&, std::vector<GameEvent>&);
[[nodiscard]] bool submit_electronics_command(GameState&, PlayerId, FleetId, ElectronicsProgram);
void apply_electronics_program(Fleet&, ElectronicsProgram);
[[nodiscard]] std::uint64_t fleet_available_turn(const Fleet&, std::uint64_t);
[[nodiscard]] std::uint64_t report_transmission_turn(const GameState&, PlayerId, Position,
    std::uint64_t observationTurn, FleetId observer = 0);
[[nodiscard]] std::uint32_t communication_jamming_delay(const GameState&, PlayerId, Position);
[[nodiscard]] bool fleet_transmits(const Fleet&, std::uint64_t);
}
