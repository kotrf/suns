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
    assert(std::abs(colonist_cargo_mass(1) - 0.0001) < 1e-12);
    assert(colonist_cargo_mass(10'000) == 1.0);

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
    colonizer.colonists = 10'000;
    colonizer.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Colonize};
    state.fleets.push_back(colonizer);
    assert(survey_level(state, 1, 2) < SurveyLevel::OrbitalSurvey);
    const auto founded = TurnProcessor{}.process(state, {});
    assert(founded.planets[1].owner == 1);
    assert(founded.planets[1].population >= 10'000);
    assert(founded.fleets.empty());

    auto occupied = state;
    occupied.planets[1].owner = 2;
    assert(TurnProcessor{}.process(occupied, {}).planets[1].owner == 2);
    auto overloaded = state;
    overloaded.fleets[0].colonists = 60'000;
    assert(fleet_cargo_used(overloaded, overloaded.fleets[0]) > fleet_cargo_capacity(overloaded, overloaded.fleets[0]));
    assert(TurnProcessor{}.process(overloaded, {}).planets[1].owner == 0);

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
