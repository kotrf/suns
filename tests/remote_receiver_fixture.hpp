#pragma once
#include "suns/campaign.hpp"
#include "suns/player_knowledge.hpp"
#include <algorithm>
#include <cassert>

namespace suns::test {
inline GameState remote_receiver_fixture()
{
    auto state = make_demo_game();
    state.turn = 10;
    state.wormholeRules.spawnChancePerTurn = 0;
    state.players.front().technology.researchAllocationPercent = 100;
    state.fleets.clear();
    state.stars[0].position = {0, 0};
    state.stars[1].position = {900, 0};
    auto& colony = state.planets[1];
    colony.owner = 1;
    colony.population = 1'000'000;
    colony.industry = 4;
    colony.mines = 0;
    colony.minerals = {100, 100, 100};
    colony.radar = PlanetaryRadarLevel::None;
    colony.productionQueue = {{ProductionKind::LocalRadar, 1}, {ProductionKind::Factory, kFactoryCost}};
    set_survey_level(state, 1, colony.star, SurveyLevel::DeepSurvey, state.turn);
    return state;
}

inline void deliver_receiver_report(GameState& state)
{
    queue_survey_report(state, 1, 2, 0, state.stars[1].position, state.turn, SurveyLevel::DeepSurvey, true);
    const auto& reports = state.players.front().pendingSurveyReports;
    const auto report = std::find_if(reports.begin(), reports.end(), [&](const auto& r) {
        return r.star == 2 && r.observedTurn == state.turn && r.planetSnapshot && r.planetSnapshot->habitat
            && r.planetSnapshot->habitat->radar == PlanetaryRadarLevel::Local;
    });
    assert(report != reports.end() && report->deliveryTurn > state.turn);
    state.turn = report->deliveryTurn;
    (void)deliver_due_survey_reports(state);
}
} // namespace suns::test
