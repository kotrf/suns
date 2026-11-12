#pragma once
#include "suns/campaign.hpp"
#include "suns/native_orbital.hpp"
#include "suns/communications.hpp"
#include <cassert>

namespace suns::test {
// Prepared host input for the startup kernel, not a playable native campaign.
inline GameState empty_native_orbital_start()
{
    auto state = generate_game(GalaxyConfig{});
    state.wormholeRules.spawnChancePerTurn = 0;
    state.players.clear(); state.fleets.clear(); state.shipDesigns.clear(); state.orbitalStations.clear();
    state.nextFleetId = 1; state.nextShipDesignId = 1; state.nextOrbitalStationId = 1;
    for (auto& planet : state.planets) {
        const auto id = planet.id, star = planet.star;
        const auto name = planet.name; const auto environment = planet.environment;
        planet = {}; planet.id = id; planet.star = star; planet.name = name; planet.environment = environment;
    }
    Player player{1, "Orbital civilization"};
    player.race = race_preset(RacePreset::Terran);
    player.race.primaryTrait = PrimaryRaceTrait::HabitatCivilization;
    state.players.push_back(player);
    state.planets.front().observedConcentration = MineralCargo{100, 50, 25};
    return state;
}
inline GameState native_orbital_fixture()
{
    auto state = make_demo_game();
    state.wormholeRules.spawnChancePerTurn = 0;
    state.stars.front().stellarClass = StarClass::Orange; // Safe legacy, unshielded station.
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
inline GameState native_orbital_founding_fixture()
{
    auto state = empty_native_orbital_start();
    assert(initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget()));
    state.players.front().technology.researchActive = false;
    state.planets.front().nativeOrbit.population -= 1000;
    state.shipDesigns.push_back({90, 1, "Orbital Settler", ShipHullType::ColonyShip,
        {ShipComponentType::QuickJump5, ShipComponentType::OrbitalConstructionModule}});
    state.nextShipDesignId = 91;
    state.stars[1].position = {25, 0};
    state.stars[1].stellarClass = StarClass::Orange; // Safe orbit for the existing unshielded founding kit.
    Fleet fleet; fleet.id = state.nextFleetId++; fleet.owner = 1; fleet.design = 90;
    fleet.name = "Orbital Settler"; fleet.ships = {{90, 1}}; fleet.position = state.stars[1].position;
    fleet.warp = 5; fleet.colonists = 1000; fleet.minerals = {3, 2, 1};
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    state.fleets.push_back(fleet);
    state.fleets.back().telemetry = confirmed_fleet_telemetry(state, state.fleets.back());
    state.players.front().surveyKnowledge.clear(); state.players.front().surveyedStars.clear();
    set_survey_level(state, 1, state.planets[1].star, SurveyLevel::GeologicalSurvey, state.turn);
    set_survey_level(state, 1, state.planets.front().star, SurveyLevel::DeepSurvey, state.turn);
    state.players.front().history.clear(); record_empire_turn_statistics(state);
    return state;
}
inline GameState native_miner_waiting_for_ironium_fixture()
{
    auto state = empty_native_orbital_start();
    auto budget = native_orbital_start_budget(); budget.minerals.ironium -= 45;
    const auto started = initialize_native_orbital_start(state, 1, state.planets.front().id, budget);
    assert(started);
    state.players.front().technology.researchActive = false;
    const auto home = state.planets.front().id;
    // Prepared transport assets carry real cargo; subsequent changes use orders.
    const auto design = state.nextShipDesignId++;
    state.shipDesigns.push_back({design, 1, "Supply transport", ShipHullType::SmallFreighter,
        {ShipComponentType::QuickJump5}});
    assert(ship_design_valid(state.shipDesigns.back()));
    Fleet transport; transport.id = state.nextFleetId++; transport.owner = 1; transport.design = design;
    transport.name = "Supply transport"; transport.ships = {{design, 2}};
    transport.position = state.stars.front().position; transport.minerals = {20, 0, 0};
    transport.fuel = fleet_fuel_capacity(state, transport);
    transport.telemetry = confirmed_fleet_telemetry(state, transport);
    state.fleets.push_back(transport);
    const PlayerOrders build{1, {SetRemoteMiningOrder{started->minerFleet, false},
        QueueShipDesignOrder{home, started->minerDesign}}};
    for (unsigned year = 0; year < 4; ++year)
        state = TurnProcessor{}.process(state, year ? std::vector<PlayerOrders>{} : std::vector<PlayerOrders>{build});
    assert(state.turn == 5 && state.planets.front().productionQueue.size() == 1);
    assert(state.planets.front().productionQueue.front().remainingCost == 0 && state.planets.front().minerals.ironium == 42);
    return state;
}
inline GameState native_orbital_miner_fixture()
{
    auto state = native_orbital_fixture();
    auto& player = state.players.front();
    player.technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 2;
    player.technology.levels[static_cast<std::size_t>(ResearchField::Electronics)] = 1;
    state.shipDesigns.push_back({90, 1, "Orbital miner", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RoboMiniMiner}});
    state.nextShipDesignId = 91;
    auto& fleet = state.fleets.front();
    fleet.design = 90; fleet.ships = {{90, 2}}; fleet.colonists = 0; fleet.minerals = {};
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    fleet.telemetry = {}; fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
    state.planets.front().minerals = {100, 100, 100};
    state.planets.front().observedConcentration = MineralCargo{100, 50, 25};
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    player.history.clear(); record_empire_turn_statistics(state);
    assert(fleet_can_remote_mine(state, fleet));
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
    state.stars[1].stellarClass = StarClass::Orange; // Unshielded received-station fixture.
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

inline GameState native_capital_evacuation_fixture()
{
    auto state = asynchronous_native_unload_fixture();
    state.planets.front().nativeOrbit.population = 1'000;
    state.planets.front().minerals = {100, 100, 100};
    auto& fleet = state.fleets.front();
    fleet.position = state.stars.front().position;
    fleet.destination.reset(); fleet.arrivalAction = {}; fleet.waypointQueue.clear();
    fleet.colonists = 0; fleet.minerals = {3, 0, 0};
    fleet.telemetry = {}; fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    state.players.front().history.clear(); record_empire_turn_statistics(state);
    assert(communication_delay_turns(state, 1, fleet.position) == 0);
    return state;
}
}
