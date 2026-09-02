#include "suns/game_state.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace suns {

std::vector<ProductionCompletionEstimate> forecast_production_queue(
    const GameState& state,
    const Planet& planet,
    const std::vector<ProductionItem>& queue,
    std::uint32_t maximumTurns)
{
    std::vector<ProductionCompletionEstimate> result(queue.size());
    if (queue.empty()) return result;

    struct ForecastItem {
        ProductionItem item;
        std::size_t originalIndex{};
    };

    std::vector<ForecastItem> remaining;
    remaining.reserve(queue.size());
    for (std::size_t index = 0; index < queue.size(); ++index) {
        remaining.push_back({queue[index], index});
    }

    Planet simulated = planet;
    simulated.productionQueue.clear();
    const auto* player = find_player(state, planet.owner);
    const auto allocationPercent = player && player->technology.researchActive
        ? player->technology.researchAllocationPercent
        : 0;
    bool hasShipyard = colony_has_orbital_service(
        state, planet.id, planet.owner, OrbitalStationModule::Shipyard);
    for (std::uint32_t offset = 1; offset <= maximumTurns && !remaining.empty(); ++offset) {
        const auto mined = projected_mineral_mining(state, simulated);
        simulated.minerals.ironium += mined.ironium;
        simulated.minerals.boranium += mined.boranium;
        simulated.minerals.germanium += mined.germanium;

        const auto output = colony_output(simulated);
        const auto research = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(output) * allocationPercent / 100U);
        std::uint32_t available = output - research;
        while (!remaining.empty()) {
            auto& front = remaining.front();
            if (front.item.kind == ProductionKind::Research) {
                remaining.erase(remaining.begin());
                continue;
            }
            if (front.item.kind == ProductionKind::ColonyShip && !hasShipyard) break;

            const auto spent = std::min(available, front.item.remainingCost);
            available -= spent;
            front.item.remainingCost -= spent;
            if (front.item.remainingCost != 0) break;

            const auto minerals = production_item_mineral_cost(state, front.item);
            if (!mineral_cargo_sufficient(simulated.minerals, minerals)) break;
            subtract_minerals(simulated.minerals, minerals);

            result[front.originalIndex].completionTurn = state.turn + offset;
            if (front.item.kind == ProductionKind::Factory) ++simulated.industry;
            else if (front.item.kind == ProductionKind::Mine) ++simulated.mines;
            else if (front.item.kind == ProductionKind::OrbitalStation) hasShipyard = true;
            remaining.erase(remaining.begin());
        }
        simulated.population += projected_population_growth(
            state, simulated, state.turn + offset - 1);
    }

    for (const auto& item : remaining) {
        result[item.originalIndex].beyondForecastHorizon = true;
    }
    return result;
}

} // namespace suns
