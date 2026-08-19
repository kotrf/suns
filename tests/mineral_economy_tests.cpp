#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>

namespace {

bool close(double a, double b)
{
    return std::abs(a - b) < 0.000001;
}

} // namespace

int main()
{
    auto state = suns::make_demo_game();
    auto& earth = state.planets.front();

    const auto concentration = suns::planet_mineral_concentration(state, earth);
    const auto repeat = suns::planet_mineral_concentration(state, earth);
    assert(close(concentration.ironium, repeat.ironium));
    assert(close(concentration.boranium, repeat.boranium));
    assert(close(concentration.germanium, repeat.germanium));
    assert(concentration.ironium >= 8.0 && concentration.ironium <= 100.0);
    assert(concentration.boranium >= 8.0 && concentration.boranium <= 100.0);
    assert(concentration.germanium >= 8.0 && concentration.germanium <= 100.0);

    // Spectral class is a prior, not a deterministic mineral table. Holding
    // seed/id constant proves the intended direction of the weak bias.
    auto blue = state;
    auto red = state;
    blue.stars.front().stellarClass = suns::StarClass::BlueWhite;
    red.stars.front().stellarClass = suns::StarClass::Red;
    const auto blueGeo = suns::planet_mineral_concentration(blue, blue.planets.front());
    const auto redGeo = suns::planet_mineral_concentration(red, red.planets.front());
    assert(blueGeo.ironium > redGeo.ironium);
    assert(redGeo.germanium > blueGeo.germanium);

    const auto mined = suns::projected_mineral_mining(state, earth);
    assert(mined.ironium > 0.0);
    assert(mined.boranium > 0.0);
    assert(mined.germanium > 0.0);

    const auto* scout = suns::find_ship_design(state, suns::kScoutDesignId);
    assert(scout != nullptr);
    const auto scoutMinerals = suns::ship_design_mineral_cost(*scout);
    assert(scoutMinerals.ironium > 0.0);
    assert(scoutMinerals.germanium > 0.0);

    // A factory completes with production points and consumes its mineral bill.
    state.planets.front().minerals = {100.0, 100.0, 100.0};
    const auto beforeMining = suns::projected_mineral_mining(state, state.planets.front());
    suns::PlayerOrders buildFactory{1, {}};
    buildFactory.orders.emplace_back(suns::QueueProductionOrder{1, suns::ProductionKind::Factory});
    const suns::TurnProcessor processor;
    const auto built = processor.process(state, {buildFactory});
    assert(built.planets.front().industry == state.planets.front().industry + 1);
    assert(built.planets.front().productionQueue.empty());
    assert(close(built.planets.front().minerals.ironium, 100.0 + beforeMining.ironium - 2.0));
    assert(close(built.planets.front().minerals.boranium, 100.0 + beforeMining.boranium - 1.0));
    assert(close(built.planets.front().minerals.germanium, 100.0 + beforeMining.germanium - 2.0));

    // Production points may finish while construction waits for material.
    auto starved = state;
    starved.planets.front().population = 0;
    starved.planets.front().stockpile = 2; // 4 output + 2 stored finishes the 6-point factory.
    starved.planets.front().minerals = {};
    starved.planets.front().productionQueue.clear();
    const auto blocked = processor.process(starved, {buildFactory});
    assert(blocked.planets.front().industry == starved.planets.front().industry);
    assert(blocked.planets.front().productionQueue.size() == 1);
    assert(blocked.planets.front().productionQueue.front().remainingCost == 0);

    return 0;
}
