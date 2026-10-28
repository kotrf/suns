#include "orbital_base_fixture.hpp"
#include "suns/stations.hpp"
#include "suns/deep_space.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/mobile_base.hpp"
#include "save_game.hpp"
#include "remote_receiver_fixture.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <cassert>
#include <cmath>
using namespace orbital_test;
void merge_regressions() {
 auto s=world();deploy(s);
 Fleet courier;courier.id=2;courier.owner=1;courier.design=201;courier.ships={{201,1}};courier.position=base(s).position;courier.fuel=100;s.fleets.push_back(courier);s.nextFleetId=3;acknowledge(s);
 auto n=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{2,base(s).position,5,{FleetArrivalActionKind::MergeWithFleet},{},false,1}}}});
 assert(n.fleets.size()==2 && fleet_ship_count(base(n))==1 && outpost_state_valid(n,base(n)));
 QTemporaryDir dir;assert(dir.isValid());SaveGameData save;save.state=n;save.campaignId=55;save.turnToken=66;save.pendingOrders.player=1;save.mode=SessionMode::Host;save.playerTokens={{1,77}};QString error;SaveGameData loaded;
 assert(write_save_game_file(dir.filePath("anchor.suns"),save,error));assert(read_save_game_file(dir.filePath("anchor.suns"),loaded,error));
 auto d=world();d.fleets[0].colonists=0;d.fleets[0].minerals={};d.fleets[0].damagePercent=50;
 auto* fit=&d.shipDesigns[d.shipDesigns.size()-3];std::erase(fit->components,C::ExpeditionWorkshop);
 auto small=d.fleets[0];small.id=2;small.design=201;small.ships={{201,1}};small.damagePercent=0;d.fleets.push_back(small);d.nextFleetId=3;acknowledge(d);
 auto manual=TurnProcessor{}.process(d,{{1,{MergeFleetsOrder{1,2}}}});
 auto arrival=TurnProcessor{}.process(d,{{1,{MoveFleetOrder{2,small.position,5,{FleetArrivalActionKind::MergeWithFleet},{},false,1}}}});
 assert(std::abs(manual.fleets[0].damagePercent-arrival.fleets[0].damagePercent)<1e-9);
}
void root_and_habitat() {
 auto s=world(9000);s.planets[0].homeworld=false;
 assert(communication_delay_turns(s,1,{9000,0})==60);
 s.planets[0].homeworld=true;const auto delay=communication_delay_turns(s,1,{9000,0});
 s.emissions.clear();record_command_transmission(s,1,{9000,0},s.turn);const auto pulses=s.emissions.size();assert(pulses);
 s.planets[0].habitat.population=s.planets[0].population;s.planets[0].population=0;
 s.emissions.clear();record_command_transmission(s,1,{9000,0},s.turn);
 assert(communication_delay_turns(s,1,{9000,0})==delay && s.emissions.size()==pulses);
 s.players.push_back({2,"Other"});s.emissions.clear();
 EmissionPulse signal;signal.id=999;signal.owner=2;signal.position={100,0};signal.emittedTurn=s.turn;signal.channel=SignalChannel::Fast;signal.maximumRange=300;s.emissions.push_back(signal);
 observe_emission_pulses(s,s.turn);assert(!s.players[0].pendingStrategicIntel.empty());
}
void mesh_order_independence() {
 auto s=world();s.fleets.clear();s.planets[0].radar=PlanetaryRadarLevel::Local;
 s.shipDesigns.push_back({500,1,"Relay scout",ShipHullType::StarsScout,{C::QuickJump5,C::RhinoScanner}});
 for(int i=1;i<=32;++i){Fleet f;f.id=i;f.owner=1;f.design=500;f.ships={{500,1}};f.position={75.0*i,0};s.fleets.push_back(f);}
 for(int order=0;order<2;++order) {
   for(const auto& f:s.fleets)assert(communication_delay_turns(s,1,f.position)==0);
   std::reverse(s.fleets.begin(),s.fleets.end());
 }
}
void safe_ai_target() {
 auto s=generate_campaign(GalaxyConfig{},{{"AI",RacePreset::Terran}});s.fleets.clear();s.players[0].technology.levels.fill(26);s.players[0].technology.researchAllocationPercent=0;s.stars.resize(3);s.planets.resize(3);s.orbitalStations.resize(1);
 auto& home=s.planets[0];home.id=1;home.star=1;home.owner=1;home.population=1000000;home.minerals={10000,10000,10000};home.productionQueue.clear();s.stars[0].id=1;s.stars[0].position={0,0};s.orbitalStations[0].planet=1;s.orbitalStations[0].owner=1;s.orbitalStations[0].modules={OrbitalStationModule::Shipyard};
 for(int i=1;i<3;++i){auto&p=s.planets[i];p=home;p.id=i+1;p.star=i+1;p.owner=0;p.population=0;p.homeworld=false;p.environment={100,100,100};p.habitability=-90;s.stars[i]={StarId(i+1),"Target",{double(100*i),0},i==1?StarClass::BlueWhite:StarClass::Red};set_survey_level(s,1,i+1,SurveyLevel::OrbitalSurvey,s.turn);}
 set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);PlayerOrders orders{1,{}};ComputerState memory;plan_orbital_bases(make_player_view(s,1),orders,memory);assert(orders.orders.size()==2);
}
void delayed_cargo_and_birth() {
 auto s=world(5000);s.turn=5;s.fleets.clear();
 for(int i=0;i<2;++i){Fleet f;f.id=10+i;f.owner=1;f.design=202;f.ships={{202,1}};f.position={5000,0};f.fuel=500;f.minerals={i?0.0:10.0,0,0};f.telemetry.observedTurn=1;f.telemetry.position=f.position;f.telemetry.ships=f.ships;f.telemetry.fuel=f.fuel;f.telemetry.minerals=f.minerals;s.fleets.push_back(f);}s.nextFleetId=12;
 auto n=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,10},{0,11},0,{5,0,0}},MoveFleetOrder{10,{5050,0},5}}}});
 assert(n.fleets[0].minerals.ironium==10 && n.fleets[1].minerals.ironium==0);
 assert(n.fleets[0].pendingCommands[0].deliveryTurn==39);
 s=world(5000);s.fleets.clear();s.nextFleetId=1;auto& p=s.planets[1];p.owner=1;p.population=1000000;p.industry=1000;p.minerals={10000,10000,10000};p.environment=s.planets[0].environment;p.habitability=100;
 s.orbitalStations.push_back({1,1,2,"Remote dock",OrbitalStationHullType::Starbase,{OrbitalStationModule::Shipyard}});s.nextOrbitalStationId=2;
 set_survey_level(s,1,2,SurveyLevel::DeepSurvey,s.turn);
 n=TurnProcessor{}.process(s,{{1,{QueueShipDesignOrder{2,201}}}});
 assert(n.fleets.size()==1 && make_player_view(n,1).state.fleets.empty());
}
void delayed_merge() {
 auto s=world(5000);s.turn=5;s.fleets.clear();
 for(int i=0;i<2;++i){Fleet f;f.id=10+i;f.owner=1;f.design=202;f.ships={{202,1}};f.name="Transport";f.position={5000.0-i,0};f.fuel=500;f.minerals={i?10.0:0.0,0,0};f.telemetry.observedTurn=1;f.telemetry.position=f.position;f.telemetry.ships=f.ships;f.telemetry.fuel=f.fuel;f.telemetry.minerals=f.minerals;if(i){f.destination=Position{5000,0};f.targetFleet=10;f.warp=5;f.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};}s.fleets.push_back(f);}s.nextFleetId=12;
 auto hidden=s;hidden.fleets[1].orbitalShipyardBirth=true;hidden.fleets[1].telemetry={};
 auto unknown=TurnProcessor{}.process(hidden,{});
 assert(make_player_view(unknown,1).state.fleets.size()==1); // No phantom contact for a never-reported source.
 auto n=TurnProcessor{}.process(s,{});const auto v=make_player_view(n,1);
 assert(n.fleets.size()==1 && v.state.fleets.size()==2);
 const auto* old=find_service_fleet(v.state,10);assert(old && fleet_ship_count(*old)==1 && old->minerals.ironium==0);
 QTemporaryDir dir;assert(dir.isValid());SaveGameData save;save.state=n;save.campaignId=55;save.turnToken=66;save.pendingOrders.player=1;save.mode=SessionMode::Host;save.playerTokens={{1,77}};QString error;SaveGameData loaded;
 assert(write_save_game_file(dir.filePath("merge.suns"),save,error));assert(read_save_game_file(dir.filePath("merge.suns"),loaded,error));
 assert(make_player_view(loaded.state,1).state.fleets.size()==2);
 // A full status sent later through a faster connection must bring the merge
 // confirmation too, rather than duplicate the absorbed ship/cargo in exports.
 n.fleets[0].position={0,0};publish_fleet_telemetry(n,n.turn+1);++n.turn;
 (void)deliver_due_player_reports(n);
 assert(n.pendingFleetLossContacts.empty() && make_player_view(n,1).state.fleets.size()==1);
}
void chained_merge_confirmation() {
 for(bool reverseReports:{false,true}) for(bool finalReportAlreadyFaster:{false,true}) {
  auto s=world(5000);s.turn=5;s.fleets.clear();
  for(int i=0;i<3;++i) {
   Fleet f;f.id=10+i;f.owner=1;f.design=202;f.ships={{202,1}};f.name="Chain transport";
   f.position={4999.0+i,0};f.fuel=500;f.minerals={10,0,0};
   f.telemetry.observedTurn=1;f.telemetry.position=f.position;f.telemetry.ships=f.ships;
   f.telemetry.fuel=f.fuel;f.telemetry.minerals=f.minerals;
   if(i==0){f.destination=Position{5000.0+i,0};f.targetFleet=11+i;f.warp=1;f.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};}
   s.fleets.push_back(f);
  }
  s.nextFleetId=13;
  // Two real rendezvous operations, with a route installed aboard B between
  // them. HQ still has the three old contacts while both reports travel home.
  s=TurnProcessor{}.process(s,{});
  assert(s.fleets.size()==2);
  auto intermediate=std::find_if(s.fleets.begin(),s.fleets.end(),[](const auto& f){return f.id==11;});
  assert(intermediate!=s.fleets.end());
  intermediate->destination=Position{5001,0};intermediate->targetFleet=12;
  intermediate->warp=1;intermediate->arrivalAction=FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
  s=TurnProcessor{}.process(s,{});
  assert(s.fleets.size()==1 && s.fleets[0].id==12 && fleet_ship_count(s.fleets[0])==3);
  assert(s.fleets[0].minerals.ironium==30 && s.pendingFleetLossContacts.size()==2);
  assert(make_player_view(s,1).state.fleets.size()==3);
  if(finalReportAlreadyFaster) {
   // Traversal must also cross an edge that needs no deadline adjustment.
   for(auto& report:s.players[0].pendingPlayerReports)
    if(report.kind==PlayerReportKind::FleetsMerged && report.fleet==12)report.deliveryTurn=s.turn+1;
   for(auto& loss:s.pendingFleetLossContacts)
    if(loss.lastContact.id==11)loss.deliveryTurn=s.turn+1;
  }
  if(reverseReports)std::reverse(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end());
  QTemporaryDir dir;assert(dir.isValid());SaveGameData save;save.state=s;save.campaignId=55;save.turnToken=66;
  save.pendingOrders.player=1;save.mode=SessionMode::Host;save.playerTokens={{1,77}};QString error;SaveGameData loaded;
  const auto path=dir.filePath("chain.suns");
  assert(write_save_game_file(path,save,error));assert(read_save_game_file(path,loaded,error));s=std::move(loaded.state);
  s.fleets[0].position={0,0};publish_fleet_telemetry(s,s.turn+1);++s.turn;
  (void)deliver_due_player_reports(s);
  const auto known=make_player_view(s,1).state;
  assert(s.pendingFleetLossContacts.empty() && known.fleets.size()==1);
  assert(fleet_ship_count(known.fleets[0])==3 && known.fleets[0].minerals.ironium==30);
  save.state=s;assert(write_save_game_file(path,make_player_turn(save,1),error));
  assert(read_save_game_file(path,loaded,error));assert(loaded.state.fleets.size()==1);
  assert(fleet_ship_count(loaded.state.fleets[0])==3 && loaded.state.fleets[0].minerals.ironium==30);
 }
}
void conditions_are_frozen() {
 auto s=world(900);auto& p=s.planets[1];p.owner=1;p.population=1000000;p.defenses=20;
 set_survey_level(s,1,2,SurveyLevel::DeepSurvey,s.turn);const auto before=make_player_view(s,1);
 p.defenses=0;p.warDamage.contamination=50;p.warDamage.lastBombardmentTurn=s.turn;
 const auto after=make_player_view(s,1);
 assert(find_planet(after.state,2)->defenses==find_planet(before.state,2)->defenses);
 assert(find_planet(after.state,2)->warDamage==find_planet(before.state,2)->warDamage);
 QTemporaryDir dir;assert(dir.isValid());SaveGameData save;save.state=s;save.campaignId=55;save.turnToken=66;save.pendingOrders.player=1;save.mode=SessionMode::Host;save.playerTokens={{1,77}};QString error;SaveGameData loaded;
 const auto path=dir.filePath("conditions.suns");
 assert(write_save_game_file(path,save,error));assert(read_save_game_file(path,loaded,error));
 const auto restored=make_player_view(loaded.state,1).state;
 assert(find_planet(restored,2)->defenses==20 && find_planet(restored,2)->warDamage.contamination==0);
 QFile file(path);assert(file.open(QIODevice::ReadOnly));auto bytes=file.readAll();file.close();
 auto truncated=bytes;truncated.chop(1);assert(file.open(QIODevice::WriteOnly));file.write(truncated);file.close();
 assert(!read_save_game_file(path,loaded,error));
 {QDataStream out(&bytes,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{99};}
 assert(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();
 assert(!read_save_game_file(path,loaded,error)); // A forged v99 header cannot discard or reinterpret the combined extension tails.
}
void delayed_founding() {
 auto s=test::remote_receiver_fixture();s.players[0].race.environmentBased=false;
 s.planets[1].owner=0;s.planets[1].population=0;s.planets[1].productionQueue.clear();s.planets[1].habitability=80;
 set_survey_level(s,1,2,SurveyLevel::GeologicalSurvey,s.turn);
 Fleet f;f.id=10;f.owner=1;f.design=kColonyShipDesignId;f.ships={{kColonyShipDesignId,1}};f.position={899,0};f.destination=Position{900,0};f.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::Colonize,2};f.warp=5;f.colonists=100;f.fuel=100;s.fleets.push_back(f);s.nextFleetId=11;
 auto n=TurnProcessor{}.process(s,{});
 assert(n.planets[1].owner==1 && known_planet_owner(n,1,2)==0);
 assert(find_planet(make_player_view(n,1).state,2)->owner==0);
 for(int i=0;i<7;++i)n=TurnProcessor{}.process(n,{});
 assert(known_planet_owner(n,1,2)==1 && find_planet(make_player_view(n,1).state,2)->owner==1);
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);chained_merge_confirmation();merge_regressions();root_and_habitat();safe_ai_target();delayed_cargo_and_birth();delayed_merge();conditions_are_frozen();delayed_founding();mesh_order_independence();}
