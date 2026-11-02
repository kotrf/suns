#pragma once

#include "suns/game_state.hpp"

namespace suns {

enum class HullDeckShape { Dart, Transport, Combat, Miner, Bomber, Support, Base };

struct HullFittingLayout {
    std::vector<ShipSlotSpec> cells;
    HullDeckShape shape{HullDeckShape::Support};
    int columns{};
    int rows{};
};

// Presentation only: retain every logical ID, category, mask and bank.
[[nodiscard]] HullFittingLayout ship_fitting_layout(const ShipHullSpec& hull);

} // namespace suns
