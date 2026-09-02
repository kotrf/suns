#include "suns/turn_processor.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace suns {

std::uint32_t production_item_cost(const GameState& state, const ProductionItem& item)
{
    if (item.kind == ProductionKind::Research) return 0;
    if (item.kind == ProductionKind::Factory) return kFactoryCost;
    if (item.kind == ProductionKind::Mine) return kMineCost;
    if (item.kind == ProductionKind::OrbitalStation) return kOrbitalDockCost;
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

std::uint8_t initial_warp_for_design(const ShipDesign& design)
{
    const auto maxWarp = ship_design_max_warp(design);
    if (maxWarp == 0) return 1;
    const auto preferred = ship_design_can_colonize(design) ? kColonyShipCruiseWarp : kScoutCruiseWarp;
    return std::min(maxWarp, preferred);
}

bool design_name_exists(const GameState& state, PlayerId owner, const std::string& name)
{
    return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [&](const ShipDesign& design) {
        return design.owner == owner && design.name == name;
    });
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

void mine_colonies(GameState& state)
{
    for (auto& planet : state.planets) {
        if (planet.owner == 0) continue;
        const auto mined = projected_mineral_mining(state, planet);
        planet.minerals.ironium += mined.ironium;
        planet.minerals.boranium += mined.boranium;
        planet.minerals.germanium += mined.germanium;
    }
}

std::optional<std::uint32_t> complete_production(
    GameState& state, Planet& planet, const ProductionItem& item)
{
    if (item.kind == ProductionKind::Research) return std::nullopt;
    if (item.kind == ProductionKind::Factory) {
        ++planet.industry;
        return FleetId{0};
    }
    if (item.kind == ProductionKind::Mine) {
        ++planet.mines;
        return FleetId{0};
    }
    if (item.kind == ProductionKind::OrbitalStation) {
        if (find_orbital_station_at_planet(state, planet.id)) return std::nullopt;
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
        initial_warp_for_design(*design),
        ship_design_fuel_capacity(*design),
        0,
    });
    state.fleets.back().ships = {{design->id, 1}};
    return id;
}

std::uint32_t run_colony_production(GameState& state, Planet& planet)
{
    if (planet.owner == 0) return 0;
    std::erase_if(planet.productionQueue, [](const ProductionItem& item) {
        return item.kind == ProductionKind::Research;
    });
    if (planet.productionQueue.empty()) planet.productionWaitingForMinerals = false;

    const auto* player = find_player(state, planet.owner);
    const bool researchActive = player && player->technology.researchActive;
    const auto output = colony_output(planet);
    const auto allocated = researchActive
        ? static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(output) * player->technology.researchAllocationPercent / 100U)
        : 0U;
    std::uint32_t researchProduced = allocated;
    std::uint32_t available = output - allocated;
    while (!planet.productionQueue.empty()) {
        auto& item = planet.productionQueue.front();
        if (item.kind == ProductionKind::ColonyShip
            && !colony_has_orbital_service(
                state, planet.id, planet.owner, OrbitalStationModule::Shipyard)) {
            if (!planet.productionWaitingForShipyard) {
                if (const auto* star = find_star(state, planet.star)) {
                    const auto blockedDesign = item.shipDesign != 0
                        ? item.shipDesign
                        : kColonyShipDesignId;
                    queue_player_report(
                        state,
                        planet.owner,
                        PlayerReportKind::ProductionWaitingForShipyard,
                        star->position,
                        state.turn + 1,
                        planet.star,
                        planet.id,
                        0,
                        blockedDesign,
                        item.kind);
                }
            }
            planet.productionWaitingForShipyard = true;
            break;
        }
        planet.productionWaitingForShipyard = false;
        if (item.remainingCost > 0) {
            planet.productionWaitingForMinerals = false;
            if (available == 0) break;
            const auto spent = std::min(available, item.remainingCost);
            available -= spent;
            item.remainingCost -= spent;
            if (item.remainingCost != 0) break;
        }

        const auto requiredMinerals = production_item_mineral_cost(state, item);
        if (!mineral_cargo_sufficient(planet.minerals, requiredMinerals)) {
            if (!planet.productionWaitingForMinerals) {
                const auto* star = find_star(state, planet.star);
                if (star) {
                    const auto blockedDesign = item.kind == ProductionKind::ColonyShip
                        ? (item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId)
                        : ShipDesignId{0};
                    queue_player_report(
                        state,
                        planet.owner,
                        PlayerReportKind::ProductionWaitingForMinerals,
                        star->position,
                        state.turn + 1,
                        planet.star,
                        planet.id,
                        0,
                        blockedDesign,
                        item.kind);
                }
            }
            planet.productionWaitingForMinerals = true;
            break;
        }
        planet.productionWaitingForMinerals = false;
        subtract_minerals(planet.minerals, requiredMinerals);

        const auto completed = item;
        planet.productionQueue.erase(planet.productionQueue.begin());
        const auto completedObject = complete_production(state, planet, completed);
        if (!completedObject) continue;

        const auto* star = find_star(state, planet.star);
        if (!star) continue;
        std::uint32_t quantity = 0;
        if (completed.kind == ProductionKind::Factory) quantity = planet.industry;
        else if (completed.kind == ProductionKind::Mine) quantity = planet.mines;
        else quantity = *completedObject;
        const auto completedDesign = completed.kind == ProductionKind::ColonyShip
            ? (completed.shipDesign != 0 ? completed.shipDesign : kColonyShipDesignId)
            : ShipDesignId{0};
        queue_player_report(
            state,
            planet.owner,
            PlayerReportKind::ProductionCompleted,
            star->position,
            state.turn + 1,
            planet.star,
            planet.id,
            completed.kind == ProductionKind::ColonyShip ? *completedObject : FleetId{0},
            completedDesign,
            completed.kind,
            quantity);
    }
    if (planet.productionQueue.empty()) {
        planet.productionWaitingForMinerals = false;
        planet.productionWaitingForShipyard = false;
    }
    if (researchActive) researchProduced += available;
    return researchProduced;
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
};

std::optional<CargoEndpointRef> resolve_cargo_endpoint(
    GameState& state, PlayerId player, CargoTransferEndpoint endpoint)
{
    if ((endpoint.planet == 0) == (endpoint.fleet == 0)) return std::nullopt;

    if (endpoint.planet != 0) {
        const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
            return candidate.id == endpoint.planet
                && (candidate.owner == player || candidate.owner == 0);
        });
        if (planet == state.planets.end()) return std::nullopt;
        const auto* star = find_star(state, planet->star);
        if (!star) return std::nullopt;
        return CargoEndpointRef{&*planet, nullptr, star->position};
    }

    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == endpoint.fleet && candidate.owner == player;
    });
    if (fleet == state.fleets.end()) return std::nullopt;
    return CargoEndpointRef{nullptr, &*fleet, fleet->position};
}

std::uint64_t endpoint_colonists(const CargoEndpointRef& endpoint)
{
    return endpoint.planet ? endpoint.planet->population : endpoint.fleet->colonists;
}

MineralCargo& endpoint_minerals(CargoEndpointRef& endpoint)
{
    return endpoint.planet ? endpoint.planet->minerals : endpoint.fleet->minerals;
}

bool transfer_cargo(GameState& state, PlayerId player, const TransferCargoOrder& order)
{
    constexpr double epsilon = 0.000001;
    auto source = resolve_cargo_endpoint(state, player, order.source);
    auto destination = resolve_cargo_endpoint(state, player, order.destination);
    if (!source || !destination || !same_position(source->position, destination->position)) return false;
    if ((source->planet && destination->planet && source->planet == destination->planet)
        || (source->fleet && destination->fleet && source->fleet == destination->fleet)) {
        return false;
    }
    if (!valid_mineral_cargo(order.minerals)) return false;

    // Population belongs only to friendly colonies. Loading from a colony
    // retains one colonist, matching the existing exact-manifest order.
    if (order.colonists > 0) {
        if ((source->planet && source->planet->owner != player)
            || (destination->planet && destination->planet->owner != player)) {
            return false;
        }
        const auto available = endpoint_colonists(*source);
        if (source->planet ? order.colonists >= available : order.colonists > available) return false;
        const auto destinationPopulation = endpoint_colonists(*destination);
        if (order.colonists > std::numeric_limits<std::uint64_t>::max() - destinationPopulation) return false;
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
        if (source->planet) source->planet->population -= order.colonists;
        else source->fleet->colonists -= order.colonists;
        if (destination->planet) destination->planet->population += order.colonists;
        else destination->fleet->colonists += order.colonists;
    }

    auto& destinationMinerals = endpoint_minerals(*destination);
    subtract_minerals(sourceMinerals, order.minerals);
    destinationMinerals.ironium += order.minerals.ironium;
    destinationMinerals.boranium += order.minerals.boranium;
    destinationMinerals.germanium += order.minerals.germanium;
    return true;
}

void mine_uncolonized_planets(GameState& state)
{
    for (const auto& fleet : state.fleets) {
        if (fleet.task != FleetTask::RemoteMining) continue;
        for (auto& planet : state.planets) {
            if (!fleet_at_planet(state, fleet, planet)) continue;
            for (const auto& stack : fleet_ship_stacks(fleet)) {
                const auto* design = find_ship_design(state, stack.design);
                if (!design || !ship_design_available_to_player(state, fleet.owner, *design)
                    || !ship_design_can_remote_mine(*design)) continue;
                const auto mined = projected_remote_mining(state, planet, *design);
                planet.minerals.ironium += mined.ironium * stack.count;
                planet.minerals.boranium += mined.boranium * stack.count;
                planet.minerals.germanium += mined.germanium * stack.count;
            }
            break;
        }
    }
}

Planet* friendly_colony_at_fleet(GameState& state, const Fleet& fleet)
{
    const auto it = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
        return planet.owner == fleet.owner && fleet_at_planet(state, fleet, planet);
    });
    return it == state.planets.end() ? nullptr : &*it;
}

bool establish_colony(GameState& state, Fleet& fleet, Planet& planet)
{
    if (planet.owner != 0 || fleet.colonists == 0 || !fleet_can_colonize(state, fleet)) return false;
    if (survey_level(state, fleet.owner, planet.star) < SurveyLevel::OrbitalSurvey) return false;
    if (fleet_cargo_used(state, fleet) > fleet_cargo_capacity(state, fleet) + 0.000001) return false;
    if (!fleet_at_planet(state, fleet, planet)) return false;

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
    planet.population = fleet.colonists;
    planet.minerals.ironium += fleet.minerals.ironium + salvage.ironium;
    planet.minerals.boranium += fleet.minerals.boranium + salvage.boranium;
    planet.minerals.germanium += fleet.minerals.germanium + salvage.germanium;
    planet.industry = 1;
    planet.productionQueue.clear();
    planet.mines = 0;
    planet.productionWaitingForMinerals = false;
    set_survey_level(
        state, fleet.owner, planet.star, SurveyLevel::GeologicalSurvey, state.turn + 1);
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

bool execute_arrival_action(GameState& state, Fleet& fleet)
{
    if (!fleet.arrivalAction) return false;

    const auto action = *fleet.arrivalAction;
    fleet.arrivalAction.reset();
    if (action.kind == FleetArrivalActionKind::None) return false;

    switch (action.kind) {
    case FleetArrivalActionKind::None:
        return false;
    case FleetArrivalActionKind::LoadAllAvailable: {
        const auto capacity = fleet_cargo_capacity(state, fleet);
        const auto freeSpace = std::max(0.0, capacity - fleet_cargo_used(state, fleet));
        if (action.cargo == FleetCargoKind::Colonists) {
            auto* colony = friendly_colony_at_fleet(state, fleet);
            if (!colony) return false;
            const auto reserve = std::max<std::uint64_t>(1, action.reservePopulation);
            if (colony->population <= reserve) return false;
            const auto available = colony->population - reserve;
            const auto capacityColonists = static_cast<std::uint64_t>(
                std::floor(freeSpace * kColonistsPerCargoUnit + 0.000001));
            const auto load = std::min(available, capacityColonists);
            colony->population -= load;
            fleet.colonists += load;
            return false;
        }

        auto surface = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
            return (planet.owner == 0 || planet.owner == fleet.owner) && fleet_at_planet(state, fleet, planet);
        });
        if (surface == state.planets.end()) return false;
        double* source{};
        double* destination{};
        switch (action.cargo) {
        case FleetCargoKind::Colonists: break;
        case FleetCargoKind::Ironium: source = &surface->minerals.ironium; destination = &fleet.minerals.ironium; break;
        case FleetCargoKind::Boranium: source = &surface->minerals.boranium; destination = &fleet.minerals.boranium; break;
        case FleetCargoKind::Germanium: source = &surface->minerals.germanium; destination = &fleet.minerals.germanium; break;
        }
        if (!source || !destination) return false;
        const auto load = std::min(*source, freeSpace);
        *source -= load;
        *destination += load;
        return false;
    }
    case FleetArrivalActionKind::UnloadAll: {
        if (action.cargo == FleetCargoKind::Colonists) {
            auto* colony = friendly_colony_at_fleet(state, fleet);
            if (!colony) return false;
            colony->population += fleet.colonists;
            fleet.colonists = 0;
            return false;
        }
        auto surface = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
            return (planet.owner == 0 || planet.owner == fleet.owner) && fleet_at_planet(state, fleet, planet);
        });
        if (surface == state.planets.end()) return false;
        switch (action.cargo) {
        case FleetCargoKind::Colonists: break;
        case FleetCargoKind::Ironium:
            surface->minerals.ironium += fleet.minerals.ironium;
            fleet.minerals.ironium = 0.0;
            break;
        case FleetCargoKind::Boranium:
            surface->minerals.boranium += fleet.minerals.boranium;
            fleet.minerals.boranium = 0.0;
            break;
        case FleetCargoKind::Germanium:
            surface->minerals.germanium += fleet.minerals.germanium;
            fleet.minerals.germanium = 0.0;
            break;
        }
        return false;
    }
    case FleetArrivalActionKind::Refuel: {
        auto* colony = friendly_colony_at_fleet(state, fleet);
        if (!colony || !colony_has_orbital_service(
                state, colony->id, fleet.owner, OrbitalStationModule::RefuelingDepot)) {
            return false;
        }
        fleet.fuel = fleet_fuel_capacity(state, fleet);
        return false;
    }
    case FleetArrivalActionKind::Colonize: {
        const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
            return candidate.owner == 0 && fleet_at_planet(state, fleet, candidate);
        });
        return planet != state.planets.end() && establish_colony(state, fleet, *planet);
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

bool fleet_ready_for_reorganization(const Fleet& fleet)
{
    return !fleet.destination
        && fleet.targetFleet == 0
        && fleet.waypointQueue.empty()
        && fleet.pendingCommands.empty()
        && fleet.task == FleetTask::None
        && !fleet.repeatOrders
        && fleet.routeTemplate.empty();
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

    const auto sourceId = source->id;
    auto sourceShips = fleet_ship_stacks(*source);
    sync_fleet_presentation(state, *destination);
    destination->ships.insert(destination->ships.end(), sourceShips.begin(), sourceShips.end());
    destination->fuel += source->fuel;
    destination->colonists += source->colonists;
    destination->minerals.ironium += source->minerals.ironium;
    destination->minerals.boranium += source->minerals.boranium;
    destination->minerals.germanium += source->minerals.germanium;
    sync_fleet_presentation(state, *destination);
    destination->warp = std::min(destination->warp, fleet_max_warp(state, *destination));
    destination->fuel = std::min(destination->fuel, fleet_fuel_capacity(state, *destination));
    destination->telemetry = {};
    destination->telemetryInTransit.clear();
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

    sync_fleet_presentation(state, *source);
    std::uint32_t movedCount = 0;
    for (const auto& moved : requested.ships) {
        const auto available = fleet_ship_count(*source, moved.design);
        if (available < moved.count) return false;
        movedCount += moved.count;
    }
    if (movedCount == 0 || movedCount >= fleet_ship_count(*source)) return false;

    const auto totalFuelCapacity = fleet_fuel_capacity(state, *source);
    const auto movedFuelCapacity = stack_capacity(state, requested.ships, ship_design_fuel_capacity);
    const auto totalCargoCapacity = fleet_cargo_capacity(state, *source);
    const auto movedCargoCapacity = stack_capacity(state, requested.ships, ship_design_cargo_capacity);
    const auto fuelShare = totalFuelCapacity > 0.0 ? movedFuelCapacity / totalFuelCapacity : 0.0;
    const auto cargoShare = totalCargoCapacity > 0.0 ? movedCargoCapacity / totalCargoCapacity : 0.0;

    Fleet detached = *source;
    detached.id = state.nextFleetId++;
    detached.name = source->name + " Detachment " + std::to_string(detached.id);
    detached.ships = requested.ships;
    sync_fleet_presentation(state, detached);
    detached.destination.reset();
    detached.targetFleet = 0;
    detached.arrivalAction.reset();
    detached.waypointQueue.clear();
    detached.pendingCommands.clear();
    detached.telemetry = {};
    detached.telemetryInTransit.clear();
    detached.fuelStalled = false;
    detached.task = FleetTask::None;
    detached.repeatOrders = false;
    detached.routeTemplate.clear();
    detached.fuel = source->fuel * fuelShare;
    detached.colonists = static_cast<std::uint64_t>(std::floor(source->colonists * cargoShare));
    detached.minerals.ironium = source->minerals.ironium * cargoShare;
    detached.minerals.boranium = source->minerals.boranium * cargoShare;
    detached.minerals.germanium = source->minerals.germanium * cargoShare;

    source->fuel -= detached.fuel;
    source->colonists -= detached.colonists;
    source->minerals.ironium -= detached.minerals.ironium;
    source->minerals.boranium -= detached.minerals.boranium;
    source->minerals.germanium -= detached.minerals.germanium;
    for (const auto& moved : requested.ships) {
        const auto stack = std::find_if(source->ships.begin(), source->ships.end(), [&](const FleetShipStack& candidate) {
            return candidate.design == moved.design;
        });
        stack->count -= moved.count;
    }
    sync_fleet_presentation(state, *source);
    source->warp = std::min(source->warp, fleet_max_warp(state, *source));
    source->telemetry = {};
    source->telemetryInTransit.clear();
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

bool finish_fleet_arrival(GameState& state, Fleet& fleet, std::vector<FleetId>& consumedFleets)
{
    fleet.fuelStalled = false;
    fleet.targetFleet = 0;
    if (execute_arrival_action(state, fleet)) {
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

void merge_rendezvous_fleet(GameState& state, Fleet& source, Fleet& destination)
{
    const auto sourceId = source.id;
    auto sourceShips = fleet_ship_stacks(source);
    sync_fleet_presentation(state, destination);
    destination.ships.insert(destination.ships.end(), sourceShips.begin(), sourceShips.end());
    destination.fuel += source.fuel;
    destination.colonists += source.colonists;
    destination.minerals.ironium += source.minerals.ironium;
    destination.minerals.boranium += source.minerals.boranium;
    destination.minerals.germanium += source.minerals.germanium;
    sync_fleet_presentation(state, destination);
    destination.warp = std::min(destination.warp, fleet_max_warp(state, destination));
    destination.fuel = std::min(destination.fuel, fleet_fuel_capacity(state, destination));

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
}

bool resolve_fleet_target_arrival(
    GameState& state,
    Fleet& fleet,
    std::vector<FleetId>& consumedFleets)
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
        merge_rendezvous_fleet(state, fleet, *target);
        consumedFleets.push_back(fleet.id);
        return true;
    }

    (void)finish_fleet_arrival(state, fleet, consumedFleets);
    return true;
}

Position projected_movement_endpoint(
    const GameState& state,
    const Fleet& fleet,
    Position destination,
    double turnFraction)
{
    const auto remaining = distance_between(fleet.position, destination);
    if (remaining <= 0.000001 || !fleet_warp_valid(state, fleet, fleet.warp)) return fleet.position;

    double distance = std::min(
        remaining,
        warp_distance(fleet.warp) * std::clamp(turnFraction, 0.0, 1.0));
    const auto fuelPerLy = fleet_fuel_change_for_distance(state, fleet, 1.0);
    if (fuelPerLy > 0.000001) distance = std::min(distance, fleet.fuel / fuelPerLy);
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

void continue_merged_fleet_route(
    GameState& state,
    Fleet& fleet,
    Position destination,
    double remainingTurnFraction,
    std::vector<FleetId>& consumedFleets)
{
    constexpr double epsilon = 0.000001;
    if (remainingTurnFraction <= epsilon) return;

    fleet.warp = std::min(fleet.warp, fleet_max_warp(state, fleet));
    const auto start = fleet.position;
    const auto routeDistance = distance_between(start, destination);
    if (routeDistance <= epsilon) {
        if (fleet.targetFleet == 0) {
            fleet.destination.reset();
            (void)finish_fleet_arrival(state, fleet, consumedFleets);
        }
        return;
    }

    const auto endpoint = projected_movement_endpoint(
        state, fleet, destination, remainingTurnFraction);
    const auto travelled = distance_between(start, endpoint);
    if (travelled <= epsilon) {
        const auto fuelPerLy = fleet_fuel_change_for_distance(state, fleet, 1.0);
        if (routeDistance > epsilon && fuelPerLy > epsilon && !fleet.fuelStalled) {
            fleet.fuelStalled = true;
            queue_fleet_movement_report(
                state, fleet, PlayerReportKind::FleetStalledForFuel, state.turn + 1);
        }
        return;
    }

    const auto fuelChange = fleet_fuel_change_for_distance(state, fleet, travelled);
    fleet.fuel = std::clamp(
        fleet.fuel - fuelChange,
        0.0,
        fleet_fuel_capacity(state, fleet));
    fleet.position = endpoint;
    fleet.fuelStalled = false;
    observe_fleet_sensor_sweep(state, fleet, start, endpoint, state.turn + 1);

    if (fleet.targetFleet == 0 && same_position(endpoint, destination)) {
        fleet.destination.reset();
        (void)finish_fleet_arrival(state, fleet, consumedFleets);
    }
}

void advance_fleets(GameState& state)
{
    constexpr double epsilon = 0.000001;
    std::vector<FleetId> consumedFleets;
    std::vector<FleetId> skipMovement;

    // A rendezvous already true at the planning boundary resolves before either
    // fleet starts another leg. This mirrors fixed-coordinate arrival behavior.
    std::vector<FleetId> fleetIds;
    fleetIds.reserve(state.fleets.size());
    for (const auto& fleet : state.fleets) fleetIds.push_back(fleet.id);
    std::sort(fleetIds.begin(), fleetIds.end());
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
            resolve_fleet_target_arrival(state, *fleet, consumedFleets);
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
    };
    std::vector<MotionPlan> plans;
    plans.reserve(state.fleets.size());
    for (const auto& fleet : state.fleets) {
        MotionPlan plan{fleet.id};
        if (!fleet_id_list_contains(skipMovement, fleet.id) && fleet.destination
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

    struct EncounterCandidate {
        FleetId pursuer{};
        FleetId target{};
        double timeFraction{};
        Position position;
        Position targetDestination;
        bool targetRouted{};
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
    std::sort(encounters.begin(), encounters.end(), [](const EncounterCandidate& left, const EncounterCandidate& right) {
        if (std::abs(left.timeFraction - right.timeFraction) > 0.000000001) {
            return left.timeFraction < right.timeFraction;
        }
        if (left.pursuer != right.pursuer) return left.pursuer < right.pursuer;
        return left.target < right.target;
    });

    // Resolve at most one encounter per fleet in a turn. Competing intercepts
    // are ordered by physical time, then stable FleetId, so replay results do
    // not depend on storage order.
    std::vector<FleetId> encounteredFleets;
    std::vector<EncounterCandidate> resolvedEncounters;
    for (const auto& encounter : encounters) {
        if (fleet_id_list_contains(encounteredFleets, encounter.pursuer)
            || fleet_id_list_contains(encounteredFleets, encounter.target)) {
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

    for (auto& fleet : state.fleets) {
        const auto plan = std::find_if(plans.begin(), plans.end(), [&](const MotionPlan& candidate) {
            return candidate.fleet == fleet.id;
        });
        if (plan == plans.end() || !plan->active) {
            fleet.fuelStalled = false;
            continue;
        }

        const Position start = fleet.position;
        const auto endpoint = plan->endpoint;
        if (plan->routed) fleet.destination = plan->destination;
        const auto remaining = distance_between(start, endpoint);
        if (remaining <= epsilon) {
            fleet.position = endpoint;
            const auto routeDistance = plan->routed
                ? distance_between(start, plan->destination)
                : 0.0;
            const auto fuelChangePerLy = fleet_fuel_change_for_distance(state, fleet, 1.0);
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
        const double fuelChangePerLy = fleet_fuel_change_for_distance(state, fleet, 1.0);
        fleet.fuelStalled = false;

        const double fuelChange = fuelChangePerLy * remaining;
        const auto capacity = fleet_fuel_capacity(state, fleet);
        fleet.fuel = std::clamp(fleet.fuel - fuelChange, 0.0, capacity);
        fleet.position = endpoint;

        observe_fleet_sensor_sweep(state, fleet, start, fleet.position, state.turn + 1);
        apply_fleet_radiation_attrition(state, fleet);

        if (plan->routed && !plan->intercepted && fleet.targetFleet == 0
            && same_position(endpoint, plan->destination)) {
            fixedArrivals.push_back(fleet.id);
        }
    }

    std::sort(fixedArrivals.begin(), fixedArrivals.end());

    for (const auto id : fixedArrivals) {
        auto* fleet = fleet_by_id(state, id);
        if (!fleet || fleet_id_list_contains(consumedFleets, id)) continue;
        fleet->destination.reset();
        (void)finish_fleet_arrival(state, *fleet, consumedFleets);
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
        const auto resolved = resolve_fleet_target_arrival(state, *fleet, consumedFleets);
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
            consumedFleets);
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
        planet.population += projected_population_growth(state, planet, state.turn);
    }
}

} // namespace

TurnResult TurnProcessor::process_with_events(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    GameState next = current;
    std::vector<GameEvent> events = deliver_due_survey_reports(next);
    auto deliveredReports = deliver_due_player_reports(next);
    events.insert(events.end(), deliveredReports.begin(), deliveredReports.end());

    // Persisted traffic may already be due at this planning boundary.
    deliver_due_fleet_telemetry(next);
    deliver_due_fleet_commands(next);

    generate_fleet_fuel(next);
    // Friendly orbital refueling services top up every docked fleet before
    // orders and movement are resolved. A basic dock without the module does
    // not refuel, leaving room for cheaper station hulls later.
    refuel_fleets_at_orbital_services(next);
    mine_colonies(next);

    for (const auto& submission : submitted_orders) {
        for (const auto& order : submission.orders) {
            std::visit(
                [&](const auto& concreteOrder) {
                    using T = std::decay_t<decltype(concreteOrder)>;

                    if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                        (void)submit_fleet_route_command(
                            next,
                            submission.player,
                            concreteOrder.fleet,
                            concreteOrder.destination,
                            concreteOrder.warp,
                            concreteOrder.arrivalAction,
                            concreteOrder.queuedWaypoints,
                            concreteOrder.repeatOrders,
                            concreteOrder.targetFleet);
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
                            if (!find_orbital_station_at_planet(next, planet->id) && !alreadyQueued) {
                                planet->productionQueue.push_back({
                                    ProductionKind::OrbitalStation, kOrbitalDockCost, 0});
                            }
                        } else if (concreteOrder.kind == ProductionKind::Research) {
                            return;
                        } else if (const auto* design = find_ship_design(next, kColonyShipDesignId);
                                   design && design->owner == submission.player) {
                            planet->productionQueue.push_back({ProductionKind::ColonyShip, ship_design_cost(*design), design->id});
                        }
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
                    } else if constexpr (std::is_same_v<T, CreateShipDesignOrder>) {
                        ShipDesign candidate{next.nextShipDesignId, submission.player, concreteOrder.name,
                            concreteOrder.hull, concreteOrder.components, concreteOrder.placements};
                        if (!ship_design_valid(candidate)
                            || !ship_design_available_to_player(next, submission.player, candidate)
                            || design_name_exists(next, submission.player, candidate.name)) return;
                        normalize_ship_design_placement(candidate);
                        next.shipDesigns.push_back(std::move(candidate));
                        ++next.nextShipDesignId;
                    } else if constexpr (std::is_same_v<T, QueueShipDesignOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        const auto* design = find_ship_design(next, concreteOrder.design);
                        if (planet == next.planets.end() || !design || design->owner != submission.player
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
                    } else if constexpr (std::is_same_v<T, SetFleetColonistsOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& candidate) {
                            return candidate.id == concreteOrder.fleet && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end() || fleet == next.fleets.end()) return;
                        if (!fleet_at_planet(next, *fleet, *planet)) return;

                        const auto requestedLoad = colonist_cargo_mass(concreteOrder.colonists)
                            + mineral_cargo_mass(fleet->minerals);
                        if (requestedLoad > fleet_cargo_capacity(next, *fleet) + 0.000001) return;

                        if (concreteOrder.colonists > fleet->colonists) {
                            const auto load = concreteOrder.colonists - fleet->colonists;
                            if (planet->population <= load) return;
                            planet->population -= load;
                        } else {
                            planet->population += fleet->colonists - concreteOrder.colonists;
                        }
                        fleet->colonists = concreteOrder.colonists;
                    } else if constexpr (std::is_same_v<T, SetFleetMineralCargoOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony
                                && (candidate.owner == submission.player || candidate.owner == 0);
                        });
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& candidate) {
                            return candidate.id == concreteOrder.fleet && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end() || fleet == next.fleets.end()) return;
                        if (!fleet_at_planet(next, *fleet, *planet) || !valid_mineral_cargo(concreteOrder.minerals)) return;

                        const auto requestedLoad = colonist_cargo_mass(fleet->colonists)
                            + mineral_cargo_mass(concreteOrder.minerals);
                        if (requestedLoad > fleet_cargo_capacity(next, *fleet) + 0.000001) return;
                        if (!minerals_available(planet->minerals, fleet->minerals, concreteOrder.minerals)) return;

                        transfer_minerals(planet->minerals, fleet->minerals, concreteOrder.minerals);
                    } else if constexpr (std::is_same_v<T, TransferCargoOrder>) {
                        (void)transfer_cargo(next, submission.player, concreteOrder);
                    } else if constexpr (std::is_same_v<T, RefuelFleetOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& candidate) {
                            return candidate.id == concreteOrder.fleet && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end() || fleet == next.fleets.end()) return;
                        if (!fleet_at_planet(next, *fleet, *planet)) return;
                        if (!colony_has_orbital_service(
                                next, planet->id, submission.player,
                                OrbitalStationModule::RefuelingDepot)) {
                            return;
                        }
                        fleet->fuel = fleet_fuel_capacity(next, *fleet);
                    } else if constexpr (std::is_same_v<T, ColonizePlanetOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.planet;
                        });
                        const auto fleet = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& candidate) {
                            return candidate.id == concreteOrder.fleet && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end() || fleet == next.fleets.end()) return;
                        if (establish_colony(next, *fleet, *planet)) next.fleets.erase(fleet);
                    } else if constexpr (std::is_same_v<T, SetRemoteMiningOrder>) {
                        (void)submit_fleet_task_command(
                            next,
                            submission.player,
                            concreteOrder.fleet,
                            concreteOrder.enabled ? FleetTask::RemoteMining : FleetTask::None);
                    } else if constexpr (std::is_same_v<T, MergeFleetsOrder>) {
                        (void)merge_fleets(next, submission.player, concreteOrder);
                    } else if constexpr (std::is_same_v<T, SplitFleetOrder>) {
                        (void)split_fleet(next, submission.player, concreteOrder);
                    }
                },
                order);
        }
    }

    mine_uncolonized_planets(next);
    advance_fleets(next);
    observe_current_sensor_coverage(next, next.turn + 1);
    std::vector<std::pair<PlayerId, std::uint32_t>> researchByPlayer;
    for (auto& planet : next.planets) {
        const auto contribution = run_colony_production(next, planet);
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
    grow_colonies(next);

    // The returned state is the next planning boundary. Commands arriving
    // during this elapsed year become active now but never rewrite movement
    // that has already happened. Telemetry is then emitted from truth.
    ++next.turn;
    deliver_due_fleet_commands(next);
    publish_fleet_telemetry(next, next.turn);
    deliver_due_fleet_telemetry(next);
    auto deliveredIntel = deliver_due_survey_reports(next);
    events.insert(events.end(), deliveredIntel.begin(), deliveredIntel.end());
    deliveredReports = deliver_due_player_reports(next);
    events.insert(events.end(), deliveredReports.begin(), deliveredReports.end());
    record_empire_turn_statistics(next);
    return {std::move(next), std::move(events)};
}

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    return process_with_events(current, submitted_orders).state;
}

} // namespace suns
