#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr std::uint8_t kOrbitalAssemblyYears = 6;
inline constexpr std::uint64_t kOrbitalWorkforce = 25000;
inline constexpr std::uint32_t kOrbitalAnnualWork = 40;
[[nodiscard]] const Fleet* orbital_base_at(const GameState&, PlanetId);
[[nodiscard]] std::uint8_t orbital_background(const StarSystem&);
[[nodiscard]] std::uint8_t orbital_screen(const GameState&, const Fleet&);
[[nodiscard]] std::uint64_t orbital_radiation_losses(const GameState&, const Fleet&);
[[nodiscard]] std::string orbital_base_status(const GameState&, const Fleet&);
[[nodiscard]] bool orbital_base_command_valid(const OrbitalBaseCommand&);
[[nodiscard]] bool submit_orbital_base_command(GameState&, PlayerId, FleetId, const OrbitalBaseCommand&);
void apply_orbital_base_command(GameState&, Fleet&, const OrbitalBaseCommand&);
void plan_orbital_bases(const struct PlayerView&, struct PlayerOrders&);
void service_orbital_bases(GameState&, std::uint64_t);
} // namespace suns
