#include "suns/communications.hpp"
#include "suns/production.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

using namespace suns;

GameState fixture(bool nearHome = false)
{
    auto state = make_demo_game();
    for (auto& player : state.players) player.technology.researchActive = false;
    for (auto& star : state.stars) star.variability = {};
    const auto home = state.stars.front().position;
    state.stars[1].position = {home.x + (nearHome ? 10.0 : 2000.0), home.y};
    auto& target = state.planets[1];
    target.owner = 0;
    target.population = 0;
    target.environment = {50, 50, 15};
    target.minerals = {30, 30, 30};
    state.fleets.clear();
    Fleet ship;
    ship.id = 2; ship.owner = 1; ship.design = kColonyShipDesignId;
    ship.role = FleetRole::ColonyShip;
    ship.ships = {{kColonyShipDesignId, 1}};
    ship.position = state.stars[1].position;
    ship.warp = 5; ship.fuel = 300; ship.colonists = 100;
    ship.electronics.mode = EmissionMode::Passive;
    state.fleets.push_back(ship);
    set_survey_level(state, 1, target.star, SurveyLevel::GeologicalSurvey, state.turn);
    publish_fleet_telemetry(state, state.turn);
    return state;
}

void shared_templates_accept_batches_and_reject_unsafe_steps()
{
    const auto presets = colony_development_presets();
    assert(presets.size() == 3);
    for (const auto& value : presets) assert(production_template_valid(value) && !value.defaultForNewColonies);
    auto value = presets[1];
    value.items.front().quantity = 0; assert(!production_template_valid(value));
    value = presets[1]; value.items.front().remainingCost = 0; assert(!production_template_valid(value));
    value = presets[1]; value.items.front().shipDesign = 1; assert(!production_template_valid(value));
    value = presets[1]; value.items.front().station = StationConstruction{}; assert(!production_template_valid(value));
    value = presets[1]; value.items.resize(65, value.items.front()); assert(!production_template_valid(value));
    value = presets[1]; value.items.front().quantity = 1001; assert(!production_template_valid(value));
    value = presets[1]; value.name.assign(81, 'x'); assert(!production_template_valid(value));
}

void industry_first_is_local_and_pays_normal_costs()
{
    auto state = fixture();
    const auto original = state;
    const auto plan = colony_development_presets()[1];
    const auto factoryMinerals = production_item_mineral_cost(state, plan.items.front());
    auto baseline = TurnProcessor{}.process(original, {{1, {ColonizePlanetOrder{2, original.planets[1].id}}}});
    state = TurnProcessor{}.process(state, {{1, {ColonizePlanetOrder{2, state.planets[1].id, plan}}}});
    assert(state.fleets.empty() && state.planets[1].owner == 1);
    assert(state.planets[1].industry == 1 && state.planets[1].radar == PlanetaryRadarLevel::None);
    assert(state.planets[1].productionQueue.front().kind == ProductionKind::Factory);
    assert(state.planets[1].productionQueue.front().remainingCost == kFactoryCost - 1);
    assert(communication_delay_turns(state, 1, state.stars[1].position) > 0);
    while (state.turn < original.turn + kFactoryCost) {
        state = TurnProcessor{}.process(state, {});
        baseline = TurnProcessor{}.process(baseline, {});
    }
    const auto& planet = state.planets[1];
    assert(planet.industry == 2 && planet.radar == PlanetaryRadarLevel::None);
    assert(std::abs(planet.minerals.ironium - (baseline.planets[1].minerals.ironium - factoryMinerals.ironium)) < 1e-6);
    assert(std::abs(planet.minerals.boranium - (baseline.planets[1].minerals.boranium - factoryMinerals.boranium)) < 1e-6);
    // No communication or follow-up orders: local production still reaches radar.
    while (state.turn < 40 && state.planets[1].radar == PlanetaryRadarLevel::None)
        state = TurnProcessor{}.process(state, {});
    assert(state.planets[1].industry == 4);
    assert(state.planets[1].radar == PlanetaryRadarLevel::Local);
    auto fast = TurnProcessor{}.process(original, {{1, {ColonizePlanetOrder{2, original.planets[1].id, colony_development_presets()[0]}}}});
    assert(fast.planets[1].productionQueue.front().kind == ProductionKind::LocalRadar);
    assert(fast.planets[1].industry == 1 && fast.planets[1].radar == PlanetaryRadarLevel::None);
    while (fast.planets[1].radar == PlanetaryRadarLevel::None && fast.turn < 40)
        fast = TurnProcessor{}.process(fast, {});
    assert(fast.planets[1].radar == PlanetaryRadarLevel::Local && fast.planets[1].industry == 1);
}

void no_plan_means_empty_even_with_a_mutable_default()
{
    for (bool near : {false, true}) {
        auto state = fixture(near);
        auto value = colony_development_presets()[0];
        value.defaultForNewColonies = true;
        state.players.front().productionTemplates = {value};
        state = TurnProcessor{}.process(state, {{1, {ColonizePlanetOrder{2, state.planets[1].id}}}});
        assert(state.planets[1].productionQueue.empty() && state.planets[1].radar == PlanetaryRadarLevel::None);
        assert((communication_delay_turns(state, 1, state.stars[1].position) == 0) == near);
    }
}

void delayed_terminal_plan_is_frozen_after_edit_and_delete()
{
    auto state = fixture();
    auto& ship = state.fleets.front();
    ship.position.x -= 100;
    ship.telemetry.position = ship.position;
    const auto plan = colony_development_presets()[1];
    state.players.front().productionTemplates = {plan};
    FleetArrivalAction action{FleetArrivalActionKind::Colonize};
    action.colonyPlan = plan;
    const auto delay = communication_delay_turns(state, 1, ship.position);
    assert(delay > 0);
    MoveFleetOrder route{ship.id, {ship.position.x + 10, ship.position.y}, 5, {},
        {{state.stars[1].position, 5, action}}};
    auto edited = plan;
    edited.items = colony_development_presets()[0].items;
    state = TurnProcessor{}.process(state, {{1, {route, SetProductionTemplateOrder{edited},
        SetProductionTemplateOrder{{plan.name}, true}}}});
    assert(state.planets[1].owner == 0 && state.players.front().productionTemplates.empty());
    assert(!state.fleets.front().pendingCommands.empty());
    const auto& carried = state.fleets.front().pendingCommands.front().program.queuedWaypoints.back().arrivalAction.colonyPlan;
    assert(carried && *carried == plan);
    while (state.planets[1].owner == 0 && state.turn < delay + 20)
        state = TurnProcessor{}.process(state, {});
    assert(state.planets[1].owner == 1 && state.fleets.empty());
    assert(state.planets[1].radar == PlanetaryRadarLevel::None);
    assert(state.planets[1].productionQueue.front().kind == ProductionKind::Factory);
}

void invalid_programs_never_consume_the_fleet_or_queue_work()
{
    auto state = fixture();
    auto broken = colony_development_presets()[1]; broken.items[0].quantity = 1001;
    auto result = TurnProcessor{}.process(state, {{1, {ColonizePlanetOrder{2, state.planets[1].id, broken}}}});
    assert(result.planets[1].owner == 0 && result.fleets.size() == 1);
    FleetArrivalAction action{FleetArrivalActionKind::Colonize}; action.colonyPlan = broken;
    assert(!submit_fleet_route_command(state, 1, 2, state.stars[1].position, 5, action, {}));
    action.colonyPlan = colony_development_presets()[1];
    assert(!fleet_route_tasks_valid({state.stars[1].position, 5, action, {{{0, 0}, 5, {}}}}));
    action.kind = FleetArrivalActionKind::None;
    assert(!submit_fleet_route_command(state, 1, 2, state.stars[1].position, 5, action, {}));
    state.planets[1].owner = 2;
    result = TurnProcessor{}.process(state, {{1, {ColonizePlanetOrder{2, state.planets[1].id, colony_development_presets()[1]}}}});
    assert(result.planets[1].owner == 2 && result.fleets.size() == 1);
}

int main()
{
    shared_templates_accept_batches_and_reject_unsafe_steps();
    industry_first_is_local_and_pays_normal_costs();
    no_plan_means_empty_even_with_a_mutable_default();
    delayed_terminal_plan_is_frozen_after_edit_and_delete();
    invalid_programs_never_consume_the_fleet_or_queue_work();
}
