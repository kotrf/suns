#include "suns/game_state.hpp"

#include <algorithm>
#include <cmath>

namespace suns {

const StarSystem* find_star(const GameState& state, StarId id)
{
    const auto it = std::find_if(state.stars.begin(), state.stars.end(), [id](const StarSystem& star) {
        return star.id == id;
    });
    return it == state.stars.end() ? nullptr : &*it;
}

const Planet* find_planet_at_star(const GameState& state, StarId star)
{
    const auto it = std::find_if(state.planets.begin(), state.planets.end(), [star](const Planet& planet) {
        return planet.star == star;
    });
    return it == state.planets.end() ? nullptr : &*it;
}

bool same_position(Position a, Position b)
{
    constexpr double epsilon = 0.000001;
    return std::abs(a.x - b.x) < epsilon && std::abs(a.y - b.y) < epsilon;
}

GameState make_demo_game()
{
    GameState state;
    state.players.push_back({1, "Terrans"});

    state.stars = {
        {1, "Sol", {0.0, 0.0}},
        {2, "Alpha Centauri", {86.0, -42.0}},
        {3, "Sirius", {-118.0, 76.0}},
        {4, "Vega", {154.0, 96.0}},
        {5, "Altair", {42.0, 142.0}},
        {6, "Tau Ceti", {-72.0, -126.0}},
        {7, "Epsilon Eridani", {142.0, -132.0}},
        {8, "Procyon", {-168.0, -34.0}},
    };

    state.planets = {
        {1, 1, "Earth", 100, 1, 1000, 4, 0, {}},
        {2, 2, "Centauri II", 82, 0, 0, 1, 0, {}},
        {3, 3, "Sirius III", 48, 0, 0, 1, 0, {}},
        {4, 4, "Vega II", 71, 0, 0, 1, 0, {}},
        {5, 5, "Altair IV", 63, 0, 0, 1, 0, {}},
        {6, 6, "Tau Ceti III", 91, 0, 0, 1, 0, {}},
        {7, 7, "Eridani II", 56, 0, 0, 1, 0, {}},
        {8, 8, "Procyon II", 76, 0, 0, 1, 0, {}},
    };

    state.fleets.push_back({1, 1, "Scout 1", FleetRole::Scout, {0.0, 0.0}});
    state.nextFleetId = 2;
    return state;
}

} // namespace suns
