#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>

namespace {

const suns::Planet& planet(const suns::GameState& state, suns::PlanetId id)
{
    for (const auto& candidate : state.planets) {
        if (candidate.id == id) {
            return candidate;
        }
    }
    assert(false);
    return state.planets.front();
}

const suns::Fleet* colony_ship(const suns::GameState& state)
{
    for (const auto& fleet : state.fleets) {
        if (fleet.owner == 1 && fleet.role == suns::FleetRole::ColonyShip) {
            return &fleet;
        }
    }
    return nullptr;
}

} // namespace

int main()
{
    const suns::TurnProcessor processor;
    const auto initial = suns::make_demo_game();

    // Home-system intel is known; other planetary data starts hidden.
    assert(suns::is_surveyed(initial, 1, 1));
    assert(!suns::is_surveyed(initial, 1, 2));

    // Moving a scout onto a star reveals that system after turn resolution.
    const auto* alpha = suns::find_star(initial, 2);
    assert(alpha != nullptr);
    suns::PlayerOrders scoutOrders{1, {}};
    scoutOrders.orders.emplace_back(suns::MoveFleetOrder{1, alpha->position});
    const auto surveyedAlpha = processor.process(initial, {scoutOrders});
    assert(initial.turn == 1);
    assert(!suns::is_surveyed(initial, 1, 2));
    assert(suns::is_surveyed(surveyedAlpha, 1, 2));
    assert(suns::same_position(surveyedAlpha.fleets.front().position, alpha->position));

    // A colony ship is a persistent production project, not an instant purchase.
    suns::PlayerOrders queueShip{1, {}};
    queueShip.orders.emplace_back(
        suns::QueueProductionOrder{1, suns::ProductionKind::ColonyShip});

    const auto shipTurn1 = processor.process(initial, {queueShip});
    assert(planet(shipTurn1, 1).productionQueue.size() == 1);
    assert(planet(shipTurn1, 1).productionQueue.front().remainingCost == 8);
    assert(colony_ship(shipTurn1) == nullptr);

    const auto shipTurn2 = processor.process(shipTurn1, {});
    assert(planet(shipTurn2, 1).productionQueue.front().remainingCost == 4);
    assert(colony_ship(shipTurn2) == nullptr);

    const auto shipTurn3 = processor.process(shipTurn2, {});
    assert(planet(shipTurn3, 1).productionQueue.empty());
    assert(colony_ship(shipTurn3) != nullptr);

    // Factories compete for local production, then increase future output.
    suns::PlayerOrders queueFactory{1, {}};
    queueFactory.orders.emplace_back(
        suns::QueueProductionOrder{1, suns::ProductionKind::Factory});

    const auto factoryTurn1 = processor.process(initial, {queueFactory});
    assert(planet(factoryTurn1, 1).industry == 4);
    assert(planet(factoryTurn1, 1).productionQueue.front().remainingCost == 2);

    const auto factoryTurn2 = processor.process(factoryTurn1, {});
    assert(planet(factoryTurn2, 1).productionQueue.empty());
    assert(planet(factoryTurn2, 1).industry == 5);
    assert(planet(factoryTurn2, 1).stockpile == 2);

    // A colony ship may travel to an unknown system, but cannot colonize it
    // until the player has survey intel from a scout.
    const auto* ship = colony_ship(shipTurn3);
    assert(ship != nullptr);

    suns::PlayerOrders moveColony{1, {}};
    moveColony.orders.emplace_back(suns::MoveFleetOrder{ship->id, alpha->position});
    const auto arrivedUnknown = processor.process(shipTurn3, {moveColony});
    assert(!suns::is_surveyed(arrivedUnknown, 1, 2));

    const auto* arrivedShip = colony_ship(arrivedUnknown);
    assert(arrivedShip != nullptr);
    suns::PlayerOrders prematureColonize{1, {}};
    prematureColonize.orders.emplace_back(
        suns::ColonizePlanetOrder{arrivedShip->id, 2});
    const auto rejected = processor.process(arrivedUnknown, {prematureColonize});
    assert(planet(rejected, 2).owner == 0);
    assert(colony_ship(rejected) != nullptr);

    suns::PlayerOrders surveyDestination{1, {}};
    surveyDestination.orders.emplace_back(suns::MoveFleetOrder{1, alpha->position});
    const auto destinationSurveyed = processor.process(rejected, {surveyDestination});
    assert(suns::is_surveyed(destinationSurveyed, 1, 2));

    const auto* readyShip = colony_ship(destinationSurveyed);
    assert(readyShip != nullptr);
    suns::PlayerOrders colonize{1, {}};
    colonize.orders.emplace_back(suns::ColonizePlanetOrder{readyShip->id, 2});
    const auto expanded = processor.process(destinationSurveyed, {colonize});

    assert(planet(expanded, 2).owner == 1);
    assert(planet(expanded, 2).population == 250);
    assert(planet(expanded, 2).industry == 1);
    assert(planet(expanded, 2).stockpile == 1);
    assert(colony_ship(expanded) == nullptr);

    // Invalid ownership remains guarded.
    suns::PlayerOrders invalid{2, {}};
    invalid.orders.emplace_back(
        suns::QueueProductionOrder{1, suns::ProductionKind::Factory});
    const auto guarded = processor.process(initial, {invalid});
    assert(planet(guarded, 1).productionQueue.empty());

    return 0;
}
