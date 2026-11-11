#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr std::uint8_t kOrbitalAssemblyYears = 6;
inline constexpr std::uint64_t kOrbitalWorkforce = 25000;
inline constexpr std::uint32_t kOrbitalAnnualWork = 40;
[[nodiscard]] const Fleet* orbital_base_at(const GameState&, PlanetId);
[[nodiscard]] std::uint8_t orbital_background(const StarSystem&);
[[nodiscard]] std::uint8_t radiation_screen_protection(ShipComponentType);
// Whole-person annual loss, 0.2% per excess dose unit, capped at 20%.
[[nodiscard]] std::uint64_t orbital_radiation_casualties(std::uint64_t, std::uint8_t exposure, std::uint8_t tolerance);
[[nodiscard]] std::uint8_t orbital_screen(const GameState&, const Fleet&);
[[nodiscard]] std::uint64_t orbital_radiation_losses(const GameState&, const Fleet&);
[[nodiscard]] std::string orbital_base_status(const GameState&, const Fleet&);
[[nodiscard]] MineralCargo orbital_base_recovery(const GameState&, const Fleet&);
[[nodiscard]] std::string orbital_retirement_error(const GameState&, const Fleet&, const OrbitalBaseCommand&);
[[nodiscard]] bool orbital_base_command_valid(const OrbitalBaseCommand&);
[[nodiscard]] bool submit_orbital_base_command(GameState&, PlayerId, FleetId, const OrbitalBaseCommand&);
void apply_orbital_base_command(GameState&, Fleet&, const OrbitalBaseCommand&);
void plan_orbital_bases(const struct PlayerView&, struct PlayerOrders&, ComputerState&);
void service_orbital_bases(GameState&, std::uint64_t);
} // namespace suns
