#include "suns/stations.hpp"
#include "suns/campaign.hpp"
#include "suns/combat.hpp"
#include "suns/production.hpp"
#include "suns/strategic_operations.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;
using C = ShipComponentType;

GameState colony()
{
    auto s=make_demo_game(); s.fleets.clear(); s.wormholeRules.spawnChancePerTurn=0;
    s.players.front().technology.researchActive=false;
    s.players.front().technology.researchAllocationPercent=0;
    auto& p=s.planets.front(); p.population=100000; p.industry=1000;
    p.minerals={10000,10000,10000}; p.mines=0; p.productionQueue.clear();
    return s;
}
void fitting_and_paid_refit()
{
    auto s=colony(); StationConstruction fit{{OrbitalStationHullType::Starbase,{C::Laser},true,true},1};
    assert(!queue_station_construction(s,1,{1,fit})); // Hull gate.
    s.players.front().technology.levels.fill(26);
    fit.loadout.components={C::FuelMizer}; assert(!station_loadout_valid(fit.loadout));
    fit.loadout.components=std::vector<C>(7,C::Laser); assert(!station_loadout_valid(fit.loadout));
    fit.loadout.components={C::Laser,C::MoleSkinShield,C::BattleComputer};
    s.orbitalStations.front().damagePercent=60;
    assert(queue_station_construction(s,1,{1,fit}));
    assert(!queue_station_construction(s,1,{1,fit}));
    assert(s.orbitalStations.front().damagePercent==60); // Old fit operates until completion.
    auto baselineState=s; baselineState.planets.front().productionQueue.clear();
    const auto before=TurnProcessor{}.process(baselineState,{}).planets.front().minerals;
    const auto expected=station_mineral_cost(fit.loadout);
    auto next=TurnProcessor{}.process(s,{});
    const auto* station=find_orbital_station(next,1);
    assert(station && station->hull==OrbitalStationHullType::Starbase && station->damagePercent==0);
    assert(station->components==fit.loadout.components && next.nextOrbitalStationId==2);
    assert(next.planets.front().minerals.ironium==before.ironium-expected.ironium);
    assert(next.planets.front().productionQueue.empty());
    // A stale target must not rebuild a destroyed station or charge minerals.
    s.orbitalStations.clear(); auto stale=TurnProcessor{}.process(s,{});
    assert(stale.orbitalStations.empty() && stale.planets.front().productionQueue.empty());
    assert(stale.planets.front().minerals.ironium==before.ironium);
}
GameState battle(bool armedStation, bool strongFleet)
{
    auto s=colony(); s.players.push_back({2,"Attacker"}); s.players.push_back({3,"Observer"});
    s.shipDesigns.push_back({10,2,"Attacker",ShipHullType::Destroyer,
        {C::FuelMizer,strongFleet?C::AntiMatterPulverizer:C::LadyFingerBomb,C::Jammer50}});
    s.nextShipDesignId=11;
    Fleet f; f.id=1; f.owner=2; f.design=10; f.ships={{10,strongFleet?100U:1U}};
    f.position=s.stars.front().position; f.fuel=fleet_fuel_capacity(s,f);
    f.telemetry.position=f.position; f.telemetry.ships=f.ships; f.telemetry.observedTurn=s.turn;
    s.fleets={f};
    if(armedStation) s.orbitalStations.front().components={C::Laser};
    return s;
}
void combat_and_delayed_knowledge()
{
    auto s=battle(false,true); const auto result=TurnProcessor{}.process_with_events(s,{});
    assert(result.state.orbitalStations.empty());
    assert(!colony_has_orbital_service(result.state,1,1,OrbitalStationModule::Shipyard));
    assert(result.state.pendingStationContacts.size()==1);
    const auto view=make_player_view(result.state,1);
    assert(view.state.orbitalStations.size()==1 && view.state.orbitalStations.front().damagePercent==0);
    assert(view.state.pendingStationContacts.empty());
    assert(make_player_view(result.state,3).state.orbitalStations.empty());
    assert(result.state.players.front().history.back().battles.ownStationsLost==0);
    assert(std::none_of(result.events.begin(),result.events.end(),[](const auto& e) {return e.kind==GameEventKind::SpaceBattle && e.recipient==1;}));
    auto deliveryState=result.state; deliveryState.fleets.clear();
    const auto delivered=TurnProcessor{}.process_with_events(deliveryState,{});
    assert(make_player_view(delivered.state,1).state.orbitalStations.empty());
    const auto& stats=delivered.state.players.front().history.back().battles;
    assert(stats.ownStationsLost==1 && stats.ownShipsLost==0 && stats.reports==1);
    const auto report=std::find_if(delivered.events.begin(),delivered.events.end(),[](const auto& e){return e.kind==GameEventKind::SpaceBattle && e.recipient==1;});
    assert(report!=delivered.events.end() && report->battle && report->fleet==0 && report->planet==1);
    assert(std::any_of(report->battle->units.begin(),report->battle->units.end(),[](const auto& u){return u.station==1 && u.fleet==0 && u.survivingShips==0;}));
    auto damagedState=battle(false,false);
    damagedState.shipDesigns.back().components={C::FuelMizer,C::Laser,C::CompletePhaseShield};
    resolve_space_battles(damagedState,damagedState.turn+1);
    assert(damagedState.orbitalStations.size()==1);
    const double damage=damagedState.orbitalStations.front().damagePercent;
    assert(damage>0 && damage<100);
    damagedState.fleets.clear();
    assert(TurnProcessor{}.process(damagedState,{}).orbitalStations.front().damagePercent==damage);
    // Armed station catches an unarmed bomber passing through the system.
    s=battle(true,false); s.fleets.front().position={-4,0}; s.fleets.front().destination=Position{4,0}; s.fleets.front().warp=3;
    const auto crossing=TurnProcessor{}.process_with_events(s,{});
    assert(crossing.state.fleets.empty() && crossing.state.orbitalStations.size()==1);
    const auto repeat=TurnProcessor{}.process_with_events(s,{});
    assert(repeat.state.orbitalStations.front().damagePercent==crossing.state.orbitalStations.front().damagePercent);
}
void defense_production_and_bombardment()
{
    auto s=colony(); auto& p=s.planets.front();
    s.players.front().technology.levels.fill(0);
    assert(!apply_production_batch(s,p,{1,ProductionKind::ColonyDefense,2}));
    s.players.front().technology.levels[std::size_t(ResearchField::Energy)]=1;
    p.defenses=99;
    assert(apply_production_batch(s,p,{1,ProductionKind::ColonyDefense,10,ProductionAutomation::Annual}));
    auto baselineState=s; baselineState.planets.front().productionQueue.clear();
    const auto mined=TurnProcessor{}.process(baselineState,{}).planets.front().minerals;
    auto next=TurnProcessor{}.process(s,{});
    assert(next.planets.front().defenses==100 && next.planets.front().productionQueue.size()==1);
    assert(next.planets.front().minerals.ironium==mined.ironium-2);
    next.planets.front().defenses=98; next=TurnProcessor{}.process(next,{});
    assert(next.planets.front().defenses==100 && next.planets.front().productionQueue.size()==1);
    auto guarded=battle(false,false); guarded.orbitalStations.front().components={C::PulsedSapper};
    guarded.shipDesigns.back().components.push_back(C::CompletePhaseShield);
    guarded.fleets.front().task=FleetTask::Bombardment;
    auto defended=TurnProcessor{}.process_with_events(guarded,{});
    assert(defended.state.orbitalStations.size()==1);
    assert(std::none_of(defended.events.begin(),defended.events.end(),[](const auto& e){return e.kind==GameEventKind::Bombardment;}));
    assert(defended.state.fleets.front().task==FleetTask::Bombardment);
    auto assault=battle(true,true); assault.shipDesigns.back().components.push_back(C::LadyFingerBomb);
    assault.fleets.front().task=FleetTask::Bombardment;
    auto breached=TurnProcessor{}.process_with_events(assault,{});
    assert(breached.state.orbitalStations.empty());
    assert(std::any_of(breached.state.players.begin(),breached.state.players.end(),[](const auto& p){return std::any_of(p.pendingPlayerReports.begin(),p.pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::Bombardment;});})
        || std::any_of(breached.events.begin(),breached.events.end(),[](const auto& e){return e.kind==GameEventKind::Bombardment;}));
    auto naked=battle(false,false); naked.orbitalStations.clear(); naked.fleets.front().task=FleetTask::Bombardment;
    // Isolate defense-installation bombard damage; an installed radar also
    // competes for installation hits and has its own separate regression.
    naked.planets.front().radar = PlanetaryRadarLevel::None;
    naked.planets.front().industry=20; naked.planets.front().mines=20;
    auto protectedState=naked; protectedState.planets.front().defenses=100;
    resolve_bombardments(naked,2); resolve_bombardments(protectedState,2);
    assert(protectedState.planets.front().population>naked.planets.front().population);
    assert(protectedState.planets.front().defenses<100);
    assert(colony_defense_coverage(protectedState.planets.front())<1);
}
int main() { fitting_and_paid_refit(); combat_and_delayed_knowledge(); defense_production_and_bombardment(); }
