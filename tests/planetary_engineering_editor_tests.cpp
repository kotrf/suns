#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/production.hpp"
#include "suns/player_knowledge.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cassert>
using namespace suns;
using E=PlanetaryEngineeringKind;
using A=EngineeringAction;
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w,std::uint64_t turn=1,bool research=false,std::uint8_t allocation=0) {
        w.state_=make_demo_game(); w.state_.players[0].race=race_preset(RacePreset::Terran);
        w.state_.turn=turn;
        w.state_.players[0].technology.levels.fill(14); w.state_.players[0].technology.researchActive=research;
        w.state_.players[0].technology.researchAllocationPercent=allocation;
        w.state_.planets[0].population=100000;
        w.state_.planets[0].environment={40,50,20}; w.state_.planets[0].industry=1000;
        w.state_.planets[0].minerals={1000,1000,1000};
        w.state_.orbitalStations[0].hull=OrbitalStationHullType::Starbase;
        w.pendingOrders_={1,{}}; w.pendingDescriptions_.clear(); w.selection_.star=1; w.rebuildScene();
        w.refreshProductionQueue();
    }
    static void append(MainWindow& w,Order order) {w.appendPendingOrder(std::move(order),"Research plan");w.refreshProductionQueue();}
    static void refresh(MainWindow& w) {w.refreshProductionQueue();}
    static const auto& orders(const MainWindow& w) {return w.pendingOrders_;}
    static GameState state(const MainWindow& w) {return w.state_;}
    static void load(MainWindow& w,GameState s) {w.state_=std::move(s);w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.star=1;w.rebuildScene();}
    static void foreign(MainWindow& w) {
        w.state_.players.push_back({2,"Other"}); w.state_.planets[0].owner=2;
        auto& p=w.state_.planets[0]; p.engineering={E::OrbitalShades,160,{16,8,16},8,false,2,1,0};
        set_survey_level(w.state_,1,1,SurveyLevel::OrbitalSurvey,w.state_.turn);
        w.pendingOrders_={1,{}}; w.pendingDescriptions_.clear(); w.rebuildScene();
    }
    static void hidden(MainWindow& w) {
        w.state_.planets[0].engineering={}; w.state_.planets[0].environment.temperature=99;
        w.state_.planets[0].minerals={99999,99999,99999}; w.state_.orbitalStations.clear(); w.rebuildScene();
    }
};
}
QByteArray bytes(const QString& p) {QFile f(p);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
void write(const QString& p,const QByteArray& b) {QFile f(p);assert(f.open(QIODevice::WriteOnly));assert(f.write(b)==b.size());}
QByteArray tag(quint32 value) {QByteArray b;QDataStream out(&b,QIODevice::WriteOnly);out<<value;return b;}
void check_estimate(MainWindow& w,bool progressing)
{
    MainWindowTestAccess::refresh(w); // The application's production pane normally refreshes on its timer.
    const auto* tree=w.findChild<QTreeWidget*>("productionQueueTree");
    if(!tree || tree->topLevelItemCount()!=1)qCritical()<<"Queue rows"<<(tree?tree->topLevelItemCount():-1)
        <<"orders"<<MainWindowTestAccess::orders(w).orders.size()<<w.findChild<QLabel*>("engineeringStatus")->text();
    assert(tree && tree->topLevelItemCount()==1);
    const auto text=w.findChild<QLabel*>("engineeringStatus")->text();
    if(progressing) {
        assert(text.contains("Estimated completion: year 40"));
        assert(tree->topLevelItem(0)->text(3)=="Turn 40 (+30)");
    } else {
        assert(text.contains("No completion within the 128-year forecast"));
        assert(tree->topLevelItem(0)->text(3)=="beyond forecast");
    }
    const auto s=MainWindowTestAccess::state(w); const auto orders=MainWindowTestAccess::orders(w);
    const auto resolved=TurnProcessor{}.process(s,{orders});
    assert(s.planets[0].engineering.kind==E::None); // Planning did not spend a year or mutate physical work.
    assert(resolved.planets[0].engineering.work==(progressing?80U:0U));
}
void newly_planned_project_uses_projected_planet(MainWindow& w)
{
    MainWindowTestAccess::setup(w,10);
    w.findChild<QPushButton*>("engineeringStart")->click(); check_estimate(w,true);
}
void research_orders_reach_both_forecasts(MainWindow& w)
{
    for(const bool initiallyBlocked:{false,true}) {
        MainWindowTestAccess::setup(w,10,true,initiallyBlocked?100:0);
        w.findChild<QPushButton*>("engineeringStart")->click();
        MainWindowTestAccess::append(w,SetResearchAllocationOrder{std::uint8_t(initiallyBlocked?0:100)});
        check_estimate(w,initiallyBlocked);
        assert(MainWindowTestAccess::state(w).players[0].technology.researchAllocationPercent==(initiallyBlocked?100:0));
    }
    MainWindowTestAccess::setup(w,10,false,100);
    w.findChild<QPushButton*>("engineeringStart")->click();
    MainWindowTestAccess::append(w,SetResearchPlanOrder{ResearchField::Construction,{},true}); check_estimate(w,false);
    MainWindowTestAccess::append(w,SetResearchPlanOrder{ResearchField::Construction,{},false}); check_estimate(w,true);
    // Invalid research orders must be rejected just as they are by End Turn.
    MainWindowTestAccess::append(w,SetResearchAllocationOrder{101});
    MainWindowTestAccess::append(w,SetResearchPlanOrder{ResearchField::Construction,{ResearchField::Energy},false});
    check_estimate(w,true);
    const auto projected=planned_production_state(MainWindowTestAccess::state(w),MainWindowTestAccess::orders(w));
    assert(projected.players[0].technology.researchAllocationPercent==100);
    assert(!projected.players[0].technology.researchActive && projected.players[0].technology.queuedFocuses.empty());
}
void persistence(GameState s)
{
    QTemporaryDir dir; assert(dir.isValid()); QString error;
    assert(apply_planetary_engineering_order(s,s.planets[0],{1,E::OrbitalMirrors,A::StartOrResume,7}));
    (void)resolve_colony_production(s,s.planets[0],true); ++s.turn;
    s.players.push_back({2,"Observer"}); s.players[1].race=race_preset(RacePreset::Terran);
    set_survey_level(s,2,1,SurveyLevel::OrbitalSurvey,s.turn);
    PendingSurveyReport survey; survey.star=1; survey.observedTurn=s.turn; survey.deliveryTurn=s.turn+5;
    survey.level=SurveyLevel::OrbitalSurvey; survey.observedOwner=1;
    survey.planetSnapshot=capture_planet_survey(s,2,1,survey.level,s.turn); s.players[1].pendingSurveyReports.push_back(survey);
    SaveGameData host; host.state=s; host.mode=SessionMode::Host; host.campaignId=9; host.turnToken=8; host.playerTokens={{1,77},{2,88}};
    host.pendingOrders={1,{SetPlanetaryEngineeringOrder{1,E::OrbitalMirrors,A::Pause,7},SetPlanetaryEngineeringOrder{1,E::OrbitalMirrors,A::SetOutput,5}}};
    host.pendingDescriptions={"Pause","Output"};
    GameEvent event; event.kind=GameEventKind::EngineeringPaused; event.recipient=1; event.turn=s.turn;
    event.observedTurn=s.turn; event.star=1; event.planet=1; event.productionKind=ProductionKind::OrbitalMirrors;
    event.quantity=std::uint32_t(EngineeringBlock::MissingStation); host.strategicMessages.push_back(event);
    const auto path=dir.filePath("engineering.suns"); SaveGameData loaded;
    if(!write_save_game_file(path,host,error)) {qCritical()<<error;assert(false);}
    if(!read_save_game_file(path,loaded,error)) {qCritical()<<error;assert(false);}
    const auto& p=loaded.state.planets[0]; assert(p.engineering.work==80 && p.engineering.strength==7 && p.engineering.invested.ironium==8);
    assert(p.productionQueue[0].remainingCost==2320 && p.engineering.builder==1 && p.engineering.startedTurn==1);
    assert(loaded.strategicMessages.back().kind==GameEventKind::EngineeringPaused);
    const auto& snapshot=*loaded.state.players[1].surveyKnowledge[0].planetSnapshot;
    assert(snapshot.engineering.work==80 && snapshot.engineering.invested.ironium==0);
    assert(loaded.state.players[1].pendingSurveyReports[0].planetSnapshot->engineering.strength==7);
    TurnOrderFileData packet{9,s.turn,77,host.pendingOrders,host.pendingDescriptions},imported;
    const auto orders=dir.filePath("engineering.sunsorders"); assert(write_turn_order_file(orders,packet,error) && read_turn_order_file(orders,imported,error));
    assert(std::get<SetPlanetaryEngineeringOrder>(imported.orders.orders[0]).action==A::Pause);
    assert(std::get<SetPlanetaryEngineeringOrder>(imported.orders.orders[1]).strength==5);
    const auto a=dir.filePath("a.sunsturn"), b=dir.filePath("b.sunsturn");
    assert(write_save_game_file(a,make_player_turn(host,2),error));
    host.state.planets[0].engineering={}; host.state.planets[0].productionQueue.clear(); host.state.planets[0].environment.temperature=99;
    host.state.orbitalStations.clear(); assert(write_save_game_file(b,make_player_turn(host,2),error));
    assert(bytes(a)==bytes(b));
    // Reject invalid progress and oversized extension lengths.
    auto corrupt=bytes(path); const auto offset=corrupt.indexOf(tag(0x454e4749)); assert(offset>=0);
    {QDataStream patch(&corrupt,QIODevice::ReadWrite);assert(patch.device()->seek(offset+17));patch<<quint32{2401};}
    const auto bad=dir.filePath("bad.suns"); write(bad,corrupt); assert(!read_save_game_file(bad,loaded,error));
    corrupt=bytes(path); {QDataStream patch(&corrupt,QIODevice::ReadWrite);assert(patch.device()->seek(offset+4));patch<<quint32{0x7fffffff};}
    write(bad,corrupt); assert(!read_save_game_file(bad,loaded,error));
    // v84 files have no ENGI tail and migrate to empty project state.
    SaveGameData old; old.state=make_demo_game(); old.pendingOrders={1,{}}; old.campaignId=9; old.turnToken=8;
    assert(write_save_game_file(path,old,error)); auto legacy=bytes(path);
    const auto tail=legacy.indexOf(tag(0x454e4749)), strategic=legacy.indexOf(tag(0x53545241)); assert(tail>=0 && strategic>=0);
    quint32 tailSize{},size{}; QDataStream th(legacy.mid(tail+4));th>>tailSize;QDataStream sh(legacy.mid(strategic+4));sh>>size;
    legacy.remove(tail,8+tailSize);
    {QDataStream patch(&legacy,QIODevice::ReadWrite);assert(patch.device()->seek(4));patch<<quint32{84};assert(patch.device()->seek(strategic+4));patch<<quint32(size-8-tailSize);}
    write(path,legacy); assert(read_save_game_file(path,loaded,error)); assert(loaded.state.planets[0].engineering.kind==E::None);
}
void magnetic_ui_persistence_and_private_evidence(MainWindow& w)
{
    MainWindowTestAccess::setup(w,10);
    auto s=MainWindowTestAccess::state(w);s.players[0].technology.levels.fill(18);
    s.unknownObjects={{7,{411,7},UnknownObjectKind::CompactRemnant}};s.nextUnknownObjectId=8;
    s.planets[0].environment.radiation=70;s.planets[0].naturalEnvironment=s.planets[0].environment;
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    MainWindowTestAccess::load(w,s);
    auto* kind=w.findChild<QComboBox*>("engineeringKind");auto* output=w.findChild<QSpinBox*>("engineeringOutput");
    auto* start=w.findChild<QPushButton*>("engineeringStart");auto* status=w.findChild<QLabel*>("engineeringStatus");
    kind->setCurrentIndex(kind->findData(int(E::AdaptiveMagnetosphere)));
    assert(!start->isEnabled() && status->text().contains("delivered completed Compact remnant"));
    s.players[0].objectKnowledge={{7,{411,7},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::CompactRemnant,9,10}};
    MainWindowTestAccess::load(w,s);kind->setCurrentIndex(kind->findData(int(E::AdaptiveMagnetosphere)));
    assert(start->isEnabled() && output->maximum()==20);output->setValue(20);
    assert(w.findChild<QLabel*>("engineeringEffect")->text().contains("Radiation now: 70 / 100. With that output: 50 / 100."));
    start->click();
    const auto& order=std::get<SetPlanetaryEngineeringOrder>(MainWindowTestAccess::orders(w).orders.back());
    assert(order.kind==E::AdaptiveMagnetosphere && order.strength==20);
    assert(status->text().contains("Estimated completion: year 70"));
    assert(apply_planetary_engineering_order(s,s.planets[0],order));
    for(unsigned i=0;i<38;++i) {(void)resolve_colony_production(s,s.planets[0],true);++s.turn;}
    assert(s.planets[0].engineering.work==3040);set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    queue_survey_report(s,1,1,0,s.stars[0].position,s.turn,SurveyLevel::DeepSurvey,true);
    SaveGameData save;save.state=s;save.mode=SessionMode::Host;save.campaignId=9;save.turnToken=8;save.playerTokens={{1,77}};
    save.pendingOrders={1,{SetPlanetaryEngineeringOrder{1,E::AdaptiveMagnetosphere,A::SetOutput,20}}};save.pendingDescriptions={"Magnetosphere output"};
    QTemporaryDir dir;QString error;SaveGameData loaded;
    const auto path=dir.filePath("magnetic.suns");assert(write_save_game_file(path,save,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.planets[0].engineering.work==3040 && loaded.state.players[0].surveyKnowledge[0].planetSnapshot->engineeringQueueIndex);
    assert(loaded.state.players[0].pendingSurveyReports.back().planetSnapshot->engineeringQueueIndex);
    TurnOrderFileData packet{9,s.turn,77,save.pendingOrders,save.pendingDescriptions},imported;
    const auto orders=dir.filePath("magnetic.sunsorders");assert(write_turn_order_file(orders,packet,error) && read_turn_order_file(orders,imported,error));
    assert(std::get<SetPlanetaryEngineeringOrder>(imported.orders.orders[0]).strength==20);
    const auto playerPath=dir.filePath("magnetic.sunsturn");assert(write_save_game_file(playerPath,make_player_turn(save,1),error));
    const auto frozen=bytes(playerPath);MainWindowTestAccess::load(w,s);const auto oldStatus=status->text();
    save.state.planets[0].engineering.work=4800;save.state.planets[0].engineering.invested={480,240,480};
    save.state.planets[0].engineering.completedTurn=save.state.turn;save.state.planets[0].engineering.operational=true;save.state.planets[0].productionQueue.clear();
    assert(write_save_game_file(playerPath,make_player_turn(save,1),error) && bytes(playerPath)==frozen);
    MainWindowTestAccess::load(w,save.state);assert(status->text()==oldStatus);
    auto corrupt=bytes(path);const auto offset=corrupt.lastIndexOf(tag(0x45514958));assert(offset>0);
    const auto bad=dir.filePath("bad-magnetic.suns");
    write(bad,corrupt.left(corrupt.size()-1));assert(!read_save_game_file(bad,loaded,error));
    corrupt[offset+8+4+4+4+4]=char(2);write(bad,corrupt);assert(!read_save_game_file(bad,loaded,error));
    // Old v91 saves have no EQIX and retain their physical work without inventing a queue report.
    SaveGameData old;old.state=make_demo_game();old.pendingOrders={1,{}};old.campaignId=9;old.turnToken=8;
    old.state.turn=2;old.state.planets[0].engineering={E::OrbitalMirrors,80,{8,4,8},7,false,1,1,0};
    old.state.planets[0].productionQueue={{ProductionKind::OrbitalMirrors,2320}};
    set_survey_level(old.state,1,1,SurveyLevel::DeepSurvey,old.state.turn);
    assert(write_save_game_file(path,old,error));auto legacy=bytes(path);const auto legacyTail=legacy.lastIndexOf(tag(0x45514958));
    legacy.truncate(legacyTail);{QDataStream patch(&legacy,QIODevice::ReadWrite);patch.device()->seek(4);patch<<quint32{91};}
    write(path,legacy);assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.planets[0].engineering.work==80 && !loaded.state.players[0].surveyKnowledge[0].planetSnapshot->engineeringQueueIndex);
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); MainWindow w;
    if(argc>1) {
        w.installUiPolish(); w.installProductionQueue(); w.installPlanetaryEngineering();
        if(QString::fromLocal8Bit(argv[1])=="--start-forecast")newly_planned_project_uses_projected_planet(w);
        else if(QString::fromLocal8Bit(argv[1])=="--research-forecast")research_orders_reach_both_forecasts(w);
        else assert(false);
        return 0;
    }
    w.installPlanetaryEngineering();
    MainWindowTestAccess::setup(w);
    const auto state=MainWindowTestAccess::state(w);
    auto* start=w.findChild<QPushButton*>("engineeringStart"); auto* pause=w.findChild<QPushButton*>("engineeringPause");
    auto* status=w.findChild<QLabel*>("engineeringStatus"); auto* output=w.findChild<QSpinBox*>("engineeringOutput");
    assert(start && start->isEnabled()); output->setValue(7); start->click();
    const auto& orders=MainWindowTestAccess::orders(w); assert(orders.orders.size()==1);
    const auto& order=std::get<SetPlanetaryEngineeringOrder>(orders.orders[0]); assert(order.colony==1 && order.strength==7);
    assert(MainWindowTestAccess::state(w).planets[0].engineering.kind==E::None);
    assert(!start->isEnabled() && pause->isEnabled()); pause->click(); assert(start->isEnabled()); start->click();
    output->setValue(5); w.findChild<QPushButton*>("engineeringApply")->click();
    assert(std::get<SetPlanetaryEngineeringOrder>(MainWindowTestAccess::orders(w).orders.back()).strength==5);
    MainWindowTestAccess::foreign(w); assert(!start->isEnabled() && !output->isEnabled());
    const auto text=status->text(), effect=w.findChild<QLabel*>("engineeringEffect")->text();
    MainWindowTestAccess::hidden(w); assert(status->text()==text && w.findChild<QLabel*>("engineeringEffect")->text()==effect);
    persistence(state);
    magnetic_ui_persistence_and_private_evidence(w);
}
