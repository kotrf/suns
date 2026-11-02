#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr double kSpaceScrapRecovery = 0.50;
inline constexpr double kColonyScrapRecovery = 0.75;
// Executes once at a physical waypoint. The caller removes a successful
// fleet only after the completion packet and its old contact are preserved.
[[nodiscard]] bool scrap_fleet(GameState&, const Fleet&, std::uint64_t turn);
} // namespace suns
