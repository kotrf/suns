#include "suns/communications.hpp"
#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace suns;

Fleet& scout(GameState& state)
{
    assert(!state.fleets.empty());
    return state.fleets.front();
}

void local_commands_remain_immediate()
{
    auto state = generate_game(GalaxyConfig{});
    auto& fleet = scout(state);
    assert(communication_delay_turns(state, 1, fleet.position) == 0);

    MoveFleetOrder move;
    move.fleet = fleet.id;
    move.destination = {64.0, 0.0};
    move.warp = 8;

    TurnProcessor processor;
    const auto next = processor.process(state, {{1, {move}}});
    const auto& moved = next.fleets.front();

    assert(same_position(moved.position, move.destination));
    assert(moved.pendingCommands.empty());
}

void delay_is_propagation_to_the_nearest_relay_node()
{
    auto state = generate_game(GalaxyConfig{});
    const auto& home = state.stars.front();

    assert(communication_delay_turns(state, 1, home.position) == 0);
    assert(communication_delay_turns(state, 1, {home.position.x + 1.0, home.position.y}) == 0);
    assert(communication_delay_turns(state, 1, {home.position.x + 149.9999, home.position.y}) == 0);
    assert(communication_delay_turns(state, 1, {home.position.x - 150.0, home.position.y}) == 1);
    assert(communication_delay_turns(state, 1, {home.position.x, home.position.y + 299.9999}) == 1);

    auto relayPlanet = state.planets.front();
    relayPlanet.id = 999;
    relayPlanet.star = state.stars[1].id;
    relayPlanet.owner = 1;
    relayPlanet.population = 1000;
    state.planets.push_back(relayPlanet);

    const auto& relay = state.stars[1];
    const Position nearRelay{relay.position.x + 10.0, relay.position.y};
    assert(communication_delay_turns(state, 1, nearRelay) == 0);
}

void remote_command_arrives_after_signal_delay()
{
    auto state = generate_game(GalaxyConfig{});
    state.turn = 10;
    auto& fleet = scout(state);
    fleet.position = {420.0, 0.0};
    fleet.destination = Position{600.0, 0.0};
    fleet.warp = 8;
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    fleet.telemetry = FleetTelemetry{
        10,
        fleet.position,
        fleet.destination,
        fleet.warp,
        fleet.fuel,
        0,
        std::nullopt,
        {},
        {},
    };

    assert(communication_delay_turns(state, 1, fleet.position) == 2);

    MoveFleetOrder returnHome;
    returnHome.fleet = fleet.id;
    returnHome.destination = {0.0, 0.0};
    returnHome.warp = 8;

    TurnProcessor processor;
    auto turn11 = processor.process(state, {{1, {returnHome}}});
    assert(turn11.turn == 11);
    assert(turn11.fleets.front().pendingCommands.size() == 1);
    assert(turn11.fleets.front().pendingCommands.front().deliveryTurn == 12);
    assert(turn11.fleets.front().destination.has_value());
    assert(same_position(*turn11.fleets.front().destination, Position{600.0, 0.0}));
    assert(turn11.fleets.front().position.x > 420.0);

    auto turn12 = processor.process(turn11, {});
    assert(turn12.turn == 12);
    assert(turn12.fleets.front().pendingCommands.empty());
    assert(turn12.fleets.front().destination.has_value());
    assert(same_position(*turn12.fleets.front().destination, Position{0.0, 0.0}));

    const auto positionAtDelivery = turn12.fleets.front().position.x;
    auto turn13 = processor.process(turn12, {});
    assert(turn13.fleets.front().position.x < positionAtDelivery);
}

void remote_clear_route_stops_when_command_arrives()
{
    auto state = generate_game(GalaxyConfig{});
    state.turn = 10;
    auto& fleet = scout(state);
    fleet.position = {420.0, 0.0};
    fleet.destination = Position{700.0, 0.0};
    fleet.warp = 8;
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    fleet.telemetry = {10, fleet.position, fleet.destination, fleet.warp, fleet.fuel, 0, std::nullopt, {}, {}};

    const auto visibleAtTransmission = projected_fleet_position(state, fleet);
    MoveFleetOrder stop;
    stop.fleet = fleet.id;
    stop.destination = visibleAtTransmission;
    stop.warp = fleet.warp;

    TurnProcessor processor;
    auto turn11 = processor.process(state, {{1, {stop}}});
    assert(turn11.fleets.front().pendingCommands.size() == 1);
    assert(turn11.fleets.front().pendingCommands.front().program.clearRoute);
    assert(turn11.fleets.front().destination.has_value());

    auto turn12 = processor.process(turn11, {});
    assert(turn12.fleets.front().pendingCommands.empty());
    assert(!turn12.fleets.front().destination.has_value());
    const auto stoppedAt = turn12.fleets.front().position;

    auto turn13 = processor.process(turn12, {});
    assert(same_position(turn13.fleets.front().position, stoppedAt));
}

void stale_telemetry_predicts_without_revealing_route_change()
{
    auto state = generate_game(GalaxyConfig{});
    state.turn = 10;
    auto& fleet = scout(state);
    fleet.position = {420.0, 0.0};
    fleet.destination = Position{600.0, 0.0};
    fleet.warp = 8;
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    fleet.telemetry = FleetTelemetry{
        10,
        fleet.position,
        fleet.destination,
        fleet.warp,
        fleet.fuel,
        0,
        std::nullopt,
        {},
        {},
    };

    MoveFleetOrder returnHome;
    returnHome.fleet = fleet.id;
    returnHome.destination = {0.0, 0.0};
    returnHome.warp = 8;

    TurnProcessor processor;
    auto next = processor.process(state, {{1, {returnHome}}});
    next = processor.process(next, {}); // command arrives at turn 12
    next = processor.process(next, {}); // actual fleet now starts home

    const auto& actual = next.fleets.front();
    assert(next.turn == 13);
    assert(actual.destination.has_value());
    assert(same_position(*actual.destination, Position{0.0, 0.0}));
    assert(fleet_telemetry_age(next, actual) >= 1);

    const auto playerView = fleet_player_view(next, actual);
    assert(playerView.destination.has_value());
    assert(same_position(*playerView.destination, Position{600.0, 0.0}));
    assert(!same_position(playerView.position, actual.position));
}

} // namespace

int main()
{
    local_commands_remain_immediate();
    delay_is_propagation_to_the_nearest_relay_node();
    remote_command_arrives_after_signal_delay();
    remote_clear_route_stops_when_command_arrives();
    stale_telemetry_predicts_without_revealing_route_change();
    std::cout << "communications tests passed\n";
    return 0;
}
