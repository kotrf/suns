#pragma once
#include "suns/orbital_bases.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/turn_processor.hpp"
#include "suns/campaign.hpp"
#include <algorithm>
namespace orbital_test {
using namespace suns;
using C=ShipComponentType;
inline Fleet& base(GameState& s) { return *std::find_if(s.fleets.begin(),s.fleets.end(),[](auto& f){return f.id==1;}); }
inline void acknowledge(GameState& s) {
    for(auto& f:s.fleets) {
        f.telemetry={};
        // Test fixtures explicitly deliver an immutable owner snapshot.
        auto t=confirmed_fleet_telemetry(s,f);
        t.observedTurn=s.turn; t.position=f.position; t.destination=f.destination; t.fuel=f.fuel;
        t.minerals=f.minerals; t.colonists=f.colonists; t.ships=f.ships; t.outpost=f.outpost;
        t.orbitalBase=f.orbitalBase; t.damagePercent=f.damagePercent; t.mobileBase=f.mobileBase;
        f.telemetry=t;
    }
}
inline GameState world(double distance=40, C shield=C::AdvancedRadiationScreen) {
    auto s=make_demo_game(); s.fleets.clear(); s.stars.resize(1); s.planets.resize(1); s.orbitalStations.clear();
    s.stars[0].position={0,0}; s.stars[0].id=1;
    s.planets[0].id=1; s.planets[0].star=1; s.planets[0].owner=1; s.planets[0].population=1000000;
    s.planets[0].homeworld=true; s.planets[0].radar=PlanetaryRadarLevel::Full;
    s.planets[0].productionQueue.clear(); s.planets[0].mines=0;
    auto p=s.planets[0]; p.id=2; p.star=2; p.owner=0; p.population=0; p.industry=0; p.mines=0;
    p.name="Uninhabitable frontier"; p.homeworld=false; p.radar=PlanetaryRadarLevel::None;
    p.environment.temperature=100; p.environment.gravity=100; p.environment.radiation=100; p.habitability=-90;
    s.planets.push_back(p); s.stars.push_back({2,"Active frontier",{distance,0},StarClass::BlueWhite});
    s.players[0].technology.levels.fill(26); s.players[0].technology.researchAllocationPercent=0;
    s.players[0].race.habitableRadiation={0,40};
    s.shipDesigns.push_back({200,1,"Orbital constructor",ShipHullType::ExpeditionBase,
        {C::QuickJump5,C::QuickJump5,C::QuickJump5,C::QuickJump5,C::OrbitalShipyardKit,shield,C::RelayArray,C::ExpeditionWorkshop}});
    s.shipDesigns.push_back({201,1,"Light courier",ShipHullType::StarsScout,{C::QuickJump5,C::BatScanner}});
    s.shipDesigns.push_back({202,1,"Supply transport",ShipHullType::LargeFreighter,{C::QuickJump5,C::QuickJump5}});
    s.nextShipDesignId=203;
    Fleet f; f.id=1; f.owner=1; f.design=200; f.ships={{200,1}}; f.position={distance,0}; f.name="Orbital constructor";
    f.fuel=5000; f.colonists=25000; f.minerals={200,120,200};
    s.fleets.push_back(f); s.nextFleetId=2;
    s.wormholes.clear(); s.wormholeRules.spawnChancePerTurn=0; s.unknownObjects.clear(); s.visitors.clear();
    queue_survey_report(s,1,2,1,{0,0},s.turn,SurveyLevel::OrbitalSurvey); (void)deliver_due_survey_reports(s);
    acknowledge(s); return s;
}
inline void deploy(GameState& s) { apply_orbital_base_command(s,base(s),{OrbitalBaseAction::Deploy,2}); }
inline void tick(GameState& s, std::vector<Order> orders={}) { s=TurnProcessor{}.process(s,{PlayerOrders{1,std::move(orders)}}); }
}
