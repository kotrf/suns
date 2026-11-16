#include "cargo_cache_fixture.hpp"
#include "suns/production.hpp"
#include "suns/transport_lines.hpp"
#include <cmath>
#include <iostream>
using namespace suns;
namespace {
bool near(double a,double b){return std::abs(a-b)<1e-6;}
void paid_manufacture_and_route_deployment()
{
    auto s=test::cargo_cache_fixture({50,-50},false);const auto mobile=fleet_cargo_capacity(s,s.fleets.front());
    assert(mobile==ship_design_cargo_capacity(*find_ship_design(s,100)));
    assert(component_spec(ShipComponentType::CargoCacheKit).cargoCapacity==0);
    auto& p=s.planets.front();p.population=1000000;p.industry=1000;p.mines=0;p.minerals={10000,10000,10000};p.productionQueue.clear();
    const auto cost=ship_design_mineral_cost(*find_ship_design(s,100));
    const auto control=TurnProcessor{}.process(s,{});
    s=TurnProcessor{}.process(s,{{1,{QueueShipDesignOrder{p.id,100}}}});
    assert(s.fleets.size()==3 && s.fleets.back().design==100 && s.fleets.back().outpost==OutpostKind::None);
    assert(near(control.planets.front().minerals.ironium-s.planets.front().minerals.ironium,cost.ironium));
    assert(near(control.planets.front().minerals.boranium-s.planets.front().minerals.boranium,cost.boranium));
    assert(near(control.planets.front().minerals.germanium-s.planets.front().minerals.germanium,cost.germanium));
    const auto id=s.fleets.back().id;const Position site{90,-60};
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{id,site,5,{FleetArrivalActionKind::DeployOutpost}}}}});
    for(unsigned i=0;i<10 && test::cache_fleet(s,id).outpost==OutpostKind::None;++i)s=TurnProcessor{}.process(s,{});
    const auto& built=test::cache_fleet(s,id);
    assert(built.outpost==OutpostKind::CargoCache && same_position(built.position,site));
    assert(fleet_cargo_capacity(s,built)==mobile+kOutpostCacheCapacityKt && !built.colonists && mineral_cargo_mass(built.minerals)==0);
    assert(outpost_state_valid(s,built));
    assert(fleet_sensor_range(s,built)==0 && !built.destination && built.waypointQueue.empty());
}
void material_exchange_capacity_and_rejection_are_atomic()
{
    auto s=test::cargo_cache_fixture();const MineralCargo amount{1000,600,400};
    s=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,2},{0,1},0,amount}}}});
    assert(near(mineral_cargo_mass(s.fleets[0].minerals),2000) && near(mineral_cargo_mass(s.fleets[1].minerals),500));
    assert(s.fleets[0].colonists==0 && s.fleets[1].colonists==1000);
    const auto unchanged=[&](const PlayerOrders& orders) {
        const auto next=TurnProcessor{}.process(s,{orders});
        for(std::size_t i=0;i<2;++i) {
            assert(near(next.fleets[i].minerals.ironium,s.fleets[i].minerals.ironium));
            assert(near(next.fleets[i].minerals.boranium,s.fleets[i].minerals.boranium));
            assert(near(next.fleets[i].minerals.germanium,s.fleets[i].minerals.germanium));
            assert(next.fleets[i].colonists==s.fleets[i].colonists);
        }
    };
    unchanged({1,{TransferCargoOrder{{0,2},{0,1},0,{0.01,0,0}}}}); // Full cache.
    unchanged({1,{TransferCargoOrder{{0,2},{0,1},1,{1,0,0}}}}); // Mixed people/materials reject together.
    s.fleets[0].fuel=0;s.fleets[0].telemetry={};s.fleets[0].telemetry=confirmed_fleet_telemetry(s,s.fleets[0]);
    assert(!outpost_online(s.fleets[0]) && fleet_cargo_capacity(s,s.fleets[0])==2000);
    const auto back=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,1},{0,2},0,{100,0,0}}}}});
    assert(near(back.fleets[0].minerals.ironium,900) && near(back.fleets[1].minerals.ironium,300));
    assert(near(mineral_cargo_mass(back.fleets[0].minerals)+mineral_cargo_mass(back.fleets[1].minerals),2500));
    if(!find_player(s,2)){Player other{2,"Other empire"};other.race=race_preset(RacePreset::Terran);s.players.push_back(other);}
    unchanged({2,{TransferCargoOrder{{0,1},{0,2},0,{1,0,0}}}});
    s.fleets[1].position.x+=1;unchanged({1,{TransferCargoOrder{{0,1},{0,2},0,{1,0,0}}}});
}
void cache_cannot_be_unanchored_or_become_housing()
{
    const auto s=test::cargo_cache_fixture();
    const auto next=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,{70,-50},5,{}},
        MergeFleetsOrder{1,2},SetFleetTaskOrder{1,FleetTask::RemoteMining}}}});
    const auto& cache=test::cache_fleet(next,1);assert(cache.outpost==OutpostKind::CargoCache);
    assert(same_position(cache.position,s.fleets.front().position) && !cache.destination && cache.task==FleetTask::None);
    assert(!cache.colonists && next.fleets.size()==s.fleets.size());
    auto corrupt=s;corrupt.fleets[0].minerals={2001,0,0};assert(!outpost_state_valid(corrupt,corrupt.fleets[0]));
    corrupt=s;corrupt.fleets[0].colonists=1;assert(!outpost_state_valid(corrupt,corrupt.fleets[0]));
    corrupt=s;corrupt.fleets[0].damagePercent=100;corrupt.fleets[0].fuel=0;
    assert(outpost_state_valid(corrupt,corrupt.fleets[0]) && fleet_cargo_capacity(corrupt,corrupt.fleets[0])==2000);
}
void researched_kit_has_no_mobile_storage_or_stock_gift()
{
    auto s=test::cargo_cache_fixture({50,-50},false);auto& tech=s.players.front().technology.levels;
    tech[std::size_t(ResearchField::Construction)]=5;assert(!component_available_to_player(s,1,ShipComponentType::CargoCacheKit));
    tech[std::size_t(ResearchField::Construction)]=6;assert(component_available_to_player(s,1,ShipComponentType::CargoCacheKit));
    assert(construction_kit_kind(ShipComponentType::CargoCacheKit)==OutpostKind::CargoCache);
    const auto denied=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,2},{0,1},0,{1,0,0}}}}});
    assert(mineral_cargo_mass(denied.fleets.front().minerals)==0);
    s.fleets.front().colonists=1;assert(!outpost_deployment_error(s,s.fleets.front(),s.fleets.front().position).empty());
}
void base_delivery_cannot_fund_command_phase_cargo()
{
    const auto s=test::cargo_cache_with_base();
    const PlayerOrders orders{1,{TransferCargoOrder{{0,3},{0,1},0,{100,0,0}},
        TransferCargoOrder{{0,1},{0,2},0,{100,0,0}}}};
    const auto actual=TurnProcessor{}.process(s,{orders});
    assert(test::cache_fleet(actual,1).minerals.ironium==100 && test::cache_fleet(actual,2).minerals.ironium==0);
    assert(test::cache_fleet(actual,3).minerals.ironium==0);
    std::vector<PlannedBaseCargoTransfer> deferred;
    const auto preview=projected_cargo_order_state(make_player_view(s,1).state,orders,&deferred);
    assert(test::cache_fleet(preview,2).minerals.ironium==0);
    assert(test::cache_fleet(preview,1).minerals.ironium==0 && test::cache_fleet(preview,3).minerals.ironium==100);
    const auto service=projected_cargo_service_state(preview,deferred);
    for(FleetId id:{1u,2u,3u})assert(test::cache_fleet(service,id).minerals.ironium==test::cache_fleet(actual,id).minerals.ironium);
    const PlayerOrders departing{1,{orders.orders.front(),MoveFleetOrder{3,{90,-50},3}}};
    const auto command=projected_cargo_order_state(make_player_view(s,1).state,departing,&deferred);
    assert(test::cache_fleet(projected_cargo_service_state(command,deferred),1).minerals.ironium==0);
    assert(test::cache_fleet(TurnProcessor{}.process(s,{departing}),1).minerals.ironium==0);
}
void cargo_cache_rejects_logistics_at_admission_and_arrival()
{
    auto s=test::cargo_cache_fixture();auto& carrier=s.fleets[1];
    const auto home=s.planets.front().id;const auto at=find_star(s,s.planets.front().star)->position;
    carrier.position=at;carrier.minerals={};carrier.colonists=0;carrier.telemetry={};carrier.telemetry=confirmed_fleet_telemetry(s,carrier);
    for(auto& p:s.planets){p.industry=0;p.mines=0;p.productionQueue.clear();}
    s.planets.front().minerals={500,0,0};set_survey_level(s,1,s.planets.front().star,SurveyLevel::DeepSurvey,s.turn);
    TransportLineSpec line{1,1,"Cache logistics",home,0,5,TransportCargo::Ironium,true};line.destinationFleet=1;
    assert(!transport_line_error(make_player_view(s,1),line).empty());
    assert(!set_transport_line(s,1,{line}));
    auto extra=line;extra.destinationFleet=0;extra.destination=s.planets.back().id;extra.deliveries={{0,1,0}};
    s.planets.back().owner=1;s.planets.back().population=1000;
    set_survey_level(s,1,s.planets.back().star,SurveyLevel::DeepSurvey,s.turn);
    assert(!transport_line_error(make_player_view(s,1),extra).empty() && !set_transport_line(s,1,{extra}));
    TransportLine saved{line,at,s.fleets.front().position,{},{}};
    s.players.front().transportLines={saved};s.players.front().lastTransportLineId=1;
    assert(!assign_transport_line(s,1,{1,2}));
    const auto programme=transport_line_program(saved);
    const MoveFleetOrder oldProgramme{2,programme.destination,programme.warp,programme.arrivalAction,
        programme.queuedWaypoints,programme.repeatOrders};
    for(unsigned year=0;year<10;++year) {
        s=TurnProcessor{}.process(s,year?std::vector<PlayerOrders>{}:std::vector<PlayerOrders>{{1,{oldProgramme}}});
        assert(test::cache_fleet(s,1).minerals.ironium==0);
    }
    s.players.front().transportLines.front().assignments={{2,s.turn}};
    auto paused=line;++paused.revision;paused.active=false;assert(set_transport_line(s,1,{paused}));
    assert(!test::cache_fleet(s,2).destination && !test::cache_fleet(s,2).repeatOrders);
}
void merge_does_not_reclassify_an_earlier_immediate_transfer()
{
    for(bool crystal:{false,true}) {
        auto s=test::cargo_cache_with_base();
        s.fleets[1].minerals=crystal?MineralCargo{}:MineralCargo{200,0,0};
        s.fleets[2].minerals=crystal?MineralCargo{}:MineralCargo{100,0,0};
        if(crystal){s.fleets[1].xenocrystal=200;s.fleets[2].xenocrystal=100;}
        for(auto& f:s.fleets){f.telemetry={};f.telemetry=confirmed_fleet_telemetry(s,f);}
        const auto transfer=[&](FleetId source)->Order {
            if(crystal)return TransferXenocrystalOrder{{0,source},{0,1},100};
            return TransferCargoOrder{{0,source},{0,1},0,{100,0,0}};
        };
        const auto quantity=[&](const GameState& state,FleetId id) {
            const auto& f=test::cache_fleet(state,id);return crystal?double(f.xenocrystal):f.minerals.ironium;
        };
        std::vector<PlannedBaseCargoTransfer> deferred;
        for(int scenario:{1,0,2}) {
            const PlayerOrders orders{1,scenario==1?std::vector<Order>{transfer(3),MergeFleetsOrder{2,3}}:
                scenario==0?std::vector<Order>{transfer(2),MergeFleetsOrder{2,3}}:
                    std::vector<Order>{MergeFleetsOrder{2,3},transfer(2)}};
            const auto actual=TurnProcessor{}.process(s,{orders});
            assert(actual.fleets.size()==2 && quantity(actual,1)==(scenario==1?0:100));
            const auto command=projected_cargo_order_state(make_player_view(s,1).state,orders,&deferred);
            assert(deferred.size()==(scenario==0?0:1));
            if(!deferred.empty())assert(deferred.front().receiver==(scenario==1?3u:2u));
            const auto service=projected_cargo_service_state(command,deferred);
            assert(quantity(service,1)==quantity(actual,1) && quantity(service,2)==quantity(actual,2));
            if(scenario==0 && !crystal) {
                const auto excessive=projected_cargo_service_draft(service,1,{{{0,1},{0,2},0,{200,0,0}}});
                assert(quantity(excessive,1)==100 && quantity(excessive,2)==200);
                const auto available=projected_cargo_service_draft(service,1,{{{0,1},{0,2},0,{100,0,0}}});
                assert(quantity(available,1)==0 && quantity(available,2)==300);
            }
        }
    }
}

}
int main()
{
    paid_manufacture_and_route_deployment();
    material_exchange_capacity_and_rejection_are_atomic();
    cache_cannot_be_unanchored_or_become_housing();
    researched_kit_has_no_mobile_storage_or_stock_gift();
    base_delivery_cannot_fund_command_phase_cargo();
    cargo_cache_rejects_logistics_at_admission_and_arrival();
    merge_does_not_reclassify_an_earlier_immediate_transfer();
    std::cout<<"Cargo cache core passed\n";
}
