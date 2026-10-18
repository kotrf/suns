#include "suns/mobile_base.hpp"
#include "suns/communications.hpp"
#include "suns/turn_processor.hpp"
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
    return fleet.outpost == OutpostKind::None && service_modules(state, fleet, ShipComponentType::ExpeditionDock) > 0;
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
    if (!mobile_base_dock(state, fleet)) return "Expedition Dock required";
    if (!fleet.mobileBase.enabled) return "External services disabled";
    if (fleet.damagePercent >= 100) return "Base immobilized by damage";
    if (fleet.lastMovedTurn >= serviceTurn) return "Base moved this year; docking starts next year";
    if (fleet.destination || fleet.targetFleet || !fleet.waypointQueue.empty() || fleet.repeatOrders)
        return "Base has an active route";
    return {};
}
bool submit_mobile_base_command(GameState& state, PlayerId player, FleetId id, const MobileBaseProgram& program)
{
    auto* fleet = client_fleet(state, id);
    if (!fleet || fleet->owner != player || !mobile_base_dock(state, *fleet) || !mobile_base_program_valid(program)) return false;
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
bool transfer_mobile_base_fuel(GameState& state, PlayerId player, const TransferFleetFuelOrder& order, std::uint64_t serviceTurn)
{
    auto* from = client_fleet(state, order.source); auto* to = client_fleet(state, order.destination);
    if (!from || !to || from == to || from->owner != player || to->owner != player
        || !same_position(from->position, to->position) || !std::isfinite(order.amount) || order.amount <= 0
        || !std::isfinite(order.sourceReserve) || order.sourceReserve < 0) return false;
    bool base = false;
    for (const auto* fleet : {from, to}) if (mobile_base_hulls(state, *fleet)) {
        base = true;
        if (!mobile_base_service_error(state, *fleet, serviceTurn).empty()) return false;
    }
    if (!base) return false;
    const auto reserve = std::max(order.sourceReserve, mobile_base_dock(state, *from) ? from->mobileBase.fuelReserve : 0.0);
    const auto amount = std::min({order.amount, std::max(0.0, from->fuel - reserve),
        std::max(0.0, fleet_fuel_capacity(state, *to) - to->fuel)});
    if (amount <= 0) return false;
    from->fuel -= amount; to->fuel += amount; return true;
}
void service_mobile_bases(GameState& state, std::uint64_t serviceTurn)
{
    std::vector<FleetId> bases;
    for (const auto& f : state.fleets) if (mobile_base_dock(state, f)) bases.push_back(f.id);
    std::sort(bases.begin(), bases.end());
    for (const auto id : bases) {
        auto& base = *client_fleet(state, id);
        if (base.lastServiceTurn >= serviceTurn || !mobile_base_service_error(state, base, serviceTurn).empty()) continue;
        base.lastServiceTurn = serviceTurn;
        double budget = mobile_base_repair_capacity(state, base);
        for (const auto clientId : base.mobileBase.clients) {
            auto* client = client_fleet(state, clientId);
            if (!client || client == &base || client->owner != base.owner || client->outpost != OutpostKind::None
                || !same_position(client->position, base.position)) continue;
            const auto fuel = std::min(std::max(0.0, base.fuel - base.mobileBase.fuelReserve),
                std::max(0.0, fleet_fuel_capacity(state, *client) - client->fuel));
            base.fuel -= fuel; client->fuel += fuel;
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
