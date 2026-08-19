#include "suns/game_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace suns {
namespace {

std::uint64_t splitmix64(std::uint64_t value)
{
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

double unitRandom(std::uint64_t seed)
{
    constexpr double denom = static_cast<double>(1ULL << 53U);
    return static_cast<double>(splitmix64(seed) >> 11U) / denom;
}

double triangular(std::uint64_t seedA, std::uint64_t seedB)
{
    return (unitRandom(seedA) + unitRandom(seedB)) * 0.5;
}

double clampConcentration(double value)
{
    return std::clamp(value, 8.0, 100.0);
}

MineralCargo classBias(StarClass stellarClass)
{
    // Deliberately weak priors. Spectral colour should inform scouting
    // intuition, not replace scouting. Large random overlap is preserved.
    switch (stellarClass) {
    case StarClass::BlueWhite:   return {18.0, 6.0, -4.0};
    case StarClass::White:       return {10.0, 5.0, 0.0};
    case StarClass::YellowWhite: return {4.0, 4.0, 3.0};
    case StarClass::Yellow:      return {0.0, 2.0, 5.0};
    case StarClass::Orange:      return {-3.0, 3.0, 9.0};
    case StarClass::Red:         return {-7.0, 2.0, 12.0};
    }
    return {};
}

MineralCargo componentMineralCost(ShipComponentType type)
{
    switch (type) {
    case ShipComponentType::FusionDrive:             return {2.0, 2.0, 1.0};
    case ShipComponentType::RamScoopDrive:           return {2.0, 3.0, 2.0};
    case ShipComponentType::RadiatingRamScoopDrive:  return {2.0, 2.0, 2.0};
    case ShipComponentType::LongRangeScanner:        return {0.0, 1.0, 3.0};
    case ShipComponentType::ColonyModule:            return {2.0, 2.0, 3.0};
    case ShipComponentType::FuelTank:                return {2.0, 0.0, 0.0};
    case ShipComponentType::CargoPod:                return {2.0, 1.0, 0.0};
    case ShipComponentType::AntimatterGenerator:     return {1.0, 3.0, 3.0};
    }
    return {};
}

MineralCargo hullMineralCost(ShipHullType type)
{
    switch (type) {
    case ShipHullType::Scout:          return {4.0, 1.0, 1.0};
    case ShipHullType::LightTransport: return {6.0, 2.0, 2.0};
    case ShipHullType::MediumTransport:return {9.0, 3.0, 3.0};
    }
    return {};
}

void addMinerals(MineralCargo& target, const MineralCargo& add)
{
    target.ironium += add.ironium;
    target.boranium += add.boranium;
    target.germanium += add.germanium;
}

} // namespace

MineralCargo ship_design_mineral_cost(const ShipDesign& design)
{
    auto cost = hullMineralCost(design.hull);
    for (const auto component : design.components) addMinerals(cost, componentMineralCost(component));
    return cost;
}

bool mineral_cargo_sufficient(const MineralCargo& available, const MineralCargo& required)
{
    constexpr double epsilon = 0.000001;
    return available.ironium + epsilon >= required.ironium
        && available.boranium + epsilon >= required.boranium
        && available.germanium + epsilon >= required.germanium;
}

void subtract_minerals(MineralCargo& available, const MineralCargo& required)
{
    available.ironium = std::max(0.0, available.ironium - required.ironium);
    available.boranium = std::max(0.0, available.boranium - required.boranium);
    available.germanium = std::max(0.0, available.germanium - required.germanium);
}

MineralCargo planet_mineral_concentration(const GameState& state, const Planet& planet)
{
    const auto* star = find_star(state, planet.star);
    const auto bias = star ? classBias(star->stellarClass) : MineralCargo{};
    const auto root = state.galaxySeed ^ (static_cast<std::uint64_t>(planet.id) * 0xD6E8FEB86659FD93ULL);

    // 20..90-ish before spectral bias, with triangular rather than uniform
    // distribution so extreme concentrations stay memorable.
    const auto baseI = 18.0 + triangular(root ^ 0x11ULL, root ^ 0x12ULL) * 72.0;
    const auto baseB = 18.0 + triangular(root ^ 0x21ULL, root ^ 0x22ULL) * 72.0;
    const auto baseG = 18.0 + triangular(root ^ 0x31ULL, root ^ 0x32ULL) * 72.0;

    return {
        clampConcentration(baseI + bias.ironium),
        clampConcentration(baseB + bias.boranium),
        clampConcentration(baseG + bias.germanium),
    };
}

MineralCargo projected_mineral_mining(const GameState& state, const Planet& planet)
{
    if (planet.owner == 0 || planet.population == 0) return {};
    const auto concentration = planet_mineral_concentration(state, planet);

    // First economy slice: every settled world has a small baseline extraction
    // workforce scaling with population. Dedicated mine infrastructure can be
    // layered on later without changing concentrations or stock semantics.
    const auto extractionUnits = 1.0 + static_cast<double>(planet.population) / 750.0;
    return {
        extractionUnits * concentration.ironium / 100.0,
        extractionUnits * concentration.boranium / 100.0,
        extractionUnits * concentration.germanium / 100.0,
    };
}

MineralCargo production_item_mineral_cost(const GameState& state, const ProductionItem& item)
{
    if (item.kind == ProductionKind::Factory) return {2.0, 1.0, 2.0};
    const auto designId = item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId;
    if (const auto* design = find_ship_design(state, designId)) return ship_design_mineral_cost(*design);
    return {};
}

} // namespace suns
