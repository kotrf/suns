#include "suns/intergalactic_visitors.hpp"
#include "suns/communications.hpp"
#include "suns/interstellar_medium.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/survey_probes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>

namespace suns {
namespace {
constexpr std::array<VisitorOffer, 3> offers{{
    {VisitorProfile::Freight, "Freight envoy", "We carry designs for holds larger than they seem.", {0,0,50}, ShipComponentType::MultiCargoPod},
    {VisitorProfile::Prospector, "Prospector", "Your ores for the tools that reach them.", {40,20,0}, ShipComponentType::AlienMiner},
    {VisitorProfile::Shieldwright, "Shieldwright", "Bring the alloys. Take the pattern of a stronger shell.", {25,25,15}, ShipComponentType::LangstonShell},
}};
}
std::span<const VisitorOffer> visitor_offers() { return offers; }
const VisitorOffer& visitor_offer(VisitorProfile profile) { return offers.at(std::size_t(profile)); }
bool visitor_gift_valid(ShipComponentType gift)
{ return std::any_of(offers.begin(), offers.end(), [gift](const auto& offer) { return offer.gift == gift; }); }
Position visitor_prediction(const VisitorObservation& x, double turn)
{ const auto dt = turn - double(x.observedTurn); return {x.position.x + dt*x.velocity.x, x.position.y + dt*x.velocity.y}; }
bool visitor_observation_valid(const VisitorObservation& x)
{
    const auto finite = [](Position p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e9 && std::abs(p.y) <= 1e9; };
    return std::size_t(x.profile) < offers.size() && x.id && x.id <= 0x7fffffffU && finite(x.position) && finite(x.velocity)
        && distance_between({}, x.velocity) > 0 && distance_between({}, x.velocity) <= 200
        && x.observedTurn && x.departureTurn && x.departureTurn <= kMaxVisitorTurn
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
{ return {v.id, position, v.velocity, turn, 0, v.departureTurn, !v.customer && turn < v.departureTurn, turn >= v.departureTurn, v.profile, v.courseChanged}; }
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
IntergalacticVisitor crossing(VisitorId id, std::uint64_t turn, double width, double height, std::mt19937_64& rng)
{
    const auto coordinate=[&](double span) { return (double(rng()%10001)/10000-0.5)*span; };
    const double hx=width/2+150, hy=height/2+150;
    const auto side=rng()%4;
    Position start;
    if(side<2) start={side==0?-hx:hx,coordinate(height*0.8)};
    else start={coordinate(width*0.8),side==2?-hy:hy};
    // Aim through the central half of the real galaxy, then extend the ray to
    // a different outer edge. This avoids short, uninteresting corner grazes.
    const Position aim{coordinate(width*0.5),coordinate(height*0.5)};
    const Position delta{aim.x-start.x,aim.y-start.y};
    double scale=std::numeric_limits<double>::infinity();
    if(delta.x!=0) scale=std::min(scale,((delta.x>0?hx:-hx)-start.x)/delta.x);
    if(delta.y!=0) scale=std::min(scale,((delta.y>0?hy:-hy)-start.y)/delta.y);
    const Position travel{delta.x*scale,delta.y*scale};
    const auto duration=std::uint64_t(std::ceil(distance_between({},travel)/121));
    IntergalacticVisitor v{id,start,{travel.x/double(duration),travel.y/double(duration)},turn,turn+duration,0,
        VisitorProfile(rng()%offers.size())};
    // One in ten new visitors may turn once, at an interior year boundary.
    // The schedule is physical host state, independent of customers/pursuers.
    if(rng()%10==0) {
        std::vector<std::uint64_t> candidates;
        for(std::uint64_t age=1;age+1<duration;++age) {
            const Position p{start.x+v.velocity.x*double(age),start.y+v.velocity.y*double(age)};
            if(std::abs(p.x)<=width/2 && std::abs(p.y)<=height/2) candidates.push_back(turn+age);
        }
        if(!candidates.empty()) v.courseChangeTurn=candidates[rng()%candidates.size()];
    }
    return v;
}
struct RayExit { double time; unsigned side; };
RayExit ray_exit(Position p, Position direction, double hx, double hy)
{
    const auto tx=direction.x==0 ? std::numeric_limits<double>::infinity()
        : ((direction.x>0?hx:-hx)-p.x)/direction.x;
    const auto ty=direction.y==0 ? std::numeric_limits<double>::infinity()
        : ((direction.y>0?hy:-hy)-p.y)/direction.y;
    return tx<=ty ? RayExit{tx,direction.x>0?1U:0U} : RayExit{ty,direction.y>0?3U:2U};
}
struct ChangedCourse { Position velocity; std::uint64_t departureTurn; };
std::optional<ChangedCourse> changed_course(const IntergalacticVisitor& v, Position position,
    std::uint64_t seed, double width, double height)
{
    if(!std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0
        || !std::isfinite(position.x) || !std::isfinite(position.y)
        || std::abs(position.x)>width/2+1e-6 || std::abs(position.y)>height/2+1e-6
        || !std::isfinite(v.velocity.x) || !std::isfinite(v.velocity.y)
        || !(std::hypot(v.velocity.x,v.velocity.y)>0) || v.courseChangeTurn>=kMaxVisitorTurn) return {};
    const double hx=width/2+150,hy=height/2+150;
    // Before the one allowed turn, the incoming segment is still straight.
    // Trace it backwards to recover its entrance, including pending v114 saves.
    const auto entrance=ray_exit(position,{-v.velocity.x,-v.velocity.y},hx,hy);
    if(!std::isfinite(entrance.time) || entrance.time<=0) return {};
    std::mt19937_64 rng(seed ^ (std::uint64_t(v.id)*0x9e3779b97f4a7c15ULL)
        ^ v.arrivalTurn ^ 0x434f55525345ULL);
    const auto angle=(20.0+double(rng()%26))*std::numbers::pi/180*(rng()%2?1:-1);
    for(const auto turnAngle:{angle,-angle}) {
        const Position direction{v.velocity.x*std::cos(turnAngle)-v.velocity.y*std::sin(turnAngle),
            v.velocity.x*std::sin(turnAngle)+v.velocity.y*std::cos(turnAngle)};
        const auto exit=ray_exit(position,direction,hx,hy);
        const auto years=std::ceil(exit.time);
        if(exit.side==entrance.side || !std::isfinite(years) || years<1
            || years>double(kMaxVisitorTurn-v.courseChangeTurn)) continue;
        // The bound is checked in floating point before conversion or addition.
        const auto duration=std::uint64_t(years);
        const Position velocity{direction.x*(exit.time/years),direction.y*(exit.time/years)};
        if(!std::isfinite(velocity.x) || !std::isfinite(velocity.y)
            || !(std::hypot(velocity.x,velocity.y)>0)) continue;
        return ChangedCourse{velocity,v.courseChangeTurn+duration};
    }
    return {};
}
}
bool visitor_course_schedule_valid(const GameState& s, const IntergalacticVisitor& v)
{
    if(!v.courseChangeTurn) return !v.courseChanged;
    if(v.courseChangeTurn<=v.arrivalTurn || v.courseChangeTurn>=v.departureTurn
        || v.courseChangeTurn>=kMaxVisitorTurn) return false;
    if(v.courseChanged) return v.courseChangeTurn<=s.turn;
    if(v.courseChangeTurn<=s.turn) return false;
    const auto dt=double(v.courseChangeTurn-s.turn);
    const Position boundary{v.position.x+v.velocity.x*dt,v.position.y+v.velocity.y*dt};
    return changed_course(v,boundary,s.galaxySeed,s.wormholeRules.width,s.wormholeRules.height).has_value();
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
    const auto price = visitor_offer(v->profile).request;
    const bool paid = !v->customer && f.minerals.ironium >= price.ironium
        && f.minerals.boranium >= price.boranium && f.minerals.germanium >= price.germanium;
    if (paid) {
        f.minerals.ironium -= price.ironium; f.minerals.boranium -= price.boranium;
        f.minerals.germanium -= price.germanium; v->customer = f.owner;
    }
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
    const auto turn = s.turn+1;
    for (auto& v : s.visitors) if (s.turn < v.departureTurn) {
        v.position.x += v.velocity.x; v.position.y += v.velocity.y;
        // The elapsed year's encounter solver used the old velocity. Only the
        // next year's segment changes, before boundary observations are made.
        if(v.courseChangeTurn==turn && !v.courseChanged) {
            if(const auto course=changed_course(v,v.position,s.galaxySeed,s.wormholeRules.width,s.wormholeRules.height)) {
                v.velocity=course->velocity; v.departureTurn=course->departureTurn; v.courseChanged=true;
            } else v.courseChangeTurn=0; // Invalid in-memory input keeps its finite old segment.
        }
    }
    if (turn < 60 || s.stars.size() < 16 || s.visitors.size() >= 128
        || std::any_of(s.visitors.begin(), s.visitors.end(), [&](const auto& v) { return v.departureTurn > turn; })) return;
    std::mt19937_64 rng(s.galaxySeed ^ (turn*0x9e3779b97f4a7c15ULL) ^ 0x56495349544f52ULL);
    if (rng()%80) return;
    const auto width = s.wormholeRules.width, height = s.wormholeRules.height;
    s.visitors.push_back(crossing(s.nextVisitorId++,turn,width,height,rng));
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
            const bool newer = old==p.visitorKnowledge.end() || ((!old->courseChanged || x.courseChanged)
                && (x.observedTurn>old->observedTurn || (x.observedTurn==old->observedTurn
                    && ((!old->courseChanged && x.courseChanged)
                        || ((old->available && !x.available) || (!old->departed && x.departed))))));
            const bool courseNotice = newer && old!=p.visitorKnowledge.end() && x.velocity!=old->velocity;
            const bool statusChanged = newer && (old==p.visitorKnowledge.end() || x.available!=old->available || x.departed!=old->departed);
            // Refusals can carry a real contact measurement, while navigation
            // failures retain their old track. Freshness uses that evidence's
            // date, never the later mission-status date in r.observedTurn.
            if (newer) {
                if (old==p.visitorKnowledge.end()) p.visitorKnowledge.push_back(x);
                else *old=x;
            }
            const auto emit=[&](GameEventKind kind,std::uint64_t discriminator) {
                GameEvent e; e.kind=kind; e.id=1469598103934665603ULL;
                for(const auto value:{std::uint64_t{0x56495349544f52ULL},std::uint64_t(x.id),r.observedTurn,
                        std::uint64_t(p.id),discriminator}) e.id=(e.id^value)*1099511628211ULL;
                e.recipient=p.id; e.turn=s.turn; e.observedTurn=r.observedTurn; e.position=x.position;
                e.quantity=x.id; e.visitorProfile=x.profile;
                if(kind==GameEventKind::VisitorExchangeFailed) e.severity=GameEventSeverity::Warning;
                events.push_back(e);
            };
            // Any accepted physical measurement can be the first evidence of a
            // turn. Do not lose that notice to the receipt's transaction kind.
            if(courseNotice) emit(GameEventKind::VisitorCourseChanged,4);
            if (r.kind==VisitorReportKind::Exchanged) {
                const auto gift = visitor_offer(x.profile).gift;
                if (std::find(p.acquiredEquipment.begin(),p.acquiredEquipment.end(),gift)!=p.acquiredEquipment.end()) continue;
                p.acquiredEquipment.push_back(gift);
            } else if (r.kind==VisitorReportKind::Sighted && !statusChanged) continue;
            emit(r.kind==VisitorReportKind::Exchanged ? GameEventKind::VisitorExchangeCompleted
                : r.kind==VisitorReportKind::Failed ? GameEventKind::VisitorExchangeFailed
                : r.kind==VisitorReportKind::Departed ? GameEventKind::VisitorDeparted : GameEventKind::VisitorSighted,
                std::uint64_t(r.kind));
        }
        std::erase_if(p.pendingVisitorReports,[&](const auto& r){
            return r.deliveryTurn<=s.turn || (r.deliveryTurn==kAwaitingTransmission && r.transmissionFleet
                && std::none_of(s.fleets.begin(),s.fleets.end(),[&](const auto& f){return f.id==r.transmissionFleet && f.owner==p.id;}));
        });
    }
    return events;
}
GameState make_visitor_demo(VisitorProfile profile)
{
    auto s=make_demo_game(); s.turn=60; s.wormholeRules.spawnChancePerTurn=0;
    auto& p=s.players[0]; p.technology.levels.fill(16); p.technology.researchActive=false;
    s.planets[0].productionQueue.clear(); s.planets[0].minerals={1000,1000,1000};
    ShipDesign d{s.nextShipDesignId++,1,"Visitor rendezvous freighter",ShipHullType::SmallFreighter,{ShipComponentType::TransGalacticDrive,ShipComponentType::MoleScanner}};
    s.shipDesigns.push_back(d); s.fleets.resize(1); auto& f=s.fleets[0]; f.design=d.id; f.ships={{d.id,1}};
    f.name="Rendezvous courier"; f.position={0,0}; f.destination.reset(); f.arrivalAction.reset(); f.waypointQueue.clear();
    f.colonists=0; f.minerals=visitor_offer(profile).request; f.fuel=fleet_fuel_capacity(s,f); f.warp=8; f.telemetry={};
    s.visitors={{1,{-140,0},{121,0},60,66,0,profile}}; s.nextVisitorId=2;
    if(profile==VisitorProfile::Prospector) {
        s.visitors[0].position={0,-140}; s.visitors[0].velocity={0,121};
    } else if(profile==VisitorProfile::Shieldwright) {
        const auto diagonal=std::sqrt(0.5);
        s.visitors[0].position={-140*diagonal,-140*diagonal}; s.visitors[0].velocity={121*diagonal,121*diagonal};
    }
    publish_fleet_telemetry(s,s.turn); deliver_due_fleet_telemetry(s); observe_visitors(s); (void)deliver_visitor_reports(s);
    return s;
}
} // namespace suns
