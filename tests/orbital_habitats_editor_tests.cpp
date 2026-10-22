#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/production.hpp"
#include "suns/campaign.hpp"
#include "suns/player_knowledge.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <cassert>
using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w,const GameState& s) {w.state_=s;w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.star=1;w.rebuildScene();}
    static const auto& orders(MainWindow& w) {return w.pendingOrders_;}
    static const auto& state(MainWindow& w) {return w.state_;}
};
}
namespace {
GameState fixture(bool complete=true)
{
    auto s=make_demo_game();s.turn=10;s.players[0].race=race_preset(RacePreset::Terran);
    s.players[0].technology.levels.fill(18);s.players[0].technology.researchActive=false;
    s.wormholeRules.spawnChancePerTurn=0;s.orbitalStations[0].hull=OrbitalStationHullType::Starbase;
    auto& p=s.planets[0];p.industry=100;p.population=1000000;p.minerals={10000,10000,10000};p.productionQueue.clear();
    if(complete) {
        assert(apply_planetary_engineering_order(s,p,{1,PlanetaryEngineeringKind::OrbitalHabitat,EngineeringAction::StartOrResume,10}));
        for(unsigned year=0;year<60;++year){(void)resolve_colony_production(s,p,true);++s.turn;}
        assert(habitat_completed(p));
    }
    refresh_planetary_engineering(s);set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);return s;
}
QByteArray bytes(const QString& path) {QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
void writeBytes(const QString& path,const QByteArray& data) {QFile f(path);assert(f.open(QIODevice::WriteOnly));assert(f.write(data)==data.size());}
SaveGameData envelope(GameState s) {SaveGameData save;save.state=std::move(s);save.pendingOrders={1,{}};save.campaignId=17;save.turnToken=29;return save;}
void annual_receipts_and_private_evidence_roundtrip()
{
    auto s=fixture();auto& p=s.planets[0];p.habitat.population=200000;
    (void)resolve_colony_production(s,p,false,false);++s.turn;set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    queue_survey_report(s,1,1,0,s.stars[0].position,s.turn,SurveyLevel::DeepSurvey,true);
    auto save=envelope(s);QTemporaryDir dir;const auto path=dir.filePath("habitat.suns");QString error;SaveGameData loaded;
    assert(write_save_game_file(path,save,error));assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.planets[0].habitat==p.habitat);
    assert(loaded.state.players[0].surveyKnowledge[0].planetSnapshot->habitat->habitat==p.habitat);
    assert(loaded.state.players[0].pendingSurveyReports.back().planetSnapshot->habitat->habitat==p.habitat);
    // A retry at the SAME annual resolution date must not debit reserve again.
    --loaded.state.turn;
    const auto reserve=loaded.state.planets[0].habitat.reserveYears;
    (void)resolve_colony_production(loaded.state,loaded.state.planets[0],false,false);
    assert(loaded.state.planets[0].habitat.reserveYears==reserve);
    for(unsigned defect=0;defect<4;++defect) {
        auto bad=save;
        if(defect==0)bad.state.planets[0].habitat.reserveYears=4;
        if(defect==1)bad.state.planets[0].habitat.population=kOrbitalPopulationCapacity+1;
        if(defect==2)bad.state.planets[0].habitat.servicedTurn=bad.state.turn+1;
        if(defect==3)bad.state.planets[0].engineering={};
        assert(write_save_game_file(path,bad,error));assert(!read_save_game_file(path,loaded,error));
    }
    assert(write_save_game_file(path,save,error));auto data=bytes(path);data.chop(1);writeBytes(path,data);assert(!read_save_game_file(path,loaded,error));
    // Historical v93 has no habitat truth/evidence and retains its existing body.
    auto old=envelope(fixture(false));assert(write_save_game_file(path,old,error));data=bytes(path);
    QByteArray magic;{QDataStream out(&magic,QIODevice::WriteOnly);out<<quint32{0x48414249};}
    const auto at=data.lastIndexOf(magic);assert(at>0);data.truncate(at);
    {QDataStream out(&data,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{93};}
    writeBytes(path,data);assert(read_save_game_file(path,loaded,error));
    assert(!loaded.state.planets[0].habitat.population && !loaded.state.players[0].surveyKnowledge[0].planetSnapshot->habitat);
}
void partial_construction_and_queue_survive_reload()
{
    auto s=fixture(false);auto& p=s.planets[0];
    assert(apply_planetary_engineering_order(s,p,{1,PlanetaryEngineeringKind::OrbitalHabitat,EngineeringAction::StartOrResume,10}));
    (void)resolve_colony_production(s,p,true);++s.turn;
    auto save=envelope(s);QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("partial.suns");
    assert(write_save_game_file(path,save,error) && read_save_game_file(path,loaded,error));
    auto& resumed=loaded.state.planets[0];
    assert(resumed.engineering.work==80 && resumed.productionQueue.size()==1 && resumed.productionQueue[0].remainingCost==4720);
    assert(apply_planetary_engineering_order(loaded.state,resumed,{1,PlanetaryEngineeringKind::OrbitalHabitat,EngineeringAction::Pause,10}));
    assert(write_save_game_file(path,loaded,error));SaveGameData paused;assert(read_save_game_file(path,paused,error));
    assert(paused.state.planets[0].productionQueue.empty() && paused.state.planets[0].engineering.work==80);
    assert(apply_planetary_engineering_order(paused.state,paused.state.planets[0],{1,PlanetaryEngineeringKind::OrbitalHabitat,EngineeringAction::StartOrResume,10}));
    (void)resolve_colony_production(paused.state,paused.state.planets[0],true);
    assert(paused.state.planets[0].engineering.work==160 && paused.state.planets[0].engineering.invested.ironium==16);
}
void pbem_and_hidden_changes_are_private()
{
    auto s=fixture();auto& p=s.planets[0];p.habitat.population=200000;
    (void)resolve_colony_production(s,p,true);++s.turn;set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    auto host=envelope(s);host.mode=SessionMode::Host;host.playerTokens={{1,31}};
    QTemporaryDir dir;QString error;const auto path=dir.filePath("turn.sunsturn");
    assert(write_save_game_file(path,make_player_turn(host,1),error));const auto received=bytes(path);
    auto& hidden=host.state.planets[0];hidden.habitat.population=1;hidden.habitat.reserveYears=0;hidden.minerals={};hidden.industry=1;
    host.state.orbitalStations[0].modules.clear();
    assert(write_save_game_file(path,make_player_turn(host,1),error));assert(bytes(path)==received);
    TurnOrderFileData packet{17,s.turn,31,{1,{TransferCargoOrder{{1,0},{1,0,true},1000,{}},
        MoveFleetOrder{1,s.stars[0].position,1,{FleetArrivalActionKind::LoadAllAvailable,0,FleetCargoKind::OrbitalColonists}},
        SetPlanetaryEngineeringOrder{1,PlanetaryEngineeringKind::OrbitalHabitat,EngineeringAction::StartOrResume,10}}},{"Resettle","Evacuate","Build"}},loaded;
    const auto orders=dir.filePath("habitats.sunsorders");assert(write_turn_order_file(orders,packet,error));assert(read_turn_order_file(orders,loaded,error));
    assert(std::get<TransferCargoOrder>(loaded.orders.orders[0]).destination.orbital);
    assert(std::get<MoveFleetOrder>(loaded.orders.orders[1]).arrivalAction.cargo==FleetCargoKind::OrbitalColonists);
    assert(std::get<SetPlanetaryEngineeringOrder>(loaded.orders.orders[2]).kind==PlanetaryEngineeringKind::OrbitalHabitat);
}
void gui_authors_build_and_population_orders_without_mutation()
{
    MainWindow w;MainWindowTestAccess::load(w,fixture(false));w.installPlanetaryEngineering();
    auto* kind=w.findChild<QComboBox*>("engineeringKind");auto* start=w.findChild<QPushButton*>("engineeringStart");
    kind->setCurrentIndex(kind->findData(int(PlanetaryEngineeringKind::OrbitalHabitat)));assert(start->isEnabled());start->click();
    assert(std::get<SetPlanetaryEngineeringOrder>(MainWindowTestAccess::orders(w).orders[0]).kind==PlanetaryEngineeringKind::OrbitalHabitat);
    assert(MainWindowTestAccess::state(w).planets[0].engineering.kind==PlanetaryEngineeringKind::None);
    MainWindowTestAccess::load(w,fixture());
    auto* settlers=w.findChild<QSpinBox*>("habitatResettlementCount");auto* toOrbit=w.findChild<QPushButton*>("habitatToOrbit");
    settlers->setValue(200000);assert(toOrbit->isEnabled());toOrbit->click();
    assert(std::get<TransferCargoOrder>(MainWindowTestAccess::orders(w).orders[0]).destination.orbital);
    assert(MainWindowTestAccess::state(w).planets[0].habitat.population==0);
    const auto text=w.findChild<QLabel*>("engineeringStatus")->text();assert(text.contains("200000") && text.contains("Emergency reserve") && text.contains("+9000/year"));
    assert(!w.findChild<QPushButton*>("engineeringDismantle")->isEnabled());
}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);annual_receipts_and_private_evidence_roundtrip();partial_construction_and_queue_survive_reload();pbem_and_hidden_changes_are_private();
    gui_authors_build_and_population_orders_without_mutation();
}
