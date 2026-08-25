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

void verify_waypoints_advance_one_leg_per_turn()
{
    suns::TurnProcessor processor;
    auto state = suns::make_demo_game();
    state.stars[1].position = {48.0, 0.0};
    const auto fuelCapacity = suns::fleet_fuel_capacity(state, state.fleets.front());

    suns::PlayerOrders orders{1, {}};
    orders.orders.emplace_back(suns::MoveFleetOrder{
        1,
        state.stars[1].position, // Alpha Centauri: reachable in one W8 turn.
        8,
        {},
        {
            {state.stars[0].position, 8, {suns::FleetArrivalActionKind::Refuel, 1}},
            {state.stars[2].position, 8, {}},
        },
    });

    const auto turn2 = processor.process(state, {orders});
    const auto* afterAlpha = fleet(turn2, 1);
    assert(afterAlpha != nullptr);
    assert(suns::same_position(afterAlpha->position, state.stars[1].position));
    assert(afterAlpha->destination.has_value());
    assert(suns::same_position(*afterAlpha->destination, state.stars[0].position));
    assert(afterAlpha->warp == 8);
    assert(afterAlpha->arrivalAction.has_value());
    assert(afterAlpha->arrivalAction->kind == suns::FleetArrivalActionKind::Refuel);
    assert(afterAlpha->waypointQueue.size() == 1);
    assert(suns::same_position(afterAlpha->waypointQueue.front().destination, state.stars[2].position));

    // The second leg is active after the first arrival, but it must not consume
    // any distance in the same turn.
    assert(suns::same_position(afterAlpha->position, state.stars[1].position));

    const auto turn3 = processor.process(turn2, {});
    const auto* afterSol = fleet(turn3, 1);
    assert(afterSol != nullptr);
    assert(suns::same_position(afterSol->position, state.stars[0].position));
    assert(std::abs(afterSol->fuel - fuelCapacity) < 0.000001); // Refuel on arrival.
    assert(afterSol->destination.has_value());
    assert(suns::same_position(*afterSol->destination, state.stars[2].position));
    assert(afterSol->warp == 8);
    assert(!afterSol->arrivalAction.has_value());
    assert(afterSol->waypointQueue.empty());

    // Again, activation does not immediately move the newly promoted leg.
    assert(suns::same_position(afterSol->position, state.stars[0].position));

    const auto turn4 = processor.process(turn3, {});
    const auto* towardSirius = fleet(turn4, 1);
    assert(towardSirius != nullptr);
    assert(!suns::same_position(towardSirius->position, state.stars[0].position));
}

void verify_replot_replaces_future_program()
{
    suns::TurnProcessor processor;
    auto state = suns::make_demo_game();
    state.stars[1].position = {48.0, 0.0};

    suns::PlayerOrders initial{1, {}};
    initial.orders.emplace_back(suns::MoveFleetOrder{
        1,
        state.stars[1].position,
        8,
        {},
        {
            {state.stars[0].position, 8, {suns::FleetArrivalActionKind::Refuel, 1}},
            {state.stars[2].position, 8, {}},
        },
    });
    auto turn2 = processor.process(state, {initial});
    assert(fleet(turn2, 1)->waypointQueue.size() == 1);

    // Replot from Alpha Centauri directly to Vega. The old Sol/Sirius program
    // must disappear as one atomic change of intent.
    suns::PlayerOrders replot{1, {}};
    replot.orders.emplace_back(suns::MoveFleetOrder{
        1,
        state.stars[3].position,
        8,
        {},
        {},
    });
    const auto turn3 = processor.process(turn2, {replot});
    const auto* changed = fleet(turn3, 1);
    assert(changed != nullptr);
    assert(changed->waypointQueue.empty());
    assert(!changed->arrivalAction.has_value());
    assert(changed->warp == 8);
    assert(changed->destination.has_value());
    assert(suns::same_position(*changed->destination, state.stars[3].position));
}

void verify_invalid_future_warp_rejects_program()
{
    suns::TurnProcessor processor;
    auto state = suns::make_demo_game();

    // Temporarily give the Scout design the W9-limited ram-scoop engine.
    auto& design = state.shipDesigns.front();
    design.components[0] = suns::ShipComponentType::RamScoopDrive;
    assert(suns::ship_design_max_warp(design) == 9);

    suns::PlayerOrders invalid{1, {}};
    invalid.orders.emplace_back(suns::MoveFleetOrder{
        1,
        state.stars[1].position,
        8,
        {},
        {
            {state.stars[2].position, 10, {}}, // Impossible future leg.
        },
    });

    const auto next = processor.process(state, {invalid});
    const auto* unchanged = fleet(next, 1);
    assert(unchanged != nullptr);
    assert(!unchanged->destination.has_value());
    assert(unchanged->waypointQueue.empty());
}

} // namespace

int main()
{
    verify_waypoints_advance_one_leg_per_turn();
    verify_replot_replaces_future_program();
    verify_invalid_future_warp_rejects_program();
    return 0;
}
