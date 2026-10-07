#pragma once

#include "suns/game_state.hpp"

#include <span>

namespace suns {

struct ScannerTechnology {
    ShipComponentType component;
    const char* name;
    double ordinaryRange;
    double penetratingRange;
    double mass;
    std::uint8_t electronics;
    std::uint8_t energy;
    std::uint8_t biology;
    std::uint8_t propulsion;
    MineralCargo minerals;
    std::uint32_t cost;
    bool superStealth{};
};

[[nodiscard]] std::span<const ScannerTechnology> scanner_technologies();
[[nodiscard]] const ScannerTechnology* scanner_technology(ShipComponentType component);
[[nodiscard]] ShipComponentSpec scanner_component_spec(const ScannerTechnology& technology);
[[nodiscard]] bool legacy_scanner_component(ShipComponentType component);
[[nodiscard]] bool player_uses_legacy_scanners(const GameState& state, PlayerId player);

} // namespace suns
