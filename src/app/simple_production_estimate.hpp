#pragma once

#include "suns/game_state.hpp"
#include "suns/stations.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace suns {

enum class SimpleProductionStatus {
    Approximate, AnnualRule, Nonlinear, Unavailable, NoProduction,
    MissingMinerals, MissingShipyard, EarlierUncertain, Overflow,
};

struct SimpleProductionEstimate {
    std::optional<std::uint64_t> completionTurn;
    SimpleProductionStatus status{SimpleProductionStatus::Nonlinear};
    std::uint64_t remainingWork{};
};

// A display estimate, not a future-turn simulation. The caller supplies the
// current production rate and received available minerals; neither increases
// while walking the queue. No growth, future mining, upkeep or new technology
// is predicted. Unknown/annual work prevents dates for the following prefix.
inline std::vector<SimpleProductionEstimate> simple_production_estimates(
    const GameState& state, const std::vector<ProductionItem>& queue,
    std::uint32_t productionPerYear, MineralCargo availableMinerals,
    bool shipyardAvailable)
{
    std::vector<SimpleProductionEstimate> result;
    result.reserve(queue.size());
    std::uint64_t accumulatedWork = 0;
    bool prefixKnown = true;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (const auto& item : queue) {
        SimpleProductionEstimate estimate;
        const auto units = std::uint64_t(item.quantity);
        estimate.remainingWork = item.remainingCost;
        if (units) estimate.remainingWork += (units - 1) * production_item_cost(state, item);
        const bool fixed = item.kind == ProductionKind::Factory || item.kind == ProductionKind::Mine
            || item.kind == ProductionKind::ColonyDefense || item.kind == ProductionKind::LocalRadar
            || item.kind == ProductionKind::PlanetaryRadar || item.kind == ProductionKind::ColonyShip
            || item.kind == ProductionKind::OrbitalStation;
        const auto* design = item.kind == ProductionKind::ColonyShip
            ? find_ship_design(state, item.shipDesign ? item.shipDesign : kColonyShipDesignId) : nullptr;
        const auto oneCost = production_item_mineral_cost(state, item);
        const MineralCargo batchCost{oneCost.ironium * units, oneCost.boranium * units,
            oneCost.germanium * units};
        const bool validCost = std::isfinite(batchCost.ironium) && batchCost.ironium >= 0
            && std::isfinite(batchCost.boranium) && batchCost.boranium >= 0
            && std::isfinite(batchCost.germanium) && batchCost.germanium >= 0;
        if (item.automation != ProductionAutomation::None) estimate.status = SimpleProductionStatus::AnnualRule;
        else if (!fixed) estimate.status = SimpleProductionStatus::Nonlinear;
        else if (!units || (item.kind == ProductionKind::ColonyShip
            && (!design || !ship_design_available_to_player(state, design->owner, *design))))
            estimate.status = SimpleProductionStatus::Unavailable;
        else if (item.kind == ProductionKind::ColonyShip && !shipyardAvailable)
            estimate.status = SimpleProductionStatus::MissingShipyard;
        else if (!prefixKnown) estimate.status = SimpleProductionStatus::EarlierUncertain;
        else if (!productionPerYear) estimate.status = SimpleProductionStatus::NoProduction;
        else if (!validCost || !mineral_cargo_sufficient(availableMinerals, batchCost))
            estimate.status = SimpleProductionStatus::MissingMinerals;
        else if (estimate.remainingWork > maximum - accumulatedWork)
            estimate.status = SimpleProductionStatus::Overflow;
        else {
            accumulatedWork += estimate.remainingWork;
            const auto years = std::max<std::uint64_t>(1, accumulatedWork / productionPerYear
                + (accumulatedWork % productionPerYear != 0));
            if (years > maximum - state.turn) estimate.status = SimpleProductionStatus::Overflow;
            else {
                estimate.status = SimpleProductionStatus::Approximate;
                estimate.completionTurn = state.turn + years;
                availableMinerals.ironium -= batchCost.ironium;
                availableMinerals.boranium -= batchCost.boranium;
                availableMinerals.germanium -= batchCost.germanium;
                if (item.kind == ProductionKind::OrbitalStation)
                    shipyardAvailable = !item.station || item.station->loadout.shipyard;
            }
        }
        if (!estimate.completionTurn) prefixKnown = false;
        result.push_back(estimate);
    }
    return result;
}

inline const char* simple_production_estimate_reason(SimpleProductionStatus status)
{
    switch (status) {
    case SimpleProductionStatus::Approximate: return "Approximate completion at the current fixed production rate and available mineral budget.";
    case SimpleProductionStatus::AnnualRule: return "Persistent annual rule; no future completion date is calculated.";
    case SimpleProductionStatus::Nonlinear: return "Completion depends on changing conditions or annual limits; resolved during actual turns.";
    case SimpleProductionStatus::Unavailable: return "This work is not currently available.";
    case SimpleProductionStatus::NoProduction: return "No current production remains after research and upkeep.";
    case SimpleProductionStatus::MissingMinerals: return "The received mineral budget does not cover this batch; future mining/deliveries are not predicted.";
    case SimpleProductionStatus::MissingShipyard: return "A shipyard is required.";
    case SimpleProductionStatus::EarlierUncertain: return "Earlier work has no simple completion estimate.";
    case SimpleProductionStatus::Overflow: return "Completion exceeds the supported date range.";
    }
    return "No simple completion estimate.";
}

} // namespace suns
