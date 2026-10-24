#include "suns/interstellar_medium.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/propulsion.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>
#include <tuple>

namespace suns {
namespace {
std::vector<InterstellarCloud> profiles(const GameState& s)
{
    if (!s.interstellarMediumEnabled) return {};
    if (!s.mediumObserver) return s.interstellarClouds;
    std::vector<InterstellarCloud> result;
    if (const auto* p = find_player(s, s.mediumObserver))
        for (const auto& x : p->mediumKnowledge)
            result.push_back({x.id, x.center, x.radiusX, x.radiusY,
                (x.densityMinimum + x.densityMaximum) * 0.5});
    return result;
}
double density(const std::vector<InterstellarCloud>& clouds, Position p)
{
    double clear = 1;
    for (const auto& c : clouds) {
        const double x = (p.x-c.center.x)/c.radiusX, y = (p.y-c.center.y)/c.radiusY;
        const double shoulder = std::max(0.0, 1-x*x-y*y);
        clear *= 1-c.density*shoulder*shoulder;
    }
    return std::clamp(1-clear, 0.0, 1.0);
}
// Ten-point Gauss-Legendre integrates the product of four quartic profiles
// exactly within every support interval (degree <= 16). Splitting at ellipse
// crossings prevents long routes from skipping a cloud between clear endpoints.
const std::array<std::pair<double,double>,10>& quadrature()
{
    static const auto values = [] {
        std::array<std::pair<double,double>,10> result{};
        for (int i=0; i<10; ++i) {
            double z=std::cos(std::numbers::pi*(i+0.75)/10.5), derivative{};
            for (int iteration=0; iteration<20; ++iteration) {
                double p=1, previous=0;
                for (int n=1; n<=10; ++n) { const auto old=p; p=((2*n-1)*z*p-(n-1)*previous)/n; previous=old; }
                derivative=10*(z*p-previous)/(z*z-1);
                const auto next=z-p/derivative;
                if (std::abs(next-z)<1e-15) { z=next; break; }
                z=next;
            }
            result[i]={z,2/((1-z*z)*derivative*derivative)};
        }
        return result;
    }();
    return values;
}
}
bool interstellar_cloud_valid(const InterstellarCloud& c)
{
    return c.id && c.id<=0x7fffffffU && std::isfinite(c.center.x) && std::isfinite(c.center.y)
        && std::abs(c.center.x)<=100000 && std::abs(c.center.y)<=100000
        && std::isfinite(c.radiusX) && std::isfinite(c.radiusY) && c.radiusX>=1 && c.radiusY>=1
        && c.radiusX<=10000 && c.radiusY<=10000 && std::isfinite(c.density) && c.density>0 && c.density<=1;
}
bool medium_observation_valid(const MediumObservation& x)
{
    return interstellar_cloud_valid({x.id,x.center,x.radiusX,x.radiusY,x.densityMaximum})
        && std::isfinite(x.densityMinimum) && x.densityMinimum>=0 && x.densityMinimum<=x.densityMaximum
        && x.level<=MediumKnowledgeLevel::Characterized && x.observedTurn
        && (!x.receivedTurn || x.receivedTurn>=x.observedTurn)
        && (x.level!=MediumKnowledgeLevel::Characterized || x.densityMinimum==x.densityMaximum);
}
void project_interstellar_medium(GameState& s, PlayerId player)
{
    s.mediumObserver=player;
    s.interstellarClouds.clear();
}
void generate_interstellar_medium(GameState& s)
{
    if (s.interstellarMediumEnabled || s.mediumObserver || s.stars.size()<16) return;
    s.interstellarMediumEnabled=true;
    std::mt19937_64 rng(s.galaxySeed ^ 0x4d454449554dULL);
    const auto unit=[&] { return double(rng()%1000001)/1000000; };
    const auto count=std::min<std::size_t>(4,1+s.stars.size()/160);
    const auto extent=std::min(s.wormholeRules.width,s.wormholeRules.height);
    for (std::size_t i=0; i<count; ++i) {
        InterstellarCloud c{std::uint32_t(i+1),
            {(unit()-0.5)*s.wormholeRules.width,(unit()-0.5)*s.wormholeRules.height},
            std::max(20.0,extent*(0.12+0.10*unit())),std::max(20.0,extent*(0.10+0.08*unit())),0.45+0.45*unit()};
        s.interstellarClouds.push_back(c);
    }
}
double interstellar_density(const GameState& s, Position p) { return density(profiles(s),p); }
namespace {
template<class Transform>
double segment_integral(const std::vector<InterstellarCloud>& clouds, Position start, Position end, Transform transform, bool adaptive=false)
{
    if (clouds.empty()) return transform(0.0);
    const double dx=end.x-start.x, dy=end.y-start.y;
    if (dx*dx+dy*dy<1e-20) return transform(density(clouds,start));
    std::vector<double> cuts{0,1};
    for (const auto& c:clouds) {
        const double x=(start.x-c.center.x)/c.radiusX, y=(start.y-c.center.y)/c.radiusY;
        const double vx=dx/c.radiusX, vy=dy/c.radiusY, a=vx*vx+vy*vy, b=2*(x*vx+y*vy);
        const double disc=b*b-4*a*(x*x+y*y-1);
        if (a<=0 || disc<=0) continue;
        for (const double t:{(-b-std::sqrt(disc))/(2*a),(-b+std::sqrt(disc))/(2*a)})
            if (t>0 && t<1) cuts.push_back(t);
    }
    std::sort(cuts.begin(),cuts.end());
    const auto integrate=[&](double a,double b) {
        const double half=(b-a)*0.5,mid=(a+b)*0.5;
        double value=0;
        for (const auto& [node,weight]:quadrature()) {
            const double t=mid+half*node;
            value+=half*weight*transform(density(clouds,{start.x+dx*t,start.y+dy*t}));
        }
        return value;
    };
    const auto refine=[&](auto&& self,double a,double b,double whole,int depth)->double {
        const double mid=(a+b)*0.5,left=integrate(a,mid),right=integrate(mid,b);
        if (depth==0 || std::abs(left+right-whole)<1e-12) return left+right;
        return self(self,a,mid,left,depth-1)+self(self,mid,b,right,depth-1);
    };
    double integral=0;
    for (std::size_t i=1;i<cuts.size();++i) {
        const double whole=integrate(cuts[i-1],cuts[i]);
        // Refine inside each support interval, so a narrow cloud cannot sit
        // entirely on one side of a whole-route subdivision and escape refinement.
        integral+=adaptive ? refine(refine,cuts[i-1],cuts[i],whole,16) : whole;
    }
    return integral;
}

// Each engine's rate is affine over density 0..1. Keep the individual curves:
// the least tolerant conventional/scoop engine can change inside a cloud.
using DamageCurves = std::vector<std::pair<double,double>>;
DamageCurves damage_curves(const GameState& s, const Fleet& fleet, std::uint8_t warp)
{
    DamageCurves result;
    if (!warp || warp>kMaxWarp) return result;
    for (const auto& stack:fleet_ship_stacks(fleet))
        if (const auto* design=find_ship_design(s,stack.design))
            for (const auto component:design->components) {
                const auto spec=component_spec(component);
                if (spec.kind!=ShipComponentKind::Engine) continue;
                const auto* ref=propulsion_technology(component);
                const bool scoop=ref ? ref->ramScoop
                    : component==ShipComponentType::RamScoopDrive
                    || component==ShipComponentType::RadiatingRamScoopDrive
                    || component==ShipComponentType::EfficientRamScoopDrive;
                const double clear=spec.overdriveDamagePercent[warp];
                // Conventional Warp 10 still pays a bounded environmental
                // penalty where its next (virtual) unsafe tier would begin.
                const double dense=scoop ? spec.overdriveDamagePercent[warp-1]
                    : warp<kMaxWarp ? spec.overdriveDamagePercent[warp+1]
                    : std::min(100.0,clear+18.0);
                result.emplace_back(clear,dense);
            }
    return result;
}
double damage_rate(const DamageCurves& curves,double d)
{
    double result=0;
    for (const auto& [clear,dense]:curves) result=std::max(result,clear+(dense-clear)*d);
    return result;
}
}
double medium_segment_density(const GameState& s, Position start, Position end)
{
    const auto clouds=profiles(s);
    return std::clamp(segment_integral(clouds,start,end,[](double d) { return d; }),0.0,1.0);
}
double fleet_medium_damage_rate(const GameState& s,const Fleet& fleet,std::uint8_t warp,double d)
{
    return damage_rate(damage_curves(s,fleet,warp),std::clamp(d,0.0,1.0));
}
double fleet_segment_damage(const GameState& s,const Fleet& fleet,Position end)
{
    const auto distance=distance_between(fleet.position,end);
    if (!fleet.warp || fleet.warp>kMaxWarp || distance<=0) return 0;
    if (!s.interstellarMediumEnabled)
        return fleet_overdrive_damage_rate(s,fleet,fleet.warp)*distance/warp_distance(fleet.warp);
    const auto curves=damage_curves(s,fleet,fleet.warp);
    const auto clouds=profiles(s);
    if (clouds.empty()) return damage_rate(curves,0)*distance/warp_distance(fleet.warp);
    // Maxima of opposing engine curves have kinks; affine single-engine
    // profiles are integrated exactly without further subdivision.
    return segment_integral(clouds,fleet.position,end,[&](double d) { return damage_rate(curves,d); },true)
        *distance/warp_distance(fleet.warp);
}
std::vector<MediumRouteAlternative> medium_route_alternatives(
    const GameState& state,const Fleet& source,Position destination,std::uint8_t warp)
{
    if (!fleet_warp_valid(state,source,warp)) return {};
    // Construct a small navigation projection rather than copying the full host.
    GameState received;
    received.interstellarMediumEnabled=state.interstellarMediumEnabled;
    received.mediumObserver=source.owner;
    if (const auto* player=find_player(state,source.owner)) {
        Player observer; observer.id=player->id; observer.race=player->race;
        observer.mediumKnowledge=player->mediumKnowledge;
        received.players.push_back(std::move(observer));
    }
    for (const auto& stack:fleet_ship_stacks(source))
        if (const auto* design=find_ship_design(state,stack.design)) received.shipDesigns.push_back(*design);
    const auto& s=received;
    std::vector<std::vector<Position>> paths{{destination}};
    const auto length=distance_between(source.position,destination);
    if (length>1e-6) {
        const double ux=(destination.x-source.position.x)/length,uy=(destination.y-source.position.y)/length;
        auto clouds=profiles(s);
        std::sort(clouds.begin(),clouds.end(),[](const auto& a,const auto& b) { return a.id<b.id; });
        if (clouds.size()>4) clouds.resize(4);
        for (const auto& c:clouds) {
            if (segment_integral(std::vector<InterstellarCloud>{c},source.position,destination,[](double d) { return d; })<1e-6) continue;
            const double center=(c.center.x-source.position.x)*ux+(c.center.y-source.position.y)*uy;
            const double along=std::hypot(c.radiusX*ux,c.radiusY*uy)+2;
            const double across=std::hypot(c.radiusX*uy,c.radiusY*ux)+2;
            const double lateral=(c.center.x-source.position.x)*(-uy)+(c.center.y-source.position.y)*ux;
            for (double sign:{-1.0,1.0}) {
                std::vector<Position> points;
                for (double t:{std::clamp(center-along,0.0,length),std::clamp(center+along,0.0,length)}) {
                    const Position p{source.position.x+ux*t-uy*(lateral+sign*across),
                        source.position.y+uy*t+ux*(lateral+sign*across)};
                    if ((points.empty() || !same_position(points.back(),p)) && !same_position(source.position,p)) points.push_back(p);
                }
                points.push_back(destination); paths.push_back(std::move(points));
            }
        }
    }
    std::vector<MediumRouteAlternative> result;
    for (auto& path:paths) {
        MediumRouteAlternative route; route.destinations=std::move(path);
        auto fleet=source; fleet.warp=warp;
        for (const auto end:route.destinations) {
            const auto distance=distance_between(fleet.position,end);
            route.distance+=distance;
            route.damage+=fleet_segment_damage(s,fleet,end);
            // Coarse whole-leg estimate. The ordinary route forecast applies
            // actual per-year billing, command delay, collection and arrivals.
            route.fuel+=fleet_fuel_consumption_for_segment(s,fleet,end);
            fleet.position=end;
        }
        route.years=route.distance/warp_distance(warp);
        result.push_back(std::move(route));
    }
    return result;
}
double medium_active_sensor_factor(const GameState& s,Position p) { return 1-0.20*interstellar_density(s,p); }
double medium_active_path_factor(const GameState& s,Position a,Position b) { return 1-0.30*medium_segment_density(s,a,b); }
void observe_interstellar_medium(GameState& s,std::uint64_t turn)
{
    if (!s.interstellarMediumEnabled || s.mediumObserver) return;
    struct Receiver { Position position; double range{}; FleetId fleet{}; PlanetId colony{}; };
    for (auto& player:s.players) {
        std::vector<Receiver> receivers;
        for (const auto& f:s.fleets) if (f.owner==player.id && fleet_has_scanner(s,f) && fleet_active_sensors(f,turn))
            receivers.push_back({f.position,std::max(1.0,strategic_sensor_range(s,f,turn)),f.id,0});
        for (const auto& p:s.planets) if (p.owner==player.id && p.population && p.radar!=PlanetaryRadarLevel::None)
            if (const auto* star=find_star(s,p.star)) receivers.push_back({star->position,
                planetary_radar_range(p.radar)*(1-sensor_interference(s,player.id,star->position,turn))
                    *medium_active_sensor_factor(s,star->position),0,p.id});
        for (const auto& c:s.interstellarClouds) {
            const Receiver* best=nullptr; MediumKnowledgeLevel level=MediumKnowledgeLevel::Detected;
            std::uint64_t due=kAwaitingTransmission;
            for (const auto& r:receivers) {
                const double x=(r.position.x-c.center.x)/c.radiusX,y=(r.position.y-c.center.y)/c.radiusY;
                const double distance=distance_between(r.position,c.center);
                if (distance>std::max(c.radiusX,c.radiusY)+r.range) continue;
                const auto quality=x*x+y*y<=0.5 ? MediumKnowledgeLevel::Characterized : MediumKnowledgeLevel::Detected;
                const auto delivery=fleet_report_delivery_turn(s,player.id,r.position,turn,r.fleet);
                if (!best || quality>level || (quality==level && std::tie(delivery,r.fleet,r.colony)<std::tie(due,best->fleet,best->colony)))
                    { best=&r; level=quality; due=delivery; }
            }
            if (!best) continue;
            MediumObservation x{c.id,c.center,c.radiusX,c.radiusY,0,1,level,turn,0};
            if (level==MediumKnowledgeLevel::Characterized) x.densityMinimum=x.densityMaximum=c.density;
            else {
                x.center={std::floor(c.center.x/20)*20+10,std::floor(c.center.y/20)*20+10};
                x.radiusX=std::ceil(c.radiusX/20)*20; x.radiusY=std::ceil(c.radiusY/20)*20;
            }
            queue_player_report(s,player.id,PlayerReportKind::InterstellarMediumObserved,best->position,turn,0,0,best->fleet);
            player.pendingPlayerReports.back().mediumObservation=x;
        }
    }
}
} // namespace suns
