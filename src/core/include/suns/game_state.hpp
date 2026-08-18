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
inline constexpr double kScoutTravelSpeed = 100.0;       // Legacy UI compatibility.
inline constexpr double kColonyShipTravelSpeed = 70.0;  // Legacy UI compatibility.
inline constexpr double kScoutSensorRange = 90.0;
inline constexpr double kColonySensorRange = 60.0;
inline constexpr ShipDesignId kScoutDesignId = 1;
inline constexpr ShipDesignId kColonyShipDesignId = 2;
inline constexpr ShipDesignId kFirstCustomShipDesignId = 3;
inline constexpr std::uint8_t kMaxWarp = 10;
inline constexpr std::uint8_t kScoutCruiseWarp = 10;
inline constexpr std::uint8_t kColonyShipCruiseWarp = 8;
inline constexpr double kColonistsPerCargoUnit = 100.0;
inline constexpr double kRadiatingDriveSafeTolerance = 0.85;
inline constexpr double kRadiatingDriveColonistLossFraction = 0.10;

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

    // engineThrust is retained only for the temporary compatibility metric.
    // Turn movement is driven by maxWarp + the ordered Warp.
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
    // ColonyShip remains as a compatibility alias for old callers. New code
    // queues a concrete ShipDesignId.
    ColonyShip,
    Factory,
};

struct ProductionItem {
    ProductionKind kind{ProductionKind::ColonyShip};
    std::uint32_t remainingCost{};
    ShipDesignId shipDesign{}; // 0 for non-ship production / legacy alias.
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
};

struct Player {
    PlayerId id{};
    std::string name;
    std::vector<StarId> surveyedStars;
    // Normalized 0..1 race trait. Radiating drives are currently safe at 0.85+.
    double radiationTolerance{0.50};
    bool radiationImmune{};
};

// FleetRole remains a temporary presentation hint for the current Qt client.
// Simulation capabilities are derived from the referenced ShipDesign.
enum class FleetRole {
    Scout,
    ColonyShip,
};

enum class FleetArrivalActionKind {
    None,
    LoadColonistsToCapacity,
    UnloadAllColonists,
    Refuel,
};

struct FleetArrivalAction {
    FleetArrivalActionKind kind{FleetArrivalActionKind::None};
    // Used by LoadColonistsToCapacity. The action is dynamic: the amount loaded
    // is calculated from the colony population and free cargo at arrival time.
    std::uint64_t reservePopulation{1};
};

struct FleetWaypoint {
    Position destination;
    std::uint8_t warp{kScoutCruiseWarp};
    FleetArrivalAction arrivalAction{};
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
    // Future legs. The active leg remains in destination/warp/arrivalAction so
    // existing movement/UI code stays stable while route programming grows.
    std::vector<FleetWaypoint> waypointQueue;
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
[[nodiscard]] double ship_design_speed(const ShipDesign& design); // Legacy compatibility metric.
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

[[nodiscard]] double fleet_speed(FleetRole role);
[[nodiscard]] double fleet_sensor_range(FleetRole role);
[[nodiscard]] std::uint32_t fleet_eta(const Fleet& fleet);
[[nodiscard]] double fleet_speed(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_sensor_range(const GameState& state, const Fleet& fleet);
[[nodiscard]] bool fleet_can_colonize(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint32_t fleet_eta(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_capacity(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_cargo_capacity(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_gross_mass(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_rate(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_change_for_distance(
    const GameState& state,
    const Fleet& fleet,
    double distance);
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
    return kind == ProductionKind::ColonyShip ? kColonyShipCost : kFactoryCost;
}

[[nodiscard]] std::uint32_t production_item_cost(const GameState& state, const ProductionItem& item);
[[nodiscard]] std::uint64_t population_capacity(const Planet& planet);
[[nodiscard]] std::uint64_t projected_population_growth(const Planet& planet);
[[nodiscard]] std::uint32_t colony_output(const Planet& planet);

[[nodiscard]] GameState generate_game(const GalaxyConfig& config);
GameState make_demo_game();

} // namespace suns
