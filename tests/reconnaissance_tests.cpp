#include "suns/reconnaissance.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/turn_processor.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;
using C=ShipComponentType;
GameState fixture(Position observer={420,0},Position target={430,0})
{
    auto s=make_demo_game(); s.wormholeRules.spawnChancePerTurn=0;
    s.players.push_back({2,"Secret empire"}); s.players.front().technology.levels[std::size_t(ResearchField::Electronics)]=3;
    s.shipDesigns.front().components={C::FuelMizer,C::RhinoScanner}; s.shipDesigns.front().placements.clear();
    s.fleets.front().position=observer; s.fleets.front().destination.reset();
    s.fleets.front().telemetry.position=observer; s.fleets.front().telemetry.observedTurn=s.turn;
    s.shipDesigns.push_back({10,2,"Secret fit",ShipHullType::SmallFreighter,{C::FuelMizer,C::StarsCargoPod}});
    s.nextShipDesignId=11;
    Fleet f; f.id=22; f.owner=2; f.name="Secret enemy name"; f.design=10; f.ships={{10,3}}; f.position=target;
    f.colonists=500; f.minerals={1,2,3}; f.fuel=300; f.telemetry.position=target; f.telemetry.ships=f.ships;
    f.telemetry.observedTurn=s.turn; s.fleets.push_back(f); s.nextFleetId=23;
    s.orbitalStations.clear(); return s;
}
void detached_snapshot_survives_both_fleets()
{
    auto s=fixture(); const auto first=TurnProcessor{}.process_with_events(s,{});
    const auto& p=first.state.players.front(); assert(p.pendingEnemyContacts.size()==1 && p.enemyContacts.empty());
    const auto observation=p.pendingEnemyContacts.front();
    assert(observation.quality==ContactQuality::Identified && observation.shipCount==3 && observation.owner==2);
    assert(observation.hulls==std::vector<ShipHullType>{ShipHullType::SmallFreighter});
    assert(observation.deliveryTurn==observation.observedTurn+3 && recon_contact_valid(observation));
    auto state=first.state; state.fleets.clear(); // Packets cannot depend on surviving IDs.
    while(state.turn<observation.deliveryTurn) state=TurnProcessor{}.process(state,{});
    const auto view=make_player_view(state,1).state;
    assert(view.players.front().enemyContacts.size()==1);
    assert(view.players.front().pendingEnemyContacts.empty() && view.players.front().observedEnemyFleets.empty());
    assert(view.players.front().observedSensorStatus.empty() && view.players.front().pendingSensorStatus.empty());
    const auto contact=std::find_if(view.fleets.begin(),view.fleets.end(),[](const auto& f){return f.id==22;});
    assert(contact!=view.fleets.end() && contact->design==0 && contact->ships.empty() && contact->fuel==0 && contact->colonists==0);
    assert(same_position(contact->position,observation.position) && contact->name.find("Secret")==std::string::npos);
    assert(make_player_view(view,1).state.players.front().enemyContacts.front().quality==ContactQuality::Identified);
    // Newer evidence wins, including a lost/reacquired track, even if delivery is reordered.
    auto old=observation; old.observedTurn=1; old.lastSeenTurn=1; old.deliveryTurn=state.turn;
    old.position={999,999}; old.lost=true; state.players.front().pendingEnemyContacts={old};
    deliver_reconnaissance(state); assert(!state.players.front().enemyContacts.front().lost);
    assert(same_position(state.players.front().enemyContacts.front().position,observation.position));
}
void confidence_hysteresis_and_knowledge()
{
    auto s=fixture({300,0},{345,0}); // Outer scanner range: probable, no owner/count/hulls.
    observe_reconnaissance(s,2); const auto first=s.players[0].observedEnemyFleets.front().observation;
    assert(first.quality==ContactQuality::Probable && first.owner==0 && first.shipCount==0 && first.hulls.empty());
    assert(distance_between(first.position,s.fleets.back().position)<=first.uncertainty);
    s.fleets.back().position={334,0}; observe_reconnaissance(s,3);
    assert(s.players[0].observedEnemyFleets.front().observation.quality==ContactQuality::Confirmed);
    s.fleets.back().position={337,0}; observe_reconnaissance(s,4); // 74%: retain confirmed inside 77% hysteresis.
    assert(s.players[0].observedEnemyFleets.front().observation.quality==ContactQuality::Confirmed);
    s.fleets.back().position={340,0}; observe_reconnaissance(s,5);
    assert(s.players[0].observedEnemyFleets.front().observation.quality==ContactQuality::Probable);
    s.fleets.back().position={352,0}; observe_reconnaissance(s,6); // Still tracked in 110% retention band.
    assert(s.players[0].observedEnemyFleets.size()==1);
    s.fleets.back().position={356,0}; observe_reconnaissance(s,7); assert(s.players[0].observedEnemyFleets.empty());
    const auto loss=s.players[0].pendingEnemyContacts.back(); assert(loss.lost && loss.lastSeenTurn==6 && loss.observedTurn==7);
    // Initial local sensing is also measured, never a shortcut to hidden design/cargo.
    s=fixture({0,0},{20,0}); auto view=make_player_view(s,1).state;
    assert(view.players.front().enemyContacts.size()==1 && view.players.front().enemyContacts.front().owner==2);
    for(const auto& d:view.shipDesigns) assert(d.owner==1);
    assert(make_player_view(s,2).state.players.front().pendingEnemyContacts.empty());
}
void interference_and_program_windows()
{
    auto s=fixture({0,0},{20,0}); s.shipDesigns.back().components.push_back(C::Jammer50);
    observe_reconnaissance(s,2);
    assert(s.players[0].pendingSensorStatus.front().interferencePercent==50);
    assert(s.players[0].pendingSensorStatus.front().activeRange==75);
    assert(s.players[0].pendingEnemyContacts.front().quality!=ContactQuality::Identified);
    assert(std::any_of(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::SensorInterferenceChanged;}));
    s.turn=3; const auto events=deliver_due_player_reports(s);
    assert(std::any_of(events.begin(),events.end(),[](const auto& e){return e.sensorStatus && e.sensorStatus->interferencePercent==50;}));
    s.shipDesigns.back().components.pop_back(); observe_reconnaissance(s,4); s.turn=4;
    const auto restored=deliver_due_player_reports(s);
    assert(std::any_of(restored.begin(),restored.end(),[](const auto& e){return e.kind==GameEventKind::SensorInterferenceChanged && e.quantity==0;}));
    s=fixture({0,0},{400,0}); auto& f=s.fleets.front();
    ElectronicsProgram program{EmissionMode::RadioSilence,8,false,2,3,true,false};
    assert(electronics_program_valid(program) && submit_electronics_command(s,1,f.id,program));
    assert(!fleet_transmits(f,2) && !fleet_active_sensors(f,2));
    assert(fleet_available_turn(f,2)==3 && fleet_transmits(f,3) && fleet_active_sensors(f,3));
    assert(!fleet_transmits(f,4) && fleet_available_turn(f,4)==5);
    auto result=TurnProcessor{}.process(s,{}); assert(result.fleets.front().electronics.mode==EmissionMode::RadioSilence);
    result=TurnProcessor{}.process(result,{}); assert(result.turn==3 && result.fleets.front().electronics.mode==EmissionMode::RadioSilence);
    assert(result.fleets.front().telemetry.observedTurn==3); // A window, not permanent wake-up.
    assert(fleet_available_turn(result.fleets.front(),7)==7 && fleet_available_turn(result.fleets.front(),8)==8);
    program.activeBurst=false; assert(!fleet_active_sensors(Fleet{.electronics=program},3));
    program.burstInterval=0; assert(!electronics_program_valid(program));
    program={EmissionMode::RadioSilence,8,false,0,0,false,true};
    s=fixture({1,0},{400,0}); s.fleets.front().electronics=program; s.fleets.front().destination=Position{0,0}; s.fleets.front().warp=1;
    result=TurnProcessor{}.process(s,{}); assert(result.fleets.front().electronics.mode==EmissionMode::Standard);
}
void periodic_scan_and_anonymous_emissions()
{
    auto s=fixture({420,0},{500,0}); s.fleets.front().electronics={EmissionMode::RadioSilence,9,false,2,3,true,false};
    s.turn=2; observe_current_sensor_coverage(s,2); assert(std::none_of(s.players[0].pendingSurveyReports.begin(),s.players[0].pendingSurveyReports.end(),[](const auto& r){return r.sourceFleet==1;}));
    // Add a system that the scheduled pulse can see.
    s.stars.push_back({9,"Pulse target",{445,0}}); s.turn=3;
    observe_current_sensor_coverage(s,3); assert(std::any_of(s.players[0].pendingSurveyReports.begin(),s.players[0].pendingSurveyReports.end(),[](const auto& r){return r.star==9;}));
    s.turn=4; s.players[0].pendingSurveyReports.clear(); observe_current_sensor_coverage(s,4);
    assert(std::none_of(s.players[0].pendingSurveyReports.begin(),s.players[0].pendingSurveyReports.end(),[](const auto& r){return r.star==9;}));
    s.fleets.back().position={430,0};
    auto replay=s; observe_reconnaissance(s,5); std::reverse(replay.fleets.begin(),replay.fleets.end()); observe_reconnaissance(replay,5);
    assert(s.players[0].pendingEnemyContacts.size()==1 && replay.players[0].pendingEnemyContacts.size()==1);
    assert(same_position(s.players[0].pendingEnemyContacts.front().position,replay.players[0].pendingEnemyContacts.front().position));
    assert(s.players[0].pendingEnemyContacts.front().deliveryTurn==replay.players[0].pendingEnemyContacts.front().deliveryTurn);
    // Colony passive reception sees emissions beyond reflected hull range, with no identity.
    s=fixture({0,0},{200,0}); s.fleets.erase(s.fleets.begin()); observe_strategic_objects(s,2);
    assert(!s.players[0].pendingStrategicIntel.empty());
    const auto signal=s.players[0].pendingStrategicIntel.front();
    assert(signal.type==StrategicObjectKind::Emission && signal.owner==0 && signal.radius==15);
    assert(std::all_of(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r){return !r.fleet && !r.contactOwner;}));
}
int main() { detached_snapshot_survives_both_fleets(); confidence_hysteresis_and_knowledge(); interference_and_program_windows(); periodic_scan_and_anonymous_emissions(); }
