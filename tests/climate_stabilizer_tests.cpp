#include "climate_stabilizer_fixture.hpp"
#include "suns/computer_player.hpp"
#include "suns/terraforming.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;
using E=PlanetaryEngineeringKind;using A=EngineeringAction;using K=ProductionKind;
namespace {
bool near(double a,double b){return std::abs(a-b)<1e-6;}
void paid_sixty_year_project_uses_real_host_turns_and_milestones()
{
    auto s=test::climate_fixture();const auto stocks=s.planets.front().minerals;MineralCargo minedTotal;
    unsigned starts=0,milestones=0,completions=0;
    for(unsigned year=1;year<=60;++year) {
        std::vector<PlayerOrders> orders;
        const auto mined=projected_mineral_mining(s,s.planets.front());
        minedTotal.ironium+=mined.ironium;minedTotal.boranium+=mined.boranium;minedTotal.germanium+=mined.germanium;
        if(year==1)orders={{1,{SetPlanetaryEngineeringOrder{1,E::ClimateStabilizer,A::StartOrResume,10,48}}}};
        auto result=TurnProcessor{}.process_with_events(s,orders);s=std::move(result.state);
        const auto& p=s.planets.front();assert(p.engineering.work==80*year && p.engineering.targetTemperature==48);
        assert(p.engineering.operational==(year==60));assert(p.environment.temperature==40 && p.naturalEnvironment->temperature==40);
        assert(near(stocks.ironium+minedTotal.ironium-p.minerals.ironium,8*year)
            && near(stocks.boranium+minedTotal.boranium-p.minerals.boranium,4*year));
        for(const auto& event:result.events)if(event.kind==GameEventKind::EngineeringProgress && event.productionKind==K::ClimateStabilizer) {
            if(event.quantity==0)++starts;else if(event.quantity==100)++completions;else ++milestones;
        }
    }
    const auto& p=s.planets.front();assert(p.productionQueue.empty() && engineering_state_valid(p.engineering));
    assert(near(p.engineering.invested.ironium,480) && near(p.engineering.invested.boranium,240) && near(p.engineering.invested.germanium,480));
    assert(engineered_environment(p).temperature==48 && starts==1 && milestones==3 && completions==1);
    assert(std::any_of(s.players.front().history.begin(),s.players.front().history.end(),[](const auto& h){return std::any_of(h.milestones.begin(),h.milestones.end(),[](const auto& m){return m.kind==HistoryMilestoneKind::EngineeringCompleted;});}));
}
void interruption_controls_and_terraforming_keep_the_natural_baseline()
{
    auto s=test::climate_fixture();test::finish_climate(s);auto& p=s.planets.front();
    assert(engineered_environment(p).temperature==48);
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,3,20}));
    assert(engineered_environment(p).temperature==37);
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,3,80}));
    assert(engineered_environment(p).temperature==43);
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,10,48}));
    const auto station=s.orbitalStations.front();s.orbitalStations.clear();refresh_planetary_engineering(s);
    assert(!p.engineering.operational && engineered_environment(p).temperature==40 && p.engineering.targetTemperature==48);
    s.orbitalStations.push_back(station);refresh_planetary_engineering(s);assert(engineered_environment(p).temperature==48);
    p.environment.temperature=35;
    const auto next=next_terraforming_environment(s,1,p);assert(next && next->temperature==36);
    assert(terraform_planet(s,p) && p.environment.temperature==36 && p.naturalEnvironment->temperature==40);
    assert(engineered_environment(p).temperature==46);
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,0,48}));
    assert(engineered_environment(p).temperature==36);
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,10,0}));
    assert(engineered_environment(p).temperature==26); // Target zero is valid; output zero is off.
    const auto stocks=p.minerals;
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::Dismantle,10,0}));
    assert(p.engineering.kind==E::None && p.environment.temperature==36 && near(p.minerals.ironium,stocks.ironium));
}
void capture_preserves_settings_and_requires_the_new_owners_technology()
{
    for(bool complete:{false,true}) {
        auto s=test::climate_fixture();
        if(complete)test::finish_climate(s,62);
        else {
            assert(apply_planetary_engineering_order(s,s.planets.front(),{1,E::ClimateStabilizer,A::StartOrResume,10,62}));
            (void)resolve_colony_production(s,s.planets.front(),true);++s.turn;
        }
        const auto work=s.planets.front().engineering.work;const auto paid=s.planets.front().engineering.invested;
        Player captor=s.players.front();captor.id=2;captor.name="Captor";captor.surveyKnowledge.clear();captor.pendingSurveyReports.clear();captor.pendingPlayerReports.clear();captor.history.clear();
        captor.technology.levels[std::size_t(ResearchField::Biology)]=17;s.players.push_back(captor);
        s.planets.front().population=1;s.orbitalStations.clear();
        auto f=make_demo_game().fleets.front();f.id=s.nextFleetId++;f.owner=2;f.position=s.stars.front().position;f.colonists=1000;f.fuel=100;
        f.destination.reset();f.telemetry.observedTurn=s.turn;f.telemetry.position=f.position;f.telemetry.colonists=1000;
        s.fleets.push_back(f);
        s=TurnProcessor{}.process(s,{{2,{TransferCargoOrder{{0,f.id},{1,0},1000,{}}}}});
        auto& p=s.planets.front();assert(p.owner==2 && p.productionQueue.empty());
        assert(p.engineering.work==work && p.engineering.builder==1 && p.engineering.targetTemperature==62);
        assert(near(p.engineering.invested.ironium,paid.ironium));
        s.orbitalStations.push_back({s.nextOrbitalStationId++,2,1,"Captured-world support",OrbitalStationHullType::Starbase,{OrbitalStationModule::Shipyard}});
        refresh_planetary_engineering(s);assert(!p.engineering.operational && engineering_block(s,p)==EngineeringBlock::ClimateTechnology);
        s.players.back().technology.levels[std::size_t(ResearchField::Biology)]=18;refresh_planetary_engineering(s);
        if(complete)assert(p.engineering.operational && engineered_environment(p).temperature==50);
        else {
            assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::StartOrResume,10,5}));
            assert(p.engineering.targetTemperature==62);
            p.population=1000000; // Restore staffing for the independent construction-resume check.
            (void)resolve_colony_production(s,p,true);
            assert(p.engineering.work>work && p.engineering.builder==1);
        }
    }
}
void admission_requires_science_funding_and_one_explicit_project()
{
    for(auto field:{ResearchField::Energy,ResearchField::Construction,ResearchField::Biology}) {
        auto s=test::climate_fixture();s.players.front().technology.levels[std::size_t(field)]=field==ResearchField::Biology?17:19;
        assert(!apply_planetary_engineering_order(s,s.planets.front(),{1,E::ClimateStabilizer,A::StartOrResume,10,48}));
        assert(s.planets.front().engineering.kind==E::None);
    }
    auto s=test::climate_fixture();auto& p=s.planets.front();
    auto native=s;native.players.front().race.primaryTrait=PrimaryRaceTrait::HabitatCivilization;
    assert(!planetary_engineering_available(native,1,E::ClimateStabilizer));
    const auto& catalog=research_unlocks();const auto unlock=std::find_if(catalog.begin(),catalog.end(),[](const auto& u){return u.name=="Climate stabilizer";});
    assert(unlock!=catalog.end() && research_unlock_available(s,1,*unlock) && !research_unlock_applicable(native,1,*unlock));
    assert(!apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::StartOrResume,10,101}));
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalMirrors,A::StartOrResume,10,48}));
    assert(!apply_production_batch(s,p,{1,K::ClimateStabilizer,2}));
    assert(!production_template_valid({"No repeated climate",{{K::ClimateStabilizer,4800}}}));
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::StartOrResume,10,48}));
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalMirrors,A::StartOrResume,10}));
    p.minerals={0.3,0.15,0.3};(void)resolve_colony_production(s,p,true);assert(p.engineering.work==3);
    const auto paid=p.engineering.invested;assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::Pause,10}));
    (void)resolve_colony_production(s,p,true);assert(p.engineering.work==3 && near(p.engineering.invested.ironium,paid.ironium));
}
void computer_uses_received_climate_evidence_and_ordinary_paid_orders()
{
    auto s=test::climate_fixture();s.players.front().controller=PlayerController::Computer;
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    const auto choose=[](const GameState& state) {
        const auto plan=plan_computer_turn(make_player_view(state,1));
        const auto i=std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& order){return std::holds_alternative<SetPlanetaryEngineeringOrder>(order);});
        assert(i!=plan.orders.orders.end());return std::get<SetPlanetaryEngineeringOrder>(*i);
    };
    const auto order=choose(s);assert(order.kind==E::ClimateStabilizer && order.targetTemperature==50);
    auto hidden=s;hidden.planets.front().environment.temperature=99;hidden.planets.front().minerals={};hidden.orbitalStations.clear();
    const auto same=choose(hidden);assert(same.kind==order.kind && same.targetTemperature==order.targetTemperature && same.strength==order.strength);
    assert(s.planets.front().engineering.kind==E::None);
    s=TurnProcessor{}.process(s,{{1,{order}}});assert(s.planets.front().engineering.work==80 && s.planets.front().engineering.targetTemperature==50);
}
}
int main()
{
    paid_sixty_year_project_uses_real_host_turns_and_milestones();
    interruption_controls_and_terraforming_keep_the_natural_baseline();
    capture_preserves_settings_and_requires_the_new_owners_technology();
    admission_requires_science_funding_and_one_explicit_project();
    computer_uses_received_climate_evidence_and_ordinary_paid_orders();
}
