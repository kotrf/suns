#include "suns/turn_processor.hpp"

#include <algorithm>
#include <string>
#include <type_traits>

namespace suns {

GameState TurnProcessor::process(
    const GameState& current,
    const std::vector<PlayerOrders>& submitted_orders) const
{
    GameState next = current;

    // First vertical-slice economy: every owned colony adds its industry to a
    // local stockpile once per turn. Production is deliberately local so that
    // geography can matter when the economy becomes richer later.
    for (auto& planet : next.planets) {
        if (planet.owner != 0) {
            planet.stockpile += planet.industry;
        }
    }

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
                    } else if constexpr (std::is_same_v<T, BuildColonyShipOrder>) {
                        const auto planet = std::find_if(
                            next.planets.begin(), next.planets.end(),
                            [&](const Planet& candidate) {
                                return candidate.id == concreteOrder.colony
                                    && candidate.owner == submission.player;
                            });
                        if (planet == next.planets.end() || planet->stockpile < kColonyShipCost) {
                            return;
                        }

                        const auto* star = find_star(next, planet->star);
                        if (!star) {
                            return;
                        }

                        planet->stockpile -= kColonyShipCost;
                        const auto id = next.nextFleetId++;
                        next.fleets.push_back({
                            id,
                            submission.player,
                            "Colony Ship " + std::to_string(id),
                            FleetRole::ColonyShip,
                            star->position,
                        });
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
                        next.fleets.erase(fleet);
                    }
                },
                order);
        }
    }

    ++next.turn;
    return next;
}

} // namespace suns
