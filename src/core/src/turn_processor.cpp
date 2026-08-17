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
    if (!star) {
        return;
    }

    const auto id = state.nextFleetId++;
    state.fleets.push_back({
        id,
        planet.owner,
        "Colony Ship " + std::to_string(id),
        FleetRole::ColonyShip,
        star->position,
    });
}

void run_colony_production(GameState& state, Planet& planet)
{
    if (planet.owner == 0) {
        return;
    }

    std::uint32_t available = planet.stockpile + colony_output(planet);

    while (!planet.productionQueue.empty() && available > 0) {
        auto& item = planet.productionQueue.front();
        const auto spent = std::min(available, item.remainingCost);
        available -= spent;
        item.remainingCost -= spent;

        if (item.remainingCost != 0) {
            break;
        }

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
    if (lengthSquared <= 0.000000000001) {
        return distance_between(point, start);
    }

    const double projection = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared;
    const double t = std::clamp(projection, 0.0, 1.0);
    const Position closest{start.x + t * dx, start.y + t * dy};
    return distance_between(point, closest);
}

void survey_scout_sweep(GameState& state, PlayerId owner, Position start, Position end, double range)
{
    if (range <= 0.0) {
        return;
    }

    for (const auto& star : state.stars) {
        if (distance_to_segment(star.position, start, end) <= range + 0.000001) {
            mark_surveyed(state, owner, star.id);
        }
    }
}

void advance_fleets(GameState& state)
{
    for (auto& fleet : state.fleets) {
        if (!fleet.destination) {
            continue;
        }

        const Position start = fleet.position;
        const auto destination = *fleet.destination;
        const auto remaining = distance_between(fleet.position, destination);
        const auto speed = fleet_speed(fleet.role);

        if (remaining <= speed || remaining <= 0.000001) {
            fleet.position = destination;
            fleet.destination.reset();
        } else {
            const auto fraction = speed / remaining;
            fleet.position.x += (destination.x - fleet.position.x) * fraction;
            fleet.position.y += (destination.y - fleet.position.y) * fraction;
        }

        survey_scout_sweep(
            state,
            fleet.owner,
            start,
            fleet.position,
            fleet_sensor_range(fleet.role));
    }
}

void grow_colonies(GameState& state)
{
    for (auto& planet : state.planets) {
        planet.population += projected_population_growth(planet);
    }
}

} // namespace

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    GameState next = current;

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
                        if (fleet != next.fleets.end()) {
                            if (same_position(fleet->position, concreteOrder.destination)) {
                                fleet->destination.reset();
                            } else {
                                fleet->destination = concreteOrder.destination;
                            }
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
                        if (planet == next.planets.end() || planet->owner != 0) {
                            return;
                        }

                        if (!is_surveyed(next, submission.player, planet->star)) {
                            return;
                        }

                        const auto fleet = std::find_if(
                            next.fleets.begin(), next.fleets.end(),
                            [&](const Fleet& candidate) {
                                return candidate.id == concreteOrder.fleet
                                    && candidate.owner == submission.player
                                    && candidate.role == FleetRole::ColonyShip;
                            });
                        if (fleet == next.fleets.end()) {
                            return;
                        }

                        const auto* star = find_star(next, planet->star);
                        if (!star || !same_position(fleet->position, star->position)) {
                            return;
                        }

                        planet->owner = submission.player;
                        planet->population = 250;
                        planet->industry = 1;
                        planet->stockpile = 0;
                        planet->productionQueue.clear();
                        next.fleets.erase(fleet);
                    }
                },
                order);
        }
    }

    // Scanner coverage moves with ships. The swept path is sampled continuously
    // over each turn of travel, then stationary fleet/colony coverage is applied
    // at the resulting positions. Survey knowledge is permanent once acquired.
    advance_fleets(next);
    refresh_sensor_intel(next);

    for (auto& planet : next.planets) {
        run_colony_production(next, planet);
    }
    grow_colonies(next);

    ++next.turn;
    return next;
}

} // namespace suns
