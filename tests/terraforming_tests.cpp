#include "suns/campaign.hpp"
#include "suns/production.hpp"
#include "suns/terraforming.hpp"

#include <algorithm>
#include <cassert>

using namespace suns;

GameState fixture()
{
    auto state = make_demo_game();
    state.players.front().race = race_preset(RacePreset::Terran);
    state.players.front().technology.levels.fill(0);
    state.players.front().technology.researchActive = false;
    auto& colony = state.planets.front();
    colony.environment = {24, 50, 20};
    colony.industry = 12;
    colony.population = 1000;
    colony.minerals = {1000, 1000, 1000};
    colony.mines = 0;
    for (auto& star : state.stars) star.variability = {};
    return state;
}

void unlock(GameState& state, int biology, int physics)
{
    auto& levels = state.players.front().technology.levels;
    levels[std::size_t(ResearchField::Biology)] = biology;
    for (const auto field : {ResearchField::Energy, ResearchField::Propulsion, ResearchField::Weapons})
        levels[std::size_t(field)] = physics;
}

void technology_requires_two_fields_and_does_not_change_the_planet()
{
    auto state = fixture();
    auto& planet = state.planets.front();
    assert(!next_terraforming_environment(state, 1, planet));
    unlock(state, 1, 0);
    assert(!next_terraforming_environment(state, 1, planet));
    assert(current_planet_habitability(state, planet, state.turn) < 0);
    for (const auto [bio, physics, expected] : {std::array<int, 3>{1, 1, 3}, {2, 5, 7}, {3, 10, 11}, {4, 16, 15}}) {
        unlock(state, bio, physics);
        const auto limits = terraforming_limits(state, 1);
        assert(limits.temperature == expected && limits.gravity == expected && limits.radiation == expected);
    }
    assert(planet.environment.temperature == 24);
    assert(race_habitability(state.players.front().race, planet.environment, 4) < 0);
    auto legacy = state.players.front().race;
    legacy.legacyBiologyAdaptation = true;
    assert(race_habitability(legacy, planet.environment, 1) > 0);
}

void production_changes_environment_before_population_growth()
{
    auto state = fixture();
    unlock(state, 1, 1);
    const auto before = state.planets.front();
    const auto next = TurnProcessor{}.process(state, {{1, {QueueProductionBatchOrder{before.id, ProductionKind::Terraforming}}}});
    const auto& planet = next.planets.front();
    assert(planet.environment.temperature == 25);
    assert(planet.naturalEnvironment && planet.naturalEnvironment->temperature == 24);
    assert(planet.population > before.population);
    assert(planet.minerals.ironium == before.minerals.ironium + projected_mineral_mining(state, before).ironium);
    assert(planet.productionQueue.empty());
    state.planets.front().industry = kTerraformingCost - 1;
    const auto partial = TurnProcessor{}.process(state, {{1, {QueueProductionBatchOrder{before.id, ProductionKind::Terraforming}}}});
    assert(partial.planets.front().environment.temperature == 24);
    assert(partial.planets.front().productionQueue.front().remainingCost == 1);
    assert(partial.planets.front().population < before.population);
}

void natural_limit_persists_and_multiple_hostile_axes_can_be_fixed()
{
    auto state = fixture();
    unlock(state, 1, 1);
    auto& planet = state.planets.front();
    planet.environment = {24, 29, 20};
    assert(terraform_planet(state, planet));
    assert(current_planet_habitability(state, planet, state.turn) < 0);
    assert(terraform_planet(state, planet));
    assert(current_planet_habitability(state, planet, state.turn) > 0);
    planet.naturalEnvironment.reset();
    planet.environment = {80, 50, 20};
    for (int i = 0; i < 3; ++i) assert(terraform_planet(state, planet));
    assert(planet.environment.temperature == 77);
    assert(!terraform_planet(state, planet));
    unlock(state, 2, 5);
    for (int i = 0; i < 4; ++i) assert(terraform_planet(state, planet));
    assert(planet.environment.temperature == 73 && planet.naturalEnvironment->temperature == 80);
    assert(!terraform_planet(state, planet));
    // Physical changes survive abandonment and a different owner. This race
    // has a hot optimum and can undo the previous owner's work towards baseline.
    state.players.push_back({2, "Hot"});
    state.players.back().race = race_preset(RacePreset::Radiotroph);
    state.players.back().race.habitableTemperature = {65, 95};
    state.players.back().technology.levels.fill(1);
    planet.owner = 0;
    assert(planet.environment.temperature == 73);
    planet.owner = 2;
    assert(terraform_planet(state, planet) && planet.environment.temperature == 74);
    assert(planet.naturalEnvironment->temperature == 80);
    assert(terraforming_limits(state, 2).radiation == 0);
}

void annual_limits_priority_and_partial_work()
{
    auto state = fixture();
    auto& planet = state.planets.front();
    planet.industry = 100;
    const auto output = planet.industry;
    state = TurnProcessor{}.process(state, {{1, {
        QueueProductionBatchOrder{planet.id, ProductionKind::Factory, 3, ProductionAutomation::Annual},
        QueueProductionBatchOrder{planet.id, ProductionKind::Mine, 2, ProductionAutomation::Annual},
    }}});
    assert(state.planets.front().industry == output + 3 && state.planets.front().mines == 2);
    assert(state.planets.front().productionQueue.size() == 2);
    state = TurnProcessor{}.process(state, {});
    assert(state.planets.front().industry == output + 6 && state.planets.front().mines == 4);
    assert(state.planets.front().productionQueue.size() == 2);
    state = fixture();
    state.planets.front().industry = 5;
    const auto id = state.planets.front().id;
    state = TurnProcessor{}.process(state, {{1, {QueueProductionBatchOrder{id, ProductionKind::Factory, 1, ProductionAutomation::Annual}}}});
    assert(state.planets.front().industry == 5);
    assert(state.planets.front().productionQueue.size() == 2);
    assert(state.planets.front().productionQueue.front().autoGenerated);
    assert(state.planets.front().productionQueue.front().remainingCost == 1);
    state = TurnProcessor{}.process(state, {});
    assert(state.planets.front().industry == 6); // Finishing last year's unit consumes this year's quota.
    assert(state.planets.front().productionQueue.size() == 1);
    state = fixture();
    unlock(state, 1, 1);
    state.planets.front().minerals = {};
    state = TurnProcessor{}.process(state, {{1, {
        QueueProductionBatchOrder{id, ProductionKind::Factory, 10, ProductionAutomation::Annual},
        QueueProductionBatchOrder{id, ProductionKind::Terraforming},
    }}});
    assert(state.planets.front().environment.temperature == 25); // Mineral-short auto factories are skipped.
    assert(state.planets.front().productionQueue.size() == 1);
}

void automatic_terraforming_stops_and_forecast_matches_host()
{
    auto state = fixture();
    unlock(state, 1, 1);
    auto& planet = state.planets.front();
    planet.industry = 60;
    planet.productionQueue = {{ProductionKind::Terraforming, kTerraformingCost, 0, ProductionAutomation::MinTerraform, 10}};
    state = TurnProcessor{}.process(state, {});
    assert(state.planets.front().environment.temperature == 25); // Stop at habitability; do not spend the spare four units.
    state.planets.front().productionQueue.front().automation = ProductionAutomation::MaxTerraform;
    state = TurnProcessor{}.process(state, {});
    assert(state.planets.front().environment.temperature == 27); // Original 24 + the technology limit 3.
    assert(state.planets.front().productionQueue.size() == 1);
    state = fixture();
    unlock(state, 1, 1);
    state.planets.front().industry = 10;
    state.planets.front().productionQueue = {
        {ProductionKind::Factory, kFactoryCost, 0, ProductionAutomation::Annual, 1},
        {ProductionKind::Terraforming, kTerraformingCost},
        {ProductionKind::Mine, kMineCost, 0, ProductionAutomation::None, 2},
    };
    const auto estimates = forecast_production_queue(state, state.planets.front(), state.planets.front().productionQueue, 50);
    assert(estimates[0].completionTurn && estimates[1].completionTurn && estimates[2].completionTurn);
    std::uint64_t terraformTurn{}, mineTurn{};
    for (int i = 0; i < 50 && mineTurn == 0; ++i) {
        state = TurnProcessor{}.process(state, {});
        if (terraformTurn == 0 && state.planets.front().environment.temperature == 25) terraformTurn = state.turn;
        if (mineTurn == 0 && state.planets.front().mines == 2) mineTurn = state.turn;
    }
    assert(terraformTurn == *estimates[1].completionTurn && mineTurn == *estimates[2].completionTurn);
}

void templates_and_orders_obey_ownership_and_validation()
{
    auto state = fixture();
    unlock(state, 1, 1);
    const auto id = state.planets.front().id;
    ProductionTemplate value{"New colony", {{ProductionKind::Factory, kFactoryCost, 0, ProductionAutomation::Annual, 10}}, true};
    const PlayerOrders orders{1, {SetProductionTemplateOrder{value}, ApplyProductionTemplateOrder{id, value.name},
        QueueProductionBatchOrder{id, ProductionKind::Mine, 0},
        QueueProductionBatchOrder{id, ProductionKind::ColonyShip, 10, ProductionAutomation::Annual},
        QueueProductionBatchOrder{id, ProductionKind::Factory, 1001}}};
    const auto planned = planned_production_state(state, orders);
    assert(planned.planets.front().productionQueue.size() == 1);
    assert(planned.players.front().productionTemplates.size() == 1);
    state.players.front().technology.researchAllocationPercent = 100;
    state.players.front().technology.researchActive = true;
    state = TurnProcessor{}.process(state, {orders, {2, {CancelProductionOrder{id, 0}, QueueProductionBatchOrder{id, ProductionKind::Mine, 1}}}});
    assert(state.planets.front().productionQueue.size() == 1);
    assert(state.planets.front().productionQueue.front().quantity == 10);
    state.planets.front().productionQueue.insert(state.planets.front().productionQueue.begin(), {ProductionKind::Mine, 2});
    assert(apply_production_template(state.planets.front(), state.players.front(), value.name));
    assert(state.planets.front().productionQueue.size() == 2 && state.planets.front().productionQueue.front().remainingCost == 2);
    auto target = state.planets.back();
    target.owner = 1;
    target.productionQueue.clear();
    apply_default_production_template(state, target);
    assert(target.productionQueue.size() == 1);
    const auto view = make_player_view(state, 1);
    assert(view.state.players.front().productionTemplates.size() == 1);
}

void founding_a_colony_applies_the_default_template()
{
    auto state = fixture();
    ProductionTemplate value{"Expansion", {{ProductionKind::Factory, kFactoryCost, 0, ProductionAutomation::Annual, 10}}, true};
    auto& target = *std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 0; });
    const auto targetId = target.id;
    const auto starId = target.star;
    target.environment = {50, 50, 20};
    auto& fleet = state.fleets.front();
    fleet.design = kColonyShipDesignId;
    fleet.ships = {{kColonyShipDesignId, 1}};
    fleet.position = find_star(state, starId)->position;
    fleet.colonists = 20'000;
    assert(fleet_cargo_used(state, fleet) <= fleet_cargo_capacity(state, fleet));
    set_survey_level(state, 1, starId, SurveyLevel::OrbitalSurvey, state.turn);
    const auto next = TurnProcessor{}.process(state, {{1, {SetProductionTemplateOrder{value}, ColonizePlanetOrder{fleet.id, targetId}}}});
    const auto* founded = find_planet_at_star(next, starId);
    assert(founded && founded->owner == 1);
    assert(std::any_of(founded->productionQueue.begin(), founded->productionQueue.end(),
        [](const auto& item) { return item.automation == ProductionAutomation::Annual && item.quantity == 10; }));
}

int main()
{
    technology_requires_two_fields_and_does_not_change_the_planet();
    production_changes_environment_before_population_growth();
    natural_limit_persists_and_multiple_hostile_axes_can_be_fixed();
    annual_limits_priority_and_partial_work();
    automatic_terraforming_stops_and_forecast_matches_host();
    templates_and_orders_obey_ownership_and_validation();
    founding_a_colony_applies_the_default_template();
}
