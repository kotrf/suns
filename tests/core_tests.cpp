#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

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

const suns::Fleet* fleet(const suns::GameState& state, suns::FleetId id)
{
    for (const auto& candidate : state.fleets) {
        if (candidate.id == id) {
            return &candidate;
        }
    }
    return nullptr;
}

const suns::Fleet* colony_ship(const suns::GameState& state)
{
    for (const auto& candidate : state.fleets) {
        if (candidate.owner == 1 && candidate.role == suns::FleetRole::ColonyShip) {
            return &candidate;
        }
    }
    return nullptr;
}

void verify_procedural_generation()
{
    suns::GalaxyConfig config;
    config.seed = 424242;
    config.starCount = 24;

    const auto first = suns::generate_game(config);
    const auto repeat = suns::generate_game(config);

    assert(first.galaxySeed == config.seed);
    assert(first.stars.size() == config.starCount);
    assert(first.planets.size() == config.starCount);
    assert(first.players.size() == 1);
    assert(suns::is_surveyed(first, 1, 1));
    assert(first.players.front().surveyedStars == repeat.players.front().surveyedStars);
    assert(first.stars.front().name == "Sol");
    assert(first.planets.front().name == "Earth");
    assert(first.planets.front().owner == 1);
    assert(first.fleets.size() == 1);
    assert(suns::same_position(first.fleets.front().position, first.stars.front().position));
    assert(!first.fleets.front().destination.has_value());

    for (std::size_t i = 0; i < first.stars.size(); ++i) {
        const auto& a = first.stars[i];
        const auto& b = repeat.stars[i];
        assert(a.id == b.id);
        assert(a.name == b.name);
        assert(a.position.x == b.position.x);
        assert(a.position.y == b.position.y);
        assert(a.stellarClass == b.stellarClass);

        const auto& pa = first.planets[i];
        const auto& pb = repeat.planets[i];
        assert(pa.name == pb.name);
        assert(pa.habitability == pb.habitability);
    }

    auto otherConfig = config;
    otherConfig.seed += 1;
    const auto other = suns::generate_game(otherConfig);
    bool differs = false;
    for (std::size_t i = 1; i < first.stars.size(); ++i) {
        if (first.stars[i].name != other.stars[i].name
            || first.stars[i].position.x != other.stars[i].position.x
            || first.stars[i].position.y != other.stars[i].position.y
            || first.planets[i].habitability != other.planets[i].habitability) {
            differs = true;
            break;
        }
    }
    assert(differs);
}

void verify_travel_and_sensor_math()
{
    assert(std::abs(suns::distance_between({0.0, 0.0}, {3.0, 4.0}) - 5.0) < 0.000001);
    assert(suns::travel_turns({0.0, 0.0}, {3.0, 4.0}, 2.0) == 3);
    assert(suns::travel_turns({0.0, 0.0}, {0.0, 0.0}, 2.0) == 0);
    assert(suns::fleet_speed(suns::FleetRole::Scout) > suns::fleet_speed(suns::FleetRole::ColonyShip));
    assert(suns::fleet_sensor_range(suns::FleetRole::Scout) == suns::kScoutSensorRange);
    assert(suns::fleet_sensor_range(suns::FleetRole::ColonyShip) == 0.0);
    assert(suns::within_range({0.0, 0.0}, {3.0, 4.0}, 5.0));
    assert(!suns::within_range({0.0, 0.0}, {3.0, 4.0}, 4.9));
}

void verify_swept_sensor_coverage()
{
    suns::GameState state;
    state.players.push_back({1, "Terrans", {}});
    state.stars = {
        {1, "Origin", {0.0, 0.0}, suns::StarClass::Yellow},
        {2, "Flyby", {50.0, 85.0}, suns::StarClass::White},
        {3, "Distant", {300.0, 0.0}, suns::StarClass::Red},
    };
    state.fleets.push_back({
        1,
        1,
        "Scout 1",
        suns::FleetRole::Scout,
        {0.0, 0.0},
        suns::Position{200.0, 0.0},
    });

    // Flyby is outside the scanner at both the start and the end of this turn,
    // but lies inside the moving sensor circle along the travelled segment.
    assert(!suns::within_range({0.0, 0.0}, {50.0, 85.0}, suns::kScoutSensorRange));
    assert(!suns::within_range({100.0, 0.0}, {50.0, 85.0}, suns::kScoutSensorRange));

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {});
    assert(suns::is_surveyed(next, 1, 2));
    assert(!suns::is_surveyed(next, 1, 3));
}

} // namespace

int main()
{
    verify_procedural_generation();
    verify_travel_and_sensor_math();
    verify_swept_sensor_coverage();

    const suns::TurnProcessor processor;
    const auto initial = suns::make_demo_game();

    const auto& earth = planet(initial, 1);
    assert(suns::population_capacity(earth) == 2500);
    assert(suns::projected_population_growth(earth) == 60);
    assert(suns::colony_output(earth) == 6);

    auto highQuality = earth;
    highQuality.population = 250;
    highQuality.habitability = 100;
    auto lowQuality = highQuality;
    lowQuality.habitability = 50;
    assert(suns::population_capacity(highQuality) > suns::population_capacity(lowQuality));
    assert(suns::projected_population_growth(highQuality) > suns::projected_population_growth(lowQuality));

    const auto peacefulTurn = processor.process(initial, {});
    assert(planet(peacefulTurn, 1).stockpile == 6);
    assert(planet(peacefulTurn, 1).population == 1060);

    assert(suns::is_surveyed(initial, 1, 1));
    assert(!suns::is_surveyed(initial, 1, 4));

    // Vega takes two travel turns, but Scout 1's scanner reaches it after the
    // first movement phase, before the ship physically arrives.
    const auto* vega = suns::find_star(initial, 4);
    assert(vega != nullptr);
    assert(suns::travel_turns(initial.fleets.front().position, vega->position, suns::kScoutTravelSpeed) == 2);

    suns::PlayerOrders scoutOrders{1, {}};
    scoutOrders.orders.emplace_back(suns::MoveFleetOrder{1, vega->position});
    const auto scoutTravel1 = processor.process(initial, {scoutOrders});
    const auto* travellingScout = fleet(scoutTravel1, 1);
    assert(travellingScout != nullptr);
    assert(travellingScout->destination.has_value());
    assert(suns::fleet_eta(*travellingScout) == 1);
    assert(!suns::same_position(travellingScout->position, vega->position));
    assert(suns::is_surveyed(scoutTravel1, 1, 4));

    const auto scoutArrived = processor.process(scoutTravel1, {});
    const auto* arrivedScout = fleet(scoutArrived, 1);
    assert(arrivedScout != nullptr);
    assert(!arrivedScout->destination.has_value());
    assert(suns::same_position(arrivedScout->position, vega->position));
    assert(suns::is_surveyed(scoutArrived, 1, 4));

    suns::PlayerOrders queueShip{1, {}};
    queueShip.orders.emplace_back(
        suns::QueueProductionOrder{1, suns::ProductionKind::ColonyShip});

    const auto shipTurn1 = processor.process(initial, {queueShip});
    assert(planet(shipTurn1, 1).productionQueue.size() == 1);
    assert(planet(shipTurn1, 1).productionQueue.front().remainingCost == 6);
    assert(colony_ship(shipTurn1) == nullptr);

    const auto shipTurn2 = processor.process(shipTurn1, {});
    assert(planet(shipTurn2, 1).productionQueue.empty());
    assert(colony_ship(shipTurn2) != nullptr);

    suns::PlayerOrders queueFactory{1, {}};
    queueFactory.orders.emplace_back(
        suns::QueueProductionOrder{1, suns::ProductionKind::Factory});

    const auto factoryTurn1 = processor.process(initial, {queueFactory});
    assert(planet(factoryTurn1, 1).productionQueue.empty());
    assert(planet(factoryTurn1, 1).industry == 5);
    assert(planet(factoryTurn1, 1).population == 1060);
    assert(suns::colony_output(planet(factoryTurn1, 1)) == 7);

    const auto factoryTurn2 = processor.process(factoryTurn1, {});
    assert(planet(factoryTurn2, 1).stockpile == 7);

    const auto* alpha = suns::find_star(initial, 2);
    assert(alpha != nullptr);
    const auto* ship = colony_ship(shipTurn2);
    assert(ship != nullptr);
    assert(suns::travel_turns(ship->position, alpha->position, suns::fleet_speed(ship->role)) == 2);

    // Colony ships are slower and have no survey scanner in the current fit.
    suns::PlayerOrders moveColony{1, {}};
    moveColony.orders.emplace_back(suns::MoveFleetOrder{ship->id, alpha->position});
    const auto colonyTravel1 = processor.process(shipTurn2, {moveColony});
    const auto* travellingShip = colony_ship(colonyTravel1);
    assert(travellingShip != nullptr);
    assert(travellingShip->destination.has_value());
    assert(suns::fleet_eta(*travellingShip) == 1);
    assert(!suns::same_position(travellingShip->position, alpha->position));
    assert(!suns::is_surveyed(colonyTravel1, 1, 2));

    suns::PlayerOrders prematureColonize{1, {}};
    prematureColonize.orders.emplace_back(
        suns::ColonizePlanetOrder{travellingShip->id, 2});
    const auto rejected = processor.process(colonyTravel1, {prematureColonize});
    assert(planet(rejected, 2).owner == 0);
    const auto* arrivedShip = colony_ship(rejected);
    assert(arrivedShip != nullptr);
    assert(!arrivedShip->destination.has_value());
    assert(suns::same_position(arrivedShip->position, alpha->position));
    assert(!suns::is_surveyed(rejected, 1, 2));

    // Scout 1 reaches Alpha in one turn; the world becomes known through sensor
    // coverage, then can be colonized by the ship already waiting there.
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
    assert(planet(expanded, 2).population == 267);
    assert(suns::population_capacity(planet(expanded, 2)) == 2050);
    assert(planet(expanded, 2).industry == 1);
    assert(planet(expanded, 2).stockpile == 1);
    assert(colony_ship(expanded) == nullptr);

    suns::PlayerOrders invalid{2, {}};
    invalid.orders.emplace_back(
        suns::QueueProductionOrder{1, suns::ProductionKind::Factory});
    const auto guarded = processor.process(initial, {invalid});
    assert(planet(guarded, 1).productionQueue.empty());

    return 0;
}
