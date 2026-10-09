#pragma once

#include "suns/game_state.hpp"

#include <span>

namespace suns {

struct MiningTechnology {
    ShipComponentType component;
    const char* name;
    double rate; // kt of each mineral per year at concentration 100.
    double mass;
    std::uint8_t construction;
    std::uint8_t electronics;
    MineralCargo minerals;
    std::uint32_t cost;
    bool advancedRemoteMining{};
    bool excludesBasicRemoteMining{};
};

[[nodiscard]] std::span<const MiningTechnology> mining_technologies();
[[nodiscard]] const MiningTechnology* mining_technology(ShipComponentType component);
[[nodiscard]] ShipComponentSpec mining_component_spec(const MiningTechnology& technology);
[[nodiscard]] bool player_uses_legacy_mining(const GameState& state, PlayerId player);
[[nodiscard]] double ship_design_remote_mining_rate(const ShipDesign& design);

} // namespace suns
