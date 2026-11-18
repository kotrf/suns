#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr std::uint8_t kTransformationYears=30;
inline constexpr std::uint32_t kExperimentalWormholeYears=40;
inline constexpr std::uint32_t kTransformationProgressBase=1000;
inline constexpr std::uint32_t kExperimentOpenedProgress=2000;
[[nodiscard]] bool surface_sheltered(const Planet&);
[[nodiscard]] bool surface_shelter_accepts(const Planet&,std::uint64_t residents);
[[nodiscard]] bool megaproject_kind(PlanetaryEngineeringKind);
[[nodiscard]] bool megaproject_state_valid(const PlanetaryEngineering&);
[[nodiscard]] bool engineering_dates_valid(const PlanetaryEngineering&,std::uint64_t);
[[nodiscard]] PlanetEnvironment transformation_target(const RaceProfile&);
// Only explicit, revalidated commitment can change a project's phase.
bool apply_megaproject_action(const GameState&,Planet&,const struct SetPlanetaryEngineeringOrder&);
// True only when a new rift requires a fresh local observation pass.
bool advance_megaprojects(GameState&);
} // namespace suns
