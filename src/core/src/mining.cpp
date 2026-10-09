#include "suns/mining.hpp"
#include "suns/hulls.hpp"

#include <algorithm>
#include <array>

namespace suns {

// Stars! Technical Reference Guide, printed p12. Orbital Adjuster is a
// terraforming device, not a miner, and has no implemented Suns! capability.
std::span<const MiningTechnology> mining_technologies()
{
    using C = ShipComponentType;
    static const std::array<MiningTechnology, 6> technologies{{
        {C::RoboMidgetMiner, "Robo-Midget Miner", 5, 80, 0, 0, {14, 0, 4}, 50, true},
        {C::RoboMiniMiner, "Robo-Mini-Miner", 4, 240, 2, 1, {30, 0, 7}, 100},
        {C::RoboMiner, "Robo-Miner", 12, 240, 4, 2, {30, 0, 7}, 100, false, true},
        {C::RoboMaxiMiner, "Robo-Maxi-Miner", 18, 240, 7, 4, {30, 0, 7}, 100, false, true},
        {C::RoboSuperMiner, "Robo-Super-Miner", 27, 240, 12, 6, {30, 0, 7}, 100, false, true},
        {C::RoboUltraMiner, "Robo-Ultra-Miner", 25, 80, 15, 8, {14, 0, 4}, 50, true},
    }};
    return technologies;
}

const MiningTechnology* mining_technology(ShipComponentType component)
{
    for (const auto& technology : mining_technologies())
        if (technology.component == component) return &technology;
    return nullptr;
}

ShipComponentSpec mining_component_spec(const MiningTechnology& technology)
{
    ShipComponentSpec spec;
    spec.type = technology.component;
    spec.name = technology.name;
    spec.kind = ShipComponentKind::Mining;
    spec.mass = technology.mass;
    spec.buildCost = technology.cost;
    spec.remoteMiningUnits = technology.rate;
    return spec;
}

bool player_uses_legacy_mining(const GameState& state, PlayerId player)
{
    return player_uses_legacy_hulls(state, player)
        || std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [player](const auto& design) {
            return design.owner == player && std::find(design.components.begin(), design.components.end(),
                ShipComponentType::RemoteMiningModule) != design.components.end();
        });
}

double ship_design_remote_mining_rate(const ShipDesign& design)
{
    double rate = 0;
    for (const auto component : design.components) rate += component_spec(component).remoteMiningUnits;
    return rate;
}

} // namespace suns
