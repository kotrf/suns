#pragma once

#include "suns/game_state.hpp"

namespace suns {

inline constexpr std::uint8_t kSpaceBattleRoundLimit = 30;
inline constexpr std::size_t kSpaceBattleLogLimit = 4096;

[[nodiscard]] bool fleet_has_space_weapons(const GameState&, const Fleet&);
// All different owners are hostile in this first slice. Actual physical
// encounters, never a player forecast, invoke this function.
void resolve_space_battles(GameState&, std::uint64_t observationTurn);

} // namespace suns
