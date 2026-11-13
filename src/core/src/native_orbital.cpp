#include "suns/native_orbital.hpp"
#include "suns/campaign.hpp"
#include "suns/stations.hpp"
#include "suns/orbital_bases.hpp"
#include <algorithm>

namespace suns {
bool native_orbital_race(const GameState& state, PlayerId owner)
{
    const auto* player = find_player(state, owner);
    return player && player->race.primaryTrait == PrimaryRaceTrait::HabitatCivilization;
}
std::uint64_t native_orbital_capacity(OrbitalStationHullType hull)
{
    // Prototype housing; independent habitats keep their own fixed capacity.
    switch (hull) {
    case OrbitalStationHullType::OrbitalDock: return 100'000;
    case OrbitalStationHullType::Starbase:
    case OrbitalStationHullType::NativeCitadel: return 500'000;
    case OrbitalStationHullType::Fortress:
    case OrbitalStationHullType::NativeBastion: return 2'000'000;
    }
    return 0;
}
const OrbitalStation* native_orbital_station(const GameState& state, const Planet& planet)
{
    if (!native_orbital_race(state, planet.owner) || !planet.nativeOrbit.station) return nullptr;
    const auto* station = find_orbital_station_at_planet(state, planet.id);
    return station && station->id == planet.nativeOrbit.station && station->owner == planet.owner
        && station->damagePercent >= 0 && station->damagePercent < 100 ? station : nullptr;
}
std::uint32_t native_orbital_potential_output(const GameState& state, const Planet& planet)
{
    const auto* station = native_orbital_station(state, planet);
    if (!station) return 0;
    const auto residents = std::min(planet.nativeOrbit.population, native_orbital_capacity(station->hull));
    const auto energy = std::min(technology_level(state, planet.owner, ResearchField::Energy), kMaximumResearchLevel);
    const auto radicand = residents * (std::uint64_t(energy) + 1) / 10;
    // Integer square root keeps the same result across host/PBEM platforms.
    std::uint64_t low = 0, high = radicand + 1;
    while (low + 1 < high) {
        const auto mid = low + (high-low)/2;
        if (mid <= radicand/mid) low = mid;
        else high = mid;
    }
    return static_cast<std::uint32_t>(low);
}
std::uint32_t native_orbital_output(const GameState& state, const Planet& planet)
{
    if (planet.nativeOrbit.servicedTurn && planet.nativeOrbit.service != HabitatService::Normal) return 0;
    return native_orbital_potential_output(state, planet);
}
StationLoadout native_orbital_founding_loadout()
{
    // Keep the existing prepaid kit's assets, including kits in old saves.
    // Radiation protection is installed through an ordinary paid station refit.
    return {};
}
std::optional<NativeOrbitalRadiationStatus> native_orbital_radiation_status(
    const GameState& state, PlayerId owner, StarId id, std::uint64_t residents, const StationLoadout& fit)
{
    const auto* player = find_player(state, owner);
    const auto* star = find_star(state, id);
    if (!native_orbital_race(state, owner) || !star || !station_loadout_valid(fit)) return {};
    OrbitalStation draft; draft.components = fit.components;
    NativeOrbitalRadiationStatus result;
    result.background = orbital_background(*star);
    result.protection = station_radiation_screen(draft);
    result.exposure = static_cast<std::uint8_t>(std::max(0, int(result.background) - int(result.protection)));
    // Orbital maximum dose, not the surface comfort-range midpoint/minimum.
    result.tolerance = player->race.habitableRadiation.maximum;
    result.annualLoss = orbital_radiation_casualties(residents, result.exposure, result.tolerance);
    const auto excess = std::max(0, int(result.exposure) - int(result.tolerance));
    result.safety = excess ? -std::min(100, excess * 5) : 100;
    return result;
}
std::optional<NativeOrbitalRadiationStatus> native_orbital_radiation_status(const GameState& state, const Planet& planet)
{
    const auto* station = native_orbital_station(state, planet);
    return station ? native_orbital_radiation_status(state, planet.owner, planet.star,
        planet.nativeOrbit.population, station_loadout(*station)) : std::nullopt;
}
std::int32_t native_orbital_habitability(const GameState& state, PlayerId owner, const Planet& planet)
{
    const auto* station = planet.owner == owner ? native_orbital_station(state, planet) : nullptr;
    const auto status = native_orbital_radiation_status(state, owner, planet.star, 0,
        station ? station_loadout(*station) : native_orbital_founding_loadout());
    return status ? status->safety : 0;
}
std::optional<std::int32_t> known_native_orbital_habitability(const GameState& state, PlayerId owner, PlanetId id)
{
    const auto* planet = find_planet(state, id);
    if (!planet || survey_level(state, owner, planet->star) < SurveyLevel::BasicScan) return {};
    const auto* station = known_orbital_station(state, owner, id);
    const auto status = native_orbital_radiation_status(state, owner, planet->star, 0,
        station && station->owner == owner ? station_loadout(*station) : native_orbital_founding_loadout());
    return status ? std::optional<std::int32_t>{status->safety} : std::nullopt;
}
NativeOrbitalFoundingKit native_orbital_founding_kit()
{
    const auto dock = native_orbital_founding_loadout();
    const auto station = station_mineral_cost(dock);
    const auto receiver = production_item_mineral_cost(GameState{}, {ProductionKind::PlanetaryRadar});
    const MineralCargo supplies{10, 10, 10};
    return {station_build_cost(dock) + production_cost(ProductionKind::PlanetaryRadar),
        {station.ironium + receiver.ironium + supplies.ironium,
         station.boranium + receiver.boranium + supplies.boranium,
         station.germanium + receiver.germanium + supplies.germanium}, supplies};
}
bool ship_design_can_found_native_orbit(const ShipDesign& design)
{
    return std::find(design.components.begin(), design.components.end(),
        ShipComponentType::OrbitalConstructionModule) != design.components.end();
}
bool fleet_can_found_native_orbit(const GameState& state, const Fleet& fleet)
{
    if (!native_orbital_race(state, fleet.owner) || fleet.outpost != OutpostKind::None) return false;
    const auto stacks = fleet_ship_stacks(fleet);
    return std::any_of(stacks.begin(), stacks.end(), [&](const auto& stack) {
        const auto* design = find_ship_design(state, stack.design);
        return design && ship_design_can_found_native_orbit(*design)
            && ship_design_valid(*design) && ship_design_available_to_player(state, fleet.owner, *design);
    });
}
std::uint64_t unload_native_orbital_colonists(GameState& state, Planet& planet, Fleet& fleet)
{
    const auto* station = native_orbital_station(state, planet);
    const auto* star = find_star(state, planet.star);
    if (!station || !star || fleet.owner != planet.owner || !same_position(star->position, fleet.position)) return 0;
    const auto capacity = native_orbital_capacity(station->hull);
    if (planet.nativeOrbit.population >= capacity) return 0;
    const auto amount = std::min(fleet.colonists, capacity - planet.nativeOrbit.population);
    fleet.colonists -= amount;
    planet.nativeOrbit.population += amount;
    return amount;
}
void destroy_native_orbital_population(GameState& state, OrbitalStationId station)
{
    if (!station) return;
    for (auto& planet : state.planets) {
        if (planet.nativeOrbit.station != station || !native_orbital_race(state, planet.owner)) continue;
        planet.nativeOrbit = {};
        planet.owner = 0;
        planet.homeworld = false;
        planet.radar = PlanetaryRadarLevel::None;
        planet.productionQueue.clear();
        planet.productionWaitingForMinerals = false;
        planet.productionWaitingForShipyard = false;
    }
}
}
