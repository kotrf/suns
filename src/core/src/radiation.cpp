#include "suns/game_state.hpp"

#include <algorithm>
#include <cmath>

namespace suns {

bool fleet_radiation_safe(const GameState& state, const Fleet& fleet)
{
    if (fleet_radiation_hazard(state, fleet) <= 0.0) return true;

    const auto* player = find_player(state, fleet.owner);
    if (!player) return true;
    return player->radiationImmune
        || player->radiationTolerance + 0.000001 >= kRadiatingDriveSafeTolerance;
}

std::uint64_t projected_fleet_radiation_losses(const GameState& state, const Fleet& fleet)
{
    if (fleet.colonists == 0 || fleet_radiation_safe(state, fleet)) return 0;

    const auto proportional = static_cast<std::uint64_t>(
        std::ceil(static_cast<double>(fleet.colonists) * kRadiatingDriveColonistLossFraction));
    return std::min(fleet.colonists, std::max<std::uint64_t>(1, proportional));
}

void apply_fleet_radiation_attrition(GameState& state, Fleet& fleet)
{
    const auto losses = projected_fleet_radiation_losses(state, fleet);
    fleet.colonists -= std::min(fleet.colonists, losses);
}

} // namespace suns
