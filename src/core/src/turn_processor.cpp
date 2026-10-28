#include "suns/transport_lines.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/science_samples.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/survey_probes.hpp"
#include "suns/intergalactic_visitors.hpp"
#include "suns/turn_processor.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/mobile_base.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/deep_space.hpp"
#include "suns/planetary_recovery.hpp"
#include "suns/unknown_objects.hpp"
#include "suns/interstellar_medium.hpp"
#include "suns/combat.hpp"
#include "suns/stations.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/wormholes.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/production.hpp"
#include "suns/terraforming.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <iterator>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace suns {

std::uint32_t production_item_cost(const GameState& state, const ProductionItem& item)
{
    if (item.kind == ProductionKind::ResonanceLaboratory) return kResonanceWork;
    if (item.kind == ProductionKind::Research) return 0;
    if (item.kind == ProductionKind::Terraforming) return kTerraformingCost;
    if (item.kind == ProductionKind::Factory) return kFactoryCost;
    if (item.kind == ProductionKind::Mine) return kMineCost;
    if (recovery_kind(item.kind) || item.kind == ProductionKind::LocalRadar || item.kind==ProductionKind::PlanetaryRadar || item.kind == ProductionKind::ColonyDefense) return production_cost(item.kind);
    if (item.kind == ProductionKind::OrbitalStation) return item.station ? station_build_cost(item.station->loadout) : kOrbitalDockCost;
    if (item.shipDesign != 0) {
        if (const auto* design = find_ship_design(state, item.shipDesign)) return ship_design_cost(*design);
    }
    return kColonyShipCost;
}

namespace {

constexpr bool valid_research_field(ResearchField field)
{
    return static_cast<std::size_t>(field) < kResearchFieldCount;
}

Player* mutable_player(GameState& state, PlayerId id)
{
    const auto it = std::find_if(state.players.begin(), state.players.end(), [id](const Player& player) {
        return player.id == id;
    });
    return it == state.players.end() ? nullptr : &*it;
}

std::uint64_t research_event_id(
    PlayerId player, std::uint64_t turn, ResearchField field, std::uint8_t level)
{
    std::uint64_t hash = 1469598103934665603ULL;
    auto mix = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (value >> (byte * 8)) & 0xffULL;
            hash *= 1099511628211ULL;
        }
    };
    mix(static_cast<std::uint64_t>(GameEventKind::ResearchLevelCompleted));
    mix(player);
    mix(turn);
    mix(static_cast<std::uint64_t>(field));
    mix(level);
    return hash;
}

void apply_research_points(
    GameState& state,
    PlayerId playerId,
    std::uint32_t points,
    std::uint64_t completionTurn,
    std::vector<GameEvent>& events)
{
    auto* player = mutable_player(state, playerId);
    if (!player || !player->technology.researchActive) return;

    while (points > 0) {
        const auto field = player->technology.focus;
        if (!valid_research_field(field)) return;
        const auto index = static_cast<std::size_t>(field);
        const auto level = player->technology.levels[index];
        if (level == std::numeric_limits<std::uint8_t>::max()) return;

        const auto nextLevel = static_cast<std::uint8_t>(level + 1);
        const auto cost = research_level_cost(field, nextLevel);
        auto& progress = player->technology.progress[index];
        const auto remaining = cost > progress ? cost - progress : 0;
        const auto spent = std::min(points, remaining);
        progress += spent;
        points -= spent;
        if (progress < cost) break;

        progress = 0;
        player->technology.levels[index] = nextLevel;
        events.push_back({
            research_event_id(playerId, completionTurn, field, nextLevel),
            completionTurn,
            completionTurn,
            playerId,
            GameEventKind::ResearchLevelCompleted,
            GameEventSeverity::Information,
            0,
            0,
            0,
            0,
            ProductionKind::Research,
            {},
            0,
            SurveyLevel::Detected,
            field,
            nextLevel,
        });

        if (!player->technology.queuedFocuses.empty()) {
            player->technology.focus = player->technology.queuedFocuses.front();
            player->technology.queuedFocuses.erase(player->technology.queuedFocuses.begin());
        }
    }
}

void apply_precursor_research(
    GameState& state,
    PlayerId playerId,
    Planet& planet,
    Position sourcePosition,
    std::uint32_t points,
    ResearchField initialField)
{
    auto* player = mutable_player(state, playerId);
    if (!player) return;

    queue_player_report(
        state,
        playerId,
        PlayerReportKind::PrecursorArtifactsDiscovered,
        sourcePosition,
        state.turn + 1,
        planet.star,
        planet.id,
        0,
        0,
        ProductionKind::Research,
        planet.precursorArtifacts.researchPoints,
        initialField);

    while (points > 0) {
        const auto field = player->technology.focus;
        if (!valid_research_field(field)) return;
        const auto index = static_cast<std::size_t>(field);
        const auto level = player->technology.levels[index];
        if (level == std::numeric_limits<std::uint8_t>::max()) return;

        const auto nextLevel = static_cast<std::uint8_t>(level + 1);
        const auto cost = research_level_cost(field, nextLevel);
        auto& progress = player->technology.progress[index];
        const auto remaining = cost > progress ? cost - progress : 0;
        const auto spent = std::min(points, remaining);
        progress += spent;
        points -= spent;
        if (progress < cost) break;

        progress = 0;
        player->technology.levels[index] = nextLevel;
        queue_player_report(
            state,
            playerId,
            PlayerReportKind::ResearchLevelCompleted,
            sourcePosition,
            state.turn + 1,
            planet.star,
            planet.id,
            0,
            0,
            ProductionKind::Research,
            0,
            field,
            nextLevel);

        if (!player->technology.queuedFocuses.empty()) {
            player->technology.focus = player->technology.queuedFocuses.front();
            player->technology.queuedFocuses.erase(player->technology.queuedFocuses.begin());
        }
    }

}

FleetRole presentation_role_for_design(const ShipDesign& design)
{
    return ship_design_can_colonize(design) ? FleetRole::ColonyShip : FleetRole::Scout;
}

void sync_fleet_presentation(GameState& state, Fleet& fleet)
{
    normalize_fleet_composition(fleet);
    if (const auto* design = fleet_design(state, fleet)) {
        fleet.role = presentation_role_for_design(*design);
    }
}

bool design_name_exists(const GameState& state, PlayerId owner, const std::string& name)
{
    return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [&](const ShipDesign& design) {
        return design.owner == owner && design.name == name;
    });
}

void create_ship_design(GameState& state, PlayerId player, const CreateShipDesignOrder& order)
{
    ShipDesign candidate{state.nextShipDesignId, player, order.name,
        order.hull, order.components, order.placements};
    if (!ship_design_valid(candidate)
        || !ship_design_available_to_player(state, player, candidate)
        || design_name_exists(state, player, candidate.name)) return;
    normalize_ship_design_placement(candidate);
    state.shipDesigns.push_back(std::move(candidate));
    ++state.nextShipDesignId;
}

bool valid_mineral_cargo(const MineralCargo& cargo)
{
    return cargo.ironium >= 0.0 && cargo.boranium >= 0.0 && cargo.germanium >= 0.0;
}

bool minerals_available(const MineralCargo& colony, const MineralCargo& current, const MineralCargo& target)
{
    constexpr double epsilon = 0.000001;
    return target.ironium - current.ironium <= colony.ironium + epsilon
        && target.boranium - current.boranium <= colony.boranium + epsilon
        && target.germanium - current.germanium <= colony.germanium + epsilon;
}

void transfer_minerals(MineralCargo& colony, MineralCargo& fleet, const MineralCargo& target)
{
    colony.ironium += fleet.ironium - target.ironium;
    colony.boranium += fleet.boranium - target.boranium;
    colony.germanium += fleet.germanium - target.germanium;
    fleet = target;
}

std::vector<ColonyExtraction> mine_colonies(GameState& state)
{
    std::vector<ColonyExtraction> extraction;
    for (auto& planet : state.planets) {
        if (planet.owner == 0) continue;
        const auto mined = projected_mineral_mining(state, planet);
        extraction.push_back({planet.id, planet.owner, mined});
        planet.minerals.ironium += mined.ironium;
        planet.minerals.boranium += mined.boranium;
        planet.minerals.germanium += mined.germanium;
    }
    return extraction;
}

std::optional<std::uint32_t> complete_production(
    GameState& state, Planet& planet, const ProductionItem& item)
{
    if (item.kind == ProductionKind::Research) return std::nullopt;
    // Local buildings and physical environment are resolved by the shared queue engine.
    if (recovery_kind(item.kind) || item.kind == ProductionKind::Factory || item.kind == ProductionKind::Mine
        || item.kind == ProductionKind::Terraforming || item.kind == ProductionKind::ColonyDefense
        || item.kind == ProductionKind::LocalRadar || item.kind == ProductionKind::PlanetaryRadar) return FleetId{0};
    if (item.kind == ProductionKind::OrbitalStation) {
        if (item.station) {
            if (!complete_station_construction(state,planet,*item.station)) return std::nullopt;
            return find_orbital_station_at_planet(state,planet.id)->id;
        }
        if (find_orbital_station_at_planet(state, planet.id) || orbital_base_at(state,planet.id)) return std::nullopt;
        const auto id = state.nextOrbitalStationId++;
        state.orbitalStations.push_back({
            id,
            planet.owner,
            planet.id,
            planet.name + " Orbital Dock",
            OrbitalStationHullType::OrbitalDock,
            {OrbitalStationModule::Shipyard, OrbitalStationModule::RefuelingDepot},
        });
        return id;
    }

    if (item.kind != ProductionKind::ColonyShip) return std::nullopt;
    const auto designId = item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId;
    const auto* star = find_star(state, planet.star);
    const auto* design = find_ship_design(state, designId);
    if (!star || !design || design->owner != planet.owner || !ship_design_valid(*design)) return std::nullopt;

    const auto id = state.nextFleetId++;
    state.fleets.push_back({
        id,
        planet.owner,
        design->name + " " + std::to_string(id),
        presentation_role_for_design(*design),
        design->id,
        star->position,
        std::nullopt,
        ship_design_cruise_warp(*design),
        ship_design_fuel_capacity(*design),
        0,
    });
    state.fleets.back().ships = {{design->id, 1}};
    state.fleets.back().orbitalShipyardBirth = true;
    return id;
}

std::uint32_t run_colony_production(GameState& state, Planet& planet)
{
    if (planet.owner == 0) return 0;
    const bool wasWaitingForMinerals = planet.productionWaitingForMinerals;
    const bool wasWaitingForShipyard = planet.productionWaitingForShipyard;
    const auto resolved = resolve_colony_production(state, planet,
        colony_has_orbital_service(state, planet.id, planet.owner, OrbitalStationModule::Shipyard));
    const auto* star = find_star(state, planet.star);
    for (const auto& completion : resolved.completions) {
        const auto& completed = completion.item;
        if (engineering_kind(completed.kind) || completed.kind == ProductionKind::ResonanceLaboratory)
            continue; // Dedicated dated installation reports, never ship construction.
        const auto object = complete_production(state, planet, completed);
        if (!object || !star) continue;
        const auto design = completed.kind == ProductionKind::ColonyShip
            ? (completed.shipDesign != 0 ? completed.shipDesign : kColonyShipDesignId) : ShipDesignId{0};
        const auto quantity = reconstruction_kind(completed.kind) || completed.kind == ProductionKind::Factory || completed.kind == ProductionKind::Mine || completed.kind == ProductionKind::ColonyDefense
            ? completion.localQuantity : *object;
        queue_player_report(state, planet.owner, PlayerReportKind::ProductionCompleted,
            star->position, state.turn + 1, planet.star, planet.id,
            completed.kind == ProductionKind::ColonyShip ? *object : FleetId{0},
            design, completed.kind, quantity);
        if(recovery_kind(completed.kind))
            if(auto player=std::find_if(state.players.begin(),state.players.end(),[&](const auto& p){return p.id==planet.owner;}); player!=state.players.end())
                player->pendingPlayerReports.back().aftermath=observe_planet_aftermath(planet,true,state.turn+1);
    }
    if (star && ((planet.productionWaitingForMinerals && !wasWaitingForMinerals)
        || (planet.productionWaitingForShipyard && !wasWaitingForShipyard))) {
        const auto blocked = std::find_if(planet.productionQueue.begin(), planet.productionQueue.end(),
            [](const auto& item) { return item.automation == ProductionAutomation::None; });
        if (blocked != planet.productionQueue.end()) {
            const auto design = blocked->kind == ProductionKind::ColonyShip
                ? (blocked->shipDesign != 0 ? blocked->shipDesign : kColonyShipDesignId) : ShipDesignId{0};
            queue_player_report(state, planet.owner, planet.productionWaitingForShipyard
                ? PlayerReportKind::ProductionWaitingForShipyard : PlayerReportKind::ProductionWaitingForMinerals,
                star->position, state.turn + 1, planet.star, planet.id, 0, design, blocked->kind);
        }
    }
    return resolved.researchPoints;
}

bool fleet_at_planet(const GameState& state, const Fleet& fleet, const Planet& planet)
{
    const auto* star = find_star(state, planet.star);
    return star && same_position(star->position, fleet.position);
}

struct CargoEndpointRef {
    Planet* planet{};
    Fleet* fleet{};
    Position position;
    bool orbital{};
};

std::optional<CargoEndpointRef> resolve_cargo_endpoint(
    GameState& state, PlayerId player, CargoTransferEndpoint endpoint)
{
    if (endpoint.orbital && !endpoint.planet) return std::nullopt;
    if ((endpoint.planet == 0) == (endpoint.fleet == 0)) return std::nullopt;

    if (endpoint.planet != 0) {
        const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
            return candidate.id == endpoint.planet;
        });
        if (planet == state.planets.end()) return std::nullopt;
        const auto* star = find_star(state, planet->star);
        if (!star) return std::nullopt;
        if(endpoint.orbital) {
            if(planet->owner!=player || !habitat_completed(*planet)) return std::nullopt;
            if(state.mediumObserver) {
                const auto* owner=find_player(state,player);
                const auto evidence=owner?std::find_if(owner->surveyKnowledge.begin(),owner->surveyKnowledge.end(),[&](const auto& k){return k.star==planet->star;}):std::vector<SystemSurveyKnowledge>::const_iterator{};
                if(!owner || evidence==owner->surveyKnowledge.end() || evidence->observedTurn!=state.turn || !evidence->planetSnapshot || !evidence->planetSnapshot->habitat) return std::nullopt;
            }
        }
        return CargoEndpointRef{&*planet, nullptr, star->position,endpoint.orbital};
    }

    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == endpoint.fleet && candidate.owner == player;
    });
    if (fleet == state.fleets.end()) return std::nullopt;
    return CargoEndpointRef{nullptr, &*fleet, fleet->position};
}

std::uint64_t ground_combat_random(
    const GameState& state,
    const Fleet& fleet,
    const Planet& planet,
    std::uint64_t attackers,
    std::uint64_t defenders,
    std::uint64_t salt)
{
    auto value = state.galaxySeed ^ (state.turn * 0x9e3779b97f4a7c15ULL);
    const auto mix = [&](std::uint64_t part) {
        value ^= part + 0x9e3779b97f4a7c15ULL + (value << 6U) + (value >> 2U);
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        value ^= value >> 31U;
    };
    mix(fleet.id);
    mix(fleet.owner);
    mix(planet.id);
    mix(planet.owner);
    mix(attackers);
    mix(defenders);
    mix(salt);
    return value;
}

double unit_random(std::uint64_t value)
{
    return static_cast<double>(value >> 11U) * (1.0 / 9007199254740992.0);
}

std::uint32_t report_population(std::uint64_t population)
{
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        population, static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())));
}

bool hostile_station_blocks_landing(GameState& state, const Fleet& fleet, const Planet& planet)
{
    const auto* station = find_orbital_station_at_planet(state, planet.id);
    const auto* base = orbital_base_at(state, planet.id);
    if ((!station || station->owner == fleet.owner) && (!base || base->owner == fleet.owner)) return false;
    // Orbital control is required even against an unarmed dock. A failed
    // landing commits neither colonists nor minerals to the surface.
    const auto* player = find_player(state, fleet.owner);
    const bool alreadyReported = player && std::any_of(player->pendingPlayerReports.begin(),
        player->pendingPlayerReports.end(), [&](const auto& report) {
            return report.kind == PlayerReportKind::LandingBlocked && report.fleet == fleet.id
                && report.planet == planet.id && report.observedTurn == state.turn + 1;
        });
    if (!alreadyReported) {
        queue_player_report(state, fleet.owner, PlayerReportKind::LandingBlocked,
            fleet.position, state.turn + 1, planet.star, planet.id, fleet.id, fleet.design);
    }
    return true;
}

// floor(losses * group / total), without overflowing and without platform-
// dependent floating point rounding. The winning force always loses <= total.
std::uint64_t proportional_population_loss(std::uint64_t losses,std::uint64_t group,std::uint64_t total)
{
    std::uint64_t result=0,remainder=0;
    for(int bit=63;bit>=0;--bit) {
        result*=2;
        if(remainder>=total-remainder) {remainder-=total-remainder;++result;}
        else remainder*=2;
        if((group>>bit)&1U) {
            if(remainder>=total-losses) {remainder-=total-losses;++result;}
            else remainder+=losses;
        }
    }
    return result;
}

bool invade_colony(
    GameState& state, Fleet& fleet, Planet& planet, std::uint64_t attackingPopulation)
{
    if (attackingPopulation == 0 || attackingPopulation > fleet.colonists
        || planet.owner == 0 || planet.owner == fleet.owner
        || !fleet_at_planet(state, fleet, planet)) {
        return false;
    }

    if (hostile_station_blocks_landing(state, fleet, planet)) return false;
    const auto defender = planet.owner;
    const auto defendingPopulation = total_colony_population(planet);
    const auto chance = ground_invasion_success_chance(attackingPopulation, defendingPopulation);
    const auto outcomeRoll = unit_random(ground_combat_random(
        state, fleet, planet, attackingPopulation, defendingPopulation, 0x494e564153494f4eULL));
    const bool attackerWon = outcomeRoll < chance;
    const auto casualtyFactor = 0.65 + 0.35 * unit_random(ground_combat_random(
        state, fleet, planet, attackingPopulation, defendingPopulation, 0x43415355414c5459ULL));

    fleet.colonists -= attackingPopulation;
    std::uint64_t survivors{};
    if (attackerWon) {
        const auto losses = std::min(
            attackingPopulation - 1,
            static_cast<std::uint64_t>(std::llround(
                static_cast<long double>(defendingPopulation) * casualtyFactor)));
        survivors = attackingPopulation - losses;
        planet.owner = fleet.owner;
        for (auto& sample : planet.scienceSamples) sample.retained = false;
        planet.radar = PlanetaryRadarLevel::None;
        planet.homeworld = false;
        planet.population = survivors;
        planet.habitat.population=0; planet.habitat.service=HabitatService::Empty; planet.habitat.productionPaid=0;
        planet.productionQueue.clear();
        // A settlement default is not a conquest program. Captured worlds
        // await explicit production orders even when that default is selected.
        planet.productionWaitingForMinerals = false;
        planet.productionWaitingForShipyard = false;
        set_survey_level(
            state, fleet.owner, planet.star, SurveyLevel::GeologicalSurvey, state.turn + 1);
    } else {
        const auto losses = defendingPopulation == 0 ? std::uint64_t{0} : std::min(
            defendingPopulation - 1,
            static_cast<std::uint64_t>(std::llround(
                static_cast<long double>(attackingPopulation) * casualtyFactor)));
        survivors = defendingPopulation - losses;
        const auto orbitalLoss = defendingPopulation ? proportional_population_loss(losses,planet.habitat.population,defendingPopulation):0;
        planet.habitat.population-=orbitalLoss;
        planet.population-=losses-orbitalLoss;
    }

    if (const auto* star = find_star(state, planet.star)) {
        const auto attackerLosses = attackingPopulation - (attackerWon ? survivors : 0);
        const auto defenderLosses = defendingPopulation - (attackerWon ? 0 : survivors);
        queue_player_report(
            state,
            fleet.owner,
            attackerWon ? PlayerReportKind::GroundInvasionWon : PlayerReportKind::GroundInvasionLost,
            star->position,
            state.turn + 1,
            planet.star,
            planet.id,
            fleet.id,
            fleet.design,
            ProductionKind::ColonyShip,
            report_population(survivors), ResearchField::Electronics, 0, {}, 0, {}, 0,
            OperationalLossReport{OperationalLossKind::Ground, attackerLosses, defenderLosses});
        queue_player_report(
            state,
            defender,
            attackerWon ? PlayerReportKind::ColonyLost : PlayerReportKind::GroundDefenseWon,
            star->position,
            state.turn + 1,
            planet.star,
            planet.id,
            fleet.id,
            fleet.design,
            ProductionKind::ColonyShip,
            report_population(survivors), ResearchField::Electronics, 0, {}, 0, {}, 0,
            OperationalLossReport{OperationalLossKind::Ground, defenderLosses, attackerLosses});
    }
    return true;
}

std::uint64_t& endpoint_colonists(const CargoEndpointRef& endpoint)
{
    return endpoint.planet ? (endpoint.orbital?endpoint.planet->habitat.population:endpoint.planet->population) : endpoint.fleet->colonists;
}

MineralCargo& endpoint_minerals(CargoEndpointRef& endpoint)
{
    return endpoint.planet ? endpoint.planet->minerals : endpoint.fleet->minerals;
}

void record_freight_delivery(std::vector<ColonyFreightDelivery>& freight, const Planet& planet,
    MineralCargo minerals, std::uint64_t colonists = 0)
{
    if (planet.owner == 0 || (colonists == 0 && mineral_cargo_mass(minerals) <= 0.0)) return;
    freight.push_back({planet.id, planet.owner, minerals, colonists});
}

bool transfer_cargo(GameState& state, PlayerId player, const TransferCargoOrder& order,
    std::vector<ColonyFreightDelivery>& freight)
{
    constexpr double epsilon = 0.000001;
    auto source = resolve_cargo_endpoint(state, player, order.source);
    auto destination = resolve_cargo_endpoint(state, player, order.destination);
    if (!source || !destination || !same_position(source->position, destination->position)) return false;
    if ((source->planet && destination->planet && source->planet == destination->planet && source->orbital==destination->orbital)
        || (source->fleet && destination->fleet && source->fleet == destination->fleet)) {
        return false;
    }
    if (!valid_mineral_cargo(order.minerals)) return false;
    if(source->orbital || destination->orbital) {
        if(mineral_cargo_mass(order.minerals)>epsilon) return false;
        if(source->planet && destination->planet && (!engineering_station_available(state,*source->planet)
            || !engineering_operation_available(state,*source->planet))) return false;
        if(destination->orbital && order.colonists>kOrbitalPopulationCapacity-destination->planet->habitat.population) return false;
    }

    const bool enemyDestination = destination->planet
        && destination->planet->owner != 0
        && destination->planet->owner != player;
    if (enemyDestination) {
        // Population may be unloaded onto an enemy colony only as an explicit
        // ground invasion. Minerals never move as a side effect of combat.
        if (!source->fleet || source->fleet->owner != player || order.colonists == 0
            || mineral_cargo_mass(order.minerals) > epsilon) {
            return false;
        }
        return invade_colony(state, *source->fleet, *destination->planet, order.colonists);
    }

    if (source->planet && source->planet->owner != 0 && source->planet->owner != player) {
        return false;
    }

    // Population belongs only to friendly colonies. Loading from a colony
    // retains one colonist, matching the existing exact-manifest order.
    if (order.colonists > 0) {
        if ((source->planet && source->planet->owner != player)
            || (destination->planet && destination->planet->owner != player)) {
            return false;
        }
        const auto available = endpoint_colonists(*source);
        const bool retainLastSurface=source->planet && !source->orbital && !source->planet->habitat.population && !destination->orbital;
        if (retainLastSurface ? order.colonists >= available : order.colonists > available) return false;
        const auto destinationPopulation = endpoint_colonists(*destination);
        if (order.colonists > std::numeric_limits<std::uint64_t>::max() - destinationPopulation) return false;
        if (source->fleet && destination->planet
            && hostile_station_blocks_landing(state, *source->fleet, *destination->planet)) return false;
    }

    auto& sourceMinerals = endpoint_minerals(*source);
    if (!mineral_cargo_sufficient(sourceMinerals, order.minerals)) return false;

    if (destination->fleet) {
        const auto transferredCargo = colonist_cargo_mass(order.colonists)
            + mineral_cargo_mass(order.minerals);
        if (fleet_cargo_used(state, *destination->fleet) + transferredCargo
            > fleet_cargo_capacity(state, *destination->fleet) + epsilon) {
            return false;
        }
    }

    // All validation is complete: apply the four resources atomically.
    if (order.colonists > 0) {
        endpoint_colonists(*source)-=order.colonists;
        endpoint_colonists(*destination)+=order.colonists;
    }

    auto& destinationMinerals = endpoint_minerals(*destination);
    subtract_minerals(sourceMinerals, order.minerals);
    destinationMinerals.ironium += order.minerals.ironium;
    destinationMinerals.boranium += order.minerals.boranium;
    destinationMinerals.germanium += order.minerals.germanium;
    if (source->fleet && destination->planet)
        record_freight_delivery(freight, *destination->planet, order.minerals, order.colonists);
    return true;
}

std::vector<RemoteExtraction> mine_uncolonized_planets(GameState& state)
{
    std::vector<RemoteExtraction> extraction;
    for (const auto& fleet : state.fleets) {
        if (fleet.task != FleetTask::RemoteMining) continue;
        for (auto& planet : state.planets) {
            if (planet.owner != 0 || !fleet_at_planet(state, fleet, planet)) continue;
            for (const auto& stack : fleet_ship_stacks(fleet)) {
                const auto* design = find_ship_design(state, stack.design);
                if (!design || !ship_design_available_to_player(state, fleet.owner, *design)
                    || !ship_design_can_remote_mine(*design)) continue;
                const auto mined = projected_remote_mining(state, planet, *design);
                const MineralCargo output{mined.ironium * stack.count,
                    mined.boranium * stack.count, mined.germanium * stack.count};
                planet.minerals.ironium += output.ironium;
                planet.minerals.boranium += output.boranium;
                planet.minerals.germanium += output.germanium;
                if (mineral_cargo_mass(output) > 0.0)
                    extraction.push_back({planet.id, fleet.owner, output});
            }
            break;
        }
    }
    return extraction;
}

Planet* friendly_colony_at_fleet(GameState& state, const Fleet& fleet)
{
    const auto it = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
        return planet.owner == fleet.owner && fleet_at_planet(state, fleet, planet);
    });
    return it == state.planets.end() ? nullptr : &*it;
}

bool establish_colony(GameState& state, Fleet& fleet, Planet& planet,
    const std::optional<ProductionTemplate>& colonyPlan)
{
    if (colonyPlan && !production_template_valid(*colonyPlan)) return false;
    // A manual founding order cannot dismantle a fleet to evade an armed
    // hostile encounter already present at the planning boundary.
    if (std::any_of(state.fleets.begin(), state.fleets.end(), [&](const auto& other) {
            return other.owner != fleet.owner
                && distance_between(fleet.position, other.position) <= kFleetEncounterRadius
                && (fleet_has_space_weapons(state, fleet) || fleet_has_space_weapons(state, other));
        })) return false;
    if (planet.owner != 0 || fleet.colonists == 0 || !fleet_can_colonize(state, fleet)) return false;
    // A fleet physically in orbit can survey and settle locally without
    // waiting for the observation to reach the empire's communication mesh.
    if (fleet_cargo_used(state, fleet) > fleet_cargo_capacity(state, fleet) + 0.000001) return false;
    if (planet.scienceSamples.size() + fleet.scienceSamples.size() > kMaximumScienceSamples) return false;
    if (fleet.xenocrystal > kMaximumXenocrystal - planet.xenocrystal.surface) return false;
    if (!fleet_at_planet(state, fleet, planet)) return false;
    if (hostile_station_blocks_landing(state, fleet, planet)) return false;

    const auto stacks = fleet_ship_stacks(fleet);
    const auto colonizer = std::find_if(stacks.begin(), stacks.end(),
        [&](const FleetShipStack& stack) {
            const auto* design = find_ship_design(state, stack.design);
            return design && ship_design_can_colonize(*design);
        });
    if (colonizer == stacks.end()) return false;
    const auto colonizerDesign = colonizer->design;
    const auto salvage = fleet_colonization_salvage(state, fleet);

    planet.owner = fleet.owner;
    planet.radar = PlanetaryRadarLevel::None;
    planet.homeworld = false;
    planet.population = fleet.colonists;
    planet.minerals.ironium += fleet.minerals.ironium + salvage.ironium;
    planet.minerals.boranium += fleet.minerals.boranium + salvage.boranium;
    planet.minerals.germanium += fleet.minerals.germanium + salvage.germanium;
    (void)unload_science_samples(state, planet, fleet, state.turn + 1);
    (void)move_xenocrystals(fleet.xenocrystal, planet.xenocrystal.surface, fleet.xenocrystal);
    // Settlers inherit the physical surviving installations and war damage.
    planet.productionQueue.clear();
    // Founding does not imply a radar or an automatic build programme. Nearby
    // colonies may be controlled through the existing homeworld radar field;
    // remote colonies use only an explicitly assigned autonomous template.
    if (colonyPlan) planet.productionQueue = colonyPlan->items;
    planet.productionWaitingForMinerals = false;
    const auto* foundedStar=find_star(state,planet.star);
    if(foundedStar) queue_survey_report(state,fleet.owner,planet.star,0,foundedStar->position,
        state.turn+1,SurveyLevel::GeologicalSurvey);
    const auto* star = find_star(state, planet.star);
    if (star) {
        queue_player_report(
            state,
            fleet.owner,
            PlayerReportKind::ColonyFounded,
            star->position,
            state.turn + 1,
            planet.star,
            planet.id,
            fleet.id,
            colonizerDesign);

        auto& artifacts = planet.precursorArtifacts;
        if (artifacts.present && !artifacts.claimed) {
            artifacts.claimed = true;
            artifacts.discoveredBy = fleet.owner;
            const auto* player = find_player(state, fleet.owner);
            const auto field = player ? player->technology.focus : ResearchField::Electronics;
            apply_precursor_research(
                state,
                fleet.owner,
                planet,
                star->position,
                artifacts.researchPoints,
                field);
        }
    }

    // Successful colonization dismantles the entire fleet. Cargo minerals are
    // deposited in full above; fuel and construction resources are otherwise
    // lost. The caller removes the now-consumed FleetId atomically.
    return true;
}

std::optional<FleetArrivalAction> active_arrival_action(FleetArrivalAction action)
{
    return action.kind == FleetArrivalActionKind::None
        ? std::optional<FleetArrivalAction>{}
        : std::optional<FleetArrivalAction>{action};
}

bool execute_arrival_action(GameState& state, Fleet& fleet,
    std::vector<ColonyFreightDelivery>& freight)
{
    if (!fleet.arrivalAction) return false;

    const auto action = *fleet.arrivalAction;
    fleet.arrivalAction.reset();
    if (action.kind == FleetArrivalActionKind::None) return false;
    if (const auto task = arrival_work_task(action.kind); task && *task != FleetTask::RemoteMining) {
        if (fleet_task_equipment_available(state, fleet, *task)) fleet.task = *task;
        return false;
    }
    // Keep a blocked colonization programme visible and retryable instead
    // of erasing its last waypoint and silently discarding the order.
    if (action.kind == FleetArrivalActionKind::Colonize) {
        const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
            return candidate.owner == 0 && fleet_at_planet(state, fleet, candidate);
        });
        if (planet == state.planets.end() || !establish_colony(state, fleet, *planet, action.colonyPlan)) {
            fleet.arrivalAction = action;
            fleet.destination = fleet.position;
            return false;
        }
        return true;
    }

    switch (action.kind) {
    case FleetArrivalActionKind::EnterWormhole:
        return false; // Handled before ordinary arrival actions.
    case FleetArrivalActionKind::LayMinefields:
    case FleetArrivalActionKind::SweepMinefields:
    case FleetArrivalActionKind::BombardColony:
    case FleetArrivalActionKind::FieldResearch:
    case FleetArrivalActionKind::RecoverWreckage:
    case FleetArrivalActionKind::RecoverSample:
    case FleetArrivalActionKind::DeployOutpost:
        return false; // Persistent work handled before this switch.
    case FleetArrivalActionKind::None:
        return false;
    case FleetArrivalActionKind::LoadAllAvailable: {
        const auto capacity = fleet_cargo_capacity(state, fleet);
        const auto freeSpace = std::max(0.0, capacity - fleet_cargo_used(state, fleet));
        if (action.cargo == FleetCargoKind::Colonists || action.cargo==FleetCargoKind::OrbitalColonists) {
            auto* colony = friendly_colony_at_fleet(state, fleet);
            if (!colony) return false;
            const bool orbital=action.cargo==FleetCargoKind::OrbitalColonists;
            if(orbital && !habitat_completed(*colony)) return false;
            auto& population=orbital?colony->habitat.population:colony->population;
            const auto reserve=orbital?action.reservePopulation:std::max<std::uint64_t>(1,action.reservePopulation);
            if(population<=reserve) return false;
            const auto available=population-reserve;
            const auto capacityColonists = static_cast<std::uint64_t>(
                std::floor(freeSpace * kColonistsPerCargoUnit + 0.000001));
            const auto load = std::min(available, capacityColonists);
            population -= load;
            fleet.colonists += load;
            return false;
        }

        auto surface = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
            return (planet.owner == 0 || planet.owner == fleet.owner) && fleet_at_planet(state, fleet, planet);
        });
        if (surface == state.planets.end()) return false;
        if (action.cargo == FleetCargoKind::Xenocrystal) {
            const auto known = known_xenocrystal(state, fleet.owner, surface->id);
            if (!known) return false;
            const auto load = std::min({surface->xenocrystal.surface, kMaximumXenocrystal - fleet.xenocrystal,
                std::uint32_t(std::min(double(kMaximumXenocrystal), std::floor(freeSpace + 1e-6)))});
            (void)move_xenocrystals(surface->xenocrystal.surface, fleet.xenocrystal, load);
            if (load) queue_player_report(state, fleet.owner, PlayerReportKind::XenocrystalAcquired, fleet.position,
                state.turn + 1, surface->star, surface->id, fleet.id, 0, ProductionKind::Research, load);
            return false;
        }
        double* source{};
        double* destination{};
        switch (action.cargo) {
        case FleetCargoKind::OrbitalColonists: break;
        case FleetCargoKind::Colonists: break;
        case FleetCargoKind::Ironium: source = &surface->minerals.ironium; destination = &fleet.minerals.ironium; break;
        case FleetCargoKind::Boranium: source = &surface->minerals.boranium; destination = &fleet.minerals.boranium; break;
        case FleetCargoKind::Germanium: source = &surface->minerals.germanium; destination = &fleet.minerals.germanium; break;
        case FleetCargoKind::All: break;
        case FleetCargoKind::Xenocrystal: break;
        }
        if (!source || !destination) return false;
        const auto load = std::min(*source, freeSpace);
        *source -= load;
        *destination += load;
        return false;
    }
    case FleetArrivalActionKind::UnloadAll: {
        if(action.cargo==FleetCargoKind::OrbitalColonists) {
            auto* colony=friendly_colony_at_fleet(state,fleet);
            if(!colony || !habitat_completed(*colony) || hostile_station_blocks_landing(state,fleet,*colony)) return false;
            const auto unload=std::min(fleet.colonists,kOrbitalPopulationCapacity-colony->habitat.population);
            colony->habitat.population+=unload; fleet.colonists-=unload;
            record_freight_delivery(freight,*colony,{},unload); return false;
        }
        if (action.cargo == FleetCargoKind::Colonists || action.cargo == FleetCargoKind::All) {
            auto* colony = friendly_colony_at_fleet(state, fleet);
            if (colony) {
                if (fleet.colonists > 0 && hostile_station_blocks_landing(state, fleet, *colony)) return false;
                record_freight_delivery(freight, *colony, {}, fleet.colonists);
                colony->population += fleet.colonists;
                fleet.colonists = 0;
            } else {
                const auto enemy = std::find_if(
                    state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
                        return planet.owner != 0 && planet.owner != fleet.owner
                            && fleet_at_planet(state, fleet, planet);
                    });
                if (enemy != state.planets.end() && fleet.colonists > 0) {
                    (void)invade_colony(state, fleet, *enemy, fleet.colonists);
                } else if (action.cargo == FleetCargoKind::Colonists) {
                    return false;
                }
            }
        }
        auto surface = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
            return (planet.owner == 0 || planet.owner == fleet.owner) && fleet_at_planet(state, fleet, planet);
        });
        if (surface == state.planets.end()) return false;
        MineralCargo delivered;
        switch (action.cargo) {
        case FleetCargoKind::OrbitalColonists: break;
        case FleetCargoKind::Colonists: break;
        case FleetCargoKind::Ironium:
            delivered.ironium = fleet.minerals.ironium;
            surface->minerals.ironium += fleet.minerals.ironium;
            fleet.minerals.ironium = 0.0;
            break;
        case FleetCargoKind::Boranium:
            delivered.boranium = fleet.minerals.boranium;
            surface->minerals.boranium += fleet.minerals.boranium;
            fleet.minerals.boranium = 0.0;
            break;
        case FleetCargoKind::Germanium:
            delivered.germanium = fleet.minerals.germanium;
            surface->minerals.germanium += fleet.minerals.germanium;
            fleet.minerals.germanium = 0.0;
            break;
        case FleetCargoKind::All:
            delivered = fleet.minerals;
            surface->minerals.ironium += fleet.minerals.ironium;
            surface->minerals.boranium += fleet.minerals.boranium;
            surface->minerals.germanium += fleet.minerals.germanium;
            fleet.minerals = {};
            if (surface->owner == fleet.owner) {
                (void)unload_science_samples(state, *surface, fleet, state.turn + 1);
            }
            break;
        case FleetCargoKind::Xenocrystal:
            break;
        }
        if (action.cargo == FleetCargoKind::Xenocrystal || action.cargo == FleetCargoKind::All) {
            const auto amount = std::min(fleet.xenocrystal, kMaximumXenocrystal - surface->xenocrystal.surface);
            (void)move_xenocrystals(fleet.xenocrystal, surface->xenocrystal.surface, amount);
            if (amount) queue_player_report(state, fleet.owner, PlayerReportKind::XenocrystalAcquired, fleet.position,
                state.turn + 1, surface->star, surface->id, fleet.id, 0, ProductionKind::Research, amount);
        }
        if (surface->owner == fleet.owner)
            record_freight_delivery(freight, *surface, delivered);
        return false;
    }
    case FleetArrivalActionKind::Refuel: {
        auto* colony = friendly_colony_at_fleet(state, fleet);
        if (!colony || !colony_has_orbital_service(
                state, colony->id, fleet.owner, OrbitalStationModule::RefuelingDepot)) {
            refuel_from_outpost(state, fleet);
            return false;
        }
        fleet.fuel = fleet_fuel_capacity(state, fleet);
        return false;
    }
    case FleetArrivalActionKind::Colonize: {
        return false; // Handled before this switch, including blocked orders.
    }
    case FleetArrivalActionKind::RemoteMining: {
        if (!fleet_can_remote_mine(state, fleet)) return false;
        const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
            return candidate.owner == 0 && fleet_at_planet(state, fleet, candidate);
        });
        if (planet == state.planets.end()) return false;
        fleet.task = FleetTask::RemoteMining;
        return false;
    }
    case FleetArrivalActionKind::MergeWithFleet:
        // Moving-target rendezvous resolves this action after both fleets have
        // completed their deterministic movement for the turn.
        return false;
    case FleetArrivalActionKind::SupplyOutpost:
        supply_outpost(state, fleet, action.reserveFuel);
        return false;
    }
    return false;
}

void activate_next_waypoint(GameState& state, Fleet& fleet)
{
    while (!fleet.destination && !fleet.waypointQueue.empty()) {
        const auto waypoint = fleet.waypointQueue.front();
        fleet.waypointQueue.erase(fleet.waypointQueue.begin());

        if (!fleet_warp_valid(state, fleet, waypoint.warp)) {
            fleet.waypointQueue.clear();
            fleet.arrivalAction.reset();
            fleet.repeatOrders = false;
            fleet.routeTemplate.clear();
            fleet.targetFleet = 0;
            return;
        }

        fleet.warp = waypoint.warp;
        fleet.arrivalAction = active_arrival_action(waypoint.arrivalAction);
        fleet.targetFleet = waypoint.targetFleet;

        if (waypoint.targetFleet != 0) {
            const auto target = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
                return candidate.id == waypoint.targetFleet && candidate.owner == fleet.owner;
            });
            fleet.destination = target != state.fleets.end() ? target->position : waypoint.destination;
            return;
        }

        if (!same_position(fleet.position, waypoint.destination)) {
            fleet.destination = waypoint.destination;
            return;
        }

        if (fleet.arrivalAction) {
            fleet.destination = waypoint.destination;
            return;
        }
    }
}

void generate_fleet_fuel(GameState& state)
{
    for (auto& fleet : state.fleets) {
        const auto capacity = fleet_fuel_capacity(state, fleet);
        if (capacity <= 0.0) {
            fleet.fuel = 0.0;
            continue;
        }
        fleet.fuel = std::min(capacity, fleet.fuel + fleet_fuel_generation(state, fleet));
    }
}

void refuel_fleets_at_orbital_services(GameState& state)
{
    for (auto& fleet : state.fleets) {
        const auto* colony = friendly_colony_at_fleet(state, fleet);
        if (!colony || !colony_has_orbital_service(
                state, colony->id, fleet.owner, OrbitalStationModule::RefuelingDepot)) {
            continue;
        }
        fleet.fuel = fleet_fuel_capacity(state, fleet);
    }
}

double fleet_field_repair_rate(const GameState& state, const Fleet& fleet)
{
    const auto shipCount = fleet_ship_count(fleet);
    if (shipCount == 0) return 0.0;
    double total = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        if (!design) continue;
        double perShip = 0.0;
        for (const auto component : design->components) {
            // A second bay on the same hull does not double its capacity.
            perShip = std::max(perShip, component_spec(component).fieldRepairPerTurn);
        }
        total += perShip * stack.count;
    }
    return total / shipCount;
}

void queue_fleet_movement_report(
    GameState& state, const Fleet& fleet, PlayerReportKind kind, std::uint64_t observationTurn);

void repair_fleet_damage(GameState& state, const GameState& previous)
{
    constexpr double shipyardRepairPerTurn = 20.0;
    for (auto& fleet : state.fleets) {
        if (fleet.damagePercent <= 0.0) continue;
        const auto* colony = friendly_colony_at_fleet(state, fleet);
        const bool atShipyard = colony && colony_has_orbital_service(
            state, colony->id, fleet.owner, OrbitalStationModule::Shipyard);
        double hullBonus = 0.0;
        const auto before = std::find_if(previous.fleets.begin(), previous.fleets.end(),
            [&](const auto& candidate) { return candidate.id == fleet.id; });
        if (before != previous.fleets.end() && same_position(before->position, fleet.position)
            && !fleet.destination && fleet.targetFleet == 0)
            for (const auto& stack : fleet_ship_stacks(fleet))
                if (const auto* design = find_ship_design(state, stack.design))
                    hullBonus = std::max(hullBonus, hull_spec(design->hull).fleetRepairBonus);
        const auto repair = (atShipyard ? shipyardRepairPerTurn : fleet_field_repair_rate(state, fleet)) + hullBonus;
        const bool immobilized = fleet.damagePercent >= 100.0;
        fleet.damagePercent = std::max(0.0, fleet.damagePercent - repair);
        if (immobilized && fleet.damagePercent < 100.0)
            queue_fleet_movement_report(
                state, fleet, PlayerReportKind::FleetMobilityRestored, state.turn + 1);
    }
}

bool fleet_ready_for_reorganization(const Fleet& fleet)
{
    if (fleet.outpost != OutpostKind::None) return false;
    return !fleet.destination
        && fleet.targetFleet == 0
        && fleet.waypointQueue.empty()
        && fleet.pendingCommands.empty()
        && fleet.task == FleetTask::None
        && !fleet.repeatOrders
        && fleet.routeTemplate.empty();
}

void refresh_merged_fleet_manifest(Fleet& fleet)
{
    // A merge keeps the destination FleetId so its route and every reference
    // to it remain valid. Give the result an unambiguous fleet name and update
    // the last confirmed manifest immediately: otherwise a delayed telemetry
    // snapshot can make the absorbed ships appear to have vanished.
    fleet.name = "Fleet #" + std::to_string(fleet.id);
    fleet.telemetry.ships = fleet.ships;
    fleet.telemetry.fuel = fleet.fuel;
    fleet.telemetry.damagePercent = fleet.damagePercent;
    fleet.telemetry.colonists = fleet.colonists;
    fleet.telemetry.minerals = fleet.minerals;
    fleet.telemetryInTransit.clear();
}

bool merge_fleet_special_cargo(GameState& state, Fleet& destination, const Fleet& source)
{
    if (source.xenocrystal > kMaximumXenocrystal - destination.xenocrystal) return false;
    if (destination.scienceSamples.size() + source.scienceSamples.size() > kMaximumScienceSamples) return false;
    auto knownSamples = confirmed_fleet_telemetry(state, destination).scienceSamples;
    for (const auto& sample : confirmed_fleet_telemetry(state, source).scienceSamples)
        if (std::none_of(knownSamples.begin(), knownSamples.end(), [&](const auto& s) { return s.source == sample.source; }))
            knownSamples.push_back(sample);
    // Historical inventories may together exceed the per-manifest bound.
    // Keep the destination's dated evidence until the combined report arrives.
    if (knownSamples.size() <= kMaximumScienceSamples) destination.telemetry.scienceSamples = std::move(knownSamples);
    const auto knownCrystals = std::uint64_t(confirmed_fleet_telemetry(state, destination).xenocrystal)
        + confirmed_fleet_telemetry(state, source).xenocrystal;
    if (knownCrystals <= kMaximumXenocrystal) destination.telemetry.xenocrystal = std::uint32_t(knownCrystals);
    destination.xenocrystal += source.xenocrystal;
    destination.scienceSamples.insert(destination.scienceSamples.end(), source.scienceSamples.begin(), source.scienceSamples.end());
    visit_fleet_report_outbox(state, [&](PlayerId owner, auto& report) {
        if (owner == destination.owner && report.transmissionFleet == source.id) report.transmissionFleet = destination.id;
    });
    return true;
}

bool merge_fleets(GameState& state, PlayerId player, const MergeFleetsOrder& order)
{
    if (order.destination == order.source) return false;
    const auto destination = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& fleet) {
        return fleet.id == order.destination && fleet.owner == player;
    });
    const auto source = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& fleet) {
        return fleet.id == order.source && fleet.owner == player;
    });
    if (destination == state.fleets.end() || source == state.fleets.end()
        || !same_position(destination->position, source->position)
        || !fleet_ready_for_reorganization(*destination)
        || !fleet_ready_for_reorganization(*source)) return false;
    if (!merge_fleet_special_cargo(state, *destination, *source)) return false;

    combine_probe_knowledge(*destination, *source);
    destination->lastMovedTurn = std::max(destination->lastMovedTurn, source->lastMovedTurn);
    destination->lastServiceTurn = std::max(destination->lastServiceTurn, source->lastServiceTurn);
    const auto sourceId = source->id;
    const auto destinationShips = fleet_dry_mass(state, *destination);
    const auto sourceShipCount = fleet_dry_mass(state, *source);
    auto sourceShips = fleet_ship_stacks(*source);
    sync_fleet_presentation(state, *destination);
    destination->ships.insert(destination->ships.end(), sourceShips.begin(), sourceShips.end());
    destination->fuel += source->fuel;
    destination->colonists += source->colonists;
    destination->minerals.ironium += source->minerals.ironium;
    destination->minerals.boranium += source->minerals.boranium;
    destination->minerals.germanium += source->minerals.germanium;
    if (destinationShips + sourceShipCount > 0) {
        destination->damagePercent =
            (destination->damagePercent * destinationShips
                + source->damagePercent * sourceShipCount)
            / (destinationShips + sourceShipCount);
    }
    sync_fleet_presentation(state, *destination);
    destination->warp = std::min(destination->warp, fleet_max_warp(state, *destination));
    destination->fuel = std::min(destination->fuel, fleet_fuel_capacity(state, *destination));
    refresh_merged_fleet_manifest(*destination);
    std::erase_if(state.fleets, [&](const Fleet& fleet) { return fleet.id == sourceId; });
    return true;
}

double stack_capacity(
    const GameState& state,
    const std::vector<FleetShipStack>& stacks,
    double (*capacity)(const ShipDesign&))
{
    double total = 0.0;
    for (const auto& stack : stacks) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            total += capacity(*design) * stack.count;
        }
    }
    return total;
}

bool split_fleet(GameState& state, PlayerId player, const SplitFleetOrder& order)
{
    const auto source = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& fleet) {
        return fleet.id == order.source && fleet.owner == player;
    });
    if (source == state.fleets.end() || !fleet_ready_for_reorganization(*source)) return false;

    if (order.ships.empty()) return false;
    Fleet requested;
    requested.design = source->design;
    requested.ships = order.ships;
    normalize_fleet_composition(requested);
    if (requested.ships.empty()) return false;

    // Build both results off-state: an impossible allocation must leave the
    // original fleet and nextFleetId untouched.
    Fleet remaining = *source;
    sync_fleet_presentation(state, remaining);
    std::uint32_t movedCount = 0;
    for (const auto& moved : requested.ships) {
        const auto available = fleet_ship_count(remaining, moved.design);
        if (available < moved.count) return false;
        movedCount += moved.count;
    }
    if (movedCount == 0 || movedCount >= fleet_ship_count(remaining)) return false;

    const auto totalFuelCapacity = fleet_fuel_capacity(state, remaining);
    const auto movedFuelCapacity = stack_capacity(state, requested.ships, ship_design_fuel_capacity);
    const auto totalCargoCapacity = fleet_cargo_capacity(state, remaining);
    const auto movedCargoCapacity = stack_capacity(state, requested.ships, ship_design_cargo_capacity);
    const auto fuelShare = totalFuelCapacity > 0.0 ? movedFuelCapacity / totalFuelCapacity : 0.0;
    // Specimens are indivisible. Keep as many as fit in the surviving source
    // hold, then move whole specimens; share ordinary cargo across the free holds.
    const auto keptSamples = std::min(remaining.scienceSamples.size(), std::size_t(std::max(0.0,
        std::floor((totalCargoCapacity - movedCargoCapacity + 1e-6) / kScienceSampleMass))));
    const auto movedSamples = remaining.scienceSamples.size() - keptSamples;
    if (movedSamples * kScienceSampleMass > movedCargoCapacity + 1e-6) return false;
    const auto ordinaryCapacity = totalCargoCapacity - science_sample_mass(remaining.scienceSamples);
    const auto cargoShare = ordinaryCapacity > 1e-6
        ? std::clamp((movedCargoCapacity - movedSamples * kScienceSampleMass) / ordinaryCapacity, 0.0, 1.0) : 0.0;

    auto remainingCapacity = totalCargoCapacity - movedCargoCapacity - keptSamples * kScienceSampleMass;
    auto detachedCapacity = movedCargoCapacity - movedSamples * kScienceSampleMass;
    const auto maxRemainingCrystals = std::uint32_t(std::min(double(remaining.xenocrystal), std::floor(remainingCapacity + 1e-6)));
    const auto maxDetachedCrystals = std::uint32_t(std::min(double(remaining.xenocrystal), std::floor(detachedCapacity + 1e-6)));
    const auto minimumCrystals = remaining.xenocrystal - maxRemainingCrystals;
    if (minimumCrystals > maxDetachedCrystals) return false;
    const auto movedCrystals = std::clamp(std::uint32_t(std::floor(remaining.xenocrystal * cargoShare)), minimumCrystals, maxDetachedCrystals);
    remainingCapacity -= remaining.xenocrystal - movedCrystals; detachedCapacity -= movedCrystals;
    const auto maxRemainingPopulation = std::uint64_t(std::floor((remainingCapacity + 1e-6) * kColonistsPerCargoUnit));
    const auto maxDetachedPopulation = std::uint64_t(std::floor((detachedCapacity + 1e-6) * kColonistsPerCargoUnit));
    const auto minimumMoved = remaining.colonists > maxRemainingPopulation ? remaining.colonists - maxRemainingPopulation : 0;
    const auto maximumMoved = std::min(remaining.colonists, maxDetachedPopulation);
    if (minimumMoved > maximumMoved) return false;

    Fleet detached = remaining;
    detached.xenocrystal = movedCrystals; remaining.xenocrystal -= movedCrystals;
    detached.scienceSamples.assign(remaining.scienceSamples.begin() + keptSamples, remaining.scienceSamples.end());
    remaining.scienceSamples.resize(keptSamples);
    detached.id = state.nextFleetId;
    detached.name = remaining.name + " Detachment " + std::to_string(detached.id);
    detached.ships = requested.ships;
    sync_fleet_presentation(state, detached);
    detached.destination.reset();
    detached.targetFleet = 0;
    detached.arrivalAction.reset();
    detached.waypointQueue.clear();
    detached.pendingCommands.clear();
    detached.visitor = {}; remaining.visitor = {};
    pause_probe(detached);
    pause_probe(remaining);
    detached.mobileBase.clients.clear();
    detached.telemetry = {};
    detached.telemetryInTransit.clear();
    detached.fuelStalled = false;
    detached.task = FleetTask::None;
    detached.repeatOrders = false;
    detached.routeTemplate.clear();
    detached.fuel = remaining.fuel * fuelShare;
    detached.colonists = std::clamp(std::uint64_t(std::floor(remaining.colonists * cargoShare)), minimumMoved, maximumMoved);
    remaining.colonists -= detached.colonists;
    // Integer population goes first. Share divisible cargo over the actual
    // free space left on each side, including when there are no minerals.
    const auto remainingFree = std::max(0.0, remainingCapacity - colonist_cargo_mass(remaining.colonists));
    const auto detachedFree = std::max(0.0, detachedCapacity - colonist_cargo_mass(detached.colonists));
    const auto mineralShare = remainingFree + detachedFree > 1e-6 ? detachedFree / (remainingFree + detachedFree) : 0.0;
    detached.minerals.ironium = remaining.minerals.ironium * mineralShare;
    detached.minerals.boranium = remaining.minerals.boranium * mineralShare;
    detached.minerals.germanium = remaining.minerals.germanium * mineralShare;

    remaining.fuel -= detached.fuel;
    remaining.minerals.ironium -= detached.minerals.ironium;
    remaining.minerals.boranium -= detached.minerals.boranium;
    remaining.minerals.germanium -= detached.minerals.germanium;
    for (const auto& moved : requested.ships) {
        const auto stack = std::find_if(remaining.ships.begin(), remaining.ships.end(), [&](const FleetShipStack& candidate) {
            return candidate.design == moved.design;
        });
        stack->count -= moved.count;
    }
    sync_fleet_presentation(state, remaining);
    remaining.warp = std::min(remaining.warp, fleet_max_warp(state, remaining));
    if (fleet_cargo_used(state, remaining) > fleet_cargo_capacity(state, remaining) + 1e-6
        || fleet_cargo_used(state, detached) > fleet_cargo_capacity(state, detached) + 1e-6) return false;
    remaining.telemetry = {};
    remaining.telemetryInTransit.clear();
    *source = std::move(remaining);
    ++state.nextFleetId;
    state.fleets.push_back(std::move(detached));
    return true;
}

void queue_fleet_movement_report(
    GameState& state,
    const Fleet& fleet,
    PlayerReportKind kind,
    std::uint64_t observationTurn)
{
    StarId starId = 0;
    PlanetId planetId = 0;
    for (const auto& star : state.stars) {
        if (!same_position(star.position, fleet.position)) continue;
        starId = star.id;
        if (const auto* planet = find_planet_at_star(state, star.id)) planetId = planet->id;
        break;
    }
    queue_player_report(
        state,
        fleet.owner,
        kind,
        fleet.position,
        observationTurn,
        starId,
        planetId,
        fleet.id,
        fleet.design);
}

bool finish_fleet_arrival(GameState& state, Fleet& fleet, std::vector<FleetId>& consumedFleets,
    std::vector<ColonyFreightDelivery>& freight)
{
    if(fleet.electronics.wakeOnArrival) fleet.electronics={};
    fleet.fuelStalled = false;
    fleet.targetFleet = 0;
    if (fleet.arrivalAction && fleet.arrivalAction->kind == FleetArrivalActionKind::EnterWormhole) {
        if (enter_wormhole(state, fleet, fleet.arrivalAction->wormholeEndpoint)) {
            consumedFleets.push_back(fleet.id);
            return true;
        }
        return false;
    }
    if (fleet.arrivalAction && fleet.arrivalAction->kind == FleetArrivalActionKind::TransitStargate) {
        const auto action = *fleet.arrivalAction;
        const auto* source = find_planet(state, action.gateSource);
        const auto* star = source ? find_star(state, source->star) : nullptr;
        // Arriving by flight and jumping require separate years. The route
        // remains at this step; no later cargo action or flight can run early.
        if (fleet.lastMovedTurn > state.turn) {
            if (star) fleet.destination = star->position;
            return false;
        }
        const bool transferred = transfer_fleet_through_stargate(state, fleet.owner, fleet.id,
            action.gateSource, action.gateDestination, action.gateResonant, true);
        const auto* planet = find_planet(state, transferred ? action.gateDestination : action.gateSource);
        queue_player_report(state, fleet.owner, transferred ? PlayerReportKind::StargateTransit
                : PlayerReportKind::StargateTransitFailed, fleet.position, state.turn + 1,
            planet ? planet->star : 0, planet ? planet->id : 0, fleet.id, 0, ProductionKind::ColonyShip,
            transferred && action.gateResonant ? kResonantGateCrystals : 0, ResearchField::Electronics, 0,
            fleet.minerals, fleet.colonists);
        if (!transferred) {
            // Hold and retry on the next turn. A delayed stop/replacement uses
            // the same ordinary command path as any other route programme.
            fleet.destination = fleet.position;
            return false;
        }
        fleet.arrivalAction.reset();
    }
    if (execute_arrival_action(state, fleet, freight)) {
        queue_fleet_movement_report(state, fleet, PlayerReportKind::RouteCompleted, state.turn + 1);
        consumedFleets.push_back(fleet.id);
        return true;
    }

    activate_next_waypoint(state, fleet);
    if (!fleet.destination && fleet.waypointQueue.empty() && fleet.repeatOrders) {
        fleet.waypointQueue = fleet.routeTemplate;
        activate_next_waypoint(state, fleet);
        if (!fleet.destination) {
            fleet.repeatOrders = false;
            fleet.routeTemplate.clear();
        }
    }
    queue_fleet_movement_report(
        state,
        fleet,
        fleet.destination ? PlayerReportKind::FleetArrived : PlayerReportKind::RouteCompleted,
        state.turn + 1);
    return false;
}

bool fleet_id_list_contains(const std::vector<FleetId>& values, FleetId id)
{
    return std::find(values.begin(), values.end(), id) != values.end();
}

Fleet* friendly_fleet(GameState& state, PlayerId owner, FleetId id)
{
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == id && candidate.owner == owner;
    });
    return fleet == state.fleets.end() ? nullptr : &*fleet;
}

Fleet* fleet_by_id(GameState& state, FleetId id)
{
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == id;
    });
    return fleet == state.fleets.end() ? nullptr : &*fleet;
}

void abandon_lost_fleet_target(GameState& state, Fleet& fleet)
{
    const auto lostTarget = fleet.targetFleet;
    fleet.destination.reset();
    fleet.targetFleet = 0;
    fleet.arrivalAction.reset();
    fleet.waypointQueue.clear();
    fleet.repeatOrders = false;
    fleet.routeTemplate.clear();
    fleet.fuelStalled = false;
    queue_player_report(
        state,
        fleet.owner,
        PlayerReportKind::FleetTargetLost,
        fleet.position,
        state.turn + 1,
        0,
        0,
        fleet.id,
        fleet.design,
        ProductionKind::ColonyShip,
        lostTarget);
}

bool merge_rendezvous_fleet(GameState& state, Fleet& source, Fleet& destination)
{
    if (source.outpost != OutpostKind::None || destination.outpost != OutpostKind::None) return false;
    const bool local=fleet_has_instant_link(state,destination);
    const bool sourceReported=!unreported_orbital_ship(state,source);
    const auto oldTelemetry=destination.telemetry;
    auto oldSource=fleet_player_view(state,source);
    oldSource.telemetryInTransit=source.telemetryInTransit;
    if (!merge_fleet_special_cargo(state, destination, source)) return false;
    combine_probe_knowledge(destination, source);
    // A gate-spent movement year and used service capacity survive the
    // pursuing FleetId just as they do an explicit merge order.
    destination.lastMovedTurn = std::max(destination.lastMovedTurn, source.lastMovedTurn);
    destination.lastServiceTurn = std::max(destination.lastServiceTurn, source.lastServiceTurn);
    const auto sourceId = source.id;
    const auto destinationShips = fleet_dry_mass(state, destination);
    const auto sourceShipCount = fleet_dry_mass(state, source);
    auto sourceShips = fleet_ship_stacks(source);
    sync_fleet_presentation(state, destination);
    destination.ships.insert(destination.ships.end(), sourceShips.begin(), sourceShips.end());
    destination.fuel += source.fuel;
    destination.colonists += source.colonists;
    destination.minerals.ironium += source.minerals.ironium;
    destination.minerals.boranium += source.minerals.boranium;
    destination.minerals.germanium += source.minerals.germanium;
    if (destinationShips + sourceShipCount > 0) {
        destination.damagePercent =
            (destination.damagePercent * destinationShips
                + source.damagePercent * sourceShipCount)
            / (destinationShips + sourceShipCount);
    }
    sync_fleet_presentation(state, destination);
    destination.warp = std::min(destination.warp, fleet_max_warp(state, destination));
    destination.fuel = std::min(destination.fuel, fleet_fuel_capacity(state, destination));
    if(local) refresh_merged_fleet_manifest(destination);
    else destination.telemetry=oldTelemetry;

    queue_player_report(
        state,
        destination.owner,
        PlayerReportKind::FleetsMerged,
        destination.position,
        state.turn + 1,
        0,
        0,
        destination.id,
        destination.design,
        ProductionKind::ColonyShip,
        sourceId);
    const auto due=fleet_report_delivery_turn(state,destination.owner,destination.position,state.turn+1,destination.id);
    if(!local && sourceReported && due>state.turn+1) {
        oldSource.pendingCommands.clear();
        state.pendingFleetLossContacts.push_back({std::move(oldSource),due});
    }
    return true;
}

bool resolve_fleet_target_arrival(
    GameState& state,
    Fleet& fleet,
    std::vector<FleetId>& consumedFleets,
    std::vector<ColonyFreightDelivery>& freight)
{
    if (fleet.targetFleet == 0) return false;
    auto* target = friendly_fleet(state, fleet.owner, fleet.targetFleet);
    if (!target || fleet_id_list_contains(consumedFleets, target->id)) {
        abandon_lost_fleet_target(state, fleet);
        return true;
    }

    fleet.destination = target->position;
    if (!same_position(fleet.position, target->position)) return false;

    const auto action = fleet.arrivalAction.value_or(FleetArrivalAction{});
    fleet.targetFleet = 0;
    fleet.destination.reset();
    if (action.kind == FleetArrivalActionKind::MergeWithFleet) {
        fleet.arrivalAction.reset();
        if (merge_rendezvous_fleet(state, fleet, *target)) consumedFleets.push_back(fleet.id);
        return true;
    }

    (void)finish_fleet_arrival(state, fleet, consumedFleets, freight);
    return true;
}

struct MovementBudget {
    double distance{};
    double fuelChange{};
    double damage{};
    double fuelConsumed{};
    double fuelMinimum{}, fuelMaximum{};
};

MovementBudget fleet_movement_budget(
    const GameState& state, const Fleet& fleet, Position destination, double remaining, double turnFraction)
{
    constexpr double epsilon = 0.000001;
    if (remaining <= epsilon || !fleet_warp_valid(state, fleet, fleet.warp)) return {};
    const auto time = std::clamp(turnFraction, 0.0, 1.0);
    const auto speed = warp_distance(fleet.warp);
    const auto maximumDistance = std::min(remaining, speed * time);
    auto paidDistance = fleet_fuel_affordable_segment_distance(state, fleet, destination, maximumDistance);
    const auto full=distance_between(fleet.position,destination);
    const auto along=[&](double distance) { return full>0 ? Position{
        fleet.position.x+(destination.x-fleet.position.x)*distance/full,
        fleet.position.y+(destination.y-fleet.position.y)*distance/full} : fleet.position; };
    const bool fuelLimited = paidDistance < maximumDistance - epsilon;
    const auto capDamageDistance=[&](const Fleet& moving,double maximum,auto endpoint) {
        if (fleet_segment_damage(state,moving,endpoint(maximum)) < 100.0-moving.damagePercent) return maximum;
        double low=0,high=maximum;
        for (int i=0;i<48;++i) {
            const auto mid=(low+high)*0.5;
            if (fleet_segment_damage(state,moving,endpoint(mid)) < 100.0-moving.damagePercent) low=mid;
            else high=mid;
        }
        return high;
    };
    paidDistance=capDamageDistance(fleet,paidDistance,along);
    MovementBudget result{
        paidDistance,
        fleet_fuel_change_for_segment(state, fleet, along(paidDistance)),
        fleet_segment_damage(state, fleet, along(paidDistance)),
        fleet_fuel_consumption_for_segment(state, fleet, along(paidDistance)),
    };
    const auto bounds=fleet_segment_fuel_bounds(state,fleet,paidDistance);
    result.fuelMinimum=bounds.consumedMinimum; result.fuelMaximum=bounds.consumedMaximum;
    // Reference engines fall back to the slowest free speed in the fleet
    // for the unused part of the year. The player's requested Warp is kept.
    const auto freeWarp = fleet_free_warp(state, fleet);
    if (fuelLimited && freeWarp > 0 && freeWarp < fleet.warp
        && fleet.damagePercent + result.damage < 100.0 - epsilon) {
        auto coasting = fleet;
        coasting.warp = freeWarp;
        coasting.position=along(paidDistance);
        coasting.damagePercent += result.damage;
        auto freeDistance = std::min(
            remaining - paidDistance, warp_distance(freeWarp) * std::max(0.0, time - paidDistance / speed));
        freeDistance=capDamageDistance(coasting,freeDistance,[&](double d) { return along(paidDistance+d); });
        result.damage += fleet_segment_damage(state,coasting,along(paidDistance+freeDistance));
        result.distance += freeDistance;
        result.fuelChange += fleet_fuel_change_for_segment(state, coasting, along(paidDistance+freeDistance));
        result.fuelConsumed += fleet_fuel_consumption_for_segment(state, coasting, along(paidDistance+freeDistance));
        const auto coastBounds=fleet_segment_fuel_bounds(state,coasting,freeDistance);
        result.fuelMinimum+=coastBounds.consumedMinimum; result.fuelMaximum+=coastBounds.consumedMaximum;
    }
    return result;
}

Position projected_movement_endpoint(
    const GameState& state,
    const Fleet& fleet,
    Position destination,
    double turnFraction)
{
    const auto remaining = distance_between(fleet.position, destination);
    if (remaining <= 0.000001 || !fleet_warp_valid(state, fleet, fleet.warp)) return fleet.position;

    const auto distance = fleet_movement_budget(state, fleet, destination, remaining, turnFraction).distance;
    if (distance <= 0.000001) return fleet.position;
    if (distance >= remaining - 0.000001) return destination;

    const auto fraction = distance / remaining;
    return {
        fleet.position.x + (destination.x - fleet.position.x) * fraction,
        fleet.position.y + (destination.y - fleet.position.y) * fraction,
    };
}

Position projected_turn_endpoint(const GameState& state, const Fleet& fleet, Position destination)
{
    return projected_movement_endpoint(state, fleet, destination, 1.0);
}

void apply_overdrive_damage(Fleet& fleet, double damage)
{
    if (damage <= 0.0) return;
    fleet.damagePercent = std::clamp(
        fleet.damagePercent + damage, 0.0, 100.0);
    if (fleet.damagePercent < 100.0 - 0.000001) return;
    fleet.damagePercent = 100.0;

    // Critically damaged fleets remain on the map for recovery mechanics and
    // future combat work, but their engines can no longer execute a route.
    fleet.destination.reset();
    fleet.arrivalAction.reset();
    fleet.waypointQueue.clear();
    fleet.repeatOrders = false;
    fleet.routeTemplate.clear();
    fleet.targetFleet = 0;
}

// Surviving colonists already exposed this year travel with their cargo cohort
// through rendezvous merges. New passengers entering an unsafe moving formation
// pay the annual loss once, regardless of how many segments it traverses.
using RadiationExposure = std::map<FleetId, std::uint64_t>;

void apply_movement_effects(GameState& state, Fleet& fleet, Position start,
    double overdriveDamage, RadiationExposure& exposure)
{
    observe_fleet_sensor_sweep(state, fleet, start, fleet.position, state.turn + 1);
    if (!fleet_radiation_safe(state, fleet)) {
        const auto alreadyExposed = std::min(fleet.colonists, exposure[fleet.id]);
        auto unexposed = fleet;
        unexposed.colonists -= alreadyExposed;
        fleet.colonists -= projected_fleet_radiation_losses(state, unexposed);
        exposure[fleet.id] = fleet.colonists;
    }
    apply_overdrive_damage(fleet, overdriveDamage);
}

void continue_merged_fleet_route(
    GameState& state,
    Fleet& fleet,
    Position destination,
    double remainingTurnFraction,
    std::vector<FleetId>& consumedFleets,
    std::vector<ColonyFreightDelivery>& freight,
    std::vector<FleetMovementFuel>& movementFuel,
    RadiationExposure& exposure)
{
    constexpr double epsilon = 0.000001;
    if (remainingTurnFraction <= epsilon) return;

    fleet.warp = std::min(fleet.warp, fleet_max_warp(state, fleet));
    const auto start = fleet.position;
    const auto routeDistance = distance_between(start, destination);
    if (routeDistance <= epsilon) {
        if (fleet.targetFleet == 0) {
            fleet.destination.reset();
            (void)finish_fleet_arrival(state, fleet, consumedFleets, freight);
        }
        return;
    }

    const auto endpoint = projected_movement_endpoint(
        state, fleet, destination, remainingTurnFraction);
    const auto travelled = distance_between(start, endpoint);
    const auto requested = std::min(routeDistance, warp_distance(fleet.warp) * remainingTurnFraction);
    const auto affordable = fleet_fuel_affordable_segment_distance(state, fleet, destination, requested);
    if (travelled <= epsilon) {
        const auto fuelPerLy = fleet_fuel_consumption_for_segment(state, fleet, destination);
        if (routeDistance > epsilon && fuelPerLy > epsilon && !fleet.fuelStalled) {
            fleet.fuelStalled = true;
            queue_fleet_movement_report(
                state, fleet, PlayerReportKind::FleetStalledForFuel, state.turn + 1);
        }
        movementFuel.push_back({fleet.id, destination, fleet.targetFleet, 0.0, fleet.fuel,
            affordable + epsilon < requested});
        return;
    }

    const auto budget = fleet_movement_budget(state, fleet, endpoint, travelled, remainingTurnFraction);
    fleet.fuel = std::clamp(
        fleet.fuel - budget.fuelChange,
        0.0,
        fleet_fuel_capacity(state, fleet));
    if (!same_position(fleet.position, endpoint)) fleet.lastMovedTurn = state.turn + 1;
    fleet.position = endpoint;
    movementFuel.push_back({fleet.id, destination, fleet.targetFleet, budget.fuelConsumed, fleet.fuel,
        affordable + epsilon < requested && travelled + epsilon >= affordable,budget.fuelMinimum,budget.fuelMaximum});
    fleet.fuelStalled = false;
    apply_movement_effects(state, fleet, start, budget.damage, exposure);

    if (fleet.damagePercent < 100.0 && fleet.targetFleet == 0
        && same_position(endpoint, destination)) {
        fleet.destination.reset();
        (void)finish_fleet_arrival(state, fleet, consumedFleets, freight);
    }
}

void advance_fleets(GameState& state, std::vector<ColonyFreightDelivery>& freight,
    std::vector<FleetMovementFuel>& movementFuel)
{
    constexpr double epsilon = 0.000001;
    std::vector<FleetId> consumedFleets;
    std::vector<FleetId> skipMovement;
    RadiationExposure exposure;

    // A rendezvous already true at the planning boundary resolves before either
    // fleet starts another leg. This mirrors fixed-coordinate arrival behavior.
    std::vector<FleetId> fleetIds;
    fleetIds.reserve(state.fleets.size());
    for (const auto& fleet : state.fleets) fleetIds.push_back(fleet.id);
    std::sort(fleetIds.begin(), fleetIds.end());
    // Gate steps already at their source run before motion/battles, so the
    // destination participates in this year's ordinary encounter resolution.
    // An approach arriving later in the year waits for the following boundary.
    for (const auto id : fleetIds) {
        auto* fleet = fleet_by_id(state,id);
        if (!fleet || !fleet->destination || !same_position(fleet->position,*fleet->destination)
            || !fleet->arrivalAction || fleet->arrivalAction->kind != FleetArrivalActionKind::TransitStargate) continue;
        fleet->destination.reset();
        (void)finish_fleet_arrival(state,*fleet,consumedFleets,freight);
        skipMovement.push_back(id);
    }
    for (const auto id : fleetIds) {
        auto* fleet = fleet_by_id(state, id);
        if (!fleet || fleet->targetFleet == 0) continue;
        auto* target = friendly_fleet(state, fleet->owner, fleet->targetFleet);
        if (!target) {
            abandon_lost_fleet_target(state, *fleet);
            skipMovement.push_back(id);
            continue;
        }
        if (same_position(fleet->position, target->position)) {
            resolve_fleet_target_arrival(state, *fleet, consumedFleets, freight);
            skipMovement.push_back(id);
        }
    }
    if (!consumedFleets.empty()) {
        std::erase_if(state.fleets, [&](const Fleet& fleet) {
            return fleet_id_list_contains(consumedFleets, fleet.id);
        });
        consumedFleets.clear();
    }

    struct MotionPlan {
        FleetId fleet{};
        Position destination;
        Position endpoint;
        Position baseEndpoint;
        bool routed{};
        bool active{};
        bool intercepted{};
        PlayerId owner{};
        bool armed{};
        bool mineRestricted{};
        Position start;
    };
    std::vector<MotionPlan> plans;
    plans.reserve(state.fleets.size());
    for (const auto& fleet : state.fleets) {
        MotionPlan plan{fleet.id};
        plan.owner = fleet.owner;
        plan.armed = fleet_has_space_weapons(state, fleet);
        plan.start = fleet.position;
        if (fleet.lastMovedTurn <= state.turn && !fleet_id_list_contains(skipMovement, fleet.id) && fleet.destination
            && fleet_warp_valid(state, fleet, fleet.warp)) {
            plan.routed = true;
            plan.active = true;
            plan.destination = *fleet.destination;
            if (fleet.targetFleet != 0) {
                if (const auto* target = friendly_fleet(state, fleet.owner, fleet.targetFleet)) {
                    plan.destination = target->position;
                }
            }
            plan.endpoint = projected_turn_endpoint(state, fleet, plan.destination);
        } else {
            plan.endpoint = fleet.position;
        }
        plan.baseEndpoint = plan.endpoint;
        plans.push_back(plan);
    }

    std::sort(plans.begin(), plans.end(), [](const auto& a, const auto& b) { return a.fleet < b.fleet; });
    // Moving-target legs aim at the target's deterministic end-of-turn
    // position. All plans use the same start-of-turn snapshot, so vector order
    // can never change pursuit results.
    for (auto& plan : plans) {
        auto* fleet = fleet_by_id(state, plan.fleet);
        if (!fleet || fleet->targetFleet == 0 || !plan.active) continue;
        const auto targetPlan = std::find_if(plans.begin(), plans.end(), [&](const MotionPlan& candidate) {
            return candidate.fleet == fleet->targetFleet;
        });
        if (targetPlan == plans.end()) continue;
        plan.destination = targetPlan->baseEndpoint;
        plan.endpoint = projected_turn_endpoint(state, *fleet, plan.destination);
    }

    // Simultaneous movement uses one field geometry. Consumption in FleetId
    // order must not invalidate a later fleet's already planned boundary.
    const auto navigationMinefields = state.minefields;
    for (auto& plan : plans) if (plan.active) {
        if (const auto* fleet = fleet_by_id(state,plan.fleet)) {
            const auto end = minefield_navigation_endpoint(state,*fleet,plan.endpoint);
            plan.mineRestricted = !same_position(end,plan.endpoint);
            plan.endpoint = end;
        }
    }

    struct EncounterCandidate {
        FleetId pursuer{};
        FleetId target{};
        double timeFraction{};
        Position position;
        Position targetDestination;
        bool targetRouted{};
        OrbitalStationId station{};
        VisitorId visitor{};
    };
    std::vector<EncounterCandidate> encounters;
    for (const auto& plan : plans) {
        const auto* pursuer = fleet_by_id(state, plan.fleet);
        if (!pursuer || pursuer->targetFleet == 0 || !plan.active) continue;
        const auto targetPlan = std::find_if(plans.begin(), plans.end(), [&](const MotionPlan& candidate) {
            return candidate.fleet == pursuer->targetFleet;
        });
        const auto* target = friendly_fleet(state, pursuer->owner, pursuer->targetFleet);
        if (targetPlan == plans.end() || !target) continue;

        const auto geometry = analyze_fleet_encounter(
            pursuer->position,
            plan.endpoint,
            target->position,
            targetPlan->endpoint);
        if (!geometry.encounterTimeFraction) continue;
        encounters.push_back({
            pursuer->id,
            target->id,
            *geometry.encounterTimeFraction,
            geometry.encounterPosition,
            targetPlan->destination,
            targetPlan->routed,
        });
    }
    for (const auto& plan : plans) {
        const auto* f = fleet_by_id(state, plan.fleet);
        if (!f || !f->visitor.track.id || state.mediumObserver) continue;
        for (const auto& v : state.visitors) if (v.id == f->visitor.track.id && state.turn < v.departureTurn) {
            const Position end{v.position.x+v.velocity.x, v.position.y+v.velocity.y};
            const auto geometry = analyze_fleet_encounter(plan.start, plan.endpoint, v.position, end);
            if (geometry.encounterTimeFraction) encounters.push_back({plan.fleet,0,*geometry.encounterTimeFraction,geometry.encounterPosition,{},false,0,v.id});
        }
    }
    for (const auto& plan : plans) for (const auto& station : state.orbitalStations) {
        if (station.owner == plan.owner || (!plan.armed && !station_has_weapons(station))) continue;
        const auto planet = std::find_if(state.planets.begin(),state.planets.end(),[&](const auto& p) { return p.id == station.planet; });
        const auto* star = planet == state.planets.end() ? nullptr : find_star(state,planet->star);
        if (!star) continue;
        const auto geometry = analyze_fleet_encounter(plan.start,plan.endpoint,star->position,star->position);
        if (geometry.encounterTimeFraction) encounters.push_back({plan.fleet,0,*geometry.encounterTimeFraction,geometry.encounterPosition,{},false,station.id});
    }
    // Armed hostile fleets also meet along their continuous motion segments,
    // including flybys of an idle fleet. Different-time path crossings do not.
    for (std::size_t i = 0; i < plans.size(); ++i) {
        for (std::size_t j = i + 1; j < plans.size(); ++j) {
            if (plans[i].owner == plans[j].owner || (!plans[i].armed && !plans[j].armed)) continue;
            const auto geometry = analyze_fleet_encounter(plans[i].start, plans[i].endpoint,
                plans[j].start, plans[j].endpoint);
            if (geometry.encounterTimeFraction) encounters.push_back({plans[i].fleet, plans[j].fleet,
                *geometry.encounterTimeFraction, geometry.encounterPosition, {}, false});
        }
    }
    std::sort(encounters.begin(), encounters.end(), [](const EncounterCandidate& left, const EncounterCandidate& right) {
        if (left.timeFraction != right.timeFraction) {
            return left.timeFraction < right.timeFraction;
        }
        if (left.pursuer != right.pursuer) return left.pursuer < right.pursuer;
        if (left.target != right.target) return left.target < right.target;
        return left.station < right.station;
    });

    // Resolve at most one encounter per fleet in a turn. Competing intercepts
    // are ordered by physical time, then stable FleetId, so replay results do
    // not depend on storage order.
    std::vector<FleetId> encounteredFleets;
    std::vector<EncounterCandidate> resolvedEncounters;
    for (const auto& encounter : encounters) {
        if (encounter.station || encounter.visitor) {
            if (fleet_id_list_contains(encounteredFleets,encounter.pursuer)) continue;
            const auto plan = std::find_if(plans.begin(),plans.end(),[&](const auto& p) { return p.fleet==encounter.pursuer; });
            if (plan == plans.end()) continue;
            plan->endpoint=encounter.position; plan->active=true; plan->intercepted=true;
            encounteredFleets.push_back(encounter.pursuer); resolvedEncounters.push_back(encounter);
            continue;
        }
        if (fleet_id_list_contains(encounteredFleets, encounter.pursuer)
            || fleet_id_list_contains(encounteredFleets, encounter.target)) {
            // A third hostile fleet meeting the same participants at the same
            // instant joins that encounter, rather than flying through it.
            const auto* first = fleet_by_id(state, encounter.pursuer);
            const auto* second = fleet_by_id(state, encounter.target);
            const bool firstUsed = fleet_id_list_contains(encounteredFleets, encounter.pursuer);
            const bool secondUsed = fleet_id_list_contains(encounteredFleets, encounter.target);
            if (first && second && first->owner != second->owner && firstUsed != secondUsed) {
                const auto used = firstUsed ? encounter.pursuer : encounter.target;
                const auto joining = firstUsed ? encounter.target : encounter.pursuer;
                const auto existing = std::find_if(resolvedEncounters.begin(), resolvedEncounters.end(), [&](const auto& prior) {
                    return (prior.pursuer == used || prior.target == used)
                        && std::abs(prior.timeFraction - encounter.timeFraction) <= 1e-9
                        && distance_between(prior.position, encounter.position) <= kFleetEncounterRadius + epsilon;
                });
                if (existing != resolvedEncounters.end()) {
                    const auto plan = std::find_if(plans.begin(), plans.end(), [&](const auto& p) { return p.fleet == joining; });
                    if (plan != plans.end()) {
                        plan->endpoint = existing->position;
                        plan->active = true;
                        plan->intercepted = true;
                        encounteredFleets.push_back(joining);
                    }
                }
            }
            continue;
        }
        const auto pursuerPlan = std::find_if(plans.begin(), plans.end(), [&](const MotionPlan& candidate) {
            return candidate.fleet == encounter.pursuer;
        });
        const auto targetPlan = std::find_if(plans.begin(), plans.end(), [&](const MotionPlan& candidate) {
            return candidate.fleet == encounter.target;
        });
        if (pursuerPlan == plans.end() || targetPlan == plans.end()) continue;
        pursuerPlan->endpoint = encounter.position;
        targetPlan->endpoint = encounter.position;
        pursuerPlan->active = true;
        targetPlan->active = true;
        pursuerPlan->intercepted = true;
        targetPlan->intercepted = true;
        encounteredFleets.push_back(encounter.pursuer);
        encounteredFleets.push_back(encounter.target);
        resolvedEncounters.push_back(encounter);
    }

    std::vector<FleetId> fixedArrivals;
    std::vector<FleetId> mineDestroyedFleets;

    for (const auto id : fleetIds) {
        auto* movingFleet = fleet_by_id(state,id); if (!movingFleet) continue;
        auto& fleet = *movingFleet;
        const auto plan = std::find_if(plans.begin(), plans.end(), [&](const MotionPlan& candidate) {
            return candidate.fleet == fleet.id;
        });
        if (plan == plans.end() || !plan->active) {
            fleet.fuelStalled = false;
            continue;
        }

        const Position start = fleet.position;
        auto endpoint = plan->endpoint;
        if (plan->routed) fleet.destination = plan->destination;
        const auto remaining = distance_between(start, endpoint);
        const auto requestedDistance = plan->routed
            ? std::min(distance_between(start, plan->destination), warp_distance(fleet.warp)) : 0.0;
        const auto affordableDistance = fleet_fuel_affordable_segment_distance(state, fleet, plan->destination, requestedDistance);
        const auto recordFuel = [&](double consumed, bool mineStrike = false,double minimum=0,double maximum=0) {
            movementFuel.push_back({fleet.id, plan->destination, fleet.targetFleet,
                consumed, fleet.fuel,
                plan->routed && !plan->intercepted && !mineStrike
                    && affordableDistance + epsilon < requestedDistance
                    && distance_between(start, fleet.position) + epsilon >= affordableDistance,minimum,maximum});
        };
        if (remaining <= epsilon) {
            if (plan->mineRestricted && !plan->intercepted) {
                if (apply_minefield_crossing(state,fleet,start,endpoint,navigationMinefields)
                    && fleet.damagePercent >= 100) mineDestroyedFleets.push_back(fleet.id);
                if (!same_position(fleet.position, endpoint)) fleet.lastMovedTurn = state.turn + 1;
                fleet.position = endpoint;
                recordFuel(0.0, true);
                continue;
            }
            if (!same_position(fleet.position, endpoint)) fleet.lastMovedTurn = state.turn + 1;
            fleet.position = endpoint;
            recordFuel(0.0);
            const auto routeDistance = plan->routed
                ? distance_between(start, plan->destination)
                : 0.0;
            const auto fuelChangePerLy = fleet_fuel_consumption_for_segment(state, fleet, plan->destination);
            if (routeDistance > epsilon && fuelChangePerLy > epsilon && !plan->intercepted
                && !fleet.fuelStalled) {
                fleet.fuelStalled = true;
                queue_fleet_movement_report(
                    state, fleet, PlayerReportKind::FleetStalledForFuel, state.turn + 1);
            } else if (routeDistance <= epsilon || plan->intercepted) {
                fleet.fuelStalled = false;
            }
            if (plan->routed && !plan->intercepted && fleet.targetFleet == 0
                && same_position(endpoint, plan->destination)) {
                fixedArrivals.push_back(fleet.id);
            }
            continue;
        }
        fleet.fuelStalled = false;
        const auto previousDamage = fleet.damagePercent;
        const bool mineStrike = apply_minefield_crossing(state, fleet, start, endpoint,navigationMinefields);
        if (mineStrike && fleet.damagePercent >= 100) mineDestroyedFleets.push_back(fleet.id);
        auto beforeStrike = fleet; beforeStrike.damagePercent = previousDamage;

        const auto budget = fleet_movement_budget(state, beforeStrike, endpoint, distance_between(start, endpoint), 1.0);
        const auto capacity = fleet_fuel_capacity(state, fleet);
        fleet.fuel = std::clamp(fleet.fuel - budget.fuelChange, 0.0, capacity);
        if (!same_position(fleet.position, endpoint)) fleet.lastMovedTurn = state.turn + 1;
        fleet.position = endpoint;
        recordFuel(budget.fuelConsumed, mineStrike,budget.fuelMinimum,budget.fuelMaximum);

        apply_movement_effects(state, fleet, start, budget.damage, exposure);

        if (!mineStrike && fleet.damagePercent < 100.0 && plan->routed && !plan->intercepted
            && fleet.targetFleet == 0
            && same_position(endpoint, plan->destination)) {
            fixedArrivals.push_back(fleet.id);
        }
    }

    // Battles precede arrival actions, so a destroyed fleet cannot unload,
    // found a colony or escape an encounter by entering a wormhole.
    std::erase_if(state.fleets, [&](const auto& f) { return fleet_id_list_contains(mineDestroyedFleets,f.id); });
    resolve_space_battles(state, state.turn + 1);
    for (const auto& contact : resolvedEncounters) if (contact.visitor) {
        auto* fleet = fleet_by_id(state, contact.pursuer);
        if (fleet && same_position(fleet->position, contact.position))
            complete_visitor_encounter(state, *fleet, contact.visitor, contact.position);
    }
    std::sort(fixedArrivals.begin(), fixedArrivals.end());

    for (const auto id : fixedArrivals) {
        auto* fleet = fleet_by_id(state, id);
        if (!fleet || !fleet->destination || fleet_id_list_contains(consumedFleets, id)) continue;
        fleet->destination.reset();
        (void)finish_fleet_arrival(state, *fleet, consumedFleets, freight);
    }

    for (const auto id : fleetIds) {
        auto* fleet = fleet_by_id(state, id);
        if (!fleet || fleet_id_list_contains(consumedFleets, id) || fleet->targetFleet == 0) continue;
        const auto encounter = std::find_if(
            resolvedEncounters.begin(), resolvedEncounters.end(), [&](const EncounterCandidate& candidate) {
                return candidate.pursuer == id;
            });
        if (fleet_id_list_contains(encounteredFleets, id) && encounter == resolvedEncounters.end()) {
            continue;
        }

        const auto targetId = fleet->targetFleet;
        const auto action = fleet->arrivalAction.value_or(FleetArrivalAction{});
        const auto* beforeMerge = fleet_by_id(state, targetId);
        const auto exposed = std::min(exposure[id], fleet->colonists)
            + (beforeMerge ? std::min(exposure[targetId], beforeMerge->colonists) : 0);
        const auto resolved = resolve_fleet_target_arrival(state, *fleet, consumedFleets, freight);
        if (action.kind == FleetArrivalActionKind::MergeWithFleet
            && fleet_id_list_contains(consumedFleets, id)) exposure[targetId] = exposed;
        if (!resolved || encounter == resolvedEncounters.end()
            || action.kind != FleetArrivalActionKind::MergeWithFleet
            || !encounter->targetRouted) {
            continue;
        }

        auto* merged = fleet_by_id(state, targetId);
        if (!merged || fleet_id_list_contains(consumedFleets, targetId)
            || !merged->destination) {
            continue;
        }

        // The target FleetId and its route survive a merge. Use only the time
        // left after contact, and reduce Warp to the fastest setting supported
        // by every ship in the new heterogeneous fleet.
        continue_merged_fleet_route(
            state,
            *merged,
            encounter->targetDestination,
            1.0 - encounter->timeFraction,
            consumedFleets, freight, movementFuel, exposure);
    }

    if (!consumedFleets.empty()) {
        std::erase_if(state.fleets, [&](const Fleet& fleet) {
            return fleet_id_list_contains(consumedFleets, fleet.id);
        });
    }
}

void grow_colonies(GameState& state)
{
    for (auto& planet : state.planets) {
        planet.habitat.population+=projected_orbital_growth(planet);
        const auto change = projected_population_growth(state, planet, state.turn);
        if (change >= 0) planet.population += static_cast<std::uint64_t>(change);
        else planet.population -= std::min(
            planet.population, static_cast<std::uint64_t>(-change));
    }
}

// Apply exactly the same cargo validation at the planning boundary and in
// the UI preview. This must not run mining, research, population growth or a
// whole year merely to display the previously queued ship manifest.
void apply_cargo_order(GameState& next, PlayerId player, const Order& order,
    std::vector<ColonyFreightDelivery>& freight)
{
    std::visit([&](const auto& concreteOrder) {
        using T = std::decay_t<decltype(concreteOrder)>;
        if constexpr (std::is_same_v<T, SetFleetColonistsOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == player;
                        });
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& candidate) {
                            return candidate.id == concreteOrder.fleet && candidate.owner == player;
                        });
                        if (planet == next.planets.end() || fleet == next.fleets.end()) return;
                        if (!fleet_at_planet(next, *fleet, *planet)) return;

                        const auto requestedLoad = colonist_cargo_mass(concreteOrder.colonists)
                            + mineral_cargo_mass(fleet->minerals) + science_sample_mass(fleet->scienceSamples) + fleet->xenocrystal;
                        if (requestedLoad > fleet_cargo_capacity(next, *fleet) + 0.000001) return;

                        if (concreteOrder.colonists > fleet->colonists) {
                            const auto load = concreteOrder.colonists - fleet->colonists;
                            if (planet->population < load || (planet->population==load && !planet->habitat.population)) return;
                            planet->population -= load;
                        } else {
                            const auto delivered = fleet->colonists - concreteOrder.colonists;
                            planet->population += delivered;
                            record_freight_delivery(freight, *planet, {}, delivered);
                        }
                        fleet->colonists = concreteOrder.colonists;
        } else if constexpr (std::is_same_v<T, SetFleetMineralCargoOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony
                                && (candidate.owner == player || candidate.owner == 0);
                        });
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& candidate) {
                            return candidate.id == concreteOrder.fleet && candidate.owner == player;
                        });
                        if (planet == next.planets.end() || fleet == next.fleets.end()) return;
                        if (!fleet_at_planet(next, *fleet, *planet) || !valid_mineral_cargo(concreteOrder.minerals)) return;

                        const auto requestedLoad = colonist_cargo_mass(fleet->colonists)
                            + mineral_cargo_mass(concreteOrder.minerals) + science_sample_mass(fleet->scienceSamples) + fleet->xenocrystal;
                        if (requestedLoad > fleet_cargo_capacity(next, *fleet) + 0.000001) return;
                        if (!minerals_available(planet->minerals, fleet->minerals, concreteOrder.minerals)) return;

                        if (planet->owner == player) {
                            record_freight_delivery(freight, *planet, {
                                std::max(0.0, fleet->minerals.ironium - concreteOrder.minerals.ironium),
                                std::max(0.0, fleet->minerals.boranium - concreteOrder.minerals.boranium),
                                std::max(0.0, fleet->minerals.germanium - concreteOrder.minerals.germanium)});
                        }
                        transfer_minerals(planet->minerals, fleet->minerals, concreteOrder.minerals);
        } else if constexpr (std::is_same_v<T, TransferCargoOrder>) {
            (void)transfer_cargo(next, player, concreteOrder, freight);
        }
    }, order);
}

std::uint64_t projected_manifest_date(const GameState& state, PlayerId player, FleetId id)
{
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(),
        [&](const Fleet& f) { return f.id == id && f.owner == player; });
    // A predicted position with a link is not evidence of a received report.
    return fleet != state.fleets.end() && fleet_has_instant_link(state, *fleet)
        ? fleet->telemetry.observedTurn : 0;
}

void refresh_projected_manifest(GameState& state, PlayerId player, FleetId id, std::uint64_t receivedTurn)
{
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(),
        [&](const Fleet& f) { return f.id == id && f.owner == player; });
    if (fleet == state.fleets.end()) return;
    // Snapshot the derived composition, but retain the input evidence date.
    // The snapshot helper's connection-based date never establishes freshness.
    fleet->telemetry = {};
    fleet->telemetry = confirmed_fleet_telemetry(state, *fleet);
    fleet->telemetry.observedTurn = receivedTurn;
}

// Command phase only: share validation and receipt semantics with forecasts.
// No movement, annual work, combat or report delivery is simulated here.
bool apply_fleet_command_order(GameState& state, PlayerId player, const Order& order, std::set<FleetId>& gated,
    bool projection = false)
{
    bool handled = true;
    if(const auto fleet=transport_manual_override(order))release_transport_assignment(state,player,*fleet);
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, SetTransportLineOrder>) {
            (void)set_transport_line(state, player, value);
        } else if constexpr (std::is_same_v<T, AssignTransportLineOrder>) {
            (void)assign_transport_line(state, player, value);
        } else if constexpr (std::is_same_v<T, MoveFleetOrder>) {
            (void)submit_fleet_route_command(
                state,
                player,
                value.fleet,
                value.destination,
                value.warp,
                value.arrivalAction,
                value.queuedWaypoints,
                value.repeatOrders,
                value.targetFleet,
                value.clearRoute);
        } else if constexpr (std::is_same_v<T, EncounterVisitorOrder>) {
            (void)submit_visitor_command(state, player, value);
        } else if constexpr (std::is_same_v<T, SetProbeProgramOrder>) {
            (void)submit_probe_command(state, player, value);
        } else if constexpr (std::is_same_v<T, CreateShipDesignOrder>) {
            create_ship_design(state, player, value);
        } else if constexpr (std::is_same_v<T, RefuelFleetOrder>) {
            const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
                return candidate.id == value.colony && candidate.owner == player;
            });
            const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
                return candidate.id == value.fleet && candidate.owner == player;
            });
            if (planet == state.planets.end() || fleet == state.fleets.end()) return;
            if (!fleet_at_planet(state, *fleet, *planet)) return;
            if (!colony_has_orbital_service(
                    state, planet->id, player,
                    OrbitalStationModule::RefuelingDepot)) {
                return;
            }
            fleet->fuel = fleet_fuel_capacity(state, *fleet);
        } else if constexpr (std::is_same_v<T, ColonizePlanetOrder>) {
            const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
                return candidate.id == value.planet;
            });
            const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
                return candidate.id == value.fleet && candidate.owner == player;
            });
            if (planet == state.planets.end() || fleet == state.fleets.end()) return;
            if (establish_colony(state, *fleet, *planet, value.colonyPlan)) state.fleets.erase(fleet);
        } else if constexpr (std::is_same_v<T, SetRemoteMiningOrder>) {
            (void)submit_fleet_task_command(
                state,
                player,
                value.fleet,
                value.enabled ? FleetTask::RemoteMining : FleetTask::None);
        } else if constexpr (std::is_same_v<T, OrbitalBaseOrder>) {
            (void)submit_orbital_base_command(state, player, value.fleet, value.command);
        } else if constexpr (std::is_same_v<T, SetMobileBaseOrder>) {
            (void)submit_mobile_base_command(state, player, value.fleet, value.program);
        } else if constexpr (std::is_same_v<T, SetFleetTaskOrder>) {
            (void)submit_fleet_task_command(state, player, value.fleet, value.task);
        } else if constexpr (std::is_same_v<T, SetFleetElectronicsOrder>) {
            (void)submit_electronics_command(state, player, value.fleet, value.program);
        } else if constexpr (std::is_same_v<T, MergeFleetsOrder>) {
            const auto receivedTurn = projection ? std::min(projected_manifest_date(state, player, value.source),
                projected_manifest_date(state, player, value.destination)) : 0;
            const bool usedGate = gated.contains(value.source)
                || gated.contains(value.destination);
            if (merge_fleets(state, player, value)) {
                if (usedGate) gated.insert(value.destination);
                if (projection) refresh_projected_manifest(state, player, value.destination, receivedTurn);
            }
        } else if constexpr (std::is_same_v<T, SplitFleetOrder>) {
            const auto receivedTurn = projection ? projected_manifest_date(state, player, value.source) : 0;
            const auto detached = state.nextFleetId;
            if (split_fleet(state, player, value)) {
                if (gated.contains(value.source)) gated.insert(detached);
                if (projection) {
                    refresh_projected_manifest(state, player, value.source, receivedTurn);
                    refresh_projected_manifest(state, player, detached, receivedTurn);
                }
            }
        } else handled = false;
    }, order);
    return handled;
}

} // namespace

// UI/AI callers supply a PlayerView: command validation must not inspect host truth.
GameState projected_cargo_order_state(const GameState& current, const PlayerOrders& pending)
{
    GameState projected = current;
    for (auto& p : projected.planets)
        p.xenocrystal = known_xenocrystal(current, pending.player, p.id).value_or(XenocrystalObservation{}).site;
    std::vector<ColonyFreightDelivery> unusedFreight;
    std::set<FleetId> gated;
    for (const auto& order : pending.orders) {
        if (apply_fleet_command_order(projected, pending.player, order, gated, true)) continue;
        if (const auto* gate = std::get_if<GateFleetOrder>(&order)) {
            if (!gated.contains(gate->fleet) && preview_stargate_transfer(projected, pending.player, *gate))
                gated.insert(gate->fleet);
        } else if (const auto* sample = std::get_if<TransferScienceSampleOrder>(&order)) {
            (void)preview_science_sample_transfer(projected, pending.player, *sample);
        } else if (const auto* crystal = std::get_if<TransferXenocrystalOrder>(&order)) {
            const auto involvesBase = [&](FleetId id) {
                const auto* f = find_service_fleet(projected, id);
                return f && mobile_base_hulls(projected, *f) > 0;
            };
            // Base exchanges happen after movement and colony production.
            // They cannot finance this year's lab work or an earlier transfer.
            if (!involvesBase(crystal->source.fleet) && !involvesBase(crystal->destination.fleet))
                (void)preview_xenocrystal_transfer(projected, pending.player, *crystal, &pending);
        } else if (const auto* process = std::get_if<ProcessScienceSampleOrder>(&order)) {
            (void)process_science_sample(projected, pending.player, *process);
        } else if (std::holds_alternative<SetFleetColonistsOrder>(order)
            || std::holds_alternative<SetFleetMineralCargoOrder>(order)
            || std::holds_alternative<TransferCargoOrder>(order)) {
            apply_cargo_order(projected, pending.player, order, unusedFreight);
        }
    }
    return projected;
}

GameState planned_ship_design_state(const GameState& state, const PlayerOrders& pending)
{
    auto result = state;
    for (const auto& order : pending.orders) {
        if (const auto* create = std::get_if<CreateShipDesignOrder>(&order))
            create_ship_design(result, pending.player, *create);
    }
    return result;
}

const ShipDesign* resolve_ship_design_order(
    const GameState& state, PlayerId player, const QueueShipDesignOrder& order)
{
    if (!order.pendingDesignName.empty()) {
        const auto it = std::find_if(state.shipDesigns.begin(), state.shipDesigns.end(), [&](const ShipDesign& design) {
            return design.owner == player && design.name == order.pendingDesignName;
        });
        return it == state.shipDesigns.end() ? nullptr : &*it;
    }
    const auto* design = find_ship_design(state, order.design);
    return design && design->owner == player ? design : nullptr;
}

TurnResult TurnProcessor::process_with_events(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    GameState next = current;
    dispatch_fleet_report_outbox(next);
    // Establish a baseline for headless callers too, before overdue reports
    // are delivered at the current planning boundary.
    if (std::any_of(next.players.begin(), next.players.end(), [&](const auto& player) {
            return player.history.empty() || player.history.back().turn != next.turn;
        })) record_empire_turn_statistics(next);
    std::vector<FleetMovementFuel> movementFuel;
    std::vector<GameEvent> events = deliver_due_survey_reports(next);
    auto wormholeEvents = deliver_wormhole_reports(next);
    events.insert(events.end(), wormholeEvents.begin(), wormholeEvents.end());
    auto visitorEvents = deliver_visitor_reports(next);
    events.insert(events.end(), visitorEvents.begin(), visitorEvents.end());
    auto deliveredReports = deliver_due_player_reports(next);
    events.insert(events.end(), deliveredReports.begin(), deliveredReports.end());

    // Persisted traffic may already be due at this planning boundary.
    deliver_due_fleet_telemetry(next);
    deliver_due_fleet_commands(next);

    refresh_planetary_engineering(next);
    generate_fleet_fuel(next);
    maintain_outposts(next, next.turn + 1);
    // Friendly orbital refueling services top up every docked fleet before
    // orders and movement are resolved. A basic dock without the module does
    // not refuel, leaving room for cheaper station hulls later.
    refuel_fleets_at_orbital_services(next);
    advance_minefields(next);
    const auto extraction = mine_colonies(next);
    std::vector<ColonyFreightDelivery> freight;

    struct BaseServiceTransfer {
        PlayerId player;
        std::variant<TransferCargoOrder, TransferXenocrystalOrder, TransferFleetFuelOrder> order;
        FleetId commandReceiver{};
    };
    std::vector<BaseServiceTransfer> baseTransfers;
    std::vector<std::pair<PlayerId, TransferFleetFuelOrder>> baseFuel;
    const auto enqueueReceivedTransfer = [&](const PendingMobileBaseTransfer& packet) {
        if (packet.xenocrystal) baseTransfers.push_back({packet.owner,
            TransferXenocrystalOrder{{0, packet.source}, {0, packet.destination}, packet.xenocrystal}, packet.base});
        else if (packet.fuelOnly) baseTransfers.push_back({packet.owner,
            TransferFleetFuelOrder{packet.source, packet.destination, packet.fuelAmount, packet.sourceReserve}, packet.base});
        else baseTransfers.push_back({packet.owner,
            TransferCargoOrder{{0, packet.source}, {0, packet.destination}, packet.colonists, packet.minerals}, packet.base});
    };
    // Commands already due at this boundary precede new submissions, retaining
    // their saved order. New immediate commands join the same queue in place,
    // alongside surface exchanges; no cargo moves until the service phase.
    for (const auto& packet : next.pendingMobileBaseTransfers)
        if (packet.deliveryTurn <= next.turn) enqueueReceivedTransfer(packet);
    std::erase_if(next.pendingMobileBaseTransfers, [&](const auto& packet) { return packet.deliveryTurn <= next.turn; });
    const auto acceptTransfer = [&](PlayerId owner, FleetId source, FleetId destination,
                                    std::uint64_t colonists, MineralCargo minerals, double fuel, double reserve, bool fuelOnly,
                                    std::uint32_t xenocrystal = 0) {
        if (xenocrystal > kMaximumXenocrystal) return;
        if (fuelOnly ? (!std::isfinite(fuel) || fuel<=0 || !std::isfinite(reserve) || reserve<0)
                     : !valid_mineral_cargo(minerals)) return;
        const auto* from=find_service_fleet(next,source); const auto* to=find_service_fleet(next,destination);
        if (!from || !to || from->owner!=owner || to->owner!=owner || from==to) return;
        const auto* base=mobile_base_hulls(next,*from)>0?from:mobile_base_hulls(next,*to)>0?to:nullptr;
        if (!base) return;
        record_command_transmission(next,owner,base->position,next.turn);
        const PendingMobileBaseTransfer packet{owner,base->id,source,destination,next.turn,
            next.turn+communication_delay_turns(next,owner,base->position),colonists,minerals,fuel,reserve,fuelOnly,xenocrystal};
        if (packet.deliveryTurn <= next.turn) enqueueReceivedTransfer(packet);
        else next.pendingMobileBaseTransfers.push_back(packet);
    };

    std::set<FleetId> gatedThisTurn; // One physical gate hop per fleet and year.
    for (const auto& submission : submitted_orders) {
        for (const auto& order : submission.orders) {
            if (apply_fleet_command_order(next, submission.player, order, gatedThisTurn)) continue;
            std::visit(
                [&](const auto& concreteOrder) {
                    using T = std::decay_t<decltype(concreteOrder)>;

                    if constexpr (std::is_same_v<T, GateFleetOrder>) {
                        if (gatedThisTurn.contains(concreteOrder.fleet)) return;
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(),
                            [&](const auto& f) { return f.id == concreteOrder.fleet && f.owner == submission.player; });
                        if (fleet == next.fleets.end()) return;
                        if (communication_delay_turns(next, submission.player, fleet->position) > 0) {
                            const auto* source = find_planet(next, concreteOrder.source);
                            const auto* star = source ? find_star(next, source->star) : nullptr;
                            if (star) {
                                FleetArrivalAction action{FleetArrivalActionKind::TransitStargate};
                                action.gateSource = concreteOrder.source; action.gateDestination = concreteOrder.destination;
                                action.gateResonant = concreteOrder.resonant;
                                (void)submit_fleet_route_command(next, submission.player, fleet->id,
                                    star->position, fleet->warp, action, {});
                            }
                            return;
                        }
                        const bool transferred = transfer_fleet_through_stargate(next, submission.player,
                            concreteOrder.fleet, concreteOrder.source, concreteOrder.destination, concreteOrder.resonant);
                        if (transferred) { pause_probe(*fleet); fleet->visitor = {}; }
                        const auto target = transferred ? concreteOrder.destination : concreteOrder.source;
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(),
                            [&](const auto& p) { return p.id == target && p.owner == submission.player; });
                        queue_player_report(next, submission.player, transferred ? PlayerReportKind::StargateTransit
                                : PlayerReportKind::StargateTransitFailed,
                            fleet->position, next.turn + 1, planet == next.planets.end() ? 0 : planet->star,
                            planet == next.planets.end() ? 0 : planet->id, fleet->id, 0,
                            ProductionKind::ColonyShip, transferred && concreteOrder.resonant ? kResonantGateCrystals : 0,
                            ResearchField::Electronics, 0,
                            fleet->minerals, fleet->colonists);
                        if (transferred) gatedThisTurn.insert(concreteOrder.fleet);
                    } else if constexpr (std::is_same_v<T, SetPlanetaryEngineeringOrder>) {
                        const auto p=std::find_if(next.planets.begin(),next.planets.end(),[&](const auto& p){return p.id==concreteOrder.colony && p.owner==submission.player;});
                        if(p!=next.planets.end()) (void)apply_planetary_engineering_order(next,*p,concreteOrder);
                    } else if constexpr (std::is_same_v<T, QueueStationOrder>) {
                        (void)queue_station_construction(next,submission.player,concreteOrder);
                    } else if constexpr (std::is_same_v<T, TransferScienceSampleOrder>) {
                        (void)transfer_science_sample(next, submission.player, concreteOrder);
                    } else if constexpr (std::is_same_v<T, TransferXenocrystalOrder>) {
                        const auto involvesBase = [&](FleetId id) {
                            const auto* f = find_service_fleet(next, id);
                            return f && mobile_base_hulls(next, *f) > 0;
                        };
                        if (involvesBase(concreteOrder.source.fleet) || involvesBase(concreteOrder.destination.fleet)) {
                            if (concreteOrder.source.fleet && concreteOrder.destination.fleet
                                && !concreteOrder.source.planet && !concreteOrder.destination.planet) {
                                if (concreteOrder.amount) acceptTransfer(submission.player, concreteOrder.source.fleet,
                                    concreteOrder.destination.fleet, 0, {}, 0, 0, false, concreteOrder.amount);
                            } else baseTransfers.push_back({submission.player, concreteOrder, 0});
                        } else (void)transfer_xenocrystals(next, submission.player, concreteOrder);
                    } else if constexpr (std::is_same_v<T, SetResonanceLaboratoryOrder>) {
                        const auto p = std::find_if(next.planets.begin(), next.planets.end(), [&](const auto& p) { return p.id == concreteOrder.colony && p.owner == submission.player; });
                        if (p != next.planets.end()) (void)apply_resonance_order(next, *p, concreteOrder);
                    } else if constexpr (std::is_same_v<T, ProcessScienceSampleOrder>) {
                        (void)process_science_sample(next, submission.player, concreteOrder);
                    } else if constexpr (std::is_same_v<T, QueueProductionOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end()) return;

                        if (concreteOrder.kind == ProductionKind::Factory) {
                            planet->productionQueue.push_back({ProductionKind::Factory, kFactoryCost, 0});
                        } else if (concreteOrder.kind == ProductionKind::Mine) {
                            planet->productionQueue.push_back({ProductionKind::Mine, kMineCost, 0});
                        } else if (concreteOrder.kind == ProductionKind::OrbitalStation) {
                            const bool alreadyQueued = std::any_of(
                                planet->productionQueue.begin(), planet->productionQueue.end(),
                                [](const ProductionItem& item) {
                                    return item.kind == ProductionKind::OrbitalStation;
                                });
                            if (!find_orbital_station_at_planet(next, planet->id) && !orbital_base_at(next, planet->id) && !alreadyQueued) {
                                planet->productionQueue.push_back({
                                    ProductionKind::OrbitalStation, kOrbitalDockCost, 0});
                            }
                        } else if (engineering_kind(concreteOrder.kind) || recovery_kind(concreteOrder.kind) || concreteOrder.kind == ProductionKind::Terraforming || concreteOrder.kind == ProductionKind::ColonyDefense) {
                            (void)apply_production_batch(next, *planet, {planet->id, concreteOrder.kind});
                        } else if (concreteOrder.kind == ProductionKind::Research) {
                            return;
                        } else if (concreteOrder.kind == ProductionKind::ColonyShip) {
                            if (!colony_has_orbital_service(next, planet->id, submission.player,
                                    OrbitalStationModule::Shipyard)) return;
                            const auto design = std::find_if(next.shipDesigns.begin(), next.shipDesigns.end(),
                                [&](const ShipDesign& candidate) {
                                    return candidate.owner == submission.player && ship_design_can_colonize(candidate);
                                });
                            if (design != next.shipDesigns.end())
                                planet->productionQueue.push_back({ProductionKind::ColonyShip, ship_design_cost(*design), design->id});
                        }
                    } else if constexpr (std::is_same_v<T, QueueProductionBatchOrder>
                        || std::is_same_v<T, ApplyProductionTemplateOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(),
                            [&](const auto& candidate) { return candidate.id == concreteOrder.colony
                                && candidate.owner == submission.player; });
                        if (planet == next.planets.end()) return;
                        if constexpr (std::is_same_v<T, QueueProductionBatchOrder>)
                            (void)apply_production_batch(next, *planet, concreteOrder);
                        else if (const auto* player = find_player(next, submission.player))
                            (void)apply_production_template(*planet, *player, concreteOrder.name);
                    } else if constexpr (std::is_same_v<T, SetProductionTemplateOrder>) {
                        if (auto* player = mutable_player(next, submission.player))
                            (void)set_production_template(*player, concreteOrder);
                    } else if constexpr (std::is_same_v<T, SetColonyResearchOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end()) return;
                        std::erase_if(planet->productionQueue, [](const ProductionItem& item) {
                            return item.kind == ProductionKind::Research;
                        });
                    } else if constexpr (std::is_same_v<T, SetResearchPlanOrder>) {
                        auto* player = mutable_player(next, submission.player);
                        if (!player
                            || (concreteOrder.active && !valid_research_field(concreteOrder.focus))
                            || (!concreteOrder.active && !concreteOrder.queuedFocuses.empty())
                            || !std::all_of(
                                concreteOrder.queuedFocuses.begin(),
                                concreteOrder.queuedFocuses.end(),
                                valid_research_field)) {
                            return;
                        }
                        player->technology.researchActive = concreteOrder.active;
                        if (concreteOrder.active) player->technology.focus = concreteOrder.focus;
                        player->technology.queuedFocuses = concreteOrder.queuedFocuses;
                    } else if constexpr (std::is_same_v<T, SetResearchAllocationOrder>) {
                        auto* player = mutable_player(next, submission.player);
                        if (!player || concreteOrder.percent > 100) return;
                        player->technology.researchAllocationPercent = concreteOrder.percent;
                    } else if constexpr (std::is_same_v<T, QueueShipDesignOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        const auto* design = resolve_ship_design_order(next, submission.player, concreteOrder);
                        if (planet == next.planets.end() || !design || design->owner != submission.player
                            || !colony_has_orbital_service(next, planet->id, submission.player,
                                OrbitalStationModule::Shipyard)
                            || !ship_design_valid(*design)) return;
                        planet->productionQueue.push_back({ProductionKind::ColonyShip, ship_design_cost(*design), design->id});
                    } else if constexpr (std::is_same_v<T, ReorderProductionQueueOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end()
                            || concreteOrder.fromIndex >= planet->productionQueue.size()
                            || concreteOrder.toIndex >= planet->productionQueue.size()
                            || concreteOrder.fromIndex == concreteOrder.toIndex) {
                            return;
                        }
                        auto item = std::move(planet->productionQueue[concreteOrder.fromIndex]);
                        planet->productionQueue.erase(
                            planet->productionQueue.begin() + concreteOrder.fromIndex);
                        planet->productionQueue.insert(
                            planet->productionQueue.begin() + concreteOrder.toIndex,
                            std::move(item));
                    } else if constexpr (std::is_same_v<T, CancelProductionOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end() || concreteOrder.index >= planet->productionQueue.size()) return;
                        planet->productionQueue.erase(planet->productionQueue.begin() + concreteOrder.index);
                        if (concreteOrder.index == 0) {
                            planet->productionWaitingForMinerals = false;
                            planet->productionWaitingForShipyard = false;
                        }
                    } else if constexpr (std::is_same_v<T, SetFleetColonistsOrder>
                        || std::is_same_v<T, SetFleetMineralCargoOrder>
                        || std::is_same_v<T, TransferCargoOrder>) {
                        const auto local=[&](FleetId id) {
                            const auto* f=find_service_fleet(next,id);
                            return !id || (f && f->owner==submission.player && communication_delay_turns(next,submission.player,f->position)==0);
                        };
                        if constexpr(std::is_same_v<T,TransferCargoOrder>) {
                            const auto* a=find_service_fleet(next,concreteOrder.source.fleet);
                            const auto* b=find_service_fleet(next,concreteOrder.destination.fleet);
                            const bool base=(a && mobile_base_hulls(next,*a)) || (b && mobile_base_hulls(next,*b));
                            if(!base && (!local(concreteOrder.source.fleet) || !local(concreteOrder.destination.fleet)))return;
                        } else if(!local(concreteOrder.fleet))return;
                        if constexpr (std::is_same_v<T, TransferCargoOrder>) {
                            const auto involvesBase = [&](FleetId id) {
                                const auto* f = find_service_fleet(next, id);
                                return f && mobile_base_hulls(next, *f) > 0;
                            };
                            if (involvesBase(concreteOrder.source.fleet) || involvesBase(concreteOrder.destination.fleet)) {
                                if (concreteOrder.source.fleet && concreteOrder.destination.fleet
                                    && !concreteOrder.source.planet && !concreteOrder.destination.planet)
                                    acceptTransfer(submission.player,concreteOrder.source.fleet,concreteOrder.destination.fleet,
                                        concreteOrder.colonists,concreteOrder.minerals,0,0,false);
                                else baseTransfers.push_back({submission.player, concreteOrder, 0});
                            } else apply_cargo_order(next, submission.player, order, freight);
                        } else apply_cargo_order(next, submission.player, order, freight);
                    } else if constexpr (std::is_same_v<T, TransferFleetFuelOrder>) {
                        acceptTransfer(submission.player,concreteOrder.source,concreteOrder.destination,0,{},concreteOrder.amount,concreteOrder.sourceReserve,true);
                    } else if constexpr (std::is_same_v<T, RenameFleetOrder>) {
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(),
                            [&](const Fleet& candidate) {
                                return candidate.id == concreteOrder.fleet
                                    && candidate.owner == submission.player;
                            });
                        const bool hasVisibleCharacter = std::any_of(
                            concreteOrder.name.begin(), concreteOrder.name.end(),
                            [](unsigned char value) { return !std::isspace(value); });
                        if (fleet != next.fleets.end() && hasVisibleCharacter
                            && concreteOrder.name.size() <= 80) fleet->name = concreteOrder.name;
                    }
                },
                order);
        }
    }

    for(auto& planet:next.planets) decay_planet_contamination(planet);
    const auto remoteExtraction = mine_uncolonized_planets(next);
    mine_xenocrystals(next);
    advance_wormholes(next);
    resolve_wormhole_approaches(next);
    advance_survey_probes(next);
    prepare_visitor_navigation(next);
    advance_fleets(next, freight, movementFuel);
    advance_visitors(next);
    resolve_bombardments(next, next.turn + 1);
    run_strategic_tasks(next, next.turn + 1);
    deploy_outposts(next, next.turn + 1);
    observe_current_wormholes(next, next.turn + 1);
    refresh_planetary_engineering(next);
    observe_current_sensor_coverage(next, next.turn + 1);
    observe_enemy_fleet_contacts(next, next.turn + 1);
    std::vector<std::pair<PlayerId, std::uint32_t>> researchByPlayer;
    for (auto& planet : next.planets) {
        const auto contribution = run_colony_production(next, planet) + retained_sample_research(next, planet);
        const auto parallel = resonance_research(next, planet);
        for (std::size_t field = 0; field < parallel.size(); ++field) if (parallel[field]) {
            const auto* star = find_star(next, planet.star);
            if (star) queue_player_report(next, planet.owner, PlayerReportKind::ScientificData, star->position,
                next.turn + 1, star->id, planet.id, 0, 0, ProductionKind::ResonanceLaboratory, parallel[field], ResearchField(field));
        }
        if (contribution == 0) continue;
        const auto total = std::find_if(researchByPlayer.begin(), researchByPlayer.end(), [&](const auto& entry) {
            return entry.first == planet.owner;
        });
        if (total == researchByPlayer.end()) researchByPlayer.emplace_back(planet.owner, contribution);
        else total->second += contribution;
    }
    for (const auto& [player, points] : researchByPlayer) {
        apply_research_points(next, player, points, next.turn + 1, events);
    }
    // Fleets which reach a serviced colony finish the year with full tanks.
    // Running this after production also activates a newly completed depot at
    // the planning boundary, without requiring a separate Refuel order.
    refuel_fleets_at_orbital_services(next);
    // Service validation uses the post-movement world, in receipt order.
    for (const auto& transfer : baseTransfers) {
        if (transfer.commandReceiver) {
            const auto* base = find_service_fleet(next, transfer.commandReceiver);
            if (!base || base->owner != transfer.player || !mobile_base_dock(next, *base)) continue;
        }
        const auto dockingAllowed = [&](FleetId id) {
            const auto* f = find_service_fleet(next, id);
            return !f || mobile_base_hulls(next, *f) == 0
                || mobile_base_service_error(next, *f, next.turn + 1).empty();
        };
        std::visit([&](const auto& cargo) {
            if constexpr (std::is_same_v<std::decay_t<decltype(cargo)>, TransferFleetFuelOrder>)
                baseFuel.emplace_back(transfer.player, cargo);
            else {
                if (!dockingAllowed(cargo.source.fleet) || !dockingAllowed(cargo.destination.fleet)) return;
                if constexpr (std::is_same_v<std::decay_t<decltype(cargo)>, TransferXenocrystalOrder>)
                    (void)transfer_xenocrystals(next, transfer.player, cargo, transfer.commandReceiver != 0);
                else apply_cargo_order(next, transfer.player, cargo, freight);
            }
        }, transfer.order);
    }
    // Repair once at the end of the year, after movement and construction.
    // Shipyards supersede onboard repair; fleets recover only once per year.
    repair_fleet_damage(next, current);
    for (const auto& [player, fuel] : baseFuel) (void)transfer_mobile_base_fuel(next, player, fuel, next.turn + 1);
    service_orbital_bases(next, next.turn + 1);
    service_mobile_bases(next, next.turn + 1);
    refresh_planetary_engineering(next);
    grow_colonies(next);
    refresh_planetary_engineering(next);
    publish_planetary_engineering_reports(next,current);
    publish_habitat_reports(next,current);
    publish_exotic_material_reports(next,current);

    // The ledger counts physical deliveries; publish one manifest per receiving
    // colony instead of one message for every split cargo order or waypoint.
    std::sort(freight.begin(), freight.end(), [](const auto& left, const auto& right) {
        if (left.owner != right.owner) return left.owner < right.owner;
        return left.planet < right.planet;
    });
    for (std::size_t index = 0; index < freight.size();) {
        const auto owner = freight[index].owner;
        const auto planetId = freight[index].planet;
        MineralCargo minerals{};
        std::uint64_t colonists = 0;
        do {
            minerals.ironium += freight[index].minerals.ironium;
            minerals.boranium += freight[index].minerals.boranium;
            minerals.germanium += freight[index].minerals.germanium;
            colonists += freight[index].colonists;
            ++index;
        } while (index < freight.size() && freight[index].owner == owner && freight[index].planet == planetId);
        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [planetId](const Planet& candidate) {
            return candidate.id == planetId;
        });
        const auto* star = planet != next.planets.end() ? find_star(next, planet->star) : nullptr;
        if (star) queue_player_report(next, owner, PlayerReportKind::FreightDelivered,
            star->position, next.turn + 1, star->id, planetId, 0, 0,
            ProductionKind::ColonyShip, 0, ResearchField::Electronics, 0, minerals, colonists);
    }

    // The returned state is the next planning boundary. Commands arriving
    // during this elapsed year become active now but never rewrite movement
    // that has already happened. Telemetry is then emitted from truth.
    ++next.turn;
    deliver_due_fleet_commands(next);
    publish_fleet_telemetry(next, next.turn);
    dispatch_fleet_report_outbox(next);
    observe_strategic_objects(next, next.turn);
    observe_planetary_aftermath(next,next.turn);
    observe_unknown_objects(next,next.turn);
    observe_visitors(next);
    observe_interstellar_medium(next,next.turn);
    observe_emission_pulses(next,next.turn);
    deliver_due_fleet_telemetry(next);
    std::erase_if(next.pendingStationContacts,[&](const auto& c) { return c.deliveryTurn <= next.turn; });
    deliver_strategic_intel(next);
    auto deliveredIntel = deliver_due_survey_reports(next);
    events.insert(events.end(), deliveredIntel.begin(), deliveredIntel.end());
    visitorEvents = deliver_visitor_reports(next);
    events.insert(events.end(), visitorEvents.begin(), visitorEvents.end());
    deliveredReports = deliver_due_player_reports(next);
    events.insert(events.end(), deliveredReports.begin(), deliveredReports.end());
    wormholeEvents = deliver_wormhole_reports(next);
    events.insert(events.end(), wormholeEvents.begin(), wormholeEvents.end());
    record_empire_turn_statistics(next, extraction, true, freight, remoteExtraction);
    for (const auto& event : events) {
        auto player = std::find_if(next.players.begin(), next.players.end(), [&](const Player& candidate) {
            return candidate.id == event.recipient;
        });
        if (player == next.players.end() || player->history.empty()) continue;
        std::optional<HistoryMilestoneKind> kind;
        switch (event.kind) {
        case GameEventKind::ColonyFounded: kind = HistoryMilestoneKind::ColonyFounded; break;
        case GameEventKind::ColonyLost:
            kind = event.losses && event.losses->kind == OperationalLossKind::Ground
                ? HistoryMilestoneKind::GroundDefenseLost : HistoryMilestoneKind::ColonyLost;
            break;
        case GameEventKind::EngineeringProgress:
            kind=event.quantity==0?HistoryMilestoneKind::EngineeringStarted:event.quantity==100?HistoryMilestoneKind::EngineeringCompleted:HistoryMilestoneKind::EngineeringProgress; break;
        case GameEventKind::EngineeringPaused: kind=HistoryMilestoneKind::EngineeringPaused; break;
        case GameEventKind::EngineeringResumed: kind=HistoryMilestoneKind::EngineeringResumed; break;
        case GameEventKind::EngineeringCaptured: kind=HistoryMilestoneKind::EngineeringCaptured; break;
        case GameEventKind::EngineeringDismantled: kind=HistoryMilestoneKind::EngineeringDismantled; break;
        case GameEventKind::ResearchLevelCompleted: kind = HistoryMilestoneKind::ResearchCompleted; break;
        case GameEventKind::FleetStalledForFuel: kind = HistoryMilestoneKind::FleetStalledForFuel; break;
        case GameEventKind::EnemyFleetDetected: kind = HistoryMilestoneKind::EnemyFleetDetected; break;
        case GameEventKind::EnemyFleetLost: kind = HistoryMilestoneKind::EnemyFleetLost; break;
        case GameEventKind::EnemyFleetReclassified: kind = HistoryMilestoneKind::EnemyFleetReclassified; break;
        case GameEventKind::SensorInterferenceChanged: kind = HistoryMilestoneKind::SensorInterferenceChanged; break;
        case GameEventKind::GroundInvasionWon: kind = HistoryMilestoneKind::GroundInvasionWon; break;
        case GameEventKind::GroundInvasionLost: kind = HistoryMilestoneKind::GroundInvasionLost; break;
        case GameEventKind::GroundDefenseWon: kind = HistoryMilestoneKind::GroundDefenseWon; break;
        case GameEventKind::SpaceBattle: kind = HistoryMilestoneKind::SpaceBattle; break;
        case GameEventKind::Bombardment: kind = HistoryMilestoneKind::Bombardment; break;
        case GameEventKind::PlanetaryAftermath: kind=HistoryMilestoneKind::PlanetaryAftermath; break;
        case GameEventKind::UnknownObjectObserved: kind=HistoryMilestoneKind::UnknownObjectReported; break;
        case GameEventKind::ScientificData:
            if(event.objectObservation) kind=HistoryMilestoneKind::UnknownObjectStudied;
            break;
        case GameEventKind::ProductionCompleted:
            if(reconstruction_kind(event.productionKind)) kind=HistoryMilestoneKind::Reconstruction;
            if(event.productionKind==ProductionKind::CleanupContamination) kind=HistoryMilestoneKind::ContaminationCleanup;
            break;
        case GameEventKind::MineStrike: kind = HistoryMilestoneKind::MineStrike; break;
        default: break;
        }
        if (!kind) continue;
        const bool fleetEvent = *kind == HistoryMilestoneKind::FleetStalledForFuel
            || *kind == HistoryMilestoneKind::EnemyFleetDetected
            || *kind == HistoryMilestoneKind::EnemyFleetLost
            || *kind == HistoryMilestoneKind::EnemyFleetReclassified
            || (*kind == HistoryMilestoneKind::SensorInterferenceChanged && event.fleet!=0)
            || *kind == HistoryMilestoneKind::SpaceBattle
            || *kind == HistoryMilestoneKind::MineStrike;
        const bool objectEvent=*kind==HistoryMilestoneKind::UnknownObjectReported || *kind==HistoryMilestoneKind::UnknownObjectStudied;
        if (objectEvent ? !event.objectObservation : *kind == HistoryMilestoneKind::ResearchCompleted ? event.technologyLevel == 0
            : fleetEvent ? (event.fleet == 0 && (*kind != HistoryMilestoneKind::SpaceBattle || event.planet == 0)) : event.planet == 0) continue;
        auto snapshot = std::prev(player->history.end());
        const auto lossKind = milestone_loss_kind(*kind);
        if (*kind>=HistoryMilestoneKind::EngineeringStarted || *kind == HistoryMilestoneKind::SpaceBattle || *kind==HistoryMilestoneKind::Reconstruction
            || *kind==HistoryMilestoneKind::ContaminationCleanup || *kind==HistoryMilestoneKind::PlanetaryAftermath || objectEvent || lossKind) {
            // Packets already due on load are received at the starting boundary,
            // not in the new year resolved by this call.
            snapshot = std::find_if(player->history.begin(), player->history.end(),
                [&](const auto& record) { return record.turn == event.turn; });
            if (snapshot == player->history.end()) continue;
            if (std::any_of(player->history.begin(), player->history.end(), [&](const auto& record) {
                    return std::any_of(record.milestones.begin(), record.milestones.end(),
                        [&](const auto& marker) { return marker.eventId == event.id; });
                })) continue;
        }
        auto& milestones = snapshot->milestones;
        if (std::any_of(milestones.begin(), milestones.end(), [&](const auto& recorded) {
                return recorded.eventId == event.id;
            })) continue;
        const bool unmeasuredBattles = !snapshot->battleStatisticsRecorded
            && std::any_of(milestones.begin(), milestones.end(), [](const auto& marker) {
                return marker.kind == HistoryMilestoneKind::SpaceBattle;
            });
        if (*kind == HistoryMilestoneKind::SpaceBattle && event.battle && !unmeasuredBattles) {
            snapshot->battleStatisticsRecorded = true;
            auto& totals = snapshot->battles;
            ++totals.reports;
            for (const auto& unit : event.battle->units) {
                const auto lost = std::uint64_t(unit.initialShips) - unit.survivingShips;
                if (unit.station) {
                    if (unit.owner == player->id) totals.ownStationsLost += lost;
                    else totals.opposingStationsLost += lost;
                } else if (unit.owner == player->id) totals.ownShipsLost += lost;
                else totals.opposingShipsLost += lost;
            }
        }
        if (lossKind) {
            auto& totals = snapshot->operationalLosses[static_cast<std::size_t>(*lossKind)];
            const bool unmeasured = !totals.recorded
                && std::any_of(milestones.begin(), milestones.end(), [&](const auto& marker) {
                    return milestone_loss_kind(marker.kind) == lossKind;
                });
            if (!event.losses || event.losses->kind != *lossKind || unmeasured) {
                // An old packet has no exact casualty summary. Keep the entire
                // category unknown rather than present a partially measured year.
                totals = {};
            } else {
                totals.recorded = true;
                ++totals.reports;
                totals.ownColonistsLost += event.losses->ownColonistsLost;
                totals.opposingColonistsLost += event.losses->opposingColonistsLost;
                totals.ownInstallationsLost += event.losses->ownInstallationsLost;
                totals.opposingInstallationsLost += event.losses->opposingInstallationsLost;
                totals.ownShipsLost += event.losses->ownShipsLost;
            }
        }
        milestones.push_back({event.id, event.observedTurn, *kind,
            event.planet, event.researchField, event.technologyLevel, event.fleet,
            event.objectObservation ? event.objectObservation->id : UnknownObjectId{0}});
    }
    return {std::move(next), std::move(events), std::move(movementFuel)};
}

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    return process_with_events(current, submitted_orders).state;
}

} // namespace suns
