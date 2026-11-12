#include "suns/native_orbital.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include "suns/orbital_bases.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace suns {
namespace {
ShipDesign starter_miner(PlayerId owner, ShipDesignId id)
{
    return {id, owner, "Orbital Miner", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RoboMiniMiner}};
}
StationLoadout starting_dock(std::optional<ShipComponentType> screen)
{
    StationLoadout fit;
    if (screen) fit.components.push_back(*screen);
    return fit;
}
NativeOrbitalStartBudget asset_bill(std::optional<ShipComponentType> screen)
{
    NativeOrbitalStartBudget bill;
    const auto miner = starter_miner(1, 1);
    const auto dock = starting_dock(screen);
    bill.radiationScreen = screen;
    const GameState prices;
    const auto receiver = production_item_mineral_cost(prices, {ProductionKind::PlanetaryRadar});
    const auto hull = station_mineral_cost(dock), ship = ship_design_mineral_cost(miner);
    bill.production = station_build_cost(dock) + ship_design_cost(miner) + production_cost(ProductionKind::PlanetaryRadar);
    bill.minerals = {hull.ironium+ship.ironium+receiver.ironium,
        hull.boranium+ship.boranium+receiver.boranium, hull.germanium+ship.germanium+receiver.germanium};
    return bill;
}
}
NativeOrbitalStartBudget native_orbital_start_budget(std::optional<ShipComponentType> screen)
{
    if (screen && !radiation_screen_protection(*screen)) throw std::invalid_argument("Not a radiation screen");
    auto budget = asset_bill(screen);
    budget.minerals.ironium += 100; budget.minerals.boranium += 100; budget.minerals.germanium += 100;
    return budget;
}
std::optional<NativeOrbitalStartReceipt> initialize_native_orbital_start(
    GameState& state, PlayerId owner, PlanetId home, const NativeOrbitalStartBudget& budget)
{
    const auto* player = find_player(state, owner);
    const auto* planet = find_planet(state, home);
    if (state.mediumObserver || state.turn != 1 || !owner || !native_orbital_race(state, owner)
        || !planet || !find_star(state, planet->star) || planet->owner || planet->population
        || planet->habitat.population || planet->nativeOrbit.station || planet->nativeOrbit.population
        || planet->engineering.kind != PlanetaryEngineeringKind::None || !planet->productionQueue.empty()
        || planet->minerals.ironium || planet->minerals.boranium || planet->minerals.germanium
        || !player->race.environmentBased || player->race.radiationImmune || player->race.basicRemoteMining
        || !player->history.empty() || !player->surveyKnowledge.empty()
        || find_orbital_station_at_planet(state, home) || orbital_base_at(state, home)) return {};
    if (std::any_of(state.planets.begin(), state.planets.end(), [=](const auto& p) { return p.owner == owner; })
        || std::any_of(state.fleets.begin(), state.fleets.end(), [=](const auto& f) { return f.owner == owner; })
        || std::any_of(state.orbitalStations.begin(), state.orbitalStations.end(), [=](const auto& s) { return s.owner == owner; })
        || std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [=](const auto& d) { return d.owner == owner; })) return {};
    const auto idAvailable = [](auto id) { return id && id < std::numeric_limits<decltype(id)>::max(); };
    if (!idAvailable(state.nextFleetId) || !idAvailable(state.nextShipDesignId) || !idAvailable(state.nextOrbitalStationId)
        || std::any_of(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == state.nextFleetId; })
        || find_ship_design(state, state.nextShipDesignId)
        || std::any_of(state.orbitalStations.begin(), state.orbitalStations.end(), [&](const auto& s) { return s.id == state.nextOrbitalStationId; })) return {};
    const auto miner = starter_miner(owner, state.nextShipDesignId);
    if (budget.radiationScreen && !radiation_screen_protection(*budget.radiationScreen)) return {};
    const auto fit = starting_dock(budget.radiationScreen);
    if (!ship_design_valid(miner) || !ship_design_available_to_player(state, owner, miner)
        || !station_loadout_available(state, owner, fit)) return {};
    const auto bill = asset_bill(budget.radiationScreen);
    const auto sufficient = [](double have, double need) { return std::isfinite(have) && have >= need; };
    if (!budget.population || budget.population > native_orbital_capacity(OrbitalStationHullType::OrbitalDock)
        || budget.production < bill.production || !sufficient(budget.minerals.ironium, bill.minerals.ironium)
        || !sufficient(budget.minerals.boranium, bill.minerals.boranium)
        || !sufficient(budget.minerals.germanium, bill.minerals.germanium)) return {};

    auto next = state;
    auto& colony = *std::find_if(next.planets.begin(), next.planets.end(), [=](const auto& p) { return p.id == home; });
    auto& star = *std::find_if(next.stars.begin(), next.stars.end(), [&](const auto& s) { return s.id == colony.star; });
    const auto stationId = next.nextOrbitalStationId++;
    colony.owner = owner; colony.population = 0; colony.industry = 0; colony.mines = 0;
    colony.advancedFactories = 0; colony.advancedMines = 0; colony.habitat = {};
    colony.nativeOrbit = {stationId, budget.population}; colony.homeworld = true;
    colony.radar = PlanetaryRadarLevel::Full; colony.observedHabitability.reset();
    colony.minerals = {budget.minerals.ironium-bill.minerals.ironium,
        budget.minerals.boranium-bill.minerals.boranium, budget.minerals.germanium-bill.minerals.germanium};
    star.stellarClass = StarClass::Yellow; star.variability = {};
    next.orbitalStations.push_back({stationId, owner, home, "Inhabited Orbital Dock",
        OrbitalStationHullType::OrbitalDock, {OrbitalStationModule::Shipyard, OrbitalStationModule::RefuelingDepot}, fit.components});
    // Reject a resource deadlock while all mutations are still in the private
    // candidate. Keep the declared positive initial gross-output requirement;
    // annual support is paid from the remaining real stock after startup.
    if (!native_orbital_output(next, colony)) return {};
    const auto radiation = native_orbital_radiation_status(next, colony);
    if (!radiation || radiation->annualLoss) return {};
    next.shipDesigns.push_back(miner); ++next.nextShipDesignId;
    Fleet fleet; fleet.id = next.nextFleetId++; fleet.owner = owner; fleet.design = miner.id;
    fleet.name = "Orbital Miner"; fleet.ships = {{miner.id, 1}}; fleet.position = star.position;
    fleet.warp = ship_design_cruise_warp(miner); fleet.fuel = ship_design_fuel_capacity(miner);
    fleet.task = FleetTask::RemoteMining;
    next.fleets.push_back(fleet);
    next.fleets.back().telemetry = confirmed_fleet_telemetry(next, next.fleets.back());
    set_survey_level(next, owner, colony.star, SurveyLevel::DeepSurvey, next.turn);
    auto& empire = *std::find_if(next.players.begin(), next.players.end(), [=](const auto& p) { return p.id == owner; });
    empire.history.push_back(empire_turn_statistics(next, owner));
    state = std::move(next);
    return NativeOrbitalStartReceipt{stationId, miner.id, fleet.id, bill.production, bill.minerals};
}
}
