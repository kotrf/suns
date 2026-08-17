#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>

namespace suns {

namespace {

void complete_production(GameState& state, Planet& planet, ProductionKind kind)
{
    if (kind == ProductionKind::Factory) {
        ++planet.industry;
        return;
    }

    const auto* star = find_star(state, planet.star);
    const auto* design = find_ship_design(state, kColonyShipDesignId);
    if (!star || !design) return;

    const auto id = state.nextFleetId++;
    state.fleets.push_back({
        id,
        planet.owner,
        "Colony Ship " + std::to_string(id),
        FleetRole::ColonyShip,
        design->id,
        star->position,
        std::nullopt,
        kColonyShipCruiseWarp,
        ship_design_fuel_capacity(*design),
        250,
    });
}

void run_colony_production(GameState& state, Planet& planet)
{
    if (planet.owner == 0) return;

    std::uint32_t available = planet.stockpile + colony_output(planet);
    while (!planet.productionQueue.empty() && available > 0) {
        auto& item = planet.productionQueue.front();
        const auto spent = std::min(available, item.remainingCost);
        available -= spent;
        item.remainingCost -= spent;
        if (item.remainingCost != 0) break;

        const auto kind = item.kind;
        planet.productionQueue.erase(planet.productionQueue.begin());
        complete_production(state, planet, kind);
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

bool fleet_at_friendly_colony(const GameState& state, const Fleet& fleet)
{
    for (const auto& planet : state.planets) {
        if (planet.owner != fleet.owner) continue;
        const auto* star = find_star(state, planet.star);
        if (star && same_position(star->position, fleet.position)) return true;
    }
    return false;
}

void service_fleet_fuel(GameState& state)
{
    for (auto& fleet : state.fleets) {
        const auto capacity = fleet_fuel_capacity(state, fleet);
        if (capacity <= 0.0) {
            fleet.fuel = 0.0;
            continue;
        }

        if (fleet_at_friendly_colony(state, fleet)) {
            fleet.fuel = capacity;
        }

        if (const auto* design = fleet_design(state, fleet)) {
            fleet.fuel = std::min(capacity, fleet.fuel + ship_design_fuel_generation(*design));
        }
    }
}

void advance_fleets(GameState& state)
{
    constexpr double epsilon = 0.000001;

    for (auto& fleet : state.fleets) {
        if (!fleet.destination || !fleet_warp_valid(state, fleet, fleet.warp)) continue;

        const Position start = fleet.position;
        const auto destination = *fleet.destination;
        const auto remaining = distance_between(start, destination);
        if (remaining <= epsilon) {
            fleet.position = destination;
            fleet.destination.reset();
            continue;
        }

        const auto maximumDistance = warp_distance(fleet.warp);
        double travelDistance = std::min(remaining, maximumDistance);
        const double fuelChangePerLy = fleet_fuel_change_for_distance(state, fleet, 1.0);

        if (fuelChangePerLy > epsilon) {
            travelDistance = std::min(travelDistance, fleet.fuel / fuelChangePerLy);
        }

        if (travelDistance <= epsilon) continue;

        const double fuelChange = fuelChangePerLy * travelDistance;
        const auto capacity = fleet_fuel_capacity(state, fleet);
        fleet.fuel = std::clamp(fleet.fuel - fuelChange, 0.0, capacity);

        if (travelDistance >= remaining - epsilon) {
            fleet.position = destination;
            fleet.destination.reset();
        } else {
            const auto fraction = travelDistance / remaining;
            fleet.position.x += (destination.x - start.x) * fraction;
            fleet.position.y += (destination.y - start.y) * fraction;
        }

        survey_fleet_sweep(state, fleet, start, fleet.position);
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

    // Fleets sitting at a friendly colony begin the planning turn refuelled.
    // Onboard generators then add their per-turn production everywhere else.
    service_fleet_fuel(next);

    for (const auto& submission : submitted_orders) {
        for (const auto& order : submission.orders) {
            std::visit(
                [&](const auto& concreteOrder) {
                    using T = std::decay_t<decltype(concreteOrder)>;

                    if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                        const auto fleet = std::find_if(
                            next.fleets.begin(), next.fleets.end(),
                            [&](const Fleet& candidate) {
                                return candidate.id == concreteOrder.fleet
                                    && candidate.owner == submission.player;
                            });
                        if (fleet == next.fleets.end()) return;

                        const auto requestedWarp = concreteOrder.warp == 0 ? fleet->warp : concreteOrder.warp;
                        if (!fleet_warp_valid(next, *fleet, requestedWarp)) return;
                        fleet->warp = requestedWarp;

                        if (same_position(fleet->position, concreteOrder.destination)) {
                            fleet->destination.reset();
                        } else {
                            fleet->destination = concreteOrder.destination;
                        }
                    } else if constexpr (std::is_same_v<T, QueueProductionOrder>) {
                        const auto planet = std::find_if(
                            next.planets.begin(), next.planets.end(),
                            [&](const Planet& candidate) {
                                return candidate.id == concreteOrder.colony
                                    && candidate.owner == submission.player;
                            });
                        if (planet != next.planets.end()) {
                            planet->productionQueue.push_back({
                                concreteOrder.kind,
                                production_cost(concreteOrder.kind),
                            });
                        }
                    } else if constexpr (std::is_same_v<T, ColonizePlanetOrder>) {
                        const auto planet = std::find_if(
                            next.planets.begin(), next.planets.end(),
                            [&](const Planet& candidate) {
                                return candidate.id == concreteOrder.planet;
                            });
                        if (planet == next.planets.end() || planet->owner != 0) return;
                        if (!is_surveyed(next, submission.player, planet->star)) return;

                        const auto fleet = std::find_if(
                            next.fleets.begin(), next.fleets.end(),
                            [&](const Fleet& candidate) {
                                return candidate.id == concreteOrder.fleet
                                    && candidate.owner == submission.player
                                    && fleet_can_colonize(next, candidate);
                            });
                        if (fleet == next.fleets.end() || fleet->colonists == 0) return;
                        if (colonist_cargo_mass(fleet->colonists) > fleet_cargo_capacity(next, *fleet) + 0.000001) {
                            return;
                        }

                        const auto* star = find_star(next, planet->star);
                        if (!star || !same_position(fleet->position, star->position)) return;

                        planet->owner = submission.player;
                        planet->population = fleet->colonists;
                        planet->industry = 1;
                        planet->stockpile = 0;
                        planet->productionQueue.clear();
                        next.fleets.erase(fleet);
                    }
                },
                order);
        }
    }

    advance_fleets(next);
    refresh_sensor_intel(next);

    for (auto& planet : next.planets) run_colony_production(next, planet);
    grow_colonies(next);

    ++next.turn;
    return next;
}

} // namespace suns
