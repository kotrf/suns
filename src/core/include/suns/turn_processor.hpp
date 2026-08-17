#pragma once

#include "suns/game_state.hpp"

#include <variant>
#include <vector>

namespace suns {

struct MoveFleetOrder {
    FleetId fleet{};
    Position destination;
};

struct QueueProductionOrder {
    PlanetId colony{};
    ProductionKind kind{ProductionKind::ColonyShip};
};

struct ColonizePlanetOrder {
    FleetId fleet{};
    PlanetId planet{};
};

using Order = std::variant<MoveFleetOrder, QueueProductionOrder, ColonizePlanetOrder>;

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
