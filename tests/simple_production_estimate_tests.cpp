#include "simple_production_estimate.hpp"

#include <cassert>
#include <limits>

namespace {
using namespace suns;

void fixed_rate_and_priority()
{
    auto state = make_demo_game(); state.turn = 10;
    const std::vector<ProductionItem> queue{{ProductionKind::Factory,3,0,ProductionAutomation::None,2},
        {ProductionKind::Mine,2,0,ProductionAutomation::None,3}};
    const auto estimates = simple_production_estimates(state, queue, 4, {100,100,100}, true);
    assert(estimates[0].remainingWork == 9 && estimates[1].remainingWork == 12);
    assert(estimates[0].completionTurn == 13 && estimates[1].completionTurn == 16);
    const auto faster = simple_production_estimates(state, queue, 8, {100,100,100}, true);
    assert(faster[0].completionTurn == 12 && faster[1].completionTurn == 13);
    // Future factories/population cannot change the supplied current rate.
    state.planets.front().industry = 100000;
    state.planets.front().population = 10000000;
    const auto unchanged = simple_production_estimates(state, queue, 4, {100,100,100}, true);
    assert(unchanged[1].completionTurn == estimates[1].completionTurn);
    assert(state.turn == 10 && queue.front().remainingCost == 3);
}

void annual_and_nonlinear_prefixes()
{
    const auto state = make_demo_game();
    std::vector<ProductionItem> queue{{ProductionKind::Factory,6,0,ProductionAutomation::Annual,1000},
        {ProductionKind::Mine,5}};
    auto estimates = simple_production_estimates(state, queue, 10, {100000,100000,100000}, true);
    assert(estimates[0].status == SimpleProductionStatus::AnnualRule && !estimates[0].completionTurn);
    assert(estimates[1].status == SimpleProductionStatus::EarlierUncertain && !estimates[1].completionTurn);
    for (const auto kind : {ProductionKind::Terraforming, ProductionKind::CleanupContamination,
        ProductionKind::ReconstructFactory, ProductionKind::ResonanceLaboratory, ProductionKind::OrbitalHabitat}) {
        queue[0] = {kind,6};
        estimates = simple_production_estimates(state, queue, 10, {100000,100000,100000}, true);
        assert(estimates[0].status == SimpleProductionStatus::Nonlinear && !estimates[0].completionTurn);
        assert(!estimates[1].completionTurn);
    }
}

void mineral_and_production_limits()
{
    const auto state = make_demo_game();
    const ProductionItem factory{ProductionKind::Factory,kFactoryCost};
    const auto cost = production_item_mineral_cost(state, factory);
    const std::vector<ProductionItem> queue{factory,factory,{ProductionKind::Mine,kMineCost}};
    const auto estimates = simple_production_estimates(state, queue, 100, cost, true);
    assert(estimates[0].completionTurn == state.turn + 1);
    assert(estimates[1].status == SimpleProductionStatus::MissingMinerals && !estimates[1].completionTurn);
    assert(estimates[2].status == SimpleProductionStatus::EarlierUncertain && !estimates[2].completionTurn);
    const auto noOutput = simple_production_estimates(state, queue, 0, {100,100,100}, true);
    assert(noOutput[0].status == SimpleProductionStatus::NoProduction && !noOutput[0].completionTurn);
    const auto noStock = simple_production_estimates(state, queue, 100, {}, true);
    assert(noStock[0].status == SimpleProductionStatus::MissingMinerals && !noStock[0].completionTurn);
    const auto waiting = simple_production_estimates(state, {{ProductionKind::Factory,0}}, 100, cost, true);
    assert(waiting[0].completionTurn == state.turn + 1); // Resolution is at a turn boundary, never instantly.
}

void missing_yard_and_design()
{
    const auto state = make_demo_game();
    const ProductionItem ship{ProductionKind::ColonyShip,kColonyShipCost,kColonyShipDesignId};
    const auto estimates = simple_production_estimates(state, {ship,{ProductionKind::Factory,6}},
        100, {10000,10000,10000}, false);
    assert(estimates[0].status == SimpleProductionStatus::MissingShipyard && !estimates[0].completionTurn);
    assert(!estimates[1].completionTurn);
    auto missing = ship; missing.shipDesign = std::numeric_limits<ShipDesignId>::max();
    const auto unavailable = simple_production_estimates(state, {missing}, 100, {10000,10000,10000}, true);
    assert(unavailable[0].status == SimpleProductionStatus::Unavailable && !unavailable[0].completionTurn);
}

void wide_work_and_date_overflow()
{
    auto state = make_demo_game();
    const auto maximum32 = std::numeric_limits<std::uint32_t>::max();
    const ProductionItem batch{ProductionKind::Factory,maximum32,0,ProductionAutomation::None,maximum32};
    const auto estimates = simple_production_estimates(state, {batch}, 1, {1e100,1e100,1e100}, true);
    assert(estimates[0].remainingWork == std::uint64_t(maximum32) + std::uint64_t(maximum32 - 1) * 6);
    assert(estimates[0].remainingWork > maximum32 && estimates[0].completionTurn);
    state.turn = std::numeric_limits<std::uint64_t>::max() - 1;
    const auto beyond = simple_production_estimates(state, {{ProductionKind::Factory,6}}, 1, {100,100,100}, true);
    assert(beyond[0].status == SimpleProductionStatus::Overflow && !beyond[0].completionTurn);
}
} // namespace

int main()
{
    fixed_rate_and_priority();
    annual_and_nonlinear_prefixes();
    mineral_and_production_limits();
    missing_yard_and_design();
    wide_work_and_date_overflow();
}
