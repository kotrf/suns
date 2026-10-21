#include "suns/planetary_engineering.hpp"
#include "suns/turn_processor.hpp"
#include "suns/campaign.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/unknown_objects.hpp"
#include <algorithm>
#include <cmath>

namespace suns {
bool engineering_kind(ProductionKind kind) { return kind==ProductionKind::OrbitalMirrors || kind==ProductionKind::OrbitalShades
    || kind==ProductionKind::ArtificialMagnetosphere || kind==ProductionKind::AdaptiveMagnetosphere; }
ProductionKind engineering_production_kind(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?ProductionKind::ArtificialMagnetosphere
    :kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?ProductionKind::AdaptiveMagnetosphere
    :kind==PlanetaryEngineeringKind::OrbitalShades?ProductionKind::OrbitalShades:ProductionKind::OrbitalMirrors; }
PlanetaryEngineeringKind engineering_project_kind(ProductionKind kind)
{ return kind==ProductionKind::ArtificialMagnetosphere?PlanetaryEngineeringKind::ArtificialMagnetosphere
    :kind==ProductionKind::AdaptiveMagnetosphere?PlanetaryEngineeringKind::AdaptiveMagnetosphere
    :kind==ProductionKind::OrbitalMirrors?PlanetaryEngineeringKind::OrbitalMirrors:kind==ProductionKind::OrbitalShades?PlanetaryEngineeringKind::OrbitalShades:PlanetaryEngineeringKind::None; }
std::string engineering_name(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?"Artificial magnetosphere"
    :kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?"Adaptive magnetosphere"
    :kind==PlanetaryEngineeringKind::OrbitalMirrors?"Orbital mirrors":kind==PlanetaryEngineeringKind::OrbitalShades?"Orbital shades":"No engineering project"; }
std::uint32_t engineering_work(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?4800:kEngineeringWork; }
std::uint8_t engineering_max_strength(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?20:kEngineeringStrength; }
bool compact_study_received(const GameState& state,PlayerId player)
{
    const auto* p=find_player(state,player);
    return p && std::any_of(p->objectKnowledge.begin(),p->objectKnowledge.end(),[&](const auto& x) {
        return x.level==ObjectKnowledgeLevel::Studied && x.kind==UnknownObjectKind::CompactRemnant && x.receivedTurn<=state.turn;
    });
}
bool planetary_engineering_available(const GameState& state,PlayerId player,PlanetaryEngineeringKind kind)
{
    const auto* p=find_player(state,player);
    const int level=kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?18:kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?16:14;
    return kind!=PlanetaryEngineeringKind::None && kind<=PlanetaryEngineeringKind::AdaptiveMagnetosphere
        && p && p->race.environmentBased && technology_level(state,player,ResearchField::Energy)>=level
        && technology_level(state,player,ResearchField::Construction)>=level
        && (kind!=PlanetaryEngineeringKind::AdaptiveMagnetosphere || compact_study_received(state,player));
}
bool engineering_operation_available(const GameState& state,const Planet& p)
{ return p.engineering.kind!=PlanetaryEngineeringKind::AdaptiveMagnetosphere || planetary_engineering_available(state,p.owner,p.engineering.kind); }
bool engineering_station_available(const GameState& state,const Planet& planet)
{
    return std::any_of(state.orbitalStations.begin(),state.orbitalStations.end(),[&](const auto& s) {
        return s.planet==planet.id && s.owner==planet.owner && s.hull>=OrbitalStationHullType::Starbase
            && s.damagePercent<100 && std::find(s.modules.begin(),s.modules.end(),OrbitalStationModule::Shipyard)!=s.modules.end();
    });
}
bool engineering_state_valid(const PlanetaryEngineering& e)
{
    if (e.kind>PlanetaryEngineeringKind::AdaptiveMagnetosphere || e.work>engineering_work(e.kind) || e.strength>engineering_max_strength(e.kind)
        || !std::isfinite(e.invested.ironium) || !std::isfinite(e.invested.boranium) || !std::isfinite(e.invested.germanium)
        || e.invested.ironium<0 || e.invested.boranium<0 || e.invested.germanium<0
        || e.invested.ironium>engineering_work(e.kind)*0.1+1e-6 || e.invested.boranium>engineering_work(e.kind)*0.05+1e-6 || e.invested.germanium>engineering_work(e.kind)*0.1+1e-6) return false;
    if(e.kind==PlanetaryEngineeringKind::None)
        return !e.work && !e.operational && !e.builder && !e.startedTurn && !e.completedTurn
            && e.invested.ironium==0 && e.invested.boranium==0 && e.invested.germanium==0;
    return e.builder && e.startedTurn && (e.work==engineering_work(e.kind) ? e.completedTurn>=e.startedTurn : !e.completedTurn)
        && (!e.operational || (e.work==engineering_work(e.kind) && e.strength>0));
}
PlanetaryEngineering engineering_observation(const Planet& planet)
{ auto e=planet.engineering; e.invested={}; return e; }
PlanetEnvironment engineered_environment(const Planet& planet)
{
    auto environment=planet.environment; const auto& e=planet.engineering;
    if(e.operational && e.work==engineering_work(e.kind)) {
        const int sign=e.kind==PlanetaryEngineeringKind::OrbitalMirrors?1:e.kind==PlanetaryEngineeringKind::OrbitalShades?-1:0;
        environment.temperature=std::uint8_t(std::clamp(int(environment.temperature)+sign*int(e.strength),0,100));
        if(e.kind==PlanetaryEngineeringKind::ArtificialMagnetosphere || e.kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere)
            environment.radiation=std::uint8_t(std::max(0,int(environment.radiation)-int(e.strength)));
    }
    return environment;
}
bool engineering_scheduled(const Planet& p)
{ return std::any_of(p.productionQueue.begin(),p.productionQueue.end(),[](const auto& item){return engineering_kind(item.kind);}); }
EngineeringBlock engineering_block(const GameState& state,const Planet& planet)
{
    const auto& e=planet.engineering;
    if(e.kind==PlanetaryEngineeringKind::None) return EngineeringBlock::NoProject;
    if(!planet.owner || !planet.population) return EngineeringBlock::NoColony;
    if(e.kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere && !engineering_operation_available(state,planet))
        return compact_study_received(state,planet.owner)?EngineeringBlock::AdaptiveTechnology:EngineeringBlock::CompactStudy;
    if(e.work==engineering_work(e.kind)) {
        if(!e.strength) return EngineeringBlock::Paused;
        return engineering_station_available(state,planet)?EngineeringBlock::None:EngineeringBlock::MissingStation;
    }
    if(!engineering_scheduled(planet)) return EngineeringBlock::Paused;
    if(!planetary_engineering_available(state,planet.owner,e.kind)) return e.kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?EngineeringBlock::MagneticTechnology:EngineeringBlock::Technology;
    if(!engineering_station_available(state,planet)) return EngineeringBlock::MissingStation;
    if(planet.minerals.ironium+1e-9<kEngineeringMineralsPerWork.ironium
        || planet.minerals.boranium+1e-9<kEngineeringMineralsPerWork.boranium
        || planet.minerals.germanium+1e-9<kEngineeringMineralsPerWork.germanium) return EngineeringBlock::Minerals;
    return EngineeringBlock::None;
}
std::string engineering_block_name(EngineeringBlock block)
{
    switch(block) {
    case EngineeringBlock::None: return "Supported";
    case EngineeringBlock::NoProject: return "No project";
    case EngineeringBlock::Paused: return "Paused by owner";
    case EngineeringBlock::NoColony: return "Populated colony required";
    case EngineeringBlock::MissingStation: return "Operational friendly Starbase or Fortress with a shipyard required";
    case EngineeringBlock::Technology: return "Energy 14 and Construction 14 required";
    case EngineeringBlock::MagneticTechnology: return "Energy 16 and Construction 16 required";
    case EngineeringBlock::AdaptiveTechnology: return "Energy 18 and Construction 18 required";
    case EngineeringBlock::CompactStudy: return "Awaiting delivered completed Compact remnant field study";
    case EngineeringBlock::Minerals: return "Engineering minerals exhausted";
    }
    return "Unknown engineering status";
}
bool apply_planetary_engineering_order(const GameState& state,Planet& p,const SetPlanetaryEngineeringOrder& order)
{
    if(p.id!=order.colony || !p.owner || order.strength>engineering_max_strength(order.action==EngineeringAction::SetOutput?p.engineering.kind:order.kind) || order.action>EngineeringAction::SetOutput
        || order.kind>PlanetaryEngineeringKind::AdaptiveMagnetosphere) return false;
    auto& e=p.engineering;
    const auto pause=[&] { std::erase_if(p.productionQueue,[](const auto& item){return engineering_kind(item.kind);}); };
    if(order.action==EngineeringAction::Dismantle) { if(e.kind==PlanetaryEngineeringKind::None)return false; pause(); e={}; p.observedHabitability.reset(); return true; }
    if(order.action==EngineeringAction::Pause) { if(e.kind==PlanetaryEngineeringKind::None)return false; pause(); return true; }
    if(order.action==EngineeringAction::SetOutput) { if(e.kind==PlanetaryEngineeringKind::None)return false; e.strength=order.strength; e.operational=e.work==engineering_work(e.kind) && e.strength>0 && p.population>0 && engineering_station_available(state,p) && engineering_operation_available(state,p); p.observedHabitability.reset(); return true; }
    if(order.kind==PlanetaryEngineeringKind::None || !planetary_engineering_available(state,p.owner,order.kind)
        || (e.kind!=PlanetaryEngineeringKind::None && e.kind!=order.kind) || e.work==engineering_work(e.kind) || engineering_scheduled(p)) return false;
    if(e.kind==PlanetaryEngineeringKind::None) { e.kind=order.kind; e.builder=p.owner; e.startedTurn=state.turn; e.strength=order.strength; }
    p.productionQueue.push_back({engineering_production_kind(e.kind),engineering_work(e.kind)-e.work}); return true;
}
std::uint32_t advance_planetary_engineering(const GameState& state,Planet& p,std::uint32_t available,bool supported,std::uint32_t annualRemaining)
{
    auto& e=p.engineering;
    if(e.kind==PlanetaryEngineeringKind::None || e.work>=engineering_work(e.kind) || !p.owner || !p.population
        || !supported || !planetary_engineering_available(state,p.owner,e.kind)) return 0;
    const auto affordable=[](double stock,double rate) { return std::uint32_t(std::clamp(std::floor(std::max(0.0,stock)/rate+1e-8),0.0,double(kEngineeringWorkPerYear))); };
    const auto work=std::min({available,annualRemaining,engineering_work(e.kind)-e.work,
        affordable(p.minerals.ironium,kEngineeringMineralsPerWork.ironium),
        affordable(p.minerals.boranium,kEngineeringMineralsPerWork.boranium),
        affordable(p.minerals.germanium,kEngineeringMineralsPerWork.germanium)});
    const auto pay=[&](double& stock,double& invested,double rate) { const auto cost=work*rate; stock=std::max(0.0,stock-cost); invested+=cost; };
    pay(p.minerals.ironium,e.invested.ironium,kEngineeringMineralsPerWork.ironium);
    pay(p.minerals.boranium,e.invested.boranium,kEngineeringMineralsPerWork.boranium);
    pay(p.minerals.germanium,e.invested.germanium,kEngineeringMineralsPerWork.germanium);
    e.work+=work;
    if(e.work==engineering_work(e.kind)) { e.completedTurn=state.turn+1; e.operational=e.strength>0; p.observedHabitability.reset(); }
    return work;
}
void refresh_planetary_engineering(GameState& state)
{
    for(auto& p:state.planets) p.engineering.operational=p.engineering.kind!=PlanetaryEngineeringKind::None
        && p.engineering.work==engineering_work(p.engineering.kind) && p.engineering.strength>0 && p.owner && p.population
        && engineering_station_available(state,p) && engineering_operation_available(state,p);
}
void publish_planetary_engineering_reports(GameState& state,const GameState& previous)
{
    for(const auto& p:state.planets) {
        if(!p.owner) continue;
        const auto* old=find_planet(previous,p.id); const PlanetaryEngineering empty;
        const auto& before=old?old->engineering:empty; const auto& after=p.engineering;
        if(before.kind==PlanetaryEngineeringKind::None && after.kind==PlanetaryEngineeringKind::None) continue;
        const auto* star=find_star(state,p.star); if(!star)continue;
        const auto report=[&](PlayerReportKind kind,PlanetaryEngineeringKind project,std::uint32_t value) {
            queue_player_report(state,p.owner,kind,star->position,state.turn+1,p.star,p.id,0,0,engineering_production_kind(project),value);
        };
        if(after.kind==PlanetaryEngineeringKind::None) { report(PlayerReportKind::EngineeringDismantled,before.kind,before.work*100/engineering_work(before.kind)); continue; }
        const auto percent=after.work*100/engineering_work(after.kind);
        const bool replaced=before.kind!=PlanetaryEngineeringKind::None
            && (before.kind!=after.kind || before.builder!=after.builder || before.startedTurn!=after.startedTurn);
        if(replaced) report(PlayerReportKind::EngineeringDismantled,before.kind,before.work*100/engineering_work(before.kind));
        if(before.kind==PlanetaryEngineeringKind::None || replaced) report(PlayerReportKind::EngineeringProgress,after.kind,0);
        if(old && old->owner!=p.owner && before.kind!=PlanetaryEngineeringKind::None && !replaced) report(PlayerReportKind::EngineeringCaptured,after.kind,percent);
        const auto oldWork=before.kind==after.kind && !replaced?before.work:0U;
        if(after.work==engineering_work(after.kind) && oldWork<engineering_work(after.kind)) report(PlayerReportKind::EngineeringProgress,after.kind,100);
        else if(after.work<engineering_work(after.kind) && after.work/(engineering_work(after.kind)/4)>oldWork/(engineering_work(after.kind)/4))
            report(PlayerReportKind::EngineeringProgress,after.kind,after.work/(engineering_work(after.kind)/4)*25);
        const auto block=engineering_block(state,p); const auto previousBlock=old && !replaced?engineering_block(previous,*old):EngineeringBlock::NoProject;
        if(block!=EngineeringBlock::None && (block!=previousBlock || before.kind==PlanetaryEngineeringKind::None)) report(PlayerReportKind::EngineeringPaused,after.kind,std::uint32_t(block));
        else if(block==EngineeringBlock::None && previousBlock!=EngineeringBlock::None && before.kind==after.kind && !replaced)
            report(PlayerReportKind::EngineeringResumed,after.kind,percent);
    }
}
} // namespace suns
