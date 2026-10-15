#include "suns/communications.hpp"
#include "suns/turn_processor.hpp"
#include <cassert>
#include <cmath>

using namespace suns;

int main()
{
    auto state = make_demo_game();
    assert(state.planets[0].population == 1'000'000);
    assert(population_capacity(state.planets[0]) == 2'500'000);
    assert(colony_output(state.planets[0]) == 6);
    assert(projected_population_growth(state.planets[0]) == 60'000);
    assert(std::abs(colonist_cargo_mass(1) - 0.01) < 1e-12);
    assert(colonist_cargo_mass(100) == 1.0);

    // Onboard arrival actions do not wait for an orbital report at HQ.
    state.stars[1].position = {500, 0};
    state.players[0].surveyedStars = {1};
    state.players[0].surveyKnowledge.clear();
    state.fleets.clear();
    Fleet colonizer;
    colonizer.id = 2;
    colonizer.owner = 1;
    colonizer.design = kColonyShipDesignId;
    colonizer.role = FleetRole::ColonyShip;
    colonizer.position = {490, 0};
    colonizer.destination = state.stars[1].position;
    colonizer.warp = 5;
    colonizer.fuel = 300;
    colonizer.colonists = 100;
    colonizer.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Colonize};
    state.fleets.push_back(colonizer);
    assert(survey_level(state, 1, 2) < SurveyLevel::OrbitalSurvey);
    const auto founded = TurnProcessor{}.process(state, {});
    assert(founded.planets[1].owner == 1);
    assert(founded.planets[1].population >= 100);
    assert(founded.fleets.empty());

    auto occupied = state;
    occupied.planets[1].owner = 2;
    assert(TurnProcessor{}.process(occupied, {}).planets[1].owner == 2);
    auto overloaded = state;
    overloaded.fleets[0].colonists = 600;
    assert(fleet_cargo_used(overloaded, overloaded.fleets[0]) > fleet_cargo_capacity(overloaded, overloaded.fleets[0]));
    assert(TurnProcessor{}.process(overloaded, {}).planets[1].owner == 0);

    // When Colonize fails on arrival, preserve the terminal intent on the
    // final waypoint instead of deleting it before the next opportunity.
    auto blocked = state;
    blocked.planets[1].owner = 2;
    auto stillOrbiting = TurnProcessor{}.process(blocked, {});
    assert(stillOrbiting.fleets.size() == 1);
    assert(stillOrbiting.fleets.front().arrivalAction);
    assert(stillOrbiting.fleets.front().arrivalAction->kind == FleetArrivalActionKind::Colonize);
    assert(stillOrbiting.fleets.front().destination.has_value());
    assert(same_position(*stillOrbiting.fleets.front().destination, stillOrbiting.stars[1].position));
    stillOrbiting.planets[1].owner = 0;
    const auto retried = TurnProcessor{}.process(stillOrbiting, {});
    assert(retried.planets[1].owner == 1);
    assert(retried.fleets.empty());

    // Arrival can precede a delayed command to Colonize. The onboard order
    // must execute when the packet physically reaches the fleet, even though
    // the old travel waypoint has long since completed.
    auto delayed = make_demo_game();
    delayed.stars[1].position = {600, 0};
    auto& waiting = delayed.fleets.front();
    waiting.design = kColonyShipDesignId;
    waiting.ships = {{kColonyShipDesignId, 1}};
    waiting.colonists = 100;
    waiting.position = delayed.stars[1].position;
    waiting.destination.reset();
    assert(submit_fleet_route_command(delayed, 1, waiting.id,
        delayed.stars[1].position, 5,
        {FleetArrivalActionKind::Colonize}, {}, false, 0, false));
    assert(!delayed.fleets.front().pendingCommands.empty());
    for (int year = 0; year < 10 && delayed.planets[1].owner == 0; ++year)
        delayed = TurnProcessor{}.process(delayed, {});
    assert(delayed.planets[1].owner == 1);
    assert(delayed.fleets.empty());

    // An explicit stop ignores stale position coordinates, clears tasks and
    // works for an immobilized ship. It still travels through the comms layer.
    auto local = make_demo_game();
    local.fleets[0].destination = Position{100, 0};
    local.fleets[0].waypointQueue.push_back({{200, 0}, 5});
    local.fleets[0].damagePercent = 100;
    MoveFleetOrder stop{1, {999, 999}, 5};
    stop.clearRoute = true;
    const auto stopped = TurnProcessor{}.process(local, {{1, {stop}}});
    assert(!stopped.fleets[0].destination && stopped.fleets[0].waypointQueue.empty());
    assert(same_position(stopped.fleets[0].position, {0, 0}));

    auto distant = make_demo_game();
    distant.fleets[0].position = {700, 0};
    distant.fleets[0].destination = Position{900, 0};
    distant.fleets[0].warp = 5;
    assert(submit_fleet_route_command(distant, 1, 1, {999, 999}, 5, {}, {}, false, 0, true));
    assert(!distant.fleets[0].pendingCommands.empty());
    assert(distant.fleets[0].pendingCommands.back().program.clearRoute);
    const auto stopDelivery = distant.fleets[0].pendingCommands.back().deliveryTurn;
    distant.fleets[0].pendingCommands.push_back({0, stopDelivery + 5,
        FleetRouteProgram{{1000, 0}, 5}});
    distant.turn = stopDelivery;
    deliver_due_fleet_commands(distant);
    assert(!distant.fleets[0].destination && distant.fleets[0].task == FleetTask::None);
    assert(distant.fleets[0].pendingCommands.empty()); // older packets cannot restart a stopped route
}
