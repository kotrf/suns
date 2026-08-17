#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace suns {

using PlayerId = std::uint32_t;
using StarId = std::uint32_t;
using PlanetId = std::uint32_t;
using FleetId = std::uint32_t;

inline constexpr std::uint32_t kColonyShipCost = 4;

struct Position {
    double x{};
    double y{};
};

struct StarSystem {
    StarId id{};
    std::string name;
    Position position;
};

struct Planet {
    PlanetId id{};
    StarId star{};
    std::string name;
    std::uint32_t habitability{}; // 0..100; informational in the first vertical slice.
    PlayerId owner{};             // 0 means uncolonized.
    std::uint64_t population{};
    std::uint32_t industry{1};
    std::uint32_t stockpile{};
};

struct Player {
    PlayerId id{};
    std::string name;
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
};

struct GameState {
    std::uint64_t turn{1};
    FleetId nextFleetId{1};
    std::vector<Player> players;
    std::vector<StarSystem> stars;
    std::vector<Planet> planets;
    std::vector<Fleet> fleets;
};

[[nodiscard]] const StarSystem* find_star(const GameState& state, StarId id);
[[nodiscard]] const Planet* find_planet_at_star(const GameState& state, StarId star);
[[nodiscard]] bool same_position(Position a, Position b);

GameState make_demo_game();

} // namespace suns
