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
    fleet.telemetry = {};
    fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
    set_survey_level(state, 1, planet.star, SurveyLevel::DeepSurvey, state.turn);
    return state;
}
inline GameState asynchronous_native_unload_fixture()
{
    auto state = native_orbital_fixture();
    state.planets.front().minerals = {100, 100, 100};
    auto remote = state.planets.front();
    remote.id = state.planets[1].id; remote.star = state.stars[1].id;
    remote.homeworld = false; remote.radar = PlanetaryRadarLevel::None;
    auto station = state.orbitalStations.front();
    station.id = state.nextOrbitalStationId++; station.planet = remote.id;
    remote.nativeOrbit.station = station.id;
    state.orbitalStations.push_back(station);
    state.planets[1] = remote;
    state.stars[1].position = {170, 0};
    auto& fleet = state.fleets.front();
    fleet.position = state.stars[1].position; fleet.destination = fleet.position; fleet.warp = 5;
    fleet.arrivalAction = {FleetArrivalActionKind::UnloadAll, 0, FleetCargoKind::All};
    fleet.waypointQueue = {{{145, 0}, 5, {}}};
    fleet.telemetry.position = fleet.position; fleet.telemetry.destination = fleet.destination;
    fleet.telemetry.warp = fleet.warp; fleet.telemetry.arrivalAction = fleet.arrivalAction;
    fleet.telemetry.waypointQueue = fleet.waypointQueue;
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    set_survey_level(state, 1, remote.star, SurveyLevel::DeepSurvey, state.turn);
    state.players.front().history.clear();
    record_empire_turn_statistics(state);
    assert(communication_delay_turns(state, 1, {170, 0}) == 2);
    assert(communication_delay_turns(state, 1, {145, 0}) == 0);
    return state;
}
}
