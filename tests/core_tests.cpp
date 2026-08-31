#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

const suns::Planet& planet(const suns::GameState& state, suns::PlanetId id)
{
    for (const auto& candidate : state.planets) {
        if (candidate.id == id) return candidate;
    }
    assert(false);
    return state.planets.front();
}

const suns::Fleet* fleet(const suns::GameState& state, suns::FleetId id)
{
    for (const auto& candidate : state.fleets) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

const suns::Fleet* colony_ship(const suns::GameState& state)
{
    for (const auto& candidate : state.fleets) {
        if (candidate.owner == 1 && suns::fleet_can_colonize(state, candidate)) return &candidate;
    }
    return nullptr;
}

void verify_ship_designs()
{
    const auto state = suns::make_demo_game();
    const auto* scout = suns::find_ship_design(state, suns::kScoutDesignId);
    const auto* colony = suns::find_ship_design(state, suns::kColonyShipDesignId);
    assert(scout != nullptr);
    assert(colony != nullptr);

    assert(std::abs(suns::ship_design_mass(*scout) - 59.5) < 0.000001);
    assert(std::abs(suns::ship_design_speed(*scout) - 100.0) < 0.000001);
    assert(std::abs(suns::ship_design_sensor_range(*scout) - 90.0) < 0.000001);
    assert(!suns::ship_design_can_colonize(*scout));
    assert(suns::ship_design_cost(*scout) == 8);
    assert(suns::ship_design_max_warp(*scout) == 8);
    assert(std::abs(suns::ship_design_fuel_rate(*scout, 7) - 0.60) < 0.000001);
    assert(std::abs(suns::ship_design_fuel_rate(*scout, 8) - 1.00) < 0.000001);
    assert(suns::ship_design_fuel_rate(*scout, 9) == 0.0);

    assert(std::abs(suns::ship_design_mass(*colony) - 85.0) < 0.000001);
    assert(std::abs(suns::ship_design_speed(*colony) - 70.0) < 0.000001);
    assert(suns::ship_design_sensor_range(*colony) == 0.0);
    assert(suns::ship_design_can_colonize(*colony));
    assert(suns::ship_design_cost(*colony) == suns::kColonyShipCost);
    assert(suns::ship_design_max_warp(*colony) == 8);

    auto hybrid = *scout;
    hybrid.components.push_back(suns::ShipComponentType::ColonyModule);
    assert(suns::ship_design_can_colonize(hybrid));
    assert(suns::ship_design_sensor_range(hybrid) == 90.0);
    assert(suns::ship_design_speed(hybrid) < suns::ship_design_speed(*scout));
    assert(suns::ship_design_cost(hybrid) > suns::ship_design_cost(*scout));

    auto radiatingScout = *scout;
    radiatingScout.components.front() = suns::ShipComponentType::RadiatingRamScoopDrive;
    assert(suns::ship_design_max_warp(radiatingScout) == 9);
    assert(suns::ship_design_fuel_rate(radiatingScout, 10) == 0.0);

    auto mismatched = state.fleets.front();
    mismatched.role = suns::FleetRole::ColonyShip;
    assert(std::abs(suns::fleet_speed(state, mismatched) - 100.0) < 0.000001);
    assert(std::abs(suns::fleet_sensor_range(state, mismatched) - 90.0) < 0.000001);
    assert(!suns::fleet_can_colonize(state, mismatched));
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
    assert(first.shipDesigns.size() == 2);
    assert(suns::is_surveyed(first, 1, 1));
    assert(first.players.front().surveyedStars == repeat.players.front().surveyedStars);
    assert(first.fleets.size() == 1);
    assert(first.fleets.front().design == suns::kScoutDesignId);
    assert(first.fleets.front().warp == suns::kScoutCruiseWarp);
    assert(first.fleets.front().warp == 8);
    assert(suns::same_position(first.fleets.front().position, first.stars.front().position));

    for (std::size_t i = 0; i < first.stars.size(); ++i) {
        assert(first.stars[i].id == repeat.stars[i].id);
        assert(first.stars[i].name == repeat.stars[i].name);
        assert(first.stars[i].position.x == repeat.stars[i].position.x);
        assert(first.stars[i].position.y == repeat.stars[i].position.y);
        assert(first.planets[i].habitability == repeat.planets[i].habitability);
    }

    auto otherConfig = config;
    ++otherConfig.seed;
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
    const auto state = suns::make_demo_game();
    const auto& scout = state.fleets.front();
    assert(std::abs(suns::distance_between({0.0, 0.0}, {3.0, 4.0}) - 5.0) < 0.000001);
    assert(suns::travel_turns({0.0, 0.0}, {3.0, 4.0}, 2.0) == 3);
    assert(std::abs(suns::fleet_speed(state, scout) - suns::kScoutTravelSpeed) < 0.000001);
    assert(std::abs(suns::fleet_sensor_range(state, scout) - suns::kScoutSensorRange) < 0.000001);
    assert(suns::kColonySensorRange > suns::kScoutSensorRange);
    assert(suns::within_range({0.0, 0.0}, {3.0, 4.0}, 5.0));
    assert(!suns::within_range({0.0, 0.0}, {3.0, 4.0}, 4.9));
}

void verify_swept_sensor_coverage()
{
    auto state = suns::make_demo_game();
    state.players = {{1, "Terrans", {}}};
    state.planets.clear();
    state.stars = {
        {1, "Origin", {0.0, 0.0}, suns::StarClass::Yellow},
        {2, "Flyby", {50.0, 85.0}, suns::StarClass::White},
        {3, "Distant", {300.0, 0.0}, suns::StarClass::Red},
    };
    state.fleets = {{1, 1, "Scout 1", suns::FleetRole::Scout, suns::kScoutDesignId,
        {0.0, 0.0}, suns::Position{200.0, 0.0}}};
    const auto scannerRange = suns::fleet_sensor_range(state, state.fleets.front());
    assert(!suns::within_range({0.0, 0.0}, {50.0, 85.0}, scannerRange));
    assert(!suns::within_range({100.0, 0.0}, {50.0, 85.0}, scannerRange));
    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {});
    assert(suns::survey_level(next, 1, 2) == suns::SurveyLevel::SystemScan);
    assert(!suns::is_surveyed(next, 1, 2));
    assert(!suns::is_surveyed(next, 1, 3));
}

} // namespace

int main()
{
    verify_ship_designs();
    verify_procedural_generation();
    verify_travel_and_sensor_math();
    verify_swept_sensor_coverage();

    const suns::TurnProcessor processor;
    const auto initial = suns::make_demo_game();
    const auto& earth = planet(initial, 1);
    assert(suns::population_capacity(earth) == 2500);
    assert(suns::colony_output(earth) == 6);

    const auto* vega = suns::find_star(initial, 4);
    assert(vega != nullptr);
    assert(suns::travel_turns(initial.fleets.front().position, vega->position,
               suns::fleet_speed(initial, initial.fleets.front())) == 2);
    suns::PlayerOrders scoutOrders{1, {}};
    scoutOrders.orders.emplace_back(suns::MoveFleetOrder{1, vega->position});
    const auto scoutTravel1 = processor.process(initial, {scoutOrders});
    const auto* travellingScout = fleet(scoutTravel1, 1);
    assert(travellingScout != nullptr);
    assert(travellingScout->destination.has_value());
    assert(suns::fleet_eta(scoutTravel1, *travellingScout) == 2);
    assert(!suns::is_surveyed(scoutTravel1, 1, 4));

    const auto scoutTravel2 = processor.process(scoutTravel1, {});
    const auto* approachingScout = fleet(scoutTravel2, 1);
    assert(approachingScout != nullptr);
    assert(approachingScout->destination.has_value());
    assert(suns::fleet_eta(scoutTravel2, *approachingScout) == 1);
    assert(!suns::is_surveyed(scoutTravel2, 1, 4));
    const auto scoutReportArrived = processor.process(scoutTravel2, {});
    assert(suns::survey_level(scoutReportArrived, 1, 4) == suns::SurveyLevel::SystemScan);
    assert(!suns::is_surveyed(scoutReportArrived, 1, 4));

    suns::PlayerOrders queueShip{1, {}};
    queueShip.orders.emplace_back(suns::QueueShipDesignOrder{1, suns::kColonyShipDesignId});
    const auto shipTurn1 = processor.process(initial, {queueShip});
    assert(planet(shipTurn1, 1).productionQueue.front().remainingCost == 6);
    const auto shipTurn2 = processor.process(shipTurn1, {});
    const auto* builtShip = colony_ship(shipTurn2);
    assert(builtShip != nullptr);
    assert(builtShip->design == suns::kColonyShipDesignId);
    assert(builtShip->colonists == 0);
    assert(std::abs(suns::fleet_speed(shipTurn2, *builtShip) - suns::kColonyShipTravelSpeed) < 0.000001);
    assert(suns::fleet_sensor_range(shipTurn2, *builtShip) == 0.0);

    // Loading is now a real logistics action: population leaves Earth and
    // becomes cargo before the colonizer can depart usefully.
    suns::PlayerOrders loadShip{1, {}};
    loadShip.orders.emplace_back(suns::SetFleetColonistsOrder{1, builtShip->id, 250});
    const auto shipLoaded = processor.process(shipTurn2, {loadShip});
    const auto* loadedShip = colony_ship(shipLoaded);
    assert(loadedShip != nullptr);
    assert(loadedShip->colonists == 250);

    const auto* alpha = suns::find_star(initial, 2);
    assert(alpha != nullptr);
    suns::PlayerOrders moveColony{1, {}};
    moveColony.orders.emplace_back(suns::MoveFleetOrder{loadedShip->id, alpha->position});
    const auto colonyTravel1 = processor.process(shipLoaded, {moveColony});
    const auto* travellingShip = colony_ship(colonyTravel1);
    assert(travellingShip != nullptr);
    assert(travellingShip->destination.has_value());
    assert(suns::fleet_eta(colonyTravel1, *travellingShip) == 1);
    assert(!suns::is_surveyed(colonyTravel1, 1, 2));

    suns::PlayerOrders prematureColonize{1, {}};
    prematureColonize.orders.emplace_back(suns::ColonizePlanetOrder{travellingShip->id, 2});
    const auto rejected = processor.process(colonyTravel1, {prematureColonize});
    assert(planet(rejected, 2).owner == 0);
    const auto* arrivedShip = colony_ship(rejected);
    assert(arrivedShip != nullptr);
    assert(suns::same_position(arrivedShip->position, alpha->position));

    suns::PlayerOrders surveyDestination{1, {}};
    surveyDestination.orders.emplace_back(suns::MoveFleetOrder{1, alpha->position});
    const auto destinationScanned = processor.process(rejected, {surveyDestination});
    assert(suns::survey_level(destinationScanned, 1, 2) == suns::SurveyLevel::SystemScan);
    const auto destinationSurveyed = processor.process(destinationScanned, {});
    assert(suns::survey_level(destinationSurveyed, 1, 2) == suns::SurveyLevel::OrbitalSurvey);

    const auto* readyShip = colony_ship(destinationSurveyed);
    assert(readyShip != nullptr);
    suns::PlayerOrders colonize{1, {}};
    colonize.orders.emplace_back(suns::ColonizePlanetOrder{readyShip->id, 2});
    const auto expanded = processor.process(destinationSurveyed, {colonize});
    assert(planet(expanded, 2).owner == 1);
    assert(planet(expanded, 2).population == 267);
    assert(colony_ship(expanded) == nullptr);

    return 0;
}
