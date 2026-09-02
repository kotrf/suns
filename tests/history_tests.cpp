#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>

namespace {

bool close(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

} // namespace

int main()
{
    auto state = suns::make_demo_game();
    assert(state.players.size() == 1);
    const auto& initialHistory = state.players.front().history;
    assert(initialHistory.size() == 1);
    assert(initialHistory.front().turn == 1);
    assert(initialHistory.front().colonies == 1);
    assert(initialHistory.front().population == 1000);
    assert(initialHistory.front().factories == 4);
    assert(initialHistory.front().fleets == 1);
    assert(initialHistory.front().ships == 1);
    assert(initialHistory.front().fleetMass > 0.0);
    assert(close(initialHistory.front().minerals.ironium, 100.0));

    const suns::TurnProcessor processor;
    const auto first = processor.process(state, {});
    const auto replay = processor.process(state, {});
    assert(first.turn == 2);
    assert(first.players.front().history.size() == 2);
    assert(first.players.front().history.back().turn == 2);
    assert(first.players.front().history.back().population > 1000);
    assert(first.players.front().history.back().population
        == replay.players.front().history.back().population);
    assert(close(
        first.players.front().history.back().minerals.germanium,
        replay.players.front().history.back().minerals.germanium));

    // Re-recording one planning boundary replaces its snapshot rather than
    // manufacturing a duplicate sample.
    auto corrected = first;
    corrected.planets.front().population += 50;
    suns::record_empire_turn_statistics(corrected);
    assert(corrected.players.front().history.size() == 2);
    assert(corrected.players.front().history.back().population
        == first.players.front().history.back().population + 50);

    // A player's history is built only from assets they own. Authoritative
    // enemy truth and neutral surface stockpiles never leak into the record.
    auto hidden = state;
    hidden.players.push_back({2, "Visitors", {}});
    hidden.planets[1].owner = 2;
    hidden.planets[1].population = 999999;
    hidden.planets[1].minerals = {888.0, 777.0, 666.0};
    hidden.fleets.push_back({
        50, 2, "Hidden fleet", suns::FleetRole::Scout, suns::kScoutDesignId,
        {20.0, 20.0}, std::nullopt, 1, 0.0, 0,
    });
    const auto playerOne = suns::empire_turn_statistics(hidden, 1);
    assert(playerOne.population == initialHistory.front().population);
    assert(close(playerOne.minerals.ironium, initialHistory.front().minerals.ironium));
    assert(playerOne.fleets == initialHistory.front().fleets);

    return 0;
}
