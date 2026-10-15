#include "suns/stations.hpp"
#include "suns/communications.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace suns {
const StationHullSpec& station_hull_spec(OrbitalStationHullType hull)
{
    // Initial Suns! station balance, separate from the reference ship hulls.
    static const std::array<StationHullSpec,3> specs{{
        {OrbitalStationHullType::OrbitalDock,"Orbital Dock",0,2,2,2,400,24,{12,6,8}},
        {OrbitalStationHullType::Starbase,"Starbase",9,6,6,4,1600,160,{70,40,60}},
        {OrbitalStationHullType::Fortress,"Fortress",16,12,12,6,6000,500,{200,160,180}},
    }};
    const auto i = static_cast<std::size_t>(hull);
    if (i >= specs.size()) throw std::invalid_argument("Unknown station hull");
    return specs[i];
}
bool station_component_supported(ShipComponentType component)
{
    const auto s = component_spec(component);
    return component == ShipComponentType::JumpGate
        || s.kind == ShipComponentKind::BeamWeapon || s.kind == ShipComponentKind::Torpedo
        || s.kind == ShipComponentKind::Armor || s.kind == ShipComponentKind::Shield
        || (s.kind == ShipComponentKind::Electrical && (s.initiativeBonus || s.accuracyBonusPercent
            || s.jammingPercent || s.beamBonusPercent || s.beamDeflectionPercent || s.battleMovementPenalty));
}
bool station_loadout_valid(const StationLoadout& fit)
{
    if (static_cast<unsigned>(fit.hull) > static_cast<unsigned>(OrbitalStationHullType::Fortress)) return false;
    const auto& h = station_hull_spec(fit.hull);
    if (fit.components.size() > std::size_t(h.weapons+h.defenses+h.electronics)) return false;
    unsigned weapons=0, defenses=0, electronics=0;
    for (auto c : fit.components) {
        if (static_cast<unsigned>(c) > static_cast<unsigned>(ShipComponentType::Superlatanium)
            || !station_component_supported(c)) return false;
        const auto kind = component_spec(c).kind;
        if (kind == ShipComponentKind::BeamWeapon || kind == ShipComponentKind::Torpedo) ++weapons;
        else if (kind == ShipComponentKind::Armor || kind == ShipComponentKind::Shield) ++defenses;
        else ++electronics;
    }
    const auto gates = std::count(fit.components.begin(), fit.components.end(), ShipComponentType::JumpGate);
    return weapons <= h.weapons && defenses <= h.defenses && electronics <= h.electronics
        && gates <= 1 && (!gates || fit.hull != OrbitalStationHullType::OrbitalDock);
}
bool station_loadout_available(const GameState& s, PlayerId owner, const StationLoadout& fit)
{
    const auto* p = find_player(s,owner);
    return p && station_loadout_valid(fit)
        && p->technology.levels[std::size_t(ResearchField::Construction)] >= station_hull_spec(fit.hull).construction
        && std::all_of(fit.components.begin(),fit.components.end(),[&](auto c) {
            if (c == ShipComponentType::JumpGate)
                return p->technology.levels[std::size_t(ResearchField::Energy)] >= kStargateEnergyLevel
                    && p->technology.levels[std::size_t(ResearchField::Electronics)] >= kStargateElectronicsLevel;
            return component_available_to_player(s,owner,c);
        });
}
bool stargate_station_available(const GameState& s, PlayerId owner, PlanetId id)
{
    const auto planet = std::find_if(s.planets.begin(), s.planets.end(),
        [=](const Planet& p) { return p.id == id && p.owner == owner; });
    const auto station = find_orbital_station_at_planet(s, id);
    return planet != s.planets.end() && station && station->owner == owner
        && station->damagePercent < 100.0
        && std::count(station->components.begin(), station->components.end(),
            ShipComponentType::JumpGate) == 1;
}
bool stargate_transfer_valid(const GameState& s, PlayerId owner, FleetId fleetId,
    PlanetId source, PlanetId destination)
{
    if (!source || !destination || source == destination
        || !stargate_station_available(s, owner, source)
        || !stargate_station_available(s, owner, destination)) return false;
    const auto atPlanet = [&](PlanetId id) -> const Planet* {
        const auto p = std::find_if(s.planets.begin(), s.planets.end(),
            [id](const Planet& candidate) { return candidate.id == id; });
        return p == s.planets.end() ? nullptr : &*p;
    };
    const auto* a = atPlanet(source);
    const auto* b = atPlanet(destination);
    const auto* start = a ? find_star(s, a->star) : nullptr;
    const auto* finish = b ? find_star(s, b->star) : nullptr;
    const auto fleet = std::find_if(s.fleets.begin(), s.fleets.end(),
        [=](const Fleet& f) { return f.id == fleetId && f.owner == owner; });
    if (!start || !finish || fleet == s.fleets.end()
        || !same_position(fleet->position, start->position)
        || fleet->destination || fleet->targetFleet || !fleet->waypointQueue.empty()
        || !fleet->pendingCommands.empty() || !fleet_has_instant_link(s, *fleet)
        || fleet->colonists || mineral_cargo_mass(fleet->minerals) > 0.000001
        || fleet_gross_mass(s, *fleet) > kStargateMaximumMassKt) return false;
    return std::hypot(start->position.x - finish->position.x,
        start->position.y - finish->position.y) <= kStargateRangeLy;
}
bool transfer_fleet_through_stargate(GameState& s, PlayerId owner, FleetId fleetId,
    PlanetId source, PlanetId destination)
{
    if (!stargate_transfer_valid(s, owner, fleetId, source, destination)) return false;
    const auto* p = std::find_if(s.planets.begin(), s.planets.end(),
        [=](const Planet& candidate) { return candidate.id == destination; }).operator->();
    const auto* star = find_star(s, p->star);
    auto fleet = std::find_if(s.fleets.begin(), s.fleets.end(),
        [=](const Fleet& candidate) { return candidate.id == fleetId && candidate.owner == owner; });
    fleet->position = star->position;
    // All fuel stays onboard. Physical movement precedes the normal telemetry
    // stage; no instant report of a remote destination is manufactured here.
    return true;
}
StationLoadout station_loadout(const OrbitalStation& s)
{ return {s.hull,s.components,orbital_station_has_module(s,OrbitalStationModule::Shipyard),orbital_station_has_module(s,OrbitalStationModule::RefuelingDepot)}; }
std::uint32_t station_build_cost(const StationLoadout& fit)
{
    auto cost = station_hull_spec(fit.hull).cost;
    // Basic dock prices already include both services; removing a service
    // does not refund a hull or generate free refit material.
    for (auto c : fit.components) cost += component_spec(c).buildCost;
    return cost;
}
MineralCargo station_mineral_cost(const StationLoadout& fit)
{
    auto cost = station_hull_spec(fit.hull).minerals;
    for (auto c : fit.components) {
        const auto m = component_mineral_cost(c);
        cost.ironium += m.ironium; cost.boranium += m.boranium; cost.germanium += m.germanium;
    }
    return cost;
}
double station_armor(const OrbitalStation& s)
{
    double armor=station_hull_spec(s.hull).armor;
    for(auto c:s.components) armor+=component_spec(c).armor;
    return armor;
}
double station_shields(const OrbitalStation& s)
{
    double shields=0;
    for(auto c:s.components) shields+=component_spec(c).shields;
    return shields;
}
bool station_has_weapons(const OrbitalStation& s)
{ return std::any_of(s.components.begin(),s.components.end(),[](auto c) { return component_spec(c).weaponPower > 0; }); }
bool station_construction_valid(const GameState& s, const Planet& p, const StationConstruction& work)
{
    if (!p.owner || !station_loadout_available(s,p.owner,work.loadout)) return false;
    const auto* existing = find_orbital_station_at_planet(s,p.id);
    return work.refitStation ? existing && existing->id == work.refitStation && existing->owner == p.owner : !existing;
}
bool queue_station_construction(GameState& s, PlayerId owner, const QueueStationOrder& order)
{
    const auto p=std::find_if(s.planets.begin(),s.planets.end(),[&](const auto& p) { return p.id==order.colony && p.owner==owner; });
    if(p==s.planets.end() || !station_construction_valid(s,*p,order.construction)
        || std::any_of(p->productionQueue.begin(),p->productionQueue.end(),[](const auto& i) { return i.kind==ProductionKind::OrbitalStation; })) return false;
    ProductionItem item{ProductionKind::OrbitalStation,station_build_cost(order.construction.loadout)};
    item.station=order.construction; p->productionQueue.push_back(std::move(item)); return true;
}
bool complete_station_construction(GameState& s, const Planet& p, const StationConstruction& work)
{
    if(!station_construction_valid(s,p,work)) return false;
    auto existing=std::find_if(s.orbitalStations.begin(),s.orbitalStations.end(),[&](const auto& o) { return o.planet==p.id; });
    OrbitalStation station;
    station.id=work.refitStation ? work.refitStation : s.nextOrbitalStationId++;
    station.owner=p.owner; station.planet=p.id; station.hull=work.loadout.hull;
    station.name=p.name+" "+station_hull_spec(station.hull).name;
    station.components=work.loadout.components;
    if(work.loadout.shipyard) station.modules.push_back(OrbitalStationModule::Shipyard);
    if(work.loadout.refueling) station.modules.push_back(OrbitalStationModule::RefuelingDepot);
    if(existing==s.orbitalStations.end()) s.orbitalStations.push_back(std::move(station));
    else *existing=std::move(station);
    return true;
}
const OrbitalStation* known_orbital_station(const GameState& s,PlayerId owner,PlanetId planet)
{
    const PendingStationContact* earliest=nullptr;
    for(const auto& c:s.pendingStationContacts) if(c.lastContact.owner==owner && c.lastContact.planet==planet && c.deliveryTurn>s.turn)
        if(!earliest || c.observedTurn<earliest->observedTurn) earliest=&c;
    if(earliest) return &earliest->lastContact;
    const auto* station=find_orbital_station_at_planet(s,planet);
    return station && station->owner==owner ? station : nullptr;
}
bool colony_defenses_available(const GameState& s,PlayerId owner)
{ const auto* p=find_player(s,owner); return p && p->technology.levels[std::size_t(ResearchField::Energy)]>=1; }
double colony_defense_coverage(const Planet& p)
{ return 1-std::pow(0.97,std::min(p.defenses,kMaximumColonyDefenses)); }
} // namespace suns
