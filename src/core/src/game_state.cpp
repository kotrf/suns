#include "suns/game_state.hpp"

namespace suns {

GameState make_demo_game()
{
    GameState state;
    state.players.push_back({1, "Terrans"});
    state.stars.push_back({1, "Sol", {0.0, 0.0}, 1});
    state.stars.push_back({2, "Alpha Centauri", {38.0, 16.0}, 0});
    state.stars.push_back({3, "Sirius", {-30.0, 34.0}, 0});
    state.fleets.push_back({1, 1, {0.0, 0.0}});
    return state;
}

} // namespace suns
