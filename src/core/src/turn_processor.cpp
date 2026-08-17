#include "suns/turn_processor.hpp"

#include <algorithm>
#include <type_traits>

namespace suns {

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    GameState next = current;

    for (const auto& submission : submitted_orders) {
        for (const auto& order : submission.orders) {
            std::visit(
                [&](const auto& concrete_order) {
                    using T = std::decay_t<decltype(concrete_order)>;
                    if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                        const auto fleet = std::find_if(
                            next.fleets.begin(), next.fleets.end(),
                            [&](const Fleet& candidate) {
                                return candidate.id == concrete_order.fleet
                                    && candidate.owner == submission.player;
                            });
                        if (fleet != next.fleets.end()) {
                            fleet->position = concrete_order.destination;
                        }
                    }
                },
                order);
        }
    }

    ++next.turn;
    return next;
}

} // namespace suns
