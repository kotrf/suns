#include "suns/mobile_base.hpp"
#include "suns/communications.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/deep_space.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/propulsion.hpp"
#include "suns/stations.hpp"
#include "suns/survey_probes.hpp"
#include "suns/turn_processor.hpp"
#include "suns/ship_refit.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace suns {
const Fleet* find_service_fleet(const GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const auto& f) { return f.id == id; });
    return it == state.fleets.end() ? nullptr : &*it;
}
namespace {
Fleet* client_fleet(GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const auto& f) { return f.id == id; });
    return it == state.fleets.end() ? nullptr : &*it;
}
double service_modules(const GameState& state, const Fleet& fleet, ShipComponentType type)
{
    double count = 0;
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(state, stack.design); design && design->hull == ShipHullType::ExpeditionBase)
            count += std::count(design->components.begin(), design->components.end(), type) * stack.count;
    return count;
}

bool has_route(const Fleet& fleet)
{
    return fleet.destination || fleet.targetFleet || !fleet.waypointQueue.empty() || fleet.repeatOrders;
}

std::string gate_endpoint_error(const GameState& state, PlayerId owner, const GateEndpoint& endpoint,
    double distance, double mass)
{
    auto range = kExpeditionGateRangeLy;
    auto capacity = kExpeditionGateMassKt;
    if (endpoint.base) {
        const auto* base = find_service_fleet(state, endpoint.base);
        if (!base || base->owner != owner || !same_position(base->position, endpoint.position))
            return "Base is unavailable at the ordered position.";
        if (const auto error = mobile_base_gate_error(state, *base); !error.empty()) return error;
    } else {
        const auto* planet = find_planet(state, endpoint.colony);
        const auto* star = planet ? find_star(state, planet->star) : nullptr;
        if (!star || !same_position(star->position, endpoint.position)
            || !stargate_station_available(state, owner, endpoint.colony))
            return "Orbital gate is unavailable at the ordered position.";
        const auto* spec = station_stargate(*find_orbital_station_at_planet(state, endpoint.colony));
        range = spec->rangeLy;
        capacity = spec->maximumMassKt;
    }
    if (range > 0 && distance > range) return "Distance exceeds the gate's range.";
    if (capacity > 0 && mass > capacity) return "Loaded gross fleet mass exceeds the gate's limit.";
    return {};
}
}
double fleet_dry_mass(const GameState& state, const Fleet& fleet)
{
    double mass = 0;
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(state, stack.design)) mass += ship_design_mass(*design) * stack.count;
    return mass;
}
std::uint32_t mobile_base_hulls(const GameState& state, const Fleet& fleet)
{
    std::uint32_t count = 0;
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(state, stack.design); design && design->hull == ShipHullType::ExpeditionBase)
            count += stack.count;
    return count;
}
bool mobile_base_dock(const GameState& state, const Fleet& fleet)
{
    if (fleet.outpost == OutpostKind::OrbitalShipyard || fleet_construction_kit(state,fleet)==OutpostKind::OrbitalShipyard) return true;
    return fleet.outpost == OutpostKind::None && service_modules(state, fleet, ShipComponentType::ExpeditionDock) > 0;
}
bool mobile_base_gate(const GameState& state, const Fleet& fleet)
{
    return fleet.outpost == OutpostKind::None && service_modules(state, fleet, ShipComponentType::ExpeditionStargate) > 0;
}
std::string mobile_base_gate_error(const GameState& state, const Fleet& fleet)
{
    if (!mobile_base_gate(state, fleet)) return "Expedition Stargate required.";
    if (!fleet.mobileBase.enabled) return "External services disabled.";
    if (!std::isfinite(fleet.damagePercent) || fleet.damagePercent >= 100) return "Base immobilized by damage.";
    if (fleet.lastMovedTurn > state.turn || has_route(fleet) || fleet.task != FleetTask::None)
        return "Base must be stationary and idle throughout the jump year.";
    const auto reserve = fleet.mobileBase.fuelReserve;
    if (!std::isfinite(fleet.fuel) || !std::isfinite(reserve) || reserve < 0
        || fuel_in_complete_containers(fleet.fuel - reserve) < kExpeditionGateFuelMg)
        return "Base needs 10 mg of fuel above its reserve.";
    return {};
}
double mobile_base_repair_capacity(const GameState& state, const Fleet& fleet)
{
    return service_modules(state, fleet, ShipComponentType::ExpeditionWorkshop) * kWorkshopRepairKt;
}
bool mobile_base_program_valid(const MobileBaseProgram& program)
{
    if (!std::isfinite(program.fuelReserve) || program.fuelReserve < 0 || program.clients.size() > 128) return false;
    std::set<FleetId> ids;
    for (auto id : program.clients) if (!id || !ids.insert(id).second) return false;
    return true;
}
std::string mobile_base_service_error(const GameState& state, const Fleet& fleet, std::uint64_t serviceTurn)
{
    // The constructor's receiving collar remains usable while assembling,
    // paused, unstaffed or exhausted: otherwise it could never be resupplied.
    if (fleet.outpost == OutpostKind::OrbitalShipyard)
        return fleet.damagePercent >= 100 ? "Base disabled by damage" : "";
    if (!mobile_base_dock(state, fleet)) return "Expedition Dock required";
    if (!fleet.mobileBase.enabled) return "External services disabled";
    if (fleet.damagePercent >= 100) return "Base immobilized by damage";
    if (fleet.lastMovedTurn >= serviceTurn) return "Base moved this year; docking starts next year";
    if (has_route(fleet))
        return "Base has an active route";
    return {};
}
bool submit_mobile_base_command(GameState& state, PlayerId player, FleetId id, const MobileBaseProgram& program)
{
    auto* fleet = client_fleet(state, id);
    if (!fleet || fleet->owner != player || !mobile_base_program_valid(program)
        || (!mobile_base_dock(state, *fleet) && !mobile_base_gate(state, *fleet))) return false;
    record_command_transmission(state, player, fleet->position, state.turn);
    const auto delay = communication_delay_turns(state, player, fleet->position);
    if (!delay) fleet->mobileBase = program;
    else {
        PendingFleetCommand command;
        command.issuedTurn = state.turn; command.deliveryTurn = state.turn + delay; command.mobileBase = program;
        fleet->pendingCommands.push_back(std::move(command));
    }
    return true;
}
bool mobile_gate_transit_valid(const MobileGateTransit& transit)
{
    const auto valid_endpoint = [](const GateEndpoint& endpoint) {
        return (bool(endpoint.colony) != bool(endpoint.base))
            && std::isfinite(endpoint.position.x) && std::isfinite(endpoint.position.y);
    };
    const auto& [source, destination] = transit;
    return valid_endpoint(source) && valid_endpoint(destination) && (source.base || destination.base)
        && (!source.base || source.base != destination.base)
        && (!source.colony || source.colony != destination.colony)
        && !same_position(source.position, destination.position);
}
std::string mobile_gate_transit_error(const GameState& state, const Fleet& fleet, const MobileGateTransit& transit)
{
    if (!mobile_gate_transit_valid(transit)) return "Two distinct gates, including an Expedition Stargate, are required.";
    if (fleet.id == transit.source.base || fleet.id == transit.destination.base || fleet.outpost != OutpostKind::None)
        return "A deployed outpost or either endpoint fleet cannot make this jump.";
    if (!same_position(fleet.position, transit.source.position) || has_route(fleet) || fleet.task != FleetTask::None)
        return "Fleet must be idle at the source gate with no route or work task.";
    if (fleet.lastMovedTurn > state.turn) return "A gate hop requires a separate movement year.";
    if (!std::isfinite(fleet.damagePercent) || fleet.damagePercent >= 100) return "An immobilized fleet cannot transit.";
    if (!stargate_cargo_allowed(state, fleet)) return "Generalist gates cannot transport cargo or colonists.";
    const auto mass = fleet_gross_mass(state, fleet);
    const auto distance = distance_between(transit.source.position, transit.destination.position);
    if (!std::isfinite(mass) || !std::isfinite(distance)) return "Invalid fleet mass or distance.";
    if (const auto error = gate_endpoint_error(state, fleet.owner, transit.source, distance, mass); !error.empty())
        return "Source: " + error;
    if (const auto error = gate_endpoint_error(state, fleet.owner, transit.destination, distance, mass); !error.empty())
        return "Destination: " + error;
    return {};
}
bool submit_mobile_gate_command(GameState& state, PlayerId player, const MobileGateOrder& order)
{
    auto* fleet = client_fleet(state, order.fleet);
    if (!fleet || fleet->owner != player || fleet->outpost != OutpostKind::None
        || !mobile_gate_transit_valid(order.transit)) return false;
    record_command_transmission(state, player, fleet->position, state.turn);
    const auto delay = communication_delay_turns(state, player, fleet->position);
    if (!delay) fleet->mobileGate = order.transit;
    else {
        PendingFleetCommand command;
        command.issuedTurn = state.turn;
        command.deliveryTurn = state.turn + delay;
        command.mobileGate = order.transit;
        fleet->pendingCommands.push_back(std::move(command));
    }
    return true;
}
void resolve_mobile_gate_transits(GameState& state)
{
    std::vector<Fleet*> jumps;
    for (auto& fleet : state.fleets) if (fleet.mobileGate) jumps.push_back(&fleet);
    std::sort(jumps.begin(), jumps.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    for (auto* fleet : jumps) {
        const auto transit = *fleet->mobileGate;
        fleet->mobileGate.reset();
        const bool success = mobile_gate_transit_error(state, *fleet, transit).empty();
        const auto& endpoint = success ? transit.destination : transit.source;
        const auto* planet = endpoint.colony ? find_planet(state, endpoint.colony) : nullptr;
        const auto* star = planet ? find_star(state, planet->star) : nullptr;
        if (success) {
            for (auto id : {transit.source.base, transit.destination.base}) if (id) {
                auto& base = *client_fleet(state, id);
                base.fuel = fuel_after_consumption(base.fuel, kExpeditionGateFuelMg);
            }
            fleet->position = endpoint.base ? find_service_fleet(state, endpoint.base)->position : star->position;
            fleet->lastMovedTurn = state.turn + 1;
            fleet->fuelStalled = false;
            pause_probe(*fleet);
            fleet->visitor = {};
        }
        const bool atColony = planet && planet->owner == fleet->owner && star && same_position(star->position, fleet->position);
        queue_player_report(state, fleet->owner, success ? PlayerReportKind::StargateTransit : PlayerReportKind::StargateTransitFailed,
            fleet->position, state.turn + 1, atColony ? star->id : 0, atColony ? planet->id : 0, fleet->id, 0,
            ProductionKind::ColonyShip, 0, ResearchField::Electronics, 0, fleet->minerals, fleet->colonists);
    }
}
bool transfer_mobile_base_fuel(GameState& state, PlayerId player, const TransferFleetFuelOrder& order, std::uint64_t serviceTurn)
{
    auto* from = client_fleet(state, order.source); auto* to = client_fleet(state, order.destination);
    if (!from || !to || from == to || from->owner != player || to->owner != player
        || !same_position(from->position, to->position) || !std::isfinite(order.amount) || order.amount <= 0
        || !std::isfinite(order.sourceReserve) || order.sourceReserve < 0) return false;
    for (const auto* fleet : {from, to}) if (mobile_base_hulls(state, *fleet)) {
        if (!mobile_base_service_error(state, *fleet, serviceTurn).empty()) return false;
    }
    const auto reserve = std::max(order.sourceReserve, mobile_base_dock(state, *from) ? from->mobileBase.fuelReserve : 0.0);
    return transfer_fuel(*from, *to, order.amount, reserve, fleet_fuel_capacity(state, *to)) > 0.0;
}
void service_mobile_bases(GameState& state, std::uint64_t serviceTurn)
{
    const auto refitting = fleets_under_refit(state);
    std::vector<FleetId> bases;
    for (const auto& f : state.fleets) if (mobile_base_dock(state, f)) bases.push_back(f.id);
    std::sort(bases.begin(), bases.end());
    for (const auto id : bases) {
        auto& base = *client_fleet(state, id);
        if (base.outpost == OutpostKind::None && service_modules(state,base,ShipComponentType::ExpeditionDock)==0) continue;
        if (base.outpost == OutpostKind::OrbitalShipyard
            && (!base.mobileBase.enabled || !base.orbitalBase.supported || !base.colonists
                || base.orbitalBase.assemblyYears < kOrbitalAssemblyYears)) continue;
        if (base.lastServiceTurn >= serviceTurn || !mobile_base_service_error(state, base, serviceTurn).empty()) continue;
        base.lastServiceTurn = serviceTurn;
        double budget = mobile_base_repair_capacity(state, base);
        for (const auto clientId : base.mobileBase.clients) {
            auto* client = client_fleet(state, clientId);
            if (!client || client == &base || client->owner != base.owner || client->outpost != OutpostKind::None
                || !same_position(client->position, base.position)) continue;
            if (!refitting.contains(clientId))
                (void)transfer_fuel(base, *client, base.fuel, base.mobileBase.fuelReserve, fleet_fuel_capacity(state, *client));
            const auto mass = fleet_dry_mass(state, *client);
            if (mass <= 0 || client->damagePercent <= 0) continue;
            const auto work = std::max(0.0, std::min({budget, mass * client->damagePercent / 100.0,
                base.minerals.ironium / kWorkshopMineralsPerKt.ironium,
                base.minerals.boranium / kWorkshopMineralsPerKt.boranium,
                base.minerals.germanium / kWorkshopMineralsPerKt.germanium}));
            client->damagePercent = std::max(0.0, client->damagePercent - work / mass * 100.0);
            base.minerals.ironium = std::max(0.0, base.minerals.ironium - work * kWorkshopMineralsPerKt.ironium);
            base.minerals.boranium = std::max(0.0, base.minerals.boranium - work * kWorkshopMineralsPerKt.boranium);
            base.minerals.germanium = std::max(0.0, base.minerals.germanium - work * kWorkshopMineralsPerKt.germanium);
            budget -= work;
        }
    }
}
} // namespace suns
