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

    // Fleet orders still preserve the immutable previous-state contract.
    const auto initial = suns::make_demo_game();
    suns::PlayerOrders moveOrders{1, {}};
    moveOrders.orders.emplace_back(suns::MoveFleetOrder{1, {12.5, -4.0}});
    const auto moved = processor.process(initial, {moveOrders});
    assert(initial.turn == 1);
    assert(moved.turn == 2);
    assert(moved.fleets.front().position.x == 12.5);
    assert(moved.fleets.front().position.y == -4.0);

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

    // Factories compete for the same local production, then increase future output.
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

    // Complete the first expansion cycle with the produced colony ship.
    const auto* ship = colony_ship(shipTurn3);
    assert(ship != nullptr);
    const auto* destination = suns::find_star(shipTurn3, 2);
    assert(destination != nullptr);

    suns::PlayerOrders moveColony{1, {}};
    moveColony.orders.emplace_back(
        suns::MoveFleetOrder{ship->id, destination->position});
    const auto arrived = processor.process(shipTurn3, {moveColony});

    const auto* arrivedShip = colony_ship(arrived);
    assert(arrivedShip != nullptr);
    assert(suns::same_position(arrivedShip->position, destination->position));

    suns::PlayerOrders colonize{1, {}};
    colonize.orders.emplace_back(suns::ColonizePlanetOrder{arrivedShip->id, 2});
    const auto expanded = processor.process(arrived, {colonize});

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
