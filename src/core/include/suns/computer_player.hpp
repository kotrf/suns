#pragma once

#include "suns/campaign.hpp"

namespace suns {

struct ComputerTurnPlan {
    PlayerOrders orders;
    ComputerState nextState;
};

[[nodiscard]] bool computer_state_valid(const Player& player, std::uint64_t boundaryTurn);

// Pure decision boundary: the controller receives only this empire's projection.
// Its state is committed only when the whole campaign resolves successfully.
[[nodiscard]] ComputerTurnPlan plan_computer_turn(const PlayerView& view);

// IDs, not envelope counts: every human must submit exactly once, including
// explicit empty passes. Computer/unknown/duplicate submissions are rejected.
[[nodiscard]] std::vector<PlayerId> missing_human_submissions(
    const GameState& state, const std::vector<PlayerOrders>& humanSubmissions);
[[nodiscard]] TurnResult resolve_controlled_campaign_turn(
    const GameState& state, std::vector<PlayerOrders> humanSubmissions);

} // namespace suns
