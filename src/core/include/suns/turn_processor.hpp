#pragma once

#include "suns/game_state.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace suns {

struct MoveFleetOrder {
    FleetId fleet{};
    Position destination;
    std::uint8_t warp{}; // 0 means keep current Warp.
};

struct QueueProductionOrder {
    PlanetId colony{};
    ProductionKind kind{ProductionKind::ColonyShip};
};

struct CreateShipDesignOrder {
    std::string name;
    ShipHullType hull{ShipHullType::Scout};
    std::vector<ShipComponentType> components;
};

struct QueueShipDesignOrder {
    PlanetId colony{};
    ShipDesignId design{};
};

struct SetFleetColonistsOrder {
    PlanetId colony{};
    FleetId fleet{};
    std::uint64_t colonists{};
};

struct RefuelFleetOrder {
    PlanetId colony{};
    FleetId fleet{};
};

struct ColonizePlanetOrder {
    FleetId fleet{};
    PlanetId planet{};
};

using Order = std::variant<
    MoveFleetOrder,
    QueueProductionOrder,
    CreateShipDesignOrder,
    QueueShipDesignOrder,
    SetFleetColonistsOrder,
    RefuelFleetOrder,
    ColonizePlanetOrder>;

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
