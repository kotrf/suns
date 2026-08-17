#pragma once

#include "suns/game_state.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace suns {

struct MoveFleetOrder {
    FleetId fleet{};
    Position destination;

    // 0 means keep the fleet's current Warp. This preserves compatibility with
    // existing callers while allowing the UI/server to choose Warp explicitly.
    std::uint8_t warp{};
};

struct QueueProductionOrder {
    PlanetId colony{};
    ProductionKind kind{ProductionKind::ColonyShip};
};

struct QueueShipDesignOrder {
    PlanetId colony{};
    ShipDesignId design{};
};

// Set the exact number of colonists carried by a fleet. The processor transfers
// the difference to/from the specified friendly colony, so repeated planning is
// deterministic and unloading uses the same command as loading.
struct SetFleetColonistsOrder {
    PlanetId colony{};
    FleetId fleet{};
    std::uint64_t colonists{};
};

// Friendly colonies currently provide fuel without a separate fuel economy.
// Refuelling is nevertheless an explicit order so route logistics are visible
// and can later be backed by a real planetary fuel stockpile.
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
