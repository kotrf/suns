#include "orbital_base_fixture.hpp"
#include "suns/mobile_base.hpp"
#include "suns/stations.hpp"
#include "suns/computer_player.hpp"
#include "suns/combat.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace orbital_test;
namespace {
void assembly_and_build() {
    auto s=world(); assert(ship_design_valid(*find_ship_design(s,200))); deploy(s);
    assert(base(s).outpost==OutpostKind::OrbitalShipyard && s.planets[1].owner==0 && !s.planets[1].population);
    assert(!submit_fleet_route_command(s,1,1,{80,0},5,{},{}));
    const auto before=base(s).minerals;
    tick(s); assert(base(s).orbitalBase.assemblyYears==1);
    assert(base(s).minerals.ironium==before.ironium-21);
    auto pause=OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive,0,0,0,false}};
    tick(s,{pause}); assert(base(s).orbitalBase.assemblyYears==1);
    tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive}}});
    for(int i=0;i<4;++i)tick(s);
    assert(base(s).orbitalBase.assemblyYears==6 && base(s).colonists==25000);
    assert(s.planets[1].owner==0 && s.planets[1].environment.temperature==100);
    const auto paid=base(s).minerals;
    tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::QueueShip,0,201}}});
    assert(s.fleets.size()==2 && s.fleets.back().design==201 && s.fleets.back().fuel==0);
    assert(base(s).orbitalBase.queue.empty());
    assert(std::abs(base(s).minerals.ironium-(paid.ironium-1-ship_design_mineral_cost(*find_ship_design(s,201)).ironium))<1e-6);
    const auto after=base(s).minerals; const auto count=s.fleets.size();
    service_orbital_bases(s,s.turn); assert(s.fleets.size()==count && base(s).minerals.ironium==after.ironium);
    tick(s); assert(s.fleets.size()==count);
    // Repeated deployment cannot replenish emergency reserves or assembly work.
    base(s).orbitalBase.reserveYears=0; deploy(s); assert(base(s).orbitalBase.reserveYears==0 && base(s).orbitalBase.assemblyYears==6);
    assert(!queue_station_construction(s,1,{2,{}}));
}
void shortages_cancellation_and_refit() {
    auto s=world(); deploy(s); auto& f=base(s); f.orbitalBase.assemblyYears=6; f.minerals={2,2,2};
    apply_orbital_base_command(s,f,{OrbitalBaseAction::QueueShip,0,201});
    tick(s); assert(base(s).orbitalBase.queue.size()==1 && base(s).orbitalBase.queue[0].work<ship_design_cost(*find_ship_design(s,201)));
    const auto work=base(s).orbitalBase.queue[0].work; tick(s); assert(base(s).orbitalBase.queue[0].work==work);
    tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::Cancel,0,0,1}}}); assert(base(s).orbitalBase.queue.empty() && s.fleets.size()==1);
    base(s).minerals={200,120,200}; // Fixture resupply; physical order delivery tested separately.
    auto changed=*find_ship_design(s,200); changed.id=203; changed.name="Refitted orbital yard";
    for(auto& c:changed.components)if(c==C::AdvancedRadiationScreen)c=C::CompositeRadiationScreen;
    s.shipDesigns.push_back(changed); s.nextShipDesignId=204;
    apply_orbital_base_command(s,base(s),{OrbitalBaseAction::Refit,0,203});
    tick(s); const auto invested=base(s).orbitalBase.queue[0].work;
    const auto old=base(s).design; tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive,0,0,0,false}}});
    assert(base(s).design==old && base(s).orbitalBase.queue[0].work==invested);
    tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive}}});
    for(int i=0;i<100 && base(s).design==old;++i) {
        auto& m=base(s).minerals; m.ironium=std::max(m.ironium,150.0);m.boranium=std::max(m.boranium,150.0);m.germanium=std::max(m.germanium,150.0);tick(s);
    }
    assert(base(s).design==203 && orbital_screen(s,base(s))==40 && base(s).outpost==OutpostKind::OrbitalShipyard);
}
void radiation_and_life_support() {
    auto safe=world(); deploy(safe); tick(safe); assert(base(safe).colonists==25000 && base(safe).orbitalBase.exposure==30);
    auto exposed=world(40,C::BulkRadiationScreen); deploy(exposed);
    assert(orbital_radiation_losses(exposed,base(exposed))==1500); tick(exposed); assert(base(exposed).colonists==23500);
    auto physical=world(); deploy(physical); physical.planets[1].environment.radiation=0;
    physical.planets[1].environment.temperature=0; physical.planets[1].environment.gravity=0;
    tick(physical); assert(base(physical).colonists==base(safe).colonists && base(physical).orbitalBase.exposure==30);
    auto immune=world(40,C::BulkRadiationScreen); immune.players[0].race.radiationImmune=true; deploy(immune); tick(immune); assert(base(immune).colonists==25000);
    auto starved=world(); deploy(starved); base(starved).minerals={};
    for(int i=0;i<3;++i)tick(starved);
    assert(base(starved).colonists==25000 && !base(starved).orbitalBase.reserveYears);
    tick(starved); assert(base(starved).colonists==22500 && !base(starved).orbitalBase.supported);
    base(starved).minerals={100,100,100}; tick(starved); assert(base(starved).colonists==22500 && base(starved).orbitalBase.supported);
}
void delivered_transfers_and_commands() {
    auto s=world(5000); assert(communication_delay_turns(s,1,base(s).position)>0);
    const auto delay=communication_delay_turns(s,1,base(s).position);
    tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::Deploy,2}}});
    assert(base(s).outpost==OutpostKind::None && !base(s).pendingCommands.empty());
    while(s.turn<=1+delay)tick(s);
    assert(base(s).outpost==OutpostKind::OrbitalShipyard);
    // Local onboard operation continues without another command or HQ receipt.
    auto& f=base(s); const auto received=f.telemetry.orbitalBase.assemblyYears;
    tick(s); assert(base(s).orbitalBase.assemblyYears>received);
    const auto years=base(s).orbitalBase.assemblyYears;
    tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive,0,0,0,false}}});
    assert(base(s).orbitalBase.active && base(s).orbitalBase.assemblyYears>=years);
    for(std::uint64_t i=0;i<delay;++i)tick(s);
    assert(!base(s).orbitalBase.active);
    auto local=world(); deploy(local); base(local).minerals={}; base(local).colonists=0;
    Fleet t; t.id=2;t.owner=1;t.design=202;t.ships={{202,1}};t.position=base(local).position;t.colonists=20000;t.minerals={100,100,100};t.fuel=500;
    local.fleets.push_back(t);local.nextFleetId=3;acknowledge(local);
    tick(local,{TransferCargoOrder{{0,2},{0,1},20000,{100,100,100}},TransferFleetFuelOrder{2,1,100,200}});
    assert(base(local).colonists==20000 && local.fleets[1].colonists==0 && base(local).orbitalBase.assemblyYears==1);
    const auto total=base(local).colonists+local.fleets[1].colonists;
    tick(local,{TransferCargoOrder{{0,1},{0,2},20000,{}}});
    assert(!base(local).colonists && local.fleets[1].colonists==total);
    // Overfull exact transfers are atomic, including the mineral payload.
    base(local).minerals={}; local.fleets[1].minerals={1000,0,0};
    tick(local,{TransferCargoOrder{{0,2},{0,1},20000,{1000,0,0}}});
    assert(!base(local).colonists && !base(local).minerals.ironium && local.fleets[1].colonists==20000);
}
void combat_and_surface_founding() {
    auto s=world();deploy(s);auto enemy=s.players[0];enemy.id=2;enemy.surveyKnowledge.clear();s.players.push_back(enemy);
    auto colony=*find_ship_design(s,kColonyShipDesignId);colony.id=250;colony.owner=2;colony.name="Enemy colony transport";s.shipDesigns.push_back(colony);s.nextShipDesignId=251;
    Fleet invader;invader.id=2;invader.owner=2;invader.design=250;invader.ships={{250,1}};invader.colonists=100;invader.position=base(s).position;invader.fuel=100;
    s.fleets.push_back(invader);s.nextFleetId=3;acknowledge(s);
    auto blocked=TurnProcessor{}.process(s,{PlayerOrders{2,{ColonizePlanetOrder{2,2}}}});
    assert(blocked.planets[1].owner==0 && blocked.fleets[1].colonists==100);
    // Founding a friendly surface colony preserves the existing orbital asset.
    auto friendly=s;friendly.fleets[1].owner=1;friendly.fleets[1].design=kColonyShipDesignId;friendly.fleets[1].ships={{kColonyShipDesignId,1}};
    friendly.players[0].race.environmentBased=false;friendly.planets[1].habitability=80;
    auto founded=TurnProcessor{}.process(friendly,{PlayerOrders{1,{ColonizePlanetOrder{2,2}}}});
    assert(founded.planets[1].owner==1 && orbital_base_at(founded,2) && base(founded).colonists==25000);
    // Physical destruction removes stocks, workers and construction together.
    auto war=world();deploy(war);war.players.push_back(enemy);
    war.shipDesigns.push_back({250,2,"Attackers",ShipHullType::Destroyer,{C::QuickJump5,C::Laser,C::Laser}});
    auto attackers=invader;attackers.colonists=0;attackers.ships={{250,100}};attackers.position=base(war).position;
    war.fleets.push_back(attackers);war.nextFleetId=3;
    resolve_space_battles(war,war.turn);
    assert(!orbital_base_at(war,2));
    tick(war);assert(!orbital_base_at(war,2));
}
void ai_received_orders() {
    auto loading=world();loading.planets[0].minerals={1000,1000,1000};
    std::erase_if(loading.players[0].surveyKnowledge,[](const auto& k){return k.star==1;});
    set_survey_level(loading,1,1,SurveyLevel::DeepSurvey,loading.turn);
    base(loading).position={0,0};base(loading).colonists=0;base(loading).minerals={};acknowledge(loading);
    ComputerState memory;PlayerOrders supply{1,{}};plan_orbital_bases(make_player_view(loading,1),supply,memory);
    assert(std::any_of(supply.orders.begin(),supply.orders.end(),[](const auto& o){return std::holds_alternative<TransferCargoOrder>(o);}));
    tick(loading,supply.orders);
    assert(base(loading).colonists==25000 && base(loading).minerals.ironium==220);

    auto s=world();deploy(s);base(s).orbitalBase.assemblyYears=6;base(s).orbitalBase.supported=true;acknowledge(s);
    PlayerOrders o{1,{}};plan_orbital_bases(make_player_view(s,1),o,memory);
    assert(std::any_of(o.orders.begin(),o.orders.end(),[](const auto& order){const auto* b=std::get_if<OrbitalBaseOrder>(&order);return b && b->command.action==OrbitalBaseAction::QueueShip;}));
    // A received idle constructor at its surveyed target can deploy honestly.
    auto c=world();PlayerOrders deployment{1,{}};plan_orbital_bases(make_player_view(c,1),deployment,memory);
    assert(std::any_of(deployment.orders.begin(),deployment.orders.end(),[](const auto& order){const auto* b=std::get_if<OrbitalBaseOrder>(&order);return b && b->command.action==OrbitalBaseAction::Deploy;}));
}
void delayed_cancel_keeps_other_jobs() {
    auto overtaken=world(5000);deploy(overtaken);base(overtaken).orbitalBase.assemblyYears=6;
    assert(submit_orbital_base_command(overtaken,1,1,{OrbitalBaseAction::QueueShip,0,201,100}));
    const auto due=base(overtaken).pendingCommands.front().deliveryTurn;
    apply_orbital_base_command(overtaken,base(overtaken),{OrbitalBaseAction::Cancel,0,0,100});
    while(overtaken.turn<=due)tick(overtaken);
    assert(base(overtaken).orbitalBase.queue.empty() && overtaken.fleets.size()==1);

    auto s=world(5000);deploy(s);base(s).orbitalBase.assemblyYears=6;
    apply_orbital_base_command(s,base(s),{OrbitalBaseAction::QueueShip,0,201,101});
    apply_orbital_base_command(s,base(s),{OrbitalBaseAction::QueueShip,0,201,102});
    assert(submit_orbital_base_command(s,1,1,{OrbitalBaseAction::Cancel,0,0,101}));
    // Only the first launch happens before the delayed cancellation arrives.
    tick(s);assert(base(s).orbitalBase.queue.size()==1 && base(s).orbitalBase.queue[0].id==102);
    base(s).orbitalBase.active=false;
    // Replayed accepted intent cannot create another job after its first launch.
    apply_orbital_base_command(s,base(s),{OrbitalBaseAction::QueueShip,0,201,101});
    assert(base(s).orbitalBase.queue.size()==1);
    const auto delivery=base(s).pendingCommands.front().deliveryTurn;
    while(s.turn<delivery)tick(s);
    assert(base(s).orbitalBase.queue.size()==1 && base(s).orbitalBase.queue[0].id==102);
}
void ai_waits_for_orbital_acknowledgement() {
    auto s=world(5000);deploy(s);base(s).orbitalBase.assemblyYears=6;base(s).orbitalBase.supported=true;
    s.players[0].controller=PlayerController::Computer;acknowledge(s);
    assert(communication_delay_turns(s,1,base(s).position)==34);
    std::size_t sent=0;std::uint64_t request=0;
    for(int year=0;year<39;++year)for(const auto& order:orbital_ai_year(s)) {
        ++sent;request=order.command.item;
    }
    assert(sent==1 && request);
    assert(base(s).orbitalBase.acceptedItems==std::vector<std::uint64_t>{request});
    assert(base(s).orbitalBase.queue.empty() && s.fleets.size()==2);
    auto newer=s;base(newer).telemetry.observedTurn=s.turn;
    auto noReceipt=plan_computer_turn(make_player_view(newer,1));
    assert(noReceipt.nextState.orbitalBuilds==s.players[0].computer.orbitalBuilds);
    for(const auto& o:noReceipt.orders.orders)if(const auto* b=std::get_if<OrbitalBaseOrder>(&o))assert(b->command.action!=OrbitalBaseAction::QueueShip);
    auto accepted=newer;auto& known=base(accepted).telemetry.orbitalBase;
    known.lastItemId=request;known.acceptedItems={request};
    known.queue={{s.players[0].computer.orbitalBuilds[0].design,0,false,request}};
    const auto working=plan_computer_turn(make_player_view(accepted,1));
    assert(working.nextState.orbitalBuilds==s.players[0].computer.orbitalBuilds);
    for(const auto& o:working.orders.orders)if(const auto* b=std::get_if<OrbitalBaseOrder>(&o))assert(b->command.action!=OrbitalBaseAction::QueueShip);
    acknowledge(s);
    const auto next=plan_computer_turn(make_player_view(s,1));std::size_t fresh=0;
    for(const auto& o:next.orders.orders)if(const auto* b=std::get_if<OrbitalBaseOrder>(&o);b && b->command.action==OrbitalBaseAction::QueueShip) {
        ++fresh;assert(b->command.item && b->command.item!=request);
    }
    assert(fresh==1);
    assert(next.nextState.orbitalBuilds==plan_computer_turn(make_player_view(s,1)).nextState.orbitalBuilds);
}
void orbit_control_and_knowledge() {
    auto s=world(); deploy(s); const auto old=base(s).orbitalBase.planet;
    Fleet duplicate=base(s); duplicate.id=2;duplicate.outpost=OutpostKind::None;duplicate.orbitalBase={};
    s.fleets.push_back(duplicate);s.nextFleetId=3;
    apply_orbital_base_command(s,s.fleets[1],{OrbitalBaseAction::Deploy,2}); assert(s.fleets[1].outpost==OutpostKind::None);
    assert(base(s).orbitalBase.planet==old);
    const auto count=s.fleets.size();tick(s,{MergeFleetsOrder{2,1},SplitFleetOrder{1,{{200,1}}}});assert(s.fleets.size()==count);
    s.planets[1].owner=1;s.planets[1].population=1000;
    assert(!queue_station_construction(s,1,{2,{}}));
    auto remote=world(5000);deploy(remote);acknowledge(remote);
    auto known=make_player_view(remote,1).state;
    base(remote).orbitalBase.assemblyYears=6;base(remote).minerals={1,1,1};base(remote).colonists=1;
    auto altered=make_player_view(remote,1).state;
    assert(base(altered).colonists==base(known).colonists);
    assert(base(altered).orbitalBase.assemblyYears==0);
    ComputerState memory;PlayerOrders ai{1,{}};plan_orbital_bases(make_player_view(remote,1),ai,memory);
    for(const auto& o:ai.orders)assert(!std::holds_alternative<OrbitalBaseOrder>(o));
}
}
int main(){assembly_and_build();shortages_cancellation_and_refit();radiation_and_life_support();delivered_transfers_and_commands();orbit_control_and_knowledge();combat_and_surface_founding();ai_received_orders();delayed_cancel_keeps_other_jobs();ai_waits_for_orbital_acknowledgement();std::cout<<"Orbital shipyard tests passed\n";}
