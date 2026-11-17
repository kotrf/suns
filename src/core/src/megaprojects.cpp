#include "suns/megaprojects.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/turn_processor.hpp"
#include "suns/wormholes.hpp"
#include "suns/orbital_habitats.hpp"
#include <algorithm>

namespace suns {
bool surface_sheltered(const Planet& p)
{ return p.engineering.kind==PlanetaryEngineeringKind::RadicalTransformation
    && p.engineering.transformation.phase==TransformationPhase::Transition; }
bool surface_shelter_accepts(const Planet& p,std::uint64_t residents)
{ return !surface_sheltered(p) || residents<=p.engineering.transformation.shelterCapacity; }
bool megaproject_kind(PlanetaryEngineeringKind kind)
{ return kind==PlanetaryEngineeringKind::RadicalTransformation || kind==PlanetaryEngineeringKind::WormholeExperiment; }
bool megaproject_state_valid(const PlanetaryEngineering& e)
{
    const auto& t=e.transformation;const auto& x=e.experiment;
    const auto validAxis=[](const PlanetEnvironment& v){return v.temperature<=100 && v.gravity<=100 && v.radiation<=100;};
    if(e.kind!=PlanetaryEngineeringKind::RadicalTransformation) {
        if(t!=PlanetaryTransformation{})return false;
    } else {
        if(e.strength!=10 || t.phase>TransformationPhase::Complete)return false;
        if(t.phase==TransformationPhase::Preparation) {if(t!=PlanetaryTransformation{})return false;}
        else if(e.work!=engineering_work(e.kind) || !t.initiator || !t.committedTurn
            || t.committedTurn<e.completedTurn || t.advancedTurn<t.committedTurn
            || !validAxis(t.original) || !validAxis(t.natural) || !validAxis(t.target)
            || (t.phase==TransformationPhase::Transition?t.years>=kTransformationYears:t.years!=kTransformationYears)
            || (t.phase==TransformationPhase::Complete && t.paused))return false;
    }
    if(e.kind!=PlanetaryEngineeringKind::WormholeExperiment)return x==EngineeringExperiment{};
    return e.strength==10 && ((!x.activationTurn && !x.openedTurn && !x.entrance)
        || (x.activationTurn>=e.completedTurn && e.work==engineering_work(e.kind)
            && ((!x.openedTurn && !x.entrance) || (x.openedTurn>=x.activationTurn && x.entrance))));
}
bool engineering_dates_valid(const PlanetaryEngineering& e,std::uint64_t turn)
{
    return e.startedTurn<=turn && e.completedTurn<=turn
        && e.transformation.committedTurn<=turn && e.transformation.advancedTurn<=turn
        && e.experiment.activationTurn<=turn && e.experiment.openedTurn<=turn;
}
PlanetEnvironment transformation_target(const RaceProfile& race)
{
    const auto midpoint=[](RaceEnvironmentRange r){return std::uint8_t((unsigned(r.minimum)+r.maximum)/2);};
    return {midpoint(race.habitableTemperature),midpoint(race.habitableGravity),midpoint(race.habitableRadiation)};
}
bool apply_megaproject_action(const GameState& s,Planet& p,const SetPlanetaryEngineeringOrder& order)
{
    auto& e=p.engineering;auto& t=e.transformation;
    if(e.kind!=order.kind || !p.owner || !megaproject_kind(e.kind))return false;
    if(order.action==EngineeringAction::SetOutput)return false;
    if(surface_sheltered(p)) {
        if(order.action==EngineeringAction::Pause){t.paused=true;e.operational=false;return true;}
        if(order.action==EngineeringAction::StartOrResume && engineering_operation_available(s,p)
            && engineering_station_available(s,p)){t.paused=false;return true;}
        return false; // There is no dismantle/retarget/undo after commitment.
    }
    if(e.work!=engineering_work(e.kind) || !engineering_operation_available(s,p)
        || !engineering_station_available(s,p))return false;
    if(order.action==EngineeringAction::CommitTransformation
        && e.kind==PlanetaryEngineeringKind::RadicalTransformation && t.phase==TransformationPhase::Preparation) {
        const auto* owner=find_player(s,p.owner);if(!owner || p.habitat.population)return false;
        t.original=p.environment;t.natural=p.naturalEnvironment.value_or(p.environment);
        t.target=transformation_target(owner->race);t.initiator=p.owner;t.shelterCapacity=p.population;
        t.committedTurn=t.advancedTurn=s.turn;t.phase=TransformationPhase::Transition;
        p.observedHabitability.reset();return true;
    }
    if(order.action==EngineeringAction::ActivateExperiment && e.kind==PlanetaryEngineeringKind::WormholeExperiment
        && !e.experiment.activationTurn && total_colony_population(p)) {
        e.experiment.activationTurn=s.turn;return true;
    }
    return false;
}
bool advance_megaprojects(GameState& s)
{
    const auto year=s.turn+1;
    bool opened=false;
    for(auto& p:s.planets) {
        auto& e=p.engineering;
        if((e.kind==PlanetaryEngineeringKind::RadicalTransformation && e.transformation.phase==TransformationPhase::Complete)
            || (e.kind==PlanetaryEngineeringKind::WormholeExperiment && e.experiment.openedTurn))continue;
        if(!megaproject_kind(e.kind) || !p.owner || !engineering_operation_available(s,p) || !engineering_station_available(s,p))continue;
        if(surface_sheltered(p)) {
            auto& t=e.transformation;
            if(t.paused || t.advancedTurn>=year)continue;
            t.advancedTurn=year;++t.years;
            const auto blend=[&](unsigned from,unsigned to){return std::uint8_t((from*(kTransformationYears-t.years)+to*t.years)/kTransformationYears);};
            p.environment={blend(t.original.temperature,t.target.temperature),blend(t.original.gravity,t.target.gravity),blend(t.original.radiation,t.target.radiation)};
            p.observedHabitability.reset();
            if(t.years==kTransformationYears) {t.phase=TransformationPhase::Complete;p.naturalEnvironment=t.target;}
        } else if(e.kind==PlanetaryEngineeringKind::WormholeExperiment && e.experiment.activationTurn
            && !e.experiment.openedTurn && total_colony_population(p)) {
            const auto* star=find_star(s,p.star);if(!star)continue;
            const auto entrance=open_experimental_wormhole(s,p.owner,star->position,
                (std::uint64_t(p.id)<<32)^e.experiment.activationTurn,kExperimentalWormholeYears);
            if(entrance) {e.experiment.entrance=entrance;e.experiment.openedTurn=year;opened=true;}
        }
    }
    return opened;
}
} // namespace suns
