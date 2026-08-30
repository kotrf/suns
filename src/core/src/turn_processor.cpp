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
    if (!player) return;

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

        if (player->technology.nextFocus) {
            player->technology.focus = *player->technology.nextFocus;
            player->technology.nextFocus.reset();
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

std::optional<FleetId> complete_production(GameState& state, Planet& planet, const ProductionItem& item)
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
    if (planet.productionQueue.empty()) planet.productionWaitingForMinerals = false;

    std::uint32_t researchProduced = 0;
    std::uint32_t available = planet.stockpile + colony_output(planet);
    while (!planet.productionQueue.empty()) {
        auto& item = planet.productionQueue.front();
        if (item.kind == ProductionKind::Research) {
            planet.productionWaitingForMinerals = false;
            researchProduced += available;
            available = 0;
            break;
        }
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
        const auto completedFleet = complete_production(state, planet, completed);
        if (!completedFleet) continue;

        const auto* star = find_star(state, planet.star);
        if (!star) continue;
        std::uint32_t quantity = 0;
        if (completed.kind == ProductionKind::Factory) quantity = planet.industry;
        else if (completed.kind == ProductionKind::Mine) quantity = planet.mines;
        else quantity = *completedFleet;
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
            *completedFleet,
            completedDesign,
            completed.kind,
            quantity);
    }
    if (planet.productionQueue.empty()) planet.productionWaitingForMinerals = false;
    planet.stockpile = available;
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
    planet.stockpile = 0;
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
        if (!colony) return false;
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
            return;
        }

        fleet.warp = waypoint.warp;
        fleet.arrivalAction = active_arrival_action(waypoint.arrivalAction);

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

bool fleet_ready_for_reorganization(const Fleet& fleet)
{
    return !fleet.destination
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

void advance_fleets(GameState& state)
{
    constexpr double epsilon = 0.000001;
    std::vector<FleetId> consumedFleets;

    for (auto& fleet : state.fleets) {
        if (!fleet.destination || !fleet_warp_valid(state, fleet, fleet.warp)) {
            fleet.fuelStalled = false;
            continue;
        }

        const Position start = fleet.position;
        const auto destination = *fleet.destination;
        const auto remaining = distance_between(start, destination);
        if (remaining <= epsilon) {
            fleet.position = destination;
            fleet.destination.reset();
            if (finish_fleet_arrival(state, fleet, consumedFleets)) continue;
            continue;
        }

        const auto maximumDistance = warp_distance(fleet.warp);
        double travelDistance = std::min(remaining, maximumDistance);
        const double fuelChangePerLy = fleet_fuel_change_for_distance(state, fleet, 1.0);
        if (fuelChangePerLy > epsilon) travelDistance = std::min(travelDistance, fleet.fuel / fuelChangePerLy);
        if (travelDistance <= epsilon) {
            if (!fleet.fuelStalled && fuelChangePerLy > epsilon) {
                fleet.fuelStalled = true;
                queue_fleet_movement_report(
                    state, fleet, PlayerReportKind::FleetStalledForFuel, state.turn + 1);
            }
            continue;
        }
        fleet.fuelStalled = false;

        const double fuelChange = fuelChangePerLy * travelDistance;
        const auto capacity = fleet_fuel_capacity(state, fleet);
        fleet.fuel = std::clamp(fleet.fuel - fuelChange, 0.0, capacity);

        bool arrived = false;
        if (travelDistance >= remaining - epsilon) {
            fleet.position = destination;
            fleet.destination.reset();
            arrived = true;
        } else {
            const auto fraction = travelDistance / remaining;
            fleet.position.x += (destination.x - start.x) * fraction;
            fleet.position.y += (destination.y - start.y) * fraction;
        }

        observe_fleet_sensor_sweep(state, fleet, start, fleet.position, state.turn + 1);
        apply_fleet_radiation_attrition(state, fleet);

        if (arrived) {
            if (finish_fleet_arrival(state, fleet, consumedFleets)) continue;
        }
    }

    if (!consumedFleets.empty()) {
        std::erase_if(state.fleets, [&](const Fleet& fleet) {
            return std::find(consumedFleets.begin(), consumedFleets.end(), fleet.id) != consumedFleets.end();
        });
    }
}

void grow_colonies(GameState& state)
{
    for (auto& planet : state.planets) planet.population += projected_population_growth(planet);
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
                            concreteOrder.repeatOrders);
                    } else if constexpr (std::is_same_v<T, QueueProductionOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end()) return;

                        if (concreteOrder.kind == ProductionKind::Factory) {
                            planet->productionQueue.push_back({ProductionKind::Factory, kFactoryCost, 0});
                        } else if (concreteOrder.kind == ProductionKind::Mine) {
                            planet->productionQueue.push_back({ProductionKind::Mine, kMineCost, 0});
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
                        const auto isResearch = [](const ProductionItem& item) {
                            return item.kind == ProductionKind::Research;
                        };
                        if (concreteOrder.enabled) {
                            if (std::none_of(planet->productionQueue.begin(), planet->productionQueue.end(), isResearch)) {
                                planet->productionQueue.push_back({ProductionKind::Research, 0, 0});
                            }
                        } else {
                            std::erase_if(planet->productionQueue, isResearch);
                        }
                    } else if constexpr (std::is_same_v<T, SetResearchPlanOrder>) {
                        auto* player = mutable_player(next, submission.player);
                        if (!player || !valid_research_field(concreteOrder.focus)
                            || (concreteOrder.nextFocus && !valid_research_field(*concreteOrder.nextFocus))) {
                            return;
                        }
                        player->technology.focus = concreteOrder.focus;
                        player->technology.nextFocus = concreteOrder.nextFocus;
                    } else if constexpr (std::is_same_v<T, CreateShipDesignOrder>) {
                        ShipDesign candidate{next.nextShipDesignId, submission.player, concreteOrder.name,
                            concreteOrder.hull, concreteOrder.components};
                        if (!ship_design_valid(candidate)
                            || !ship_design_available_to_player(next, submission.player, candidate)
                            || design_name_exists(next, submission.player, candidate.name)) return;
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
    return {std::move(next), std::move(events)};
}

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    return process_with_events(current, submitted_orders).state;
}

} // namespace suns
