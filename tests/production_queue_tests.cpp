#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>

namespace {
using namespace suns;

void reorder_is_applied_before_production()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    auto& colony = state.planets.front();
    colony.productionQueue = {
        {ProductionKind::Factory, kFactoryCost, 0},
        {ProductionKind::Mine, kMineCost, 0},
    };
    const auto mines = colony.mines;

    const auto result = processor.process(
        state, {{1, {ReorderProductionQueueOrder{colony.id, 1, 0}}}});
    assert(result.planets.front().mines == mines + 1);
    assert(result.planets.front().productionQueue.size() == 1);
    assert(result.planets.front().productionQueue.front().kind == ProductionKind::Factory);
}

void forecast_matches_resolved_completion_turns()
{
    TurnProcessor processor;
    auto state = make_demo_game();
    auto& colony = state.planets.front();
    colony.minerals = {100.0, 100.0, 100.0};
    colony.productionQueue = {
        {ProductionKind::Factory, kFactoryCost, 0},
        {ProductionKind::Mine, kMineCost, 0},
    };

    const auto forecast = forecast_production_queue(state, colony, colony.productionQueue);
    assert(forecast.size() == 2);
    assert(forecast[0].completionTurn);
    assert(forecast[1].completionTurn);

    std::uint64_t factoryTurn{};
    std::uint64_t mineTurn{};
    const auto startingIndustry = colony.industry;
    const auto startingMines = colony.mines;
    for (int step = 0; step < 20 && mineTurn == 0; ++step) {
        state = processor.process(state, {});
        if (factoryTurn == 0 && state.planets.front().industry > startingIndustry) factoryTurn = state.turn;
        if (mineTurn == 0 && state.planets.front().mines > startingMines) mineTurn = state.turn;
    }
    assert(factoryTurn == *forecast[0].completionTurn);
    assert(mineTurn == *forecast[1].completionTurn);
}

void research_marks_later_items_as_blocked()
{
    auto state = make_demo_game();
    auto& colony = state.planets.front();
    colony.productionQueue = {
        {ProductionKind::Research, 0, 0},
        {ProductionKind::Mine, kMineCost, 0},
    };
    const auto blocked = forecast_production_queue(state, colony, colony.productionQueue);
    assert(!blocked[0].completionTurn);
    assert(blocked[1].blockedByResearch);

    std::swap(colony.productionQueue[0], colony.productionQueue[1]);
    const auto unblocked = forecast_production_queue(state, colony, colony.productionQueue);
    assert(unblocked[0].completionTurn);
    assert(!unblocked[0].blockedByResearch);
}

} // namespace

int main()
{
    reorder_is_applied_before_production();
    forecast_matches_resolved_completion_turns();
    research_marks_later_items_as_blocked();
}
