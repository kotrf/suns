#include "suns/intergalactic_visitors.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include "suns/communications.hpp"
#include "suns/equipment.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace suns;
namespace {
void demo_exchange_and_real_payment()
{
    auto s=make_visitor_demo(); assert(known_visitor(s,1,1)); assert(s.players[0].acquiredEquipment.empty());
    const auto id=s.fleets[0].id; const auto fuel=s.fleets[0].fuel;
    assert(ship_design_valid(*find_ship_design(s,s.fleets[0].design)));
    assert(ship_design_available_to_player(s,1,*find_ship_design(s,s.fleets[0].design)));
    assert(fleet_cargo_used(s,s.fleets[0])<=fleet_cargo_capacity(s,s.fleets[0]));
    assert(!equipment_access_applicable(s,1,*equipment_technology(kVisitorGift)));
    s=TurnProcessor{}.process(s,{{1,{EncounterVisitorOrder{id,1,8}}}});
    assert(s.visitors[0].customer==1 && s.fleets[0].minerals.germanium==0);
    assert(s.fleets[0].fuel<fuel && s.fleets[0].position.x<0 && !s.fleets[0].visitor.track.id);
    assert(s.visitors[0].position!=s.fleets[0].position); // Visitor continues after the timed rendezvous.
    assert(s.players[0].acquiredEquipment==std::vector<ShipComponentType>{kVisitorGift});
    assert(equipment_access_applicable(s,1,*equipment_technology(kVisitorGift)));
    const auto after=s.fleets[0].minerals.germanium;
    s=TurnProcessor{}.process(s,{{1,{EncounterVisitorOrder{id,1,8},EncounterVisitorOrder{id,1,8}}}});
    assert(s.fleets[0].minerals.germanium==after && s.players[0].acquiredEquipment.size()==1);
    auto insufficient=make_visitor_demo(); insufficient.fleets[0].minerals.germanium=49;
    insufficient=TurnProcessor{}.process(insufficient,{{1,{EncounterVisitorOrder{id,1,8}}}});
    assert(!insufficient.visitors[0].customer && insufficient.fleets[0].minerals.germanium==49 && insufficient.players[0].acquiredEquipment.empty());
}
GameState remote()
{
    auto s=make_visitor_demo(); auto& f=s.fleets[0]; f.position.x+=900;
    s.visitors[0].position.x+=900; s.players[0].visitorKnowledge[0].position.x+=900;
    return s;
}
void gift_and_sightings_travel_as_immutable_packets()
{
    auto s=remote(); auto& f=s.fleets[0]; apply_visitor_command(s,f,{s.players[0].visitorKnowledge[0],8});
    s=TurnProcessor{}.process(s,{});
    assert(s.visitors[0].customer==1 && s.fleets[0].minerals.germanium==0 && s.players[0].acquiredEquipment.empty());
    const auto paid=std::find_if(s.players[0].pendingVisitorReports.begin(),s.players[0].pendingVisitorReports.end(),[](const auto& r){return r.kind==VisitorReportKind::Exchanged;});
    assert(paid!=s.players[0].pendingVisitorReports.end() && paid->deliveryTurn>s.turn);
    const auto packet=*paid; s.visitors[0].position={5000,5000}; s.visitors[0].velocity={-100,0};
    s.fleets.clear(); s.turn=packet.deliveryTurn; const auto events=deliver_visitor_reports(s);
    assert(s.players[0].acquiredEquipment.size()==1);
    assert(std::any_of(events.begin(),events.end(),[](const auto& e){return e.kind==GameEventKind::VisitorExchangeCompleted;}));
    assert(known_visitor(s,1,1)->position==packet.observation.position);
    s.players[0].pendingVisitorReports.push_back(packet); assert(deliver_visitor_reports(s).empty());

    auto unseen=remote(); unseen.fleets[0].position.x=unseen.visitors[0].position.x+10; unseen.players[0].visitorKnowledge.clear(); observe_visitors(unseen);
    assert(!known_visitor(unseen,1,1) && !unseen.players[0].pendingVisitorReports.empty());
    const auto observation=unseen.players[0].pendingVisitorReports.front();
    unseen.visitors.clear(); unseen.fleets.clear(); unseen.turn=observation.deliveryTurn; (void)deliver_visitor_reports(unseen);
    assert(known_visitor(unseen,1,1)->position==observation.observation.position);
    const auto view=make_player_view(unseen,1).state;
    assert(view.visitors.empty() && view.nextVisitorId==1 && view.players[0].pendingVisitorReports.empty());
}
void delayed_commands_and_hidden_truth_do_not_change_navigation()
{
    auto s=remote(); const auto id=s.fleets[0].id;
    assert(submit_visitor_command(s,1,{id,1,8}) && !s.fleets[0].visitor.track.id);
    const auto due=s.fleets[0].pendingCommands.back().deliveryTurn; assert(due>s.turn);
    const auto velocity=s.players[0].visitorKnowledge[0].velocity;
    s.players[0].visitorKnowledge[0].velocity={0,121};
    s.turn=due-1; deliver_due_fleet_commands(s); assert(!s.fleets[0].visitor.track.id);
    s.turn=due; deliver_due_fleet_commands(s); assert(s.fleets[0].visitor.track.velocity==velocity);

    s=make_visitor_demo(); s.fleets[0].electronics.mode=EmissionMode::Passive;
    assert(submit_visitor_command(s,1,{s.fleets[0].id,1,8}));
    auto other=s; other.visitors[0].position={6000,5000}; other.visitors[0].customer=1; other.visitors[0].velocity={-100,30};
    prepare_visitor_navigation(s); prepare_visitor_navigation(other);
    assert(s.fleets[0].destination==other.fleets[0].destination && s.fleets[0].warp==other.fleets[0].warp);
    auto miss=TurnProcessor{}.process(other,{}); assert(miss.fleets[0].minerals.germanium==50 && miss.players[0].acquiredEquipment.empty());
    auto noFuel=make_visitor_demo(); noFuel.fleets[0].fuel=0; noFuel.orbitalStations.clear();
    noFuel=TurnProcessor{}.process(noFuel,{{1,{EncounterVisitorOrder{noFuel.fleets[0].id,1,8}}}});
    assert(!noFuel.visitors[0].customer && noFuel.fleets[0].minerals.germanium==50);

    s=remote(); s.visitors[0].departureTurn=100; s.players[0].visitorKnowledge[0].departureTurn=100;
    apply_visitor_command(s,s.fleets[0],{s.players[0].visitorKnowledge[0],8});
    assert(submit_visitor_command(s,1,{s.fleets[0].id,0,8}));
    const auto cancelDue=s.fleets[0].pendingCommands.back().deliveryTurn;
    s.turn=cancelDue-1; deliver_due_fleet_commands(s); assert(s.fleets[0].visitor.track.id);
    s.turn=cancelDue; deliver_due_fleet_commands(s); assert(!s.fleets[0].visitor.track.id && !s.fleets[0].destination);
    // A later cancellation must also supersede an older launch still in flight.
    for (const auto position : {0.0,600.0}) {
        auto superseded=remote(); const auto fleet=superseded.fleets[0].id;
        assert(submit_visitor_command(superseded,1,{fleet,1,8}));
        const auto launchDue=superseded.fleets[0].pendingCommands[0].deliveryTurn;
        ++superseded.turn; superseded.fleets[0].position={position,0};
        assert(submit_visitor_command(superseded,1,{fleet,0,8}));
        if (!superseded.fleets[0].pendingCommands.empty()) {
            const auto stopDue=superseded.fleets[0].pendingCommands.back().deliveryTurn; assert(stopDue<launchDue);
            superseded.turn=stopDue; deliver_due_fleet_commands(superseded);
        }
        assert(superseded.fleets[0].pendingCommands.empty());
        superseded.turn=launchDue; deliver_due_fleet_commands(superseded); assert(!superseded.fleets[0].visitor.track.id);
    }
}
void silent_receipts_and_old_measurements()
{
    auto s=remote(); s.fleets[0].electronics.mode=EmissionMode::RadioSilence;
    apply_visitor_command(s,s.fleets[0],{s.players[0].visitorKnowledge[0],8});
    s=TurnProcessor{}.process(s,{});
    assert(s.visitors[0].customer==1 && s.players[0].acquiredEquipment.empty());
    const auto paid=std::find_if(s.players[0].pendingVisitorReports.begin(),s.players[0].pendingVisitorReports.end(),[](const auto& r){return r.kind==VisitorReportKind::Exchanged;});
    assert(paid!=s.players[0].pendingVisitorReports.end() && paid->deliveryTurn==kAwaitingTransmission);
    s.fleets[0].electronics={}; (void)deliver_visitor_reports(s);
    const auto sent=std::find_if(s.players[0].pendingVisitorReports.begin(),s.players[0].pendingVisitorReports.end(),[](const auto& r){return r.kind==VisitorReportKind::Exchanged;});
    assert(sent!=s.players[0].pendingVisitorReports.end() && sent->deliveryTurn!=kAwaitingTransmission && sent->deliveryTurn>s.turn);
    const auto packet=*sent; s.fleets.clear(); s.turn=packet.deliveryTurn; (void)deliver_visitor_reports(s);
    assert(s.players[0].acquiredEquipment.size()==1 && !known_visitor(s,1,1)->available);
    auto old=packet; old.kind=VisitorReportKind::Sighted; old.observation.available=true; old.deliveryTurn=s.turn;
    s.players[0].pendingVisitorReports.push_back(old); assert(deliver_visitor_reports(s).empty());
    assert(!known_visitor(s,1,1)->available);
}
void physical_refusal_delivers_evidence_but_navigation_failure_keeps_its_age()
{
    auto s=remote(); s.players.push_back({2,"Earlier customer"}); s.visitors[0].customer=2;
    s.fleets[0].electronics={EmissionMode::RadioSilence,63};
    apply_visitor_command(s,s.fleets[0],{s.players[0].visitorKnowledge[0],8});
    s=TurnProcessor{}.process(s,{});
    const auto refusal=std::find_if(s.players[0].pendingVisitorReports.begin(),s.players[0].pendingVisitorReports.end(),
        [](const auto& r){return r.kind==VisitorReportKind::Failed;});
    assert(refusal!=s.players[0].pendingVisitorReports.end() && refusal->deliveryTurn==kAwaitingTransmission);
    assert(!refusal->observation.available && refusal->observation.observedTurn==61);
    assert(known_visitor(s,1,1)->available && known_visitor(s,1,1)->observedTurn==60);
    assert(s.fleets[0].minerals.germanium==50 && s.players[0].acquiredEquipment.empty());
    while(s.turn<63) s=TurnProcessor{}.process(s,{});
    assert(distance_between(s.fleets[0].position,s.visitors[0].position)>fleet_sensor_range(s,s.fleets[0]));
    const auto sent=std::find_if(s.players[0].pendingVisitorReports.begin(),s.players[0].pendingVisitorReports.end(),
        [](const auto& r){return r.kind==VisitorReportKind::Failed;});
    assert(sent!=s.players[0].pendingVisitorReports.end() && sent->deliveryTurn>s.turn && sent->deliveryTurn!=kAwaitingTransmission);
    const auto due=sent->deliveryTurn;
    while(s.turn+1<due) s=TurnProcessor{}.process(s,{});
    assert(known_visitor(s,1,1)->available && known_visitor(s,1,1)->observedTurn==60);
    s=TurnProcessor{}.process(s,{});
    assert(!known_visitor(s,1,1)->available && known_visitor(s,1,1)->observedTurn==61 && known_visitor(s,1,1)->receivedTurn==due);
    assert(!submit_visitor_command(s,1,{s.fleets[0].id,1,8}));
    assert(s.fleets[0].minerals.germanium==50 && s.players[0].acquiredEquipment.empty());

    // A failed prediction carries its old track, not a new world measurement.
    auto navigation=remote(); navigation.fleets[0].electronics={EmissionMode::RadioSilence,62};
    auto& known=navigation.players[0].visitorKnowledge[0]; known.position={1040,0}; known.departureTurn=100;
    navigation.visitors[0].position={9999,9999}; navigation.visitors[0].customer=1;
    apply_visitor_command(navigation,navigation.fleets[0],{known,8});
    navigation=TurnProcessor{}.process(navigation,{});
    const auto failure=std::find_if(navigation.players[0].pendingVisitorReports.begin(),navigation.players[0].pendingVisitorReports.end(),
        [](const auto& r){return r.kind==VisitorReportKind::Failed;});
    assert(failure!=navigation.players[0].pendingVisitorReports.end() && failure->observedTurn==61 && failure->observation.observedTurn==60);
    assert(failure->deliveryTurn==kAwaitingTransmission);
    navigation=TurnProcessor{}.process(navigation,{});
    const auto sentFailure=std::find_if(navigation.players[0].pendingVisitorReports.begin(),navigation.players[0].pendingVisitorReports.end(),
        [](const auto& r){return r.kind==VisitorReportKind::Failed;});
    assert(sentFailure!=navigation.players[0].pendingVisitorReports.end());
    const auto failureDue=sentFailure->deliveryTurn; assert(failureDue>navigation.turn && failureDue!=kAwaitingTransmission);
    while(navigation.turn<failureDue) navigation=TurnProcessor{}.process(navigation,{});
    const auto* unchanged=known_visitor(navigation,1,1);
    assert(unchanged->available && unchanged->observedTurn==60 && unchanged->receivedTurn==60 && unchanged->position==Position(1040,0));
}
void manual_control_and_computer_exchange()
{
    auto manual=make_visitor_demo(); const auto id=manual.fleets[0].id;
    assert(submit_visitor_command(manual,1,{id,1,8}));
    assert(submit_fleet_route_command(manual,1,id,{100,100},8,{},{}));
    assert(!manual.fleets[0].visitor.track.id);
    manual=TurnProcessor{}.process(manual,{}); assert(manual.players[0].acquiredEquipment.empty() && manual.fleets[0].minerals.germanium==50);
    auto ai=make_visitor_demo(); ai.players[0].controller=PlayerController::Computer; ai.fleets[0].minerals={};
    publish_fleet_telemetry(ai,ai.turn); deliver_due_fleet_telemetry(ai);
    const auto before=ai.planets[0].minerals.germanium;
    const auto plan=plan_computer_turn(make_player_view(ai,1));
    assert(std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){return std::holds_alternative<EncounterVisitorOrder>(o);}));
    assert(std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){return std::holds_alternative<SetFleetMineralCargoOrder>(o);}));
    ai=TurnProcessor{}.process(ai,{plan.orders});
    assert(ai.visitors[0].customer==1 && ai.fleets[0].minerals.germanium==0);
    assert(ai.planets[0].minerals.germanium<before && ai.players[0].acquiredEquipment.size()==1);
}
void simultaneous_customers_and_spawn_are_deterministic()
{
    auto initial=make_visitor_demo(); auto second=initial.fleets[0]; second.id=initial.nextFleetId++;
    initial.fleets.push_back(second); auto reordered=initial; std::reverse(reordered.fleets.begin(),reordered.fleets.end());
    const PlayerOrders orders{1,{EncounterVisitorOrder{1,1,8},EncounterVisitorOrder{second.id,1,8}}};
    auto a=TurnProcessor{}.process(initial,{orders}), b=TurnProcessor{}.process(reordered,{orders});
    for(const auto& f:a.fleets) { const auto g=std::find_if(b.fleets.begin(),b.fleets.end(),[&](const auto& g){return g.id==f.id;}); assert(f.minerals.germanium==g->minerals.germanium && f.position==g->position); }
    assert(a.fleets[0].minerals.germanium==0 && a.fleets[1].minerals.germanium==50);
    auto generated=generate_game({771,32,400,400,15}); auto replay=generated;
    for(std::uint64_t year=60;year<500;++year) { generated.turn=replay.turn=year; advance_visitors(generated); advance_visitors(replay); }
    assert(!generated.visitors.empty() && generated.visitors.size()==replay.visitors.size());
    for(std::size_t n=0;n<generated.visitors.size();++n) assert(generated.visitors[n].position==replay.visitors[n].position && generated.visitors[n].departureTurn==replay.visitors[n].departureTurn);
}
}
int main()
{
    demo_exchange_and_real_payment(); gift_and_sightings_travel_as_immutable_packets();
    physical_refusal_delivers_evidence_but_navigation_failure_keeps_its_age();
    delayed_commands_and_hidden_truth_do_not_change_navigation(); silent_receipts_and_old_measurements(); manual_control_and_computer_exchange(); simultaneous_customers_and_spawn_are_deterministic();
    std::cout<<"Visitor physical encounters, payment, delayed knowledge/control and determinism passed\n";
}
