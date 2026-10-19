#include "suns/intergalactic_visitors.hpp"
#include "suns/communications.hpp"
#include "suns/interstellar_medium.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/survey_probes.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace suns {
Position visitor_prediction(const VisitorObservation& x, double turn)
{ const auto dt = turn - double(x.observedTurn); return {x.position.x + dt*x.velocity.x, x.position.y + dt*x.velocity.y}; }
bool visitor_observation_valid(const VisitorObservation& x)
{
    const auto finite = [](Position p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e9 && std::abs(p.y) <= 1e9; };
    return x.id && x.id <= 0x7fffffffU && finite(x.position) && finite(x.velocity)
        && distance_between({}, x.velocity) > 0 && distance_between({}, x.velocity) <= 200
        && x.observedTurn && x.departureTurn && x.departureTurn <= 1'000'000'000ULL
        && (!x.receivedTurn || x.receivedTurn >= x.observedTurn)
        && (x.departed ? x.observedTurn >= x.departureTurn && !x.available : x.observedTurn < x.departureTurn);
}
const VisitorObservation* known_visitor(const GameState& s, PlayerId owner, VisitorId id)
{
    const auto* p = find_player(s, owner); if (!p) return nullptr;
    for (const auto& x : p->visitorKnowledge) if (x.id == id) return &x;
    return nullptr;
}
namespace {
VisitorObservation snapshot(const IntergalacticVisitor& v, std::uint64_t turn, Position position)
{ return {v.id, position, v.velocity, turn, 0, v.departureTurn, !v.customer && turn < v.departureTurn, turn >= v.departureTurn}; }
bool sees(const GameState& s, const Fleet& f, Position position)
{
    if (!fleet_has_scanner(s, f) || !fleet_active_sensors(f, s.turn)) return false;
    return distance_between(f.position, position) <= std::max(1.0, strategic_sensor_range(s, f, s.turn))
        * medium_active_path_factor(s, f.position, position) + 1e-6;
}
void report(GameState& s, PlayerId owner, VisitorObservation x, VisitorReportKind kind, Position source, FleetId fleet)
{
    auto p = std::find_if(s.players.begin(), s.players.end(), [&](const auto& p) { return p.id == owner; });
    if (p == s.players.end()) return;
    x.receivedTurn = 0;
    const auto turn = kind == VisitorReportKind::Failed ? s.turn+1 : x.observedTurn;
    const auto due = dispatch_report_delivery_turn(s, owner, source, turn, fleet);
    p->pendingVisitorReports.push_back({x, kind, turn, due, due == kAwaitingTransmission ? fleet : 0});
}
void clear_navigation(Fleet& f)
{
    f.destination.reset(); f.targetFleet = 0; f.arrivalAction.reset(); f.waypointQueue.clear();
    f.routeTemplate.clear(); f.repeatOrders = false; f.task = FleetTask::None;
}
}
std::optional<Position> visitor_intercept_destination(const Fleet& f, const VisitorObservation& x, std::uint64_t turn, std::uint8_t warp)
{
    if (!x.id || x.departed || turn >= x.departureTurn || warp < 1 || warp > kMaxWarp) return {};
    const auto target = visitor_prediction(x, double(turn));
    const Position r{target.x-f.position.x, target.y-f.position.y};
    const auto speed = warp_distance(warp);
    const auto a = x.velocity.x*x.velocity.x + x.velocity.y*x.velocity.y - speed*speed;
    const auto b = 2*(r.x*x.velocity.x + r.y*x.velocity.y), c = r.x*r.x+r.y*r.y;
    double time = 1e30;
    if (c <= kFleetEncounterRadius*kFleetEncounterRadius) return f.position;
    if (std::abs(a) < 1e-12) { if (b < 0) time = -c/b; }
    else if (const auto disc = b*b-4*a*c; disc >= 0) {
        for (const auto t : {(-b-std::sqrt(disc))/(2*a), (-b+std::sqrt(disc))/(2*a)}) if (t >= 0) time = std::min(time, t);
    }
    if (time >= double(x.departureTurn-turn) || time >= 1e29) return {};
    const auto meet = visitor_prediction(x, double(turn)+time);
    const auto distance = distance_between(f.position, meet);
    if (distance < 1e-9) return f.position;
    // Preserve a full year's velocity for the ordinary continuous encounter
    // solver. A short destination would incorrectly stretch arrival to t=1.
    const auto length = std::max(distance, speed) + speed;
    return Position{f.position.x+(meet.x-f.position.x)*length/distance, f.position.y+(meet.y-f.position.y)*length/distance};
}
void apply_visitor_command(GameState& s, Fleet& f, const VisitorMission& mission)
{
    if (mission.track.id && (f.outpost != OutpostKind::None || !fleet_warp_valid(s, f, mission.warp))) return;
    pause_probe(f); clear_navigation(f); f.visitor = mission;
    if (mission.track.id) f.warp = mission.warp;
}
bool submit_visitor_command(GameState& s, PlayerId owner, const EncounterVisitorOrder& order)
{
    const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const auto& f) { return f.id == order.fleet && f.owner == owner; });
    if (f == s.fleets.end() || order.warp < 1 || order.warp > kMaxWarp) return false;
    VisitorMission mission; mission.warp = order.warp;
    if (order.visitor) {
        const auto* known = known_visitor(s, owner, order.visitor);
        if (!known || known->receivedTurn > s.turn || !known->available || known->departed || f->outpost != OutpostKind::None) return false;
        mission.track = *known;
    }
    record_command_transmission(s, owner, f->position, s.turn);
    const auto delivery = s.turn + communication_delay_turns(s, owner, f->position);
    if (delivery == s.turn) {
        apply_visitor_command(s, *f, mission);
        if (!order.visitor) f->pendingCommands.clear();
    }
    else { PendingFleetCommand c; c.issuedTurn = s.turn; c.deliveryTurn = delivery; c.visitor = mission; f->pendingCommands.push_back(c); }
    return true;
}
void prepare_visitor_navigation(GameState& s)
{
    if (s.mediumObserver) return;
    for (auto& f : s.fleets) if (f.visitor.track.id) {
        auto& track = f.visitor.track;
        for (const auto& v : s.visitors) if (v.id == track.id && s.turn < v.departureTurn && sees(s, f, v.position)) track = snapshot(v, s.turn, v.position);
        const auto destination = visitor_intercept_destination(f, track, s.turn, f.visitor.warp);
        if (!destination || !track.available || !fleet_warp_valid(s, f, f.visitor.warp)) {
            // Failure is a mission receipt, not a new observation of hidden truth.
            report(s, f.owner, track, VisitorReportKind::Failed, f.position, f.id);
            f.visitor = {}; clear_navigation(f); continue;
        }
        f.destination = *destination; f.warp = f.visitor.warp;
    }
}
void complete_visitor_encounter(GameState& s, Fleet& f, VisitorId id, Position contact)
{
    auto v = std::find_if(s.visitors.begin(), s.visitors.end(), [&](const auto& v) { return v.id == id; });
    if (v == s.visitors.end() || f.visitor.track.id != id || s.turn >= v->departureTurn) return;
    const bool paid = !v->customer && f.minerals.germanium >= kVisitorGermaniumPrice;
    if (paid) { f.minerals.germanium -= kVisitorGermaniumPrice; v->customer = f.owner; }
    auto x = snapshot(*v, s.turn+1, contact);
    // Contact is measured at its physical point during the elapsed year;
    // report the boundary position for a consistent dated trajectory.
    x.position = {v->position.x+v->velocity.x, v->position.y+v->velocity.y};
    report(s, f.owner, x, paid ? VisitorReportKind::Exchanged : VisitorReportKind::Failed, contact, f.id);
    f.visitor = {}; clear_navigation(f);
}
void advance_visitors(GameState& s)
{
    if (s.mediumObserver) return;
    for (auto& v : s.visitors) if (s.turn < v.departureTurn) { v.position.x += v.velocity.x; v.position.y += v.velocity.y; }
    const auto turn = s.turn+1;
    if (turn < 60 || s.stars.size() < 16 || s.visitors.size() >= 128
        || std::any_of(s.visitors.begin(), s.visitors.end(), [&](const auto& v) { return v.departureTurn > turn; })) return;
    std::mt19937_64 rng(s.galaxySeed ^ (turn*0x9e3779b97f4a7c15ULL) ^ 0x56495349544f52ULL);
    if (rng()%80) return;
    const auto width = std::max(200.0, s.wormholeRules.width), height = std::max(200.0, s.wormholeRules.height);
    const auto duration = std::uint64_t(std::ceil((width+300)/121)); const bool reverse = rng()%2;
    s.visitors.push_back({s.nextVisitorId++, {(reverse?1:-1)*(width/2+150), (double(rng()%10001)/10000-0.5)*height*0.8},
        {(reverse?-1:1)*(width+300)/double(duration), 0}, turn, turn+duration, 0});
}
void observe_visitors(GameState& s)
{
    if (s.mediumObserver) return;
    for (const auto& v : s.visitors) if (s.turn <= v.departureTurn) for (auto& p : s.players) {
        bool observed = false; std::uint64_t due = kAwaitingTransmission; FleetId fleet = 0; Position source;
        for (const auto& f : s.fleets) if (f.owner == p.id && sees(s, f, v.position)) {
            const auto candidate = fleet_report_delivery_turn(s, p.id, f.position, s.turn, f.id);
            if (!observed || candidate < due || (candidate == due && f.id < fleet)) { observed = true; due = candidate; fleet = f.id; source = f.position; }
        }
        for (const auto& colony : s.planets) if (colony.owner == p.id && colony.population && colony.radar != PlanetaryRadarLevel::None) {
            const auto* star = find_star(s, colony.star); if (!star) continue;
            const auto range = planetary_radar_range(colony.radar)*(1-sensor_interference(s,p.id,star->position,s.turn))*medium_active_sensor_factor(s,star->position);
            if (distance_between(star->position, v.position) > range*medium_active_path_factor(s,star->position,v.position)) continue;
            const auto candidate = fleet_report_delivery_turn(s, p.id, star->position, s.turn, 0);
            if (!observed || candidate <= due) { observed = true; due = candidate; fleet = 0; source = star->position; }
        }
        if (observed) report(s, p.id, snapshot(v, s.turn, v.position), s.turn == v.departureTurn ? VisitorReportKind::Departed : VisitorReportKind::Sighted, source, fleet);
    }
}
std::vector<GameEvent> deliver_visitor_reports(GameState& s)
{
    dispatch_fleet_report_outbox(s);
    std::vector<GameEvent> events;
    for (auto& p : s.players) {
        std::stable_sort(p.pendingVisitorReports.begin(),p.pendingVisitorReports.end(),[](const auto& a,const auto& b){
            if (a.deliveryTurn!=b.deliveryTurn) return a.deliveryTurn<b.deliveryTurn;
            if (a.observation.observedTurn!=b.observation.observedTurn) return a.observation.observedTurn<b.observation.observedTurn;
            if (a.observation.id!=b.observation.id) return a.observation.id<b.observation.id;
            return a.kind<b.kind;
        });
        for (const auto& r : p.pendingVisitorReports) if (r.deliveryTurn <= s.turn) {
            auto x = r.observation; x.receivedTurn = s.turn;
            auto old = std::find_if(p.visitorKnowledge.begin(),p.visitorKnowledge.end(),[&](const auto& a){return a.id==x.id;});
            const bool newer = old==p.visitorKnowledge.end() || x.observedTurn>old->observedTurn
                || (x.observedTurn==old->observedTurn && ((old->available && !x.available) || (!old->departed && x.departed)));
            const bool changed = newer && (old==p.visitorKnowledge.end() || x.available!=old->available || x.departed!=old->departed);
            // Refusals can carry a real contact measurement, while navigation
            // failures retain their old track. Freshness uses that evidence's
            // date, never the later mission-status date in r.observedTurn.
            if (newer) {
                if (old==p.visitorKnowledge.end()) p.visitorKnowledge.push_back(x);
                else *old=x;
            }
            if (r.kind==VisitorReportKind::Exchanged) {
                if (std::find(p.acquiredEquipment.begin(),p.acquiredEquipment.end(),kVisitorGift)!=p.acquiredEquipment.end()) continue;
                p.acquiredEquipment.push_back(kVisitorGift);
            } else if (r.kind==VisitorReportKind::Sighted && !changed) continue;
            GameEvent e; e.kind = r.kind==VisitorReportKind::Exchanged ? GameEventKind::VisitorExchangeCompleted
                : r.kind==VisitorReportKind::Failed ? GameEventKind::VisitorExchangeFailed
                : r.kind==VisitorReportKind::Departed ? GameEventKind::VisitorDeparted : GameEventKind::VisitorSighted;
            e.id = 1469598103934665603ULL;
            for (const auto value : {std::uint64_t{0x56495349544f52ULL}, std::uint64_t(x.id), r.observedTurn,
                    std::uint64_t(p.id), std::uint64_t(r.kind)})
                e.id = (e.id ^ value)*1099511628211ULL;
            e.recipient=p.id; e.turn=s.turn; e.observedTurn=r.observedTurn; e.position=x.position;
            e.quantity=x.id;
            if (r.kind==VisitorReportKind::Failed) e.severity=GameEventSeverity::Warning;
            events.push_back(e);
        }
        std::erase_if(p.pendingVisitorReports,[&](const auto& r){
            return r.deliveryTurn<=s.turn || (r.deliveryTurn==kAwaitingTransmission && r.transmissionFleet
                && std::none_of(s.fleets.begin(),s.fleets.end(),[&](const auto& f){return f.id==r.transmissionFleet && f.owner==p.id;}));
        });
    }
    return events;
}
GameState make_visitor_demo()
{
    auto s=make_demo_game(); s.turn=60; s.wormholeRules.spawnChancePerTurn=0;
    auto& p=s.players[0]; p.technology.levels.fill(16); p.technology.researchActive=false;
    s.planets[0].productionQueue.clear(); s.planets[0].minerals={1000,1000,1000};
    ShipDesign d{s.nextShipDesignId++,1,"Visitor rendezvous freighter",ShipHullType::SmallFreighter,{ShipComponentType::TransGalacticDrive,ShipComponentType::MoleScanner}};
    s.shipDesigns.push_back(d); s.fleets.resize(1); auto& f=s.fleets[0]; f.design=d.id; f.ships={{d.id,1}};
    f.name="Rendezvous courier"; f.position={0,0}; f.destination.reset(); f.arrivalAction.reset(); f.waypointQueue.clear();
    f.colonists=0; f.minerals={0,0,50}; f.fuel=fleet_fuel_capacity(s,f); f.warp=8; f.telemetry={};
    s.visitors={{1,{-140,0},{121,0},60,66,0}}; s.nextVisitorId=2;
    publish_fleet_telemetry(s,s.turn); deliver_due_fleet_telemetry(s); observe_visitors(s); (void)deliver_visitor_reports(s);
    return s;
}
} // namespace suns
