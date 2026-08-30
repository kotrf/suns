#pragma once

#include "suns/game_event.hpp"
#include "suns/game_state.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace suns {

struct MoveFleetOrder {
    FleetId fleet{};
    Position destination;
    std::uint8_t warp{};
    FleetArrivalAction arrivalAction{};
    std::vector<FleetWaypoint> queuedWaypoints;
    bool repeatOrders{};
};

struct QueueProductionOrder {
    PlanetId colony{};
    ProductionKind kind{ProductionKind::ColonyShip};
};

struct SetColonyResearchOrder {
    PlanetId colony{};
    bool enabled{true};
};

struct SetResearchPlanOrder {
    ResearchField focus{ResearchField::Electronics};
    std::optional<ResearchField> nextFocus;
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

struct SetFleetMineralCargoOrder {
    PlanetId colony{};
    FleetId fleet{};
    MineralCargo minerals;
};

// Exactly one ID is set. A planet endpoint denotes its surface stockpile (and,
// for a friendly colony, its population); a fleet endpoint denotes its shared
// cargo hold.
struct CargoTransferEndpoint {
    PlanetId planet{};
    FleetId fleet{};
};

struct TransferCargoOrder {
    CargoTransferEndpoint source;
    CargoTransferEndpoint destination;
    std::uint64_t colonists{};
    MineralCargo minerals;
};

struct RefuelFleetOrder {
    PlanetId colony{};
    FleetId fleet{};
};

struct ColonizePlanetOrder {
    FleetId fleet{};
    PlanetId planet{};
};

struct SetRemoteMiningOrder {
    FleetId fleet{};
    bool enabled{true};
};

using Order = std::variant<
    MoveFleetOrder,
    QueueProductionOrder,
    SetColonyResearchOrder,
    SetResearchPlanOrder,
    CreateShipDesignOrder,
    QueueShipDesignOrder,
    SetFleetColonistsOrder,
    SetFleetMineralCargoOrder,
    TransferCargoOrder,
    RefuelFleetOrder,
    ColonizePlanetOrder,
    SetRemoteMiningOrder>;

struct PlayerOrders {
    PlayerId player{};
    std::vector<Order> orders;
};

struct TurnResult {
    GameState state;
    std::vector<GameEvent> events;
};

class TurnProcessor {
public:
    [[nodiscard]] TurnResult process_with_events(
        const GameState& current,
        const std::vector<PlayerOrders>& submitted_orders) const;

    // Compatibility path for simulations and previews that need state only.
    [[nodiscard]] GameState process(
        const GameState& current,
        const std::vector<PlayerOrders>& submitted_orders) const;
};

} // namespace suns
