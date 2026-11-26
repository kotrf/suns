#include "main_window.hpp"
#include "save_game.hpp"
#include "rogue_planetoid_fixture.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include <QApplication>
#include <QDataStream>
#include <QDockWidget>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cassert>
#include <limits>
using namespace suns;
namespace {
SaveGameData host()
{
    SaveGameData h;h.state=test::rogue_planetoid_fixture();h.mode=SessionMode::Host;
    h.campaignId=55;h.turnToken=17;h.playerTokens={{1,17}};h.pendingOrders={1,{}};return h;
}
QByteArray bytes(const QString& path)
{ QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll(); }
void raw(const QString& path,const QByteArray& data)
{ QFile f(path);assert(f.open(QIODevice::WriteOnly) && f.write(data)==data.size()); }
void observations_remain_frozen_through_host_and_player_files()
{
    auto h=host();auto& s=h.state;
    auto previous=test::drifting_observation(s,ObjectKnowledgeLevel::Classified);
    previous.position={401,0.25};previous.observedTurn=previous.receivedTurn=9;
    s.players.front().objectKnowledge={previous};
    auto& body=s.unknownObjects.front();body.position={401.5,0.5};
    auto& fleet=s.fleets.front();fleet.position={390,0};fleet.telemetry.position=fleet.position;fleet.telemetry.observedTurn=9;
    fleet.task=FleetTask::FieldResearch;research_unknown_objects(s,fleet,s.turn);publish_fleet_telemetry(s,s.turn);
    const auto report=s.players.front().pendingPlayerReports.front();assert(report.deliveryTurn>s.turn && report.deliveryTurn!=kAwaitingTransmission);
    QTemporaryDir dir;QString error;SaveGameData loaded;
    const auto path=dir.filePath("host.suns"),a=dir.filePath("a.sunsturn"),b=dir.filePath("b.sunsturn");
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.unknownObjects.front().kind==UnknownObjectKind::RoguePlanetoid);
    assert(loaded.state.unknownObjects.front().velocity==Position(0.5,0.25));
    assert(loaded.state.players.front().pendingPlayerReports.front().objectObservation==report.objectObservation);
    assert(write_save_game_file(a,make_player_turn(h,1),error));
    body.position={999,777};body.velocity={-0.5,-0.25};body.sampleRecovered=true;
    assert(write_save_game_file(b,make_player_turn(h,1),error) && bytes(a)==bytes(b));
    s.turn=report.deliveryTurn;h.strategicMessages=deliver_due_player_reports(s);
    const auto* known=known_unknown_object(s,1,7);
    assert(known && known->level==ObjectKnowledgeLevel::Studied && known->position==report.objectObservation->position);
    assert(known->velocity==Position(0.5,0.25) && known->position!=body.position);
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    assert(!loaded.strategicMessages.empty() && loaded.strategicMessages.front().objectObservation->kind==UnknownObjectKind::RoguePlanetoid);
    assert(write_save_game_file(b,make_player_turn(h,1),error) && read_save_game_file(b,loaded,error));
    assert(loaded.state.unknownObjects.empty() && known_unknown_object(loaded.state,1,7)->velocity==Position(0.5,0.25));
}
void version130_ranges_keep_old_layouts_and_reject_invalid_new_motion()
{
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("version.suns");
    auto h=host();assert(write_save_game_file(path,h,error));auto current=bytes(path);
    { QDataStream in(current);quint32 magic{},version{};in>>magic>>version;assert(version==137); }
    auto prior=current;
    { QDataStream patch(&prior,QIODevice::ReadWrite);assert(patch.device()->seek(4));patch<<quint32{130}; }
    raw(path,prior);assert(read_save_game_file(path,loaded,error) && loaded.state.unknownObjects.front().kind==UnknownObjectKind::RoguePlanetoid);
    { QDataStream old(&current,QIODevice::ReadWrite);assert(old.device()->seek(4));old<<quint32{129}; }
    raw(path,current);assert(!read_save_game_file(path,loaded,error)); // New kind is outside v129's range.
    // Synthetic v129 record layout: the old drifting/static fields are unchanged.
    h.state=test::drifting_object_fixture();assert(write_save_game_file(path,h,error));auto old=bytes(path);
    { QDataStream patch(&old,QIODevice::ReadWrite);assert(patch.device()->seek(4));patch<<quint32{129}; }
    raw(path,old);assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.unknownObjects.size()==1 && loaded.state.unknownObjects.front().kind==UnknownObjectKind::DriftingDerelict);
    h=host();h.state.unknownObjects.front().velocity={};assert(!write_save_game_file(path,h,error));
    h=host();h.state.unknownObjects.front().velocity.x=std::numeric_limits<double>::infinity();assert(!write_save_game_file(path,h,error));
    h=host();auto x=test::drifting_observation(h.state);x.velocity.reset();h.state.players.front().objectKnowledge={x};
    assert(!write_save_game_file(path,h,error));
}
}
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w,bool studied,bool hidden)
    {
        w.state_=test::rogue_planetoid_fixture();
        w.state_.players.front().objectKnowledge={test::drifting_observation(w.state_,studied?ObjectKnowledgeLevel::Studied:ObjectKnowledgeLevel::Classified)};
        if(hidden) {
            w.state_.unknownObjects.front().position={999,999};w.state_.unknownObjects.front().velocity={-0.5,-0.25};
            w.state_.unknownObjects.front().sampleRecovered=true;
        }
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.fleet=1;w.selection_.star.reset();w.rebuildScene();
    }
    static void journal_authors_received_intercepts_and_real_pbem_recovery(MainWindow& w)
    {
        setup(w,false,true);auto* tree=w.findChild<QTreeWidget*>("unknownObjectContacts");assert(tree && tree->topLevelItemCount()==1);
        tree->setCurrentItem(tree->topLevelItem(0));
        const auto details=tree->topLevelItem(0)->toolTip(0);
        assert(tree->topLevelItem(0)->text(0).contains("Rogue planetoid") && tree->topLevelItem(0)->text(4).contains("Biology"));
        assert(details.contains("Reported course: 0.50, 0.25") && !details.contains("999") && details.contains("32"));
        auto* fly=w.findChild<QPushButton*>("approachUnknownObject");auto* recover=w.findChild<QPushButton*>("recoverScienceSample");
        assert(fly && recover && fly->text().contains("estimated intercept") && !recover->isEnabled());
        fly->click();const auto move=std::get<MoveFleetOrder>(w.pendingOrders_.orders.front());
        assert(move.destination==Position(111.5,0.25));
        setup(w,true,true);tree->setCurrentItem(tree->topLevelItem(0));
        assert(recover->isEnabled() && tree->topLevelItem(0)->toolTip(0).contains("prebiotic ice"));
        assert(tree->topLevelItem(0)->toolTip(0).contains("80 Biology"));
        const auto capture=qEnvironmentVariable("SUNS_ROGUE_OBJECT_SCREENSHOT");
        if(!capture.isEmpty()) {
            auto* dock=w.findChild<QDockWidget*>("unknownObjectsDock");assert(dock);dock->setFloating(true);dock->resize(1250,600);dock->show();
            QApplication::processEvents();assert(dock->grab().save(capture));
        }
        setup(w,true,false);tree->setCurrentItem(tree->topLevelItem(0));recover->click();
        const auto& recovery=std::get<MoveFleetOrder>(w.pendingOrders_.orders.front());
        assert(recovery.arrivalAction.kind==FleetArrivalActionKind::RecoverSample && recovery.arrivalAction.recoveryTarget==7);
        QTemporaryDir dir;QString error;const auto path=dir.filePath("planetoid.sunsorders");
        TurnOrderFileData packet{55,w.state_.turn,17,w.pendingOrders_,w.pendingDescriptions_},decoded;
        assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,decoded,error));
        auto h=host();h.state=TurnProcessor{}.process(w.state_,{decoded.orders});
        assert(h.state.fleets.front().position==h.state.unknownObjects.front().position && h.state.fleets.front().fuel<200);
        assert(h.state.unknownObjects.front().sampleRecovered && h.state.fleets.front().scienceSamples.front().kind==UnknownObjectKind::RoguePlanetoid);
        const auto saved=dir.filePath("recovered.suns"),player=dir.filePath("recovered.sunsturn");SaveGameData loaded;
        assert(write_save_game_file(saved,h,error) && read_save_game_file(saved,loaded,error));
        assert(loaded.state.fleets.front().scienceSamples==h.state.fleets.front().scienceSamples);
        ++h.state.turn;auto& fleet=h.state.fleets.front();fleet.position={390,0};publish_fleet_telemetry(h.state,h.state.turn);
        assert(!fleet.telemetryInTransit.empty() && fleet.telemetryInTransit.back().telemetry.scienceSamples.front().kind==UnknownObjectKind::RoguePlanetoid);
        assert(write_save_game_file(saved,h,error) && read_save_game_file(saved,loaded,error));
        assert(loaded.state.fleets.front().telemetryInTransit.back().telemetry.scienceSamples==fleet.scienceSamples);
        assert(write_save_game_file(player,make_player_turn(h,1),error));const auto before=bytes(player);
        fleet.scienceSamples.clear();h.state.unknownObjects.front().position={777,777};
        assert(write_save_game_file(player,make_player_turn(h,1),error) && bytes(player)==before);
    }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    observations_remain_frozen_through_host_and_player_files();
    version130_ranges_keep_old_layouts_and_reject_invalid_new_motion();
    MainWindow window;window.installUnknownObjects();
    MainWindowTestAccess::journal_authors_received_intercepts_and_real_pbem_recovery(window);
}
