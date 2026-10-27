#include "main_window.hpp"
#include "orbital_base_fixture.hpp"
#include "suns/campaign.hpp"
#include "suns/stations.hpp"
#include <QApplication>
#include <QLabel>
#include <QComboBox>
#include <QDialog>
#include <QTimer>
#include <chrono>
#include <ctime>
#include <iostream>
#include <cassert>
#include <cmath>
using namespace suns;
namespace suns {
struct MainWindowTestAccess {
 static void set(MainWindow& w, GameState s) { w.state_=std::move(s);w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.star=w.state_.planets.front().star;w.selection_.fleet=w.state_.fleets.front().id;w.rebuildScene(); }
 static QString counter(MainWindow& w){ return w.empireLabel_->text(); }
 static void change(MainWindow& w,GameState s){w.state_=std::move(s);w.rebuildScene();}
 static void selectStar(MainWindow& w,StarId id){w.selection_.star=id;w.rebuildScene();}
 static void clearWarning(MainWindow& w){w.waypointFuelKey_.clear();}
 static void selectFleet(MainWindow& w,FleetId id){w.selection_.fleet=id;w.rebuildScene();}
 static void merge(MainWindow& w){w.openMergeFleetsDialog();}
};
}
void missing_miner_forecast(MainWindow& w) {
 auto s=orbital_test::world(5000);s.fleets.clear();s.nextFleetId=3;
 auto& p=s.planets[1];p.minerals={};p.observedConcentration=MineralCargo{100,100,100};
 s.shipDesigns.push_back({204,1,"Remote miner",ShipHullType::MiniMiner,
     {ShipComponentType::QuickJump5,ShipComponentType::RoboMiniMiner,ShipComponentType::RoboMiniMiner}});
 s.nextShipDesignId=205;assert(ship_design_valid(*find_ship_design(s,204)));
 assert(ship_design_available_to_player(s,1,*find_ship_design(s,204)));
 Fleet carrier;carrier.id=1;carrier.owner=1;carrier.design=202;carrier.ships={{202,1}};
 carrier.name="Collector";carrier.position={5000,0};carrier.fuel=500;
 carrier.destination=carrier.position;carrier.warp=1;
 carrier.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::LoadAllAvailable};
 carrier.arrivalAction->cargo=FleetCargoKind::Ironium;
 Fleet miner;miner.id=2;miner.owner=1;miner.design=204;miner.ships={{204,1}};
 miner.name="Last reported miner";miner.position=carrier.position;miner.fuel=100;miner.task=FleetTask::RemoteMining;
 s.fleets={carrier,miner};orbital_test::acknowledge(s);
 s.fleets[0].telemetry.warp=1;s.fleets[0].telemetry.arrivalAction=carrier.arrivalAction;
 s.fleets[1].telemetry.task=FleetTask::RemoteMining;
 set_survey_level(s,1,2,SurveyLevel::GeologicalSurvey,s.turn);
 s.pendingFleetLossContacts.push_back({s.fleets[1],s.turn+34});s.fleets.pop_back();
 auto received=make_player_view(s,1).state;
 assert(received.fleets.size()==2);
 const auto control=TurnProcessor{}.process(received,{});
 const auto f=std::find_if(control.fleets.begin(),control.fleets.end(),[](const auto& f){return f.id==1;});
 assert(f!=control.fleets.end() && std::abs(f->minerals.ironium-8)<1e-6);
 MainWindowTestAccess::set(w,s);
 const auto forecast=w.selectedFleetRouteProgramForecast();
 assert(forecast.contains("projected mineral cargo aboard: 8.0"));
 assert(!forecast.contains("projected mineral cargo aboard: 16.0"));
}
int main(int argc,char** argv){
 QApplication app(argc,argv);MainWindow w;missing_miner_forecast(w);
 auto s=orbital_test::world(5000);orbital_test::deploy(s);orbital_test::base(s).orbitalBase.assemblyYears=6;orbital_test::base(s).orbitalBase.supported=true;
 s.shipDesigns.push_back({203,1,"New colony ship",ShipHullType::ColonyShip,{ShipComponentType::FusionDrive,ShipComponentType::ColonyModule}});s.nextShipDesignId=204;
 orbital_test::acknowledge(s);MainWindowTestAccess::set(w,s);
 const auto before=MainWindowTestAccess::counter(w);
 std::cout<<"COUNTER BEFORE "<<MainWindowTestAccess::counter(w).toStdString()<<"\n";
 apply_orbital_base_command(s,orbital_test::base(s),{OrbitalBaseAction::QueueShip,0,203,101});service_orbital_bases(s,s.turn);
 std::cout<<"physical="<<s.fleets.size()<<" received="<<make_player_view(s,1).state.fleets.size()<<" hidden="<<unreported_orbital_ship(s,s.fleets.back())<<"\n";
 MainWindowTestAccess::change(w,s);assert(MainWindowTestAccess::counter(w)==before);std::cout<<"COUNTER AFTER "<<MainWindowTestAccess::counter(w).toStdString()<<"\n";


 auto merge=orbital_test::world(5000);orbital_test::deploy(merge);orbital_test::base(merge).orbitalBase.assemblyYears=6;orbital_test::base(merge).orbitalBase.supported=true;
 Fleet knownTransport;knownTransport.id=2;knownTransport.owner=1;knownTransport.design=202;knownTransport.ships={{202,1}};knownTransport.position={5000,0};knownTransport.name="Known supply transport";merge.fleets.push_back(knownTransport);merge.nextFleetId=3;
 orbital_test::acknowledge(merge);apply_orbital_base_command(merge,orbital_test::base(merge),{OrbitalBaseAction::QueueShip,0,201,101});service_orbital_bases(merge,merge.turn);MainWindowTestAccess::set(w,merge);MainWindowTestAccess::selectFleet(w,2);
 std::cout<<"MERGE physical="<<merge.fleets.size()<<" received="<<make_player_view(merge,1).state.fleets.size()<<" new_hidden="<<unreported_orbital_ship(merge,merge.fleets.back())<<"\n";
 QTimer::singleShot(0,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());auto* combo=dialog?dialog->findChild<QComboBox*>():nullptr;assert(dialog && !combo);std::cout<<"MERGE COMBO "<<(combo?combo->count():-1)<<" first="<<(combo?combo->itemText(0).toStdString():"none")<<"\n";if(dialog)dialog->reject();});MainWindowTestAccess::merge(w);
 auto refuel=orbital_test::world(5000);auto& ship=orbital_test::base(refuel);
 ship.design=201;ship.ships={{201,1}};ship.position={4995,0};ship.fuel=10;ship.colonists=0;ship.minerals={};
 ship.destination=Position{5000,0};ship.warp=5;ship.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::Refuel};ship.waypointQueue={{{5050,0},5,{}}};
 auto third=refuel.planets[1];third.id=3;third.star=3;refuel.planets.push_back(third);refuel.stars.push_back({3,"Further target",{5050,0},StarClass::Yellow});
 refuel.planets[1].owner=1;refuel.planets[1].population=1000;
 refuel.orbitalStations.push_back({2,1,2,"Remote refueling station",OrbitalStationHullType::OrbitalDock,{OrbitalStationModule::Shipyard,OrbitalStationModule::RefuelingDepot}});refuel.nextOrbitalStationId=3;
 queue_survey_report(refuel,1,2,0,{0,0},refuel.turn,SurveyLevel::DeepSurvey);queue_survey_report(refuel,1,3,0,{0,0},refuel.turn,SurveyLevel::DeepSurvey);deliver_due_survey_reports(refuel);
 orbital_test::acknowledge(refuel);ship.telemetry.warp=ship.warp;ship.telemetry.arrivalAction=ship.arrivalAction;ship.telemetry.waypointQueue=ship.waypointQueue;MainWindowTestAccess::set(w,refuel);
 const auto liveForecast=w.selectedFleetRouteProgramForecast();std::cout<<"REFUEL LIVE "<<liveForecast.toStdString()<<"\n";
 auto lost=refuel;lost.pendingStationContacts.push_back({lost.orbitalStations.front(),lost.turn,lost.turn+34});lost.orbitalStations.clear();MainWindowTestAccess::change(w,lost);
 const auto lostForecast=w.selectedFleetRouteProgramForecast();assert(liveForecast==lostForecast);std::cout<<"REFUEL LOST "<<lostForecast.toStdString()<<"\n";
 std::cout<<"REFUEL EQUAL "<<(liveForecast==lostForecast)<<" known_live="<<make_player_view(refuel,1).state.orbitalStations.size()<<" known_lost="<<make_player_view(lost,1).state.orbitalStations.size()<<"\n"<<std::flush;
}
