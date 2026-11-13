#pragma once
#include "suns/campaign.hpp"
#include "suns/unknown_objects.hpp"
namespace suns::test {
inline GameState drifting_object_fixture()
{
    auto s=make_demo_game();s.turn=10;s.stars.resize(1);s.planets.resize(1);s.fleets.resize(1);
    s.stars.front().position={0,0};s.wormholes.clear();s.wormholeRules.spawnChancePerTurn=0;
    auto& p=s.players.front();p.technology.levels.fill(0);p.technology.progress.fill(0);
    p.technology.levels[std::size_t(ResearchField::Construction)]=4;
    p.technology.levels[std::size_t(ResearchField::Electronics)]=4;
    p.technology.researchActive=false;p.technology.researchAllocationPercent=0;
    p.pendingPlayerReports.clear();p.pendingSurveyReports.clear();p.objectKnowledge.clear();
    s.planets.front().productionQueue.clear();
    auto& d=s.shipDesigns.front();d.hull=ShipHullType::Privateer;
    d.components={ShipComponentType::QuickJump5,ShipComponentType::MoleScanner};
    d.placements.clear();normalize_ship_design_placement(d);
    auto& f=s.fleets.front();f.position={100,0};f.destination.reset();f.waypointQueue.clear();f.routeTemplate.clear();
    f.arrivalAction.reset();f.repeatOrders=false;f.task=FleetTask::None;f.targetFleet=0;
    f.ships={{d.id,1}};f.warp=5;f.fuel=200;f.colonists=0;f.minerals={};
    f.telemetry={};f.telemetry.observedTurn=s.turn;f.telemetry.position=f.position;
    f.telemetry.fuel=f.fuel;f.telemetry.warp=f.warp;f.telemetry.ships=f.ships;f.telemetryInTransit.clear();
    s.unknownObjects={{7,{111,0},UnknownObjectKind::DriftingDerelict,false,{1.5,0.5}}};s.nextUnknownObjectId=8;
    return s;
}
inline ObjectObservation drifting_observation(const GameState& s,ObjectKnowledgeLevel level=ObjectKnowledgeLevel::Studied)
{
    const auto& x=s.unknownObjects.front();return {x.id,x.position,0,level,x.kind,s.turn,s.turn,x.velocity};
}
}
