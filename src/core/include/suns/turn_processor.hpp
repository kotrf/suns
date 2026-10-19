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
    FleetId targetFleet{};
    bool clearRoute{};
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
    std::vector<ResearchField> queuedFocuses;
    bool active{true};
};

struct SetResearchAllocationOrder {
    std::uint8_t percent{};
};

struct CreateShipDesignOrder {
    std::string name;
    ShipHullType hull{ShipHullType::Scout};
    std::vector<ShipComponentType> components;
    std::vector<ShipComponentPlacement> placements;
};

struct QueueShipDesignOrder {
    PlanetId colony{};
    ShipDesignId design{};
    // Pending designs use an owner-scoped name, never a predicted global ID.
    std::string pendingDesignName;
};

struct ReorderProductionQueueOrder {
    PlanetId colony{};
    std::uint32_t fromIndex{};
    std::uint32_t toIndex{};
};

struct CancelProductionOrder {
    PlanetId colony{};
    std::uint32_t index{};
};

struct QueueProductionBatchOrder {
    PlanetId colony{};
    ProductionKind kind{ProductionKind::Factory};
    std::uint32_t quantity{1};
    ProductionAutomation automation{ProductionAutomation::None};
};

struct SetProductionTemplateOrder {
    ProductionTemplate value;
    bool remove{};
};

struct ApplyProductionTemplateOrder {
    PlanetId colony{};
    std::string name;
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
    std::optional<ProductionTemplate> colonyPlan;
};

struct SetRemoteMiningOrder {
    FleetId fleet{};
    bool enabled{true};
};

// The destination FleetId survives; the source is consumed.
struct MergeFleetsOrder {
    FleetId destination{};
    FleetId source{};
};

// The source FleetId survives and a new FleetId is allocated for `ships`.
struct SplitFleetOrder {
    FleetId source{};
    std::vector<FleetShipStack> ships;
};

struct RenameFleetOrder {
    FleetId fleet{};
    std::string name;
};

struct QueueStationOrder { PlanetId colony{}; StationConstruction construction; };
// Transit is explicit and revalidated against real paid gates during resolution.
struct GateFleetOrder {
    FleetId fleet{};
    PlanetId source{};
    PlanetId destination{};
};

struct SetFleetTaskOrder { FleetId fleet{}; FleetTask task{FleetTask::None}; };
struct SetFleetElectronicsOrder { FleetId fleet{}; ElectronicsProgram program; };
struct SetMobileBaseOrder { FleetId fleet{}; MobileBaseProgram program; };
struct TransferFleetFuelOrder {
    FleetId source{}, destination{};
    double amount{}, sourceReserve{};
};

struct SetPlanetaryEngineeringOrder {
    PlanetId colony{};
    PlanetaryEngineeringKind kind{PlanetaryEngineeringKind::None};
    EngineeringAction action{EngineeringAction::StartOrResume};
    std::uint8_t strength{10};
};

struct TransferScienceSampleOrder {
    CargoTransferEndpoint source, destination;
    UnknownObjectId sample{};
};
struct ProcessScienceSampleOrder {
    PlanetId colony{};
    UnknownObjectId sample{};
    bool retain{}; // false consumes the specimen for a one-time scientific-data report.
};

struct TransferXenocrystalOrder {
    CargoTransferEndpoint source, destination;
    std::uint32_t amount{};
};
struct SetResonanceLaboratoryOrder {
    PlanetId colony{};
    ResonanceAction action{ResonanceAction::StartOrResume};
    bool active{true};
};

using Order = std::variant<
    MoveFleetOrder,
    QueueProductionOrder,
    SetColonyResearchOrder,
    SetResearchPlanOrder,
    SetResearchAllocationOrder,
    CreateShipDesignOrder,
    QueueShipDesignOrder,
    ReorderProductionQueueOrder,
    SetFleetColonistsOrder,
    SetFleetMineralCargoOrder,
    TransferCargoOrder,
    RefuelFleetOrder,
    ColonizePlanetOrder,
    SetRemoteMiningOrder,
    MergeFleetsOrder,
    SplitFleetOrder,
    CancelProductionOrder,
    RenameFleetOrder,
    QueueProductionBatchOrder,
    SetProductionTemplateOrder,
    ApplyProductionTemplateOrder,
    SetFleetTaskOrder,
    SetFleetElectronicsOrder,
    QueueStationOrder,
    GateFleetOrder,
    SetMobileBaseOrder,
    TransferFleetFuelOrder,
    SetPlanetaryEngineeringOrder,
    TransferScienceSampleOrder,
    ProcessScienceSampleOrder,
    TransferXenocrystalOrder,
    SetResonanceLaboratoryOrder>;

struct PlayerOrders {
    PlayerId player{};
    std::vector<Order> orders;
};

// Replay only valid queued local cargo orders, using the same rules as End
// Turn, without advancing a year or changing the actual physical state.
[[nodiscard]] GameState projected_cargo_order_state(const GameState&, const PlayerOrders&);

// Apply design creation only, without advancing the year or spending resources.
[[nodiscard]] GameState planned_ship_design_state(const GameState& state, const PlayerOrders& pending);
[[nodiscard]] const ShipDesign* resolve_ship_design_order(
    const GameState& state, PlayerId player, const QueueShipDesignOrder& order);

// Transient movement accounting for navigation previews. Captured before
// arrival actions, refuelling, merging or dismantling can change the tank.
// This is host-side output, not a player report or persisted game state.
struct FleetMovementFuel {
    FleetId fleet{};
    Position destination;
    FleetId targetFleet{};
    double consumed{};
    double remaining{};
    bool fuelLimited{};
    double consumedMinimum{}, consumedMaximum{}; // Same travelled path, density envelope 0..1.
};

struct TurnResult {
    GameState state;
    std::vector<GameEvent> events;
    std::vector<FleetMovementFuel> movementFuel;
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
