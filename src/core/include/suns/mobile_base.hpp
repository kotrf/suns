#pragma once
#include "suns/game_state.hpp"
#include <functional>

namespace suns {
inline constexpr double kWorkshopRepairKt = 100.0;
inline constexpr double kExpeditionGateRangeLy = 300.0;
inline constexpr double kExpeditionGateMassKt = 600.0;
inline constexpr double kExpeditionGateFuelMg = 10.0;
// Minerals per kt of restored dry ship structure (hull and fitted equipment).
inline constexpr MineralCargo kWorkshopMineralsPerKt{0.1, 0.05, 0.05};
[[nodiscard]] const Fleet* find_service_fleet(const GameState&, FleetId);
[[nodiscard]] double fleet_dry_mass(const GameState&, const Fleet&);
[[nodiscard]] std::uint32_t mobile_base_hulls(const GameState&, const Fleet&);
[[nodiscard]] bool mobile_base_dock(const GameState&, const Fleet&);
[[nodiscard]] bool mobile_base_gate(const GameState&, const Fleet&);
[[nodiscard]] std::string mobile_base_gate_error(const GameState&, const Fleet&);
[[nodiscard]] double mobile_base_repair_capacity(const GameState&, const Fleet&);
[[nodiscard]] bool mobile_base_program_valid(const MobileBaseProgram&);
[[nodiscard]] std::string mobile_base_service_error(const GameState&, const Fleet&, std::uint64_t serviceTurn);
[[nodiscard]] bool submit_mobile_base_command(GameState&, PlayerId, FleetId, const MobileBaseProgram&);
[[nodiscard]] bool mobile_gate_transit_valid(const MobileGateTransit&);
[[nodiscard]] std::string mobile_gate_transit_error(const GameState&, const Fleet&, const MobileGateTransit&);
[[nodiscard]] bool submit_mobile_gate_command(GameState&, PlayerId, const struct MobileGateOrder&);
void resolve_mobile_gate_transits(GameState&, const std::function<void(Fleet&)>& beforeJump = {});
// Called once after movement/combat and normal onboard repair. FleetId order
// breaks ties between bases; each base's persistent list orders its clients.
// Transfers between any co-located owned fleets. Mobile bases retain their
// service/docking restrictions and configured donor reserve.
[[nodiscard]] bool transfer_mobile_base_fuel(GameState&, PlayerId, const struct TransferFleetFuelOrder&, std::uint64_t);
void service_mobile_bases(GameState&, std::uint64_t serviceTurn);
} // namespace suns
