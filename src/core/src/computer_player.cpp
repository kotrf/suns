#include "suns/computer_player.hpp"

#include <algorithm>
#include <stdexcept>

namespace suns {

bool computer_state_valid(const Player& player, std::uint64_t boundaryTurn)
{
    if (player.controller > PlayerController::Computer
        || player.computer.decisions.size() > kMaximumComputerDecisionRecords
        || player.computer.initialized != !player.computer.decisions.empty()) return false;
    if (player.controller == PlayerController::Human) return !player.computer.initialized;
    std::uint64_t previous = 0;
    for (const auto& record : player.computer.decisions) {
        if (record.turn <= previous || record.turn >= boundaryTurn
            || record.reason.empty() || record.reason.size() > kMaximumComputerDecisionReasonBytes)
            return false;
        previous = record.turn;
    }
    return true;
}

ComputerTurnPlan plan_computer_turn(const PlayerView& view)
{
    const auto* player = find_player(view.state, view.player);
    if (!player || player->controller != PlayerController::Computer
        || view.state.players.size() != 1 || view.state.turn == 0
        || !computer_state_valid(*player, view.state.turn))
        throw std::invalid_argument("A computer controller needs its own PlayerView");
    if (!player->computer.decisions.empty()
        && player->computer.decisions.back().turn >= view.state.turn)
        throw std::invalid_argument("Computer already planned this boundary");

    ComputerTurnPlan plan{{view.player, {}}, player->computer};
    std::string reason = "Keep standing orders and research settings.";
    if (!plan.nextState.initialized) {
        std::string actions;
        // A small real policy exercises ordinary, paid orders without replacing
        // routes or queues. Exploration/economy/military policies follow later.
        if (player->technology.researchAllocationPercent == 0) {
            plan.orders.orders.push_back(SetResearchAllocationOrder{20});
            actions = "Allocate 20% of production to research. ";
        }
        if (!player->technology.researchActive) {
            plan.orders.orders.push_back(SetResearchPlanOrder{
                player->technology.focus, player->technology.queuedFocuses, true});
            actions += "Resume research. ";
        }
        reason = plan.orders.orders.empty()
            ? "Initialize controller; retain existing research settings."
            : actions + "Retain the current focus and research queue.";
        plan.nextState.initialized = true;
    }
    plan.nextState.decisions.push_back({view.state.turn,
        static_cast<std::uint32_t>(plan.orders.orders.size()), std::move(reason)});
    if (plan.nextState.decisions.size() > kMaximumComputerDecisionRecords)
        plan.nextState.decisions.erase(plan.nextState.decisions.begin(),
            plan.nextState.decisions.end() - kMaximumComputerDecisionRecords);
    return plan;
}

std::vector<PlayerId> missing_human_submissions(
    const GameState& state, const std::vector<PlayerOrders>& humanSubmissions)
{
    std::vector<PlayerId> submitted;
    for (const auto& orders : humanSubmissions) {
        const auto* player = find_player(state, orders.player);
        if (!player || player->controller != PlayerController::Human
            || std::find(submitted.begin(), submitted.end(), orders.player) != submitted.end())
            throw std::invalid_argument("Unknown, computer or duplicate human submission");
        submitted.push_back(orders.player);
    }
    std::vector<PlayerId> missing;
    for (const auto& player : state.players) {
        if (player.controller > PlayerController::Computer)
            throw std::invalid_argument("Invalid empire controller");
        if (player.controller == PlayerController::Human
            && std::find(submitted.begin(), submitted.end(), player.id) == submitted.end())
            missing.push_back(player.id);
    }
    std::sort(missing.begin(), missing.end());
    return missing;
}

TurnResult resolve_controlled_campaign_turn(
    const GameState& state, std::vector<PlayerOrders> humanSubmissions)
{
    if (!missing_human_submissions(state, humanSubmissions).empty())
        throw std::invalid_argument("Waiting for every human to submit orders (or an explicit pass)");
    std::vector<std::pair<PlayerId, ComputerState>> plannedStates;
    for (const auto& player : state.players) {
        if (player.controller != PlayerController::Computer) continue;
        auto plan = plan_computer_turn(make_player_view(state, player.id));
        humanSubmissions.push_back(std::move(plan.orders));
        plannedStates.emplace_back(player.id, std::move(plan.nextState));
    }
    auto result = resolve_campaign_turn(state, std::move(humanSubmissions));
    for (auto& player : result.state.players) {
        const auto plan = std::find_if(plannedStates.begin(), plannedStates.end(),
            [&](const auto& entry) { return entry.first == player.id; });
        if (plan != plannedStates.end()) player.computer = std::move(plan->second);
    }
    return result;
}

} // namespace suns
