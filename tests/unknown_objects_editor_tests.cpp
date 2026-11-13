#include "main_window.hpp"
#include "drifting_object_fixture.hpp"
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
    static void hiddenDrift(MainWindow& w) {
        w.state_.unknownObjects.front().position={-777,777};w.state_.unknownObjects.front().sampleRecovered=true;w.rebuildScene();
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
    const auto capture=qEnvironmentVariable("SUNS_DRIFTING_OBJECT_SCREENSHOT");
    if(!capture.isEmpty()) {
        auto* dock=window.findChild<QDockWidget*>("unknownObjectsDock");assert(dock);dock->setFloating(true);dock->resize(1250,600);dock->show();
        app.processEvents();assert(dock->grab().save(capture));
    }
}
