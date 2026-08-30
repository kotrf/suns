#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

using namespace suns;

bool near(double left, double right, double epsilon = 0.000001)
{
    return std::abs(left - right) < epsilon;
}

const Fleet* fleet(const GameState& state, FleetId id)
{
    const auto found = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == id;
    });
    return found == state.fleets.end() ? nullptr : &*found;
}

void verify_encounter_geometry()
{
    const auto headOn = analyze_fleet_encounter(
        {0.0, 0.0}, {10.0, 0.0}, {10.0, 0.0}, {0.0, 0.0});
    assert(headOn.encounterTimeFraction);
    assert(near(headOn.closestTimeFraction, 0.5));
    assert(near(headOn.closestDistance, 0.0));
    assert(near(*headOn.encounterTimeFraction, 0.4995));
    assert(near(headOn.encounterPosition.x, 5.0));

    const auto crossing = analyze_fleet_encounter(
        {0.0, 0.0}, {10.0, 0.0}, {5.0, -5.0}, {5.0, 5.0});
    assert(crossing.encounterTimeFraction);
    assert(near(crossing.closestTimeFraction, 0.5));
    assert(near(crossing.closestDistance, 0.0));
    assert(*crossing.encounterTimeFraction < crossing.closestTimeFraction);
    assert(distance_between(crossing.encounterPosition, {5.0, 0.0}) < kFleetEncounterRadius);

    const auto sameDirectionCatch = analyze_fleet_encounter(
        {0.0, 0.0}, {10.0, 0.0}, {5.0, 0.0}, {8.0, 0.0});
    assert(sameDirectionCatch.encounterTimeFraction);
    assert(near(sameDirectionCatch.closestDistance, 0.0));

    // The spatial paths cross at (5, 0), but the target arrives there only at
    // the end of the turn, after the pursuer passed it at T+0.5.
    const auto differentTimes = analyze_fleet_encounter(
        {0.0, 0.0}, {10.0, 0.0}, {5.0, -10.0}, {5.0, 0.0});
    assert(!differentTimes.encounterTimeFraction);
    assert(differentTimes.closestDistance > kFleetEncounterRadius);

    const auto impossibleChase = analyze_fleet_encounter(
        {0.0, 0.0}, {5.0, 0.0}, {10.0, 0.0}, {20.0, 0.0});
    assert(!impossibleChase.encounterTimeFraction);
    assert(near(impossibleChase.closestTimeFraction, 0.0));
    assert(near(impossibleChase.closestDistance, 10.0));

    const auto highSpeedMiss = analyze_fleet_encounter(
        {0.0, 0.0}, {0.0, 0.0}, {-100.0, 1.0}, {100.0, 1.0});
    assert(!highSpeedMiss.encounterTimeFraction);
    assert(near(highSpeedMiss.closestTimeFraction, 0.5));
    assert(near(highSpeedMiss.closestDistance, 1.0));
}

void verify_within_turn_flyby_stops_both_fleets()
{
    auto state = make_demo_game();
    state.stars.clear();
    state.planets.clear();
    state.fleets.clear();
    state.shipDesigns.push_back({
        kFirstCustomShipDesignId,
        1,
        "Fast scoop",
        ShipHullType::Scout,
        {ShipComponentType::RamScoopDrive},
    });

    Fleet pursuer{
        1, 1, "Pursuer", FleetRole::Scout, kScoutDesignId,
        {0.0, 0.0}, Position{-71.0, 0.0}, 8, 300.0, 0,
    };
    pursuer.targetFleet = 2;
    pursuer.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
    pursuer.ships = {{kScoutDesignId, 1}};

    Fleet target{
        2, 1, "Fast target", FleetRole::Scout, kFirstCustomShipDesignId,
        {10.0, 0.0}, Position{-100.0, 0.0}, 9, 300.0, 0,
    };
    target.ships = {{kFirstCustomShipDesignId, 1}};
    state.fleets = {pursuer, target};
    state.nextFleetId = 3;

    // Their ordinary endpoints are -64 and -71, so endpoint-only resolution
    // would miss a head-on crossing near x=-37.65.
    const TurnProcessor processor;
    const auto next = processor.process(state, {});
    assert(!fleet(next, 1));
    const auto* merged = fleet(next, 2);
    assert(merged);
    assert(near(merged->position.x, -37.6044117647059));
    assert(fleet_ship_count(*merged, kScoutDesignId) == 1);
    assert(fleet_ship_count(*merged, kFirstCustomShipDesignId) == 1);
    assert(merged->destination);
    assert(near(merged->destination->x, -100.0));
}

} // namespace

int main()
{
    verify_encounter_geometry();
    verify_within_turn_flyby_stops_both_fleets();
    return 0;
}
