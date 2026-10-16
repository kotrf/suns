#include "suns/planetary_recovery.hpp"
#include "suns/production.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/communications.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"
#include <algorithm>
#include <limits>
#include <tuple>

namespace suns {
void decay_planet_contamination(Planet& planet)
{
    auto& level=planet.warDamage.contamination;
    if(level && planet.aftermathKnownLocally) planet.observedHabitability.reset();
    level-=std::min<unsigned>(level,kContaminationDecay);
}
int contamination_habitability_penalty(const GameState& s,PlayerId owner,const Planet& planet)
{
    const auto* player=find_player(s,owner);
    return player && player->race.radiationImmune ? 0 : (planet.warDamage.contamination+4)/5;
}
std::uint32_t cleanup_units_remaining(const Planet& p)
{ return (p.warDamage.contamination+kContaminationCleanup-1)/kContaminationCleanup; }
std::uint32_t cleanup_units_available(const Planet& p)
{
    auto result=cleanup_units_remaining(p);
    for(const auto& item:p.productionQueue) if(item.kind==ProductionKind::CleanupContamination)
        result-=std::min(result,item.quantity);
    return result;
}
PlanetAftermathObservation observe_planet_aftermath(const Planet& p,bool exact,std::uint64_t turn)
{
    PlanetAftermathObservation x; x.planet=p.id; x.observedTurn=turn; x.exact=exact;
    const auto bound=[&](std::uint64_t n) {
        if(exact || !n) return std::pair{n,n};
        const auto low=(n/5)*5;
        return std::pair{low,low+std::min<std::uint64_t>(4,std::numeric_limits<std::uint64_t>::max()-low)};
    };
    std::tie(x.factoriesMinimum,x.factoriesMaximum)=bound(reconstruction_remaining(p,ProductionKind::ReconstructFactory));
    std::tie(x.minesMinimum,x.minesMaximum)=bound(reconstruction_remaining(p,ProductionKind::ReconstructMine));
    const auto level=p.warDamage.contamination;
    x.contaminationMinimum=exact || !level ? level : (level/10)*10;
    x.contaminationMaximum=exact || !level ? level : std::min(100,int(x.contaminationMinimum)+9);
    return x;
}
bool aftermath_observation_valid(const PlanetAftermathObservation& x)
{
    return x.planet && x.observedTurn && (!x.receivedTurn || x.receivedTurn>=x.observedTurn)
        && x.factoriesMinimum<=x.factoriesMaximum && x.minesMinimum<=x.minesMaximum
        && x.contaminationMinimum<=x.contaminationMaximum && x.contaminationMaximum<=100
        && (x.exact ? x.factoriesMinimum==x.factoriesMaximum && x.minesMinimum==x.minesMaximum
                && x.contaminationMinimum==x.contaminationMaximum
            : x.factoriesMaximum-x.factoriesMinimum<=4 && x.minesMaximum-x.minesMinimum<=4
                && x.contaminationMaximum-x.contaminationMinimum<=9);
}
bool same_aftermath_estimate(const PlanetAftermathObservation& a,const PlanetAftermathObservation& b)
{
    return a.planet==b.planet && a.exact==b.exact && a.factoriesMinimum==b.factoriesMinimum
        && a.factoriesMaximum==b.factoriesMaximum && a.minesMinimum==b.minesMinimum
        && a.minesMaximum==b.minesMaximum && a.contaminationMinimum==b.contaminationMinimum
        && a.contaminationMaximum==b.contaminationMaximum;
}
std::optional<PlanetAftermathObservation> known_planet_aftermath(const GameState& s,PlayerId owner,PlanetId id)
{
    const auto p=std::find_if(s.planets.begin(),s.planets.end(),[&](const auto& p){return p.id==id;});
    if(p!=s.planets.end() && p->owner==owner && p->aftermathKnownLocally) {
        auto result=observe_planet_aftermath(*p,true,s.turn); result.receivedTurn=s.turn; return result;
    }
    if(const auto* player=find_player(s,owner)) {
        const auto x=std::find_if(player->planetaryAftermath.begin(),player->planetaryAftermath.end(),[&](const auto& x){return x.planet==id;});
        if(x!=player->planetaryAftermath.end()) return *x;
    }
    return {};
}
void observe_planetary_aftermath(GameState& s,std::uint64_t turn)
{
    // Select one real receiver per planet/year, in stable ID order. A faster
    // connected observer wins; identical estimates still refresh their date.
    for(auto& player:s.players) for(const auto& p:s.planets) {
        if(p.owner==player.id) continue;
        if(!p.aftermathKnownLocally || !p.warDamage.lastBombardmentTurn) continue;
        const auto* star=find_star(s,p.star); if(!star) continue;
        const Fleet* best=nullptr; std::uint64_t delay=kAwaitingTransmission;
        for(const auto& f:s.fleets) {
            if(f.owner!=player.id || !same_position(f.position,star->position)
                || !fleet_has_scanner(s,f) || !fleet_active_sensors(f,turn)
                || sensor_interference(s,player.id,f.position,turn)>=0.5) continue;
            const auto candidate=fleet_report_delivery_turn(s,player.id,f.position,turn,f.id);
            if(!best || std::tie(candidate,f.id)<std::tie(delay,best->id)) { best=&f; delay=candidate; }
        }
        if(!best) continue;
        if(std::any_of(player.pendingPlayerReports.begin(),player.pendingPlayerReports.end(),[&](const auto& r) {
            return r.aftermath && r.planet==p.id && r.observedTurn==turn;
        })) continue;
        queue_player_report(s,player.id,PlayerReportKind::PlanetaryAftermath,best->position,turn,p.star,p.id,best->id);
        player.pendingPlayerReports.back().aftermath=observe_planet_aftermath(p,false,turn);
    }
}
} // namespace suns
