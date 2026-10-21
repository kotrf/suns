#include "suns/production.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/planetary_recovery.hpp"

#include <algorithm>
#include <numeric>

namespace suns {

std::vector<ProductionCompletionEstimate> forecast_production_queue(
    const GameState& state, const Planet& planet,
    const std::vector<ProductionItem>& queue, std::uint32_t maximumTurns)
{
    std::vector<ProductionCompletionEstimate> result(queue.size());
    if (queue.empty()) return result;
    if (std::any_of(queue.begin(), queue.end(), [](const auto& item) { return item.automation != ProductionAutomation::None; }))
        maximumTurns = std::min(maximumTurns, 256U);
    Planet simulated = planet;
    simulated.observedHabitability.reset();
    simulated.productionQueue = queue;
    std::vector<std::size_t> origins(queue.size());
    std::iota(origins.begin(), origins.end(), 0);
    bool hasShipyard = colony_has_orbital_service(state, planet.id, planet.owner, OrbitalStationModule::Shipyard);
    bool engineeringSupport=engineering_station_available(state,planet);
    auto simulationState = state;
    const auto exotic = known_xenocrystal(state, planet.owner, planet.id);
    const bool receivedColony = exotic && exotic->colony && exotic->colony->owner == planet.owner;
    for (std::uint32_t offset = 1; offset <= maximumTurns && !simulated.productionQueue.empty(); ++offset) {
        simulationState.turn = state.turn + offset - 1;
        decay_planet_contamination(simulated);
        const auto mined = projected_mineral_mining(simulationState, simulated);
        simulated.minerals.ironium += mined.ironium;
        simulated.minerals.boranium += mined.boranium;
        simulated.minerals.germanium += mined.germanium;
        if (receivedColony) {
            const auto crystals = std::min({colony_xenocrystal_mining_rate(simulationState, simulated),
                simulated.xenocrystal.remaining, kMaximumXenocrystal - simulated.xenocrystal.surface});
            simulated.xenocrystal.remaining -= crystals;
            simulated.xenocrystal.surface += crystals;
        }
        const auto resolved = resolve_colony_production(simulationState, simulated, hasShipyard, engineeringSupport);
        hasShipyard = resolved.hasShipyard;
        engineeringSupport=resolved.engineeringSupport;
        for (const auto& completion : resolved.completions) {
            const auto original = origins[completion.originalIndex];
            const bool automatic = queue[original].automation != ProductionAutomation::None;
            if ((!automatic && completion.finishedBatch) || (automatic && !result[original].completionTurn))
                result[original].completionTurn = state.turn + offset;
        }
        auto previous = std::move(origins);
        origins.clear();
        for (const auto index : resolved.queueOrigins) origins.push_back(previous[index]);
        for(const auto original:previous) if(recovery_kind(queue[original].kind)
            && !result[original].completionTurn && !result[original].retiredTurn
            && std::find(origins.begin(),origins.end(),original)==origins.end())
                result[original].retiredTurn=state.turn+offset;
        const auto populationChange = projected_population_growth(simulationState, simulated, simulationState.turn);
        if (populationChange >= 0) simulated.population += static_cast<std::uint64_t>(populationChange);
        else simulated.population -= std::min(simulated.population, static_cast<std::uint64_t>(-populationChange));
        if (std::all_of(result.begin(), result.end(), [](const auto& estimate) { return estimate.completionTurn.has_value(); })) break;
    }
    for (const auto index : origins)
        if (!result[index].completionTurn) result[index].beyondForecastHorizon = true;
    return result;
}

} // namespace suns
