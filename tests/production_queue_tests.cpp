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

void guaranteed_research_allocation_reduces_local_production()
{
    auto state = make_demo_game();
    auto& colony = state.planets.front();
    colony.productionQueue = {{ProductionKind::Factory, kFactoryCost, 0}};
    const auto fullProduction = forecast_production_queue(state, colony, colony.productionQueue);
    assert(fullProduction.front().completionTurn == state.turn + 1);

    state.players.front().technology.researchAllocationPercent = 50;
    const auto shared = forecast_production_queue(state, colony, colony.productionQueue);
    assert(shared.front().completionTurn == state.turn + 2);
}

} // namespace

void cancellation_obeys_order_sequence_and_ownership()
{
    using namespace suns;
    auto state = make_demo_game();
    state.players.front().technology.researchAllocationPercent = 100;
    auto& colony = state.planets.front();
    colony.productionQueue = {{ProductionKind::Factory, 2, 0}, {ProductionKind::Mine, 4, 0}};
    colony.productionWaitingForMinerals = true;
    colony.productionWaitingForShipyard = true;
    const auto planetId = colony.id;
    const auto next = TurnProcessor{}.process(state, {{1, {
        QueueShipDesignOrder{planetId, kScoutDesignId},
        ReorderProductionQueueOrder{planetId, 2, 0},
        CancelProductionOrder{planetId, 1}, // partially built factory, after reorder
        CancelProductionOrder{planetId, 0}, // freshly queued ship
        CancelProductionOrder{planetId, 99}, // invalid index is harmless
    }}, {2, {CancelProductionOrder{planetId, 0}}}});
    assert(next.planets.front().productionQueue.size() == 1);
    assert(next.planets.front().productionQueue.front().kind == ProductionKind::Mine);
    assert(next.planets.front().productionQueue.front().remainingCost == 4);
    assert(next.planets.front().industry == colony.industry);
    assert(!next.planets.front().productionWaitingForMinerals);
    assert(!next.planets.front().productionWaitingForShipyard);
    const auto cleared = TurnProcessor{}.process(next, {{1, {CancelProductionOrder{planetId, 0}}}});
    assert(cleared.planets.front().productionQueue.empty());
}

void cancelled_build_does_not_spend_minerals_or_complete()
{
    using namespace suns;
    auto state = make_demo_game();
    auto& colony = state.planets.front();
    colony.productionQueue = {{ProductionKind::Factory, 0, 0}};
    auto empty = state;
    empty.planets.front().productionQueue.clear();
    const auto cancelled = TurnProcessor{}.process(state, {{1, {CancelProductionOrder{colony.id, 0}}}});
    const auto idle = TurnProcessor{}.process(empty, {});
    assert(cancelled.planets.front().productionQueue.empty());
    assert(cancelled.planets.front().industry == colony.industry);
    assert(cancelled.planets.front().minerals.ironium == idle.planets.front().minerals.ironium);
    assert(cancelled.planets.front().minerals.boranium == idle.planets.front().minerals.boranium);
    assert(cancelled.planets.front().minerals.germanium == idle.planets.front().minerals.germanium);
}

int main()
{
    reorder_is_applied_before_production();
    forecast_matches_resolved_completion_turns();
    guaranteed_research_allocation_reduces_local_production();
    cancellation_obeys_order_sequence_and_ownership();
    cancelled_build_does_not_spend_minerals_or_complete();
}
