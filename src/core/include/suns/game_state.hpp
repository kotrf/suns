#pragma once

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
inline constexpr double kScoutTravelSpeed = 100.0;
inline constexpr double kColonyShipTravelSpeed = 70.0;
inline constexpr double kScoutSensorRange = 90.0;
inline constexpr double kColonySensorRange = 60.0;
inline constexpr ShipDesignId kScoutDesignId = 1;
inline constexpr ShipDesignId kColonyShipDesignId = 2;

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
    LongRangeScanner,
    ColonyModule,
};

enum class ShipComponentKind {
    Engine,
    Scanner,
    Special,
};

struct ShipComponentSpec {
    ShipComponentType type{ShipComponentType::FusionDrive};
    std::string name;
    ShipComponentKind kind{ShipComponentKind::Engine};
    double mass{};
    std::uint32_t buildCost{};
    double engineThrust{};
    double sensorRange{};
    bool enablesColonization{};
};

struct ShipDesign {
    ShipDesignId id{};
    PlayerId owner{};
    std::string name;
    double hullMass{};
    std::uint32_t hullCost{};
    std::vector<ShipComponentType> components;
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
[[nodiscard]] double ship_design_speed(const ShipDesign& design);
[[nodiscard]] double ship_design_sensor_range(const ShipDesign& design);
[[nodiscard]] bool ship_design_can_colonize(const ShipDesign& design);

[[nodiscard]] bool same_position(Position a, Position b);
[[nodiscard]] double distance_between(Position a, Position b);

// Compatibility helpers for the current UI. Default designs are tuned to these
// values; turn resolution uses the design-aware overloads below.
[[nodiscard]] double fleet_speed(FleetRole role);
[[nodiscard]] double fleet_sensor_range(FleetRole role);
[[nodiscard]] std::uint32_t fleet_eta(const Fleet& fleet);

[[nodiscard]] double fleet_speed(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_sensor_range(const GameState& state, const Fleet& fleet);
[[nodiscard]] bool fleet_can_colonize(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint32_t fleet_eta(const GameState& state, const Fleet& fleet);

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
