#include "suns/hulls.hpp"

#include <algorithm>
#include <initializer_list>

namespace suns {
namespace {

struct Bank { std::uint16_t mask; std::uint8_t capacity; };

ShipSlotCategory bank_category(std::uint16_t mask)
{
    switch (mask) {
    case 1: return ShipSlotCategory::Engine;
    case 2: return ShipSlotCategory::Scanner;
    case 4: return ShipSlotCategory::Shield;
    case 8: return ShipSlotCategory::Armor;
    case 48: return ShipSlotCategory::Weapon;
    case 64: return ShipSlotCategory::Bomb;
    case 128: return ShipSlotCategory::Mining;
    case 256: return ShipSlotCategory::MineLayer;
    case 2048: return ShipSlotCategory::Electrical;
    case 4096: return ShipSlotCategory::Mechanical;
    case 6462: return ShipSlotCategory::General;
    default: return ShipSlotCategory::Mixed;
    }
}

ShipHullSpec make_hull(
    ShipHullType type, const char* name, std::uint8_t construction, double mass,
    std::uint32_t cost, MineralCargo minerals, double cargo, double fuel,
    std::uint16_t armor, std::uint8_t initiative, std::initializer_list<Bank> banks,
    HullAccess access, bool advancedMining, bool excludesBasicMining, bool trader,
    double generation, double repair, bool doubleMines)
{
    ShipHullSpec hull;
    hull.type = type; hull.name = name; hull.mass = mass; hull.buildCost = cost;
    hull.baseFuelCapacity = fuel; hull.baseCargoCapacity = cargo;
    hull.baseMineralCost = minerals; hull.armor = armor; hull.initiative = initiative;
    hull.constructionLevel = construction; hull.access = access;
    hull.requiresAdvancedRemoteMining = advancedMining;
    hull.excludesBasicRemoteMining = excludesBasicMining; hull.mysteryTrader = trader;
    hull.fuelGenerationPerTurn = generation; hull.fleetRepairBonus = repair;
    hull.doublesMineLaying = doubleMines;
    hull.requiredEngines = 0;
    std::uint8_t row = 0, bankId = 0, engineCell = 0, equipmentCell = 0;
    for (const auto bank : banks) {
        ++bankId;
        for (std::uint8_t index = 0; index < bank.capacity; ++index) {
            const bool engine = bank.mask == 1;
            const auto id = static_cast<ShipSlotId>(engine ? 100 + engineCell++ : 200 + equipmentCell++);
            hull.fittingSlots.push_back({id, bank_category(bank.mask),
                static_cast<std::uint8_t>(index % 4), static_cast<std::uint8_t>(row + index / 4),
                bank.mask, bankId, bank.capacity});
            if (engine) ++hull.requiredEngines;
            else if (bank.mask == 128) ++hull.miningSlots;
            else ++hull.generalSlots;
        }
        row += static_cast<std::uint8_t>((bank.capacity + 3) / 4);
    }
    return hull;
}

std::uint16_t equipment_mask(ShipComponentType component)
{
    const auto kind = component_spec(component).kind;
    if (kind == ShipComponentKind::Engine) return 1;
    if (kind == ShipComponentKind::Scanner) return 2;
    if (kind == ShipComponentKind::Mining) return 128;
    if (component == ShipComponentType::RelayArray) return 2048;
    return 4096; // Tanks, cargo pods, generators, colonizers and repair bays.
}

} // namespace

const std::vector<ShipHullSpec>& reference_hulls()
{
    // Original component-data export, cross-checked with Technical Reference
    // Guide pp.16-23. Preserve bank masks and capacities; presentation is ours.
    using H = ShipHullType;
    static const std::vector<ShipHullSpec> hulls{
        make_hull(H::SmallFreighter, "Small Freighter", 0, 25, 20, {12, 0, 17}, 70, 130, 25, 0, {{1, 1}, {6146, 1}, {12, 1}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::MediumFreighter, "Medium Freighter", 3, 60, 40, {20, 0, 19}, 210, 450, 50, 0, {{1, 1}, {6146, 1}, {12, 1}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::LargeFreighter, "Large Freighter", 8, 125, 100, {35, 0, 21}, 1200, 2600, 150, 0, {{1, 2}, {6146, 2}, {12, 2}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::SuperFreighter, "Super Freighter", 13, 175, 125, {45, 0, 21}, 3000, 8000, 400, 0, {{1, 3}, {6146, 3}, {12, 5}, {2048, 2}}, HullAccess::InnerStrength, false, false, false, 0, 0, false),
        make_hull(H::StarsScout, "Scout", 0, 8, 10, {4, 2, 4}, 0, 50, 20, 1, {{1, 1}, {2, 1}, {6462, 1}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::Frigate, "Frigate", 6, 8, 12, {4, 2, 4}, 0, 125, 45, 4, {{1, 1}, {2, 2}, {6462, 3}, {12, 2}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::Destroyer, "Destroyer", 3, 30, 35, {15, 3, 5}, 0, 280, 200, 3, {{1, 1}, {48, 1}, {48, 1}, {6462, 1}, {8, 2}, {4096, 1}, {2048, 1}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::Cruiser, "Cruiser", 9, 90, 85, {40, 5, 8}, 0, 600, 700, 5, {{1, 2}, {6148, 1}, {6148, 1}, {48, 2}, {48, 2}, {6462, 2}, {12, 2}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::BattleCruiser, "Battle Cruiser", 10, 120, 120, {55, 8, 12}, 0, 1400, 1000, 5, {{1, 2}, {6148, 2}, {6148, 2}, {48, 3}, {48, 3}, {6462, 3}, {12, 4}}, HullAccess::WarMonger, false, false, false, 0, 0, false),
        make_hull(H::Battleship, "Battleship", 13, 222, 225, {120, 25, 20}, 0, 2800, 2000, 10, {{1, 4}, {6146, 1}, {4, 8}, {48, 6}, {48, 6}, {48, 2}, {48, 2}, {48, 4}, {8, 6}, {2048, 3}, {2048, 3}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::Dreadnought, "Dreadnought", 16, 250, 275, {140, 30, 25}, 0, 4500, 4500, 10, {{1, 5}, {12, 4}, {12, 4}, {48, 6}, {48, 6}, {2048, 4}, {2048, 4}, {48, 8}, {48, 8}, {8, 8}, {52, 5}, {52, 5}, {6462, 2}}, HullAccess::WarMonger, false, false, false, 0, 0, false),
        make_hull(H::Privateer, "Privateer", 4, 65, 50, {50, 3, 2}, 250, 650, 150, 3, {{1, 1}, {12, 2}, {6146, 1}, {6462, 1}, {6462, 1}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::Rogue, "Rogue", 8, 75, 60, {80, 5, 5}, 500, 2250, 450, 4, {{1, 2}, {12, 3}, {6400, 2}, {2, 1}, {6462, 2}, {6462, 2}, {6400, 2}, {2048, 1}, {2048, 1}}, HullAccess::SuperStealth, false, false, false, 0, 0, false),
        make_hull(H::Galleon, "Galleon", 11, 125, 105, {70, 5, 5}, 1000, 2500, 900, 4, {{1, 4}, {12, 2}, {12, 2}, {6462, 3}, {6462, 3}, {6400, 2}, {6144, 2}, {2, 2}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::StarsMiniColonyShip, "Mini-Colony Ship", 0, 8, 3, {2, 0, 2}, 10, 150, 10, 0, {{1, 1}, {4096, 1}}, HullAccess::HyperExpansion, false, false, false, 0, 0, false),
        make_hull(H::ColonyShip, "Colony Ship", 0, 20, 20, {10, 0, 15}, 25, 200, 20, 0, {{1, 1}, {4096, 1}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::MiniBomber, "Mini Bomber", 1, 28, 35, {20, 5, 10}, 0, 120, 50, 0, {{1, 1}, {64, 2}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::B17Bomber, "B-17 Bomber", 6, 69, 150, {55, 10, 10}, 0, 400, 175, 0, {{1, 2}, {64, 4}, {64, 4}, {6146, 1}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::StealthBomber, "Stealth Bomber", 8, 70, 175, {55, 10, 15}, 0, 750, 225, 0, {{1, 2}, {64, 4}, {64, 4}, {6146, 1}, {2048, 3}}, HullAccess::SuperStealth, false, false, false, 0, 0, false),
        make_hull(H::B52Bomber, "B-52 Bomber", 15, 110, 280, {90, 15, 10}, 0, 750, 450, 0, {{1, 3}, {64, 4}, {64, 4}, {64, 4}, {64, 4}, {6146, 2}, {4, 2}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::MidgetMiner, "Midget Miner", 0, 10, 20, {10, 0, 3}, 0, 210, 100, 0, {{1, 1}, {128, 2}}, HullAccess::Standard, true, true, false, 0, 0, false),
        make_hull(H::MiniMiner, "Mini-Miner", 2, 80, 50, {25, 0, 6}, 0, 210, 130, 0, {{1, 1}, {6146, 1}, {128, 1}, {128, 1}}, HullAccess::Standard, false, true, false, 0, 0, false),
        make_hull(H::Miner, "Miner", 6, 110, 110, {32, 0, 6}, 0, 500, 475, 0, {{1, 2}, {6154, 2}, {128, 2}, {128, 1}, {128, 2}, {128, 1}}, HullAccess::Standard, false, true, false, 0, 0, false),
        make_hull(H::MaxiMiner, "Maxi-Miner", 11, 110, 140, {32, 0, 6}, 0, 850, 1400, 0, {{1, 3}, {6154, 2}, {128, 4}, {128, 1}, {128, 4}, {128, 1}}, HullAccess::Standard, false, true, false, 0, 0, false),
        make_hull(H::UltraMiner, "Ultra-Miner", 14, 100, 130, {30, 0, 6}, 0, 1300, 1500, 0, {{1, 2}, {6154, 3}, {128, 4}, {128, 2}, {128, 4}, {128, 2}}, HullAccess::Standard, true, true, false, 0, 0, false),
        make_hull(H::FuelTransport, "Fuel Transport", 4, 12, 50, {10, 0, 5}, 0, 750, 5, 0, {{1, 1}, {4, 1}}, HullAccess::InnerStrength, false, false, false, 200, 5, false),
        make_hull(H::SuperFuelXport, "Super-Fuel Xport", 7, 111, 70, {20, 0, 8}, 0, 2250, 12, 0, {{1, 2}, {4, 2}, {2, 1}}, HullAccess::Standard, false, false, false, 200, 10, false),
        make_hull(H::MiniMineLayer, "Mini Mine Layer", 0, 10, 20, {8, 2, 5}, 0, 400, 60, 0, {{1, 1}, {256, 2}, {256, 2}, {6146, 1}}, HullAccess::SpaceDemolition, false, false, false, 0, 0, true),
        make_hull(H::SuperMineLayer, "Super Mine Layer", 15, 30, 30, {20, 3, 9}, 0, 2200, 1200, 0, {{1, 3}, {256, 8}, {256, 8}, {12, 3}, {6146, 3}, {6400, 3}}, HullAccess::SpaceDemolition, false, false, false, 0, 0, true),
        make_hull(H::Nubian, "Nubian", 26, 100, 150, {75, 12, 12}, 0, 5000, 5000, 2, {{1, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}, {6462, 3}}, HullAccess::Standard, false, false, false, 0, 0, false),
        make_hull(H::MiniMorph, "Mini Morph", 8, 70, 100, {30, 8, 8}, 150, 400, 250, 2, {{1, 2}, {6462, 3}, {6462, 1}, {6462, 1}, {6462, 1}, {6462, 2}, {6462, 2}}, HullAccess::Standard, false, false, true, 0, 0, false),
        make_hull(H::MetaMorph, "Meta Morph", 10, 85, 120, {50, 12, 12}, 300, 700, 500, 2, {{1, 3}, {6462, 8}, {6462, 2}, {6462, 2}, {6462, 1}, {6462, 2}, {6462, 2}}, HullAccess::HyperExpansion, false, false, false, 0, 0, false),
    };
    return hulls;
}

const ShipHullSpec* reference_hull(ShipHullType type)
{
    const auto& hulls = reference_hulls();
    const auto it = std::find_if(hulls.begin(), hulls.end(), [type](const auto& hull) { return hull.type == type; });
    return it == hulls.end() ? nullptr : &*it;
}

bool player_uses_legacy_hulls(const GameState& state, PlayerId player)
{
    return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [player](const auto& design) {
        return design.owner == player && !reference_hull(design.hull);
    });
}

std::string hull_access_name(HullAccess access)
{
    switch (access) {
    case HullAccess::Standard: return "Standard";
    case HullAccess::InnerStrength: return "Inner Strength";
    case HullAccess::SuperStealth: return "Super Stealth";
    case HullAccess::WarMonger: return "War Monger";
    case HullAccess::SpaceDemolition: return "Space Demolition";
    case HullAccess::HyperExpansion: return "Hyper Expansion";
    }
    return "Unknown";
}

bool ship_slot_accepts(const ShipSlotSpec& slot, ShipComponentType component)
{
    return slot.allowedEquipment ? (slot.allowedEquipment & equipment_mask(component)) != 0
        : slot.category == ship_component_slot_category(component);
}

std::string ship_component_equipment_name(ShipComponentType component)
{
    ShipSlotSpec slot;
    slot.allowedEquipment = equipment_mask(component);
    return ship_slot_name(slot);
}

std::string ship_slot_name(const ShipSlotSpec& slot)
{
    if (slot.allowedEquipment) {
        if (slot.allowedEquipment == 6462) return "General";
        std::string result;
        for (const auto& [mask, name] : {
                 std::pair{1, "Engine"}, {2, "Scanner"}, {4, "Shield"}, {8, "Armor"},
                 {48, "Weapon"}, {64, "Bomb"}, {128, "Mining robot"}, {256, "Mine layer"},
                 {2048, "Electrical"}, {4096, "Mechanical"}}) {
            if (!(slot.allowedEquipment & mask)) continue;
            if (!result.empty()) result += "/";
            result += name;
        }
        return result;
    }
    switch (slot.category) {
    case ShipSlotCategory::Engine: return "Engine";
    case ShipSlotCategory::Mining: return "Mining";
    default: return "General";
    }
}

} // namespace suns
