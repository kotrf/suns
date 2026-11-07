#pragma once
#include "suns/campaign.hpp"
#include "suns/native_orbital.hpp"
#include "suns/communications.hpp"
#include <cassert>

namespace suns::test {
inline GameState native_orbital_fixture()
{
    auto state = make_demo_game();
    state.wormholeRules.spawnChancePerTurn = 0;
    auto& player = state.players.front();
    player.race.primaryTrait = PrimaryRaceTrait::HabitatCivilization;
    player.technology.researchActive = false;
    player.technology.researchAllocationPercent = 0;
    auto& planet = state.planets.front();
    planet.population = 0; planet.industry = 0; planet.mines = 0;
    planet.productionQueue.clear(); planet.habitat = {}; planet.engineering = {};
    planet.nativeOrbit = {state.orbitalStations.front().id, 10'000};
    state.fleets.resize(1);
    auto& fleet = state.fleets.front();
    fleet.design = kColonyShipDesignId; fleet.ships = {{fleet.design, 3}};
    fleet.position = state.stars.front().position; fleet.destination.reset();
    fleet.waypointQueue.clear(); fleet.task = FleetTask::None;
    fleet.repeatOrders = false; fleet.targetFleet = 0;
    fleet.colonists = 500; fleet.minerals = {3, 2, 1};
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    assert(fleet_cargo_used(state, fleet) <= fleet_cargo_capacity(state, fleet));
    fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
    set_survey_level(state, 1, planet.star, SurveyLevel::DeepSurvey, state.turn);
    return state;
}
}
