#include "suns/deep_space.hpp"
#include "suns/campaign.hpp"
#include "suns/combat.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/turn_processor.hpp"
#include "suns/wormholes.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace suns;
using C=ShipComponentType;
namespace {
bool near(double a,double b) { return std::abs(a-b)<1e-6; }
GameState world()
{
    auto s=make_demo_game(); s.stars.resize(1); s.planets.resize(1); s.fleets.clear();
    s.stars[0].position={0,0}; s.players[0].technology.levels.fill(26);
    s.players[0].technology.researchAllocationPercent=0;
    s.planets[0].productionQueue.clear(); s.planets[0].mines=0;
    s.wormholes.clear(); s.wormholeRules.spawnChancePerTurn=0;
    s.shipDesigns.push_back({100,1,"Depot constructor",ShipHullType::Scout,{C::FusionDrive,C::FuelDepotKit,C::LongRangeScanner}});
    s.shipDesigns.push_back({101,1,"Relay constructor",ShipHullType::Scout,{C::FusionDrive,C::DeepSpaceRelayKit,C::LongRangeScanner}});
    s.nextShipDesignId=102; s.nextFleetId=1;
    return s;
}
Fleet& add(GameState& s,ShipDesignId design=100,Position position={60,0})
{
    Fleet f; f.id=s.nextFleetId++; f.owner=1; f.name="Builder"; f.design=design; f.ships={{design,1}};
    f.position=position; f.fuel=100; s.fleets.push_back(f); return s.fleets.back();
}
void snapshot(GameState& s) { publish_fleet_telemetry(s,s.turn); deliver_due_fleet_telemetry(s); }
void deployment_is_physical_paid_and_permanent()
{
    auto s=world(); auto& builder=add(s); const auto id=builder.id;
    const auto cost=ship_design_cost(*find_ship_design(s,100));
    assert(cost>80 && component_mineral_cost(C::FuelDepotKit).ironium==40);
    auto design=*find_ship_design(s,100); design.components.push_back(C::DeepSpaceRelayKit);
    assert(!ship_design_valid(design));
    s.players[0].technology.levels.fill(0); assert(!component_available_to_player(s,1,C::FuelDepotKit));
    s.players[0].technology.levels[std::size_t(ResearchField::Construction)]=4;
    s.players[0].technology.levels[std::size_t(ResearchField::Energy)]=3;
    assert(component_available_to_player(s,1,C::FuelDepotKit));
    assert(!component_available_to_player(s,1,C::DeepSpaceRelayKit));
    s.players[0].technology.levels.fill(26); snapshot(s);
    MoveFleetOrder move{id,{65,0},3,{FleetArrivalActionKind::DeployOutpost}};
    const auto next=TurnProcessor{}.process_with_events(s,{{1,{move}}});
    assert(next.state.fleets.size()==1); const auto& f=next.state.fleets.front();
    assert(f.id==id && f.outpost==OutpostKind::FuelDepot && f.name==builder.name);
    assert(same_position(f.position,move.destination) && f.fuel<builder.fuel);
    assert(outpost_state_valid(next.state,f) && fleet_max_warp(next.state,f)==0);
    assert(!fleet_can_colonize(next.state,f) && !fleet_can_remote_mine(next.state,f));
    assert(std::any_of(next.events.begin(),next.events.end(),[](const auto& e){return e.kind==GameEventKind::OutpostDeployed;}));
    auto anchored=next.state;
    assert(!submit_fleet_route_command(anchored,1,id,{80,0},2,{},{}));
    auto after=TurnProcessor{}.process(anchored,{{1,{SplitFleetOrder{id,{{100,1}}},SetFleetTaskOrder{id,FleetTask::LayMines}}}});
    assert(after.fleets.size()==1 && after.fleets[0].outpost==OutpostKind::FuelDepot);
    assert(same_position(after.fleets[0].position,f.position) && after.fleets[0].task==FleetTask::None);
    assert(!enter_wormhole(after,after.fleets[0],123));
    // Paid production consumes the entire constructor's ordinary ship cost.
    auto paid=world(); paid.planets[0].industry=1000; paid.planets[0].minerals={1000,1000,1000};
    const auto minerals=ship_design_mineral_cost(*find_ship_design(paid,100));
    const auto baseline=TurnProcessor{}.process(paid,{});
    const auto built=TurnProcessor{}.process(paid,{{1,{QueueShipDesignOrder{1,100}}}});
    assert(built.fleets.size()==1 && built.fleets[0].outpost==OutpostKind::None);
    assert(near(built.planets[0].minerals.ironium,baseline.planets[0].minerals.ironium-minerals.ironium));
}
void validation_and_arbitration_do_not_depend_on_storage_order()
{
    auto s=world(); auto& first=add(s);
    assert(!outpost_deployment_error(s,first,{4.99,0}).empty());
    assert(outpost_deployment_error(s,first,{5,0}).empty());
    first.colonists=1; assert(!outpost_deployment_error(s,first,first.position).empty()); first.colonists=0;
    first.ships[0].count=2; assert(fleet_construction_kit(s,first)==OutpostKind::None); first.ships[0].count=1;
    first.damagePercent=100; assert(!outpost_deployment_error(s,first,first.position).empty()); first.damagePercent=0;
    add(s,101); for(auto& f:s.fleets) f.task=FleetTask::DeployOutpost;
    auto reversed=s; std::reverse(reversed.fleets.begin(),reversed.fleets.end());
    deploy_outposts(s,s.turn+1); deploy_outposts(reversed,reversed.turn+1);
    for(const auto& f:s.fleets) {
        const auto peer=std::find_if(reversed.fleets.begin(),reversed.fleets.end(),[&](const auto& x){return x.id==f.id;});
        assert(peer!=reversed.fleets.end() && peer->outpost==f.outpost);
        assert((f.id==1)==(f.outpost!=OutpostKind::None));
    }
    FleetRouteProgram route; route.destination={70,0}; route.warp=3;
    route.arrivalAction.kind=FleetArrivalActionKind::DeployOutpost; route.queuedWaypoints.push_back({{80,0},3});
    assert(!fleet_route_tasks_valid(route)); route.queuedWaypoints.clear(); route.repeatOrders=true;
    assert(!fleet_route_tasks_valid(route));
}
void fuel_is_finite_owned_and_conserved()
{
    auto s=world(); add(s).task=FleetTask::DeployOutpost; deploy_outposts(s,s.turn);
    s.fleets[0].fuel=25; add(s,kScoutDesignId); auto& visitor=s.fleets[1]; visitor.fuel=0;
    assert(near(refuel_from_outpost(s,visitor),25)); assert(near(visitor.fuel,25) && near(s.fleets[0].fuel,0));
    assert(!outpost_online(s.fleets[0]) && refuel_from_outpost(s,visitor)==0);
    visitor.fuel=200; assert(near(supply_outpost(s,visitor,75),125));
    assert(near(visitor.fuel,75) && near(s.fleets[0].fuel,125));
    visitor.owner=2; assert(refuel_from_outpost(s,visitor)==0 && supply_outpost(s,visitor,0)==0);
    visitor.owner=1; visitor.position.x+=0.01; assert(supply_outpost(s,visitor,0)==0);
    visitor.position=s.fleets[0].position; assert(supply_outpost(s,visitor,std::numeric_limits<double>::quiet_NaN())==0);
    s.fleets[0].fuel=fleet_fuel_capacity(s,s.fleets[0])-5;
    assert(near(supply_outpost(s,visitor,0),5)); assert(near(visitor.fuel,70));
    s.fleets[0].damagePercent=100; assert(supply_outpost(s,visitor,0)==0);
}
void route_stops_replenish_and_draw_from_the_reservoir()
{
    auto s=world(); add(s).task=FleetTask::DeployOutpost; deploy_outposts(s,s.turn);
    s.fleets[0].fuel=25; add(s,kScoutDesignId); s.fleets[1].fuel=100; snapshot(s);
    FleetArrivalAction supply{FleetArrivalActionKind::SupplyOutpost}; supply.reserveFuel=40;
    const auto supplied=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{2,{60,0},2,supply}}}});
    assert(near(supplied.fleets[0].fuel,84) && near(supplied.fleets[1].fuel,40)); // one upkeep unit first
    auto refueled=TurnProcessor{}.process(supplied,{{1,{MoveFleetOrder{2,{60,0},2,{FleetArrivalActionKind::Refuel}}}}});
    assert(near(refueled.fleets[0].fuel,0) && near(refueled.fleets[1].fuel,123));
    // An empty site's services resume after a physical supply, including relays.
    auto relay=world(); add(relay,101).task=FleetTask::DeployOutpost; deploy_outposts(relay,relay.turn);
    relay.fleets[0].fuel=0; add(relay,kScoutDesignId); relay.fleets[1].fuel=20;
    assert(near(supply_outpost(relay,relay.fleets[1],5),15) && outpost_online(relay.fleets[0]));
}
void relay_upkeep_and_loss_cut_the_live_mesh()
{
    auto s=world(); auto& builder=add(s,101,{400,0});
    assert(communication_delay_turns(s,1,{650,0})>0);
    assert(fleet_communication_range(s,builder)==50); // ordinary scanner only while mobile
    builder.task=FleetTask::DeployOutpost; deploy_outposts(s,s.turn);
    assert(communication_delay_turns(s,1,{650,0})==0 && fleet_communication_range(s,s.fleets[0])==300);
    s.fleets[0].fuel=1; maintain_outposts(s,s.turn+1);
    assert(s.fleets[0].outpost==OutpostKind::CommunicationsRelay && s.fleets[0].fuel==0);
    assert(communication_delay_turns(s,1,{650,0})>0 && fleet_sensor_range(s,s.fleets[0])==0);
    const auto count=s.players[0].pendingPlayerReports.size(); maintain_outposts(s,s.turn+2);
    assert(s.players[0].pendingPlayerReports.size()==count); // no repeated exhaustion spam
    s.fleets[0].fuel=10; assert(communication_delay_turns(s,1,{650,0})==0);
    s.fleets[0].damagePercent=100; assert(communication_delay_turns(s,1,{650,0})>0);
    s.fleets[0].damagePercent=0; s.fleets.clear(); assert(communication_delay_turns(s,1,{650,0})>0);
}
void deployment_and_stock_wait_for_received_telemetry()
{
    auto s=world(); add(s,100,{1000,0}); auto& f=s.fleets[0];
    f.telemetry.observedTurn=s.turn; f.telemetry.position=f.position; f.telemetry.fuel=100; f.telemetry.ships=f.ships;
    f.task=FleetTask::DeployOutpost; deploy_outposts(s,s.turn); snapshot(s);
    assert(f.outpost==OutpostKind::FuelDepot && !f.telemetryInTransit.empty());
    assert(known_outposts(s,1).empty() && fleet_player_view(s,f).outpost==OutpostKind::None);
    const auto first=make_player_view(s,1); f.fuel=12; f.damagePercent=80;
    const auto second=make_player_view(s,1);
    assert(first.state.fleets[0].fuel==second.state.fleets[0].fuel);
    assert(first.state.fleets[0].outpost==second.state.fleets[0].outpost);
    s.turn=f.telemetryInTransit.front().deliveryTurn; deliver_due_fleet_telemetry(s);
    const auto sites=known_outposts(s,1); assert(sites.size()==1 && sites[0].fuel==100 && sites[0].damagePercent==0);
    assert(same_position(sites[0].position,Position{1000,0}));
}
void destruction_is_an_ordinary_delayed_battle_loss()
{
    auto s=world(); s.players.push_back({2,"Enemy"});
    add(s,101,{1000,0}).task=FleetTask::DeployOutpost; deploy_outposts(s,s.turn);
    auto& relay=s.fleets[0]; relay.telemetry.observedTurn=s.turn; relay.telemetry.position=relay.position;
    relay.telemetry.fuel=relay.fuel; relay.telemetry.ships=relay.ships; relay.telemetry.outpost=relay.outpost;
    s.shipDesigns.push_back({200,2,"Assault",ShipHullType::Destroyer,{C::QuickJump5,C::AntiMatterPulverizer}});
    auto& enemy=add(s,200,{1000,0}); enemy.owner=2; enemy.ships[0].count=30;
    resolve_space_battles(s,s.turn);
    assert(std::none_of(s.fleets.begin(),s.fleets.end(),[](const auto& f){return f.id==1;}));
    assert(!s.pendingFleetLossContacts.empty() && known_outposts(s,1).size()==1);
    const auto report=std::find_if(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),
        [](const auto& r){return r.kind==PlayerReportKind::SpaceBattle;});
    assert(report!=s.players[0].pendingPlayerReports.end() && report->battle && report->battle->units[0].survivingShips==0);
    s.turn=report->deliveryTurn; const auto events=deliver_due_player_reports(s);
    assert(std::any_of(events.begin(),events.end(),[](const auto& e){return e.kind==GameEventKind::SpaceBattle;}));
    assert(known_outposts(s,1).empty());
}
}
int main()
{
    deployment_is_physical_paid_and_permanent(); validation_and_arbitration_do_not_depend_on_storage_order();
    fuel_is_finite_owned_and_conserved(); route_stops_replenish_and_draw_from_the_reservoir();
    relay_upkeep_and_loss_cut_the_live_mesh(); deployment_and_stock_wait_for_received_telemetry();
    destruction_is_an_ordinary_delayed_battle_loss();
    std::cout << "deep-space infrastructure tests passed\n";
}
