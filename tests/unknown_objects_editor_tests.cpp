#include "main_window.hpp"
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsItem>
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
    static void classify(MainWindow& w,ObjectKnowledgeLevel level) {
        auto& x=w.state_.players[0].objectKnowledge[0]; x.level=level; x.kind=UnknownObjectKind::AncientProbe;
        x.position={111,7}; x.uncertainty=0; w.rebuildScene();
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
    suns::MainWindowTestAccess::classify(window,suns::ObjectKnowledgeLevel::Classified);
    assert(tree->topLevelItem(0)->text(0).contains("Ancient probe") && tree->topLevelItem(0)->text(4).contains("24"));
    suns::MainWindowTestAccess::classify(window,suns::ObjectKnowledgeLevel::Studied);
    assert(!study->isEnabled() && tree->topLevelItem(0)->text(4).contains("complete"));
    assert(!window.queueUnknownObjectApproach(7,true));
}
