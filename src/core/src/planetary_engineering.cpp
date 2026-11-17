#include "suns/planetary_engineering.hpp"
#include "suns/megaprojects.hpp"
#include "suns/turn_processor.hpp"
#include "suns/campaign.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/unknown_objects.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cmath>

namespace suns {
bool engineering_kind(ProductionKind kind) { return kind==ProductionKind::RadicalTransformation || kind==ProductionKind::WormholeExperiment || kind==ProductionKind::AtmosphericRegulator || kind==ProductionKind::ClimateStabilizer || kind==ProductionKind::OrbitalHabitat || kind==ProductionKind::OrbitalMirrors || kind==ProductionKind::OrbitalShades
    || kind==ProductionKind::ArtificialMagnetosphere || kind==ProductionKind::AdaptiveMagnetosphere; }
ProductionKind engineering_production_kind(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::RadicalTransformation?ProductionKind::RadicalTransformation:kind==PlanetaryEngineeringKind::WormholeExperiment?ProductionKind::WormholeExperiment:kind==PlanetaryEngineeringKind::AtmosphericRegulator?ProductionKind::AtmosphericRegulator:kind==PlanetaryEngineeringKind::ClimateStabilizer?ProductionKind::ClimateStabilizer:kind==PlanetaryEngineeringKind::OrbitalHabitat?ProductionKind::OrbitalHabitat:kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?ProductionKind::ArtificialMagnetosphere
    :kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?ProductionKind::AdaptiveMagnetosphere
    :kind==PlanetaryEngineeringKind::OrbitalShades?ProductionKind::OrbitalShades:ProductionKind::OrbitalMirrors; }
PlanetaryEngineeringKind engineering_project_kind(ProductionKind kind)
{ return kind==ProductionKind::RadicalTransformation?PlanetaryEngineeringKind::RadicalTransformation:kind==ProductionKind::WormholeExperiment?PlanetaryEngineeringKind::WormholeExperiment:kind==ProductionKind::AtmosphericRegulator?PlanetaryEngineeringKind::AtmosphericRegulator:kind==ProductionKind::ClimateStabilizer?PlanetaryEngineeringKind::ClimateStabilizer:kind==ProductionKind::OrbitalHabitat?PlanetaryEngineeringKind::OrbitalHabitat:kind==ProductionKind::ArtificialMagnetosphere?PlanetaryEngineeringKind::ArtificialMagnetosphere
    :kind==ProductionKind::AdaptiveMagnetosphere?PlanetaryEngineeringKind::AdaptiveMagnetosphere
    :kind==ProductionKind::OrbitalMirrors?PlanetaryEngineeringKind::OrbitalMirrors:kind==ProductionKind::OrbitalShades?PlanetaryEngineeringKind::OrbitalShades:PlanetaryEngineeringKind::None; }
std::string engineering_name(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::RadicalTransformation?"Radical transformation":kind==PlanetaryEngineeringKind::WormholeExperiment?"Wormhole experiment":kind==PlanetaryEngineeringKind::AtmosphericRegulator?"Atmospheric regulator":kind==PlanetaryEngineeringKind::ClimateStabilizer?"Climate stabilizer":kind==PlanetaryEngineeringKind::OrbitalHabitat?"Orbital habitat":kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?"Artificial magnetosphere"
    :kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?"Adaptive magnetosphere"
    :kind==PlanetaryEngineeringKind::OrbitalMirrors?"Orbital mirrors":kind==PlanetaryEngineeringKind::OrbitalShades?"Orbital shades":"No engineering project"; }
std::uint32_t engineering_work(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::RadicalTransformation?9600:kind==PlanetaryEngineeringKind::WormholeExperiment?8000:kind==PlanetaryEngineeringKind::AtmosphericRegulator?6000:(kind==PlanetaryEngineeringKind::ClimateStabilizer || kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere || kind==PlanetaryEngineeringKind::OrbitalHabitat)?4800:kEngineeringWork; }
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
    if(kind==PlanetaryEngineeringKind::RadicalTransformation) return p && p->race.environmentBased && p->race.primaryTrait!=PrimaryRaceTrait::HabitatCivilization
        && technology_level(state,player,ResearchField::Energy)>=26 && technology_level(state,player,ResearchField::Construction)>=26
        && technology_level(state,player,ResearchField::Biology)>=24;
    if(kind==PlanetaryEngineeringKind::WormholeExperiment) return p
        && technology_level(state,player,ResearchField::Energy)>=26 && technology_level(state,player,ResearchField::Construction)>=24
        && technology_level(state,player,ResearchField::Electronics)>=26;
    if(kind==PlanetaryEngineeringKind::AtmosphericRegulator) return p && p->race.environmentBased && p->race.primaryTrait!=PrimaryRaceTrait::HabitatCivilization
        && technology_level(state,player,ResearchField::Energy)>=22
        && technology_level(state,player,ResearchField::Construction)>=22
        && technology_level(state,player,ResearchField::Biology)>=20;
    if(kind==PlanetaryEngineeringKind::ClimateStabilizer) return p && p->race.environmentBased && p->race.primaryTrait!=PrimaryRaceTrait::HabitatCivilization
        && technology_level(state,player,ResearchField::Energy)>=20
        && technology_level(state,player,ResearchField::Construction)>=20
        && technology_level(state,player,ResearchField::Biology)>=18;
    const int level=kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?18:kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?16:14;
    if(kind==PlanetaryEngineeringKind::OrbitalHabitat) return p && p->race.environmentBased
        && technology_level(state,player,ResearchField::Energy)>=16
        && technology_level(state,player,ResearchField::Construction)>=18
        && technology_level(state,player,ResearchField::Biology)>=14;
    return kind!=PlanetaryEngineeringKind::None && kind<=PlanetaryEngineeringKind::AdaptiveMagnetosphere
        && p && p->race.environmentBased && technology_level(state,player,ResearchField::Energy)>=level
        && technology_level(state,player,ResearchField::Construction)>=level
        && (kind!=PlanetaryEngineeringKind::AdaptiveMagnetosphere || compact_study_received(state,player));
}
bool engineering_operation_available(const GameState& state,const Planet& p)
{ return (!megaproject_kind(p.engineering.kind) && p.engineering.kind!=PlanetaryEngineeringKind::AtmosphericRegulator && p.engineering.kind!=PlanetaryEngineeringKind::ClimateStabilizer && p.engineering.kind!=PlanetaryEngineeringKind::AdaptiveMagnetosphere && p.engineering.kind!=PlanetaryEngineeringKind::OrbitalHabitat) || planetary_engineering_available(state,p.owner,p.engineering.kind); }
bool engineering_station_available(const GameState& state,const Planet& planet)
{
    if(state.mediumObserver && planet.owner==state.mediumObserver) {
        const auto* player=find_player(state,state.mediumObserver);
        if(player) for(const auto& record:player->surveyKnowledge)
            if(record.star==planet.star && record.observedOwner==planet.owner && record.planetSnapshot && record.planetSnapshot->habitat)
                return record.planetSnapshot->habitat->stationSupported;
    }
    const bool station=std::any_of(state.orbitalStations.begin(),state.orbitalStations.end(),[&](const auto& s) {
        return s.planet==planet.id && s.owner==planet.owner && station_hull_at_least(s.hull,OrbitalStationHullType::Starbase)
            && s.damagePercent<100 && std::find(s.modules.begin(),s.modules.end(),OrbitalStationModule::Shipyard)!=s.modules.end();
    });
    if(station)return true;
    const auto* base=surface_sheltered(planet)?orbital_base_at(state,planet.id):nullptr;
    return base && base->owner==planet.owner && base->damagePercent<100 && base->orbitalBase.supported
        && base->orbitalBase.active && base->orbitalBase.assemblyYears==kOrbitalAssemblyYears;
}
bool engineering_targets_valid(PlanetaryEngineeringKind kind,std::uint8_t temperature,std::uint8_t radiation)
{
    return kind<=PlanetaryEngineeringKind::WormholeExperiment && temperature<=100 && radiation<=100
        && (kind==PlanetaryEngineeringKind::ClimateStabilizer || temperature==50)
        && (kind==PlanetaryEngineeringKind::AtmosphericRegulator || radiation==50);
}
bool engineering_order_shape_valid(const SetPlanetaryEngineeringOrder& o)
{
    return o.colony && o.action<=EngineeringAction::ActivateExperiment
        && engineering_targets_valid(o.kind,o.targetTemperature,o.targetRadiation)
        && o.strength<=engineering_max_strength(o.kind)
        && (!megaproject_kind(o.kind) || (o.strength==kEngineeringStrength && o.action!=EngineeringAction::SetOutput))
        && (o.action!=EngineeringAction::StartOrResume || o.kind!=PlanetaryEngineeringKind::None)
        && (o.action!=EngineeringAction::CommitTransformation || o.kind==PlanetaryEngineeringKind::RadicalTransformation)
        && (o.action!=EngineeringAction::ActivateExperiment || o.kind==PlanetaryEngineeringKind::WormholeExperiment);
}
bool engineering_state_valid(const PlanetaryEngineering& e)
{
    if (!megaproject_state_valid(e) || !engineering_targets_valid(e.kind,e.targetTemperature,e.targetRadiation)
        || (e.kind==PlanetaryEngineeringKind::OrbitalHabitat && e.strength!=kEngineeringStrength) || e.work>engineering_work(e.kind) || e.strength>engineering_max_strength(e.kind)
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
        if(e.kind==PlanetaryEngineeringKind::ClimateStabilizer) {
            const int correction=std::clamp(int(e.targetTemperature)-int(environment.temperature),-int(e.strength),int(e.strength));
            environment.temperature=std::uint8_t(int(environment.temperature)+correction);
        }
        if(e.kind==PlanetaryEngineeringKind::AtmosphericRegulator) {
            const int correction=std::clamp(int(e.targetRadiation)-int(environment.radiation),-int(e.strength),int(e.strength));
            environment.radiation=std::uint8_t(int(environment.radiation)+correction);
        }
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
    if(e.kind==PlanetaryEngineeringKind::RadicalTransformation && e.transformation.phase==TransformationPhase::Complete)
        return EngineeringBlock::TransformationComplete;
    if(e.kind==PlanetaryEngineeringKind::WormholeExperiment && e.experiment.openedTurn)return EngineeringBlock::ExperimentSpent;
    if(megaproject_kind(e.kind) && planet.owner) {
        if(!engineering_operation_available(state,planet))return EngineeringBlock::MegaprojectTechnology;
        if(!engineering_station_available(state,planet))return EngineeringBlock::MissingStation;
        if(surface_sheltered(planet))return e.transformation.paused?EngineeringBlock::Paused:EngineeringBlock::Transforming;
        if(e.work==engineering_work(e.kind)) {
            if(e.kind==PlanetaryEngineeringKind::RadicalTransformation)return EngineeringBlock::AwaitingCommitment;
            return e.experiment.openedTurn?EngineeringBlock::ExperimentSpent:e.experiment.activationTurn?EngineeringBlock::ExperimentPending:EngineeringBlock::ExperimentReady;
        }
    }
    if(!planet.owner || !total_colony_population(planet)) return EngineeringBlock::NoColony;
    if(e.kind==PlanetaryEngineeringKind::AtmosphericRegulator && !engineering_operation_available(state,planet))
        return EngineeringBlock::AtmosphericTechnology;
    if(e.kind==PlanetaryEngineeringKind::ClimateStabilizer && !engineering_operation_available(state,planet))
        return EngineeringBlock::ClimateTechnology;
    if(e.kind==PlanetaryEngineeringKind::OrbitalHabitat) {
        if(!engineering_operation_available(state,planet)) return EngineeringBlock::HabitatTechnology;
        if(habitat_completed(planet) && planet.habitat.population) {
            if(planet.habitat.service==HabitatService::Reserve) return EngineeringBlock::HabitatReserve;
            if(planet.habitat.service==HabitatService::Failing) return EngineeringBlock::HabitatFailure;
        }
    }
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
    case EngineeringBlock::MegaprojectTechnology: return "Radical transformation needs a surface race, E26/C26/B24; wormhole experiment needs E26/C24/Electronics26";
    case EngineeringBlock::AwaitingCommitment: return "Preparation complete; awaiting explicit irreversible commitment";
    case EngineeringBlock::Transforming: return "Transformation in progress; residents sheltered, surface output and growth suspended";
    case EngineeringBlock::TransformationComplete: return "Permanent transformation complete; supporting equipment can be dismantled without reversing the planet";
    case EngineeringBlock::ExperimentReady: return "Generator complete; awaiting explicit experiment activation";
    case EngineeringBlock::ExperimentPending: return "Experiment activation pending; awaiting support";
    case EngineeringBlock::ExperimentSpent: return "Experiment spent; the transient wormhole continues independently";
    case EngineeringBlock::AtmosphericTechnology: return "Atmospheric regulator requires a race with surface colonies, Energy 22, Construction 22 and Biology 20";
    case EngineeringBlock::ClimateTechnology: return "Climate stabilizer requires a race with surface colonies, Energy 20, Construction 20 and Biology 18";
    case EngineeringBlock::HabitatTechnology: return "Orbital habitat requires Construction 18, Energy 16 and Biology 14";
    case EngineeringBlock::HabitatReserve: return "Emergency reserve: no orbital growth or output";
    case EngineeringBlock::HabitatFailure: return "Life support exhausted: 10% orbital population lost per year";
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
    if(p.id!=order.colony || !p.owner || !engineering_order_shape_valid(order)) return false;
    auto& e=p.engineering;
    if(e.kind!=PlanetaryEngineeringKind::None && (e.kind==PlanetaryEngineeringKind::ClimateStabilizer || order.kind==PlanetaryEngineeringKind::ClimateStabilizer
        || e.kind==PlanetaryEngineeringKind::AtmosphericRegulator || order.kind==PlanetaryEngineeringKind::AtmosphericRegulator
        || megaproject_kind(e.kind) || megaproject_kind(order.kind))
        && e.kind!=order.kind) return false;
    const auto pause=[&] { std::erase_if(p.productionQueue,[](const auto& item){return engineering_kind(item.kind);}); };
    if(order.action>=EngineeringAction::CommitTransformation || surface_sheltered(p))
        return apply_megaproject_action(state,p,order);
    if(megaproject_kind(order.kind) && (order.strength!=kEngineeringStrength || order.action==EngineeringAction::SetOutput))return false;
    if(order.action==EngineeringAction::Dismantle) { if(p.habitat.population || e.kind==PlanetaryEngineeringKind::None)return false; pause(); e={}; p.habitat={}; p.observedHabitability.reset(); return true; }
    if(order.action==EngineeringAction::Pause) { if(e.kind==PlanetaryEngineeringKind::None)return false; pause(); return true; }
    if(order.action==EngineeringAction::SetOutput) { if(e.kind==PlanetaryEngineeringKind::OrbitalHabitat || e.kind==PlanetaryEngineeringKind::None)return false; e.strength=order.strength; e.targetTemperature=order.targetTemperature; e.targetRadiation=order.targetRadiation; e.operational=e.work==engineering_work(e.kind) && e.strength>0 && p.population>0 && engineering_station_available(state,p) && engineering_operation_available(state,p); p.observedHabitability.reset(); return true; }
    if((order.kind==PlanetaryEngineeringKind::OrbitalHabitat && e.kind==PlanetaryEngineeringKind::None && !p.population) || order.kind==PlanetaryEngineeringKind::None || !planetary_engineering_available(state,p.owner,order.kind)
        || (e.kind!=PlanetaryEngineeringKind::None && e.kind!=order.kind) || e.work==engineering_work(e.kind) || engineering_scheduled(p)) return false;
    if(e.kind==PlanetaryEngineeringKind::None) { e.kind=order.kind; e.builder=p.owner; e.startedTurn=state.turn; e.strength=e.kind==PlanetaryEngineeringKind::OrbitalHabitat?kEngineeringStrength:order.strength; e.targetTemperature=order.targetTemperature; e.targetRadiation=order.targetRadiation; }
    p.productionQueue.push_back({engineering_production_kind(e.kind),engineering_work(e.kind)-e.work}); return true;
}
std::uint32_t advance_planetary_engineering(const GameState& state,Planet& p,std::uint32_t available,bool supported,std::uint32_t annualRemaining)
{
    auto& e=p.engineering;
    if(e.kind==PlanetaryEngineeringKind::None || e.work>=engineering_work(e.kind) || !p.owner || !total_colony_population(p)
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
    if(e.work==engineering_work(e.kind)) { if(e.kind==PlanetaryEngineeringKind::OrbitalHabitat) p.habitat.reserveYears=kHabitatReserveYears; e.completedTurn=state.turn+1; e.operational=e.strength>0; p.observedHabitability.reset(); }
    return work;
}
void refresh_planetary_engineering(GameState& state)
{
    for(auto& p:state.planets) p.engineering.operational=p.engineering.kind!=PlanetaryEngineeringKind::None
        && p.engineering.work==engineering_work(p.engineering.kind) && p.engineering.strength>0 && p.owner && (total_colony_population(p) || surface_sheltered(p))
        && engineering_station_available(state,p) && engineering_operation_available(state,p)
        && (!surface_sheltered(p) || !p.engineering.transformation.paused)
        && (p.engineering.kind!=PlanetaryEngineeringKind::OrbitalHabitat || p.habitat.service==HabitatService::Normal);
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
        if(after.kind==PlanetaryEngineeringKind::RadicalTransformation && after.transformation.phase!=TransformationPhase::Preparation) {
            const auto oldYears=before.kind==after.kind?before.transformation.years:0;
            const auto progress=unsigned(after.transformation.years)*100/kTransformationYears;
            if(before.transformation.phase==TransformationPhase::Preparation || before.kind!=after.kind)
                report(PlayerReportKind::EngineeringProgress,after.kind,kTransformationProgressBase);
            if(after.transformation.years>oldYears && (after.transformation.phase==TransformationPhase::Complete
                || progress/25>(unsigned(oldYears)*100/kTransformationYears)/25))
                report(PlayerReportKind::EngineeringProgress,after.kind,kTransformationProgressBase+progress);
        }
        if(after.kind==PlanetaryEngineeringKind::WormholeExperiment && after.experiment.openedTurn
            && (before.kind!=after.kind || !before.experiment.openedTurn))
            report(PlayerReportKind::EngineeringProgress,after.kind,kExperimentOpenedProgress);
        if(habitat_completed(p)) continue; // Dedicated reserve/loss reports carry exact orbital status.
        const auto block=engineering_block(state,p); const auto previousBlock=old && !replaced?engineering_block(previous,*old):EngineeringBlock::NoProject;
        const auto blocked=[](EngineeringBlock b){return b!=EngineeringBlock::None && b<EngineeringBlock::AwaitingCommitment;};
        if(blocked(block) && (block!=previousBlock || before.kind==PlanetaryEngineeringKind::None)) report(PlayerReportKind::EngineeringPaused,after.kind,std::uint32_t(block));
        else if(!blocked(block) && blocked(previousBlock) && before.kind==after.kind && !replaced)
            report(PlayerReportKind::EngineeringResumed,after.kind,percent);
    }
}
} // namespace suns
