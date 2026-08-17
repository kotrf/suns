#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace suns {

using PlayerId = std::uint32_t;
using StarId = std::uint32_t;
using FleetId = std::uint32_t;

struct Position {
    double x{};
    double y{};
};

struct StarSystem {
    StarId id{};
    std::string name;
    Position position;
    PlayerId owner{}; // 0 means unowned for the bootstrap model.
};

struct Player {
    PlayerId id{};
    std::string name;
};

struct Fleet {
    FleetId id{};
    PlayerId owner{};
    std::string name;
    Position position;
};

struct GameState {
    std::uint64_t turn{1};
    std::vector<Player> players;
    std::vector<StarSystem> stars;
    std::vector<Fleet> fleets;
};

GameState make_demo_game();

} // namespace suns
