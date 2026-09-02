#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>

namespace {

const suns::Fleet* fleet(const suns::GameState& state, suns::FleetId id)
{
    for (const auto& candidate : state.fleets) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

const suns::Planet* planet(const suns::GameState& state, suns::PlanetId id)
{
    for (const auto& candidate : state.planets) {
        if (candidate.id == id) return &candidate;
    }
    return nullptr;
}

suns::GameState logistics_fixture()
{
    auto state = suns::make_demo_game();
    state.planets.front().habitability = 0; // Keep population fixed during transfer tests.

    const auto* design = suns::find_ship_design(state, suns::kColonyShipDesignId);
    assert(design != nullptr);
    state.fleets.push_back({
        2,
        1,
        "Colony Ship 2",
        suns::FleetRole::ColonyShip,
        design->id,
        state.stars.front().position,
        std::nullopt,
        suns::kColonyShipCruiseWarp,
        12.0,
        0,
    });
    state.nextFleetId = 3;
    return state;
}

void verify_design_production()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();

    suns::PlayerOrders order{1, {}};
    order.orders.emplace_back(suns::QueueShipDesignOrder{1, suns::kScoutDesignId});
    const auto first = processor.process(state, {order});

    const auto* earth1 = planet(first, 1);
    assert(earth1 != nullptr);
    assert(earth1->productionQueue.size() == 1);
    assert(earth1->productionQueue.front().shipDesign == suns::kScoutDesignId);
    assert(earth1->productionQueue.front().remainingCost == 2);
    assert(fleet(first, 2) == nullptr);

    const auto second = processor.process(first, {});
    const auto* built = fleet(second, 2);
    assert(built != nullptr);
    assert(built->design == suns::kScoutDesignId);
    assert(built->colonists == 0);
    assert(built->name == "Scout 2");
    assert(std::abs(built->fuel - suns::fleet_fuel_capacity(second, *built)) < 0.000001);
}

void verify_colonist_loading_and_unloading()
{
    const suns::TurnProcessor processor;
    auto state = logistics_fixture();

    suns::PlayerOrders load{1, {}};
    load.orders.emplace_back(suns::SetFleetColonistsOrder{1, 2, 300});
    const auto loaded = processor.process(state, {load});
    assert(fleet(loaded, 2)->colonists == 300);
    assert(planet(loaded, 1)->population == 700);

    suns::PlayerOrders unload{1, {}};
    unload.orders.emplace_back(suns::SetFleetColonistsOrder{1, 2, 100});
    const auto unloaded = processor.process(loaded, {unload});
    assert(fleet(unloaded, 2)->colonists == 100);
    assert(planet(unloaded, 1)->population == 900);

    suns::PlayerOrders overload{1, {}};
    overload.orders.emplace_back(suns::SetFleetColonistsOrder{1, 2, 600});
    const auto rejected = processor.process(unloaded, {overload});
    assert(fleet(rejected, 2)->colonists == 100);
    assert(planet(rejected, 1)->population == 900);
}

void verify_loading_precedes_movement_in_the_same_turn()
{
    const suns::TurnProcessor processor;
    auto state = logistics_fixture();
    const auto origin = fleet(state, 2)->position;

    suns::PlayerOrders orders{1, {}};
    orders.orders.emplace_back(suns::SetFleetColonistsOrder{1, 2, 300});
    orders.orders.emplace_back(suns::MoveFleetOrder{2, {origin.x + 20.0, origin.y}, 4});
    const auto next = processor.process(state, {orders});

    assert(fleet(next, 2)->colonists == 300);
    assert(!suns::same_position(fleet(next, 2)->position, origin));
    assert(planet(next, 1)->population == 700);
}

void verify_colony_cannot_be_accidentally_emptied()
{
    const suns::TurnProcessor processor;
    auto state = logistics_fixture();
    state.planets.front().population = 100;

    suns::PlayerOrders order{1, {}};
    order.orders.emplace_back(suns::SetFleetColonistsOrder{1, 2, 100});
    const auto next = processor.process(state, {order});
    assert(fleet(next, 2)->colonists == 0);
    assert(planet(next, 1)->population == 100);
}

void verify_automatic_station_refuel()
{
    const suns::TurnProcessor processor;
    auto state = logistics_fixture();
    const auto capacity = suns::fleet_fuel_capacity(state, *fleet(state, 2));
    assert(capacity > 12.0);

    // The homeworld station has a refueling depot, so no explicit order is
    // needed at the planning boundary.
    const auto refuelled = processor.process(state, {});
    assert(std::abs(fleet(refuelled, 2)->fuel - capacity) < 0.000001);

    // Legacy orders remain valid for old saves and multiplayer turn files.
    auto legacyState = logistics_fixture();
    suns::PlayerOrders refuel{1, {}};
    refuel.orders.emplace_back(suns::RefuelFleetOrder{1, 2});
    const auto legacyRefuelled = processor.process(legacyState, {refuel});
    assert(std::abs(fleet(legacyRefuelled, 2)->fuel - capacity) < 0.000001);

    auto away = logistics_fixture();
    away.fleets.back().position = {20.0, 20.0};
    suns::PlayerOrders invalid{1, {}};
    invalid.orders.emplace_back(suns::RefuelFleetOrder{1, 2});
    const auto rejected = processor.process(away, {invalid});
    assert(std::abs(fleet(rejected, 2)->fuel - 12.0) < 0.000001);
}

} // namespace

int main()
{
    verify_design_production();
    verify_colonist_loading_and_unloading();
    verify_loading_precedes_movement_in_the_same_turn();
    verify_colony_cannot_be_accidentally_emptied();
    verify_automatic_station_refuel();
    return 0;
}
