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
GameState plateau_fixture(bool cool=false)
{
    auto s=test::climate_fixture();test::finish_climate(s,cool?54:46);
    auto& p=s.planets.front();
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,3,std::uint8_t(cool?54:46)}));
    p.environment.temperature=cool?55:45;p.naturalEnvironment=p.environment;p.observedHabitability.reset();
    set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);
    return s;
}
void paid_terraforming_crosses_the_active_controllers_plateau()
{
    for(bool cool:{false,true}) {
        auto s=plateau_fixture(cool);const auto initial=s.planets.front().environment.temperature;
        const auto natural=s.planets.front().naturalEnvironment;
        assert(current_planet_habitability(s,s.planets.front(),s.turn)==96);
        const PlayerOrders first{1,{QueueProductionBatchOrder{1,K::Terraforming}}};
        const auto next=TurnProcessor{}.process(s,{first});
        assert(next.planets.front().environment.temperature==initial+(cool?-1:1));
        assert(current_planet_habitability(next,next.planets.front(),next.turn)==96);
        assert(next.planets.front().productionQueue.empty());
        const auto projected=planned_production_state(make_player_view(s,1).state,first);
        assert(projected.planets.front().productionQueue.size()==1 && s.planets.front().environment.temperature==initial);
        const auto potential=terraforming_potential(s,1,s.planets.front());
        assert(potential.environment.temperature==(cool?47:53) && engineered_environment(potential).temperature==50);
        s=TurnProcessor{}.process(next,{{1,{QueueProductionBatchOrder{1,K::Terraforming,7}}}});
        const auto& p=s.planets.front();assert(p.environment.temperature==(cool?47:53));
        assert(p.naturalEnvironment->temperature==natural->temperature && engineered_environment(p).temperature==50);
        assert(p.engineering.strength==3 && p.engineering.targetTemperature==(cool?54:46));
        assert(current_planet_habitability(s,p,s.turn)==100 && !next_terraforming_environment(s,1,p));
    }
}
void plateau_progress_respects_limits_costs_and_other_axes()
{
    auto s=plateau_fixture();
    s.planets.front().population=4000;s.planets.front().industry=0;
    s=TurnProcessor{}.process(s,{{1,{QueueProductionBatchOrder{1,K::Terraforming}}}});
    assert(s.planets.front().environment.temperature==45 && s.planets.front().productionQueue.size()==1);
    assert(s.planets.front().productionQueue.front().remainingCost>0 && s.planets.front().productionQueue.front().remainingCost<kTerraformingCost);
    for(unsigned year=0;year<10 && !s.planets.front().productionQueue.empty();++year)s=TurnProcessor{}.process(s,{});
    assert(s.planets.front().environment.temperature==46 && s.planets.front().productionQueue.empty());
    auto automatic=plateau_fixture();
    automatic=TurnProcessor{}.process(automatic,{{1,{QueueProductionBatchOrder{1,K::Terraforming,10,ProductionAutomation::MaxTerraform}}}});
    assert(automatic.planets.front().environment.temperature==53 && engineered_environment(automatic.planets.front()).temperature==50);
    automatic=TurnProcessor{}.process(automatic,{});assert(automatic.planets.front().environment.temperature==53);
    auto priority=plateau_fixture();priority.planets.front().environment.gravity=49;
    const auto immediate=next_terraforming_environment(priority,1,priority.planets.front());
    assert(immediate && immediate->gravity==50 && immediate->temperature==45);
    auto limited=plateau_fixture();auto& p=limited.planets.front();
    assert(apply_planetary_engineering_order(limited,p,{1,E::ClimateStabilizer,A::SetOutput,10,55}));
    limited.players.front().race.habitableTemperature={60,80};
    assert(!next_terraforming_environment(limited,1,p)); // Exit66 exceeds natural45 + limit15.
    limited=TurnProcessor{}.process(limited,{{1,{QueueProductionBatchOrder{1,K::Terraforming}}}});
    assert(limited.planets.front().environment.temperature==45 && limited.planets.front().productionQueue.empty());
    auto capped=plateau_fixture();capped.players.front().race.habitableTemperature={99,100};
    capped.planets.front().environment.temperature=95;capped.planets.front().naturalEnvironment=capped.planets.front().environment;
    assert(apply_planetary_engineering_order(capped,capped.planets.front(),{1,E::ClimateStabilizer,A::SetOutput,10,98}));
    assert(!next_terraforming_environment(capped,1,capped.planets.front())); // No progress past raw100.
    auto old=plateau_fixture();old.planets.front().naturalEnvironment->temperature=80;
    const auto undo=next_terraforming_environment(old,1,old.planets.front());
    assert(undo && undo->temperature==46); // Older out-of-band work may still move toward its baseline.
    auto off=plateau_fixture();assert(apply_planetary_engineering_order(off,off.planets.front(),{1,E::ClimateStabilizer,A::SetOutput,0,46}));
    assert(next_terraforming_environment(off,1,off.planets.front())->temperature==46);
}
void half_integer_optimum_preserves_research_unless_the_exit_improves()
{
    for(bool useful:{false,true}) {
        auto s=test::climate_fixture();test::finish_climate(s,useful?52:51);
        auto& owner=s.players.front();owner.race.habitableTemperature={30,71};
        owner.technology.researchActive=true;owner.technology.focus=ResearchField::Electronics;
        owner.technology.progress.fill(0);owner.technology.queuedFocuses.clear();
        auto& p=s.planets.front();p.environment.temperature=55;p.naturalEnvironment=p.environment;
        p.observedHabitability.reset();set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);
        const auto control=TurnProcessor{}.process(s,{});
        const auto automatic=TurnProcessor{}.process(s,{{1,{QueueProductionBatchOrder{1,K::Terraforming,15,ProductionAutomation::MaxTerraform}}}});
        const auto& after=automatic.planets.front();
        assert(after.environment.temperature==(useful?41:55));
        assert(engineered_environment(after).temperature==51 && after.naturalEnvironment->temperature==55);
        assert(current_planet_habitability(automatic,after,automatic.turn)==99);
        const auto focus=std::size_t(ResearchField::Electronics);
        assert(automatic.players.front().technology.levels==control.players.front().technology.levels);
        assert(control.players.front().technology.progress[focus]-automatic.players.front().technology.progress[focus]
            ==(useful?14*kTerraformingCost:0));
        const auto potential=terraforming_potential(s,1,p);
        assert(potential.environment.temperature==(useful?41:55));
        if(!useful) {
            assert(!next_terraforming_environment(s,1,p));
            const auto manual=TurnProcessor{}.process(s,{{1,{QueueProductionBatchOrder{1,K::Terraforming}}}});
            assert(manual.planets.front().environment.temperature==55 && manual.planets.front().productionQueue.empty());
            assert(manual.players.front().technology.progress==control.players.front().technology.progress);
            const auto planned=planned_production_state(make_player_view(s,1).state,{1,{QueueProductionBatchOrder{1,K::Terraforming}}});
            assert(planned.planets.front().productionQueue.empty());
        }
    }
}
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
    paid_terraforming_crosses_the_active_controllers_plateau();
    plateau_progress_respects_limits_costs_and_other_axes();
    half_integer_optimum_preserves_research_unless_the_exit_improves();
    paid_sixty_year_project_uses_real_host_turns_and_milestones();
    interruption_controls_and_terraforming_keep_the_natural_baseline();
    capture_preserves_settings_and_requires_the_new_owners_technology();
    admission_requires_science_funding_and_one_explicit_project();
    computer_uses_received_climate_evidence_and_ordinary_paid_orders();
}
