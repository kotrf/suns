#pragma once

#include "suns/turn_processor.hpp"

namespace suns {

enum class RacePreset { Terran, Cryophile, Radiotroph };
struct EmpireSetup {
    std::string name;
    RacePreset race{RacePreset::Terran};
};
struct ResearchUnlock {
    ResearchField field;
    std::uint8_t level;
    std::string name;
    std::string description;
    std::optional<ShipComponentType> component;
};

[[nodiscard]] RaceProfile race_preset(RacePreset preset);
[[nodiscard]] const std::vector<ResearchUnlock>& research_unlocks();
[[nodiscard]] std::uint32_t race_habitability(
    const RaceProfile& race, PlanetEnvironment environment, std::uint8_t biology);
[[nodiscard]] std::uint32_t player_planet_habitability(
    const GameState& state, PlayerId player, const Planet& planet, std::uint64_t turn);
[[nodiscard]] GameState generate_campaign(
    const GalaxyConfig& config, const std::vector<EmpireSetup>& empires);

// Constructed from delivered knowledge, never serialized from host state directly.
// Unknown planets and enemy internals are absent. Seed and in-flight reports are private.
struct PlayerView {
    PlayerId player{};
    GameState state;
};
[[nodiscard]] PlayerView make_player_view(const GameState& host, PlayerId player);

// Canonical ordering and exactly one submission per empire. Missing submissions
// are not silently converted into passes; a pass is an explicit empty envelope.
[[nodiscard]] TurnResult resolve_campaign_turn(
    const GameState& state, std::vector<PlayerOrders> submissions);

} // namespace suns
