#include "suns/communications.hpp"
#include "suns/wormholes.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/production.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <type_traits>
#include <tuple>

namespace suns {

namespace {

struct CommunicationField {
    Position position;
    double radius{};
    bool connected{};
};

bool has_friendly_colony(const GameState& state, PlayerId owner, Position position)
{
    return std::any_of(state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
        if (planet.owner != owner || planet.population == 0) return false;
        const auto* star = find_star(state, planet.star);
        return star && same_position(star->position, position);
    });
}

bool fleet_can_share_reports(const GameState& state, const Fleet& fleet, std::uint64_t turn)
{
    // Local ship/surface exchange does not enable the fleet's long-range
    // transmitter, active sensors or relay field. The colony forwards the data.
    return fleet_transmits(fleet, turn) || has_friendly_colony(state, fleet.owner, fleet.position);
}

const Fleet* report_source(const GameState& state, PlayerId owner, Position position,
    std::uint64_t turn, FleetId observer)
{
    if (has_friendly_colony(state, owner, position)) return nullptr;
    if(observer) for(const auto& f:state.fleets) if(f.id==observer && f.owner==owner) return &f;
    const Fleet* best=nullptr;
    for(const auto& f:state.fleets) if(f.owner==owner && same_position(f.position,position))
        if(!best || std::pair{fleet_available_turn(f,turn),f.id}<std::pair{fleet_available_turn(*best,turn),best->id}) best=&f;
    return best;
}

std::vector<CommunicationField> communication_fields(
    const GameState& state,
    PlayerId player, std::uint64_t atTurn)
{
    std::vector<CommunicationField> fields;

    // A built radar serves both scanning and communications. Only a homeworld
    // receiver roots the capital mesh: outposts form separate radio islands
    // until their radar or allied fleet scanner range overlaps that mesh.
    for (const auto& planet : state.planets) {
        if (planet.owner != player || planet.population == 0 || planet.radar == PlanetaryRadarLevel::None) continue;
        const auto* star = find_star(state, planet.star);
        if (!star) continue;
        fields.push_back({star->position, planetary_radar_range(planet.radar), planet.homeworld});
    }

    // Ordinary scanners and dedicated relay arrays participate in the mesh.
    // Penetrating-only scanners do not extend communications.
    for (const auto& fleet : state.fleets) {
        if (fleet.owner != player || !fleet_transmits(fleet, atTurn)) continue;
        const auto range = fleet_communication_range(state, fleet);
        if (range <= 0.0) continue;
        fields.push_back({fleet.position, range, false});
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& candidate : fields) {
            if (candidate.connected) continue;
            const auto overlapsConnected = std::any_of(
                fields.begin(), fields.end(), [&](const CommunicationField& connected) {
                    return connected.connected
                        && distance_between(candidate.position, connected.position)
                            <= candidate.radius + connected.radius + 0.000001;
                });
            if (!overlapsConnected) continue;
            candidate.connected = true;
            changed = true;
        }
    }

    return fields;
}

bool inside_field(Position position, const CommunicationField& field)
{
    return distance_between(position, field.position) <= field.radius + 0.000001;
}

std::vector<bool> field_component_containing(
    const std::vector<CommunicationField>& fields,
    Position position)
{
    std::vector<bool> component(fields.size(), false);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        component[index] = inside_field(position, fields[index]);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t candidate = 0; candidate < fields.size(); ++candidate) {
            if (component[candidate]) continue;
            for (std::size_t member = 0; member < fields.size(); ++member) {
                if (!component[member]) continue;
                if (distance_between(fields[candidate].position, fields[member].position)
                    > fields[candidate].radius + fields[member].radius + 0.000001) {
                    continue;
                }
                component[candidate] = true;
                changed = true;
                break;
            }
        }
    }
    return component;
}

bool inside_component(
    Position position,
    const std::vector<CommunicationField>& fields,
    const std::vector<bool>& component)
{
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (component[index] && inside_field(position, fields[index])) return true;
    }
    return false;
}

std::optional<FleetArrivalAction> active_arrival_action(FleetArrivalAction action)
{
    return action.kind == FleetArrivalActionKind::None
        ? std::optional<FleetArrivalAction>{}
        : std::optional<FleetArrivalAction>{action};
}

bool route_tasks_valid(const FleetRouteProgram& program)
{
    const auto targetActionValid = [](FleetId target, const FleetArrivalAction& action) {
        if (action.colonyPlan && (action.kind != FleetArrivalActionKind::Colonize
            || !production_template_valid(*action.colonyPlan))) return false;
        if (action.kind < FleetArrivalActionKind::None || action.kind > FleetArrivalActionKind::RecoverWreckage) return false;
        if ((action.kind == FleetArrivalActionKind::EnterWormhole) != (action.wormholeEndpoint != 0)) return false;
        if (target != 0) {
            return action.kind == FleetArrivalActionKind::None
                || action.kind == FleetArrivalActionKind::MergeWithFleet;
        }
        return action.kind != FleetArrivalActionKind::MergeWithFleet;
    };
    if (!targetActionValid(program.targetFleet, program.arrivalAction)) return false;
    if (std::any_of(program.queuedWaypoints.begin(), program.queuedWaypoints.end(),
            [&](const FleetWaypoint& waypoint) {
                return !targetActionValid(waypoint.targetFleet, waypoint.arrivalAction);
            })) {
        return false;
    }
    if (terminal_arrival_action(program.arrivalAction.kind) && !program.queuedWaypoints.empty()) return false;
    for (std::size_t index = 0; index + 1 < program.queuedWaypoints.size(); ++index) {
        if (terminal_arrival_action(program.queuedWaypoints[index].arrivalAction.kind)) return false;
    }
    if (program.repeatOrders) {
        if (program.queuedWaypoints.empty()) return false;
        const auto incompatible = [](const FleetArrivalAction& action) {
            return terminal_arrival_action(action.kind);
        };
        if (incompatible(program.arrivalAction)
            || std::any_of(program.queuedWaypoints.begin(), program.queuedWaypoints.end(),
                [&](const FleetWaypoint& waypoint) { return incompatible(waypoint.arrivalAction); })) {
            return false;
        }
        const auto distinctDestination = std::any_of(
            program.queuedWaypoints.begin(), program.queuedWaypoints.end(),
            [&](const FleetWaypoint& waypoint) {
                return program.targetFleet != waypoint.targetFleet
                    || !same_position(program.destination, waypoint.destination);
            });
        if (!distinctDestination) return false;
    }
    return true;
}

bool route_work_equipment_valid(const GameState& state, const Fleet& fleet, const FleetRouteProgram& program)
{
    const auto valid = [&](const FleetArrivalAction& action) {
        const auto task = arrival_work_task(action.kind);
        return !task || fleet_task_equipment_available(state, fleet, *task);
    };
    return valid(program.arrivalAction) && std::all_of(program.queuedWaypoints.begin(), program.queuedWaypoints.end(),
        [&](const auto& waypoint) { return valid(waypoint.arrivalAction); });
}

void activate_next_waypoint(GameState& state, Fleet& fleet)
{
    while (!fleet.destination && !fleet.waypointQueue.empty()) {
        const auto waypoint = fleet.waypointQueue.front();
        fleet.waypointQueue.erase(fleet.waypointQueue.begin());

        if (!fleet_warp_valid(state, fleet, waypoint.warp)) {
            fleet.waypointQueue.clear();
            fleet.arrivalAction.reset();
            fleet.repeatOrders = false;
            fleet.routeTemplate.clear();
            fleet.targetFleet = 0;
            return;
        }

        fleet.warp = waypoint.warp;
        fleet.arrivalAction = active_arrival_action(waypoint.arrivalAction);
        fleet.targetFleet = waypoint.targetFleet;

        if (waypoint.targetFleet != 0) {
            const auto target = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
                return candidate.id == waypoint.targetFleet && candidate.owner == fleet.owner;
            });
            fleet.destination = target != state.fleets.end() ? target->position : waypoint.destination;
            return;
        }

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
    if (program.clearRoute) {
        fleet.destination.reset();
        fleet.arrivalAction.reset();
        fleet.waypointQueue.clear();
        fleet.task = FleetTask::None;
        fleet.repeatOrders = false;
        fleet.routeTemplate.clear();
        fleet.targetFleet = 0;
        return true;
    }
    if (!route_tasks_valid(program) || !route_work_equipment_valid(state, fleet, program)) return false;
    const auto requestedWarp = program.warp == 0 ? fleet.warp : program.warp;
    if (!fleet_warp_valid(state, fleet, requestedWarp)) return false;
    if (std::any_of(program.queuedWaypoints.begin(), program.queuedWaypoints.end(),
            [&](const FleetWaypoint& waypoint) { return !fleet_warp_valid(state, fleet, waypoint.warp); })) {
        return false;
    }

    fleet.warp = requestedWarp;

    // A travel programme replaces stationary work. Delayed movement commands
    // cancel mining only when they physically reach the fleet.
    fleet.task = FleetTask::None;
    fleet.arrivalAction = active_arrival_action(program.arrivalAction);
    fleet.waypointQueue = program.queuedWaypoints;
    fleet.repeatOrders = program.repeatOrders;
    fleet.targetFleet = program.targetFleet;
    fleet.routeTemplate.clear();
    if (program.repeatOrders) {
        fleet.routeTemplate.reserve(1 + program.queuedWaypoints.size());
        fleet.routeTemplate.push_back({
            program.destination, requestedWarp, program.arrivalAction, program.targetFleet});
        fleet.routeTemplate.insert(
            fleet.routeTemplate.end(), program.queuedWaypoints.begin(), program.queuedWaypoints.end());
    }

    Position resolvedDestination = program.destination;
    if (program.targetFleet != 0) {
        const auto target = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
            return candidate.id == program.targetFleet && candidate.owner == fleet.owner;
        });
        if (target != state.fleets.end()) resolvedDestination = target->position;
    }

    if (program.targetFleet != 0) {
        fleet.destination = resolvedDestination;
    } else if (same_position(fleet.position, resolvedDestination)) {
        if (fleet.arrivalAction) {
            fleet.destination = resolvedDestination;
        } else {
            fleet.destination.reset();
            activate_next_waypoint(state, fleet);
        }
    } else {
        fleet.destination = resolvedDestination;
    }
    return true;
}

bool apply_task_program(GameState& state, Fleet& fleet, FleetTask task)
{
    if (task == FleetTask::None) {
        fleet.task = FleetTask::None;
        return true;
    }
    if (task != FleetTask::RemoteMining) {
        if (!strategic_task_available(state, fleet, task)) return false;
        fleet.task = task;
        return true;
    }
    if (fleet.destination || fleet.targetFleet != 0
        || !fleet.waypointQueue.empty()) return false;

    if (!fleet_can_remote_mine(state, fleet)) return false;

    const auto atUncolonizedPlanet = std::any_of(
        state.planets.begin(), state.planets.end(), [&](const Planet& planet) {
            const auto* star = find_star(state, planet.star);
            return planet.owner == 0 && star && same_position(star->position, fleet.position);
        });
    if (!atUncolonizedPlanet) return false;
    fleet.task = FleetTask::RemoteMining;
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
    result.damagePercent = fleet.damagePercent;
    result.electronics = fleet.electronics;
    result.colonists = fleet.colonists;
    result.arrivalAction = fleet.arrivalAction;
    result.waypointQueue = fleet.waypointQueue;
    result.minerals = fleet.minerals;
    result.task = fleet.task;
    result.repeatOrders = fleet.repeatOrders;
    result.routeTemplate = fleet.routeTemplate;
    result.ships = fleet_ship_stacks(fleet);
    result.targetFleet = fleet.targetFleet;
    return result;
}

void activate_next_predicted_waypoint(
    Position position,
    std::optional<Position>& destination,
    std::uint8_t& warp,
    std::vector<FleetWaypoint>& queue,
    bool repeatOrders,
    const std::vector<FleetWaypoint>& routeTemplate)
{
    if (!destination && queue.empty() && repeatOrders) queue = routeTemplate;
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
            activate_next_predicted_waypoint(
                position, destination, warp, queue, telemetry.repeatOrders, telemetry.routeTemplate);
            if (!destination) break;
        }

        const auto remaining = distance_between(position, *destination);
        const auto maximumDistance = warp_distance(warp);
        if (remaining <= epsilon || maximumDistance <= epsilon) {
            position = *destination;
            destination.reset();
            activate_next_predicted_waypoint(
                position, destination, warp, queue, telemetry.repeatOrders, telemetry.routeTemplate);
            continue;
        }

        if (maximumDistance >= remaining - epsilon) {
            position = *destination;
            destination.reset();
            activate_next_predicted_waypoint(
                position, destination, warp, queue, telemetry.repeatOrders, telemetry.routeTemplate);
        } else {
            const auto fraction = maximumDistance / remaining;
            position.x += (destination->x - position.x) * fraction;
            position.y += (destination->y - position.y) * fraction;
        }
    }

    return position;
}

} // namespace

bool fleet_route_tasks_valid(const FleetRouteProgram& program)
{
    return route_tasks_valid(program);
}

CommunicationRoute communication_route(const GameState& state, PlayerId player, Position position, std::uint64_t atTurn)
{
    if (!atTurn) atTurn = state.turn;
    const auto fields = communication_fields(state, player, atTurn);
    const auto jamDelay = communication_jamming_delay(state, player, position, atTurn);

    // A colony without its capital radar has no instant route. Synthetic
    // ship-only combat scenarios have no colony/root and resolve onboard
    // orders locally, without a fictitious distant receiver.
    if (std::none_of(fields.begin(), fields.end(), [](const CommunicationField& field) {
            return field.connected;
        })) {
        const bool hasColony = std::any_of(state.planets.begin(), state.planets.end(),
            [&](const Planet& planet) { return planet.owner == player && planet.population > 0; });
        Position capital=position;
        for(const auto& p:state.planets) if(p.owner==player && p.population)
            if(const auto* star=find_star(state,p.star)) { capital=star->position; if(p.homeworld) break; }
        return {hasColony ? SignalChannel::Slow : SignalChannel::Fast, position, capital, hasColony ? 1U : 0U, jamDelay};
    }

    // A position inside any field connected to a colony root belongs to the
    // instantaneous empire mesh.
    for (const auto& field : fields) {
        if (field.connected && inside_field(position, field)) return {SignalChannel::Fast, position, field.position, 0, jamDelay};
    }

    // A detached scanner island still communicates instantaneously inside its
    // own component. Find all physical transceivers belonging to the source
    // island and to the colony-rooted mesh, then use the shortest real
    // point-to-point subspace hop. Scanner radii never subtract from this distance:
    // there is no receiver on the imaginary surface of a sensor field.
    const auto sourceComponent = field_component_containing(fields, position);
    std::vector<Position> sourceTransceivers{position};
    std::vector<Position> connectedTransceivers;

    for (const auto& planet : state.planets) {
        if (planet.owner != player || planet.population == 0 || planet.radar == PlanetaryRadarLevel::None) continue;
        const auto* star = find_star(state, planet.star);
        if (star && std::any_of(fields.begin(), fields.end(), [&](const CommunicationField& field) {
            return field.connected && inside_field(star->position, field);
        })) connectedTransceivers.push_back(star->position);
    }
    for (const auto& fleet : state.fleets) {
        if (fleet.owner != player || !fleet_transmits(fleet,atTurn)) continue;
        const auto inConnectedMesh = std::any_of(
            fields.begin(), fields.end(), [&](const CommunicationField& field) {
                return field.connected && inside_field(fleet.position, field);
            });
        if (inConnectedMesh) connectedTransceivers.push_back(fleet.position);
        if (inside_component(fleet.position, fields, sourceComponent)) {
            sourceTransceivers.push_back(fleet.position);
        }
    }

    Position transmitter=position, receiverPosition=position;
    double nearest = std::numeric_limits<double>::infinity();
    for (const auto source : sourceTransceivers) {
        for (const auto receiver : connectedTransceivers) {
            const auto gap = distance_between(source, receiver);
            if (gap < nearest - 1e-6 || (std::abs(gap-nearest)<=1e-6
                && std::tuple{source.x,source.y,receiver.x,receiver.y}
                    < std::tuple{transmitter.x,transmitter.y,receiverPosition.x,receiverPosition.y})) {
                nearest=gap; transmitter=source; receiverPosition=receiver;
            }
        }
    }
    if (!std::isfinite(nearest)) return {SignalChannel::Slow, position, position, 1, jamDelay};
    if (nearest <= 0.000001) return {SignalChannel::Fast, transmitter, receiverPosition, 0, jamDelay};

    // Any non-zero subspace hop lands at a later annual planning boundary;
    // priority can never alter this physical propagation time.
    return {SignalChannel::Slow, transmitter, receiverPosition,
        static_cast<std::uint32_t>(std::ceil(nearest / kCommunicationSignalSpeed)), jamDelay};
}

std::uint32_t communication_delay_turns(const GameState& state, PlayerId player, Position position, std::uint64_t atTurn)
{
    const auto route=communication_route(state,player,position,atTurn);
    return route.propagationTurns+route.receiverDelay;
}

namespace {
void record_pulse(GameState& state, PlayerId owner, Position position, std::uint64_t turn, SignalChannel channel)
{
    if (!find_player(state,owner)) return;
    if (std::any_of(state.emissions.begin(),state.emissions.end(),[&](const auto& p) {
        return p.owner==owner && p.emittedTurn==turn && p.channel==channel && same_position(p.position,position);
    })) return; // One physical annual packet batch, not one marker per payload.
    EmissionPulse pulse; pulse.id=state.nextEmissionId++; pulse.owner=owner;
    pulse.position=position; pulse.emittedTurn=turn; pulse.channel=channel;
    // Preserve a pulse through every currently fitted receiver's possible reach.
    // This bounds host storage without querying a future target or its position.
    pulse.maximumRange=2*kColonySensorRange;
    for(const auto& f:state.fleets) {
        pulse.maximumRange=std::max(pulse.maximumRange,2*fleet_sensor_range(state,f));
        if(f.owner==owner && same_position(f.position,position) && fleet_transmits(f,turn)) {
            if(!pulse.fleet || f.id<pulse.fleet) pulse.fleet=f.id;
            if(f.electronics.deception) {
                double strength=0;
                for(const auto& stack:fleet_ship_stacks(f)) if(const auto* d=find_ship_design(state,stack.design)) {
                    double remaining=1;
                    for(auto c:d->components) remaining*=1-std::clamp(component_spec(c).jammingPercent/100,0.0,1.0);
                    strength=std::max(strength,1-remaining);
                }
                pulse.deceptionStrength=std::max(pulse.deceptionStrength,std::min(0.95,strength));
            }
        }
    }
    for(const auto& p:state.planets) if(p.owner==owner && p.population)
        if(const auto* star=find_star(state,p.star); star && same_position(star->position,position)) {
            pulse.colony=p.id; pulse.fleet=0; pulse.deceptionStrength=0; break;
        }
    state.emissions.push_back(std::move(pulse));
}
}
void record_report_transmission(GameState& state,PlayerId owner,Position position,std::uint64_t turn,FleetId observer)
{
    if(held_report_source(state,owner,position,turn,observer)) return;
    const auto route=communication_route(state,owner,position,turn);
    if(!same_position(route.transmitter,position)) record_pulse(state,owner,position,turn,SignalChannel::Fast);
    record_pulse(state,owner,route.transmitter,turn,route.channel);
}
void record_command_transmission(GameState& state,PlayerId owner,Position target,std::uint64_t turn)
{
    const auto route=communication_route(state,owner,target,turn);
    // Commands originate at the actual connected endpoint, never at their silent receiver.
    if(std::any_of(state.planets.begin(),state.planets.end(),[&](const auto& p){return p.owner==owner && p.homeworld && p.population;}))
        record_pulse(state,owner,route.receiver,turn,route.channel);
}
std::uint64_t dispatch_report_delivery_turn(GameState& state,PlayerId owner,Position source,std::uint64_t turn,FleetId observer)
{
    const auto due=fleet_report_delivery_turn(state,owner,source,turn,observer);
    if(due!=kAwaitingTransmission) record_report_transmission(state,owner,source,turn,observer);
    return due;
}

bool fleet_has_instant_link(const GameState& state, const Fleet& fleet)
{
    return fleet_can_share_reports(state, fleet, state.turn)
        && communication_delay_turns(state, fleet.owner, fleet.position) == 0;
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
    if (const auto contact = fleet_radar_position(state, fleet)) return contact->position;
    const auto telemetry = confirmed_fleet_telemetry(state, fleet);
    return project_from_telemetry(telemetry, fleet_telemetry_age(state, fleet));
}

std::optional<ReconContact> fleet_radar_position(const GameState& state, const Fleet& fleet)
{
    const auto* player = find_player(state, fleet.owner);
    if (!player) return {};
    const auto telemetry = confirmed_fleet_telemetry(state, fleet);
    for (const auto& contact : player->friendlyFleetContacts)
        if (contact.fleet == fleet.id && !contact.lost && contact.deliveryTurn <= state.turn
            && contact.lastSeenTurn <= state.turn && contact.lastSeenTurn > telemetry.observedTurn)
            return contact;
    return {};
}

std::uint64_t fleet_position_age(const GameState& state, const Fleet& fleet)
{
    const auto contact = fleet_radar_position(state, fleet);
    const auto observed = contact ? contact->lastSeenTurn : confirmed_fleet_telemetry(state, fleet).observedTurn;
    return state.turn > observed ? state.turn - observed : 0;
}

std::vector<Fleet> missing_fleet_contacts(const GameState& state, PlayerId player)
{
    auto contacts = wormhole_missing_contacts(state, player);
    for (const auto& loss : state.pendingFleetLossContacts)
        if (loss.lastContact.owner == player && loss.deliveryTurn > state.turn)
            contacts.push_back(fleet_player_view(state, loss.lastContact));
    return contacts;
}

Fleet fleet_player_view(const GameState& state, const Fleet& fleet)
{
    for (const auto& transit : state.wormholeTransits) {
        if (transit.lastContact.id == fleet.id && transit.lastContact.owner == fleet.owner
            && &fleet != &transit.lastContact && fleet.telemetry.observedTurn < transit.enteredTurn
            && transit.status != WormholeTransitStatus::EmergenceConfirmed
            && transit.status != WormholeTransitStatus::PresumedLost)
            return fleet_player_view(state, transit.lastContact);
    }
    const auto telemetry = confirmed_fleet_telemetry(state, fleet);
    Fleet view = fleet;
    view.position = projected_fleet_position(state, fleet);
    view.destination = telemetry.destination;
    view.warp = telemetry.warp;
    view.fuel = telemetry.fuel;
    view.damagePercent = telemetry.damagePercent;
    view.electronics = telemetry.electronics;
    view.colonists = telemetry.colonists;
    view.arrivalAction = telemetry.arrivalAction;
    view.waypointQueue = telemetry.waypointQueue;
    view.minerals = telemetry.minerals;
    view.task = telemetry.task;
    view.repeatOrders = telemetry.repeatOrders;
    view.routeTemplate = telemetry.routeTemplate;
    view.targetFleet = telemetry.targetFleet;
    if (!telemetry.ships.empty()) view.ships = telemetry.ships;
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
    const std::vector<FleetWaypoint>& queuedWaypoints,
    bool repeatOrders,
    FleetId targetFleet,
    bool clearRoute)
{
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == fleetId && candidate.owner == player;
    });
    if (fleet == state.fleets.end()) return false;

    FleetRouteProgram program{
        destination, warp, arrivalAction, queuedWaypoints, false, repeatOrders, targetFleet};
    if (!route_tasks_valid(program) || !route_work_equipment_valid(state, *fleet, program)) return false;
    const auto knownFleet = fleet_player_view(state, *fleet);
    if (!fleet_work_order_error(state, knownFleet, program.destination, program.arrivalAction.kind).empty()
        || std::any_of(program.queuedWaypoints.begin(), program.queuedWaypoints.end(), [&](const auto& waypoint) {
            return !fleet_work_order_error(state, knownFleet, waypoint.destination, waypoint.arrivalAction.kind).empty();
        })) return false;
    const auto validTarget = [&](FleetId candidateId) {
        if (candidateId == 0) return true;
        return candidateId != fleetId
            && std::any_of(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
                return candidate.id == candidateId && candidate.owner == player;
            });
    };
    const auto knownEntry = [&](const FleetArrivalAction& action) {
        if (action.kind != FleetArrivalActionKind::EnterWormhole) return true;
        const auto* knowledge = known_wormhole(state, player, action.wormholeEndpoint);
        return knowledge && !knowledge->collapsed && knowledge->stability != WormholeStability::Unknown;
    };
    if (!knownEntry(arrivalAction) || std::any_of(queuedWaypoints.begin(), queuedWaypoints.end(),
        [&](const auto& waypoint) { return !knownEntry(waypoint.arrivalAction); })) return false;
    if (!validTarget(targetFleet)
        || std::any_of(queuedWaypoints.begin(), queuedWaypoints.end(),
            [&](const FleetWaypoint& waypoint) { return !validTarget(waypoint.targetFleet); })) {
        return false;
    }
    const auto visiblePosition = projected_fleet_position(state, *fleet);
    program.clearRoute = clearRoute || (targetFleet == 0 && same_position(destination, visiblePosition)
        && arrivalAction.kind == FleetArrivalActionKind::None
        && queuedWaypoints.empty());
    const auto requestedWarp = warp == 0 ? fleet->warp : warp;
    if (!program.clearRoute && !fleet_warp_valid(state, *fleet, requestedWarp)) return false;
    if (std::any_of(queuedWaypoints.begin(), queuedWaypoints.end(),
            [&](const FleetWaypoint& waypoint) { return !fleet_warp_valid(state, *fleet, waypoint.warp); })) {
        return false;
    }

    record_command_transmission(state,player,fleet->position,state.turn);
    const auto delivery = state.turn + communication_delay_turns(state, player, fleet->position);
    const auto delay = delivery - state.turn;
    if (delay == 0) {
        if (program.clearRoute) fleet->pendingCommands.clear();
        return apply_route_program(state, *fleet, program);
    }

    fleet->pendingCommands.push_back(PendingFleetCommand{
        state.turn,
        state.turn + delay,
        std::move(program),
    });
    return true;
}

bool submit_fleet_task_command(
    GameState& state,
    PlayerId player,
    FleetId fleetId,
    FleetTask task)
{
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == fleetId && candidate.owner == player;
    });
    if (fleet == state.fleets.end()) return false;

    record_command_transmission(state,player,fleet->position,state.turn);
    const auto delivery = state.turn + communication_delay_turns(state, player, fleet->position);
    const auto delay = delivery - state.turn;
    if (delay == 0) return apply_task_program(state, *fleet, task);

    fleet->pendingCommands.push_back(PendingFleetCommand{
        state.turn,
        state.turn + delay,
        {},
        task,
    });
    return true;
}

void deliver_due_fleet_commands(GameState& state)
{
    for (auto& fleet : state.fleets) {
        if (fleet.electronics.resumeTurn && state.turn >= fleet.electronics.resumeTurn) fleet.electronics = {};
        std::stable_sort(fleet.pendingCommands.begin(), fleet.pendingCommands.end(),
            [](const PendingFleetCommand& lhs, const PendingFleetCommand& rhs) {
                if (lhs.deliveryTurn != rhs.deliveryTurn) return lhs.deliveryTurn < rhs.deliveryTurn;
                return lhs.issuedTurn < rhs.issuedTurn;
            });

        std::optional<std::uint64_t> stoppedThrough;
        std::size_t processed = 0;
        for (const auto& pending : fleet.pendingCommands) {
            if (pending.deliveryTurn > state.turn) break;
            ++processed;
            if (stoppedThrough && pending.issuedTurn <= *stoppedThrough) continue;
            if (pending.electronics) {
                const auto& e = *pending.electronics;
                apply_electronics_program(fleet,e.resumeTurn && e.resumeTurn <= state.turn ? ElectronicsProgram{} : e);
            }
            else if (pending.task) apply_task_program(state, fleet, *pending.task);
            else {
                apply_route_program(state, fleet, pending.program);
                if (pending.program.clearRoute) stoppedThrough = pending.issuedTurn;
            }
        }
        fleet.pendingCommands.erase(fleet.pendingCommands.begin(), fleet.pendingCommands.begin() + processed);
        std::erase_if(fleet.pendingCommands, [&](const PendingFleetCommand& pending) {
            return stoppedThrough && pending.issuedTurn <= *stoppedThrough;
        });
    }
}

void deliver_due_fleet_telemetry(GameState& state)
{
    const auto deliver = [&](Fleet& fleet) {
        for (const auto& packet : fleet.telemetryInTransit) {
            if (packet.deliveryTurn > state.turn) continue;
            if (packet.telemetry.observedTurn >= fleet.telemetry.observedTurn) {
                fleet.telemetry = packet.telemetry;
            }
        }
        std::erase_if(fleet.telemetryInTransit, [&](const PendingFleetTelemetry& packet) {
            return packet.deliveryTurn <= state.turn;
        });
    };
    for (auto& fleet : state.fleets) deliver(fleet);
    // A lost physical source cannot recall packets that were already sent.
    // Keep updating its last known contact until the loss report itself arrives.
    for (auto& loss : state.pendingFleetLossContacts) deliver(loss.lastContact);
}

void publish_fleet_telemetry(GameState& state, std::uint64_t observationTurn)
{
    for (auto& fleet : state.fleets) {
        if (!fleet_can_share_reports(state, fleet, observationTurn)) continue;
        // Active scan/jamming is a real fast-channel emission even when this
        // year's data packet must take the slow route. Communication-only
        // windows leave these emitters off.
        if(fleet_active_sensors(fleet,observationTurn)) {
            bool jammer=false;
            for(const auto& stack:fleet_ship_stacks(fleet)) if(const auto* d=find_ship_design(state,stack.design))
                for(auto c:d->components) jammer|=component_spec(c).jammingPercent>0;
            if(fleet_has_scanner(state,fleet) || jammer) record_pulse(state,fleet.owner,fleet.position,observationTurn,SignalChannel::Fast);
        }
        record_report_transmission(state,fleet.owner,fleet.position,observationTurn,fleet.id);
        const auto snapshot = authoritative_snapshot(state, fleet, observationTurn);
        const auto delay = communication_delay_turns(state, fleet.owner, fleet.position, observationTurn);
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

FleetId held_report_source(const GameState& state, PlayerId owner, Position position,
    std::uint64_t turn, FleetId observer)
{
    const auto* source=report_source(state,owner,position,turn,observer);
    return source && !fleet_transmits(*source,turn) ? source->id : 0;
}

std::uint64_t fleet_report_delivery_turn(const GameState& state, PlayerId owner, Position position,
    std::uint64_t turn, FleetId observer)
{
    if(held_report_source(state,owner,position,turn,observer)) return kAwaitingTransmission;
    return turn+communication_delay_turns(state,owner,position,turn);
}

void dispatch_fleet_report_outbox(GameState& state)
{
    visit_fleet_report_outbox(state,[&](PlayerId owner,auto& report) {
        if(report.deliveryTurn!=kAwaitingTransmission || !report.transmissionFleet) return;
        const auto source=std::find_if(state.fleets.begin(),state.fleets.end(),[&](const auto& f) {
            return f.id==report.transmissionFleet && f.owner==owner;
        });
        if(source==state.fleets.end() || !fleet_can_share_reports(state,*source,state.turn)) return;
        report.deliveryTurn=dispatch_report_delivery_turn(state,owner,source->position,state.turn,source->id);
        report.transmissionFleet=0;
        if constexpr(std::is_same_v<std::remove_cvref_t<decltype(report)>,PendingPlayerReport>) {
            if(report.contact) { report.contact->deliveryTurn=report.deliveryTurn; report.contact->transmissionFleet=0; }
            if(report.sensorStatus) { report.sensorStatus->deliveryTurn=report.deliveryTurn; report.sensorStatus->transmissionFleet=0; }
            if(report.battle) {
                for(auto& loss:state.pendingFleetLossContacts) if(loss.deliveryTurn==kAwaitingTransmission && loss.lastContact.owner==owner)
                    if(std::any_of(report.battle->units.begin(),report.battle->units.end(),[&](const auto& u){return u.owner==owner && u.fleet==loss.lastContact.id;})) loss.deliveryTurn=report.deliveryTurn;
                for(auto& loss:state.pendingStationContacts) if(loss.deliveryTurn==kAwaitingTransmission && loss.lastContact.owner==owner)
                    if(std::any_of(report.battle->units.begin(),report.battle->units.end(),[&](const auto& u){return u.owner==owner && u.station==loss.lastContact.id;})) loss.deliveryTurn=report.deliveryTurn;
            }
        }
    });
}

} // namespace suns
