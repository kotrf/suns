#include "atmospheric_regulator_fixture.hpp"
#include "suns/computer_player.hpp"
#include "suns/terraforming.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;using E=PlanetaryEngineeringKind;using A=EngineeringAction;using K=ProductionKind;
namespace {
bool near(double a,double b){return std::abs(a-b)<1e-6;}
void real_seventy_five_year_project_pays_each_year_and_preserves_raw_axes()
{
    auto s=test::atmospheric_fixture();const auto stocks=s.planets.front().minerals;MineralCargo minedTotal;
    unsigned starts=0,milestones=0,completions=0;
    assert(production_cost(K::AtmosphericRegulator)==6000 && engineering_work(E::AtmosphericRegulator)==6000);
    for(unsigned year=1;year<=75;++year) {
        const auto mined=projected_mineral_mining(s,s.planets.front());
        minedTotal.ironium+=mined.ironium;minedTotal.boranium+=mined.boranium;minedTotal.germanium+=mined.germanium;
        const std::vector<PlayerOrders> orders=year==1?std::vector<PlayerOrders>{{1,{test::atmospheric_order(1,A::StartOrResume,10,70)}}}:std::vector<PlayerOrders>{};
        auto result=TurnProcessor{}.process_with_events(s,orders);s=std::move(result.state);const auto& p=s.planets.front();
        assert(p.engineering.work==80*year && p.engineering.targetRadiation==70 && p.engineering.targetTemperature==50);
        assert(p.engineering.operational==(year==75) && p.environment.radiation==40 && p.naturalEnvironment->radiation==40);
        assert(p.environment.temperature==50 && p.environment.gravity==50);
        assert(near(stocks.ironium+minedTotal.ironium-p.minerals.ironium,8*year));
        assert(near(stocks.boranium+minedTotal.boranium-p.minerals.boranium,4*year));
        assert(near(stocks.germanium+minedTotal.germanium-p.minerals.germanium,8*year));
        for(const auto& event:result.events)if(event.kind==GameEventKind::EngineeringProgress && event.productionKind==K::AtmosphericRegulator) {
            if(event.quantity==0)++starts;else if(event.quantity==100)++completions;else ++milestones;
        }
    }
    const auto& p=s.planets.front();assert(p.productionQueue.empty() && engineering_state_valid(p.engineering));
    assert(near(p.engineering.invested.ironium,600) && near(p.engineering.invested.boranium,300) && near(p.engineering.invested.germanium,600));
    assert(engineered_environment(p).radiation==50 && starts==1 && milestones==3 && completions==1);
    assert(std::any_of(s.players.front().history.begin(),s.players.front().history.end(),[](const auto& h) {
        return std::any_of(h.milestones.begin(),h.milestones.end(),[](const auto& m){return m.kind==HistoryMilestoneKind::EngineeringCompleted;});
    }));
}
void targets_support_loss_capture_and_dismantling_keep_physical_meaning()
{
    auto s=test::atmospheric_fixture();test::finish_atmosphere(s,70);auto& p=s.planets.front();
    const auto paid=p.engineering.invested;
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::SetOutput,10,0)) && engineered_environment(p).radiation==30);
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::SetOutput,10,100)) && engineered_environment(p).radiation==50);
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::SetOutput,10,45)) && engineered_environment(p).radiation==45);
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::SetOutput,0,0)) && !p.engineering.operational);
    assert(engineered_environment(p).radiation==40 && p.engineering.targetRadiation==0 && p.naturalEnvironment->radiation==40);
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::SetOutput,10,70)));
    const auto stations=s.orbitalStations;s.orbitalStations.clear();refresh_planetary_engineering(s);
    assert(!p.engineering.operational && p.engineering.targetRadiation==70 && near(p.engineering.invested.ironium,paid.ironium));
    s.orbitalStations=stations;refresh_planetary_engineering(s);assert(p.engineering.operational);
    auto captor=s.players.front();captor.id=333;captor.name="Captor";captor.technology.levels.fill(0);captor.race.habitableRadiation={0,20};
    s.players.push_back(captor);p.owner=333;for(auto& station:s.orbitalStations)if(station.planet==p.id)station.owner=333;
    refresh_planetary_engineering(s);assert(!p.engineering.operational && engineering_block(s,p)==EngineeringBlock::AtmosphericTechnology);
    assert(p.engineering.builder==1 && p.engineering.targetRadiation==70);
    s.players.back().technology.levels.fill(22);refresh_planetary_engineering(s);
    assert(p.engineering.operational && p.engineering.targetRadiation==70 && engineered_environment(p).radiation==50);
    const auto stocks=p.minerals;assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::Dismantle)));
    assert(p.engineering.kind==E::None && p.engineering.targetRadiation==50 && engineered_environment(p).radiation==40);
    assert(near(p.minerals.ironium,stocks.ironium) && near(p.minerals.boranium,stocks.boranium));
}
void admission_partial_payment_and_unused_targets_are_strict()
{
    auto s=test::atmospheric_fixture();
    for(auto field:{ResearchField::Energy,ResearchField::Construction,ResearchField::Biology}) {
        auto locked=s;--locked.players.front().technology.levels[std::size_t(field)];
        assert(!planetary_engineering_available(locked,1,E::AtmosphericRegulator));
    }
    auto native=s;native.players.front().race.primaryTrait=PrimaryRaceTrait::HabitatCivilization;
    assert(!planetary_engineering_available(native,1,E::AtmosphericRegulator));
    auto& p=s.planets.front();
    for(unsigned invalid=0;invalid<4;++invalid) {
        auto order=test::atmospheric_order(1,A::StartOrResume);
        if(invalid==0)order.targetRadiation=101;if(invalid==1)order.strength=11;
        if(invalid==2)order.targetTemperature=40;if(invalid==3){order.kind=E::ClimateStabilizer;order.targetRadiation=40;}
        assert(!apply_planetary_engineering_order(s,p,order) && p.engineering.kind==E::None);
    }
    p.minerals={0.3,0.15,0.3};assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::StartOrResume,10,0)));
    (void)resolve_colony_production(s,p,true);assert(p.engineering.work==3 && near(p.engineering.invested.ironium,0.3));
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::Pause)));
    (void)resolve_colony_production(s,p,true);assert(p.engineering.work==3 && p.engineering.targetRadiation==0);
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::StartOrResume,10,90)) && p.engineering.targetRadiation==0);
    assert(!apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,10,60}));
    auto malformed=p.engineering;malformed.targetTemperature=20;assert(!engineering_state_valid(malformed));
}
GameState plateau(bool downward=false)
{
    auto s=test::atmospheric_fixture();test::finish_atmosphere(s,downward?54:46);auto& p=s.planets.front();
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::SetOutput,3,downward?54:46)));
    p.environment.radiation=downward?55:45;p.naturalEnvironment=p.environment;p.observedHabitability.reset();
    set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);return s;
}
void paid_radiation_terraforming_crosses_only_useful_reachable_plateaus()
{
    for(bool downward:{false,true}) {
        auto s=plateau(downward);const auto raw=s.planets.front().environment.radiation;
        const auto pending=planned_production_state(make_player_view(s,1).state,{1,{QueueProductionBatchOrder{1,K::Terraforming}}});
        assert(!pending.planets.front().productionQueue.empty());
        auto after=TurnProcessor{}.process(s,{{1,{QueueProductionBatchOrder{1,K::Terraforming,8}}}});
        assert(after.planets.front().environment.radiation==(downward?47:53));
        assert(after.planets.front().naturalEnvironment->radiation==raw && engineered_environment(after.planets.front()).radiation==50);
        assert(after.planets.front().productionQueue.empty());
        auto priority=s;priority.planets.front().environment.gravity=49;
        const auto immediate=next_terraforming_environment(priority,1,priority.planets.front());assert(immediate && immediate->gravity==50 && immediate->radiation==raw);
    }
    auto limited=plateau();auto& p=limited.planets.front();
    assert(apply_planetary_engineering_order(limited,p,test::atmospheric_order(1,A::SetOutput,10,55)));
    limited.players.front().race.habitableRadiation={60,80};assert(!next_terraforming_environment(limited,1,p));
    for(bool useful:{false,true}) {
        auto s=test::atmospheric_fixture();test::finish_atmosphere(s,useful?52:51);auto& owner=s.players.front();
        owner.race.habitableRadiation={30,71};owner.technology.researchActive=true;owner.technology.focus=ResearchField::Electronics;
        owner.technology.progress.fill(0);owner.technology.queuedFocuses.clear();
        auto& world=s.planets.front();world.environment.radiation=55;world.naturalEnvironment=world.environment;world.observedHabitability.reset();
        set_survey_level(s,1,world.star,SurveyLevel::DeepSurvey,s.turn);
        const auto control=TurnProcessor{}.process(s,{});
        const auto automatic=TurnProcessor{}.process(s,{{1,{QueueProductionBatchOrder{1,K::Terraforming,15,ProductionAutomation::MaxTerraform}}}});
        assert(automatic.planets.front().environment.radiation==(useful?41:55));
        const auto field=std::size_t(ResearchField::Electronics);
        assert(control.players.front().technology.progress[field]-automatic.players.front().technology.progress[field]==(useful?14*kTerraformingCost:0));
        if(!useful)assert(!next_terraforming_environment(s,1,world));
    }
}
void computer_selects_funded_received_raising_and_prefers_cheaper_shielding()
{
    auto s=test::atmospheric_fixture();s.players.front().controller=PlayerController::Computer;s.players.front().race.habitableRadiation={70,90};s.planets.front().environment.radiation=60;
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    const auto find=[](const ComputerTurnPlan& plan) {
        return std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& order){const auto* p=std::get_if<SetPlanetaryEngineeringOrder>(&order);return p && p->colony==1;});
    };
    const auto plan=plan_computer_turn(make_player_view(s,1));const auto order=find(plan);assert(order!=plan.orders.orders.end());
    const auto& project=std::get<SetPlanetaryEngineeringOrder>(*order);
    assert(project.kind==E::AtmosphericRegulator && project.targetRadiation==80 && project.targetTemperature==50);
    auto hidden=s;hidden.planets.front().minerals={};hidden.planets.front().environment.radiation=100;hidden.orbitalStations.clear();
    const auto same=plan_computer_turn(make_player_view(hidden,1));const auto other=find(same);assert(other!=same.orders.orders.end());
    const auto& received=std::get<SetPlanetaryEngineeringOrder>(*other);assert(received.kind==project.kind && received.targetRadiation==80);
    auto capped=test::atmospheric_fixture();capped.players.front().controller=PlayerController::Computer;capped.players.front().race.habitableRadiation={70,90};
    set_survey_level(capped,1,1,SurveyLevel::DeepSurvey,capped.turn);
    const auto ineffective=plan_computer_turn(make_player_view(capped,1));
    assert(std::none_of(ineffective.orders.orders.begin(),ineffective.orders.orders.end(),[](const auto& o) {
        const auto* p=std::get_if<SetPlanetaryEngineeringOrder>(&o);return p && p->colony==1 && p->kind==E::AtmosphericRegulator;
    })); // Both received scores remain at -100: a surface project would be wasted.
    auto lower=test::atmospheric_fixture();lower.players.front().controller=PlayerController::Computer;lower.planets.front().environment.radiation=60;
    set_survey_level(lower,1,1,SurveyLevel::DeepSurvey,lower.turn);
    const auto cheaper=plan_computer_turn(make_player_view(lower,1));const auto chosen=find(cheaper);assert(chosen!=cheaper.orders.orders.end());
    assert(std::get<SetPlanetaryEngineeringOrder>(*chosen).kind==E::ArtificialMagnetosphere);
}
}
int main()
{
    real_seventy_five_year_project_pays_each_year_and_preserves_raw_axes();
    targets_support_loss_capture_and_dismantling_keep_physical_meaning();
    admission_partial_payment_and_unused_targets_are_strict();
    paid_radiation_terraforming_crosses_only_useful_reachable_plateaus();
    computer_selects_funded_received_raising_and_prefers_cheaper_shielding();
}
