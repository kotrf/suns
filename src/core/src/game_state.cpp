#include "suns/game_state.hpp"

namespace suns {

GameState make_demo_game()
{
    GameState state;
    state.players.push_back({1, "Terrans"});

    state.stars = {
        {1, "Sol", {0.0, 0.0}, 1},
        {2, "Alpha Centauri", {86.0, -42.0}, 0},
        {3, "Sirius", {-118.0, 76.0}, 0},
        {4, "Vega", {154.0, 96.0}, 0},
        {5, "Altair", {42.0, 142.0}, 0},
        {6, "Tau Ceti", {-72.0, -126.0}, 0},
        {7, "Epsilon Eridani", {142.0, -132.0}, 0},
        {8, "Procyon", {-168.0, -34.0}, 0},
    };

    state.fleets.push_back({1, 1, "Scout 1", {0.0, 0.0}});
    return state;
}

} // namespace suns
