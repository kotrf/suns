#include "suns/turn_processor.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>

namespace suns {

std::uint32_t production_item_cost(const GameState& state, const ProductionItem& item)
{
    if (item.kind == ProductionKind::Factory) return kFactoryCost;
    if (item.kind == ProductionKind::Mine) return kMineCost;
    if (item.shipDesign != 0) {
        if (const auto* design = find_ship_design(state, item.shipDesign)) return ship_design_cost(*design);
    }
    return kColonyShipCost;
}

namespace {

FleetRole presentation_role_for_design(const ShipDesign& design)
{
    return ship_design_can_colonize(design) ? FleetRole::ColonyShip : FleetRole::Scout;
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

void complete_production(GameState& state, Planet& planet, const ProductionItem& item)
{
    if (item.kind == ProductionKind::Factory) {
        ++planet.industry;
        return;
    }
    if (item.kind == ProductionKind::Mine) {
        ++planet.mines;
        return;
    }

    const auto designId = item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId;
    const auto* star = find_star(state, planet.star);
    const auto* design = find_ship_design(state, designId);
    if (!star || !design || design->owner != planet.owner || !ship_design_valid(*design)) return;

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
}

void run_colony_production(GameState& state, Planet& planet)
{
    if (planet.owner == 0) return;

    std::uint32_t available = planet.stockpile + colony_output(planet);
    while (!planet.productionQueue.empty()) {
        auto& item = planet.productionQueue.front();
        if (item.remainingCost > 0) {
            if (available == 0) break;
            const auto spent = std::min(available, item.remainingCost);
            available -= spent;
            item.remainingCost -= spent;
            if (item.remainingCost != 0) break;
        }

        const auto requiredMinerals = production_item_mineral_cost(state, item);
        if (!mineral_cargo_sufficient(planet.minerals, requiredMinerals)) break;
        subtract_minerals(planet.minerals, requiredMinerals);

        const auto completed = item;
        planet.productionQueue.erase(planet.productionQueue.begin());
        complete_production(state, planet, completed);
    }
    planet.stockpile = available;
}

double distance_to_segment(Position point, Position start, Position end)
{
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.000000000001) return distance_between(point, start);

    const double projection = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared;
    const double t = std::clamp(projection, 0.0, 1.0);
    const Position closest{start.x + t * dx, start.y + t * dy};
    return distance_between(point, closest);
}

void survey_fleet_sweep(GameState& state, const Fleet& fleet, Position start, Position end)
{
    const auto range = fleet_sensor_range(state, fleet);
    if (range <= 0.0) return;

    for (const auto& star : state.stars) {
        if (distance_to_segment(star.position, start, end) <= range + 0.000001) {
            mark_surveyed(state, fleet.owner, star.id);
        }
    }
}

bool fleet_at_planet(const GameState& state, const Fleet& fleet, const Planet& planet)
{
    const auto* star = find_star(state, planet.star);
    return star && same_position(star->position, fleet.position);
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
    if (!is_surveyed(state, fleet.owner, planet.star)) return false;
    if (fleet_cargo_used(state, fleet) > fleet_cargo_capacity(state, fleet) + 0.000001) return false;
    if (!fleet_at_planet(state, fleet, planet)) return false;

    planet.owner = fleet.owner;
    planet.population = fleet.colonists;
    planet.minerals.ironium += fleet.minerals.ironium;
    planet.minerals.boranium += fleet.minerals.boranium;
    planet.minerals.germanium += fleet.minerals.germanium;
    planet.industry = 1;
    planet.stockpile = 0;
    planet.productionQueue.clear();
    planet.mines = 0;
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
    case FleetArrivalActionKind::LoadColonistsToCapacity: {
        auto* colony = friendly_colony_at_fleet(state, fleet);
        if (!colony) return false;

        const auto capacity = fleet_cargo_capacity(state, fleet);
        const auto mineralLoad = mineral_cargo_mass(fleet.minerals);
        const auto freeForColonists = std::max(0.0, capacity - mineralLoad);
        const auto capacityColonists = static_cast<std::uint64_t>(
            std::floor(freeForColonists * kColonistsPerCargoUnit + 0.000001));
        if (fleet.colonists >= capacityColonists) return false;

        const auto reserve = std::max<std::uint64_t>(1, action.reservePopulation);
        if (colony->population <= reserve) return false;

        const auto available = colony->population - reserve;
        const auto freeSpace = capacityColonists - fleet.colonists;
        const auto load = std::min(available, freeSpace);
        colony->population -= load;
        fleet.colonists += load;
        return false;
    }
    case FleetArrivalActionKind::UnloadAllColonists: {
        auto* colony = friendly_colony_at_fleet(state, fleet);
        if (!colony) return false;
        colony->population += fleet.colonists;
        fleet.colonists = 0;
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
        if (const auto* design = fleet_design(state, fleet)) {
            fleet.fuel = std::min(capacity, fleet.fuel + ship_design_fuel_generation(*design));
        }
    }
}

void advance_fleets(GameState& state)
{
    constexpr double epsilon = 0.000001;
    std::vector<FleetId> consumedFleets;

    for (auto& fleet : state.fleets) {
        if (!fleet.destination || !fleet_warp_valid(state, fleet, fleet.warp)) continue;

        const Position start = fleet.position;
        const auto destination = *fleet.destination;
        const auto remaining = distance_between(start, destination);
        if (remaining <= epsilon) {
            fleet.position = destination;
            fleet.destination.reset();
            if (execute_arrival_action(state, fleet)) {
                consumedFleets.push_back(fleet.id);
                continue;
            }
            activate_next_waypoint(state, fleet);
            continue;
        }

        const auto maximumDistance = warp_distance(fleet.warp);
        double travelDistance = std::min(remaining, maximumDistance);
        const double fuelChangePerLy = fleet_fuel_change_for_distance(state, fleet, 1.0);
        if (fuelChangePerLy > epsilon) travelDistance = std::min(travelDistance, fleet.fuel / fuelChangePerLy);
        if (travelDistance <= epsilon) continue;

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

        survey_fleet_sweep(state, fleet, start, fleet.position);
        apply_fleet_radiation_attrition(state, fleet);

        if (arrived) {
            if (execute_arrival_action(state, fleet)) {
                consumedFleets.push_back(fleet.id);
                continue;
            }
            activate_next_waypoint(state, fleet);
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

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    GameState next = current;

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
                            concreteOrder.queuedWaypoints);
                    } else if constexpr (std::is_same_v<T, QueueProductionOrder>) {
                        const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
                        });
                        if (planet == next.planets.end()) return;

                        if (concreteOrder.kind == ProductionKind::Factory) {
                            planet->productionQueue.push_back({ProductionKind::Factory, kFactoryCost, 0});
                        } else if (concreteOrder.kind == ProductionKind::Mine) {
                            planet->productionQueue.push_back({ProductionKind::Mine, kMineCost, 0});
                        } else if (const auto* design = find_ship_design(next, kColonyShipDesignId);
                                   design && design->owner == submission.player) {
                            planet->productionQueue.push_back({ProductionKind::ColonyShip, ship_design_cost(*design), design->id});
                        }
                    } else if constexpr (std::is_same_v<T, CreateShipDesignOrder>) {
                        ShipDesign candidate{next.nextShipDesignId, submission.player, concreteOrder.name,
                            concreteOrder.hull, concreteOrder.components};
                        if (!ship_design_valid(candidate) || design_name_exists(next, submission.player, candidate.name)) return;
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
                            return candidate.id == concreteOrder.colony && candidate.owner == submission.player;
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
                    }
                },
                order);
        }
    }

    advance_fleets(next);
    refresh_sensor_intel(next);
    for (auto& planet : next.planets) run_colony_production(next, planet);
    grow_colonies(next);

    // The returned state is the next planning boundary. Commands arriving
    // during this elapsed year become active now but never rewrite movement
    // that has already happened. Telemetry is then emitted from truth.
    ++next.turn;
    deliver_due_fleet_commands(next);
    publish_fleet_telemetry(next, next.turn);
    deliver_due_fleet_telemetry(next);
    return next;
}

} // namespace suns
