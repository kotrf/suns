#pragma once

#include "suns/game_state.hpp"

#include <variant>
#include <vector>

namespace suns {

struct MoveFleetOrder {
    FleetId fleet{};
    Position destination;
};

using Order = std::variant<MoveFleetOrder>;

struct PlayerOrders {
    PlayerId player{};
    std::vector<Order> orders;
};

class TurnProcessor {
public:
    [[nodiscard]] GameState process(
        const GameState& current,
        const std::vector<PlayerOrders>& submitted_orders) const;
};

} // namespace suns
