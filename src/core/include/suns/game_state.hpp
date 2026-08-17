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
inline constexpr std::uint8_t kMaxWarp = 10;
inline constexpr std::uint8_t kScoutCruiseWarp = 10;
inline constexpr std::uint8_t kColonyShipCruiseWarp = 8;
inline constexpr double kColonistsPerCargoUnit = 100.0;

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

    // engineThrust is retained for the temporary pre-Warp UI compatibility
    // calculation. Turn movement is driven by maxWarp + the ordered Warp.
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
    double hullMass{};
    std::uint32_t hullCost{};
    std::vector<ShipComponentType> components;
    double baseFuelCapacity{};
    double baseCargoCapacity{};
};

enum class ProductionKind {
    ColonyShip,
    Factory,
};

struct ProductionItem {
    ProductionKind kind{ProductionKind::ColonyShip};
    std::uint32_t remainingCost{};
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
};

// FleetRole remains a temporary presentation hint for the current Qt client.
// Simulation capabilities are derived from the referenced ShipDesign.
enum class FleetRole {
    Scout,
    ColonyShip,
};

struct Fleet {
    FleetId id{};
    PlayerId owner{};
    std::string name;
    FleetRole role{FleetRole::Scout};
    ShipDesignId design{kScoutDesignId};
    Position position;
    std::optional<Position> destination;

    // Warp is persistent while a course is active. An order may change it.
    std::uint8_t warp{kScoutCruiseWarp};
    double fuel{300.0};
    std::uint64_t colonists{};
};

struct GameState {
    std::uint64_t turn{1};
    std::uint64_t galaxySeed{};
    FleetId nextFleetId{1};
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

[[nodiscard]] ShipComponentSpec component_spec(ShipComponentType type);
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

// Compatibility helpers for the current Qt client. Turn resolution no longer
// uses these role-derived speeds; it uses Fleet::warp and Warp^2 movement.
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

[[nodiscard]] bool within_range(Position source, Position target, double range);
[[nodiscard]] std::uint32_t travel_turns(Position from, Position to, double speed);
[[nodiscard]] bool is_surveyed(const GameState& state, PlayerId player, StarId star);
void mark_surveyed(GameState& state, PlayerId player, StarId star);
void refresh_sensor_intel(GameState& state);

[[nodiscard]] constexpr std::uint32_t production_cost(ProductionKind kind)
{
    return kind == ProductionKind::ColonyShip ? kColonyShipCost : kFactoryCost;
}

[[nodiscard]] std::uint64_t population_capacity(const Planet& planet);
[[nodiscard]] std::uint64_t projected_population_growth(const Planet& planet);
[[nodiscard]] std::uint32_t colony_output(const Planet& planet);

[[nodiscard]] GameState generate_game(const GalaxyConfig& config);
GameState make_demo_game();

} // namespace suns
