#include "suns/transport_lines.hpp"
#include "transport_lines_fixture.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace orbital_test;
double ironium(const GameState& s) {
    double total=0;for(const auto& p:s.planets)total+=p.minerals.ironium;
    for(const auto& f:s.fleets)total+=f.minerals.ironium;return total;
}
void autonomous_and_conserved() {
    auto s=transport_world();auto baseline=s;
    assert(set_transport_line(s,1,{line_spec()}));
    assert(assign_transport_line(s,1,{1,1}) && assign_transport_line(s,1,{1,2}));
    assert(!assign_transport_line(s,1,{1,1}));assert(s.players[0].transportLines[0].assignments.size()==2);
    auto noHQ=s;noHQ.players[0].transportLines.clear();
    for(int i=0;i<20;++i) {
        tick(s);tick(noHQ);tick(baseline);
        assert(std::abs(ironium(s)-ironium(baseline))<1e-6);
        for(std::size_t j=0;j<2;++j) {
            assert(same_position(s.fleets[j].position,noHQ.fleets[j].position));
            assert(s.fleets[j].fuel==noHQ.fleets[j].fuel && s.fleets[j].minerals.ironium==noHQ.fleets[j].minerals.ironium);
            assert(fleet_cargo_used(s,s.fleets[j])<=fleet_cargo_capacity(s,s.fleets[j])+1e-6);
        }
    }
    assert(s.planets[0].minerals.ironium>baseline.planets[0].minerals.ironium+900);
    assert(s.planets[1].minerals.ironium==0);
    assert(s.fleets[0].repeatOrders && s.fleets[1].repeatOrders);
}
void delayed_update_stop_and_manual() {
    auto s=transport_world(900);s.fleets[0].position={900,0};acknowledge(s);
    assert(set_transport_line(s,1,{line_spec()}));assert(assign_transport_line(s,1,{1,1}));
    const auto due=s.fleets[0].pendingCommands.front().deliveryTurn;
    assert(due>s.turn && !s.fleets[0].destination);
    assert(!set_transport_line(s,1,{line_spec()})); // Exact revision replay does not dispatch twice.
    assert(s.fleets[0].pendingCommands.size()==1);
    while(s.turn+1<due){tick(s);assert(!s.fleets[0].destination);}
    tick(s);assert(s.turn==due && s.fleets[0].repeatOrders);
    auto pause=s.players[0].transportLines[0].spec;++pause.revision;pause.active=false;
    assert(set_transport_line(s,1,{pause}));assert(s.fleets[0].repeatOrders);
    const auto stop=s.fleets[0].pendingCommands.back().deliveryTurn;
    while(s.turn<=stop)tick(s);
    assert(!s.fleets[0].destination && !s.fleets[0].repeatOrders);
    ++pause.revision;pause.active=true;assert(set_transport_line(s,1,{pause}));
    const auto resume=s.fleets[0].pendingCommands.back().deliveryTurn;
    while(s.turn<=resume)tick(s);assert(s.fleets[0].repeatOrders);
    tick(s,{MoveFleetOrder{1,{1000,0},5}});
    assert(s.players[0].transportLines[0].assignments.empty());
    auto local=transport_world();assert(set_transport_line(local,1,{line_spec()}));assert(assign_transport_line(local,1,{1,1}));
    auto deletion=line_spec();++deletion.revision;assert(set_transport_line(local,1,{deletion,true}));
    assert(local.players[0].transportLines.empty() && !local.fleets[0].repeatOrders);
    assert(!set_transport_line(local,1,{line_spec()})); // Deleted IDs cannot be resurrected.
}
void pause_resume_same_dispatch() {
    auto s=transport_world(900);s.fleets[0].position={900,0};acknowledge(s);
    auto d=line_spec();assert(set_transport_line(s,1,{d}));assert(assign_transport_line(s,1,{1,1}));
    ++d.revision;d.active=false;assert(set_transport_line(s,1,{d}));
    ++d.revision;d.active=true;assert(set_transport_line(s,1,{d}));
    const auto due=s.fleets[0].pendingCommands.back().deliveryTurn;
    assert(s.fleets[0].pendingCommands.size()==3);
    while(s.turn<=due)tick(s);
    assert(s.fleets[0].repeatOrders);
}
void reserve_and_local_wait() {
    auto s=transport_world(900);s.fleets.resize(1);s.fleets[0].position={900,0};s.planets[1].minerals.ironium=130;acknowledge(s);
    auto d=line_spec();d.sourceReserve=100;d.departureMinimum=80;
    assert(set_transport_line(s,1,{d}) && assign_transport_line(s,1,{1,1}));
    const auto due=s.fleets[0].pendingCommands.front().deliveryTurn;
    while(s.turn<=due)tick(s);
    assert(s.fleets[0].minerals.ironium==30 && s.planets[1].minerals.ironium==100);
    assert(s.fleets[0].arrivalAction && s.fleets[0].arrivalAction->kind==FleetArrivalActionKind::LoadMinerals);
    const auto fuel=s.fleets[0].fuel;
    for(int i=0;i<3;++i){tick(s);assert(s.fleets[0].minerals.ironium==30 && s.planets[1].minerals.ironium==100);}
    assert(s.fleets[0].fuel==fuel && same_position(s.fleets[0].position,{900,0}));
    s.planets[1].minerals.ironium+=50; // A physical delivery; no command or HQ update.
    tick(s);assert(s.fleets[0].minerals.ironium==80 && s.planets[1].minerals.ironium==100);
    assert(s.fleets[0].destination && same_position(*s.fleets[0].destination,{0,0}));
    assert(!s.fleets[0].arrivalAction || s.fleets[0].arrivalAction->kind!=FleetArrivalActionKind::LoadMinerals);
}
void hold_limit_capture_and_cancel() {
    auto s=transport_world();s.fleets.resize(1);s.fleets[0].position={40,0};s.fleets[0].minerals.boranium=fleet_cargo_capacity(s,s.fleets[0])-50;acknowledge(s);
    auto d=line_spec();d.sourceReserve=100;d.departureMinimum=80;
    assert(set_transport_line(s,1,{d}) && assign_transport_line(s,1,{1,1}));tick(s);
    assert(s.fleets[0].minerals.ironium==50 && s.fleets[0].minerals.boranium==fleet_cargo_capacity(s,s.fleets[0])-50);
    assert(s.fleets[0].arrivalAction && s.fleets[0].arrivalAction->kind==FleetArrivalActionKind::LoadMinerals);
    assert(fleet_cargo_used(s,s.fleets[0])==fleet_cargo_capacity(s,s.fleets[0]));
    auto paused=d;++paused.revision;paused.active=false;assert(set_transport_line(s,1,{paused}));
    assert(s.fleets[0].minerals.ironium==50);tick(s);assert(!s.fleets[0].destination);
    ++paused.revision;paused.active=true;assert(set_transport_line(s,1,{paused}));
    s.planets[1].owner=2;const auto stock=s.planets[1].minerals.ironium;
    tick(s);assert(s.planets[1].minerals.ironium==stock && s.fleets[0].minerals.ironium==50);
    assert(s.fleets[0].arrivalAction); // Captured source cannot supply the old owner's waiting fleet.
    auto invalid=transport_line_program(s.players[0].transportLines[0]);invalid.arrivalAction.cargo=FleetCargoKind::Colonists;
    assert(!fleet_route_tasks_valid(invalid));
}
void reserve_only_and_competition() {
    auto s=transport_world();for(auto& f:s.fleets)f.position={40,0};s.planets[1].minerals.ironium=250;acknowledge(s);
    auto d=line_spec();d.sourceReserve=100;
    assert(set_transport_line(s,1,{d}) && assign_transport_line(s,1,{1,1}) && assign_transport_line(s,1,{1,2}));
    tick(s);
    assert(s.planets[1].minerals.ironium==100);
    assert(s.fleets[0].minerals.ironium+s.fleets[1].minerals.ironium==150);
    for(const auto& f:s.fleets)assert(f.destination && same_position(*f.destination,{0,0}));
}
void draft_order_precedence() {
    auto s=transport_world();PlayerOrders orders{1,{MoveFleetOrder{1,{10,0},5},SetTransportLineOrder{line_spec()},AssignTransportLineOrder{1,1}}};
    const auto line=pending_transport_route(s,orders,1);assert(line && line->repeatOrders);
    orders.orders.push_back(MoveFleetOrder{1,{20,0},5});
    const auto manual=pending_transport_route(s,orders,1);assert(manual && !manual->repeatOrders && manual->destination.x==20);
    auto result=TurnProcessor{}.process(s,{orders});assert(result.players[0].transportLines[0].assignments.empty());
    assert(!result.fleets[0].repeatOrders && result.fleets[0].position.x==20);
    auto second=line_spec();second.id=2;second.name="Second";
    assert(set_transport_line(s,1,{line_spec()}) && set_transport_line(s,1,{second}));
    assert(assign_transport_line(s,1,{1,1}) && assign_transport_line(s,1,{2,1}));
    assert(s.players[0].transportLines[0].assignments.empty() && s.players[0].transportLines[1].assignments.size()==1);
}
void received_validation() {
    auto s=transport_world(900);const auto view=make_player_view(s,1);
    s.planets[0].owner=2;s.planets[1].owner=2; // Hidden ownership changes are not validation evidence.
    assert(set_transport_line(s,1,{line_spec()}));
    assert(s.players[0].transportLines.size()==1);
    auto wrong=line_spec();wrong.id=2;wrong.source=9999;assert(!set_transport_line(s,1,{wrong}));
    wrong=line_spec();wrong.id=2;wrong.warp=11;assert(!set_transport_line(s,1,{wrong}));
    auto copy=s;copy.players.push_back({2,"Other"});assert(!assign_transport_line(copy,2,{1,1}));
}

std::vector<Order> line_orders(const ComputerTurnPlan& plan) {
    std::vector<Order> result;for(const auto& o:plan.orders.orders)
        if(std::holds_alternative<SetTransportLineOrder>(o) || std::holds_alternative<AssignTransportLineOrder>(o))result.push_back(o);
    return result;
}
void computer_supplies_and_conserves() {
    auto s=computer_transport_world();auto baseline=s;
    const auto plan=plan_computer_turn(make_player_view(s,1));const auto orders=line_orders(plan);
    assert(orders.size()==2);
    const auto& spec=std::get<SetTransportLineOrder>(orders[0]).value;
    assert(spec.source==2 && spec.destination==1 && spec.sourceReserve==200 && spec.departureMinimum==50);
    const auto fleet=std::get<AssignTransportLineOrder>(orders[1]).fleet;
    assert(fleet==1 && spec.cargo==TransportCargo::Ironium);
    s=resolve_controlled_campaign_turn(s,{}).state;tick(baseline);
    assert(s.players[0].transportLines.size()==1);
    for(int i=0;i<10;++i) {
        assert(line_orders(plan_computer_turn(make_player_view(s,1))).empty());
        s=resolve_controlled_campaign_turn(s,{}).state;tick(baseline);
        assert(s.players[0].transportLines.size()==1 && s.players[0].transportLines[0].assignments.size()==1);
        assert(s.fleets[0].minerals.ironium>=0 && fleet_cargo_used(s,s.fleets[0])<=fleet_cargo_capacity(s,s.fleets[0]));
    }
    // Normal production may consume delivered minerals; conserved comparison
    // uses only the line orders to isolate freight from the AI economy.
    auto freight=computer_transport_world(),control=freight;
    tick(freight,orders);tick(control);
    assert(freight.fleets[0].fuel<fleet_fuel_capacity(freight,freight.fleets[0]));
    for(int i=0;i<12;++i){tick(freight);tick(control);assert(std::abs(ironium(freight)-ironium(control))<1e-6);}
    assert(freight.planets[0].minerals.ironium>=800 && freight.planets[1].minerals.ironium==200);
}
void computer_delayed_intent_and_reservation() {
    auto s=computer_transport_world(5100);s.fleets[0].position={5100,0};
    for(auto& d:s.shipDesigns)if(d.id==202)d.components={C::FuelMizer,C::FuelMizer};acknowledge(s);
    const auto plan=plan_computer_turn(make_player_view(s,1));const auto orders=line_orders(plan);
    assert(orders.size()==2);tick(s,orders);s.players[0].computer=plan.nextState;
    const auto due=s.fleets[0].pendingCommands.front().deliveryTurn;assert(due>s.turn+20);
    int assignments=1;
    while(s.turn<due) {
        const auto next=plan_computer_turn(make_player_view(s,1));const auto extra=line_orders(next);
        assignments+=std::count_if(extra.begin(),extra.end(),[](const auto& o){return std::holds_alternative<AssignTransportLineOrder>(o);});
        assert(extra.empty());
        assert(std::none_of(next.orders.orders.begin(),next.orders.orders.end(),[](const auto& o){
            const auto manual=transport_manual_override(o);return manual && *manual==1;
        }));
        assert(std::none_of(next.nextState.fleetGoals.begin(),next.nextState.fleetGoals.end(),[](auto g){return g.fleet==1;}));
        tick(s);s.players[0].computer=next.nextState;
    }
    assert(assignments==1 && s.fleets[0].repeatOrders && s.players[0].transportLines.size()==1);
    auto view=make_player_view(s,1);
    // Received enemy ownership engages military planning, but a freight roster
    // cannot silently become a gathered/merged/colonist-loaded assault fleet.
    view.state.planets[0].owner=2;
    for(auto& k:view.state.players[0].surveyKnowledge)if(k.star==1)k.observedOwner=2;
    const auto war=plan_computer_turn(view);
    for(const auto& o:war.orders.orders)std::visit([](const auto& order){
        using T=std::decay_t<decltype(order)>;
        if constexpr(std::is_same_v<T,MergeFleetsOrder>)assert(order.source!=1 && order.destination!=1);
        else if constexpr(requires {order.fleet;})assert(order.fleet!=1);
    },o);
}
void computer_received_only_and_guards() {
    auto s=computer_transport_world();const auto view=make_player_view(s,1);
    const auto check=[&](const PlayerView& changed){assert(line_orders(plan_computer_turn(changed)).empty());};
    auto changed=view;changed.state.orbitalStations.clear();check(changed);
    changed=view;for(auto& p:changed.state.planets)p.minerals={};check(changed);
    changed=view;for(auto& f:changed.state.fleets){f.fuel=0;f.telemetry.fuel=0;f.position={2000,0};f.telemetry.position=f.position;}
    const auto slow=line_orders(plan_computer_turn(changed));assert(slow.size()==2);
    assert(std::get<SetTransportLineOrder>(slow[0]).value.warp==1); // Quick Jump's real fuel-free speed, no invented fuel.
    changed=view;for(auto& f:changed.state.fleets)f.damagePercent=1;check(changed);
    changed=view;for(auto& f:changed.state.fleets){f.colonists=1;f.telemetry.colonists=1;}check(changed);
    changed=view;for(auto& f:changed.state.fleets){f.design=201;f.ships={{201,1}};}check(changed); // Scouts are not requisitioned.
    auto received=plan_computer_turn(view);const auto expected=line_orders(received);assert(expected.size()==2);
    auto shuffled=view;std::reverse(shuffled.state.planets.begin(),shuffled.state.planets.end());std::reverse(shuffled.state.fleets.begin(),shuffled.state.fleets.end());
    const auto replay=line_orders(plan_computer_turn(shuffled));assert(replay.size()==2);
    assert(std::get<AssignTransportLineOrder>(replay[1]).fleet==std::get<AssignTransportLineOrder>(expected[1]).fleet);
    assert(std::get<SetTransportLineOrder>(replay[0]).value.source==std::get<SetTransportLineOrder>(expected[0]).value.source);
    // Hidden supplies, capture, loss of a depot and fuel/cargo cannot change
    // the received-only plan or its decision journal.
    s.planets[1].owner=2;s.planets[1].minerals={};s.orbitalStations.clear();
    for(auto& f:s.fleets){f.fuel=0;f.minerals={500,0,0};f.position={9999,0};}
    const auto hidden=plan_computer_turn(make_player_view(s,1));
    assert(received.nextState==hidden.nextState);
    const auto actual=line_orders(hidden);assert(actual.size()==expected.size());
    const auto& a=std::get<SetTransportLineOrder>(actual[0]).value;const auto& b=std::get<SetTransportLineOrder>(expected[0]).value;
    assert(a.name==b.name && a.source==b.source && a.destination==b.destination && a.warp==b.warp && a.cargo==b.cargo);
    // A paused roster remains intent and does not provoke a replacement route.
    tick(s,expected);auto paused=s.players[0].transportLines.front().spec;++paused.revision;paused.active=false;
    assert(set_transport_line(s,1,{paused}));check(make_player_view(s,1));
}
void attributed_delivery_and_reorganization() {
    for(const int merge:{0,1,2}) {
        auto s=transport_world();s.fleets.resize(1);s.fleets[0].position={40,0};s.fleets[0].ships={{202,2}};acknowledge(s);
        assert(set_transport_line(s,1,{line_spec()}) && assign_transport_line(s,1,{1,1}));tick(s);
        assert(s.fleets[0].minerals.ironium==1000);
        // Normal reorganization requires an idle fleet; Stop preserves cargo.
        tick(s,{MoveFleetOrder{1,s.fleets[0].position,5,{}, {},false,0,true},SplitFleetOrder{1,{{202,1}}}});
        assert(s.fleets.size()==2);const auto child=s.fleets.back().id;
        assert(s.players[0].transportReceipts.empty() && !s.fleets.back().repeatOrders);
        if(merge==1)tick(s,{MergeFleetsOrder{1,child}});
        if(merge==2)tick(s,{MoveFleetOrder{child,s.fleets[0].position,5,{FleetArrivalActionKind::MergeWithFleet},{},false,1}});
        assert(assign_transport_line(s,1,{1,1}));if(!merge)assert(assign_transport_line(s,1,{1,child}));
        for(int i=0;i<4;++i)tick(s);
        const auto view=make_player_view(s,1);const auto m=transport_line_metrics(view,s.players[0].transportLines[0]);
        assert(m.delivered.ironium==1000 && m.deliveries==(merge?1:2));
        assert(m.nominalRoundTripYears==4);
        for(const auto& r:s.players[0].transportReceipts)assert(r.line==1 && r.revision==1 && r.planet==1);
        // An ordinary manual delivery is not attributed by fleet roster/name.
        s.fleets[0].minerals.ironium=17;s.fleets[0].position={0,0};
        tick(s,{TransferCargoOrder{{0,1},{1,0},0,{17,0,0}}});
        assert(transport_line_metrics(make_player_view(s,1),s.players[0].transportLines[0]).delivered.ironium==1000);
    }
    for(int blocked:{0,1,2}) {
        auto s=computer_transport_world();s.players[0].controller=PlayerController::Human;
        auto d=line_spec();d.source=1;d.destination=2;assert(set_transport_line(s,1,{d}));
        auto unload=transport_line_program(s.players[0].transportLines[0]).queuedWaypoints[0].arrivalAction;
        if(blocked==0)s.planets[1].owner=0;
        if(blocked==1)s.planets[1].owner=2;
        if(blocked==2)unload.transportDestination=1;
        s.fleets[0].position={40,0};s.fleets[0].destination=s.fleets[0].position;s.fleets[0].arrivalAction=unload;s.fleets[0].minerals.ironium=17;
        tick(s);assert(s.players[0].transportReceipts.empty());
        if(blocked==1)assert(s.fleets[0].minerals.ironium==17); // Enemy stock cannot receive a credited delivery.
    }
}
void delayed_receipt_and_revision() {
    auto s=computer_transport_world(5100);s.players[0].controller=PlayerController::Human;s.fleets.resize(1);
    auto d=line_spec();d.source=1;d.destination=2;
    assert(set_transport_line(s,1,{d}));
    const auto unload=transport_line_program(s.players[0].transportLines[0]).queuedWaypoints[0].arrivalAction;
    s.fleets[0].position={5100,0};s.fleets[0].destination=s.fleets[0].position;s.fleets[0].arrivalAction=unload;
    s.fleets[0].minerals.ironium=70;acknowledge(s);auto control=s;control.fleets[0].minerals={};tick(control);tick(s);
    assert(std::abs(s.planets[1].minerals.ironium-control.planets[1].minerals.ironium-70)<1e-6 && s.players[0].transportReceipts.empty());
    const auto packet=*std::find_if(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::TransportLineDelivery;});
    assert(packet.deliveryTurn>s.turn+20 && packet.transportRevision==1);
    ++d.revision;d.name="Renamed after delivery";d.cargo=TransportCargo::Boranium;assert(set_transport_line(s,1,{d}));
    while(s.turn<packet.deliveryTurn){assert(s.players[0].transportReceipts.empty());tick(s);}
    auto m=transport_line_metrics(make_player_view(s,1),s.players[0].transportLines[0]);
    assert(m.deliveries==1 && m.delivered.ironium==70 && m.delivered.boranium==0);
    assert(s.players[0].transportReceipts[0].revision==1 && s.players[0].transportReceipts[0].observedTurn==2);
    auto duplicate=packet;duplicate.deliveryTurn=s.turn;s.players[0].pendingPlayerReports.push_back(duplicate);
    (void)deliver_due_player_reports(s);assert(s.players[0].transportReceipts.size()==1);
    s.turn+=kTransportStatisticsYears;
    s.players[0].pendingPlayerReports.push_back(duplicate);(void)deliver_due_player_reports(s);
    assert(s.players[0].transportReceipts.size()==1); // Replays never refresh the original receipt date.
    m=transport_line_metrics(make_player_view(s,1),s.players[0].transportLines[0]);assert(m.deliveries==0);
}
void bounded_receipts_and_received_status() {
    Player p;p.id=1;p.lastTransportLineId=1;
    PendingPlayerReport r;r.kind=PlayerReportKind::TransportLineDelivery;r.transportLine=1;r.transportRevision=1;
    r.observedTurn=2;r.deliveryTurn=2;r.planet=1;r.deliveredMinerals={1,0,0};
    for(std::size_t i=0;i<=kMaximumTransportReceipts;++i){r.fleet=FleetId(i+1);receive_transport_receipt(p,r,3);}
    assert(p.transportReceipts.empty() && p.transportReceiptsTruncatedThrough==2);
    receive_transport_receipt(p,r,4);assert(p.transportReceipts.empty());
    r.observedTurn=3;r.deliveryTurn=3;receive_transport_receipt(p,r,4);assert(p.transportReceipts.size()==1);
    auto s=transport_world();s.fleets[0].position={40,0};acknowledge(s);auto d=line_spec();d.departureMinimum=1300;
    assert(set_transport_line(s,1,{d}) && assign_transport_line(s,1,{1,1}));tick(s);
    auto view=make_player_view(s,1);const auto& line=view.state.players[0].transportLines[0];
    assert(transport_assignment_status(view,line,line.assignments[0]).find("cannot fit")!=std::string::npos);
    const auto before=transport_line_metrics(view,line);assert(before.contacts==1 && before.waiting==1);
    // A hypothetical projected dot is not a new snapshot and cannot alter status or utilization.
    view.state.fleets[0].position={0,0};view.state.fleets[0].destination=Position{0,0};
    assert(transport_assignment_status(view,line,line.assignments[0]).find("cannot fit")!=std::string::npos);
    assert(transport_line_metrics(view,line).waiting==before.waiting);
    auto invalid=transport_line_program(line);invalid.queuedWaypoints[0].arrivalAction.transportLine=0;
    assert(!fleet_route_tasks_valid(invalid));
}
int main(){attributed_delivery_and_reorganization();delayed_receipt_and_revision();bounded_receipts_and_received_status();computer_supplies_and_conserves();computer_delayed_intent_and_reservation();computer_received_only_and_guards();reserve_and_local_wait();hold_limit_capture_and_cancel();reserve_only_and_competition();autonomous_and_conserved();delayed_update_stop_and_manual();draft_order_precedence();pause_resume_same_dispatch();received_validation();std::cout<<"Transport line core tests passed\n";}
