#include "main_window.hpp"
#include "drifting_object_fixture.hpp"
#include "suns/turn_processor.hpp"
#include "suns/player_knowledge.hpp"
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QDockWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <cassert>
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w,bool received) {
        w.state_=make_demo_game(); w.state_.unknownObjects={{7,{900,900},UnknownObjectKind::CompactRemnant}};
        w.state_.nextUnknownObjectId=8; w.pendingOrders_={1,{}}; w.pendingDescriptions_.clear();
        if(received) w.state_.players[0].objectKnowledge={{7,{110,10},15,ObjectKnowledgeLevel::Detected,{},1,1}};
        w.selection_.fleet=1; w.rebuildScene();
    }
    static const MoveFleetOrder& move(const MainWindow& w) { return std::get<MoveFleetOrder>(w.pendingOrders_.orders[0]); }
    static void weakScanner(MainWindow& w) { w.state_.shipDesigns[0].components.back()=ShipComponentType::BatScanner; w.rebuildScene(); }
    static void classify(MainWindow& w,ObjectKnowledgeLevel level) {
        auto& x=w.state_.players[0].objectKnowledge[0]; x.level=level; x.kind=UnknownObjectKind::AncientProbe;
        x.position={111,7}; x.uncertainty=0; w.rebuildScene();
    }
    static void drifting(MainWindow& w) {
        w.state_=test::drifting_object_fixture();
        w.state_.players.front().objectKnowledge={test::drifting_observation(w.state_,ObjectKnowledgeLevel::Classified)};
        w.state_.turn=12;w.state_.fleets.front().telemetry.observedTurn=12;
        w.state_.unknownObjects.front().position={999,999};w.state_.unknownObjects.front().velocity={-1.5,0.5};
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.fleet=1;w.rebuildScene();
    }
    static void studyDrift(MainWindow& w) {w.state_.players.front().objectKnowledge.front().level=ObjectKnowledgeLevel::Studied;w.rebuildScene();}
    static void ambiguousRecoveryNeighbour(MainWindow& w) {
        w.state_.unknownObjects.push_back({9,{999,-999},UnknownObjectKind::DriftingDerelict,false,{1.5,0.5}});w.state_.nextUnknownObjectId=10;
        w.state_.players.front().objectKnowledge.push_back({9,{115.5,1.5},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::DriftingDerelict,12,12,Position{1.5,0.5}});
        w.rebuildScene();
    }
    static void hiddenDrift(MainWindow& w) {
        w.state_.unknownObjects.front().position={-777,777};w.state_.unknownObjects.front().sampleRecovered=true;w.rebuildScene();
    }
    static void recoveryAfterCoarseReport(MainWindow& w) {
        w.state_=test::drifting_object_fixture();
        w.state_.unknownObjects.front().velocity={1.5,0};
        w.state_.players.front().objectKnowledge={test::drifting_observation(w.state_)};
        assert(object_course_contains(w.state_.players.front().objectKnowledge.front(),{129,0}));
        auto& f=w.state_.fleets.front();
        FleetArrivalAction recovery{FleetArrivalActionKind::RecoverSample};recovery.recoveryTarget=7;
        f.destination=Position{129,0};f.warp=2;f.arrivalAction=recovery;
        f.telemetry.destination=f.destination;f.telemetry.warp=f.warp;f.telemetry.arrivalAction=recovery;
        // A real newer sensor packet retains Studied but removes the exact course.
        w.state_.unknownObjects.front().position={150,0};++w.state_.turn;
        observe_unknown_objects(w.state_,w.state_.turn);(void)deliver_due_player_reports(w.state_);
        const auto* known=known_unknown_object(w.state_,1,7);
        assert(known && known->level==ObjectKnowledgeLevel::Studied && known->uncertainty==15 && !known->velocity);
        assert(known->position!=*f.destination);
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.fleet=1;w.rebuildScene();
    }
    static GameState executePending(const MainWindow& w) {return TurnProcessor{}.process(w.state_,{w.pendingOrders_});}
    static void removeRecoveryScanner(MainWindow& w) {
        w.state_.shipDesigns.front().components={ShipComponentType::QuickJump5};w.rebuildScene();
    }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); suns::MainWindow window; window.installUnknownObjects();
    suns::MainWindowTestAccess::setup(window,false);
    auto* tree=window.findChild<QTreeWidget*>("unknownObjectContacts");
    auto* fly=window.findChild<QPushButton*>("approachUnknownObject");
    auto* study=window.findChild<QPushButton*>("studyUnknownObject");
    assert(tree && fly && study && tree->topLevelItemCount()==0 && !study->isEnabled());
    assert(!window.queueUnknownObjectApproach(7,true));
    suns::MainWindowTestAccess::setup(window,true); assert(tree->topLevelItemCount()==1);
    tree->setCurrentItem(tree->topLevelItem(0)); assert(fly->isEnabled() && study->isEnabled());
    assert(!tree->topLevelItem(0)->text(0).contains("remnant",Qt::CaseInsensitive));
    bool marker=false;
    for(const auto* item:window.routeProgramScene()->items()) if(item->data(1).toInt()==6) {
        marker=true; assert(item->pos().x()==110 && !item->toolTip().contains("900"));
    }
    assert(marker); fly->click(); assert(suns::MainWindowTestAccess::move(window).destination==suns::Position(110,10));
    study->click(); assert(suns::MainWindowTestAccess::move(window).arrivalAction.kind==suns::FleetArrivalActionKind::FieldResearch);
    suns::MainWindowTestAccess::weakScanner(window); assert(!study->isEnabled() && !window.queueUnknownObjectApproach(7,true));
    suns::MainWindowTestAccess::classify(window,suns::ObjectKnowledgeLevel::Classified);
    assert(study->isEnabled());
    assert(tree->topLevelItem(0)->text(0).contains("Ancient probe") && tree->topLevelItem(0)->text(4).contains("24"));
    suns::MainWindowTestAccess::classify(window,suns::ObjectKnowledgeLevel::Studied);
    assert(!study->isEnabled() && tree->topLevelItem(0)->text(4).contains("complete"));
    assert(!window.queueUnknownObjectApproach(7,true));
    suns::MainWindowTestAccess::drifting(window);tree->setCurrentItem(tree->topLevelItem(0));
    assert(tree->topLevelItem(0)->text(0).contains("Drifting derelict") && tree->topLevelItem(0)->text(4).contains("Construction"));
    assert(fly->text().contains("estimated intercept") && study->isEnabled());
    const auto details=tree->topLevelItem(0)->toolTip(0);
    assert(details.contains("Reported course: 1.50, 0.50") && !details.contains("999"));
    fly->click();assert(suns::MainWindowTestAccess::move(window).destination==suns::Position(115.5,1.5));
    suns::MainWindowTestAccess::hiddenDrift(window);
    assert(tree->topLevelItem(0)->toolTip(0)==details);
    for(const auto* item:window.routeProgramScene()->items())if(item->data(1).toInt()==6)
        assert(item->pos()==QPointF(111,0));
    suns::MainWindowTestAccess::studyDrift(window);tree->setCurrentItem(tree->topLevelItem(0));
    auto* recover=window.findChild<QPushButton*>("recoverScienceSample");assert(recover && recover->isEnabled());
    assert(tree->topLevelItem(0)->toolTip(0).contains("architectural core") && tree->topLevelItem(0)->toolTip(0).contains("80 Construction"));
    recover->click();assert(suns::MainWindowTestAccess::move(window).arrivalAction.kind==suns::FleetArrivalActionKind::RecoverSample);
    assert(suns::MainWindowTestAccess::move(window).destination==suns::Position(115.5,1.5));
    assert(suns::MainWindowTestAccess::move(window).arrivalAction.recoveryTarget==7);
    suns::MainWindowTestAccess::ambiguousRecoveryNeighbour(window);
    assert(window.setSelectedFleetRouteProgramLegWarp(0,6));
    assert(window.setSelectedFleetRouteProgramLegAction(0,{suns::FleetArrivalActionKind::RecoverSample}));
    assert(suns::MainWindowTestAccess::move(window).arrivalAction.recoveryTarget==7);

    assert(window.setSelectedFleetRouteProgramLegAction(0,{}));
    assert(!window.setSelectedFleetRouteProgramLegAction(0,{suns::FleetArrivalActionKind::RecoverSample}));
    suns::FleetArrivalAction selected{suns::FleetArrivalActionKind::RecoverSample};selected.recoveryTarget=7;
    assert(window.setSelectedFleetRouteProgramLegAction(0,selected));
    const auto capture=qEnvironmentVariable("SUNS_DRIFTING_OBJECT_SCREENSHOT");
    if(!capture.isEmpty()) {
        auto* dock=window.findChild<QDockWidget*>("unknownObjectsDock");assert(dock);dock->setFloating(true);dock->resize(1250,600);dock->show();
        app.processEvents();assert(dock->grab().save(capture));
    }
    suns::MainWindowTestAccess::recoveryAfterCoarseReport(window);
    assert(window.setSelectedFleetRouteProgramLegWarp(0,5));
    assert(suns::MainWindowTestAccess::move(window).arrivalAction.recoveryTarget==7);
    const auto executed=suns::MainWindowTestAccess::executePending(window);
    assert(executed.fleets.front().warp==5 && executed.fleets.front().position==suns::Position(125,0));
    assert(executed.fleets.front().arrivalAction && executed.fleets.front().arrivalAction->recoveryTarget==7);
    assert(window.setSelectedFleetRouteProgramLegAction(0,{suns::FleetArrivalActionKind::RecoverSample}));
    assert(suns::MainWindowTestAccess::move(window).arrivalAction.recoveryTarget==7);
    suns::MainWindowTestAccess::removeRecoveryScanner(window);
    assert(!window.setSelectedFleetRouteProgramLegWarp(0,6));
    assert(suns::MainWindowTestAccess::move(window).warp==5);
}
