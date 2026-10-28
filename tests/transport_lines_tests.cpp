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
int main(){autonomous_and_conserved();delayed_update_stop_and_manual();draft_order_precedence();pause_resume_same_dispatch();received_validation();std::cout<<"Transport line core tests passed\n";}
