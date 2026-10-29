#include "computer_policy.hpp"
#include "suns/transport_lines.hpp"
#include "suns/combat.hpp"
#include "suns/deep_space.hpp"
#include "suns/interstellar_medium.hpp"
#include "suns/mobile_base.hpp"
#include "suns/stations.hpp"
#include "suns/survey_probes.hpp"

#include <algorithm>
#include <limits>
#include <tuple>

namespace suns::computer_detail {
namespace {
constexpr std::uint32_t kSourceReserve = 200, kDepartureMinimum = 50;
constexpr double kSourceSurplus = 300, kDestinationShortage = 100;
constexpr std::size_t kAutomaticLineLimit = 8;

double stock(const Planet& p, TransportCargo cargo)
{
    if (cargo == TransportCargo::Ironium) return p.minerals.ironium;
    if (cargo == TransportCargo::Boranium) return p.minerals.boranium;
    return p.minerals.germanium;
}

bool pending_fleet_order(const Order& order, FleetId fleet)
{
    return std::visit([&](const auto& o) {
        using T = std::decay_t<decltype(o)>;
        if constexpr (std::is_same_v<T, MergeFleetsOrder>) return o.source == fleet || o.destination == fleet;
        else if constexpr (std::is_same_v<T, TransferCargoOrder>) return o.source.fleet == fleet || o.destination.fleet == fleet;
        else if constexpr (requires { o.fleet; }) return o.fleet == fleet;
        return false;
    }, order);
}

// Catalog programmes fly directly. Unlike general expansion routing, a safe
// bypass found by navigation_leg cannot be credited without storing its stops.
std::optional<std::uint8_t> shuttle_warp(const GameState& s, Fleet fleet, Position source, Position destination)
{
    const auto capacity = fleet_fuel_capacity(s, fleet);
    // Budget the heaviest possible load on every leg, without crediting source
    // production, unloading or scooping. Received depots make a repeatable
    // tank-funded cycle possible; the initial approach uses received fuel.
    fleet.minerals.ironium += std::max(0.0, fleet_cargo_capacity(s, fleet) - fleet_cargo_used(s, fleet));
    auto cruise = fleet_max_warp(s, fleet);
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(s, stack.design))
            cruise = std::min(cruise, ship_design_cruise_warp(*design));
    const auto start = fleet.position;
    for (int warp = cruise; warp >= 1; --warp) {
        fleet.position = start; fleet.warp = static_cast<std::uint8_t>(warp);
        if (!fleet_warp_valid(s, fleet, fleet.warp) || fleet_overdrive_damage_rate(s, fleet, fleet.warp) > 1e-8
            || fleet_segment_damage(s, fleet, source) > 1e-6 || leg_fuel(s, fleet, source) > fleet.fuel + 1e-6) continue;
        fleet.position = source;
        if (fleet_segment_damage(s, fleet, destination) > 1e-6) continue;
        const auto outbound = leg_fuel(s, fleet, destination);
        fleet.position = destination;
        if (fleet_segment_damage(s, fleet, source) > 1e-6) continue;
        if (outbound + leg_fuel(s, fleet, source) <= capacity + 1e-6) return fleet.warp;
    }
    return {};
}
}

bool transport_reserved(const PlayerView& view, const ComputerTurnPlan& plan, FleetId fleet)
{
    for (const auto& line : view.state.players.front().transportLines)
        if (std::any_of(line.assignments.begin(), line.assignments.end(), [&](auto a) { return a.fleet == fleet; })) return true;
    return std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const auto& order) {
        const auto* assign = std::get_if<AssignTransportLineOrder>(&order);
        return assign && !assign->remove && assign->fleet == fleet;
    });
}

void plan_transport_lines(const PlayerView& view, ComputerTurnPlan& plan, std::string& reason)
{
    const auto& s = view.state;
    const auto& player = s.players.front();
    // The catalog is durable sent intent, not telemetry. Never resend an
    // assignment merely because a remote fleet still reports its old idle state.
    if (player.transportLines.size() >= kAutomaticLineLimit
        || player.lastTransportLineId == std::numeric_limits<TransportLineId>::max()) return;
    std::vector<const Planet*> colonies;
    for (const auto& p : s.planets)
        if (p.owner == view.player && p.population
            && colony_has_orbital_service(s, p.id, view.player, OrbitalStationModule::RefuelingDepot)) colonies.push_back(&p);

    using Score = std::tuple<double, double, PlanetId, PlanetId, TransportCargo, FleetId>;
    std::optional<Score> best;
    TransportLineSpec chosen;
    FleetId assigned{};
    for (const auto& original : s.fleets) {
        if (original.owner != view.player || original.outpost != OutpostKind::None
            || transport_reserved(view, plan, original.id) || !confirmed_idle(original, 0) || !idle(original)
            || original.damagePercent > 0 || fleet_cargo_used(s, original) > 1e-6
            || fleet_cargo_capacity(s, original) < kDepartureMinimum || fleet_can_colonize(s, original)
            || fleet_can_remote_mine(s, original) || fleet_has_space_weapons(s, original)
            || fleet_has_scanner(s, original) || probe_capable(s, original) || mobile_base_hulls(s, original)
            || fleet_construction_kit(s, original) != OutpostKind::None) continue;
        bool bomb = false;
        for (const auto& stack : fleet_ship_stacks(original))
            if (const auto* design = find_ship_design(s, stack.design))
                for (const auto c : design->components) bomb = bomb || component_spec(c).kind == ShipComponentKind::Bomb;
        if (bomb || std::any_of(plan.nextState.fleetGoals.begin(), plan.nextState.fleetGoals.end(),
                [&](const auto& g) { return g.fleet == original.id; })
            || std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(),
                [&](const auto& o) { return pending_fleet_order(o, original.id); })) continue;
        auto fleet = original; fleet.position = fleet.telemetry.position;
        for (const auto* source : colonies) for (const auto* destination : colonies) {
            const auto* from = find_star(s, source->star); const auto* to = find_star(s, destination->star);
            if (source == destination || !from || !to || same_position(from->position, to->position)) continue;
            for (const auto cargo : {TransportCargo::Ironium, TransportCargo::Boranium, TransportCargo::Germanium}) {
                if (stock(*source, cargo) < kSourceSurplus || stock(*destination, cargo) >= kDestinationShortage) continue;
                // One intended supplier per destination/mineral, including
                // paused or missing contacts: these are not failed deliveries.
                if (std::any_of(player.transportLines.begin(), player.transportLines.end(), [&](const auto& l) {
                    return l.spec.destination == destination->id && l.spec.cargo == cargo;
                })) continue;
                const Score score{stock(*destination, cargo), distance_between(fleet.position, from->position)
                    + 2 * distance_between(from->position, to->position), source->id, destination->id, cargo, fleet.id};
                if (best && score >= *best) continue;
                const auto warp = shuttle_warp(s, fleet, from->position, to->position);
                if (!warp) continue;
                TransportLineSpec spec{player.lastTransportLineId + 1, 1,
                    "AI supply " + std::to_string(source->id) + " to " + std::to_string(destination->id)
                        + " " + (cargo == TransportCargo::Ironium ? "I" : cargo == TransportCargo::Boranium ? "B" : "G"),
                    source->id, destination->id, *warp, cargo, true, kSourceReserve, kDepartureMinimum};
                if (!transport_line_error(view, spec).empty()) continue;
                chosen = std::move(spec); assigned = fleet.id; best = score;
            }
        }
    }
    if (!best) return;
    plan.orders.orders.push_back(SetTransportLineOrder{chosen});
    plan.orders.orders.push_back(AssignTransportLineOrder{chosen.id, assigned});
    explain(reason, "Assign paid freighter " + std::to_string(assigned) + " to " + chosen.name
        + "; reserve 200 kt, depart with 50 kt; await received telemetry");
}
} // namespace suns::computer_detail
