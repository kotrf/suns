#include "suns/campaign.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include "suns/terraforming.hpp"
#include "suns/unknown_objects.hpp"
#include "suns/computer_player.hpp"
#include "suns/exotic_materials.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;
using E=PlanetaryEngineeringKind;
using A=EngineeringAction;
using K=ProductionKind;

GameState fixture()
{
    auto s=make_demo_game(); s.wormholeRules.spawnChancePerTurn=0;
    s.players[0].race=race_preset(RacePreset::Terran); s.players[0].technology.levels.fill(14);
    s.players[0].technology.researchActive=false;
    auto& p=s.planets[0]; p.environment={40,50,20}; p.naturalEnvironment=p.environment;
    p.population=100000; p.industry=1000; p.mines=0; p.minerals={10000,10000,10000}; p.productionQueue.clear();
    s.orbitalStations[0].hull=OrbitalStationHullType::Starbase;
    for(auto& star:s.stars)star.variability={};
    return s;
}
void start(GameState& s,E kind=E::OrbitalMirrors)
{assert(apply_planetary_engineering_order(s,s.planets[0],{1,kind,A::StartOrResume,10}));}
void complete(GameState& s,E kind=E::OrbitalMirrors)
{
    start(s,kind);
    for(unsigned i=0;i<30;++i) { (void)resolve_colony_production(s,s.planets[0],true); ++s.turn; }
    refresh_planetary_engineering(s);
}
bool near(double a,double b) {return std::abs(a-b)<1e-6;}
void unlocks_materials_and_annual_limit()
{
    auto s=fixture(); auto& p=s.planets[0];
    s.players[0].technology.levels[std::size_t(ResearchField::Energy)]=13;
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalMirrors,A::StartOrResume,10}));
    s.players[0].technology.levels[std::size_t(ResearchField::Energy)]=14;
    s.players[0].race.environmentBased=false; assert(!planetary_engineering_available(s,1));
    s.players[0].race.environmentBased=true; start(s);
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalShades,A::StartOrResume,10}));
    assert(!apply_production_batch(s,p,{1,K::OrbitalMirrors,2}));
    assert(!production_template_valid({"Climate",{{K::OrbitalMirrors,kEngineeringWork}}}));
    auto forecast=forecast_production_queue(s,p,p.productionQueue,40);
    assert(forecast[0].completionTurn==s.turn+30);
    const auto raw=p.environment, natural=*p.naturalEnvironment;
    const auto stocks=p.minerals;
    for(unsigned year=1;year<=30;++year) {
        const auto resolved=resolve_colony_production(s,p,true); ++s.turn;
        assert(p.engineering.work==80*year);
        assert(near(stocks.ironium-p.minerals.ironium,8*year));
        assert(near(stocks.boranium-p.minerals.boranium,4*year));
        assert(p.engineering.operational==(year==30));
        assert(p.environment.temperature==raw.temperature && p.naturalEnvironment->temperature==natural.temperature);
        assert(resolved.completions.size()==(year==30?1:0));
    }
    assert(p.productionQueue.empty() && p.engineering.completedTurn==s.turn);
    assert(near(p.engineering.invested.ironium,240) && near(p.engineering.invested.boranium,120));
    assert(engineered_environment(p).temperature==50);
    assert(engineering_state_valid(p.engineering));
    auto low=fixture(); start(low); low.planets[0].minerals={0.3,0.15,0.3};
    low.planets[0].productionQueue.push_back({K::Factory,10});
    (void)resolve_colony_production(low,low.planets[0],true);
    assert(low.planets[0].engineering.work==3 && near(low.planets[0].minerals.ironium,0));
    assert(low.planets[0].productionQueue.back().remainingCost==10); // Material shortage retains priority.
    low.planets[0].minerals={100,100,100}; low.planets[0].productionQueue.push_back(low.planets[0].productionQueue.front());
    (void)resolve_colony_production(low,low.planets[0],true);
    assert(low.planets[0].engineering.work==83); // Duplicate tokens cannot multiply the annual allowance.
}
void pause_capture_and_station_loss_preserve_investment()
{
    auto s=fixture(); start(s); auto& p=s.planets[0];
    (void)resolve_colony_production(s,p,true); const auto paid=p.engineering.invested;
    assert(apply_planetary_engineering_order(s,p,{1,E::OrbitalMirrors,A::Pause,10}));
    (void)resolve_colony_production(s,p,true); assert(p.engineering.work==80);
    assert(near(p.engineering.invested.ironium,paid.ironium)); start(s);
    s.orbitalStations[0].damagePercent=100;
    (void)resolve_colony_production(s,p,true); assert(p.engineering.work==80);
    assert(engineering_block(s,p)==EngineeringBlock::MissingStation);
    s.orbitalStations[0].damagePercent=0;
    (void)resolve_colony_production(s,p,true); assert(p.engineering.work==160);
    p.productionQueue.clear(); // The ordinary queue cancel path also pauses.
    assert(p.engineering.work==160); start(s);
    // Real invasion must clear scheduling, not paid physical work.
    s.players.push_back({2,"Defender"}); s.players[1].race=race_preset(RacePreset::Terran);
    p.owner=2; p.population=1; s.orbitalStations.clear();
    auto& fleet=s.fleets[0]; fleet.position={0,0}; fleet.colonists=100; fleet.destination.reset();
    const auto invaded=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,fleet.id},{1,0},100,{}}}}});
    assert(invaded.planets[0].owner==1 && invaded.planets[0].productionQueue.empty());
    assert(invaded.planets[0].engineering.work==160 && invaded.planets[0].engineering.builder==1);
    assert(near(invaded.planets[0].engineering.invested.ironium,16));
    auto finished=fixture(); complete(finished); auto& done=finished.planets[0];
    const auto hab=current_planet_habitability(finished,done,finished.turn);
    finished.orbitalStations.clear(); refresh_planetary_engineering(finished);
    assert(!done.engineering.operational && done.engineering.work==2400 && engineered_environment(done).temperature==40);
    assert(current_planet_habitability(finished,done,finished.turn)<hab);
    finished.orbitalStations.push_back({2,1,1,"Replacement",OrbitalStationHullType::Fortress,{OrbitalStationModule::Shipyard}});
    refresh_planetary_engineering(finished); assert(engineered_environment(done).temperature==50);
    assert(apply_planetary_engineering_order(finished,done,{1,E::OrbitalMirrors,A::SetOutput,0}));
    assert(engineered_environment(done).temperature==40 && done.engineering.work==2400);
    assert(apply_planetary_engineering_order(finished,done,{1,E::OrbitalMirrors,A::SetOutput,5}));
    assert(engineered_environment(done).temperature==45);
    const auto minerals=done.minerals;
    assert(apply_planetary_engineering_order(finished,done,{1,E::OrbitalMirrors,A::Dismantle,0}));
    assert(done.engineering.kind==E::None && engineered_environment(done).temperature==40 && done.minerals.ironium==minerals.ironium);
}
void effective_environment_and_terraforming_remain_separate()
{
    auto s=fixture(); s.planets[0].environment.temperature=45; s.planets[0].naturalEnvironment=s.planets[0].environment;
    complete(s); auto& p=s.planets[0];
    // Mirrors overshoot the optimum: conventional terraforming should cool the raw world.
    const auto next=next_terraforming_environment(s,1,p);
    assert(next && next->temperature==44);
    const auto before=current_planet_habitability(s,p,s.turn);
    assert(terraform_planet(s,p) && p.environment.temperature==44);
    assert(current_planet_habitability(s,p,s.turn)>before);
    assert(p.naturalEnvironment->temperature==45 && engineered_environment(p).temperature==54);
    auto shade=fixture(); shade.planets[0].environment.temperature=60; complete(shade,E::OrbitalShades);
    assert(engineered_environment(shade.planets[0]).temperature==50);
    shade.planets[0].environment.temperature=4; assert(engineered_environment(shade.planets[0]).temperature==0);
    p.environment.temperature=99; assert(engineered_environment(p).temperature==100);
}
void supporting_station_forecast_and_research_allocation()
{
    auto s=fixture(); s.orbitalStations.clear();
    assert(queue_station_construction(s,1,{1,{{OrbitalStationHullType::Starbase,{},true,true},0}}));
    start(s);
    const auto forecast=forecast_production_queue(s,s.planets[0],s.planets[0].productionQueue,40);
    assert(forecast.size()==2 && forecast[1].completionTurn==s.turn+30);
    s=TurnProcessor{}.process(s,{});
    assert(s.planets[0].engineering.work==80 && engineering_station_available(s,s.planets[0]));
    s.players[0].technology.researchActive=true; s.players[0].technology.researchAllocationPercent=100;
    const auto minerals=s.planets[0].minerals;
    (void)resolve_colony_production(s,s.planets[0],true);
    assert(s.planets[0].engineering.work==80 && s.planets[0].minerals.ironium==minerals.ironium);
    auto done=fixture(); complete(done);
    assert(queue_station_construction(done,1,{1,{{OrbitalStationHullType::Starbase,{},false,true},1}}));
    done=TurnProcessor{}.process(done,{});
    assert(!done.planets[0].engineering.operational && done.planets[0].engineering.work==2400);
}
void planning_forecasts_reports_and_frozen_observations()
{
    auto s=fixture();
    PlayerOrders orders{1,{SetPlanetaryEngineeringOrder{1,E::OrbitalMirrors,A::StartOrResume,7}}};
    const auto planned=planned_production_state(s,orders);
    assert(s.planets[0].engineering.kind==E::None && planned.planets[0].engineering.strength==7);
    const auto estimate=forecast_production_queue(planned,planned.planets[0],planned.planets[0].productionQueue,40);
    unsigned starts=0,milestones=0,completions=0; bool history=false;
    for(unsigned year=0;year<30;++year) {
        auto result=TurnProcessor{}.process_with_events(s,year==0?std::vector<PlayerOrders>{orders}:std::vector<PlayerOrders>{}); s=std::move(result.state);
        for(const auto& e:result.events) if(e.kind==GameEventKind::EngineeringProgress) {
            if(e.quantity==0)++starts; else if(e.quantity==100)++completions; else ++milestones;
        }
        for(const auto& h:s.players[0].history)for(const auto& m:h.milestones)history|=m.kind==HistoryMilestoneKind::EngineeringCompleted;
    }
    assert(s.turn==*estimate[0].completionTurn && starts==1 && milestones==3 && completions==1 && history);
    assert(near(s.planets[0].engineering.invested.germanium,240));
    s.players.push_back({2,"Observer"}); s.players[1].race=race_preset(RacePreset::Terran);
    set_survey_level(s,2,1,SurveyLevel::OrbitalSurvey,s.turn);
    const auto observation=make_player_view(s,2).state;
    const auto& observed=observation.planets[0]; assert(observed.engineering.work==2400 && observed.engineering.invested.ironium==0);
    const auto observedHab=known_planet_habitability(s,2,1);
    s.planets[0].engineering={}; s.planets[0].environment.temperature=99; s.orbitalStations.clear();
    const auto frozen=make_player_view(s,2).state;
    assert(frozen.planets[0].engineering.work==2400 && frozen.planets[0].engineering.operational);
    assert(known_planet_habitability(s,2,1)==observedHab);
    assert(current_planet_habitability(frozen,frozen.planets[0],s.turn)==current_planet_habitability(observation,observed,s.turn));
    // Interruption packets are emitted once per change, with the observation year.
    auto previous=fixture(); start(previous); auto paused=previous; paused.orbitalStations.clear();
    publish_planetary_engineering_reports(paused,previous);
    auto& reports=paused.players[0].pendingPlayerReports;
    assert(std::count_if(reports.begin(),reports.end(),[](const auto& r){return r.kind==PlayerReportKind::EngineeringPaused;})==1);
    const auto count=reports.size(); publish_planetary_engineering_reports(paused,paused); assert(reports.size()==count);
    auto remote=fixture(); remote.stars[0].position={600,0}; remote.planets[0].homeworld=false; remote.planets[0].radar=PlanetaryRadarLevel::None;
    remote.planets[1].owner=1; remote.planets[1].population=10000; remote.planets[1].homeworld=true;
    auto begun=remote; start(begun); publish_planetary_engineering_reports(begun,remote);
    const auto packet=std::find_if(begun.players[0].pendingPlayerReports.begin(),begun.players[0].pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::EngineeringProgress;});
    assert(packet!=begun.players[0].pendingPlayerReports.end() && packet->deliveryTurn>packet->observedTurn);
}
void magnetic_projects_are_paid_finite_and_reversible()
{
    for(const auto kind:{E::ArtificialMagnetosphere,E::AdaptiveMagnetosphere}) {
        auto s=fixture();s.players[0].technology.levels.fill(18);
        s.players[0].objectKnowledge={{7,{411,7},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::CompactRemnant,s.turn,s.turn}};
        auto& p=s.planets[0];p.environment.radiation=70;p.naturalEnvironment=p.environment;
        const auto natural=*p.naturalEnvironment;const auto minerals=p.minerals;
        const auto strength=engineering_max_strength(kind);const auto total=engineering_work(kind);
        assert(apply_planetary_engineering_order(s,p,{1,kind,A::StartOrResume,strength}));
        assert(!apply_planetary_engineering_order(s,p,{1,kind,A::StartOrResume,strength}));
        assert(!apply_production_batch(s,p,{1,engineering_production_kind(kind),2}));
        assert(!production_template_valid({"Magnetism",{{engineering_production_kind(kind),total}}}));
        assert(forecast_production_queue(s,p,p.productionQueue,80)[0].completionTurn==s.turn+total/80);
        (void)resolve_colony_production(s,p,true);++s.turn;
        const auto paid=p.engineering.invested.ironium;
        assert(p.engineering.work==80 && paid==8 && !p.engineering.operational);
        assert(apply_planetary_engineering_order(s,p,{1,kind,A::Pause,strength}));
        (void)resolve_colony_production(s,p,true);++s.turn;
        assert(p.engineering.work==80 && p.engineering.invested.ironium==paid);
        assert(apply_planetary_engineering_order(s,p,{1,kind,A::StartOrResume,strength}));
        const auto station=s.orbitalStations[0];s.orbitalStations.clear();
        (void)resolve_colony_production(s,p,false);++s.turn;
        assert(p.engineering.work==80 && p.engineering.invested.ironium==paid);
        s.orbitalStations.push_back(station);
        while(p.engineering.work<total) {(void)resolve_colony_production(s,p,true);++s.turn;}
        assert(p.productionQueue.empty() && engineering_state_valid(p.engineering));
        assert(near(minerals.ironium-p.minerals.ironium,total*0.1));
        assert(near(minerals.boranium-p.minerals.boranium,total*0.05));
        assert(engineered_environment(p).radiation==70-strength && engineered_environment(p).temperature==40);
        assert(p.environment.radiation==70 && p.naturalEnvironment->radiation==natural.radiation);
        s.orbitalStations.clear();refresh_planetary_engineering(s);assert(engineered_environment(p).radiation==70);
        s.orbitalStations.push_back(station);refresh_planetary_engineering(s);assert(engineered_environment(p).radiation==70-strength);
        assert(apply_planetary_engineering_order(s,p,{1,kind,A::SetOutput,0}));assert(engineered_environment(p).radiation==70);
        assert(apply_planetary_engineering_order(s,p,{1,kind,A::SetOutput,strength}));
        p.environment.radiation=5;assert(engineered_environment(p).radiation==0);
        const auto stocks=p.minerals;assert(apply_planetary_engineering_order(s,p,{1,kind,A::Dismantle,strength}));
        assert(p.minerals.ironium==stocks.ironium && p.environment.radiation==5);
    }
}
void magnetic_technology_and_delivered_science_gate_capability()
{
    auto s=fixture();auto& p=s.planets[0];
    assert(!apply_planetary_engineering_order(s,p,{1,E::ArtificialMagnetosphere,A::StartOrResume,10}));
    s.players[0].technology.levels.fill(16);
    assert(planetary_engineering_available(s,1,E::ArtificialMagnetosphere));
    assert(!apply_planetary_engineering_order(s,p,{1,E::ArtificialMagnetosphere,A::StartOrResume,11}));
    s.players[0].technology.levels.fill(18);
    s.unknownObjects={{7,{411,7},UnknownObjectKind::CompactRemnant}};s.nextUnknownObjectId=8;
    s.shipDesigns[0].hull=ShipHullType::Destroyer;
    s.shipDesigns[0].components={ShipComponentType::FuelMizer,ShipComponentType::MoleScanner};s.shipDesigns[0].placements.clear();
    auto& f=s.fleets[0];f.position={410,7};f.task=FleetTask::FieldResearch;
    assert(!compact_study_received(s,1));
    assert(!apply_planetary_engineering_order(s,p,{1,E::AdaptiveMagnetosphere,A::StartOrResume,20}));
    research_unknown_objects(s,f,s.turn);
    const auto packet=s.players[0].pendingPlayerReports.back();assert(packet.objectObservation && packet.deliveryTurn>s.turn);
    assert(!compact_study_received(s,1) && !compact_study_received(make_player_view(s,1).state,1));
    auto hidden=s;hidden.unknownObjects.clear();hidden.fleets.clear();
    assert(!compact_study_received(hidden,1)); // Physical presence/kind or another observer's work cannot unlock it.
    s.turn=packet.deliveryTurn;(void)deliver_due_player_reports(s);
    assert(compact_study_received(s,1) && compact_study_received(make_player_view(s,1).state,1));
    const auto unlock=std::find_if(research_unlocks().begin(),research_unlocks().end(),[](const auto& u){return u.requiresCompactStudy;});
    assert(unlock!=research_unlocks().end() && research_unlock_available(s,1,*unlock));
    assert(apply_planetary_engineering_order(s,p,{1,E::AdaptiveMagnetosphere,A::StartOrResume,20}));
    assert(!apply_planetary_engineering_order(s,p,{1,E::ArtificialMagnetosphere,A::StartOrResume,10}));
    s.players.push_back({2,"Captor"});s.players[1].race=race_preset(RacePreset::Terran);s.players[1].technology.levels.fill(18);
    p.owner=2;p.productionQueue.clear();s.orbitalStations[0].owner=2;
    assert(!apply_planetary_engineering_order(s,p,{1,E::AdaptiveMagnetosphere,A::StartOrResume,20}));
    assert(p.engineering.builder==1 && p.engineering.kind==E::AdaptiveMagnetosphere);
    p.engineering.work=4800;p.engineering.invested={480,240,480};p.engineering.completedTurn=s.turn;
    refresh_planetary_engineering(s);assert(!p.engineering.operational && engineering_block(s,p)==EngineeringBlock::CompactStudy);
    s.players[1].objectKnowledge=s.players[0].objectKnowledge;
    refresh_planetary_engineering(s);assert(p.engineering.operational);
    s.players[1].technology.levels[std::size_t(ResearchField::Energy)]=17;
    refresh_planetary_engineering(s);assert(!p.engineering.operational);
}
void computer_magnetic_projects_use_received_science_and_ordinary_costs()
{
    auto s=fixture();s.players[0].controller=PlayerController::Computer;s.players[0].technology.levels.fill(18);
    s.planets[0].environment.radiation=55;s.planets[0].naturalEnvironment=s.planets[0].environment;
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    const auto basic=plan_computer_turn(make_player_view(s,1));
    const auto project=[](const auto& plan) {
        const auto it=std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){return std::holds_alternative<SetPlanetaryEngineeringOrder>(o);});
        assert(it!=plan.orders.orders.end());return std::get<SetPlanetaryEngineeringOrder>(*it);
    };
    assert(project(basic).kind==E::ArtificialMagnetosphere);
    auto hidden=s;hidden.unknownObjects={{7,{411,7},UnknownObjectKind::CompactRemnant}};
    assert(project(plan_computer_turn(make_player_view(hidden,1))).kind==E::ArtificialMagnetosphere);
    s.players[0].objectKnowledge={{7,{411,7},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::CompactRemnant,s.turn,s.turn}};
    const auto advanced=plan_computer_turn(make_player_view(s,1));assert(project(advanced).kind==E::AdaptiveMagnetosphere);
    const auto crystals=s.planets[0].xenocrystal.surface;const auto iron=s.planets[0].minerals.ironium;
    assert(apply_planetary_engineering_order(s,s.planets[0],project(advanced)));
    (void)resolve_colony_production(s,s.planets[0],true);
    assert(s.planets[0].engineering.work==80 && near(s.planets[0].minerals.ironium,iron-8));
    assert(s.planets[0].xenocrystal.surface==crystals);
}
void received_queue_retains_engineering_and_laboratory_order()
{
    for(const bool labFirst:{false,true}) {
        auto s=fixture();s.players[0].technology.levels.fill(18);auto& p=s.planets[0];
        const auto engineering=[&]{assert(apply_planetary_engineering_order(s,p,{1,E::ArtificialMagnetosphere,A::StartOrResume,10}));};
        const auto lab=[&]{assert(apply_resonance_order(s,p,{1}));};
        if(labFirst){lab();engineering();}else{engineering();lab();}
        p.productionQueue.insert(p.productionQueue.begin(),{K::Factory,5});p.productionQueue.push_back({K::Mine,4});
        set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
        const auto received=make_player_view(s,1).state;
        assert(received.planets[0].productionQueue.size()==p.productionQueue.size());
        for(std::size_t i=0;i<p.productionQueue.size();++i)assert(received.planets[0].productionQueue[i].kind==p.productionQueue[i].kind);
    }
}
int main()
{
    unlocks_materials_and_annual_limit(); pause_capture_and_station_loss_preserve_investment();
    effective_environment_and_terraforming_remain_separate(); supporting_station_forecast_and_research_allocation();
    planning_forecasts_reports_and_frozen_observations();
    magnetic_projects_are_paid_finite_and_reversible(); magnetic_technology_and_delivered_science_gate_capability();
    computer_magnetic_projects_use_received_science_and_ordinary_costs();
    received_queue_retains_engineering_and_laboratory_order();
}
