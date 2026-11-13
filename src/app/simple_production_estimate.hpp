#pragma once

#include "suns/game_state.hpp"
#include "suns/stations.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/production.hpp"

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
    Skipped, InactiveAnnualRule, HousingTooSmall,
};

struct SimpleProductionEstimate {
    std::optional<std::uint64_t> completionTurn;
    SimpleProductionStatus status{SimpleProductionStatus::Nonlinear};
    std::uint64_t remainingWork{};
    std::uint32_t effectiveQuantity{};
    std::uint32_t skippedQuantity{};
};

// A display estimate, not a future-turn simulation. The caller supplies the
// current production rate and received available minerals; neither increases
// while walking the queue. No growth, future mining, upkeep or new technology
// is predicted. Applicable unknown/annual work prevents dates for the following prefix.
inline std::vector<SimpleProductionEstimate> simple_production_estimates(
    const GameState& state, const Planet& planet, const std::vector<ProductionItem>& queue,
    std::uint32_t productionPerYear, MineralCargo availableMinerals,
    bool shipyardAvailable)
{
    std::vector<SimpleProductionEstimate> result;
    result.reserve(queue.size());
    std::uint64_t accumulatedWork = 0;
    bool prefixKnown = true;
    auto defenses = planet.defenses;
    auto radar = planet.radar;
    auto factories = std::uint64_t(planet.industry), mines = std::uint64_t(planet.mines);
    auto advancedFactories = std::uint64_t(planet.advancedFactories), advancedMines = std::uint64_t(planet.advancedMines);
    bool stationExists = shipyardAvailable || find_orbital_station_at_planet(state, planet.id)
        || orbital_base_at(state, planet.id);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const auto* owner = find_player(state, planet.owner);
    for (const auto& item : queue) {
        SimpleProductionEstimate estimate;
        const bool automatic = item.automation != ProductionAutomation::None;
        const bool factoryBuild = item.kind == ProductionKind::Factory || item.kind == ProductionKind::AdvancedFactory;
        const bool mineBuild = item.kind == ProductionKind::Mine || item.kind == ProductionKind::AdvancedMine;
        const bool housingBlocked = item.station && station_refit_over_capacity(state,planet,*item.station);
        const auto eligible = item.kind == ProductionKind::UpgradeFactory ? factories - std::min(factories, advancedFactories)
            : item.kind == ProductionKind::UpgradeMine ? mines - std::min(mines, advancedMines)
            : factoryBuild ? std::numeric_limits<std::uint32_t>::max() - factories
            : mineBuild ? std::numeric_limits<std::uint32_t>::max() - mines : maximum;
        // These are the resolver's current, free-retirement predicates. Retired
        // work must neither reserve minerals nor make a known prefix uncertain.
        const bool skipped = (owner && !production_kind_applicable(owner->race.primaryTrait, item.kind))
            || item.kind == ProductionKind::Research || item.quantity == 0
            || (prefixKnown && !automatic && !eligible)
            || (item.kind == ProductionKind::OrbitalStation && !housingBlocked && (item.station
                ? !station_construction_valid(state, planet, *item.station) : stationExists))
            || (item.kind == ProductionKind::LocalRadar && radar != PlanetaryRadarLevel::None)
            || (item.kind == ProductionKind::PlanetaryRadar && radar == PlanetaryRadarLevel::Full)
            || (item.kind == ProductionKind::ColonyDefense && (!colony_defenses_available(state, planet.owner)
                || (!automatic && defenses >= kMaximumColonyDefenses)));
        const bool inactiveAnnual = automatic && item.kind == ProductionKind::ColonyDefense
            && defenses >= kMaximumColonyDefenses;
        if (skipped || inactiveAnnual) {
            estimate.status = skipped ? SimpleProductionStatus::Skipped : SimpleProductionStatus::InactiveAnnualRule;
            estimate.skippedQuantity = skipped ? item.quantity : 0;
            result.push_back(estimate);
            continue;
        }
        auto effective = item.quantity;
        if (!automatic && prefixKnown) effective = std::uint32_t(std::min<std::uint64_t>(effective, eligible));
        if (!automatic && item.kind == ProductionKind::ColonyDefense)
            effective = std::min<std::uint32_t>(effective, kMaximumColonyDefenses - defenses);
        if (!automatic && (item.kind == ProductionKind::LocalRadar || item.kind == ProductionKind::PlanetaryRadar))
            effective = std::min<std::uint32_t>(effective, 1);
        estimate.effectiveQuantity = effective;
        estimate.skippedQuantity = item.quantity - effective;
        const auto units = std::uint64_t(effective);
        estimate.remainingWork = item.remainingCost;
        if (units) estimate.remainingWork += (units - 1) * production_item_cost(state, item);
        const bool fixed = item.kind == ProductionKind::Factory || item.kind == ProductionKind::Mine
            || item.kind == ProductionKind::AdvancedFactory || item.kind == ProductionKind::AdvancedMine
            || industry_upgrade_kind(item.kind)
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
        if (housingBlocked) estimate.status = SimpleProductionStatus::HousingTooSmall;
        else if (item.automation != ProductionAutomation::None) estimate.status = SimpleProductionStatus::AnnualRule;
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
                if (item.kind == ProductionKind::OrbitalStation) stationExists = true;
                if (item.kind == ProductionKind::ColonyDefense) defenses += effective;
                if (item.kind == ProductionKind::LocalRadar) radar = std::max(radar, PlanetaryRadarLevel::Local);
                if (item.kind == ProductionKind::PlanetaryRadar) radar = PlanetaryRadarLevel::Full;
                if (factoryBuild) factories += effective;
                if (mineBuild) mines += effective;
                if (item.kind == ProductionKind::AdvancedFactory || item.kind == ProductionKind::UpgradeFactory) advancedFactories += effective;
                if (item.kind == ProductionKind::AdvancedMine || item.kind == ProductionKind::UpgradeMine) advancedMines += effective;
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
    case SimpleProductionStatus::Skipped: return "Currently skipped without spending work or minerals under this civilization's rules and received technology, cap, radar or station conditions.";
    case SimpleProductionStatus::InactiveAnnualRule: return "Annual rule currently does no work at the defense cap; it remains active for future turns and conditions.";
    case SimpleProductionStatus::HousingTooSmall: return "Evacuate residents before reducing station housing. The queued refit retains its paid work until everyone fits.";
    }
    return "No simple completion estimate.";
}

} // namespace suns
