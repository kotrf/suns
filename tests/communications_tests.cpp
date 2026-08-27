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

void homeworld_scanner_defines_the_initial_network_field()
{
    auto state = generate_game(GalaxyConfig{});
    const auto& home = state.stars.front();
    state.fleets.clear();

    assert(communication_delay_turns(state, 1, home.position) == 0);
    assert(communication_delay_turns(state, 1, {home.position.x + kColonySensorRange, home.position.y}) == 0);
    assert(communication_delay_turns(state, 1, {home.position.x + kColonySensorRange + 0.001, home.position.y}) == 1);
    assert(communication_delay_turns(
        state, 1, {home.position.x + kColonySensorRange + kCommunicationSignalSpeed, home.position.y}) == 1);
    assert(communication_delay_turns(
        state, 1, {home.position.x + kColonySensorRange + kCommunicationSignalSpeed + 0.001, home.position.y}) == 2);
}

void ordinary_scanners_automatically_form_a_relay_chain()
{
    auto state = generate_game(GalaxyConfig{});
    auto& firstRelay = state.fleets.front();
    firstRelay.position = {140.0, 0.0};

    auto secondRelay = firstRelay;
    secondRelay.id = 99;
    secondRelay.position = {300.0, 0.0};
    state.fleets.push_back(secondRelay);

    // Homeworld R60 overlaps Scout A R90; the two scout fields then overlap.
    assert(communication_delay_turns(state, 1, {380.0, 0.0}) == 0);

    // Breaking the first overlap detaches the entire mobile branch.
    state.fleets.front().position = {151.0, 0.0};
    assert(communication_delay_turns(state, 1, {380.0, 0.0}) == 3);
}

void penetrating_only_scanner_does_not_extend_the_network()
{
    auto state = generate_game(GalaxyConfig{});
    state.shipDesigns.push_back({
        99,
        1,
        "Penetrating-only",
        ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::PenetratingScanner},
    });
    state.fleets.front().design = 99;
    state.fleets.front().position = {120.0, 0.0};

    assert(communication_delay_turns(state, 1, {180.0, 0.0}) == 1);
}

void nearest_colony_scanner_field_is_used()
{
    auto state = generate_game(GalaxyConfig{});

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

    assert(communication_delay_turns(state, 1, fleet.position) == 3);

    MoveFleetOrder returnHome;
    returnHome.fleet = fleet.id;
    returnHome.destination = {0.0, 0.0};
    returnHome.warp = 8;

    TurnProcessor processor;
    auto turn11 = processor.process(state, {{1, {returnHome}}});
    assert(turn11.turn == 11);
    assert(turn11.fleets.front().pendingCommands.size() == 1);
    assert(turn11.fleets.front().pendingCommands.front().deliveryTurn == 13);
    assert(turn11.fleets.front().destination.has_value());
    assert(same_position(*turn11.fleets.front().destination, Position{600.0, 0.0}));
    assert(turn11.fleets.front().position.x > 420.0);

    auto turn12 = processor.process(turn11, {});
    assert(turn12.fleets.front().pendingCommands.size() == 1);

    auto turn13 = processor.process(turn12, {});
    assert(turn13.turn == 13);
    assert(turn13.fleets.front().pendingCommands.empty());
    assert(turn13.fleets.front().destination.has_value());
    assert(same_position(*turn13.fleets.front().destination, Position{0.0, 0.0}));

    const auto positionAtDelivery = turn13.fleets.front().position.x;
    auto turn14 = processor.process(turn13, {});
    assert(turn14.fleets.front().position.x < positionAtDelivery);
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
    assert(turn12.fleets.front().pendingCommands.size() == 1);

    auto turn13 = processor.process(turn12, {});
    assert(turn13.fleets.front().pendingCommands.empty());
    assert(!turn13.fleets.front().destination.has_value());
    const auto stoppedAt = turn13.fleets.front().position;

    auto turn14 = processor.process(turn13, {});
    assert(same_position(turn14.fleets.front().position, stoppedAt));
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
    next = processor.process(next, {});
    next = processor.process(next, {}); // command arrives at turn 13
    next = processor.process(next, {}); // actual fleet now starts home

    const auto& actual = next.fleets.front();
    assert(next.turn == 14);
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
    homeworld_scanner_defines_the_initial_network_field();
    ordinary_scanners_automatically_form_a_relay_chain();
    penetrating_only_scanner_does_not_extend_the_network();
    nearest_colony_scanner_field_is_used();
    remote_command_arrives_after_signal_delay();
    remote_clear_route_stops_when_command_arrives();
    stale_telemetry_predicts_without_revealing_route_change();
    std::cout << "communications tests passed\n";
    return 0;
}
