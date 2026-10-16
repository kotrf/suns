#include "main_window.hpp"
#include <QAction>
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPushButton>
#include <QTreeWidget>
#include <cassert>
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w) {
        w.state_=make_demo_game(); w.state_.interstellarMediumEnabled=true;
        w.state_.interstellarClouds={{1,{9000,9000},80,60,0.9}};
        w.state_.fleets[0].position={0,0}; w.state_.fleets[0].fuel=200;
        w.pendingOrders_={1,{MoveFleetOrder{1,{80,0},8}}}; w.pendingDescriptions_.clear();
        w.selection_.fleet=1; w.rebuildScene();
    }
    static void receive(MainWindow& w) {
        w.state_.players[0].mediumKnowledge={{1,{710,10},80,60,0,1,MediumKnowledgeLevel::Detected,1,1}};
        w.rebuildScene();
    }
    static void changeHidden(MainWindow& w) {
        w.state_.interstellarClouds={{1,{40,0},100,100,1},{2,{0,0},100,100,1}};
        w.rebuildScene();
    }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); suns::MainWindow w; w.installInterstellarMedium();
    suns::MainWindowTestAccess::setup(w);
    auto* tree=w.findChild<QTreeWidget*>("interstellarCloudContacts");
    auto* overlay=w.findChild<QAction*>("interstellarMediumOverlay");
    auto* locate=w.findChild<QPushButton*>("locateInterstellarCloud");
    assert(tree && overlay && locate && tree->topLevelItemCount()==0 && !locate->isEnabled());
    const auto count=[&] { int n=0; for (const auto* item:w.routeProgramScene()->items()) if (item->data(1).toInt()==7) ++n; return n; };
    assert(count()==0);
    const bool original=overlay->isChecked(); overlay->setChecked(true);
    suns::MainWindowTestAccess::receive(w); assert(tree->topLevelItemCount()==1 && count()>0);
    tree->setCurrentItem(tree->topLevelItem(0)); assert(locate->isEnabled());
    assert(tree->topLevelItem(0)->text(1)=="Detected" && tree->topLevelItem(0)->text(3).contains("1.00"));
    for (const auto* item:w.routeProgramScene()->items()) if (item->data(1).toInt()==7) {
        assert(item->sceneBoundingRect().center().x()==710 && !item->toolTip().contains("9000"));
    }
    const auto forecast=w.selectedFleetRouteProgramForecast(); const auto rows=w.selectedFleetRouteProgramRows();
    assert(forecast.contains("Medium estimate") && !rows.empty() && rows[0].fuelUsed.contains("–"));
    assert(rows[0].fuelTooltip.contains("received profiles") && rows[0].eta.startsWith("≈"));
    suns::MainWindowTestAccess::changeHidden(w);
    assert(w.selectedFleetRouteProgramForecast()==forecast);
    const auto changed=w.selectedFleetRouteProgramRows();
    assert(changed[0].eta==rows[0].eta && changed[0].fuelUsed==rows[0].fuelUsed && changed[0].fuelRemaining==rows[0].fuelRemaining);
    overlay->setChecked(false); assert(count()==0 && tree->topLevelItemCount()==1);
    overlay->setChecked(original);
}
