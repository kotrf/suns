#include "suns/communications.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace suns {

namespace {

std::optional<FleetArrivalAction> active_arrival_action(FleetArrivalAction action)
{
    return action.kind == FleetArrivalActionKind::None
        ? std::optional<FleetArrivalAction>{}
        : std::optional<FleetArrivalAction>{action};
}

void activate_next_waypoint(GameState& state, Fleet& fleet)
{
    while (!fleet.destination && !fleet.waypointQueue.empty()) {
        const auto waypoint = fleet.waypointQueue.front();
        fleet.waypointQueue.erase(fleet.waypointQueue.begin());

        if (!fleet_warp_valid(state, fleet, waypoint.warp)) {
            fleet.waypointQueue.clear();
            fleet.arrivalAction.reset();
            return;
        }

        fleet.warp = waypoint.warp;
        fleet.arrivalAction = active_arrival_action(waypoint.arrivalAction);

        if (!same_position(fleet.position, waypoint.destination)) {
            fleet.destination = waypoint.destination;
            return;
        }

        if (fleet.arrivalAction) {
            fleet.destination = waypoint.destination;
            return;
        }
    }
}

bool apply_route_program(GameState& state, Fleet& fleet, const FleetRouteProgram& program)
{
    const auto requestedWarp = program.warp == 0 ? fleet.warp : program.warp;
    if (!fleet_warp_valid(state, fleet, requestedWarp)) return false;
    if (std::any_of(program.queuedWaypoints.begin(), program.queuedWaypoints.end(),
            [&](const FleetWaypoint& waypoint) { return !fleet_warp_valid(state, fleet, waypoint.warp); })) {
        return false;
    }

    fleet.warp = requestedWarp;
    if (program.clearRoute) {
        fleet.destination.reset();
        fleet.arrivalAction.reset();
        fleet.waypointQueue.clear();
        return true;
    }

    fleet.arrivalAction = active_arrival_action(program.arrivalAction);
    fleet.waypointQueue = program.queuedWaypoints;

    if (same_position(fleet.position, program.destination)) {
        if (fleet.arrivalAction) {
            fleet.destination = program.destination;
        } else {
            fleet.destination.reset();
            activate_next_waypoint(state, fleet);
        }
    } else {
        fleet.destination = program.destination;
    }
    return true;
}

FleetTelemetry authoritative_snapshot(const GameState& state, const Fleet& fleet, std::uint64_t observedTurn)
{
    FleetTelemetry result;
    result.observedTurn = observedTurn;
    result.position = fleet.position;
    result.destination = fleet.destination;
    result.warp = fleet.warp;
    result.fuel = fleet.fuel;
    result.colonists = fleet.colonists;
    result.arrivalAction = fleet.arrivalAction;
    result.waypointQueue = fleet.waypointQueue;
    result.minerals = fleet.minerals;
    return result;
}

void activate_next_predicted_waypoint(
    Position position,
    std::optional<Position>& destination,
    std::uint8_t& warp,
    std::vector<FleetWaypoint>& queue)
{
    while (!destination && !queue.empty()) {
        const auto waypoint = queue.front();
        queue.erase(queue.begin());
        warp = waypoint.warp;
        if (!same_position(position, waypoint.destination)) {
            destination = waypoint.destination;
            return;
        }
    }
}

Position project_from_telemetry(const FleetTelemetry& telemetry, std::uint64_t age)
{
    constexpr double epsilon = 0.000001;
    Position position = telemetry.position;
    auto destination = telemetry.destination;
    auto warp = telemetry.warp;
    auto queue = telemetry.waypointQueue;

    for (std::uint64_t turn = 0; turn < age; ++turn) {
        if (!destination) {
            activate_next_predicted_waypoint(position, destination, warp, queue);
            if (!destination) break;
        }

        const auto remaining = distance_between(position, *destination);
        const auto maximumDistance = warp_distance(warp);
        if (remaining <= epsilon || maximumDistance <= epsilon) {
            position = *destination;
            destination.reset();
            activate_next_predicted_waypoint(position, destination, warp, queue);
            continue;
        }

        if (maximumDistance >= remaining - epsilon) {
            position = *destination;
            destination.reset();
            activate_next_predicted_waypoint(position, destination, warp, queue);
        } else {
            const auto fraction = maximumDistance / remaining;
            position.x += (destination->x - position.x) * fraction;
            position.y += (destination->y - position.y) * fraction;
        }
    }

    return position;
}

} // namespace

std::uint32_t communication_delay_turns(const GameState& state, PlayerId player, Position position)
{
    double nearest = std::numeric_limits<double>::infinity();
    bool hasRelay = false;

    for (const auto& planet : state.planets) {
        if (planet.owner != player || planet.population == 0) continue;
        const auto* star = find_star(state, planet.star);
        if (!star) continue;
        hasRelay = true;
        nearest = std::min(nearest, distance_between(position, star->position));
    }

    // Compatibility for tiny test fixtures and edge states that have no colony
    // node yet. Normal generated games always begin with the homeworld relay.
    if (!hasRelay) return 0;
    if (nearest <= kCommunicationRelayRange + 0.000001) return 0;

    const auto uncovered = nearest - kCommunicationRelayRange;
    return static_cast<std::uint32_t>(std::ceil(uncovered / kCommunicationSignalSpeed));
}

bool fleet_has_instant_link(const GameState& state, const Fleet& fleet)
{
    return communication_delay_turns(state, fleet.owner, fleet.position) == 0;
}

FleetTelemetry confirmed_fleet_telemetry(const GameState& state, const Fleet& fleet)
{
    if (fleet.telemetry.observedTurn != 0) return fleet.telemetry;
    if (fleet_has_instant_link(state, fleet)) return authoritative_snapshot(state, fleet, state.turn);
    return {};
}

std::uint64_t fleet_telemetry_age(const GameState& state, const Fleet& fleet)
{
    const auto telemetry = confirmed_fleet_telemetry(state, fleet);
    return state.turn > telemetry.observedTurn ? state.turn - telemetry.observedTurn : 0;
}

Position projected_fleet_position(const GameState& state, const Fleet& fleet)
{
    const auto telemetry = confirmed_fleet_telemetry(state, fleet);
    return project_from_telemetry(telemetry, fleet_telemetry_age(state, fleet));
}

Fleet fleet_player_view(const GameState& state, const Fleet& fleet)
{
    const auto telemetry = confirmed_fleet_telemetry(state, fleet);
    Fleet view = fleet;
    view.position = project_from_telemetry(telemetry, fleet_telemetry_age(state, fleet));
    view.destination = telemetry.destination;
    view.warp = telemetry.warp;
    view.fuel = telemetry.fuel;
    view.colonists = telemetry.colonists;
    view.arrivalAction = telemetry.arrivalAction;
    view.waypointQueue = telemetry.waypointQueue;
    view.minerals = telemetry.minerals;
    view.pendingCommands.clear();
    view.telemetry = telemetry;
    view.telemetryInTransit.clear();
    return view;
}

bool submit_fleet_route_command(
    GameState& state,
    PlayerId player,
    FleetId fleetId,
    Position destination,
    std::uint8_t warp,
    FleetArrivalAction arrivalAction,
    const std::vector<FleetWaypoint>& queuedWaypoints)
{
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == fleetId && candidate.owner == player;
    });
    if (fleet == state.fleets.end()) return false;

    FleetRouteProgram program{destination, warp, arrivalAction, queuedWaypoints};
    const auto visiblePosition = projected_fleet_position(state, *fleet);
    program.clearRoute = same_position(destination, visiblePosition)
        && arrivalAction.kind == FleetArrivalActionKind::None
        && queuedWaypoints.empty();
    const auto requestedWarp = warp == 0 ? fleet->warp : warp;
    if (!fleet_warp_valid(state, *fleet, requestedWarp)) return false;
    if (std::any_of(queuedWaypoints.begin(), queuedWaypoints.end(),
            [&](const FleetWaypoint& waypoint) { return !fleet_warp_valid(state, *fleet, waypoint.warp); })) {
        return false;
    }

    const auto delay = communication_delay_turns(state, player, fleet->position);
    if (delay == 0) return apply_route_program(state, *fleet, program);

    fleet->pendingCommands.push_back(PendingFleetCommand{
        state.turn,
        state.turn + delay,
        std::move(program),
    });
    return true;
}

void deliver_due_fleet_commands(GameState& state)
{
    for (auto& fleet : state.fleets) {
        std::stable_sort(fleet.pendingCommands.begin(), fleet.pendingCommands.end(),
            [](const PendingFleetCommand& lhs, const PendingFleetCommand& rhs) {
                if (lhs.deliveryTurn != rhs.deliveryTurn) return lhs.deliveryTurn < rhs.deliveryTurn;
                return lhs.issuedTurn < rhs.issuedTurn;
            });

        for (const auto& pending : fleet.pendingCommands) {
            if (pending.deliveryTurn > state.turn) break;
            apply_route_program(state, fleet, pending.program);
        }
        std::erase_if(fleet.pendingCommands, [&](const PendingFleetCommand& pending) {
            return pending.deliveryTurn <= state.turn;
        });
    }
}

void deliver_due_fleet_telemetry(GameState& state)
{
    for (auto& fleet : state.fleets) {
        for (const auto& packet : fleet.telemetryInTransit) {
            if (packet.deliveryTurn > state.turn) continue;
            if (packet.telemetry.observedTurn >= fleet.telemetry.observedTurn) {
                fleet.telemetry = packet.telemetry;
            }
        }
        std::erase_if(fleet.telemetryInTransit, [&](const PendingFleetTelemetry& packet) {
            return packet.deliveryTurn <= state.turn;
        });
    }
}

void publish_fleet_telemetry(GameState& state, std::uint64_t observationTurn)
{
    for (auto& fleet : state.fleets) {
        const auto snapshot = authoritative_snapshot(state, fleet, observationTurn);
        const auto delay = communication_delay_turns(state, fleet.owner, fleet.position);
        if (delay == 0) {
            if (snapshot.observedTurn >= fleet.telemetry.observedTurn) fleet.telemetry = snapshot;
            std::erase_if(fleet.telemetryInTransit, [&](const PendingFleetTelemetry& packet) {
                return packet.telemetry.observedTurn <= snapshot.observedTurn;
            });
        } else {
            fleet.telemetryInTransit.push_back(PendingFleetTelemetry{
                observationTurn + delay,
                snapshot,
            });
        }
    }
}

} // namespace suns
