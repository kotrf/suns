#include "suns/native_orbital.hpp"
#include "suns/campaign.hpp"
#include "suns/stations.hpp"
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
    case OrbitalStationHullType::Starbase: return 500'000;
    case OrbitalStationHullType::Fortress: return 2'000'000;
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
std::uint32_t native_orbital_output(const GameState& state, const Planet& planet)
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
NativeOrbitalFoundingKit native_orbital_founding_kit()
{
    const StationLoadout dock;
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
