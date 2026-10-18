#include "suns/mobile_base.hpp"
#include "suns/turn_processor.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace suns;
using C=ShipComponentType;
namespace {
void initialize_initial_fleet_telemetry(Fleet& f,std::uint64_t turn)
{
    f.telemetry.observedTurn=turn; f.telemetry.position=f.position; f.telemetry.ships=f.ships;
    f.telemetry.fuel=f.fuel; f.telemetry.damagePercent=f.damagePercent; f.telemetry.minerals=f.minerals;
    f.telemetry.mobileBase=f.mobileBase; f.telemetry.lastMovedTurn=f.lastMovedTurn;
}
bool near(double a,double b) { return std::abs(a-b)<1e-6; }
GameState world(Position pos={20,0})
{
    auto s=make_demo_game(); s.fleets.clear(); s.stars.resize(1); s.stars[0].position={0,0}; s.planets.resize(1);
    s.planets[0].productionQueue.clear(); s.planets[0].mines=0; s.players[0].technology.levels.fill(26);
    s.players[0].technology.researchAllocationPercent=0; s.wormholes.clear(); s.wormholeRules.spawnChancePerTurn=0;
    s.shipDesigns.push_back({200,1,"Expedition",ShipHullType::ExpeditionBase,
        {C::QuickJump5,C::QuickJump5,C::QuickJump5,C::QuickJump5,C::ExpeditionDock,C::ExpeditionWorkshop,C::RelayArray}});
    s.shipDesigns.push_back({201,1,"Client",ShipHullType::LightTransport,{C::FusionDrive,C::CargoPod}});
    Fleet base; base.id=1; base.owner=1; base.name="Base"; base.design=200; base.ships={{200,1}}; base.position=pos;
    base.fuel=5000; base.minerals={100,100,100}; base.mobileBase.clients={2,3};
    Fleet client; client.id=2; client.owner=1; client.name="Client"; client.design=201; client.ships={{201,10}};
    client.position=pos; client.damagePercent=50; client.fuel=0;
    s.fleets={base,client}; client.id=3; s.fleets.push_back(client); s.nextFleetId=4; s.nextShipDesignId=202;
    for(auto& f:s.fleets) initialize_initial_fleet_telemetry(f,s.turn);
    return s;
}
void design_and_resources()
{
    auto s=world(); const auto d=*find_ship_design(s,200); assert(ship_design_valid(d));
    assert(ship_design_mass(d)>1500 && ship_design_cost(d)>1400);
    auto ai=s; ai.players[0].controller=PlayerController::Computer;
    const auto plan=plan_computer_turn(make_player_view(ai,1));
    for(const auto& order:plan.orders.orders) if(const auto* move=std::get_if<MoveFleetOrder>(&order)) assert(move->fleet!=1);
    auto wrong=d; wrong.hull=ShipHullType::Utility; assert(!ship_design_valid(wrong));
    s.players[0].technology.levels.fill(0); assert(!ship_hull_available_to_player(s,1,ShipHullType::ExpeditionBase));
    assert(!component_available_to_player(s,1,C::ExpeditionWorkshop)); s.players[0].technology.levels.fill(26);
    assert(component_available_to_player(s,1,C::ExpeditionWorkshop));
    auto& base=s.fleets[0]; base.fuel=200; base.mobileBase.fuelReserve=150; base.minerals={10,5,5};
    auto& a=s.fleets[1]; auto& b=s.fleets[2]; const auto mass=fleet_dry_mass(s,a);
    a.damagePercent=60.0/mass*100; b.damagePercent=60.0/mass*100;
    service_mobile_bases(s,2);
    assert(near(a.damagePercent,0) && near(b.damagePercent,20.0/mass*100));
    assert(near(base.minerals.ironium,0) && near(base.minerals.boranium,0) && near(base.minerals.germanium,0));
    assert(near(base.fuel,150) && near(a.fuel,50) && near(b.fuel,0));
    base.minerals={100,100,100}; service_mobile_bases(s,2); assert(near(b.damagePercent,20.0/mass*100));
    service_mobile_bases(s,3); assert(near(b.damagePercent,0));
    auto fractional=world(); fractional.fleets[0].minerals={0.3,0.15,0.15}; service_mobile_bases(fractional,2);
    assert(fractional.fleets[0].minerals.ironium>=0 && fractional.fleets[0].minerals.boranium>=0 && fractional.fleets[0].minerals.germanium>=0);
    for(int limiting=0;limiting<3;++limiting) {
        auto t=world(); auto& minerals=t.fleets[0].minerals;
        if(limiting==0) minerals.ironium=0; if(limiting==1) minerals.boranium=0; if(limiting==2) minerals.germanium=0;
        service_mobile_bases(t,2); assert(near(t.fleets[1].damagePercent,50));
    }
}
void priorities_and_dry_mass()
{
    auto s=world(); s.fleets[0].mobileBase.clients={999,3,2}; s.fleets[2].position={21,0};
    auto loaded=s; loaded.fleets[1].minerals={100,50,50}; loaded.fleets[1].fuel=500;
    service_mobile_bases(s,2); service_mobile_bases(loaded,2);
    assert(near(s.fleets[1].damagePercent,loaded.fleets[1].damagePercent));
    assert(s.fleets[1].damagePercent<50 && near(s.fleets[2].damagePercent,50));
    auto absent=world(); absent.fleets[0].mobileBase.clients={3,2}; absent.fleets[2].damagePercent=0;
    service_mobile_bases(absent,2); assert(absent.fleets[1].damagePercent<50);
    auto foreign=world(); foreign.fleets[1].owner=2; foreign.fleets[0].mobileBase.clients={2}; service_mobile_bases(foreign,2);
    assert(near(foreign.fleets[1].damagePercent,50) && near(foreign.fleets[1].fuel,0));
    auto gone=world(); gone.fleets.erase(gone.fleets.begin()); service_mobile_bases(gone,2); assert(near(gone.fleets[0].damagePercent,50));
}
void movement_and_arrival()
{
    auto s=world(); s.fleets[0].mobileBase.clients={2}; s.fleets[2].position={40,0};
    s.fleets[1].position={24,0}; initialize_initial_fleet_telemetry(s.fleets[1],s.turn);
    MoveFleetOrder move{1,{24,0},3};
    auto after=TurnProcessor{}.process(s,{{1,{move}}});
    assert(same_position(after.fleets[0].position,Position{24,0}));
    assert(after.fleets[0].lastMovedTurn==after.turn && near(after.fleets[1].damagePercent,50));
    assert(fleet_communication_range(after,after.fleets[0])>0); // Mobile radio remains available.
    auto resumed=TurnProcessor{}.process(after,{}); assert(resumed.fleets[1].damagePercent<50);
    auto returned=world(); returned.fleets[0].lastMovedTurn=2;
    service_mobile_bases(returned,2); assert(near(returned.fleets[1].damagePercent,50));
    assert(!fleet_warp_valid(returned,returned.fleets[0],6));
    auto arriving=world(); arriving.fleets[1].position={24,0}; arriving.fleets[1].fuel=500;
    initialize_initial_fleet_telemetry(arriving.fleets[1],arriving.turn);
    auto arrival=TurnProcessor{}.process(arriving,{{1,{MoveFleetOrder{2,{20,0},3}}}});
    assert(same_position(arrival.fleets[1].position,Position{20,0}) && arrival.fleets[1].damagePercent<50);
}
void fuel_and_cargo_delivery()
{
    auto s=world(); s.fleets[0].mobileBase.clients.clear(); s.fleets[0].fuel=0; s.fleets[1].fuel=300;
    assert(transfer_mobile_base_fuel(s,1,{2,1,500,80},2)); assert(near(s.fleets[1].fuel,80) && near(s.fleets[0].fuel,220));
    s.fleets[0].lastMovedTurn=2; assert(!transfer_mobile_base_fuel(s,1,{2,1,10,0},2));
    assert(!transfer_mobile_base_fuel(s,2,{2,1,10,0},3));
    s.fleets[1].minerals={10,0,0}; s.fleets[0].lastMovedTurn=0;
    auto sent=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,2},{0,1},0,{5,0,0}}}}});
    assert(near(sent.fleets[1].minerals.ironium,5) && near(sent.fleets[0].minerals.ironium,105));
    auto moved=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,2},{0,1},0,{5,0,0}},MoveFleetOrder{1,{24,0},3}}}});
    assert(near(moved.fleets[1].minerals.ironium,10));
}
void remote_transfer_commands()
{
    auto s=world({900,0}); s.fleets[0].mobileBase.clients.clear(); s.fleets[0].fuel=0;
    s.fleets[1].fuel=300; s.fleets[1].minerals={10,0,0};
    const auto delay=communication_delay_turns(s,1,s.fleets[0].position); assert(delay>0 && delay<100);
    auto pending=TurnProcessor{}.process(s,{{1,{TransferFleetFuelOrder{2,1,200,80},TransferCargoOrder{{0,2},{0,1},0,{5,0,0}}}}});
    assert(pending.pendingMobileBaseTransfers.size()==2 && near(pending.fleets[0].fuel,0));
    assert(near(pending.fleets[1].minerals.ironium,10)); // No reservations in transit.
    const auto initialView=make_player_view(pending,1); assert(initialView.state.pendingMobileBaseTransfers.empty());
    auto unavailable=pending; unavailable.fleets[1].position={901,0};
    while(pending.turn<=s.turn+delay) {
        pending=TurnProcessor{}.process(pending,{}); unavailable=TurnProcessor{}.process(unavailable,{});
    }
    assert(pending.pendingMobileBaseTransfers.empty() && near(pending.fleets[0].fuel,200) && near(pending.fleets[1].fuel,100));
    assert(near(pending.fleets[0].minerals.ironium,105) && near(pending.fleets[1].minerals.ironium,5));
    assert(near(unavailable.fleets[0].fuel,0) && near(unavailable.fleets[1].minerals.ironium,10));
}
void generation_and_service_blocks()
{
    auto s=world(); auto* d=const_cast<ShipDesign*>(find_ship_design(s,200)); d->components.push_back(C::AntimatterGenerator);
    assert(ship_design_valid(*d));
    const auto generated=std::min(fleet_fuel_generation(s,s.fleets[0]),fleet_fuel_capacity(s,s.fleets[0])-s.fleets[0].fuel);
    double before=0; for(const auto& f:s.fleets) before+=f.fuel;
    const auto after=TurnProcessor{}.process(s,{}); double fuel=0; for(const auto& f:after.fleets) fuel+=f.fuel;
    assert(generated>0 && near(fuel-before,generated));
    for(int reason=0;reason<3;++reason) {
        auto blocked=world(); auto& base=blocked.fleets[0];
        if(reason==0) base.mobileBase.enabled=false;
        if(reason==1) base.damagePercent=100;
        if(reason==2) base.destination=Position{40,0};
        service_mobile_bases(blocked,2); assert(near(blocked.fleets[1].damagePercent,50) && near(blocked.fleets[1].fuel,0));
    }
    auto partial=world(); partial.fleets[0].ships={{200,2}}; partial.fleets[0].lastMovedTurn=2;
    const auto detached=TurnProcessor{}.process(partial,{{1,{SplitFleetOrder{1,{{200,1}}},SetMobileBaseOrder{4,partial.fleets[0].mobileBase}}}});
    assert(near(detached.fleets[1].damagePercent,50) && detached.fleets.back().lastMovedTurn==2);
}
void reorganization_and_commands()
{
    auto s=world(); s.fleets[0].ships={{200,2}}; s.fleets[0].mobileBase.clients={2};
    auto split=TurnProcessor{}.process(s,{{1,{SplitFleetOrder{1,{{200,1}}}}}});
    assert(split.fleets.size()==4); assert(split.fleets.back().mobileBase.clients.empty());
    const auto repaired=(50-split.fleets[1].damagePercent)/100*fleet_dry_mass(s,s.fleets[1]); assert(near(repaired,100));
    auto duplicate=TurnProcessor{}.process(s,{{1,{SetMobileBaseOrder{1,s.fleets[0].mobileBase},SetMobileBaseOrder{1,s.fleets[0].mobileBase}}}});
    assert(near((50-duplicate.fleets[1].damagePercent)/100*fleet_dry_mass(s,s.fleets[1]),200));
    auto invalid=s.fleets[0].mobileBase; invalid.clients={2,2}; assert(!mobile_base_program_valid(invalid));
    invalid.clients={}; invalid.fuelReserve=std::numeric_limits<double>::quiet_NaN(); assert(!mobile_base_program_valid(invalid));
    // Different-size merges conserve damaged dry mass.
    auto mixed=world(); mixed.fleets[0].mobileBase.enabled=false; mixed.fleets[2].ships={{200,1}}; mixed.fleets[2].design=200;
    mixed.fleets[2].damagePercent=0; mixed.fleets[2].mobileBase.enabled=false;
    const auto damaged=fleet_dry_mass(mixed,mixed.fleets[1])*.5;
    auto merged=TurnProcessor{}.process(mixed,{{1,{MergeFleetsOrder{2,3}}}});
    assert(near(fleet_dry_mass(merged,*find_service_fleet(merged,2))*find_service_fleet(merged,2)->damagePercent/100,damaged));
    auto remote=world({900,0}); auto p=remote.fleets[0].mobileBase; p.fuelReserve=777;
    assert(submit_mobile_base_command(remote,1,1,p)); assert(remote.fleets[0].mobileBase.fuelReserve!=777);
    assert(remote.fleets[0].pendingCommands.back().mobileBase==p);
    remote.turn=remote.fleets[0].pendingCommands.back().deliveryTurn; deliver_due_fleet_commands(remote); assert(remote.fleets[0].mobileBase==p);
    // Hidden physical policy and movement never replace delivered evidence.
    remote.fleets[0].telemetry.mobileBase.fuelReserve=123; remote.fleets[0].lastMovedTurn=999;
    const auto view=fleet_player_view(remote,remote.fleets[0]); assert(view.mobileBase.fuelReserve==123 && view.lastMovedTurn!=999);
}
}
int main() { design_and_resources(); priorities_and_dry_mass(); movement_and_arrival(); fuel_and_cargo_delivery(); remote_transfer_commands(); generation_and_service_blocks(); reorganization_and_commands(); std::cout<<"mobile base tests passed\n"; }
