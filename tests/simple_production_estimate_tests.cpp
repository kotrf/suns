#include "simple_production_estimate.hpp"
#include "suns/production.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {
using namespace suns;

void fixed_rate_and_priority()
{
    auto state = make_demo_game(); state.turn = 10;
    const std::vector<ProductionItem> queue{{ProductionKind::Factory,3,0,ProductionAutomation::None,2},
        {ProductionKind::Mine,2,0,ProductionAutomation::None,3}};
    const auto estimates = simple_production_estimates(state, state.planets.front(), queue, 4, {100,100,100}, true);
    assert(estimates[0].remainingWork == 13 && estimates[1].remainingWork == 12);
    assert(estimates[0].completionTurn == 14 && estimates[1].completionTurn == 17);
    const auto faster = simple_production_estimates(state, state.planets.front(), queue, 8, {100,100,100}, true);
    assert(faster[0].completionTurn == 12 && faster[1].completionTurn == 14);
    // Future factories/population cannot change the supplied current rate.
    state.planets.front().industry = 100000;
    state.planets.front().population = 10000000;
    const auto unchanged = simple_production_estimates(state, state.planets.front(), queue, 4, {100,100,100}, true);
    assert(unchanged[1].completionTurn == estimates[1].completionTurn);
    assert(state.turn == 10 && queue.front().remainingCost == 3);
}

void annual_and_nonlinear_prefixes()
{
    const auto state = make_demo_game();
    std::vector<ProductionItem> queue{{ProductionKind::Factory,6,0,ProductionAutomation::Annual,1000},
        {ProductionKind::Mine,5}};
    auto estimates = simple_production_estimates(state, state.planets.front(), queue, 10, {100000,100000,100000}, true);
    assert(estimates[0].status == SimpleProductionStatus::AnnualRule && !estimates[0].completionTurn);
    assert(estimates[1].status == SimpleProductionStatus::EarlierUncertain && !estimates[1].completionTurn);
    for (const auto kind : {ProductionKind::Terraforming, ProductionKind::CleanupContamination,
        ProductionKind::ReconstructFactory, ProductionKind::ResonanceLaboratory, ProductionKind::OrbitalHabitat}) {
        queue[0] = {kind,6};
        estimates = simple_production_estimates(state, state.planets.front(), queue, 10, {100000,100000,100000}, true);
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
    const auto estimates = simple_production_estimates(state, state.planets.front(), queue, 100, cost, true);
    assert(estimates[0].completionTurn == state.turn + 1);
    assert(estimates[1].status == SimpleProductionStatus::MissingMinerals && !estimates[1].completionTurn);
    assert(estimates[2].status == SimpleProductionStatus::EarlierUncertain && !estimates[2].completionTurn);
    const auto noOutput = simple_production_estimates(state, state.planets.front(), queue, 0, {100,100,100}, true);
    assert(noOutput[0].status == SimpleProductionStatus::NoProduction && !noOutput[0].completionTurn);
    const auto noStock = simple_production_estimates(state, state.planets.front(), queue, 100, {}, true);
    assert(noStock[0].status == SimpleProductionStatus::MissingMinerals && !noStock[0].completionTurn);
    const auto waiting = simple_production_estimates(state, state.planets.front(), {{ProductionKind::Factory,0}}, 100, cost, true);
    assert(waiting[0].completionTurn == state.turn + 1); // Resolution is at a turn boundary, never instantly.
}

void missing_yard_and_design()
{
    const auto state = make_demo_game();
    const ProductionItem ship{ProductionKind::ColonyShip,kColonyShipCost,kColonyShipDesignId};
    const auto estimates = simple_production_estimates(state, state.planets.front(), {ship,{ProductionKind::Factory,6}},
        100, {10000,10000,10000}, false);
    assert(estimates[0].status == SimpleProductionStatus::MissingShipyard && !estimates[0].completionTurn);
    assert(!estimates[1].completionTurn);
    auto missing = ship; missing.shipDesign = std::numeric_limits<ShipDesignId>::max();
    const auto unavailable = simple_production_estimates(state, state.planets.front(), {missing}, 100, {10000,10000,10000}, true);
    assert(unavailable[0].status == SimpleProductionStatus::Unavailable && !unavailable[0].completionTurn);
}

void wide_work_and_date_overflow()
{
    auto state = make_demo_game();
    const auto maximum32 = std::numeric_limits<std::uint32_t>::max();
    const ProductionItem batch{ProductionKind::Factory,maximum32,0,ProductionAutomation::None,maximum32};
    const auto estimates = simple_production_estimates(state, state.planets.front(), {batch}, 1, {1e100,1e100,1e100}, true);
    assert(estimates[0].remainingWork == std::uint64_t(maximum32) + std::uint64_t(maximum32 - 5) * kFactoryCost);
    assert(estimates[0].remainingWork > maximum32 && estimates[0].completionTurn);
    state.turn = std::numeric_limits<std::uint64_t>::max() - 1;
    const auto beyond = simple_production_estimates(state, state.planets.front(), {{ProductionKind::Factory,6}}, 1, {100,100,100}, true);
    assert(beyond[0].status == SimpleProductionStatus::Overflow && !beyond[0].completionTurn);
}

GameState production_fixture(std::uint32_t rate = kFactoryCost)
{
    auto state = make_demo_game();
    auto& player = state.players.front();
    player.technology.researchActive = false;
    player.technology.levels.fill(0);
    auto& planet = state.planets.front();
    planet.population = std::uint64_t(rate) * kPopulationPerProductionUnit;
    planet.industry = 0;
    planet.mines = 0;
    planet.defenses = 0;
    planet.radar = PlanetaryRadarLevel::None;
    planet.productionQueue.clear();
    assert(colony_output(state, planet) == rate);
    return state;
}

MineralCargo plus(MineralCargo a, const MineralCargo& b)
{
    return {a.ironium + b.ironium, a.boranium + b.boranium, a.germanium + b.germanium};
}

void assert_empty_minerals(const MineralCargo& minerals)
{
    assert(std::abs(minerals.ironium) < 1e-9);
    assert(std::abs(minerals.boranium) < 1e-9);
    assert(std::abs(minerals.germanium) < 1e-9);
}

void skipped_defense_templates_match_one_turn_resolution()
{
    const ProductionItem factory{ProductionKind::Factory, kFactoryCost};
    for (const bool capped : {false, true}) {
        auto state = production_fixture();
        auto& planet = state.planets.front();
        auto& player = state.players.front();
        player.technology.levels[std::size_t(ResearchField::Energy)] = capped ? 1 : 0;
        planet.defenses = capped ? kMaximumColonyDefenses : 0;
        const ProductionTemplate value{"Skip impossible defenses", {
            {ProductionKind::ColonyDefense, production_cost(ProductionKind::ColonyDefense), 0,
                ProductionAutomation::None, 100}, factory}};
        assert(production_template_valid(value));
        assert(set_production_template(player, {value}));
        assert(apply_production_template(planet, player, value.name));
        // Exactly one factory's minerals: skipped defenses must not reserve any.
        planet.minerals = production_item_mineral_cost(state, factory);
        const auto estimates = simple_production_estimates(state, planet, planet.productionQueue,
            colony_output(state, planet), planet.minerals, false);
        assert(estimates[0].status == SimpleProductionStatus::Skipped);
        assert(estimates[0].remainingWork == 0 && estimates[0].effectiveQuantity == 0);
        assert(estimates[0].skippedQuantity == 100 && !estimates[0].completionTurn);
        assert(estimates[1].completionTurn == state.turn + 1);
        assert(planet.productionQueue == value.items); // Estimation never changes the intent.
        auto resolved = planet;
        const auto result = resolve_colony_production(state, resolved, false, false);
        assert(result.completions.size() == 1 && result.completions[0].originalIndex == 1);
        assert(resolved.industry == 1 && resolved.defenses == planet.defenses);
        assert(resolved.productionQueue.empty());
        assert_empty_minerals(resolved.minerals);
    }
}

void inactive_annual_defenses_do_not_block_later_work()
{
    for (const bool capped : {false, true}) {
        auto state = production_fixture();
        auto& planet = state.planets.front();
        state.players.front().technology.levels[std::size_t(ResearchField::Energy)] = capped ? 1 : 0;
        planet.defenses = capped ? kMaximumColonyDefenses : 0;
        const ProductionItem factory{ProductionKind::Factory, kFactoryCost};
        planet.productionQueue = {
            {ProductionKind::ColonyDefense, production_cost(ProductionKind::ColonyDefense), 0,
                ProductionAutomation::Annual, 100}, factory};
        planet.minerals = production_item_mineral_cost(state, factory);
        const auto estimates = simple_production_estimates(state, planet, planet.productionQueue,
            colony_output(state, planet), planet.minerals, false);
        assert(estimates[0].status == (capped ? SimpleProductionStatus::InactiveAnnualRule : SimpleProductionStatus::Skipped));
        assert(estimates[0].remainingWork == 0 && estimates[0].effectiveQuantity == 0);
        assert(!estimates[0].completionTurn && estimates[1].completionTurn == state.turn + 1);
        auto resolved = planet;
        const auto result = resolve_colony_production(state, resolved, false, false);
        assert(result.completions.size() == 1 && resolved.industry == 1);
        assert(resolved.productionQueue.size() == (capped ? 1 : 0));
        if (capped) assert(resolved.productionQueue.front().automation == ProductionAutomation::Annual);
        assert_empty_minerals(resolved.minerals);
    }
}

void partial_and_repeated_defense_caps_use_only_buildable_units()
{
    const auto defenseCost = production_cost(ProductionKind::ColonyDefense);
    for (const bool separateRows : {false, true}) {
        const auto firstRemaining = separateRows ? defenseCost : 2U;
        auto state = production_fixture(firstRemaining + kFactoryCost);
        state.players.front().technology.levels[std::size_t(ResearchField::Energy)] = 1;
        auto& planet = state.planets.front();
        planet.defenses = kMaximumColonyDefenses - 1;
        const ProductionItem defense{ProductionKind::ColonyDefense, firstRemaining, 0,
            ProductionAutomation::None, separateRows ? 1U : 100U};
        const ProductionItem factory{ProductionKind::Factory, kFactoryCost};
        planet.productionQueue = {defense};
        if (separateRows) planet.productionQueue.push_back({ProductionKind::ColonyDefense, defenseCost, 0,
            ProductionAutomation::None, 100});
        planet.productionQueue.push_back(factory);
        planet.minerals = plus(production_item_mineral_cost(state, defense), production_item_mineral_cost(state, factory));
        const auto estimates = simple_production_estimates(state, planet, planet.productionQueue,
            colony_output(state, planet), planet.minerals, false);
        assert(estimates.front().effectiveQuantity == 1 && estimates.front().remainingWork == firstRemaining);
        assert(estimates.front().skippedQuantity == (separateRows ? 0 : 99));
        assert(estimates.front().completionTurn == state.turn + 1);
        if (separateRows) {
            assert(estimates[1].status == SimpleProductionStatus::Skipped);
            assert(estimates[1].remainingWork == 0 && estimates[1].effectiveQuantity == 0);
        }
        assert(estimates.back().completionTurn == state.turn + 1);
        auto resolved = planet;
        const auto result = resolve_colony_production(state, resolved, false, false);
        assert(result.completions.size() == 2 && resolved.defenses == kMaximumColonyDefenses);
        assert(resolved.industry == 1 && resolved.productionQueue.empty());
        assert_empty_minerals(resolved.minerals);
    }
}

void installed_and_duplicate_radars_do_not_reserve_extra_work()
{
    const ProductionItem factory{ProductionKind::Factory, kFactoryCost};
    for (const auto kind : {ProductionKind::LocalRadar, ProductionKind::PlanetaryRadar}) {
        auto state = production_fixture();
        auto& planet = state.planets.front();
        planet.radar = kind == ProductionKind::LocalRadar ? PlanetaryRadarLevel::Local : PlanetaryRadarLevel::Full;
        planet.productionQueue = {{kind, production_cost(kind)}, factory};
        planet.minerals = production_item_mineral_cost(state, factory);
        const auto estimates = simple_production_estimates(state, planet, planet.productionQueue,
            colony_output(state, planet), planet.minerals, false);
        assert(estimates[0].status == SimpleProductionStatus::Skipped && estimates[0].effectiveQuantity == 0);
        assert(estimates[1].completionTurn == state.turn + 1);
        auto resolved = planet;
        const auto result = resolve_colony_production(state, resolved, false, false);
        assert(result.completions.size() == 1 && resolved.industry == 1 && resolved.productionQueue.empty());
        assert_empty_minerals(resolved.minerals);
    }
    // Both orders are legal before either receiver is completed.
    auto state = production_fixture(production_cost(ProductionKind::LocalRadar) + kFactoryCost);
    auto& planet = state.planets.front();
    assert(apply_production_batch(state, planet, {planet.id, ProductionKind::LocalRadar}));
    assert(apply_production_batch(state, planet, {planet.id, ProductionKind::LocalRadar}));
    planet.productionQueue.push_back(factory);
    planet.minerals = plus(production_item_mineral_cost(state, planet.productionQueue.front()), production_item_mineral_cost(state, factory));
    const auto estimates = simple_production_estimates(state, planet, planet.productionQueue,
        colony_output(state, planet), planet.minerals, false);
    assert(estimates[0].effectiveQuantity == 1 && estimates[0].completionTurn == state.turn + 1);
    assert(estimates[1].status == SimpleProductionStatus::Skipped && estimates[1].remainingWork == 0);
    assert(estimates[2].completionTurn == state.turn + 1);
    auto resolved = planet;
    const auto result = resolve_colony_production(state, resolved, false, false);
    assert(result.completions.size() == 2 && resolved.radar == PlanetaryRadarLevel::Local);
    assert(resolved.industry == 1 && resolved.productionQueue.empty());
    assert_empty_minerals(resolved.minerals);
}

void invalid_stations_are_free_skips_and_do_not_create_a_shipyard()
{
    const ProductionItem factory{ProductionKind::Factory, kFactoryCost};
    for (const unsigned scenario : {0U, 1U, 2U, 3U}) {
        auto state = production_fixture();
        state.orbitalStations.front().modules.clear(); // Occupied orbit, no shipyard.
        auto& planet = state.planets.front();
        ProductionItem station{ProductionKind::OrbitalStation, kOrbitalDockCost};
        if (scenario) {
            StationConstruction construction;
            if (scenario == 1) construction.refitStation = state.nextOrbitalStationId; // Stale target.
            // scenario 2: a new station conflicts with the existing station.
            if (scenario == 3) {
                state.orbitalStations.clear();
                construction.loadout.hull = OrbitalStationHullType::Fortress; // Technology unavailable.
            }
            station.station = construction;
            station.remainingCost = station_build_cost(construction.loadout);
            assert(!station_construction_valid(state, planet, construction));
        }
        const ProductionItem ship{ProductionKind::ColonyShip, kColonyShipCost, kColonyShipDesignId};
        planet.productionQueue = {station, factory, ship};
        planet.minerals = production_item_mineral_cost(state, factory);
        const auto estimates = simple_production_estimates(state, planet, planet.productionQueue,
            colony_output(state, planet), planet.minerals, false);
        assert(estimates[0].status == SimpleProductionStatus::Skipped);
        assert(estimates[0].remainingWork == 0 && estimates[0].effectiveQuantity == 0);
        assert(estimates[1].completionTurn == state.turn + 1);
        assert(estimates[2].status == SimpleProductionStatus::MissingShipyard);
        auto resolved = planet;
        const auto result = resolve_colony_production(state, resolved, false, false);
        assert(!result.hasShipyard && result.completions.size() == 1 && result.completions[0].originalIndex == 1);
        assert(resolved.industry == 1 && resolved.productionWaitingForShipyard);
        assert(resolved.productionQueue.size() == 1 && resolved.productionQueue.front().kind == ProductionKind::ColonyShip);
        assert_empty_minerals(resolved.minerals);
    }
}

void free_skips_never_restore_an_uncertain_prefix()
{
    auto state = production_fixture();
    const auto& planet = state.planets.front();
    const ProductionItem factory{ProductionKind::Factory, kFactoryCost};
    const auto estimates = simple_production_estimates(state, planet,
        {factory, {ProductionKind::ColonyDefense, production_cost(ProductionKind::ColonyDefense)}, factory},
        colony_output(state, planet), {}, false);
    assert(estimates[0].status == SimpleProductionStatus::MissingMinerals);
    assert(estimates[1].status == SimpleProductionStatus::Skipped);
    assert(estimates[1].effectiveQuantity == 0 && estimates[1].remainingWork == 0);
    assert(estimates[2].status == SimpleProductionStatus::EarlierUncertain && !estimates[2].completionTurn);
}
} // namespace

int main()
{
    fixed_rate_and_priority();
    annual_and_nonlinear_prefixes();
    mineral_and_production_limits();
    missing_yard_and_design();
    wide_work_and_date_overflow();
    skipped_defense_templates_match_one_turn_resolution();
    inactive_annual_defenses_do_not_block_later_work();
    partial_and_repeated_defense_caps_use_only_buildable_units();
    installed_and_duplicate_radars_do_not_reserve_extra_work();
    invalid_stations_are_free_skips_and_do_not_create_a_shipyard();
    free_skips_never_restore_an_uncertain_prefix();
}
