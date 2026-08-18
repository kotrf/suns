#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace {

using namespace suns;

struct ArrivalFixture {
    GameState state{generate_game(GalaxyConfig{})};
    PlanetId targetPlanet{};
    FleetId colonyFleet{};
};

ArrivalFixture make_arrival_fixture(std::uint64_t colonists)
{
    ArrivalFixture fixture;

    auto home = std::find_if(fixture.state.planets.begin(), fixture.state.planets.end(), [](const Planet& planet) {
        return planet.owner == 1;
    });
    auto target = std::find_if(fixture.state.planets.begin(), fixture.state.planets.end(), [](const Planet& planet) {
        return planet.owner == 0;
    });
    assert(home != fixture.state.planets.end());
    assert(target != fixture.state.planets.end());

    const auto* homeStar = find_star(fixture.state, home->star);
    assert(homeStar);

    auto targetStar = std::find_if(fixture.state.stars.begin(), fixture.state.stars.end(), [&](const StarSystem& star) {
        return star.id == target->star;
    });
    assert(targetStar != fixture.state.stars.end());
    targetStar->position = {homeStar->position.x + 10.0, homeStar->position.y};
    mark_surveyed(fixture.state, 1, targetStar->id);

    const auto* design = find_ship_design(fixture.state, kColonyShipDesignId);
    assert(design);

    fixture.colonyFleet = fixture.state.nextFleetId++;
    fixture.state.fleets.push_back({
        fixture.colonyFleet,
        1,
        "Arrival Colonizer",
        FleetRole::ColonyShip,
        kColonyShipDesignId,
        homeStar->position,
        std::nullopt,
        kColonyShipCruiseWarp,
        ship_design_fuel_capacity(*design),
        colonists,
        std::nullopt,
        {},
        // 250 colonists use 2.5 of the default 5 cargo units; keep the
        // mixed mineral manifest inside the remaining capacity.
        MineralCargo{1.0, 0.5, 0.5},
    });

    fixture.targetPlanet = target->id;
    return fixture;
}

void colonize_on_arrival_establishes_colony_and_consumes_ship()
{
    auto fixture = make_arrival_fixture(250);
    const auto targetPlanet = std::find_if(fixture.state.planets.begin(), fixture.state.planets.end(), [&](const Planet& planet) {
        return planet.id == fixture.targetPlanet;
    });
    assert(targetPlanet != fixture.state.planets.end());
    const auto* targetStar = find_star(fixture.state, targetPlanet->star);
    assert(targetStar);

    MoveFleetOrder move;
    move.fleet = fixture.colonyFleet;
    move.destination = targetStar->position;
    move.warp = kColonyShipCruiseWarp;
    move.arrivalAction.kind = FleetArrivalActionKind::Colonize;

    TurnProcessor processor;
    const auto next = processor.process(fixture.state, {{1, {move}}});

    const auto colonized = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& planet) {
        return planet.id == fixture.targetPlanet;
    });
    assert(colonized != next.planets.end());
    assert(colonized->owner == 1);
    assert(colonized->population >= 250);
    assert(colonized->minerals.ironium >= 1.0);
    assert(colonized->minerals.boranium >= 0.5);
    assert(colonized->minerals.germanium >= 0.5);

    const auto ship = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& fleet) {
        return fleet.id == fixture.colonyFleet;
    });
    assert(ship == next.fleets.end());
}

void failed_colonize_on_arrival_keeps_ship()
{
    auto fixture = make_arrival_fixture(0);
    const auto target = std::find_if(fixture.state.planets.begin(), fixture.state.planets.end(), [&](const Planet& planet) {
        return planet.id == fixture.targetPlanet;
    });
    assert(target != fixture.state.planets.end());
    const auto* targetStar = find_star(fixture.state, target->star);
    assert(targetStar);

    MoveFleetOrder move;
    move.fleet = fixture.colonyFleet;
    move.destination = targetStar->position;
    move.warp = kColonyShipCruiseWarp;
    move.arrivalAction.kind = FleetArrivalActionKind::Colonize;

    TurnProcessor processor;
    const auto next = processor.process(fixture.state, {{1, {move}}});

    const auto planet = std::find_if(next.planets.begin(), next.planets.end(), [&](const Planet& candidate) {
        return candidate.id == fixture.targetPlanet;
    });
    assert(planet != next.planets.end());
    assert(planet->owner == 0);

    const auto ship = std::find_if(next.fleets.begin(), next.fleets.end(), [&](const Fleet& fleet) {
        return fleet.id == fixture.colonyFleet;
    });
    assert(ship != next.fleets.end());
    assert(same_position(ship->position, targetStar->position));
}

} // namespace

int main()
{
    colonize_on_arrival_establishes_colony_and_consumes_ship();
    failed_colonize_on_arrival_keeps_ship();
    std::cout << "route program tests passed\n";
    return 0;
}
