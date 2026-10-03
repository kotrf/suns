#pragma once

#include "suns/game_state.hpp"

namespace suns {

[[nodiscard]] const std::vector<ShipHullSpec>& reference_hulls();
[[nodiscard]] const ShipHullSpec* reference_hull(ShipHullType type);
[[nodiscard]] bool player_uses_legacy_hulls(const GameState& state, PlayerId player);
[[nodiscard]] std::string hull_access_name(HullAccess access);
[[nodiscard]] std::string ship_slot_name(const ShipSlotSpec& slot);
[[nodiscard]] std::string ship_component_equipment_name(ShipComponentType component);
[[nodiscard]] bool ship_slot_accepts(const ShipSlotSpec& slot, ShipComponentType component);

} // namespace suns
