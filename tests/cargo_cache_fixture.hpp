#pragma once
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/deep_space.hpp"
#include <algorithm>
#include <cassert>
namespace suns::test {
inline GameState cargo_cache_fixture(Position position={50,-50},bool anchored=true)
{
    auto s=make_demo_game();s.wormholeRules.spawnChancePerTurn=0;
    s.stars.resize(2);s.planets.resize(2);s.stars[0].position={0,0};s.stars[1].position={1040,0};
    s.players.front().technology.levels.fill(26);s.players.front().technology.researchActive=false;
    s.players.front().pendingSurveyReports.clear();s.players.front().pendingPlayerReports.clear();
    s.shipDesigns.push_back({100,1,"Cache constructor",ShipHullType::Scout,
        {ShipComponentType::FusionDrive,ShipComponentType::CargoCacheKit}});
    s.shipDesigns.push_back({101,1,"Cache carrier",ShipHullType::HeavyTransport,
        {ShipComponentType::FusionDrive,ShipComponentType::FusionDrive,ShipComponentType::FusionDrive}});
    for(auto& d:s.shipDesigns)normalize_ship_design_placement(d);s.nextShipDesignId=102;
    assert(ship_design_valid(s.shipDesigns[s.shipDesigns.size()-2]) && ship_design_valid(s.shipDesigns.back()));
    s.fleets.clear();Fleet cache;cache.id=1;cache.owner=1;cache.design=100;cache.ships={{100,1}};
    cache.name="Forward cache";cache.position=position;cache.fuel=20;
    cache.telemetry.observedTurn=s.turn;cache.telemetry.position=position;cache.telemetry.ships=cache.ships;cache.telemetry.fuel=cache.fuel;
    auto visitor=cache;visitor.id=2;visitor.design=101;visitor.ships={{101,12}};visitor.name="Mineral carrier";
    visitor.colonists=1000;visitor.minerals={1200,700,600};visitor.fuel=1000;
    visitor.telemetry.ships=visitor.ships;visitor.telemetry.colonists=visitor.colonists;
    visitor.telemetry.minerals=visitor.minerals;visitor.telemetry.fuel=visitor.fuel;
    s.fleets={cache,visitor};s.nextFleetId=3;
    if(anchored) {s.fleets.front().task=FleetTask::DeployOutpost;deploy_outposts(s,s.turn);assert(s.fleets.front().outpost==OutpostKind::CargoCache);}
    publish_fleet_telemetry(s,s.turn);return s;
}
inline const Fleet& cache_fleet(const GameState& state,FleetId id)
{
    const auto i=std::find_if(state.fleets.begin(),state.fleets.end(),[&](const auto& f){return f.id==id;});
    assert(i!=state.fleets.end());return *i;
}
inline GameState cargo_cache_with_base()
{
    auto s=cargo_cache_fixture();s.fleets[1].minerals={};s.fleets[1].colonists=0;
    s.shipDesigns.push_back({102,1,"Cache supply base",ShipHullType::ExpeditionBase,
        {ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,
         ShipComponentType::QuickJump5,ShipComponentType::ExpeditionDock}});
    normalize_ship_design_placement(s.shipDesigns.back());assert(ship_design_valid(s.shipDesigns.back()));s.nextShipDesignId=103;
    Fleet f;f.id=3;f.owner=1;f.name="Cache supply base";f.design=102;f.ships={{102,1}};
    f.position=s.fleets.front().position;f.fuel=fleet_fuel_capacity(s,f);f.minerals={100,0,0};
    s.fleets.push_back(f);s.nextFleetId=4;
    for(auto& fleet:s.fleets){fleet.telemetry={};fleet.telemetry=confirmed_fleet_telemetry(s,fleet);}
    return s;
}
}
