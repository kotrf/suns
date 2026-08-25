#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess


def run(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing anchor: {label}")
    return text.replace(old, new, 1)


def rewrite(path: str, fn) -> None:
    p = Path(path)
    old = p.read_text()
    new = fn(old)
    if new == old:
        raise RuntimeError(f"no changes for {path}")
    p.write_text(new)


# Save writer: the first rewrite accidentally matched writeTelemetry's mineral
# tail instead of writeFleet's tail. Remove that block and append it explicitly
# inside writeFleet.
def fix_save(text: str) -> str:
    wrong = """    writeMinerals(stream, value.minerals);

    stream << static_cast<quint32>(value.pendingCommands.size());
    for (const auto& command : value.pendingCommands) writePendingCommand(stream, command);
    writeTelemetry(stream, value.telemetry);
    stream << static_cast<quint32>(value.telemetryInTransit.size());
    for (const auto& packet : value.telemetryInTransit) writePendingTelemetry(stream, packet);
}

void readTelemetry(QDataStream& stream, FleetTelemetry& value)
"""
    corrected = """    writeMinerals(stream, value.minerals);
}

void readTelemetry(QDataStream& stream, FleetTelemetry& value)
"""
    text = replace_once(text, wrong, corrected, "remove recursive telemetry serialization")

    start = text.index("void writeFleet(QDataStream& stream, const Fleet& value)")
    end = text.index("\nvoid readFleet(QDataStream& stream, Fleet& value)", start)
    block = text[start:end]
    tail = """    stream << static_cast<quint32>(value.waypointQueue.size());
    for (const auto& waypoint : value.waypointQueue) writeWaypoint(stream, waypoint);
    writeMinerals(stream, value.minerals);
}
"""
    new_tail = """    stream << static_cast<quint32>(value.waypointQueue.size());
    for (const auto& waypoint : value.waypointQueue) writeWaypoint(stream, waypoint);
    writeMinerals(stream, value.minerals);

    stream << static_cast<quint32>(value.pendingCommands.size());
    for (const auto& command : value.pendingCommands) writePendingCommand(stream, command);
    writeTelemetry(stream, value.telemetry);
    stream << static_cast<quint32>(value.telemetryInTransit.size());
    for (const auto& packet : value.telemetryInTransit) writePendingTelemetry(stream, packet);
}
"""
    block = replace_once(block, tail, new_tail, "writeFleet communication tail")
    return text[:start] + block + text[end:]

rewrite("src/app/save_game.cpp", fix_save)


# Route programs need an explicit delayed Stop semantic. The legacy local UI used
# destination=current-position as an implicit clear command, which becomes wrong
# once the ship can move for years before receiving it.
def fix_game_state(text: str) -> str:
    old = """struct FleetRouteProgram {
    Position destination;
    std::uint8_t warp{kScoutCruiseWarp};
    FleetArrivalAction arrivalAction{};
    std::vector<FleetWaypoint> queuedWaypoints;
};
"""
    new = """struct FleetRouteProgram {
    Position destination;
    std::uint8_t warp{kScoutCruiseWarp};
    FleetArrivalAction arrivalAction{};
    std::vector<FleetWaypoint> queuedWaypoints;
    bool clearRoute{};
};
"""
    return replace_once(text, old, new, "FleetRouteProgram clearRoute")

rewrite("src/core/include/suns/game_state.hpp", fix_game_state)


def fix_communications(text: str) -> str:
    # Invalid/hand-made remote states with no telemetry must fail closed rather
    # than revealing truth. Normal generated fleets receive an initial snapshot.
    old = """FleetTelemetry confirmed_fleet_telemetry(const GameState& state, const Fleet& fleet)
{
    if (fleet.telemetry.observedTurn != 0) return fleet.telemetry;
    return authoritative_snapshot(state, fleet, state.turn);
}
"""
    new = """FleetTelemetry confirmed_fleet_telemetry(const GameState& state, const Fleet& fleet)
{
    if (fleet.telemetry.observedTurn != 0) return fleet.telemetry;
    if (fleet_has_instant_link(state, fleet)) return authoritative_snapshot(state, fleet, state.turn);
    return {};
}
"""
    text = replace_once(text, old, new, "fail-closed telemetry fallback")

    # Player-view copies must never carry hidden packet queues or exact physical
    # delivery schedules into UI/forecast code by accident.
    old = """    view.arrivalAction = telemetry.arrivalAction;
    view.waypointQueue = telemetry.waypointQueue;
    view.minerals = telemetry.minerals;
    return view;
}
"""
    new = """    view.arrivalAction = telemetry.arrivalAction;
    view.waypointQueue = telemetry.waypointQueue;
    view.minerals = telemetry.minerals;
    view.pendingCommands.clear();
    view.telemetry = telemetry;
    view.telemetryInTransit.clear();
    return view;
}
"""
    text = replace_once(text, old, new, "strip hidden packet state from player view")

    # Detect the UI's established clear-route intent against the position known
    # to the player at transmission time, then carry that intent explicitly.
    old = """    FleetRouteProgram program{destination, warp, arrivalAction, queuedWaypoints};
    const auto requestedWarp = warp == 0 ? fleet->warp : warp;
"""
    new = """    FleetRouteProgram program{destination, warp, arrivalAction, queuedWaypoints};
    const auto visiblePosition = projected_fleet_position(state, *fleet);
    program.clearRoute = same_position(destination, visiblePosition)
        && arrivalAction.kind == FleetArrivalActionKind::None
        && queuedWaypoints.empty();
    const auto requestedWarp = warp == 0 ? fleet->warp : warp;
"""
    text = replace_once(text, old, new, "clear-route intent detection")

    old = """    fleet.warp = requestedWarp;
    fleet.arrivalAction = active_arrival_action(program.arrivalAction);
    fleet.waypointQueue = program.queuedWaypoints;

    if (same_position(fleet.position, program.destination)) {
"""
    new = """    fleet.warp = requestedWarp;
    if (program.clearRoute) {
        fleet.destination.reset();
        fleet.arrivalAction.reset();
        fleet.waypointQueue.clear();
        return true;
    }

    fleet.arrivalAction = active_arrival_action(program.arrivalAction);
    fleet.waypointQueue = program.queuedWaypoints;

    if (same_position(fleet.position, program.destination)) {
"""
    text = replace_once(text, old, new, "apply clear route")
    return text

rewrite("src/core/src/communications.cpp", fix_communications)


# Generated/demo games begin with a confirmed Turn-1 snapshot. This removes the
# only legitimate runtime path where an outbound fleet could leave relay range
# before any persistent telemetry existed.
def fix_game_state_cpp(text: str) -> str:
    anchor = """GameState generate_game(const GalaxyConfig& config)
{
"""
    helper = """namespace {

void initialize_initial_fleet_telemetry(Fleet& fleet, std::uint64_t turn)
{
    fleet.telemetry.observedTurn = turn;
    fleet.telemetry.position = fleet.position;
    fleet.telemetry.destination = fleet.destination;
    fleet.telemetry.warp = fleet.warp;
    fleet.telemetry.fuel = fleet.fuel;
    fleet.telemetry.colonists = fleet.colonists;
    fleet.telemetry.arrivalAction = fleet.arrivalAction;
    fleet.telemetry.waypointQueue = fleet.waypointQueue;
    fleet.telemetry.minerals = fleet.minerals;
}

} // namespace

GameState generate_game(const GalaxyConfig& config)
{
"""
    text = replace_once(text, anchor, helper, "initial telemetry helper")
    old = """    state.nextFleetId = 2;
    refresh_sensor_intel(state);
    return state;
"""
    new = """    initialize_initial_fleet_telemetry(state.fleets.back(), state.turn);
    state.nextFleetId = 2;
    refresh_sensor_intel(state);
    return state;
"""
    if text.count(old) != 2:
        raise RuntimeError(f"expected two initial fleet tails, got {text.count(old)}")
    return text.replace(old, new)

rewrite("src/core/src/game_state.cpp", fix_game_state_cpp)


# Save clearRoute as part of the physical command packet.
def fix_save_route(text: str) -> str:
    old = """    stream << static_cast<quint32>(value.queuedWaypoints.size());
    for (const auto& waypoint : value.queuedWaypoints) writeWaypoint(stream, waypoint);
}

void readRouteProgram(QDataStream& stream, FleetRouteProgram& value)
"""
    new = """    stream << static_cast<quint32>(value.queuedWaypoints.size());
    for (const auto& waypoint : value.queuedWaypoints) writeWaypoint(stream, waypoint);
    stream << static_cast<quint8>(value.clearRoute ? 1 : 0);
}

void readRouteProgram(QDataStream& stream, FleetRouteProgram& value)
"""
    text = replace_once(text, old, new, "write clearRoute")

    old = """        value.queuedWaypoints.push_back(waypoint);
    }
}

void writeTelemetry(QDataStream& stream, const FleetTelemetry& value)
"""
    new = """        value.queuedWaypoints.push_back(waypoint);
    }
    quint8 clearRoute{};
    stream >> clearRoute;
    if (clearRoute > 1) {
        markCorrupt(stream);
        return;
    }
    value.clearRoute = clearRoute != 0;
}

void writeTelemetry(QDataStream& stream, const FleetTelemetry& value)
"""
    text = replace_once(text, old, new, "read clearRoute")
    return text

rewrite("src/app/save_game.cpp", fix_save_route)


# Communications panel must not reveal the authoritative delivery turn. The user
# knows a command was transmitted and can see a latency estimate from known data,
# but reception is not confirmed until telemetry says so.
def fix_comm_ui(text: str) -> str:
    old = """        if (!fleet->pendingCommands.empty()) {
            const auto nextCommand = std::min_element(
                fleet->pendingCommands.begin(), fleet->pendingCommands.end(),
                [](const PendingFleetCommand& lhs, const PendingFleetCommand& rhs) {
                    return lhs.deliveryTurn < rhs.deliveryTurn;
                });
            text += QString("<br><b>Command in transit:</b> issued turn %1, delivery turn %2")
                        .arg(static_cast<qulonglong>(nextCommand->issuedTurn))
                        .arg(static_cast<qulonglong>(nextCommand->deliveryTurn));
            if (fleet->pendingCommands.size() > 1) {
                text += QString(" (+%1 queued behind it)")
                            .arg(static_cast<qulonglong>(fleet->pendingCommands.size() - 1));
            }
        }
"""
    new = """        if (!fleet->pendingCommands.empty()) {
            const auto oldest = std::min_element(
                fleet->pendingCommands.begin(), fleet->pendingCommands.end(),
                [](const PendingFleetCommand& lhs, const PendingFleetCommand& rhs) {
                    return lhs.issuedTurn < rhs.issuedTurn;
                });
            text += QString("<br><b>Command in transit:</b> transmitted turn %1 — reception not yet confirmed")
                        .arg(static_cast<qulonglong>(oldest->issuedTurn));
            if (fleet->pendingCommands.size() > 1) {
                text += QString(" (+%1 later transmission%2)")
                            .arg(static_cast<qulonglong>(fleet->pendingCommands.size() - 1))
                            .arg(fleet->pendingCommands.size() == 2 ? "" : "s");
            }
        }
"""
    return replace_once(text, old, new, "hide actual command delivery turn")

rewrite("src/app/main_window_communications.cpp", fix_comm_ui)


# Route forecasting begins from the player's estimated fleet state rather than
# authoritative coordinates. This makes the preview a genuine knowledge-based
# forecast once telemetry is stale.
def fix_route_program(text: str) -> str:
    old = """const Fleet* findFleet(const GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    return it == state.fleets.end() ? nullptr : &*it;
}
"""
    new = """Fleet* findFleet(GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    return it == state.fleets.end() ? nullptr : &*it;
}

const Fleet* findFleet(const GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    return it == state.fleets.end() ? nullptr : &*it;
}
"""
    text = replace_once(text, old, new, "mutable route forecast fleet lookup")

    old = """    GameState simulated = state;
    QStringList lines;
"""
    new = """    GameState simulated = state;
    if (auto* simulatedFleet = findFleet(simulated, fleetId)) {
        *simulatedFleet = fleet_player_view(state, *simulatedFleet);
    }
    QStringList lines;
"""
    text = replace_once(text, old, new, "forecast from player knowledge")
    text = text.replace(
        "<i>Pending program becomes authoritative on End Turn.</i>",
        "<i>Pending program is transmitted on End Turn; a remote fleet keeps its known onboard program until the command arrives.</i>")
    return text

rewrite("src/app/main_window_route_program.cpp", fix_route_program)


# Extend core tests with the delayed Stop case that motivated clearRoute.
def fix_comm_tests(text: str) -> str:
    insert_before = """void stale_telemetry_predicts_without_revealing_route_change()
{
"""
    test = r'''void remote_clear_route_stops_when_command_arrives()
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

'''
    text = replace_once(text, insert_before, test + insert_before, "remote clear test")
    text = replace_once(
        text,
        "    remote_command_arrives_after_signal_delay();\n    stale_telemetry_predicts_without_revealing_route_change();\n",
        "    remote_command_arrives_after_signal_delay();\n    remote_clear_route_stops_when_command_arrives();\n    stale_telemetry_predicts_without_revealing_route_change();\n",
        "invoke remote clear test")
    return text

rewrite("tests/communications_tests.cpp", fix_comm_tests)


# Persist and verify the explicit clearRoute bit as part of the v3 round trip.
def fix_save_tests(text: str) -> str:
    text = replace_once(
        text,
        "    scout.pendingCommands.push_back({\n        77,\n        80,\n        {{0.0, 0.0}, 7, {FleetArrivalActionKind::Refuel, 1}, {{{90.0, 30.0}, 6, {}}}},\n    });\n",
        "    scout.pendingCommands.push_back({\n        77,\n        80,\n        {{0.0, 0.0}, 7, {FleetArrivalActionKind::Refuel, 1}, {{{90.0, 30.0}, 6, {}}}, true},\n    });\n",
        "save test clearRoute setup")
    text = replace_once(
        text,
        "    assert(fleet.pendingCommands.front().program.queuedWaypoints.size() == 1);\n",
        "    assert(fleet.pendingCommands.front().program.queuedWaypoints.size() == 1);\n    assert(fleet.pendingCommands.front().program.clearRoute);\n",
        "save test clearRoute assertion")
    return text

rewrite("tests/save_game_tests.cpp", fix_save_tests)


# Remove one-shot helper files from the resulting feature commit.
Path("scripts/fix_communications_once.py").unlink(missing_ok=True)
Path(".github/workflows/fix-communications-once.yml").unlink(missing_ok=True)

subprocess.run(["git", "config", "user.name", "Suns CI Rewriter"], check=True)
subprocess.run(["git", "config", "user.email", "actions@users.noreply.github.com"], check=True)
subprocess.run(["git", "add", "-A"], check=True)
subprocess.run(["git", "commit", "-m", "Harden delayed communications semantics [skip ci]"], check=True)
subprocess.run(["git", "push", "origin", "HEAD:feature/delayed-communications"], check=True)
print(run("git", "rev-parse", "HEAD"))
