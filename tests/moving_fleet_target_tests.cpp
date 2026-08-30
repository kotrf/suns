#include "suns/communications.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

using namespace suns;

Fleet* fleet(GameState& state, FleetId id)
{
    const auto found = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == id;
    });
    return found == state.fleets.end() ? nullptr : &*found;
}

const Fleet* fleet(const GameState& state, FleetId id)
{
    const auto found = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == id;
    });
    return found == state.fleets.end() ? nullptr : &*found;
}

bool near(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

GameState pursuit_state()
{
    auto state = make_demo_game();
    state.stars[0].position = {0.0, 0.0};
    state.planets[0].star = state.stars[0].id;
    state.fleets.resize(1);

    auto& pursuer = state.fleets.front();
    pursuer.id = 1;
    pursuer.owner = 1;
    pursuer.name = "Pursuer";
    pursuer.position = {0.0, 0.0};
    pursuer.destination = Position{50.0, 0.0};
    pursuer.targetFleet = 2;
    pursuer.warp = 8;
    pursuer.fuel = fleet_fuel_capacity(state, pursuer);
    pursuer.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
    pursuer.ships = {{kScoutDesignId, 2}};
    normalize_fleet_composition(pursuer);

    Fleet target = pursuer;
    target.id = 2;
    target.name = "Target";
    target.position = {50.0, 0.0};
    target.destination = Position{200.0, 0.0};
    target.targetFleet = 0;
    target.warp = 4;
    target.arrivalAction.reset();
    target.ships = {{kColonyShipDesignId, 1}};
    normalize_fleet_composition(target);
    target.fuel = fleet_fuel_capacity(state, target);
    state.fleets.push_back(target);
    state.nextFleetId = 3;
    return state;
}

} // namespace

int main()
{
    using namespace suns;
    TurnProcessor processor;

    // The pursuer aims at the target's predicted end-of-turn position. It does
    // not claim a meeting merely because it reached the target's old position.
    auto state = pursuit_state();
    auto first = processor.process(state, {});
    assert(fleet(first, 1));
    assert(fleet(first, 2));
    assert(near(fleet(first, 1)->position.x, 64.0));
    assert(near(fleet(first, 2)->position.x, 66.0));
    assert(fleet(first, 1)->targetFleet == 2);

    // On the following turn the fleets enter the strategic encounter radius
    // immediately before x=82. Both stop at the shared encounter midpoint;
    // the pursuer is consumed and its heterogeneous stacks join the target.
    auto second = processor.process(first, {});
    assert(!fleet(second, 1));
    const auto* merged = fleet(second, 2);
    assert(merged);
    assert(near(merged->position.x, 81.915));
    assert(merged->destination && near(merged->destination->x, 200.0));
    assert(fleet_ship_count(*merged, kScoutDesignId) == 2);
    assert(fleet_ship_count(*merged, kColonyShipDesignId) == 1);

    // Reordering the backing vector cannot change the simultaneous movement
    // projection or meeting result.
    auto reversed = pursuit_state();
    std::reverse(reversed.fleets.begin(), reversed.fleets.end());
    auto reorderedFirst = processor.process(reversed, {});
    assert(near(fleet(reorderedFirst, 1)->position.x, fleet(first, 1)->position.x));
    assert(near(fleet(reorderedFirst, 2)->position.x, fleet(first, 2)->position.x));
    auto reorderedSecond = processor.process(reorderedFirst, {});
    assert(!fleet(reorderedSecond, 1));
    assert(near(fleet(reorderedSecond, 2)->position.x, merged->position.x));

    // A moving target can also be a plain waypoint. On meeting, the pursuing
    // fleet survives and completes the leg normally.
    auto rendezvousOnly = pursuit_state();
    fleet(rendezvousOnly, 1)->arrivalAction.reset();
    auto rendezvousFirst = processor.process(rendezvousOnly, {});
    auto rendezvousSecond = processor.process(rendezvousFirst, {});
    const auto* rendezvoused = fleet(rendezvousSecond, 1);
    assert(rendezvoused);
    assert(near(rendezvoused->position.x, 81.915));
    assert(rendezvoused->targetFleet == 0);
    assert(!rendezvoused->destination);

    // A target that disappeared before the turn clears the pursuit and emits a
    // typed warning instead of silently flying to a stale coordinate.
    auto lost = pursuit_state();
    std::erase_if(lost.fleets, [](const Fleet& candidate) { return candidate.id == 2; });
    const auto lostResult = processor.process_with_events(lost, {});
    const auto* abandoned = fleet(lostResult.state, 1);
    assert(abandoned);
    assert(abandoned->targetFleet == 0);
    assert(!abandoned->destination);
    assert(std::any_of(lostResult.events.begin(), lostResult.events.end(), [](const GameEvent& event) {
        return event.kind == GameEventKind::FleetTargetLost
            && event.fleet == 1 && event.quantity == 2;
    }));

    // Commands carry the FleetId, validate ownership, and do not freeze only a
    // coordinate snapshot into the onboard route.
    auto command = pursuit_state();
    auto* commandPursuer = fleet(command, 1);
    commandPursuer->destination.reset();
    commandPursuer->targetFleet = 0;
    commandPursuer->arrivalAction.reset();
    assert(submit_fleet_route_command(
        command,
        1,
        1,
        fleet(command, 2)->position,
        8,
        FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet},
        {},
        false,
        2));
    assert(fleet(command, 1)->targetFleet == 2);
    assert(!submit_fleet_route_command(
        command,
        1,
        1,
        fleet(command, 2)->position,
        8,
        FleetArrivalAction{},
        {},
        false,
        999));

    // An already co-located rendezvous resolves at the planning boundary, then
    // the surviving destination fleet may continue its own route that turn.
    auto colocated = pursuit_state();
    fleet(colocated, 2)->position = fleet(colocated, 1)->position;
    auto colocatedResult = processor.process(colocated, {});
    assert(!fleet(colocatedResult, 1));
    assert(fleet(colocatedResult, 2));
    assert(fleet(colocatedResult, 2)->position.x > 0.0);

    return 0;
}
