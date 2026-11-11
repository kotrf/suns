#include "native_orbital_fixture.hpp"
#include "suns/native_orbital_support.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/player_knowledge.hpp"
#include <cassert>
using namespace suns;

namespace {
GameState capital()
{
    auto state = test::empty_native_orbital_start();
    state.stars.front().stellarClass = StarClass::Yellow;
    assert(initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget()));
    state.players.front().technology.researchActive = false;
    state.players.front().technology.researchAllocationPercent = 0;
    state.players.front().race.populationGrowthRate = 10;
    return state;
}
void paid_service_and_growth_are_once_per_year()
{
    auto state = capital(); auto& planet = state.planets.front();
    const auto q = native_orbital_support_quote(state, planet);
    assert(q.service == HabitatService::Normal && q.requiredUnits == 1 && q.reserveAdded == 1);
    assert(q.output == 46 && q.productionPaid == 4 && q.mineralUnitsPaid == 2);
    assert(projected_native_population_change(state, planet) == 2500);
    assert(service_native_orbit(state, planet) == 4);
    assert(planet.minerals.ironium == 98 && planet.nativeOrbit.reserveUnits == 1);
    const auto serviced = planet.nativeOrbit;
    assert(service_native_orbit(state, planet) == 4 && planet.nativeOrbit == serviced);
    assert(planet.minerals.germanium == 98);
    grow_native_orbit(state, planet);
    assert(planet.nativeOrbit.population == 27'500 && planet.nativeOrbit.populationGrown == 2500);
    grow_native_orbit(state, planet);
    assert(planet.nativeOrbit.population == 27'500);
    const auto result = TurnProcessor{}.process(capital(), {});
    assert(result.planets.front().nativeOrbit.population == 27'500);
    assert(result.planets.front().nativeOrbit.productionAvailable == 46);
    assert(result.players.front().history.back().population == 27'500);
    assert(result.players.front().history.back().productionOutput == 46);
}
void reserves_are_paid_units_and_only_uncovered_people_die()
{
    auto state = capital(); auto& p = state.planets.front();
    p.nativeOrbit.population = 50'000; p.nativeOrbit.reserveUnits = 1;
    p.minerals = {1, 1, 100};
    auto q = native_orbital_support_quote(state, p);
    assert(q.service == HabitatService::Reserve && q.directUnits == 1 && q.reserveUsed == 1 && !q.output && !q.populationLost);
    service_native_orbit(state, p); grow_native_orbit(state, p);
    assert(p.nativeOrbit.population == 50'000 && p.nativeOrbit.reserveUnits == 0 && p.minerals.germanium == 99);
    ++state.turn; p.minerals = {}; p.nativeOrbit.reserveUnits = 1;
    service_native_orbit(state, p); grow_native_orbit(state, p);
    assert(p.nativeOrbit.population == 47'500 && p.nativeOrbit.populationLost == 2500 && !native_orbital_output(state, p));
    ++state.turn; p.minerals = {100, 100, 100};
    service_native_orbit(state, p); grow_native_orbit(state, p);
    assert(p.nativeOrbit.service == HabitatService::Normal && p.nativeOrbit.population > 47'500 && p.nativeOrbit.productionAvailable > 0);
    auto immigrants = capital(); auto& crowded = immigrants.planets.front();
    crowded.nativeOrbit.population = 100'000; crowded.nativeOrbit.reserveUnits = 1; crowded.minerals = {};
    service_native_orbit(immigrants, crowded);
    assert(crowded.nativeOrbit.populationLost == 7500 && crowded.nativeOrbit.reserveUnits == 0);
}
void growth_uses_race_capacity_and_orbital_dose()
{
    auto state = capital(); auto& p = state.planets.front();
    state.players.front().race.populationGrowthRate = 20;
    assert(projected_native_population_change(state, p) == 5000);
    p.environment = {0, 100, 100}; p.habitability = -100;
    assert(projected_native_population_change(state, p) == 5000);
    p.nativeOrbit.population = 50'000;
    assert(projected_native_population_change(state, p) == 4444);
    p.nativeOrbit.population = 100'000;
    assert(projected_native_population_change(state, p) == 0);
    p.nativeOrbit.population = 1;
    service_native_orbit(state, p); grow_native_orbit(state, p);
    assert(p.nativeOrbit.population == 2 && p.nativeOrbit.productionPaid == 0 && p.nativeOrbit.mineralUnitsPaid == 1);
    auto large = capital(); large.orbitalStations.front().hull = OrbitalStationHullType::Fortress;
    large.players.front().race.populationGrowthRate = 20;
    large.planets.front().nativeOrbit.population = 500'001;
    large.planets.front().minerals = {1000, 1000, 1000};
    assert(projected_native_population_change(large, large.planets.front()) == 99'999);
    auto unsafe = capital(); unsafe.orbitalStations.front().components.clear();
    unsafe.players.front().race.habitableRadiation.maximum = 0;
    assert(projected_native_population_change(unsafe, unsafe.planets.front()) < 0);
}
void late_arrivals_do_not_expand_the_paid_growth_allowance()
{
    auto state = capital(); auto& p = state.planets.front();
    p.nativeOrbit.population = 1000;
    service_native_orbit(state, p);
    p.nativeOrbit.population += 24'000;
    grow_native_orbit(state, p);
    assert(p.nativeOrbit.populationGrown == 100 && p.nativeOrbit.population == 25'100);
}
void older_same_year_scans_cannot_split_a_completed_economic_packet()
{
    auto state = capital(); auto& p = state.planets.front();
    const auto position = state.stars.front().position;
    queue_survey_report(state, 1, p.star, 0, position, state.turn+1, SurveyLevel::GeologicalSurvey);
    const auto early = state.players.front().pendingSurveyReports.back();
    service_native_orbit(state, p); grow_native_orbit(state, p);
    queue_survey_report(state, 1, p.star, 0, position, state.turn+1, SurveyLevel::GeologicalSurvey, true);
    const auto final = state.players.front().pendingSurveyReports.back();
    state.players.front().pendingSurveyReports = {final, early};
    ++state.turn; (void)deliver_due_survey_reports(state);
    auto received = make_player_view(state, 1).state;
    assert(received.planets.front().nativeOrbit == p.nativeOrbit);
    assert(received.planets.front().minerals.ironium == p.minerals.ironium && p.minerals.ironium == 98);
    p.minerals.ironium += 10;
    queue_survey_report(state, 1, p.star, 0, position, state.turn+1, SurveyLevel::GeologicalSurvey);
    ++state.turn; (void)deliver_due_survey_reports(state);
    received = make_player_view(state, 1).state;
    assert(received.planets.front().minerals.ironium == 108);
}
}
int main()
{
    paid_service_and_growth_are_once_per_year();
    reserves_are_paid_units_and_only_uncovered_people_die();
    growth_uses_race_capacity_and_orbital_dose();
    late_arrivals_do_not_expand_the_paid_growth_allowance();
    older_same_year_scans_cannot_split_a_completed_economic_packet();
}
