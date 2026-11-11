#include "suns/native_orbital_support.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cmath>

namespace suns {
namespace {
std::uint16_t affordable_units(const MineralCargo& stock, std::uint16_t limit)
{
    const auto available = std::min({stock.ironium, stock.boranium, stock.germanium});
    if (!std::isfinite(available) || available <= 0) return 0;
    return static_cast<std::uint16_t>(std::min<double>(limit, std::floor(available)));
}
void spend_units(MineralCargo& stock, std::uint16_t units)
{
    stock.ironium -= units; stock.boranium -= units; stock.germanium -= units;
}
std::uint64_t births(std::uint64_t population, std::uint64_t eligible,
    std::uint64_t capacity, std::uint8_t rate)
{
    if (!population || population >= capacity || !eligible) return 0;
    eligible = std::min(population, eligible);
    auto growth = std::max<std::uint64_t>(1, (eligible * std::clamp<int>(rate, 1, 20) + 50) / 100);
    if (population * 4 > capacity) {
        const auto headroom = capacity - population;
        // Stars! crowding above 25%. Capacity <= 2M and rate <= 20% keep the
        // numerator below 3.8e18; integer results agree across Windows/Linux.
        growth = std::max<std::uint64_t>(1, growth * 16 * headroom * headroom / (9 * capacity * capacity));
    }
    return std::min(growth, capacity - population);
}
}

NativeOrbitalSupportQuote native_orbital_support_quote(const GameState& state, const Planet& planet)
{
    NativeOrbitalSupportQuote result;
    const auto* station = native_orbital_station(state, planet);
    if (!station) return result;
    const auto& residents = planet.nativeOrbit;
    result.reserveAfter = residents.reserveUnits;
    const auto population = std::min(residents.population, native_orbital_capacity(station->hull));
    if (!population) return result;
    result.requiredUnits = static_cast<std::uint16_t>((population + kNativeSupportPopulationPerUnit - 1) / kNativeSupportPopulationPerUnit);
    const auto potential = native_orbital_potential_output(state, planet);
    auto stock = planet.minerals;
    result.directUnits = affordable_units(stock, result.requiredUnits);
    spend_units(stock, result.directUnits);
    result.mineralUnitsPaid = result.directUnits;
    // Tiny colonies may spend all output on upkeep and still grow. Supplies
    // remain paid; zero/one-work starts must not be permanently locked.
    result.productionPaid = std::min(potential, kNativeSupportWorkPerUnit * result.directUnits);
    if (result.directUnits == result.requiredUnits) {
        result.service = HabitatService::Normal;
        const auto target = kNativeSupportReserveBills * result.requiredUnits;
        if (result.reserveAfter < target) {
            const auto allowance = static_cast<std::uint16_t>(std::min<std::uint32_t>(
                result.requiredUnits, (potential - result.productionPaid) / kNativeSupportWorkPerUnit));
            result.reserveAdded = affordable_units(stock, std::min<std::uint16_t>(allowance, target - result.reserveAfter));
            result.reserveAfter += result.reserveAdded;
            result.mineralUnitsPaid += result.reserveAdded;
            result.productionPaid += kNativeSupportWorkPerUnit * result.reserveAdded;
        }
        result.output = potential - result.productionPaid;
    } else {
        result.reserveUsed = std::min<std::uint16_t>(result.reserveAfter, result.requiredUnits - result.directUnits);
        result.reserveAfter -= result.reserveUsed;
        const auto covered = std::uint64_t(result.directUnits + result.reserveUsed) * kNativeSupportPopulationPerUnit;
        if (covered >= population) result.service = HabitatService::Reserve;
        else {
            result.service = HabitatService::Failing;
            result.populationLost = (population - covered + 9) / 10;
        }
    }
    return result;
}

std::string native_orbital_support_name(HabitatService service)
{
    switch (service) {
    case HabitatService::Empty: return "Empty / not serviced yet";
    case HabitatService::Normal: return "Normal service affordable";
    case HabitatService::Reserve: return "Would use emergency kits; no growth or production";
    case HabitatService::Failing: return "Supply shortage; unsupported residents at risk";
    }
    return "Unknown";
}

std::uint32_t service_native_orbit(const GameState& state, Planet& planet)
{
    if (!native_orbital_station(state, planet)) return 0;
    auto& residents = planet.nativeOrbit;
    const auto year = state.turn + 1;
    if (residents.servicedTurn == year) return residents.productionPaid;
    const auto quote = native_orbital_support_quote(state, planet);
    spend_units(planet.minerals, quote.mineralUnitsPaid);
    residents.servicedTurn = year;
    residents.service = quote.service;
    residents.reserveUnits = quote.reserveAfter;
    residents.productionPaid = quote.productionPaid;
    residents.productionAvailable = quote.output;
    residents.mineralUnitsPaid = quote.mineralUnitsPaid;
    residents.populationLost = quote.populationLost;
    residents.population -= quote.populationLost;
    residents.growthEligiblePopulation = quote.service == HabitatService::Normal ? residents.population : 0;
    residents.growthAppliedTurn = 0;
    residents.populationGrown = 0;
    return residents.productionPaid;
}

void grow_native_orbit(const GameState& state, Planet& planet)
{
    const auto* station = native_orbital_station(state, planet);
    if (!station) return;
    auto& residents = planet.nativeOrbit;
    const auto year = state.turn + 1;
    if (residents.servicedTurn != year || residents.growthAppliedTurn == year) return;
    residents.growthAppliedTurn = year;
    const auto dose = native_orbital_radiation_status(state, planet);
    const auto* owner = find_player(state, planet.owner);
    if (residents.service != HabitatService::Normal || !dose || dose->safety < 100 || !owner) return;
    residents.populationGrown = births(residents.population, residents.growthEligiblePopulation,
        native_orbital_capacity(station->hull), owner->race.populationGrowthRate);
    residents.population += residents.populationGrown;
}

std::int64_t projected_native_population_change(const GameState& state, const Planet& planet)
{
    const auto* station = native_orbital_station(state, planet);
    const auto* owner = find_player(state, planet.owner);
    if (!station || !owner) return 0;
    const auto quote = native_orbital_support_quote(state, planet);
    const auto supported = planet.nativeOrbit.population - quote.populationLost;
    const auto dose = native_orbital_radiation_status(state, planet.owner, planet.star, supported, station_loadout(*station));
    if (!dose) return 0;
    const auto afterRadiation = supported - dose->annualLoss;
    const auto growth = quote.service == HabitatService::Normal && dose->safety == 100
        ? births(afterRadiation, afterRadiation, native_orbital_capacity(station->hull), owner->race.populationGrowthRate) : 0;
    return std::int64_t(growth) - std::int64_t(quote.populationLost) - std::int64_t(dose->annualLoss);
}

void publish_native_support_reports(GameState& state, const GameState& previous)
{
    for (const auto& planet : state.planets) {
        if (!native_orbital_station(state, planet) || planet.nativeOrbit.servicedTurn != state.turn + 1) continue;
        const auto& residents = planet.nativeOrbit;
        // A final owner packet captures this year's paid stocks and births.
        // It uses the ordinary transmitter/delay path, replacing pre-production
        // telemetry only on receipt, including at the capital.
        if (const auto* star = find_star(state, planet.star))
            queue_survey_report(state, planet.owner, planet.star, 0, star->position,
                state.turn + 1, SurveyLevel::GeologicalSurvey, true);
        const auto* old = find_planet(previous, planet.id);
        if (!residents.populationLost && old && old->nativeOrbit.station == residents.station
            && old->nativeOrbit.service == residents.service && residents.service != HabitatService::Reserve) continue;
        if (residents.service == HabitatService::Empty) continue;
        const auto kind = residents.service == HabitatService::Normal ? PlayerReportKind::NativeOrbitalSupportRestored
            : residents.service == HabitatService::Reserve ? PlayerReportKind::NativeOrbitalReserveUsed : PlayerReportKind::NativeOrbitalSupportFailure;
        if (const auto* star = find_star(state, planet.star))
            queue_player_report(state, planet.owner, kind, star->position, state.turn + 1, planet.star, planet.id, 0, 0,
                ProductionKind::OrbitalStation, residents.reserveUnits, ResearchField::Electronics, 0,
                {double(residents.mineralUnitsPaid), double(residents.mineralUnitsPaid), double(residents.mineralUnitsPaid)}, residents.populationLost);
    }
}
}
