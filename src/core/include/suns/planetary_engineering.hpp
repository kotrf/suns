#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr std::uint32_t kEngineeringWork = 2400;
inline constexpr std::uint32_t kEngineeringWorkPerYear = 80;
inline constexpr std::uint8_t kEngineeringStrength = 10;
inline constexpr MineralCargo kEngineeringMineralsPerWork{0.1,0.05,0.1};
[[nodiscard]] bool engineering_kind(ProductionKind);
[[nodiscard]] ProductionKind engineering_production_kind(PlanetaryEngineeringKind);
[[nodiscard]] PlanetaryEngineeringKind engineering_project_kind(ProductionKind);
[[nodiscard]] std::string engineering_name(PlanetaryEngineeringKind);
[[nodiscard]] std::uint32_t engineering_work(PlanetaryEngineeringKind);
[[nodiscard]] std::uint8_t engineering_max_strength(PlanetaryEngineeringKind);
[[nodiscard]] bool compact_study_received(const GameState&,PlayerId);
[[nodiscard]] bool planetary_engineering_available(const GameState&,PlayerId,PlanetaryEngineeringKind = PlanetaryEngineeringKind::OrbitalMirrors);
[[nodiscard]] bool engineering_operation_available(const GameState&,const Planet&);
[[nodiscard]] bool engineering_station_available(const GameState&,const Planet&);
[[nodiscard]] bool engineering_state_valid(const PlanetaryEngineering&);
[[nodiscard]] PlanetaryEngineering engineering_observation(const Planet&);
[[nodiscard]] PlanetEnvironment engineered_environment(const Planet&);
[[nodiscard]] bool engineering_scheduled(const Planet&);
[[nodiscard]] EngineeringBlock engineering_block(const GameState&,const Planet&);
[[nodiscard]] std::string engineering_block_name(EngineeringBlock);
bool apply_planetary_engineering_order(const GameState&,Planet&,const struct SetPlanetaryEngineeringOrder&);
std::uint32_t advance_planetary_engineering(const GameState&,Planet&,std::uint32_t available,bool supported,std::uint32_t annualRemaining);
void refresh_planetary_engineering(GameState&);
void publish_planetary_engineering_reports(GameState&,const GameState& previous);
} // namespace suns
