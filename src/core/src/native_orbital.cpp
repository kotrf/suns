#include "suns/native_orbital.hpp"
#include "suns/campaign.hpp"
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
