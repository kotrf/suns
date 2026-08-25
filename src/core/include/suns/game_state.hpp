#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace suns {

using PlayerId = std::uint32_t;
using StarId = std::uint32_t;
using PlanetId = std::uint32_t;
using FleetId = std::uint32_t;
using ShipDesignId = std::uint32_t;

inline constexpr std::uint32_t kColonyShipCost = 12;
inline constexpr std::uint32_t kFactoryCost = 6;
inline constexpr std::uint32_t kMineCost = 5;
inline constexpr double kScoutTravelSpeed = 100.0;       // Legacy UI compatibility.
inline constexpr double kColonyShipTravelSpeed = 70.0;  // Legacy UI compatibility.
inline constexpr double kScoutSensorRange = 90.0;
inline constexpr double kColonySensorRange = 60.0;
inline constexpr ShipDesignId kScoutDesignId = 1;
inline constexpr ShipDesignId kColonyShipDesignId = 2;
inline constexpr ShipDesignId kFirstCustomShipDesignId = 3;
inline constexpr std::uint8_t kMaxWarp = 10;
inline constexpr std::uint8_t kScoutCruiseWarp = 8;
inline constexpr std::uint8_t kColonyShipCruiseWarp = 7;
inline constexpr double kColonistsPerCargoUnit = 100.0;
inline constexpr double kRadiatingDriveSafeTolerance = 0.85;
inline constexpr double kRadiatingDriveColonistLossFraction = 0.10;

// First communications slice: established friendly colonies temporarily act as
// relay nodes. Orbital stations will later move this capability to explicit
// station/ship modules without changing packet and latency semantics.
inline constexpr double kCommunicationRelayRange = 120.0;
inline constexpr double kCommunicationSignalSpeed = 150.0;

struct Position {
    double x{};
    double y{};
};

enum class StarClass {
    BlueWhite,
    White,
    YellowWhite,
    Yellow,
    Orange,
    Red,
};

struct StarSystem {
    StarId id{};
    std::string name;
    Position position;
    StarClass stellarClass{StarClass::Yellow};
};

struct MineralCargo {
    double ironium{};
    double boranium{};
    double germanium{};
};

enum class ShipHullType {
    Scout,
    LightTransport,
    MediumTransport,
};

struct ShipHullSpec {
    ShipHullType type{ShipHullType::Scout};
    std::string name;
    double mass{};
    std::uint32_t buildCost{};
    double baseFuelCapacity{};
    double baseCargoCapacity{};
    std::uint8_t engineSlots{1};
    std::uint8_t generalSlots{};
};

enum class ShipComponentType {
    FusionDrive,
    RamScoopDrive,
    RadiatingRamScoopDrive,
    LongRangeScanner,
    ColonyModule,
    FuelTank,
    CargoPod,
    AntimatterGenerator,
};

enum class ShipComponentKind {
    Engine,
    Scanner,
    Fuel,
    Cargo,
    Special,
};

struct ShipComponentSpec {
    ShipComponentType type{ShipComponentType::FusionDrive};
    std::string name;
    ShipComponentKind kind{ShipComponentKind::Engine};
    double mass{};
    std::uint32_t buildCost{};

    double engineThrust{};
    std::uint8_t maxWarp{};
    std::array<double, kMaxWarp + 1> fuelPer100MassLy{};

    double sensorRange{};
    double fuelCapacity{};
    double cargoCapacity{};
    double fuelGenerationPerTurn{};
    bool enablesColonization{};
    double radiationHazard{};
};

struct ShipDesign {
    ShipDesignId id{};
    PlayerId owner{};
    std::string name;
    ShipHullType hull{ShipHullType::Scout};
    std::vector<ShipComponentType> components;
};

enum class ProductionKind {
    ColonyShip,
    Factory,
    Mine,
};

struct ProductionItem {
    ProductionKind kind{ProductionKind::ColonyShip};
    std::uint32_t remainingCost{};
    ShipDesignId shipDesign{};
};

struct Planet {
    PlanetId id{};
    StarId star{};
    std::string name;
    std::uint32_t habitability{};
    PlayerId owner{};
    std::uint64_t population{};
    std::uint32_t industry{1};
    std::uint32_t stockpile{};
    std::vector<ProductionItem> productionQueue;
    MineralCargo minerals;
    std::uint32_t mines{};
    bool productionWaitingForMinerals{};
};

struct PendingSurveyReport {
    StarId star{};
    FleetId sourceFleet{}; // Zero means a stationary colony sensor source.
    std::uint64_t observedTurn{};
    std::uint64_t deliveryTurn{};
};

enum class PlayerReportKind {
    FleetArrived,
    RouteCompleted,
    FleetStalledForFuel,
    ProductionCompleted,
    ColonyFounded,
    ProductionWaitingForMinerals,
};

// Player-facing operational facts travel independently from fleet telemetry.
// This keeps the simulation truth authoritative while ensuring that Turn
// Messages cannot reveal a remote result before its communication packet lands.
struct PendingPlayerReport {
    PlayerReportKind kind{PlayerReportKind::FleetArrived};
    std::uint64_t observedTurn{};
    std::uint64_t deliveryTurn{};
    StarId star{};
    PlanetId planet{};
    FleetId fleet{};
    ShipDesignId shipDesign{};
    ProductionKind productionKind{ProductionKind::ColonyShip};
    Position position;
    std::uint32_t quantity{};
};

struct Player {
    PlayerId id{};
    std::string name;
    std::vector<StarId> surveyedStars;
    std::vector<PendingSurveyReport> pendingSurveyReports;
    std::vector<PendingPlayerReport> pendingPlayerReports;
    double radiationTolerance{0.50};
    bool radiationImmune{};
};

enum class FleetRole {
    Scout,
    ColonyShip,
};

enum class FleetArrivalActionKind {
    None,
    LoadColonistsToCapacity,
    UnloadAllColonists,
    Refuel,
    Colonize,
};

struct FleetArrivalAction {
    FleetArrivalActionKind kind{FleetArrivalActionKind::None};
    std::uint64_t reservePopulation{1};
};

struct FleetWaypoint {
    Position destination;
    std::uint8_t warp{kScoutCruiseWarp};
    FleetArrivalAction arrivalAction{};
};

struct FleetRouteProgram {
    Position destination;
    std::uint8_t warp{kScoutCruiseWarp};
    FleetArrivalAction arrivalAction{};
    std::vector<FleetWaypoint> queuedWaypoints;
    bool clearRoute{};
};

struct FleetTelemetry {
    std::uint64_t observedTurn{};
    Position position;
    std::optional<Position> destination;
    std::uint8_t warp{kScoutCruiseWarp};
    double fuel{};
    std::uint64_t colonists{};
    std::optional<FleetArrivalAction> arrivalAction;
    std::vector<FleetWaypoint> waypointQueue;
    MineralCargo minerals;
};

struct PendingFleetCommand {
    std::uint64_t issuedTurn{};
    std::uint64_t deliveryTurn{};
    FleetRouteProgram program;
};

struct PendingFleetTelemetry {
    std::uint64_t deliveryTurn{};
    FleetTelemetry telemetry;
};

struct Fleet {
    FleetId id{};
    PlayerId owner{};
    std::string name;
    FleetRole role{FleetRole::Scout};
    ShipDesignId design{kScoutDesignId};
    Position position;
    std::optional<Position> destination;
    std::uint8_t warp{kScoutCruiseWarp};
    double fuel{300.0};
    std::uint64_t colonists{};
    std::optional<FleetArrivalAction> arrivalAction;
    std::vector<FleetWaypoint> waypointQueue;
    MineralCargo minerals;

    // Simulation truth stays in the fields above. These trailing fields model
    // confirmed owner knowledge and command/telemetry packets physically in flight.
    std::vector<PendingFleetCommand> pendingCommands;
    FleetTelemetry telemetry;
    std::vector<PendingFleetTelemetry> telemetryInTransit;
    bool fuelStalled{};
};

struct GameState {
    std::uint64_t turn{1};
    std::uint64_t galaxySeed{};
    FleetId nextFleetId{1};
    ShipDesignId nextShipDesignId{kFirstCustomShipDesignId};
    std::vector<Player> players;
    std::vector<ShipDesign> shipDesigns;
    std::vector<StarSystem> stars;
    std::vector<Planet> planets;
    std::vector<Fleet> fleets;
};

struct GalaxyConfig {
    std::uint64_t seed{20260817};
    std::size_t starCount{24};
    double width{900.0};
    double height{650.0};
    double minimumSeparation{48.0};
};

[[nodiscard]] const StarSystem* find_star(const GameState& state, StarId id);
[[nodiscard]] const Planet* find_planet_at_star(const GameState& state, StarId star);
[[nodiscard]] const Player* find_player(const GameState& state, PlayerId id);
[[nodiscard]] const ShipDesign* find_ship_design(const GameState& state, ShipDesignId id);
[[nodiscard]] const ShipDesign* fleet_design(const GameState& state, const Fleet& fleet);

[[nodiscard]] ShipHullSpec hull_spec(ShipHullType type);
[[nodiscard]] ShipComponentSpec component_spec(ShipComponentType type);
[[nodiscard]] std::size_t ship_design_engine_slots_used(const ShipDesign& design);
[[nodiscard]] std::size_t ship_design_general_slots_used(const ShipDesign& design);
[[nodiscard]] bool ship_design_valid(const ShipDesign& design);
[[nodiscard]] double ship_design_mass(const ShipDesign& design);
[[nodiscard]] std::uint32_t ship_design_cost(const ShipDesign& design);
[[nodiscard]] MineralCargo ship_design_mineral_cost(const ShipDesign& design);
[[nodiscard]] double ship_design_speed(const ShipDesign& design);
[[nodiscard]] double ship_design_sensor_range(const ShipDesign& design);
[[nodiscard]] bool ship_design_can_colonize(const ShipDesign& design);
[[nodiscard]] std::uint8_t ship_design_max_warp(const ShipDesign& design);
[[nodiscard]] double ship_design_fuel_rate(const ShipDesign& design, std::uint8_t warp);
[[nodiscard]] double ship_design_fuel_capacity(const ShipDesign& design);
[[nodiscard]] double ship_design_cargo_capacity(const ShipDesign& design);
[[nodiscard]] double ship_design_fuel_generation(const ShipDesign& design);
[[nodiscard]] double ship_design_radiation_hazard(const ShipDesign& design);

[[nodiscard]] bool same_position(Position a, Position b);
[[nodiscard]] double distance_between(Position a, Position b);
[[nodiscard]] double warp_distance(std::uint8_t warp);
[[nodiscard]] double colonist_cargo_mass(std::uint64_t colonists);
[[nodiscard]] double mineral_cargo_mass(const MineralCargo& minerals);
[[nodiscard]] bool mineral_cargo_sufficient(const MineralCargo& available, const MineralCargo& required);
void subtract_minerals(MineralCargo& available, const MineralCargo& required);

[[nodiscard]] MineralCargo planet_mineral_concentration(const GameState& state, const Planet& planet);
[[nodiscard]] MineralCargo projected_mineral_mining(const GameState& state, const Planet& planet);
[[nodiscard]] MineralCargo production_item_mineral_cost(const GameState& state, const ProductionItem& item);

[[nodiscard]] double fleet_speed(FleetRole role);
[[nodiscard]] double fleet_sensor_range(FleetRole role);
[[nodiscard]] std::uint32_t fleet_eta(const Fleet& fleet);
[[nodiscard]] double fleet_speed(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_sensor_range(const GameState& state, const Fleet& fleet);
[[nodiscard]] bool fleet_can_colonize(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint32_t fleet_eta(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_capacity(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_cargo_capacity(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_cargo_used(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_gross_mass(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_rate(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_change_for_distance(const GameState& state, const Fleet& fleet, double distance);
[[nodiscard]] bool fleet_warp_valid(const GameState& state, const Fleet& fleet, std::uint8_t warp);
[[nodiscard]] bool fleet_radiation_safe(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint64_t projected_fleet_radiation_losses(const GameState& state, const Fleet& fleet);
void apply_fleet_radiation_attrition(GameState& state, Fleet& fleet);

[[nodiscard]] bool within_range(Position source, Position target, double range);
[[nodiscard]] std::uint32_t travel_turns(Position from, Position to, double speed);
[[nodiscard]] bool is_surveyed(const GameState& state, PlayerId player, StarId star);
void mark_surveyed(GameState& state, PlayerId player, StarId star);
void refresh_sensor_intel(GameState& state);

[[nodiscard]] constexpr std::uint32_t production_cost(ProductionKind kind)
{
    switch (kind) {
    case ProductionKind::ColonyShip: return kColonyShipCost;
    case ProductionKind::Factory: return kFactoryCost;
    case ProductionKind::Mine: return kMineCost;
    }
    return kColonyShipCost;
}

[[nodiscard]] std::uint32_t production_item_cost(const GameState& state, const ProductionItem& item);
[[nodiscard]] std::uint64_t population_capacity(const Planet& planet);
[[nodiscard]] std::uint64_t projected_population_growth(const Planet& planet);
[[nodiscard]] std::uint32_t colony_output(const Planet& planet);

[[nodiscard]] GameState generate_game(const GalaxyConfig& config);
GameState make_demo_game();

} // namespace suns
