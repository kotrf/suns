#pragma once

#include "suns/game_state.hpp"

namespace suns {

[[nodiscard]] bool interstellar_cloud_valid(const InterstellarCloud&);
[[nodiscard]] bool medium_observation_valid(const MediumObservation&);
void generate_interstellar_medium(GameState&);
void observe_interstellar_medium(GameState&, std::uint64_t turn);
// Discard host truth before forecasts/AI; this operation is idempotent.
void project_interstellar_medium(GameState&, PlayerId);
[[nodiscard]] double interstellar_density(const GameState&, Position);
[[nodiscard]] double medium_segment_density(const GameState&, Position start, Position end);
[[nodiscard]] double medium_active_sensor_factor(const GameState&, Position);
[[nodiscard]] double medium_active_path_factor(const GameState&, Position start, Position end);
// Local rate and integrated percentage-point hull damage for the actual fitted
// engines. Movement uses the segment integral, never just endpoint density.
[[nodiscard]] double fleet_medium_damage_rate(const GameState&, const Fleet&, std::uint8_t warp, double density);
[[nodiscard]] double fleet_segment_damage(const GameState&, const Fleet&, Position end);

struct MediumRouteAlternative {
    std::vector<Position> destinations; // includes the final destination
    double distance{}, years{}, fuel{}, damage{};
};
// Uses received profiles only, even when passed host state. Direct plus at most
// two coarse bypasses per known cloud; no global fine-grid pathfinding.
[[nodiscard]] std::vector<MediumRouteAlternative> medium_route_alternatives(
    const GameState&, const Fleet&, Position destination, std::uint8_t warp);

struct SegmentFuelBounds {
    double consumedMinimum{}, consumedMaximum{};
    double changeMinimum{}, changeMaximum{};
};
[[nodiscard]] double fleet_fuel_change_for_segment(const GameState&, const Fleet&, Position end);
[[nodiscard]] double fleet_fuel_consumption_for_segment(const GameState&, const Fleet&, Position end);
[[nodiscard]] double fleet_fuel_affordable_segment_distance(
    const GameState&, const Fleet&, Position destination, double maximumDistance);
// Conservative physical 0..1 density envelope for the specified segment and
// fitted ships; no hidden field is consulted. Costs still include normal rounding.
[[nodiscard]] SegmentFuelBounds fleet_segment_fuel_bounds(const GameState&, const Fleet&, double distance);

} // namespace suns
