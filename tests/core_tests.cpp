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
    assert(first.players.front().surveyedStars.size() == 1);
    assert(first.players.front().surveyedStars.front() == 1);
    assert(first.stars.front().name == "Sol");
    assert(first.planets.front().name == "Earth");
    assert(first.planets.front().owner == 1);
    assert(first.fleets.size() == 1);
    assert(suns::same_position(first.fleets.front().position, first.stars.front().position));

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

} // namespace

int main()
{
    verify_procedural_generation();

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
    assert(!suns::is_surveyed(initial, 1, 2));

    const auto* alpha = suns::find_star(initial, 2);
    assert(alpha != nullptr);
    suns::PlayerOrders scoutOrders{1, {}};
    scoutOrders.orders.emplace_back(suns::MoveFleetOrder{1, alpha->position});
    const auto surveyedAlpha = processor.process(initial, {scoutOrders});
    assert(!suns::is_surveyed(initial, 1, 2));
    assert(suns::is_surveyed(surveyedAlpha, 1, 2));

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

    const auto* ship = colony_ship(shipTurn2);
    assert(ship != nullptr);

    suns::PlayerOrders moveColony{1, {}};
    moveColony.orders.emplace_back(suns::MoveFleetOrder{ship->id, alpha->position});
    const auto arrivedUnknown = processor.process(shipTurn2, {moveColony});
    assert(!suns::is_surveyed(arrivedUnknown, 1, 2));

    const auto* arrivedShip = colony_ship(arrivedUnknown);
    assert(arrivedShip != nullptr);
    suns::PlayerOrders prematureColonize{1, {}};
    prematureColonize.orders.emplace_back(
        suns::ColonizePlanetOrder{arrivedShip->id, 2});
    const auto rejected = processor.process(arrivedUnknown, {prematureColonize});
    assert(planet(rejected, 2).owner == 0);

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
