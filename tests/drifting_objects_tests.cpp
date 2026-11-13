#include "drifting_object_fixture.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/science_samples.hpp"
#include "suns/native_orbital.hpp"
#include "suns/unknown_objects.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
using namespace suns;
namespace {
GameState fixture() {return test::drifting_object_fixture();}
ObjectObservation observation(const GameState& s,ObjectKnowledgeLevel level=ObjectKnowledgeLevel::Studied)
{return test::drifting_observation(s,level);}
void generation_is_rare_and_keeps_the_static_sequence()
{
    const GalaxyConfig config{720,120,1100,800,25};const auto a=generate_game(config),b=generate_game(config);
    assert(std::count_if(a.unknownObjects.begin(),a.unknownObjects.end(),[](const auto& x){return x.kind==UnknownObjectKind::DriftingDerelict;})==1);
    auto legacy=a;legacy.unknownObjects.clear();legacy.nextUnknownObjectId=1;generate_unknown_objects(legacy,false);
    assert(a.unknownObjects.size()==legacy.unknownObjects.size()+1 && a.unknownObjects.size()==b.unknownObjects.size());
    for(std::size_t i=0;i<a.unknownObjects.size();++i) {
        const auto& x=a.unknownObjects[i];assert(unknown_object_valid(x));
        assert(x.id==b.unknownObjects[i].id && x.position==b.unknownObjects[i].position && x.velocity==b.unknownObjects[i].velocity);
        if(i<legacy.unknownObjects.size())assert(x.position==legacy.unknownObjects[i].position && x.kind==legacy.unknownObjects[i].kind);
    }
    const auto view=make_player_view(a,1).state;assert(view.unknownObjects.empty() && view.nextUnknownObjectId==1);
}
void motion_and_rendezvous_spend_real_movement_and_fuel()
{
    auto s=fixture();s.players.front().objectKnowledge={observation(s)};
    const auto target=s.unknownObjects.front().position;
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,target,5,{FleetArrivalActionKind::RecoverSample}}}}});
    assert(s.unknownObjects.front().position==Position(112.5,0.5));
    assert(s.fleets.front().position==target && s.fleets.front().scienceSamples.empty());
    assert(s.fleets.front().task==FleetTask::RecoverSample);
    const auto fuel=s.fleets.front().fuel;
    s=TurnProcessor{}.process(s,{});
    assert(s.fleets.front().position==Position(114,1) && s.fleets.front().fuel<fuel);
    assert(s.unknownObjects.front().sampleRecovered && s.fleets.front().scienceSamples.size()==1);
    assert(s.fleets.front().scienceSamples.front().kind==UnknownObjectKind::DriftingDerelict);
    assert(science_sample_mass(s.fleets.front().scienceSamples)==5 && s.fleets.front().task==FleetTask::None);
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,{0,0},5,{FleetArrivalActionKind::UnloadAll,1,FleetCargoKind::All}}}}});
    for(unsigned year=0;year<10 && s.planets.front().scienceSamples.empty();++year)s=TurnProcessor{}.process(s,{});
    assert(s.fleets.front().scienceSamples.empty() && s.planets.front().scienceSamples.size()==1);
    assert(process_science_sample(s,1,{1,7,true}));
    s.players.front().technology.researchActive=true;s.players.front().technology.researchAllocationPercent=20;
    s.players.front().technology.focus=ResearchField::Construction;
    assert(retained_sample_research(s,s.planets.front())==4);
    const auto technology=s.players.front().technology;
    assert(process_science_sample(s,1,{1,7,false}) && !process_science_sample(s,1,{1,7,false}));
    assert(s.planets.front().scienceSamples.empty() && s.unknownObjects.front().sampleRecovered);
    assert(s.players.front().technology.progress==technology.progress);
    assert(s.players.front().pendingPlayerReports.back().quantity==80
        && s.players.front().pendingPlayerReports.back().researchField==ResearchField::Construction);
}
void local_recovery_requires_received_study_and_active_mobile_sensors()
{
    for(unsigned invalid=0;invalid<4;++invalid) {
        auto s=fixture();s.players.front().objectKnowledge={observation(s)};
        s.fleets.front().task=FleetTask::RecoverSample;s.fleets.front().recoveryTarget=7;
        if(invalid==0)s.players.front().objectKnowledge.clear();
        if(invalid==1)s.fleets.front().electronics.mode=EmissionMode::Passive;
        if(invalid==2)s.fleets.front().position={50,0};
        if(invalid==3)s.fleets.front().destination=Position{90,0};
        const auto dest=s.fleets.front().destination;
        prepare_science_sample_recovery(s,s.turn+1);
        assert(s.fleets.front().destination==dest && s.fleets.front().scienceSamples.empty());
    }
    auto s=fixture();s.players.front().objectKnowledge={observation(s)};
    s.fleets.front().task=FleetTask::RecoverSample;s.fleets.front().recoveryTarget=7;
    auto other=s.fleets.front();other.id=2;s.fleets.push_back(other);s.nextFleetId=3;
    auto reversed=s;std::reverse(reversed.fleets.begin(),reversed.fleets.end());
    const auto a=TurnProcessor{}.process(s,{}),b=TurnProcessor{}.process(reversed,{});
    for(const auto& f:a.fleets) {
        const auto same=std::find_if(b.fleets.begin(),b.fleets.end(),[&](const auto& x){return x.id==f.id;});
        assert(same!=b.fleets.end() && f.scienceSamples==same->scienceSamples);
        assert(f.scienceSamples.size()==(f.id==1?1:0));
    }
}
void static_recovery_waiting_for_sensors_does_not_chase_a_neighbour()
{
    auto s=fixture();s.unknownObjects.front().position={119,0};
    s.players.front().objectKnowledge={observation(s)};
    s.unknownObjects.push_back({8,{111,0},UnknownObjectKind::AncientProbe});s.nextUnknownObjectId=9;
    s.players.front().objectKnowledge.push_back({8,{111,0},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::AncientProbe,10,10});
    ElectronicsProgram silent;silent.mode=EmissionMode::RadioSilence;silent.resumeTurn=12;
    s=TurnProcessor{}.process(s,{{1,{SetFleetElectronicsOrder{1,silent},
        MoveFleetOrder{1,{111,0},5,{FleetArrivalActionKind::RecoverSample}}}}});
    assert(s.fleets.front().position==Position(111,0) && s.fleets.front().scienceSamples.empty());
    assert(s.fleets.front().recoveryTarget==8);
    const auto fuel=s.fleets.front().fuel;
    s=TurnProcessor{}.process(s,{});
    assert(s.fleets.front().position==Position(111,0) && s.fleets.front().fuel==fuel);
    assert(s.fleets.front().scienceSamples.size()==1 && s.fleets.front().scienceSamples.front().source==8);
    assert(!s.unknownObjects.front().sampleRecovered && s.unknownObjects.back().sampleRecovered);
}
void recovery_keeps_the_named_moving_body_and_legacy_tasks_stay_put()
{
    auto s=fixture();s.players.front().objectKnowledge={observation(s)};
    s.unknownObjects.push_back({9,{101,0},UnknownObjectKind::DriftingDerelict,false,{1.5,0.5}});s.nextUnknownObjectId=10;
    s.players.front().objectKnowledge.push_back({9,{101,0},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::DriftingDerelict,10,10,Position{1.5,0.5}});
    auto legacy=s;legacy.fleets.front().task=FleetTask::RecoverSample;
    legacy=TurnProcessor{}.process(legacy,{});
    assert(legacy.fleets.front().position==Position(100,0) && legacy.fleets.front().scienceSamples.empty());
    auto depleted=s;depleted.unknownObjects.front().sampleRecovered=true;
    depleted=TurnProcessor{}.process(depleted,{{1,{SetFleetTaskOrder{1,FleetTask::RecoverSample,7}}}});
    assert(depleted.fleets.front().position==depleted.unknownObjects.front().position && depleted.fleets.front().scienceSamples.empty());
    assert(!depleted.unknownObjects.back().sampleRecovered && !depleted.fleets.front().recoveryTarget);
    s=TurnProcessor{}.process(s,{{1,{SetFleetTaskOrder{1,FleetTask::RecoverSample,7}}}});
    assert(s.fleets.front().fuel==depleted.fleets.front().fuel);
    assert(s.fleets.front().position==s.unknownObjects.front().position);
    assert(s.fleets.front().scienceSamples.size()==1 && s.fleets.front().scienceSamples.front().source==7);
    assert(s.unknownObjects.front().sampleRecovered && !s.unknownObjects.back().sampleRecovered);
    assert(!s.fleets.front().recoveryTarget);
}
void ambiguous_new_coordinates_require_an_explicit_target()
{
    auto s=fixture();s.players.front().objectKnowledge={observation(s)};
    s.unknownObjects.push_back({9,{111,0},UnknownObjectKind::AncientProbe});s.nextUnknownObjectId=10;
    s.players.front().objectKnowledge.push_back({9,{111,0},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::AncientProbe,10,10});
    assert(!submit_fleet_route_command(s,1,1,{111,0},5,{FleetArrivalActionKind::RecoverSample},{}));
    assert(!s.fleets.front().destination && s.fleets.front().pendingCommands.empty());
    FleetArrivalAction chosen{FleetArrivalActionKind::RecoverSample};chosen.recoveryTarget=9;
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,{111,0},5,chosen}}}});
    assert(s.fleets.front().scienceSamples.size()==1 && s.fleets.front().scienceSamples.front().source==9);
    assert(!s.unknownObjects.front().sampleRecovered);
}
void newer_fixes_survive_a_late_study_and_coarse_updates()
{
    auto s=fixture();auto& f=s.fleets.front();f.task=FleetTask::FieldResearch;
    research_unknown_objects(s,f,10);assert(s.players.front().pendingPlayerReports.size()==1);
    auto study=s.players.front().pendingPlayerReports.front();assert(study.quantity==32 && study.researchField==ResearchField::Construction);
    study.deliveryTurn=20;s.players.front().pendingPlayerReports.clear();
    advance_unknown_objects(s);advance_unknown_objects(s);observe_unknown_objects(s,12);
    auto fix=s.players.front().pendingPlayerReports.front();fix.deliveryTurn=13;
    s.players.front().pendingPlayerReports={study,fix};s.turn=13;(void)deliver_due_player_reports(s);
    const auto position=known_unknown_object(s,1,7)->position;
    s.turn=20;(void)deliver_due_player_reports(s);const auto* known=known_unknown_object(s,1,7);
    assert(known->level==ObjectKnowledgeLevel::Studied && known->position==position && known->observedTurn==12);
    assert(known->velocity==Position(1.5,0.5));
    s.unknownObjects.front().position={150,0};observe_unknown_objects(s,21);s.turn=21;
    (void)deliver_due_player_reports(s);known=known_unknown_object(s,1,7);
    assert(known->level==ObjectKnowledgeLevel::Studied && known->observedTurn==21 && known->uncertainty==15 && !known->velocity);
    assert(object_observation_valid(*known));
    fix.deliveryTurn=22;s.players.front().pendingPlayerReports={fix};s.turn=22;(void)deliver_due_player_reports(s);
    assert(known_unknown_object(s,1,7)->observedTurn==21);
}
void bound_recovery_survives_coarse_reports_and_command_delay()
{
    auto base=fixture();base.unknownObjects.front().velocity={1.5,0};
    base.players.front().objectKnowledge={observation(base)};
    assert(object_course_contains(base.players.front().objectKnowledge.front(),{129,0}));
    base.unknownObjects.front().position={150,0};++base.turn;
    observe_unknown_objects(base,base.turn);(void)deliver_due_player_reports(base);
    const auto* known=known_unknown_object(base,1,7);
    assert(known && known->level==ObjectKnowledgeLevel::Studied && known->uncertainty==15 && !known->velocity);
    FleetArrivalAction recovery{FleetArrivalActionKind::RecoverSample};recovery.recoveryTarget=7;
    for(bool queued:{false,true})for(bool delayed:{false,true}) {
        auto s=base;
        if(delayed)s.fleets.front().position=s.fleets.front().telemetry.position=Position{400,0};
        const MoveFleetOrder order=queued
            ? MoveFleetOrder{1,{110,0},2,{},{{{129,0},5,recovery}}}
            : MoveFleetOrder{1,{129,0},5,recovery};
        s=TurnProcessor{}.process(s,{{1,{order}}});
        if(delayed) {
            assert(!s.fleets.front().pendingCommands.empty());
            const auto due=s.fleets.front().pendingCommands.front().deliveryTurn;
            while(s.turn<due)s=TurnProcessor{}.process(s,{});
        }
        const auto& f=s.fleets.front();
        if(queued) {
            assert(f.waypointQueue.size()==1 && f.waypointQueue.front().warp==5);
            assert(f.waypointQueue.front().arrivalAction.recoveryTarget==7);
        } else assert(f.warp==5 && f.arrivalAction && f.arrivalAction->recoveryTarget==7);
    }
    for(unsigned invalid=0;invalid<6;++invalid) {
        auto s=base;auto action=recovery;
        if(invalid==0)action.recoveryTarget=0; // No coordinate/course evidence for a new target.
        if(invalid==1)action.recoveryTarget=8;
        if(invalid==2)s.players.front().objectKnowledge.front().level=ObjectKnowledgeLevel::Classified;
        if(invalid==3)s.players.front().objectKnowledge.front().receivedTurn=s.turn+1;
        if(invalid==4)s.shipDesigns.front().components={ShipComponentType::QuickJump5};
        if(invalid==5)s.shipDesigns.front().hull=ShipHullType::Scout;
        assert(!submit_fleet_route_command(s,1,1,{129,0},5,action,{}));
        assert(!s.fleets.front().destination && s.fleets.front().pendingCommands.empty());
    }
    auto hidden=base;hidden.unknownObjects.clear();
    assert(submit_fleet_route_command(hidden,1,1,{129,0},5,recovery,{}));
}
void computer_aims_from_received_course_only()
{
    auto s=fixture();s.players.front().controller=PlayerController::Computer;
    s.players.front().objectKnowledge={observation(s,ObjectKnowledgeLevel::Classified)};
    const auto plan=plan_computer_turn(make_player_view(s,1));
    const auto move=std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){
        const auto* m=std::get_if<MoveFleetOrder>(&o);return m && m->arrivalAction.kind==FleetArrivalActionKind::FieldResearch;
    });
    assert(move!=plan.orders.orders.end());const auto point=std::get<MoveFleetOrder>(*move).destination;
    assert(object_course_contains(s.players.front().objectKnowledge.front(),point));
    assert(point!=s.unknownObjects.front().position);
    s.unknownObjects.front().position={999,999};s.unknownObjects.front().velocity={-1.5,0.5};s.unknownObjects.front().sampleRecovered=true;
    const auto hidden=plan_computer_turn(make_player_view(s,1));
    assert(plan.nextState==hidden.nextState && plan.orders.orders.size()==hidden.orders.orders.size());
    const auto other=std::find_if(hidden.orders.orders.begin(),hidden.orders.orders.end(),[](const auto& o){
        const auto* m=std::get_if<MoveFleetOrder>(&o);return m && m->arrivalAction.kind==FleetArrivalActionKind::FieldResearch;
    });
    assert(other!=hidden.orders.orders.end() && std::get<MoveFleetOrder>(*other).destination==point);
}
void native_stations_receive_and_research_real_cores_without_surface_population()
{
    EmpireSetup setup{"Orbitals"};setup.primaryTrait=PrimaryRaceTrait::HabitatCivilization;
    auto s=generate_campaign({}, {setup});
    s.unknownObjects={{7,{400,0},UnknownObjectKind::DriftingDerelict,true,{1.5,0.5}}};s.nextUnknownObjectId=8;
    s.players.front().objectKnowledge={test::drifting_observation(s)};
    const auto design=s.nextShipDesignId++;
    s.shipDesigns.push_back({design,1,"Specimen tender",ShipHullType::Privateer,{ShipComponentType::QuickJump5,ShipComponentType::RhinoScanner}});
    Fleet f;f.id=s.nextFleetId++;f.owner=1;f.design=design;f.ships={{design,1}};
    f.position=s.stars.front().position;f.fuel=200;f.scienceSamples={{7,UnknownObjectKind::DriftingDerelict,false}};
    s.fleets.push_back(f);
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{f.id,f.position,5,{FleetArrivalActionKind::UnloadAll,1,FleetCargoKind::All}}}}});
    assert(!s.planets.front().population && s.planets.front().scienceSamples.size()==1);
    assert(make_player_view(s,1).state.planets.front().scienceSamples.size()==1);
    const auto colony=s.planets.front().id;
    assert(transfer_science_sample(s,1,{{colony,0,true},{0,f.id},7}));
    assert(transfer_science_sample(s,1,{{0,f.id},{colony,0,true},7}));
    assert(process_science_sample(s,1,{s.planets.front().id,7,true}));
    s.players.front().technology.researchActive=true;s.players.front().technology.researchAllocationPercent=20;
    s.players.front().technology.focus=ResearchField::Construction;
    assert(retained_sample_research(s,s.planets.front())==4);
    s.planets.front().nativeOrbit.service=HabitatService::Reserve;
    assert(retained_sample_research(s,s.planets.front())==0);
    s.orbitalStations.front().damagePercent=100;
    assert(!process_science_sample(s,1,{s.planets.front().id,7,false}));
    assert(s.planets.front().scienceSamples.size()==1);
}
}
int main()
{
    generation_is_rare_and_keeps_the_static_sequence();
    motion_and_rendezvous_spend_real_movement_and_fuel();
    static_recovery_waiting_for_sensors_does_not_chase_a_neighbour();
    local_recovery_requires_received_study_and_active_mobile_sensors();
    recovery_keeps_the_named_moving_body_and_legacy_tasks_stay_put();
    ambiguous_new_coordinates_require_an_explicit_target();
    newer_fixes_survive_a_late_study_and_coarse_updates();
    bound_recovery_survives_coarse_reports_and_command_delay();
    computer_aims_from_received_course_only();
    native_stations_receive_and_research_real_cores_without_surface_population();
}
