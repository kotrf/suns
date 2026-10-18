#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr double kWorkshopRepairKt = 100.0;
// Minerals per kt of restored dry ship structure (hull and fitted equipment).
inline constexpr MineralCargo kWorkshopMineralsPerKt{0.1, 0.05, 0.05};
[[nodiscard]] const Fleet* find_service_fleet(const GameState&, FleetId);
[[nodiscard]] double fleet_dry_mass(const GameState&, const Fleet&);
[[nodiscard]] std::uint32_t mobile_base_hulls(const GameState&, const Fleet&);
[[nodiscard]] bool mobile_base_dock(const GameState&, const Fleet&);
[[nodiscard]] double mobile_base_repair_capacity(const GameState&, const Fleet&);
[[nodiscard]] bool mobile_base_program_valid(const MobileBaseProgram&);
[[nodiscard]] std::string mobile_base_service_error(const GameState&, const Fleet&, std::uint64_t serviceTurn);
[[nodiscard]] bool submit_mobile_base_command(GameState&, PlayerId, FleetId, const MobileBaseProgram&);
// Called once after movement/combat and normal onboard repair. FleetId order
// breaks ties between bases; each base's persistent list orders its clients.
[[nodiscard]] bool transfer_mobile_base_fuel(GameState&, PlayerId, const struct TransferFleetFuelOrder&, std::uint64_t);
void service_mobile_bases(GameState&, std::uint64_t serviceTurn);
} // namespace suns
