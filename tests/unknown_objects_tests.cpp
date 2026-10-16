#include "suns/unknown_objects.hpp"
#include "suns/communications.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace suns;
GameState fixture()
{
    GameState s; s.turn=10; s.players={{1,"Observer"},{2,"Other"}}; s.wormholeRules.spawnChancePerTurn=0;
    s.stars={{1,"HQ",{0,0}},{2,"Other HQ",{-500,0}}};
    for(auto id:{1U,2U}) {
        Planet p; p.id=id; p.star=id; p.owner=id; p.population=10000;
        p.homeworld=true; p.radar=PlanetaryRadarLevel::Full; s.planets.push_back(p);
        s.players[id-1].technology.researchActive=false;
    }
    s.unknownObjects={{7,{411,7},UnknownObjectKind::AncientProbe}}; s.nextUnknownObjectId=8;
    s.shipDesigns={{1,1,"Scout",ShipHullType::Destroyer,{ShipComponentType::FuelMizer,ShipComponentType::MoleScanner}}};
    Fleet f; f.id=1; f.owner=1; f.design=1; f.ships={{1,1}}; f.position={370,7}; f.fuel=1000; s.fleets={f};
    return s;
}
void generation_is_rare_stable_and_separate_from_player_knowledge()
{
    GalaxyConfig config{721,120,1100,800,25}; const auto a=generate_game(config),b=generate_game(config);
    assert(a.unknownObjects.size()==2 && a.nextUnknownObjectId==3);
    for(std::size_t n=0;n<a.unknownObjects.size();++n) {
        const auto& x=a.unknownObjects[n]; assert(x.id==b.unknownObjects[n].id && x.kind==b.unknownObjects[n].kind);
        assert(x.position==b.unknownObjects[n].position);
        for(const auto& star:a.stars) assert(distance_between(star.position,x.position)>=30);
    }
    auto again=a; generate_unknown_objects(again); assert(again.nextUnknownObjectId==a.nextUnknownObjectId);
    const auto player=make_player_view(a,1).state;
    assert(player.unknownObjects.empty() && player.nextUnknownObjectId==1 && player.players[0].objectKnowledge.empty());
}
void detection_is_an_uncertain_immutable_delayed_observation()
{
    auto s=fixture(); observe_unknown_objects(s,10);
    assert(s.players[0].pendingPlayerReports.size()==1 && !known_unknown_object(s,1,7));
    const auto packet=s.players[0].pendingPlayerReports.front(); assert(packet.objectObservation);
    const auto x=*packet.objectObservation; assert(x.level==ObjectKnowledgeLevel::Detected && !x.kind && x.uncertainty==15);
    assert(x.position!=s.unknownObjects[0].position && distance_between(x.position,s.unknownObjects[0].position)<=15);
    assert(packet.deliveryTurn>10 && s.players[1].pendingPlayerReports.empty());
    s.unknownObjects[0].position={999,999}; s.unknownObjects[0].kind=UnknownObjectKind::CompactRemnant; s.fleets.clear();
    s.turn=packet.deliveryTurn; const auto events=deliver_due_player_reports(s);
    assert(events.size()==1 && events[0].position==x.position && events[0].objectObservation->receivedTurn==s.turn);
    const auto* known=known_unknown_object(s,1,7); assert(known && !known->kind && known->position==x.position);
    const auto view=make_player_view(s,1).state; assert(view.unknownObjects.empty());
    assert(*known_unknown_object(view,1,7)==*known);
}
void classification_newer_evidence_and_routine_reports()
{
    auto s=fixture(); s.fleets[0].position={400,7}; observe_unknown_objects(s,10);
    auto close=s.players[0].pendingPlayerReports[0]; assert(close.objectObservation->level==ObjectKnowledgeLevel::Classified);
    assert(close.objectObservation->kind==UnknownObjectKind::AncientProbe && close.objectObservation->position==s.unknownObjects[0].position);
    close.deliveryTurn=12; s.players[0].pendingPlayerReports.clear(); s.fleets[0].position={370,7}; observe_unknown_objects(s,11);
    auto distant=s.players[0].pendingPlayerReports[0]; distant.deliveryTurn=13; s.players[0].pendingPlayerReports={close,distant};
    s.turn=12; assert(deliver_due_player_reports(s).size()==1); s.turn=13;
    assert(deliver_due_player_reports(s).empty() && known_unknown_object(s,1,7)->level==ObjectKnowledgeLevel::Classified);
    auto repeat=close; repeat.observedTurn=repeat.objectObservation->observedTurn=14; repeat.deliveryTurn=15;
    s.players[0].pendingPlayerReports={repeat}; s.turn=15;
    assert(deliver_due_player_reports(s).empty() && known_unknown_object(s,1,7)->observedTurn==14);
}
void passive_or_insufficient_equipment_does_not_reveal_objects()
{
    auto s=fixture(); s.fleets[0].electronics.mode=EmissionMode::Passive; observe_unknown_objects(s,10);
    assert(s.players[0].pendingPlayerReports.empty());
    s.fleets[0].task=FleetTask::FieldResearch; research_unknown_objects(s,s.fleets[0],10);
    assert(s.players[0].fieldScience.empty());
    s.fleets[0].electronics.mode=EmissionMode::Standard; s.shipDesigns[0].components.pop_back();
    observe_unknown_objects(s,10); assert(s.players[0].pendingPlayerReports.empty());
    auto action=FleetArrivalAction{FleetArrivalActionKind::FieldResearch};
    assert(!fleet_work_order_error(s,s.fleets[0],s.unknownObjects[0].position,action.kind).empty());
}
void field_research_is_explicit_finite_and_usable_only_after_delivery()
{
    auto s=fixture(); s.fleets[0].position={410,7}; s.fleets[0].task=FleetTask::FieldResearch;
    const auto before=s.players[0].technology;
    research_unknown_objects(s,s.fleets[0],11); research_unknown_objects(s,s.fleets[0],12);
    assert(s.players[0].fieldScience.size()==1 && s.players[0].pendingPlayerReports.size()==1);
    const auto packet=s.players[0].pendingPlayerReports[0]; assert(packet.kind==PlayerReportKind::ScientificData);
    assert(packet.quantity==24 && packet.objectObservation->level==ObjectKnowledgeLevel::Studied && packet.deliveryTurn>11);
    assert(s.players[0].technology.progress==before.progress && s.players[0].technology.levels==before.levels);
    auto copy=packet; copy.deliveryTurn=packet.deliveryTurn+1; s.players[0].pendingPlayerReports.push_back(copy);
    s.fleets.clear(); s.unknownObjects.clear(); s.turn=packet.deliveryTurn;
    const auto events=deliver_due_player_reports(s); assert(!events.empty());
    assert(known_unknown_object(s,1,7)->level==ObjectKnowledgeLevel::Studied);
    const auto delivered=s.players[0].technology; assert(delivered.progress!=before.progress || delivered.levels!=before.levels);
    ++s.turn; assert(deliver_due_player_reports(s).empty());
    assert(s.players[0].technology.progress==delivered.progress && s.players[0].technology.levels==delivered.levels);
}
void route_actions_and_receipt_year_history_use_ordinary_processing()
{
    auto s=fixture(); ObjectObservation known{7,{410,10},15,ObjectKnowledgeLevel::Detected,{},9,10};
    s.players[0].objectKnowledge={known}; const FleetArrivalAction study{FleetArrivalActionKind::FieldResearch};
    assert(fleet_work_order_error(make_player_view(s,1).state,s.fleets[0],known.position,study.kind).empty());
    auto result=TurnProcessor{}.process_with_events(s,{{1,{MoveFleetOrder{1,known.position,8,study}}}});
    for(int year=0;year<8 && result.state.fleets[0].task!=FleetTask::FieldResearch;++year)
        result=TurnProcessor{}.process_with_events(result.state,{});
    assert(result.state.fleets[0].task==FleetTask::FieldResearch);
    assert(result.state.unknownObjects.size()==1 && result.state.players[0].fieldScience.size()==1);
    bool history=false;
    for(int year=0;year<10;++year) {
        for(const auto& h:result.state.players[0].history) for(const auto& m:h.milestones)
            if(m.kind==HistoryMilestoneKind::UnknownObjectStudied) { assert(m.object==7 && h.turn>=m.observedTurn); history=true; }
        if(history) break;
        result=TurnProcessor{}.process_with_events(result.state,{});
    }
    assert(history && result.state.players[0].fieldScience.size()==1);
}
void computer_expeditions_use_received_positions_and_ordinary_orders()
{
    auto s=make_demo_game(); auto& player=s.players[0]; player.controller=PlayerController::Computer;
    s.unknownObjects={{7,{900,900},UnknownObjectKind::CompactRemnant}}; s.nextUnknownObjectId=8;
    auto hidden=plan_computer_turn(make_player_view(s,1));
    assert(std::none_of(hidden.nextState.fleetGoals.begin(),hidden.nextState.fleetGoals.end(),[](const auto& g){return g.kind==ComputerFleetGoalKind::InvestigateObject;}));
    player.objectKnowledge={{7,{10,0},15,ObjectKnowledgeLevel::Detected,{},1,1}};
    const auto plan=plan_computer_turn(make_player_view(s,1));
    const auto move=std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o) {
        const auto* m=std::get_if<MoveFleetOrder>(&o); return m && m->arrivalAction.kind==FleetArrivalActionKind::FieldResearch;
    });
    assert(move!=plan.orders.orders.end() && std::get<MoveFleetOrder>(*move).destination==Position(10,0));
    assert(std::any_of(plan.nextState.fleetGoals.begin(),plan.nextState.fleetGoals.end(),[](const auto& g){return g.kind==ComputerFleetGoalKind::InvestigateObject && g.target==7;}));
    s.wormholeRules.spawnChancePerTurn=0; s.unknownObjects[0].position={10,0};
    auto next=resolve_controlled_campaign_turn(s,{}).state;
    assert(known_unknown_object(next,1,7)->level==ObjectKnowledgeLevel::Studied);
    next=resolve_controlled_campaign_turn(next,{}).state;
    assert(next.fleets[0].task==FleetTask::None && next.players[0].fieldScience.size()==1);
    assert(std::none_of(next.players[0].computer.fleetGoals.begin(),next.players[0].computer.fleetGoals.end(),[](const auto& g){return g.kind==ComputerFleetGoalKind::InvestigateObject;}));
}

void uncertain_areas_require_appropriate_scanners()
{
    auto s=make_demo_game(); s.players[0].controller=PlayerController::Computer;
    s.shipDesigns[0].components.back()=ShipComponentType::BatScanner;
    s.players[0].objectKnowledge={{7,{10,0},15,ObjectKnowledgeLevel::Detected,{},1,1}};
    const auto plan=plan_computer_turn(make_player_view(s,1));
    assert(std::none_of(plan.nextState.fleetGoals.begin(),plan.nextState.fleetGoals.end(),[](const auto& g){return g.kind==ComputerFleetGoalKind::InvestigateObject;}));
    assert(object_study_radius(s,s.fleets[0])==1);
    assert(!fleet_work_order_error(s,s.fleets[0],{10,0},FleetArrivalActionKind::FieldResearch).empty());
    auto& x=s.players[0].objectKnowledge[0]; x.uncertainty=0; x.level=ObjectKnowledgeLevel::Classified; x.kind=UnknownObjectKind::AncientProbe;
    assert(fleet_work_order_error(s,s.fleets[0],{10,0},FleetArrivalActionKind::FieldResearch).empty());
}

int main()
{
    uncertain_areas_require_appropriate_scanners();
    computer_expeditions_use_received_positions_and_ordinary_orders();
    generation_is_rare_stable_and_separate_from_player_knowledge(); detection_is_an_uncertain_immutable_delayed_observation();
    classification_newer_evidence_and_routine_reports(); passive_or_insufficient_equipment_does_not_reveal_objects();
    field_research_is_explicit_finite_and_usable_only_after_delivery(); route_actions_and_receipt_year_history_use_ordinary_processing();
    std::cout<<"unknown object and expedition tests passed\n";
}
