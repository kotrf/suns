#pragma once

#include "suns/game_state.hpp"

namespace suns {

inline constexpr std::uint8_t kSpaceBattleRoundLimit = 30;
inline constexpr std::size_t kSpaceBattleLogLimit = 4096;

[[nodiscard]] bool fleet_has_space_weapons(const GameState&, const Fleet&);
[[nodiscard]] bool fleet_battle_plan_valid(const FleetBattlePlan&);

// A mixed fleet presents all the target categories of its surviving stacks.
struct BattleContact {
    PlayerId owner{};
    bool armed{};
    bool cargoShips{};
    bool station{};
    FleetBattlePlan plan;
};

[[nodiscard]] BattleContact battle_contact(const GameState&, const Fleet&);
[[nodiscard]] BattleContact battle_contact(const OrbitalStation&);
[[nodiscard]] bool can_start_space_battle(const BattleContact& attacker, const BattleContact& target);
[[nodiscard]] bool space_battle_between(const BattleContact&, const BattleContact&);

// Only physical encounters invoke the tactical resolver.
void resolve_space_battles(GameState&, std::uint64_t observationTurn);

} // namespace suns
