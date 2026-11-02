#include "suns/fleet_recycling.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/turn_processor.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>

using namespace suns;
namespace {
GameState fixture()
{
    auto s = make_demo_game();
    s.wormholeRules.spawnChancePerTurn = 0;
    s.players[0].technology.researchActive = false;
    s.shipDesigns = {{10,1,"Hauler",ShipHullType::LargeFreighter,
        {ShipComponentType::FuelMizer,ShipComponentType::RhinoScanner}},
        {11,1,"Scout",ShipHullType::Scout,{ShipComponentType::FuelMizer,ShipComponentType::RhinoScanner}}};
    s.nextShipDesignId = 12;
    auto& fleet = s.fleets[0]; fleet.design = 10; fleet.ships = {{10,2},{11,3}};
    fleet.fuel = 1000; fleet.minerals = {7,11,13};
    fleet.telemetry = {}; fleet.telemetry = confirmed_fleet_telemetry(s, fleet);
    auto& p = s.planets[0]; p.mines = p.industry = 0; p.productionQueue.clear(); p.minerals = {};
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    return s;
}
double total(MineralCargo c) { return c.ironium + c.boranium + c.germanium; }
bool near(double a, double b) { return std::abs(a-b) < 1e-7; }
MineralCargo buildCost(const GameState& s)
{
    MineralCargo result;
    for (const auto& stack : fleet_ship_stacks(s.fleets[0])) {
        const auto cost = ship_design_mineral_cost(*find_ship_design(s,stack.design));
        result.ironium += cost.ironium * stack.count; result.boranium += cost.boranium * stack.count;
        result.germanium += cost.germanium * stack.count;
    }
    return result;
}
void onboard(GameState& s)
{
    auto& f=s.fleets[0]; f.destination=f.position; f.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::ScrapFleet};
}
void refundsAndRandomness()
{
    int losses = 0;
    for (unsigned seed = 0; seed < 1000; ++seed) {
        auto s=fixture(); s.galaxySeed=seed;
        const auto cost=buildCost(s), cargo=s.fleets[0].minerals;
        const auto source=s;
        assert(scrap_fleet(s,s.fleets[0],2));
        const auto recovered=s.players[0].pendingPlayerReports.back().deliveredMinerals;
        const auto rate=(recovered.ironium-cargo.ironium)/cost.ironium;
        assert(rate >= 0.60-1e-9 && rate <= 0.75+1e-9);
        if (!near(rate,0.75)) { ++losses; assert(rate<=0.70+1e-9); }
        assert(near(recovered.boranium,cargo.boranium+cost.boranium*rate));
        assert(near(recovered.germanium,cargo.germanium+cost.germanium*rate));
        auto replay=source; assert(scrap_fleet(replay,replay.fleets[0],2));
        assert(near(total(replay.planets[0].minerals),total(s.planets[0].minerals)));
        auto space=source; space.fleets[0].position={1000,0};
        assert(scrap_fleet(space,space.fleets[0],2));
        assert(space.wrecks.size()==1);
        const auto material=space.wrecks[0].minerals;
        assert(near((material.ironium-cargo.ironium)/cost.ironium,rate-0.25));
        assert(std::all_of(space.wrecks[0].technology.begin(),space.wrecks[0].technology.end(),[](auto v){return v==0;}));
        assert(space.players[0].strategicIntel.empty()); // New debris is not globally known yet.
    }
    assert(losses > 140 && losses < 260); // Detect missing/always-on or badly biased loss rolls.
}
void actualOrdersAndDelay()
{
    auto s=fixture();
    const auto result=TurnProcessor{}.process_with_events(s,{{1,{MoveFleetOrder{1,{0,0},5,{FleetArrivalActionKind::ScrapFleet}}}}});
    assert(result.state.fleets.empty() && result.state.wrecks.empty());
    auto immobilized=s;immobilized.fleets[0].damagePercent=100;onboard(immobilized);
    assert(TurnProcessor{}.process(immobilized,{}).fleets.empty());
    assert(std::any_of(result.events.begin(),result.events.end(),[](const auto& e){return e.kind==GameEventKind::FleetScrapStatus && e.quantity==1;}));
    const auto repeated=TurnProcessor{}.process(result.state,{{1,{MoveFleetOrder{1,{0,0},5,{FleetArrivalActionKind::ScrapFleet}}}}});
    const auto withoutRepeat=TurnProcessor{}.process(result.state,{});
    assert(near(total(repeated.planets[0].minerals),total(withoutRepeat.planets[0].minerals)));
    FleetRouteProgram invalid{{0,0},5,{FleetArrivalActionKind::ScrapFleet},{{{10,0},5,{}}}};
    assert(!fleet_route_tasks_valid(invalid));
    invalid.arrivalAction={}; invalid.queuedWaypoints[0].arrivalAction={FleetArrivalActionKind::ScrapFleet}; invalid.repeatOrders=true;
    assert(!fleet_route_tasks_valid(invalid));

    auto remote=fixture(); remote.fleets[0].position={1000,0};
    remote.fleets[0].telemetry={}; remote.fleets[0].telemetry=confirmed_fleet_telemetry(remote,remote.fleets[0]);
    auto delayed=TurnProcessor{}.process(remote,{{1,{MoveFleetOrder{1,{1000,0},5,{FleetArrivalActionKind::ScrapFleet}}}}});
    assert(!delayed.fleets.empty() && !delayed.fleets[0].pendingCommands.empty());
    for (int year=0;year<30 && !delayed.fleets.empty();++year) delayed=TurnProcessor{}.process(delayed,{});
    assert(delayed.fleets.empty() && delayed.wrecks.size()==1);
    assert(!delayed.pendingFleetLossContacts.empty());
    const auto view=make_player_view(delayed,1).state;
    assert(!view.fleets.empty() && view.wrecks.empty());
    assert(view.players[0].strategicIntel.empty());
    const auto due=delayed.pendingFleetLossContacts[0].deliveryTurn;
    while(delayed.turn<due) delayed=TurnProcessor{}.process(delayed,{});
    const auto received=make_player_view(delayed,1).state;
    assert(received.fleets.empty() && received.players[0].strategicIntel.size()==1);
}
void materialCollection()
{
    auto s=fixture(); auto& f=s.fleets[0]; f.task=FleetTask::Salvage;
    const auto capacity=fleet_cargo_capacity(s,f);
    f.minerals={capacity-10,0,0};
    s.wrecks={{1,{0,0},100,{}, {100,200,300}}}; s.nextWreckId=2;
    run_strategic_tasks(s,2);
    assert(s.wrecks.size()==1 && near(total(s.wrecks[0].minerals),590));
    assert(near(f.minerals.boranium,10.0/3) && near(f.minerals.germanium,5));
    assert(near(fleet_cargo_used(s,f),capacity));
    run_strategic_tasks(s,3); assert(near(total(s.wrecks[0].minerals),590));
    auto second=f; second.id=2; second.minerals={}; s.fleets.push_back(second);
    run_strategic_tasks(s,4);
    assert(s.wrecks.empty());
    assert(near(total(s.fleets[1].minerals),590));
    run_strategic_tasks(s,5); assert(near(total(s.fleets[1].minerals),590));
    assert(std::none_of(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),
        [](const auto& r){return r.kind==PlayerReportKind::ScientificData;}));
}
void safeFailuresAndFinalPacket()
{
    for (int special=0;special<4;++special) {
        auto s=fixture();
        if(special==0)s.fleets[0].colonists=1000;
        if(special==1)s.fleets[0].xenocrystal=1;
        if(special==2)s.fleets[0].outpost=OutpostKind::FuelDepot;
        if(special==3)s.fleets[0].scienceSamples.push_back({});
        assert(!scrap_fleet(s,s.fleets[0],2));
        assert(s.fleets.size()==1 && s.wrecks.empty() && total(s.planets[0].minerals)==0);
    }
    auto full=fixture(); full.planets[0].minerals.ironium=1e15;
    assert(!scrap_fleet(full,full.fleets[0],2));
    assert(full.players[0].pendingPlayerReports.back().quantity==6);
    for (const auto owner : {PlayerId{0},PlayerId{2}}) {
        auto other=fixture();other.planets[0].owner=owner;
        assert(scrap_fleet(other,other.fleets[0],2));
        assert(other.wrecks.size()==1 && total(other.planets[0].minerals)==0);
    }
    auto silent=fixture(); silent.fleets[0].position={1000,0};
    silent.fleets[0].electronics.mode=EmissionMode::RadioSilence;
    silent.fleets[0].electronics.resumeTurn=4;
    silent.fleets[0].telemetry={}; silent.fleets[0].telemetry=confirmed_fleet_telemetry(silent,silent.fleets[0]);
    onboard(silent);
    silent=TurnProcessor{}.process(silent,{});
    assert(silent.fleets.size()==1 && silent.wrecks.empty());
    assert(silent.players[0].pendingPlayerReports.back().deliveryTurn==kAwaitingTransmission);
    for(int i=0;i<5 && !silent.fleets.empty();++i)silent=TurnProcessor{}.process(silent,{});
    assert(silent.fleets.empty() && silent.wrecks.size()==1);
    assert(std::none_of(silent.players[0].pendingPlayerReports.begin(),silent.players[0].pendingPlayerReports.end(),
        [](const auto& r){return r.deliveryTurn==kAwaitingTransmission && r.transmissionFleet==1;}));
}
}
int main()
{
    refundsAndRandomness(); actualOrdersAndDelay(); materialCollection(); safeFailuresAndFinalPacket();
}
