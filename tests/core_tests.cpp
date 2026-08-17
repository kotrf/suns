#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

int main()
{
    const suns::TurnProcessor processor;
    const auto initial = suns::make_demo_game();

    assert(initial.turn == 1);
    assert(initial.planets.size() == initial.stars.size());
    assert(initial.fleets.size() == 1);

    const auto* earth = suns::find_planet_at_star(initial, 1);
    assert(earth != nullptr);
    assert(earth->owner == 1);
    assert(earth->stockpile == suns::kColonyShipCost);

    // The previous state remains immutable and production happens before build resolution.
    suns::PlayerOrders buildOrders{1, {suns::BuildColonyShipOrder{earth->id}}};
    const auto afterBuild = processor.process(initial, {buildOrders});
    assert(initial.turn == 1);
    assert(afterBuild.turn == 2);
    assert(afterBuild.fleets.size() == 2);

    const auto* earthAfterBuild = suns::find_planet_at_star(afterBuild, 1);
    assert(earthAfterBuild != nullptr);
    assert(earthAfterBuild->stockpile == 1); // 4 + 1 production - 4 ship cost.

    const auto colonyShipIt = std::find_if(
        afterBuild.fleets.begin(), afterBuild.fleets.end(), [](const suns::Fleet& fleet) {
            return fleet.role == suns::FleetRole::ColonyShip;
        });
    assert(colonyShipIt != afterBuild.fleets.end());

    const auto* alpha = suns::find_star(afterBuild, 2);
    assert(alpha != nullptr);

    suns::PlayerOrders moveOrders{
        1,
        {suns::MoveFleetOrder{colonyShipIt->id, alpha->position}},
    };
    const auto afterMove = processor.process(afterBuild, {moveOrders});
    assert(afterMove.turn == 3);

    const auto movedShip = std::find_if(
        afterMove.fleets.begin(), afterMove.fleets.end(), [](const suns::Fleet& fleet) {
            return fleet.role == suns::FleetRole::ColonyShip;
        });
    assert(movedShip != afterMove.fleets.end());
    assert(suns::same_position(movedShip->position, alpha->position));

    const auto* alphaPlanet = suns::find_planet_at_star(afterMove, 2);
    assert(alphaPlanet != nullptr);
    assert(alphaPlanet->owner == 0);

    suns::PlayerOrders colonizeOrders{
        1,
        {suns::ColonizePlanetOrder{movedShip->id, alphaPlanet->id}},
    };
    const auto afterColonize = processor.process(afterMove, {colonizeOrders});
    assert(afterColonize.turn == 4);

    const auto* colony = suns::find_planet_at_star(afterColonize, 2);
    assert(colony != nullptr);
    assert(colony->owner == 1);
    assert(colony->population == 250);
    assert(std::none_of(
        afterColonize.fleets.begin(), afterColonize.fleets.end(), [](const suns::Fleet& fleet) {
            return fleet.role == suns::FleetRole::ColonyShip;
        }));

    // On the following empty turn both colonies contribute local production.
    const auto afterProduction = processor.process(afterColonize, {});
    const auto* earthAfterProduction = suns::find_planet_at_star(afterProduction, 1);
    const auto* alphaAfterProduction = suns::find_planet_at_star(afterProduction, 2);
    assert(earthAfterProduction != nullptr && alphaAfterProduction != nullptr);
    assert(earthAfterProduction->stockpile == 4);
    assert(alphaAfterProduction->stockpile == 1);

    // A different player cannot move the Terran scout.
    suns::PlayerOrders invalidMove{2, {suns::MoveFleetOrder{1, {999.0, 999.0}}}};
    const auto guarded = processor.process(initial, {invalidMove});
    assert(suns::same_position(guarded.fleets.front().position, initial.fleets.front().position));

    return 0;
}
