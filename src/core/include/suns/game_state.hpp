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

inline constexpr std::uint32_t kColonyShipCost = 12;
inline constexpr std::uint32_t kFactoryCost = 6;
inline constexpr double kScoutTravelSpeed = 100.0;
inline constexpr double kColonyShipTravelSpeed = 70.0;

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
    std::uint32_t habitability{}; // 0..100; becomes known after surveying the system.
    PlayerId owner{};             // 0 means uncolonized.
    std::uint64_t population{};
    std::uint32_t industry{1};    // Infrastructure contribution; factories increase this.
    std::uint32_t stockpile{};
    std::vector<ProductionItem> productionQueue;
};

struct Player {
    PlayerId id{};
    std::string name;
    std::vector<StarId> surveyedStars;
};

enum class FleetRole {
    Scout,
    ColonyShip,
};

struct Fleet {
    FleetId id{};
    PlayerId owner{};
    std::string name;
    FleetRole role{FleetRole::Scout};
    Position position;
    std::optional<Position> destination;
};

struct GameState {
    std::uint64_t turn{1};
    std::uint64_t galaxySeed{};
    FleetId nextFleetId{1};
    std::vector<Player> players;
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
[[nodiscard]] bool same_position(Position a, Position b);
[[nodiscard]] double distance_between(Position a, Position b);
[[nodiscard]] double fleet_speed(FleetRole role);
[[nodiscard]] std::uint32_t travel_turns(Position from, Position to, double speed);
[[nodiscard]] std::uint32_t fleet_eta(const Fleet& fleet);
[[nodiscard]] bool is_surveyed(const GameState& state, PlayerId player, StarId star);
void mark_surveyed(GameState& state, PlayerId player, StarId star);

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
