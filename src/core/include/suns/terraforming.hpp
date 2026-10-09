#pragma once

#include "suns/game_state.hpp"

namespace suns {

struct TerraformingLimits { std::uint8_t temperature{}, gravity{}, radiation{}; };
[[nodiscard]] TerraformingLimits terraforming_limits(const GameState& state, PlayerId player);
[[nodiscard]] std::optional<PlanetEnvironment> next_terraforming_environment(
    const GameState& state, PlayerId player, const Planet& planet);
bool terraform_planet(const GameState& state, Planet& planet);
[[nodiscard]] Planet terraforming_potential(const GameState& state, PlayerId player, const Planet& planet);

} // namespace suns
