#pragma once

#include "suns/game_state.hpp"

#include <span>

namespace suns {

enum class EquipmentAccess {
    Any, InnerStrength, SuperStealth, WarMonger, SpaceDemolition, HyperExpansion,
    InnerStrengthOrSpaceDemolition, ClaimAdjuster, AlternateReality, InterstellarTraveler, MysteryTrader,
};

struct EquipmentTechnology {
    ShipComponentType component;
    const char* name;
    ShipComponentKind kind;
    double mass;
    std::uint32_t cost;
    MineralCargo minerals;
    std::array<std::uint8_t, kResearchFieldCount> levels;
    EquipmentAccess access{EquipmentAccess::Any};
    bool excludesInnerStrength{};
    bool excludesWarMonger{};
    bool unarmedTransportOnly{};
    ShipComponentSpec effects;
};

[[nodiscard]] std::span<const EquipmentTechnology> equipment_technologies();
[[nodiscard]] const EquipmentTechnology* equipment_technology(ShipComponentType component);
[[nodiscard]] ShipComponentSpec equipment_component_spec(const EquipmentTechnology& technology);
[[nodiscard]] bool equipment_access_applicable(const GameState& state, PlayerId player, const EquipmentTechnology& technology);
[[nodiscard]] std::string equipment_access_requirement(const EquipmentTechnology& technology);
[[nodiscard]] std::string equipment_effect_description(const EquipmentTechnology& technology);
[[nodiscard]] bool legacy_equipment_component(ShipComponentType component);
[[nodiscard]] bool player_uses_legacy_equipment(const GameState& state, PlayerId player);
[[nodiscard]] double ship_design_armor(const ShipDesign& design);
[[nodiscard]] double ship_design_shields(const ShipDesign& design);

} // namespace suns
