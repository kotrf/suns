#pragma once
#include "suns/game_event.hpp"
#include <span>

namespace suns {
inline constexpr double kAreaJammerRangeLy = 90.0;
inline constexpr double kAreaJammerSensorReduction = 0.40;
inline constexpr double kDecoyPulseRangeLy = 240.0;
inline constexpr double kDecoyPulseStrength = 0.18;

[[nodiscard]] double minefield_radius(const Minefield&);
[[nodiscard]] std::uint8_t minefield_safe_warp(std::uint8_t kind);
[[nodiscard]] double fleet_cloak_fraction(const GameState&, const Fleet&);
[[nodiscard]] double fleet_tachyon_fraction(const GameState&, const Fleet&);
[[nodiscard]] double strategic_sensor_range(const GameState&, const Fleet&, std::uint64_t atTurn = 0);
[[nodiscard]] double sensor_interference(const GameState&, PlayerId, Position, std::uint64_t atTurn = 0);
[[nodiscard]] bool fleet_has_area_jammer(const GameState&, const Fleet&);
[[nodiscard]] bool fleet_area_jammer_active(const GameState&, const Fleet&, std::uint64_t turn);
[[nodiscard]] bool station_area_jammer_active(const OrbitalStation&);
[[nodiscard]] std::vector<Position> hostile_area_jammers(const GameState&, PlayerId, std::uint64_t turn);
struct SensorInterferenceSpan {
    double begin, end, strength; // Interior of a normalized flight interval.
};
[[nodiscard]] std::vector<SensorInterferenceSpan> sensor_interference_spans(
    const GameState&, PlayerId, Position start, Position end, std::uint64_t turn);
[[nodiscard]] double fleet_detection_range(const GameState&, const Fleet&, double scannerRange,
    double tachyon = 0, bool passive = false, std::uint64_t atTurn = 0);
[[nodiscard]] bool colony_detects_fleet(const GameState&, const Planet&, const Fleet&, std::uint64_t atTurn = 0);
[[nodiscard]] bool detector_detects_fleet(const GameState&, const Fleet&, const Fleet&, std::uint64_t atTurn = 0);
[[nodiscard]] bool strategic_task_available(const GameState&, const Fleet&, FleetTask);
// Equipment-only check: a travelling fleet may program work for a later arrival.
[[nodiscard]] bool fleet_task_equipment_available(const GameState&, const Fleet&, FleetTask);
// Planning validation uses received target knowledge, never hidden physical objects.
// Pass the complete action to retain a named recovery target after coarse reports.
[[nodiscard]] std::string fleet_work_order_error(const GameState&, const Fleet&, Position, const FleetArrivalAction&);
[[nodiscard]] std::string fleet_work_order_error(const GameState&, const Fleet&, Position, FleetArrivalActionKind);
[[nodiscard]] std::string fleet_task_name(FleetTask);
void resolve_bombardments(GameState&, std::uint64_t);
void advance_minefields(GameState&);
// Called on the actual travelled segment, before arrival actions and combat.
// Stops at the first dangerous field boundary; consumes mines and damages armor.
[[nodiscard]] bool apply_minefield_crossing(GameState&, Fleet&, Position start, Position& end,
    std::span<const Minefield> navigationFields = {});
[[nodiscard]] Position minefield_navigation_endpoint(const GameState&, const Fleet&, Position end);
void run_strategic_tasks(GameState&, std::uint64_t);
[[nodiscard]] bool same_strategic_intel_subject(const StrategicIntel&, const StrategicIntel&);
[[nodiscard]] const char* signal_activity_name(SignalActivity);
void discard_redundant_emission_intel(Player&, std::uint64_t turn);
void discard_redundant_emission_reports(Player&, std::uint64_t turn);
void record_decoy_pulse(GameState&, const Fleet&, std::uint64_t turn);
void observe_emission_pulses(GameState&, std::uint64_t);
void observe_strategic_objects(GameState&, std::uint64_t);
void deliver_strategic_intel(GameState&);
void apply_scientific_data(GameState&, Player&, const PendingPlayerReport&, std::vector<GameEvent>&);
bool collect_field_science_once(GameState&,Fleet&,std::uint64_t key,Position,StarId,ResearchField,std::uint32_t,std::uint64_t);
[[nodiscard]] bool submit_electronics_command(GameState&, PlayerId, FleetId, ElectronicsProgram);
void apply_electronics_program(Fleet&, ElectronicsProgram);
void wake_radio_after_encounter(GameState&, Fleet&, std::uint64_t turn, bool combat);
[[nodiscard]] std::uint64_t fleet_available_turn(const Fleet&, std::uint64_t);
[[nodiscard]] std::uint64_t report_transmission_turn(const GameState&, PlayerId, Position,
    std::uint64_t observationTurn, FleetId observer = 0);
[[nodiscard]] std::uint32_t communication_jamming_delay(const GameState&, PlayerId, Position, std::uint64_t atTurn = 0);
[[nodiscard]] bool fleet_transmits(const Fleet&, std::uint64_t);
}
