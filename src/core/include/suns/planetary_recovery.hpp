#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr unsigned kContaminationDecay = 2;
inline constexpr unsigned kContaminationCleanup = 10;
void decay_planet_contamination(Planet&);
[[nodiscard]] int contamination_habitability_penalty(const GameState&, PlayerId, const Planet&);
[[nodiscard]] std::uint32_t cleanup_units_remaining(const Planet&);
[[nodiscard]] std::uint32_t cleanup_units_available(const Planet&);
[[nodiscard]] PlanetAftermathObservation observe_planet_aftermath(const Planet&, bool exact, std::uint64_t turn);
[[nodiscard]] bool aftermath_observation_valid(const PlanetAftermathObservation&);
[[nodiscard]] bool same_aftermath_estimate(const PlanetAftermathObservation&, const PlanetAftermathObservation&);
[[nodiscard]] std::optional<PlanetAftermathObservation> known_planet_aftermath(const GameState&, PlayerId, PlanetId);
// Orbital active scanners only; their report can remain aboard a silent fleet.
void observe_planetary_aftermath(GameState&, std::uint64_t turn);
} // namespace suns
