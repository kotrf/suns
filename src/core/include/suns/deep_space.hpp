#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr double kOutpostUpkeepMg = 1.0;
inline constexpr double kOutpostRelayRangeLy = 300.0;
inline constexpr double kOutpostStellarClearanceLy = 5.0;
[[nodiscard]] OutpostKind construction_kit_kind(ShipComponentType);
[[nodiscard]] OutpostKind fleet_construction_kit(const GameState&, const Fleet&);
[[nodiscard]] bool outpost_online(const Fleet&);
[[nodiscard]] std::string outpost_kind_name(OutpostKind);
[[nodiscard]] std::string outpost_deployment_error(const GameState&, const Fleet&, Position);
[[nodiscard]] bool outpost_state_valid(const GameState&, const Fleet&);
[[nodiscard]] std::vector<Fleet> known_outposts(const GameState&, PlayerId);
void maintain_outposts(GameState&, std::uint64_t);
void deploy_outposts(GameState&, std::uint64_t);
// Both operations require physical co-location and move an actual finite fuel
// stock. No fleet/order ID can create fuel or draw from a foreign installation.
double refuel_from_outpost(GameState&, Fleet&);
double supply_outpost(GameState&, Fleet&, double reserveFuel);
} // namespace suns
