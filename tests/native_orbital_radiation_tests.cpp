#include "native_orbital_fixture.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

using namespace suns;
namespace {
GameState started()
{
    auto state = test::empty_native_orbital_start();
    assert(initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget()));
    state.players.front().technology.researchActive = false;
    return state;
}
StationLoadout screened_dock()
{
    return {OrbitalStationHullType::OrbitalDock, {ShipComponentType::BulkRadiationScreen}, true, true};
}
void dose_thresholds_rounding_and_overflow()
{
    assert(orbital_radiation_casualties(25'000, 40, 40) == 0);
    assert(orbital_radiation_casualties(25'000, 39, 40) == 0);
    assert(orbital_radiation_casualties(25'000, 45, 40) == 250);
    assert(orbital_radiation_casualties(1, 41, 40) == 1);
    assert(orbital_radiation_casualties(0, 100, 0) == 0);
    assert(orbital_radiation_casualties(501, 41, 40) == 2);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    assert(orbital_radiation_casualties(maximum, 255, 0) == maximum / 5);
    assert(radiation_screen_protection(ShipComponentType::BulkRadiationScreen) == 20);
    assert(radiation_screen_protection(ShipComponentType::CompositeRadiationScreen) == 40);
    assert(radiation_screen_protection(ShipComponentType::AdvancedRadiationScreen) == 60);
    OrbitalStation station;
    station.components = {ShipComponentType::BulkRadiationScreen, ShipComponentType::AdvancedRadiationScreen};
    assert(station_radiation_screen(station) == 60); // Never additive, even on malformed legacy fits.
}
void legal_paid_screens_and_early_access()
{
    auto state = started();
    const auto levels = state.players.front().technology.levels;
    auto fit = screened_dock();
    assert(station_loadout_valid(fit) && station_loadout_available(state, 1, fit));
    assert(station_build_cost(fit) == 74);
    const auto price = station_mineral_cost(fit);
    assert(price.ironium == 52 && price.boranium == 16 && price.germanium == 18);
    fit.components.push_back(ShipComponentType::BulkRadiationScreen);
    assert(!station_loadout_valid(fit));
    fit.components = {ShipComponentType::CompositeRadiationScreen};
    assert(station_loadout_valid(fit) && !station_loadout_available(state, 1, fit));
    state.players.front().technology.levels[std::size_t(ResearchField::Construction)] = 18;
    state.players.front().technology.levels[std::size_t(ResearchField::Biology)] = 10;
    assert(station_loadout_available(state, 1, fit));
    fit.components = {ShipComponentType::AdvancedRadiationScreen};
    assert(!station_loadout_available(state, 1, fit));
    state.players.front().technology.levels[std::size_t(ResearchField::Energy)] = 16;
    state.players.front().technology.levels[std::size_t(ResearchField::Construction)] = 22;
    state.players.front().technology.levels[std::size_t(ResearchField::Biology)] = 14;
    assert(station_loadout_available(state, 1, fit));
    state.players.front().race.primaryTrait = PrimaryRaceTrait::Generalist;
    assert(component_available_to_player(state, 1, fit.components.front()));
    assert(!station_loadout_available(state, 1, fit)); // Ordinary yard screens remain ship equipment.
    state.players.front().technology.levels = levels;
    assert(!component_available_to_player(state, 1, ShipComponentType::BulkRadiationScreen));
}
void starter_is_safe_and_never_gifts_protection()
{
    auto state = started();
    const auto status = native_orbital_radiation_status(state, state.planets.front());
    assert(status && status->background == 45 && status->protection == 20);
    assert(status->exposure == 25 && status->tolerance == 40 && !status->annualLoss);
    assert(state.players.front().technology.levels == test::empty_native_orbital_start().players.front().technology.levels);
    auto unsafe = test::empty_native_orbital_start();
    const auto bare = native_orbital_start_budget(std::nullopt);
    const auto nextId = unsafe.nextOrbitalStationId;
    assert(!initialize_native_orbital_start(unsafe, 1, unsafe.planets.front().id, bare));
    assert(unsafe.orbitalStations.empty() && unsafe.fleets.empty() && !unsafe.planets.front().owner);
    assert(unsafe.nextOrbitalStationId == nextId);
    unsafe.players.front().race.habitableRadiation.maximum = 45;
    assert(initialize_native_orbital_start(unsafe, 1, unsafe.planets.front().id, bare));
    assert(!station_radiation_screen(unsafe.orbitalStations.front()));
    auto shortBudget = native_orbital_start_budget(); shortBudget.minerals.ironium = 111;
    auto shortState = test::empty_native_orbital_start();
    assert(!initialize_native_orbital_start(shortState, 1, shortState.planets.front().id, shortBudget));
    assert(shortState.orbitalStations.empty() && shortState.shipDesigns.empty());
    bool rejected = false;
    try { (void)native_orbital_start_budget(ShipComponentType::QuickJump5); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
void orbital_dose_ignores_surface_and_variability()
{
    auto state = started();
    auto& star = state.stars.front();
    auto& planet = state.planets.front();
    star.stellarClass = StarClass::BlueWhite;
    const auto original = native_orbital_radiation_status(state, planet);
    assert(original && original->exposure == 70 && original->annualLoss == 1500 && original->safety == -100);
    for (unsigned value : {0, 50, 100}) {
        planet.environment = {std::uint8_t(value), std::uint8_t(value), std::uint8_t(value)};
        planet.engineering.kind = PlanetaryEngineeringKind::AdaptiveMagnetosphere;
        planet.engineering.operational = true;
        state.players.front().technology.levels[std::size_t(ResearchField::Biology)] = 26;
        star.variability = {4, 80, 1};
        ++state.turn;
        const auto changed = native_orbital_radiation_status(state, planet);
        assert(changed && changed->annualLoss == original->annualLoss && changed->exposure == original->exposure);
        assert(player_planet_habitability(state, 1, planet, state.turn) == original->safety);
    }
    state.players.front().race.habitableRadiation.minimum = 40;
    star.stellarClass = StarClass::Red;
    assert(!native_orbital_radiation_status(state, planet)->annualLoss); // Maximum tolerated dose, no minimum-dose need.
    state.orbitalStations.front().components.clear();
    star.stellarClass = StarClass::BlueWhite;
    assert(native_orbital_radiation_status(state, planet)->annualLoss == 2500);
}
void real_refit_only_protects_on_completion()
{
    auto state = started();
    state.orbitalStations.front().components.clear();
    set_survey_level(state, 1, state.planets.front().star, SurveyLevel::DeepSurvey, state.turn);
    const auto station = state.orbitalStations.front().id;
    const StationConstruction refit{screened_dock(), station};
    const auto first = TurnProcessor{}.process_with_events(state, {{1, {QueueStationOrder{state.planets.front().id, refit}}}});
    state = first.state;
    assert(state.planets.front().nativeOrbit.population == 24'750);
    assert(!station_radiation_screen(state.orbitalStations.front()));
    assert(state.planets.front().productionQueue.front().remainingCost == 28);
    const auto firstSupport = state.planets.front().nativeOrbit.mineralUnitsPaid;
    const auto loss = std::find_if(first.events.begin(), first.events.end(), [](const auto& e) {
        return e.kind == GameEventKind::NativeOrbitalRadiationLoss;
    });
    assert(loss != first.events.end() && loss->deliveredColonists == 250 && loss->planet == state.planets.front().id);
    state = TurnProcessor{}.process(state, {});
    assert(state.planets.front().productionQueue.empty() && state.orbitalStations.front().id == station);
    assert(station_radiation_screen(state.orbitalStations.front()) == 20);
    assert(state.planets.front().nativeOrbit.population - state.planets.front().nativeOrbit.populationGrown == 24'750);
    assert(state.planets.front().nativeOrbit.populationGrown > 0);
    const auto support = firstSupport + state.planets.front().nativeOrbit.mineralUnitsPaid;
    assert(state.planets.front().minerals.ironium == 56-support); // Mining minus paid fit and life support.
    assert(state.planets.front().minerals.boranium == 88-support && state.planets.front().minerals.germanium == 84-support);
    assert(TurnProcessor{}.process(state, {}).planets.front().nativeOrbit.population > state.planets.front().nativeOrbit.population);
}
void evacuation_and_empty_surviving_station()
{
    auto state = test::native_orbital_fixture();
    state.stars.front().stellarClass = StarClass::BlueWhite;
    const auto fleet = state.fleets.front().id;
    const auto next = TurnProcessor{}.process_with_events(state, {{1, {
        TransferCargoOrder{{state.planets.front().id, 0, true}, {0, fleet}, 100, {}}}}});
    assert(next.state.fleets.front().colonists == 600);
    assert(next.state.planets.front().nativeOrbit.population == 8910); // 9900 residents minus 10%.
    state.planets.front().nativeOrbit.population = 1;
    const auto exhausted = TurnProcessor{}.process(state, {});
    assert(!exhausted.planets.front().nativeOrbit.population && exhausted.planets.front().owner == 1);
    assert(native_orbital_station(exhausted, exhausted.planets.front()));
    assert(native_orbital_station(TurnProcessor{}.process(exhausted, {}), exhausted.planets.front()));
    destroy_native_orbital_population(state, state.orbitalStations.front().id);
    const auto destroyed = TurnProcessor{}.process_with_events(state, {});
    assert(std::none_of(destroyed.events.begin(), destroyed.events.end(), [](const auto& e) {
        return e.kind == GameEventKind::NativeOrbitalRadiationLoss;
    }));
}
void known_safety_uses_received_fit()
{
    auto state = started();
    const auto planet = state.planets.front().id;
    state.stars.front().stellarClass = StarClass::White;
    state.orbitalStations.front().components.clear();
    set_survey_level(state, 1, state.planets.front().star, SurveyLevel::DeepSurvey, state.turn);
    const auto old = known_planet_habitability(state, 1, planet);
    assert(old && *old == -100);
    state.orbitalStations.front().components = {ShipComponentType::AdvancedRadiationScreen};
    assert(player_planet_habitability(state, 1, state.planets.front(), state.turn) == 100);
    assert(known_planet_habitability(state, 1, planet) == old);
    set_survey_level(state, 1, state.planets.front().star, SurveyLevel::DeepSurvey, state.turn + 1);
    assert(known_planet_habitability(state, 1, planet) == 100);
}
void founding_pays_only_for_its_existing_unshielded_assets()
{
    auto state = test::native_orbital_founding_fixture();
    state.stars[1].stellarClass = StarClass::Yellow;
    set_survey_level(state, 1, state.planets[1].star, SurveyLevel::DeepSurvey, state.turn);
    assert(known_planet_habitability(state, 1, state.planets[1].id) == -25);
    const auto next = TurnProcessor{}.process_with_events(state, {{1, {
        ColonizePlanetOrder{state.fleets.back().id, state.planets[1].id}}}});
    const auto& colony = next.state.planets[1];
    const auto* station = native_orbital_station(next.state, colony);
    assert(station && station->components.empty() && colony.nativeOrbit.population == 990);
    assert(!colony.population && colony.owner == 1);
    assert(std::any_of(next.events.begin(), next.events.end(), [&](const auto& event) {
        return event.kind == GameEventKind::NativeOrbitalRadiationLoss
            && event.planet == colony.id && event.deliveredColonists == 10;
    }));
}
}
int main()
{
    dose_thresholds_rounding_and_overflow();
    legal_paid_screens_and_early_access();
    starter_is_safe_and_never_gifts_protection();
    orbital_dose_ignores_surface_and_variability();
    real_refit_only_protects_on_completion();
    evacuation_and_empty_surviving_station();
    known_safety_uses_received_fit();
    founding_pays_only_for_its_existing_unshielded_assets();
}
