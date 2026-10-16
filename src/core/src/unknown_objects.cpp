#include "suns/unknown_objects.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <tuple>

namespace suns {
const char* unknown_object_name(UnknownObjectKind kind)
{ return kind==UnknownObjectKind::AncientProbe ? "Ancient probe" : "Compact remnant"; }
const char* object_knowledge_name(ObjectKnowledgeLevel level)
{ return level==ObjectKnowledgeLevel::Studied ? "Studied" : level==ObjectKnowledgeLevel::Classified ? "Classified" : "Unknown"; }
ResearchField unknown_object_research_field(UnknownObjectKind kind)
{ return kind==UnknownObjectKind::AncientProbe ? ResearchField::Electronics : ResearchField::Energy; }
std::uint32_t unknown_object_research_points(UnknownObjectKind kind)
{ return kind==UnknownObjectKind::AncientProbe ? 24 : 32; }
double object_study_radius(const GameState& s,const Fleet& fleet)
{ return fleet_has_scanner(s,fleet) ? std::min(20.0,std::max(1.0,fleet_sensor_range(s,fleet))) : 0; }
bool object_observation_valid(const ObjectObservation& x)
{
    return x.id && x.id<=0x7fffffffU && x.level<=ObjectKnowledgeLevel::Studied
        && std::isfinite(x.position.x) && std::isfinite(x.position.y)
        && std::isfinite(x.uncertainty) && x.uncertainty>=0 && x.uncertainty<=15
        && x.observedTurn && (!x.receivedTurn || x.receivedTurn>=x.observedTurn)
        && (x.level==ObjectKnowledgeLevel::Detected ? !x.kind && x.uncertainty==15
            : x.kind && *x.kind<=UnknownObjectKind::CompactRemnant && x.uncertainty==0);
}
const ObjectObservation* known_unknown_object(const GameState& s,PlayerId owner,UnknownObjectId id)
{
    const auto* player=find_player(s,owner); if(!player) return nullptr;
    const auto found=std::find_if(player->objectKnowledge.begin(),player->objectKnowledge.end(),[&](const auto& x){return x.id==id;});
    return found==player->objectKnowledge.end() ? nullptr : &*found;
}
void generate_unknown_objects(GameState& s)
{
    if(!s.unknownObjects.empty() || s.nextUnknownObjectId!=1 || s.stars.size()<16) return;
    std::mt19937_64 rng(s.galaxySeed ^ 0x554e4b4e4f574eULL);
    const auto count=std::min<std::size_t>(4,1+s.stars.size()/80);
    const auto coordinate=[&](double extent){return (double(rng()%1'000'001)/1'000'000-0.5)*extent;};
    for(std::size_t n=0;n<count;++n) for(int attempt=0;attempt<2000;++attempt) {
        Position p{coordinate(s.wormholeRules.width),coordinate(s.wormholeRules.height)};
        if(std::any_of(s.stars.begin(),s.stars.end(),[&](const auto& star){return distance_between(star.position,p)<30;})
            || std::any_of(s.unknownObjects.begin(),s.unknownObjects.end(),[&](const auto& x){return distance_between(x.position,p)<60;})) continue;
        s.unknownObjects.push_back({s.nextUnknownObjectId++,p,UnknownObjectKind(rng()%2)}); break;
    }
}
namespace {
struct Receiver { Position position; double range{}; FleetId fleet{}; PlanetId colony{}; };
ObjectObservation observation(const UnknownObject& object,ObjectKnowledgeLevel level,std::uint64_t turn)
{
    ObjectObservation result; result.id=object.id; result.level=level; result.observedTurn=turn;
    result.position=object.position;
    if(level==ObjectKnowledgeLevel::Detected) {
        result.position={std::floor(object.position.x/20)*20+10,std::floor(object.position.y/20)*20+10};
        result.uncertainty=15;
    } else result.kind=object.kind;
    return result;
}
}
void observe_unknown_objects(GameState& s,std::uint64_t turn)
{
    for(auto& player:s.players) {
        std::vector<Receiver> receivers;
        for(const auto& f:s.fleets) if(f.owner==player.id && fleet_has_scanner(s,f) && fleet_active_sensors(f,turn))
            receivers.push_back({f.position,std::max(1.0,strategic_sensor_range(s,f,turn)),f.id,0});
        for(const auto& p:s.planets) if(p.owner==player.id && p.population && p.radar!=PlanetaryRadarLevel::None)
            if(const auto* star=find_star(s,p.star)) receivers.push_back({star->position,
                planetary_radar_range(p.radar)*(1-sensor_interference(s,player.id,star->position,turn)),0,p.id});
        for(const auto& object:s.unknownObjects) {
            const Receiver* best=nullptr; ObjectKnowledgeLevel level=ObjectKnowledgeLevel::Detected;
            std::uint64_t delivery=kAwaitingTransmission;
            for(const auto& r:receivers) {
                const auto distance=distance_between(r.position,object.position);
                if(distance>r.range+1e-6) continue;
                const auto candidate=distance<=std::min(40.0,r.range*0.5)+1e-6
                    ? ObjectKnowledgeLevel::Classified : ObjectKnowledgeLevel::Detected;
                const auto due=fleet_report_delivery_turn(s,player.id,r.position,turn,r.fleet);
                if(!best || candidate>level || (candidate==level && std::tie(due,r.fleet,r.colony)<std::tie(delivery,best->fleet,best->colony))) {
                    best=&r; level=candidate; delivery=due;
                }
            }
            if(!best) continue;
            // An already dispatched study/classification from this observation
            // year is at least as good as this routine sensor measurement.
            if(std::any_of(player.pendingPlayerReports.begin(),player.pendingPlayerReports.end(),[&](const auto& r){
                return r.objectObservation && r.objectObservation->id==object.id && r.observedTurn==turn
                    && r.objectObservation->level>=level && r.deliveryTurn<=delivery;
            })) continue;
            queue_player_report(s,player.id,PlayerReportKind::UnknownObjectObserved,best->position,turn,0,0,best->fleet);
            player.pendingPlayerReports.back().objectObservation=observation(object,level,turn);
        }
    }
}
void research_unknown_objects(GameState& s,Fleet& fleet,std::uint64_t turn)
{
    if(fleet.task!=FleetTask::FieldResearch || fleet.destination || !fleet_has_scanner(s,fleet) || !fleet_active_sensors(fleet,turn)) return;
    const auto range=std::min(object_study_radius(s,fleet),std::max(1.0,strategic_sensor_range(s,fleet,turn)));
    for(const auto& object:s.unknownObjects) if(distance_between(fleet.position,object.position)<=range+1e-6) {
        if(!collect_field_science_once(s,fleet,(5ULL<<32)|object.id,object.position,0,
            unknown_object_research_field(object.kind),unknown_object_research_points(object.kind),turn)) continue;
        const auto player=std::find_if(s.players.begin(),s.players.end(),[&](const auto& p){return p.id==fleet.owner;});
        player->pendingPlayerReports.back().objectObservation=observation(object,ObjectKnowledgeLevel::Studied,turn);
    }
}
} // namespace suns
