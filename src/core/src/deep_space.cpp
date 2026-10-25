#include "suns/deep_space.hpp"
#include "suns/communications.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/stations.hpp"
#include "suns/player_knowledge.hpp"
#include <algorithm>
#include <cmath>

namespace suns {
OutpostKind construction_kit_kind(ShipComponentType component)
{
    if (component == ShipComponentType::OrbitalShipyardKit) return OutpostKind::OrbitalShipyard;
    if (component == ShipComponentType::FuelDepotKit) return OutpostKind::FuelDepot;
    if (component == ShipComponentType::DeepSpaceRelayKit) return OutpostKind::CommunicationsRelay;
    return OutpostKind::None;
}
OutpostKind fleet_construction_kit(const GameState& state, const Fleet& fleet)
{
    if (fleet_ship_count(fleet) != 1) return OutpostKind::None;
    OutpostKind result = OutpostKind::None;
    for (const auto& stack : fleet_ship_stacks(fleet)) if (const auto* design = find_ship_design(state, stack.design))
        for (auto component : design->components) if (const auto kind = construction_kit_kind(component); kind != OutpostKind::None) {
            if (result != OutpostKind::None) return OutpostKind::None;
            result = kind;
        }
    return result;
}
bool outpost_online(const Fleet& fleet)
{
    return fleet.outpost != OutpostKind::None && fleet.damagePercent < 100
        && std::isfinite(fleet.fuel) && fleet.fuel > 0;
}
std::string outpost_kind_name(OutpostKind kind)
{
    switch (kind) {
    case OutpostKind::OrbitalShipyard: return "Orbital shipyard";
    case OutpostKind::FuelDepot: return "Fuel depot";
    case OutpostKind::CommunicationsRelay: return "Communications relay";
    default: return "Mobile constructor";
    }
}
std::string outpost_deployment_error(const GameState& state, const Fleet& fleet, Position position)
{
    if (fleet.outpost != OutpostKind::None) return "This outpost is already permanently deployed.";
    if (fleet_construction_kit(state, fleet) == OutpostKind::None) return "Deploy one ship fitted with exactly one outpost construction kit; split mixed fleets first.";
    if (fleet_construction_kit(state, fleet) == OutpostKind::OrbitalShipyard) return "Use Orbital shipyards to select the destination planet and deploy.";
    if (fleet.damagePercent >= 100 || fleet.colonists) return "Deployment requires an operational hull with no colonists aboard.";
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) return "Deployment needs a finite position.";
    for (const auto& star : state.stars) if (distance_between(star.position, position) < kOutpostStellarClearanceLy)
        return "Deep-space outposts need at least 5 ly clearance from a star; use orbital stations at planets.";
    return {};
}
bool outpost_state_valid(const GameState& state, const Fleet& fleet)
{
    if (fleet.outpost > OutpostKind::OrbitalShipyard) return false;
    if (fleet.outpost == OutpostKind::OrbitalShipyard) {
        // Validate physical anchoring here; the trailing ORBI section validates
        // the independently dated construction/crew state after it has loaded.
        return fleet_construction_kit(state, fleet) == OutpostKind::OrbitalShipyard
            && !fleet.destination && !fleet.targetFleet && fleet.waypointQueue.empty()
            && !fleet.arrivalAction && !fleet.repeatOrders && fleet.routeTemplate.empty()
            && fleet.task == FleetTask::None;
    }
    if (fleet.outpost == OutpostKind::None) return true;
    auto constructor = fleet; constructor.outpost = OutpostKind::None;
    // A disabled installation still exists; it simply has no active services.
    constructor.damagePercent = std::min(constructor.damagePercent, 99.0);
    return outpost_deployment_error(state, constructor, fleet.position).empty()
        && fleet_construction_kit(state, fleet) == fleet.outpost
        && !fleet.destination && !fleet.arrivalAction && !fleet.targetFleet && fleet.waypointQueue.empty()
        && fleet.routeTemplate.empty() && !fleet.repeatOrders && fleet.task == FleetTask::None
        && std::isfinite(fleet.fuel) && fleet.fuel >= 0 && fleet.fuel <= fleet_fuel_capacity(state, fleet) + 1e-6;
}
std::vector<Fleet> known_outposts(const GameState& state, PlayerId owner)
{
    std::vector<Fleet> result;
    for (const auto& fleet : state.fleets) if (fleet.owner == owner) {
        auto known = fleet_player_view(state, fleet);
        if (known.outpost != OutpostKind::None) result.push_back(std::move(known));
    }
    for (auto contact : missing_fleet_contacts(state, owner)) if (contact.outpost != OutpostKind::None
        && std::none_of(result.begin(), result.end(), [&](const auto& f) { return f.id == contact.id; }))
        result.push_back(std::move(contact));
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return result;
}
void maintain_outposts(GameState& state, std::uint64_t turn)
{
    std::vector<FleetId> exhausted;
    for (auto& fleet : state.fleets) if (fleet.outpost != OutpostKind::None && fleet.outpost != OutpostKind::OrbitalShipyard && fleet.fuel > 0) {
        fleet.fuel = std::max(0.0, fleet.fuel - kOutpostUpkeepMg);
        if (fleet.fuel == 0) exhausted.push_back(fleet.id);
    }
    // Every service observes the same post-upkeep network. Queueing during
    // deduction could route an earlier warning through a later exhausted node.
    std::sort(exhausted.begin(), exhausted.end());
    for (const auto id : exhausted) {
        const auto& fleet = *std::find_if(state.fleets.begin(), state.fleets.end(), [=](const auto& f) { return f.id == id; });
        queue_player_report(state, fleet.owner, PlayerReportKind::OutpostFuelExhausted,
            fleet.position, turn, 0, 0, fleet.id, 0, ProductionKind::ColonyShip, std::uint32_t(fleet.outpost));
    }
}
void deploy_outposts(GameState& state, std::uint64_t turn)
{
    std::vector<FleetId> builders;
    for (const auto& fleet : state.fleets) if (fleet.task == FleetTask::DeployOutpost) builders.push_back(fleet.id);
    std::sort(builders.begin(), builders.end());
    for (const auto id : builders) {
        auto& fleet = *std::find_if(state.fleets.begin(), state.fleets.end(), [=](const auto& f) { return f.id == id; });
        const auto kind = fleet_construction_kit(state, fleet);
        bool valid = !fleet.destination && !fleet.targetFleet && fleet.waypointQueue.empty()
            && !fleet.repeatOrders && outpost_deployment_error(state, fleet, fleet.position).empty();
        for (const auto& other : state.fleets) if (other.id != fleet.id && other.outpost != OutpostKind::None
            && distance_between(other.position, fleet.position) < 1.0) valid = false;
        fleet.task = FleetTask::None;
        if (valid) {
            fleet.outpost = fleet_construction_kit(state, fleet);
            fleet.arrivalAction.reset(); fleet.routeTemplate.clear();
        }
        queue_player_report(state, fleet.owner, valid ? PlayerReportKind::OutpostDeployed : PlayerReportKind::OutpostDeploymentFailed,
            fleet.position, turn, 0, 0, fleet.id, 0, ProductionKind::ColonyShip, std::uint32_t(kind));
    }
}
namespace {
Fleet* depot_at(GameState& state, const Fleet& visitor)
{
    Fleet* result = nullptr;
    for (auto& candidate : state.fleets) if (candidate.id != visitor.id && candidate.owner == visitor.owner
        && (candidate.outpost == OutpostKind::FuelDepot || (candidate.outpost == OutpostKind::OrbitalShipyard
            && candidate.orbitalBase.supported && candidate.orbitalBase.assemblyYears == kOrbitalAssemblyYears)) && candidate.damagePercent < 100
        && same_position(candidate.position, visitor.position) && (!result || candidate.id < result->id)) result = &candidate;
    return result;
}
double transfer(Fleet& source, Fleet& destination, double amount)
{
    if (!std::isfinite(amount) || amount <= 0) return 0;
    source.fuel -= amount; destination.fuel += amount; return amount;
}
}
double refuel_from_outpost(GameState& state, Fleet& visitor)
{
    if (visitor.outpost != OutpostKind::None) return 0;
    auto* depot = depot_at(state, visitor);
    if (!depot || !outpost_online(*depot)) return 0;
    const auto amount = transfer(*depot, visitor, std::min(std::max(0.0, depot->fuel - (depot->outpost == OutpostKind::OrbitalShipyard ? depot->mobileBase.fuelReserve : 0.0)),
        std::max(0.0, fleet_fuel_capacity(state, visitor) - visitor.fuel)));
    if (amount > 0 && depot->fuel == 0) queue_player_report(state, depot->owner, PlayerReportKind::OutpostFuelExhausted,
        depot->position, state.turn + 1, 0, 0, depot->id, 0, ProductionKind::ColonyShip, std::uint32_t(depot->outpost));
    return amount;
}
double supply_outpost(GameState& state, Fleet& tanker, double reserve)
{
    if (tanker.outpost != OutpostKind::None || !std::isfinite(reserve) || reserve < 0) return 0;
    // Relays also accept physical fuel deliveries for their operating reserve.
    Fleet* receiver = nullptr;
    for (auto& candidate : state.fleets) if (candidate.id != tanker.id && candidate.owner == tanker.owner
        && candidate.outpost != OutpostKind::None && candidate.damagePercent < 100
        && same_position(candidate.position, tanker.position) && (!receiver || candidate.id < receiver->id)) receiver = &candidate;
    if (!receiver) return 0;
    return transfer(tanker, *receiver, std::min(std::max(0.0, tanker.fuel - reserve),
        std::max(0.0, fleet_fuel_capacity(state, *receiver) - receiver->fuel)));
}
} // namespace suns
