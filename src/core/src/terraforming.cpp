#include "suns/terraforming.hpp"
#include "suns/campaign.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

namespace suns {

TerraformingLimits terraforming_limits(const GameState& state, PlayerId player)
{
    const auto* empire = find_player(state, player);
    if (!empire || !empire->race.environmentBased) return {};
    constexpr std::array<int, 4> limits{3, 7, 11, 15};
    constexpr std::array<int, 4> levels{1, 5, 10, 16};
    const int biology = technology_level(state, player, ResearchField::Biology);
    const auto limit = [&](ResearchField field) {
        int result = 0;
        for (int i = 0; i < 4; ++i)
            if (biology >= i + 1 && technology_level(state, player, field) >= levels[i]) result = limits[i];
        return static_cast<std::uint8_t>(result);
    };
    return {limit(ResearchField::Energy), limit(ResearchField::Propulsion),
        empire->race.radiationImmune ? std::uint8_t{0} : limit(ResearchField::Weapons)};
}

std::optional<PlanetEnvironment> next_terraforming_environment(
    const GameState& state, PlayerId player, const Planet& planet)
{
    const auto* empire = find_player(state, player);
    if (!empire || !empire->race.environmentBased) return std::nullopt;
    const auto limits = terraforming_limits(state, player);
    const auto natural = planet.naturalEnvironment.value_or(planet.environment);
    const std::array<int, 3> original{natural.temperature, natural.gravity, natural.radiation};
    const std::array<int, 3> current{planet.environment.temperature, planet.environment.gravity, planet.environment.radiation};
    const std::array<int, 3> maximum{limits.temperature, limits.gravity, limits.radiation};
    const std::array<RaceEnvironmentRange, 3> ranges{
        empire->race.habitableTemperature, empire->race.habitableGravity, empire->race.habitableRadiation};
    const auto score = [&](PlanetEnvironment environment) {
        const std::array<int, 3> values{environment.temperature, environment.gravity, environment.radiation};
        int outside = 0, displacement = 0;
        for (int axis = 0; axis < 3; ++axis) {
            if (axis == 2 && empire->race.radiationImmune) continue;
            outside += std::max({0, int(ranges[axis].minimum) - values[axis], values[axis] - int(ranges[axis].maximum)});
            displacement += std::abs(2 * values[axis] - int(ranges[axis].minimum) - int(ranges[axis].maximum));
        }
        return std::tuple{race_habitability(empire->race, environment,
            technology_level(state, player, ResearchField::Biology)), -outside, -displacement};
    };
    auto bestScore = score(planet.environment);
    std::optional<PlanetEnvironment> best;
    for (int axis = 0; axis < 3; ++axis) {
        if (maximum[axis] == 0) continue;
        const int center = (int(ranges[axis].minimum) + int(ranges[axis].maximum)) / 2;
        const int value = current[axis] + (current[axis] < center ? 1 : current[axis] > center ? -1 : 0);
        // A captured planet may already lie outside the new owner's limit:
        // allow undoing old terraforming towards the natural baseline.
        if (value == current[axis] || value < 0 || value > 100
            || (std::abs(value - original[axis]) > maximum[axis]
                && std::abs(value - original[axis]) >= std::abs(current[axis] - original[axis]))) continue;
        auto candidate = planet.environment;
        if (axis == 0) candidate.temperature = static_cast<std::uint8_t>(value);
        else if (axis == 1) candidate.gravity = static_cast<std::uint8_t>(value);
        else candidate.radiation = static_cast<std::uint8_t>(value);
        const auto candidateScore = score(candidate);
        if (candidateScore > bestScore) { bestScore = candidateScore; best = candidate; }
    }
    return best;
}

bool terraform_planet(const GameState& state, Planet& planet)
{
    const auto next = next_terraforming_environment(state, planet.owner, planet);
    if (!next) return false;
    if (!planet.naturalEnvironment) planet.naturalEnvironment = planet.environment;
    planet.environment = *next;
    planet.observedHabitability.reset();
    return true;
}

Planet terraforming_potential(const GameState& state, PlayerId player, const Planet& planet)
{
    auto result = planet;
    result.owner = player;
    result.observedHabitability.reset();
    // Three bounded physical axes; even reversing a previous owner's work
    // cannot require more than 300 unit changes.
    for (int step = 0; step < 300 && terraform_planet(state, result); ++step) {}
    return result;
}

} // namespace suns
