#include "suns/turn_processor.hpp"

#include <algorithm>
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

    std::uint32_t available = planet.stockpile + planet.industry;

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

} // namespace

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    GameState next = current;

    // Orders change intentions/state first. Production then runs for every
    // colony from the resulting state, keeping the turn pipeline explicit.
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
                            fleet->position = concreteOrder.destination;
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

    for (auto& planet : next.planets) {
        run_colony_production(next, planet);
    }

    ++next.turn;
    return next;
}

} // namespace suns
