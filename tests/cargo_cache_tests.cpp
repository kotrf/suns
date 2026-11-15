#include "cargo_cache_fixture.hpp"
#include "suns/production.hpp"
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
}
int main()
{
    paid_manufacture_and_route_deployment();
    material_exchange_capacity_and_rejection_are_atomic();
    cache_cannot_be_unanchored_or_become_housing();
    researched_kit_has_no_mobile_storage_or_stock_gift();
    std::cout<<"Cargo cache core passed\n";
}
