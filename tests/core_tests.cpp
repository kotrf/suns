#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <vector>

int main()
{
    const auto initial = suns::make_demo_game();
    assert(initial.turn == 1);
    assert(initial.fleets.size() == 1);

    suns::PlayerOrders orders;
    orders.player = 1;
    orders.orders.emplace_back(suns::MoveFleetOrder{1, {12.5, -4.0}});

    const suns::TurnProcessor processor;
    const auto next = processor.process(initial, {orders});

    assert(initial.turn == 1); // Processing must not mutate the previous state.
    assert(next.turn == 2);
    assert(next.fleets.front().position.x == 12.5);
    assert(next.fleets.front().position.y == -4.0);

    // A player cannot move another player's fleet.
    auto invalid = orders;
    invalid.player = 2;
    invalid.orders.clear();
    invalid.orders.emplace_back(suns::MoveFleetOrder{1, {99.0, 99.0}});
    const auto guarded = processor.process(initial, {invalid});
    assert(guarded.fleets.front().position.x == initial.fleets.front().position.x);

    return 0;
}
