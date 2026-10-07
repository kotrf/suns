#include "suns/scanners.hpp"

#include <algorithm>
#include <array>

namespace suns {

// Stars! Technical Reference Guide, printed p10. Special cargo theft,
// cloaking and No Advanced Scanners are outside the current Suns! rules.
std::span<const ScannerTechnology> scanner_technologies()
{
    using C = ShipComponentType;
    static const std::array<ScannerTechnology, 16> technologies{{
        {C::BatScanner, "Bat Scanner", 0, 0, 2, 0, 0, 0, 0, {1, 0, 1}, 1},
        {C::RhinoScanner, "Rhino Scanner", 50, 0, 5, 1, 0, 0, 0, {3, 0, 2}, 3},
        {C::MoleScanner, "Mole Scanner", 100, 0, 2, 4, 0, 0, 0, {2, 0, 2}, 9},
        {C::DnaScanner, "DNA Scanner", 125, 0, 2, 0, 0, 6, 3, {1, 1, 1}, 5},
        {C::PossumScanner, "Possum Scanner", 150, 0, 3, 5, 0, 0, 0, {3, 0, 3}, 18},
        {C::PickPocketScanner, "Pick Pocket Scanner", 80, 0, 15, 4, 4, 4, 0, {8, 10, 6}, 35, true},
        {C::ChameleonScanner, "Chameleon Scanner", 160, 45, 6, 6, 3, 0, 0, {4, 6, 4}, 25, true},
        {C::FerretScanner, "Ferret Scanner", 185, 50, 2, 7, 3, 2, 0, {2, 0, 8}, 36},
        {C::DolphinScanner, "Dolphin Scanner", 220, 100, 4, 10, 5, 4, 0, {5, 5, 10}, 40},
        {C::GazelleScanner, "Gazelle Scanner", 225, 0, 5, 8, 4, 0, 0, {4, 0, 5}, 24},
        {C::RnaScanner, "RNA Scanner", 230, 0, 2, 0, 0, 10, 5, {1, 1, 2}, 20},
        {C::CheetahScanner, "Cheetah Scanner", 275, 0, 4, 11, 5, 0, 0, {3, 1, 13}, 50},
        {C::ElephantScanner, "Elephant Scanner", 300, 200, 6, 16, 6, 7, 0, {8, 5, 14}, 70},
        {C::EagleEyeScanner, "Eagle Eye Scanner", 335, 0, 3, 14, 6, 0, 0, {3, 2, 21}, 64},
        {C::RobberBaronScanner, "Robber Baron Scanner", 220, 120, 20, 15, 10, 10, 0, {10, 10, 10}, 90, true},
        {C::PeerlessScanner, "Peerless Scanner", 500, 0, 4, 24, 7, 0, 0, {3, 2, 30}, 90},
    }};
    return technologies;
}

const ScannerTechnology* scanner_technology(ShipComponentType component)
{
    for (const auto& technology : scanner_technologies())
        if (technology.component == component) return &technology;
    return nullptr;
}

ShipComponentSpec scanner_component_spec(const ScannerTechnology& technology)
{
    ShipComponentSpec spec;
    spec.type = technology.component;
    spec.name = technology.name;
    spec.kind = ShipComponentKind::Scanner;
    spec.mass = technology.mass;
    spec.buildCost = technology.cost;
    spec.sensorRange = technology.ordinaryRange;
    spec.penetratingSensorRange = technology.penetratingRange;
    spec.penetratesPlanets = technology.penetratingRange > 0;
    return spec;
}

bool legacy_scanner_component(ShipComponentType component)
{
    using C = ShipComponentType;
    return component == C::LongRangeScanner || component == C::CompactLongRangeScanner
        || component == C::ExtendedRangeScanner || component == C::PenetratingScanner
        || component == C::DeepPenetratingScanner;
}

bool player_uses_legacy_scanners(const GameState& state, PlayerId player)
{
    return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [player](const auto& design) {
        return design.owner == player && std::any_of(design.components.begin(), design.components.end(),
            legacy_scanner_component);
    });
}

} // namespace suns
