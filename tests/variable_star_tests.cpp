#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>

namespace {

bool close(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

} // namespace

int main()
{
    auto state = suns::make_demo_game();
    auto& star = state.stars[1];
    auto& planet = state.planets[1];
    star.variability = {4, 20, 0};
    planet.habitability = 70;

    assert(suns::star_is_variable(star));
    assert(close(suns::stellar_luminosity(star, 1), 1.0));
    assert(close(suns::stellar_luminosity(star, 2), 1.2));
    assert(close(suns::stellar_luminosity(star, 3), 1.0));
    assert(close(suns::stellar_luminosity(star, 4), 0.8));
    assert(suns::current_planet_habitability(state, planet, 1) == 70);
    assert(suns::current_planet_habitability(state, planet, 2) == 90);
    assert(suns::current_planet_habitability(state, planet, 4) == 50);
    assert(suns::population_capacity(state, planet, 2) == 2250);
    assert(suns::population_capacity(state, planet, 4) == 1250);
    planet.owner = 1;
    planet.population = 500;
    assert(suns::projected_population_growth(state, planet, 2)
        > suns::projected_population_growth(state, planet, 4));
    planet.owner = 0;
    planet.population = 0;

    auto brightState = suns::make_demo_game();
    brightState.stars.front().variability = {4, 20, 0};
    brightState.planets.front().habitability = 70;
    brightState.planets.front().population = 500;
    brightState.turn = 2;
    auto dimState = brightState;
    dimState.turn = 4;
    const suns::TurnProcessor processor;
    const auto afterBrightYear = processor.process(brightState, {});
    const auto afterDimYear = processor.process(dimState, {});
    assert(afterBrightYear.planets.front().population
        > afterDimYear.planets.front().population);

    // A basic scan has no long-baseline stellar diagnosis. Arrival identifies
    // variability, and a full turn in orbit characterizes its cycle.
    suns::set_survey_level(state, 1, star.id, suns::SurveyLevel::BasicScan, 1);
    assert(!suns::known_stellar_variability(state, 1, star.id).has_value());
    suns::set_survey_level(state, 1, star.id, suns::SurveyLevel::OrbitalSurvey, 2);
    const auto suspected = suns::known_stellar_variability(state, 1, star.id);
    assert(suspected.has_value());
    assert(suspected->variable);
    assert(!suspected->characterized);

    // Without the period, confirmed habitability remains the last observation
    // rather than tracking authoritative future values invisibly.
    state.turn = 4;
    assert(suns::known_planet_habitability(state, 1, planet.id) == 90);
    suns::set_survey_level(state, 1, star.id, suns::SurveyLevel::GeologicalSurvey, 4);
    const auto characterized = suns::known_stellar_variability(state, 1, star.id);
    assert(characterized.has_value());
    assert(characterized->variable);
    assert(characterized->characterized);
    assert(characterized->periodTurns == 4);
    assert(characterized->amplitudePercent == 20);
    assert(suns::known_planet_habitability(state, 1, planet.id) == 50);

    // Generation is deterministic, preserves a stable home star, and makes
    // variability uncommon rather than exceptional in every tiny galaxy.
    std::size_t variableStars{};
    std::size_t generatedStars{};
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        const auto first = suns::generate_game({seed, 32, 900.0, 700.0, 45.0});
        const auto repeat = suns::generate_game({seed, 32, 900.0, 700.0, 45.0});
        assert(!suns::star_is_variable(first.stars.front()));
        for (std::size_t index = 1; index < first.stars.size(); ++index) {
            const auto& generated = first.stars[index];
            const auto& repeated = repeat.stars[index];
            assert(generated.variability.periodTurns == repeated.variability.periodTurns);
            assert(generated.variability.amplitudePercent == repeated.variability.amplitudePercent);
            assert(generated.variability.phaseOffset == repeated.variability.phaseOffset);
            ++generatedStars;
            if (!suns::star_is_variable(generated)) continue;
            ++variableStars;
            assert(generated.variability.periodTurns >= 6);
            assert(generated.variability.periodTurns <= 16);
            assert(generated.variability.amplitudePercent >= 5);
            assert(generated.variability.amplitudePercent <= 16);
            assert(generated.variability.phaseOffset < generated.variability.periodTurns);
        }
    }
    assert(variableStars > generatedStars * 3 / 100);
    assert(variableStars < generatedStars * 7 / 100);

    return 0;
}
