#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/mobile_base.hpp"
#include "suns/turn_processor.hpp"
#include <cassert>
#include <cmath>
#include <limits>

using namespace suns;

static const Fleet& fleet(const GameState& state, FleetId id)
{
    for (const auto& f : state.fleets) if (f.id == id) return f;
    assert(false); return state.fleets.front();
}
static bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
static GameState pair()
{
    auto state = make_demo_game(); state.orbitalStations.clear();
    auto& first = state.fleets.front(); first.telemetry = {}; first.fuel = 200;
    auto second = first; second.id = 2; second.name = "Receiver"; second.fuel = 10;
    state.fleets.push_back(second); state.nextFleetId = 3;
    return state;
}

int main()
{
    // Ordinary tank transfer: conservation, donor reserve, receiver capacity,
    // ownership/co-location and malformed amounts use the same physical rules.
    auto state = pair();
    assert(transfer_mobile_base_fuel(state, 1, {1, 2, 50, 175}, state.turn));
    assert(near(fleet(state, 1).fuel, 175) && near(fleet(state, 2).fuel, 35));
    state.fleets[1].fuel = fleet_fuel_capacity(state, state.fleets[1]) - 3;
    const auto total = state.fleets[0].fuel + state.fleets[1].fuel;
    assert(transfer_mobile_base_fuel(state, 1, {1, 2, 50, 0}, state.turn));
    assert(near(state.fleets[0].fuel + state.fleets[1].fuel, total));
    assert(near(state.fleets[1].fuel, fleet_fuel_capacity(state, state.fleets[1])));
    for (const auto amount : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        assert(!transfer_mobile_base_fuel(state, 1, {1, 2, amount, 0}, state.turn));
    assert(!transfer_mobile_base_fuel(state, 2, {1, 2, 1, 0}, state.turn));
    assert(!transfer_mobile_base_fuel(state, 1, {1, 1, 1, 0}, state.turn));
    state.fleets[1].position.x += 10;
    assert(!transfer_mobile_base_fuel(state, 1, {1, 2, 1, 0}, state.turn));

    state = pair();
    const PlayerOrders fuel{1, {TransferFleetFuelOrder{1, 2, 50, 0}}};
    const auto planned = projected_cargo_order_state(make_player_view(state, 1).state, fuel);
    assert(near(fleet(planned, 1).fuel, 150) && near(fleet(planned, 2).fuel, 60));
    const auto actual = TurnProcessor{}.process(state, {fuel});
    assert(near(fleet(actual, 1).fuel, 150) && near(fleet(actual, 2).fuel, 60));

    // Exchange also works away from a star and finances this year's flight.
    state = pair();
    for (auto& f : state.fleets) f.position = {10, 10};
    state.fleets[1].fuel = 0;
    const PlayerOrders depart{1, {TransferFleetFuelOrder{1, 2, 50, 0}, MoveFleetOrder{2, {35, 10}, 5}}};
    const auto departed = TurnProcessor{}.process(state, {depart});
    assert(same_position(fleet(departed, 2).position, {35, 10}));
    assert(near(fleet(departed, 1).fuel, 150));

    // A received detachment reference cannot be a guessed global allocation.
    // Hidden fleet births change the physical ID but never redirect commands.
    for (const FleetId allocation : {100U, 500U}) {
        state = make_demo_game(); state.orbitalStations.clear();
        state.fleets[0].ships = {{kScoutDesignId, 2}};
        state.fleets[0].fuel = 80; state.fleets[0].telemetry = {};
        auto hidden = state.fleets.front(); hidden.id = 2; hidden.owner = 2;
        hidden.position = {5000, 5000}; hidden.name = "Hidden foreign fleet";
        state.fleets.push_back(hidden); state.nextFleetId = allocation;
        const auto received = make_player_view(state, 1).state;
        const auto child = received.nextFleetId; assert(child == 2);
        const PlayerOrders orders{1, {SplitFleetOrder{1, {{kScoutDesignId, 1}}},
            RenameFleetOrder{child, "Detached scout"}, TransferFleetFuelOrder{1, child, 10, 0}}};
        const auto preview = projected_cargo_order_state(received, orders);
        assert(fleet_ship_count(fleet(preview, 1)) == 1 && fleet_ship_count(fleet(preview, child)) == 1);
        assert(fleet(preview, child).name == "Detached scout" && near(fleet(preview, child).fuel, 50));
        const auto next = TurnProcessor{}.process(state, {orders});
        assert(fleet(next, allocation).owner == 1 && fleet(next, allocation).name == "Detached scout");
        assert(near(fleet(next, 1).fuel, 30) && near(fleet(next, allocation).fuel, 50));
        assert(fleet(next, 2).owner == 2 && fleet(next, 2).name == "Hidden foreign fleet");

        // Nested split and a subsequent move use the same reference chain.
        state.fleets[0].ships = {{kScoutDesignId, 3}}; state.fleets[0].fuel = 300;
        PlayerOrders nested{1, {SplitFleetOrder{1, {{kScoutDesignId, 2}}},
            SplitFleetOrder{child, {{kScoutDesignId, 1}}}, RenameFleetOrder{child + 1, "Second child"},
            MoveFleetOrder{child + 1, {25, 0}, 5}}};
        const auto travelled = TurnProcessor{}.process(state, {nested});
        assert(fleet(travelled, allocation + 1).name == "Second child");
        assert(same_position(fleet(travelled, allocation + 1).position, {25, 0}));
    }

    // Each player's split references are scoped to their own submission,
    // regardless of which empire receives the first physical allocation.
    GalaxyConfig config; config.starCount = 16;
    auto multiplayer = generate_campaign(config, {{"First"}, {"Second"}});
    for (auto& f : multiplayer.fleets) { f.ships = {{f.design, 2}}; f.telemetry = {}; }
    multiplayer.nextFleetId = 100;
    const auto firstSource = multiplayer.fleets[0].id, secondSource = multiplayer.fleets[1].id;
    const auto firstChild = make_player_view(multiplayer, 1).state.nextFleetId;
    const auto secondChild = make_player_view(multiplayer, 2).state.nextFleetId;
    const PlayerOrders first{1, {SplitFleetOrder{firstSource, {{multiplayer.fleets[0].design, 1}}}, RenameFleetOrder{firstChild, "First child"}}};
    const PlayerOrders second{2, {SplitFleetOrder{secondSource, {{multiplayer.fleets[1].design, 1}}}, RenameFleetOrder{secondChild, "Second child"}}};
    for (const auto& submissions : {std::vector<PlayerOrders>{first, second}, std::vector<PlayerOrders>{second, first}}) {
        const auto next = TurnProcessor{}.process(multiplayer, submissions);
        const bool firstGoesFirst = submissions.front().player == 1;
        assert(fleet(next, 100).owner == (firstGoesFirst ? 1 : 2));
        assert(fleet(next, 100).name == (firstGoesFirst ? "First child" : "Second child"));
        assert(fleet(next, 101).name == (firstGoesFirst ? "Second child" : "First child"));
    }

    // Stale composition may predict splits that the actual hulls cannot do.
    // A failed split and its descendants must not create or rename anything.
    state = make_demo_game();
    state.fleets[0].telemetry.ships = {{kScoutDesignId, 3}};
    const auto failedChild = make_player_view(state, 1).state.nextFleetId;
    const PlayerOrders impossible{1, {SplitFleetOrder{1, {{kScoutDesignId, 2}}},
        SplitFleetOrder{failedChild, {{kScoutDesignId, 1}}}, RenameFleetOrder{failedChild + 1, "Must not exist"}}};
    assert(projected_cargo_order_state(make_player_view(state, 1).state, impossible).fleets.size() == 3);
    const auto failed = TurnProcessor{}.process(state, {impossible});
    assert(failed.fleets.size() == 1 && fleet(failed, 1).name == state.fleets[0].name);

    // An already sent command blocks reorganisation. A rejected split must
    // not consume a planning ID and redirect a later valid detachment's name.
    state = pair();
    for (auto& f : state.fleets) f.ships = {{kScoutDesignId,2}};
    state.fleets[0].position = {5000,0};
    state.fleets[0].telemetry.position = {5000,0};
    state.fleets[0].telemetry.observedTurn = state.turn;
    state.fleets[0].telemetry.ships = {{kScoutDesignId,2}};
    auto hidden = state.fleets[0]; hidden.id = 99; hidden.owner = 2; hidden.position = {10000,10000};
    state.fleets.push_back(hidden); state.nextFleetId = 100;
    state = TurnProcessor{}.process(state,{{1,{MoveFleetOrder{1,{5025,0},5}}}});
    assert(state.turn == 2 && fleet(state,1).pendingCommands.size() == 1);
    assert(fleet(state,1).pendingCommands.front().deliveryTurn == 35 && !fleet(state,1).destination);
    const auto clientChild = make_player_view(state,1).state.nextFleetId;
    assert(clientChild == 3);
    const PlayerOrders reorganise{1,{SplitFleetOrder{1,{{kScoutDesignId,1}}},
        SplitFleetOrder{2,{{kScoutDesignId,1}}},RenameFleetOrder{clientChild,"Actual detachment"}}};
    const auto reorganised = TurnProcessor{}.process(state,{reorganise});
    assert(fleet_ship_count(fleet(reorganised,1)) == 2 && fleet_ship_count(fleet(reorganised,2)) == 1);
    assert(fleet(reorganised,100).name == "Actual detachment");
    assert(fleet(reorganised,1).pendingCommands.size() == 1);

    // A tanker can follow an ordinary fleet and deliver fuel without merging.
    state = pair(); state.fleets[0].position = {25, 0}; state.fleets[1].fuel = 0;
    FleetArrivalAction supply; supply.kind = FleetArrivalActionKind::SupplyOutpost; supply.reserveFuel = 50;
    const PlayerOrders delivery{1, {MoveFleetOrder{1, state.fleets[1].position, 5, supply, {}, false, 2}}};
    const auto delivered = TurnProcessor{}.process(state, {delivery});
    assert(delivered.fleets.size() == 2 && same_position(fleet(delivered, 1).position, fleet(delivered, 2).position));
    assert(near(fleet(delivered, 1).fuel, 50) && fleet(delivered, 2).fuel > 0);
    assert(!fleet(delivered, 1).destination && !fleet(delivered, 1).targetFleet);
}
