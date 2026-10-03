#include "suns/propulsion.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace suns {

// Stars! Technical Reference Guide, Engines (printed page 11).
// Fuel scaling, thrust and deterministic hull damage belong to Suns!.
std::span<const PropulsionTechnology> propulsion_technologies()
{
    using C = ShipComponentType;
    using A = EngineAccess;
    static const std::array<PropulsionTechnology, 15> technologies{{
        {C::SettlersDelight, "Settler's Delight", 0, 0, 6, 6, 9, 2, 2, {1, 0, 1}, A::Settler, true},
        {C::QuickJump5, "Quick Jump 5", 0, 0, 5, 1, 9, 4, 3, {3, 0, 1}},
        {C::LongHump6, "Long Hump 6", 3, 0, 6, 1, 9, 9, 6, {5, 0, 1}},
        {C::DaddyLongLegs7, "Daddy Long Legs 7", 5, 0, 7, 1, 9, 13, 12, {11, 0, 3}},
        {C::AlphaDrive8, "Alpha Drive 8", 7, 0, 8, 1, 9, 17, 28, {16, 0, 3}},
        {C::TransGalacticDrive, "Trans-Galactic Drive", 9, 0, 9, 1, 9, 25, 50, {20, 20, 9}},
        {C::Interspace10, "Interspace-10", 11, 0, 10, 1, 10, 25, 60, {18, 25, 10}, A::NoRamScoops},
        {C::TransStar10, "Trans-Star 10", 23, 0, 10, 1, 10, 5, 10, {3, 0, 3}},
        {C::FuelMizer, "Fuel Mizer", 2, 0, 6, 4, 9, 6, 11, {8, 0, 0}, A::ImprovedFuelEfficiency},
        {C::RadiatingHydroRamScoop, "Radiating Hydro-Ram Scoop", 6, 2, 6, 6, 9, 10, 8, {3, 2, 9}, A::Any, true, true},
        {C::SubGalacticFuelScoop, "Sub-Galactic Fuel Scoop", 8, 2, 7, 5, 9, 20, 12, {4, 4, 7}, A::Any, true},
        {C::TransGalacticFuelScoop, "Trans-Galactic Fuel Scoop", 9, 3, 8, 6, 9, 19, 18, {5, 4, 12}, A::Any, true},
        {C::TransGalacticSuperScoop, "Trans-Galactic Super Scoop", 12, 4, 9, 7, 9, 18, 24, {6, 4, 16}, A::Any, true},
        {C::TransGalacticMizerScoop, "Trans-Galactic Mizer Scoop", 16, 4, 10, 8, 10, 11, 20, {5, 2, 13}, A::Any, true},
        {C::GalaxyScoop, "Galaxy Scoop", 20, 5, 10, 9, 10, 8, 12, {4, 2, 9}, A::ImprovedFuelEfficiency, true},
    }};
    return technologies;
}

const PropulsionTechnology* propulsion_technology(ShipComponentType component)
{
    for (const auto& technology : propulsion_technologies())
        if (technology.component == component) return &technology;
    return nullptr;
}

ShipComponentSpec propulsion_component_spec(const PropulsionTechnology& technology)
{
    ShipComponentSpec spec;
    spec.type = technology.component;
    spec.name = technology.name;
    spec.kind = ShipComponentKind::Engine;
    spec.mass = technology.mass;
    spec.buildCost = technology.cost;
    spec.maxWarp = technology.safeWarp;
    spec.optimalWarp = technology.optimalWarp;
    spec.freeWarp = technology.freeWarp;
    spec.engineThrust = 350.0 + 35.0 * technology.optimalWarp;
    spec.radiationHazard = technology.radiating ? 1.0 : 0.0;
    for (std::uint8_t warp = 1; warp <= kMaxWarp; ++warp) {
        if (warp <= technology.freeWarp) {
            // Genuine scoops collect fuel below their free-speed ceiling.
            // Fuel Mizer has a zero-cost band, but does not collect fuel.
            spec.fuelPer100MassLy[warp] = technology.ramScoop
                ? -0.02 * (technology.freeWarp - warp + 1) : 0.0;
        } else {
            const double speedRatio = double(warp) / technology.optimalWarp;
            spec.fuelPer100MassLy[warp] = 0.15 * std::pow(speedRatio, 6.0);
        }
        if (warp > technology.safeWarp) spec.overdriveDamagePercent[warp] = 18.0;
    }
    return spec;
}

bool engine_access_available(const RaceProfile& race, EngineAccess access)
{
    switch (access) {
    case EngineAccess::Any: return true;
    case EngineAccess::ImprovedFuelEfficiency: return race.improvedFuelEfficiency;
    case EngineAccess::NoRamScoops: return race.noRamScoopEngines;
    case EngineAccess::Settler: return race.settlerEngineAccess;
    }
    return false;
}

bool legacy_propulsion_component(ShipComponentType component)
{
    using C = ShipComponentType;
    return component == C::FusionDrive || component == C::RamScoopDrive
        || component == C::RadiatingRamScoopDrive || component == C::AdvancedFusionDrive
        || component == C::HighWarpDrive || component == C::EfficientRamScoopDrive;
}

bool player_uses_legacy_propulsion(const GameState& state, PlayerId player)
{
    return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [player](const auto& design) {
        return design.owner == player && std::any_of(design.components.begin(), design.components.end(),
            legacy_propulsion_component);
    });
}

} // namespace suns
