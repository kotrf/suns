#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "suns/deep_space.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPushButton>
#include <QTreeWidget>
#include <cassert>
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w) {
        w.state_=make_demo_game(); w.state_.stars.resize(1); w.state_.planets.resize(1);
        w.state_.players[0].technology.levels.fill(26); w.state_.players[0].technology.researchAllocationPercent=0;
        w.state_.wormholeRules.spawnChancePerTurn=0; w.state_.fleets.resize(1);
        w.state_.shipDesigns.push_back({100,1,"Depot constructor",ShipHullType::Scout,
            {ShipComponentType::FusionDrive,ShipComponentType::FuelDepotKit,ShipComponentType::LongRangeScanner}});
        w.state_.nextShipDesignId=101;
        auto& f=w.state_.fleets[0]; f.position={50,0}; f.design=100; f.ships={{100,1}}; f.fuel=500;
        f.destination.reset(); f.arrivalAction.reset(); f.waypointQueue.clear(); f.repeatOrders=false;
        f.telemetry={}; f.telemetry.observedTurn=w.state_.turn; f.telemetry.position=f.position;
        f.telemetry.fuel=f.fuel; f.telemetry.ships=f.ships;
        auto site=f; site.id=2; site.name="Forward depot"; site.position={60,0}; site.outpost=OutpostKind::FuelDepot;
        site.fuel=40; site.telemetry.position=site.position; site.telemetry.outpost=site.outpost; site.telemetry.fuel=120;
        w.state_.fleets.push_back(site); w.state_.nextFleetId=3;
        w.pendingOrders_={1,{}}; w.pendingDescriptions_.clear(); w.selection_.fleet=1; w.rebuildScene();
    }
    static void hidden(MainWindow& w) { w.state_.fleets[1].fuel=2; w.state_.fleets[1].damagePercent=90; w.rebuildScene(); }
    static const auto& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
    static void clear(MainWindow& w) { w.pendingOrders_.orders.clear(); w.pendingDescriptions_.clear(); w.rebuildScene(); }
    static bool unchanged(const MainWindow& w) { return w.state_.fleets[0].outpost==OutpostKind::None; }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); using namespace suns; MainWindow w; w.installOutposts(); attachRouteProgramDock(w);
    MainWindowTestAccess::setup(w);
    auto* tree=w.findChild<QTreeWidget*>("outpostContacts"); auto* overlay=w.findChild<QAction*>("outpostsOverlay");
    auto* supply=w.findChild<QPushButton*>("supplyOutpost"); auto* refuel=w.findChild<QPushButton*>("refuelAtOutpost");
    assert(tree && overlay && supply && tree->topLevelItemCount()==1);
    assert(tree->topLevelItem(0)->text(3).startsWith("120.0") && tree->topLevelItem(0)->text(4)=="0%");
    const bool original=overlay->isChecked(); overlay->setChecked(true);
    const auto markers=[&] { int n=0; for (const auto* i:w.routeProgramScene()->items()) if (i->data(9).toBool()) ++n; return n; };
    assert(markers()==1); overlay->setChecked(false); assert(markers()==0); overlay->setChecked(true);
    tree->setCurrentItem(tree->topLevelItem(0)); assert(supply->isEnabled() && refuel->isEnabled());
    w.findChild<QDoubleSpinBox*>("outpostTankerReserve")->setValue(75.25); supply->click();
    const auto& move=std::get<MoveFleetOrder>(MainWindowTestAccess::orders(w).back());
    assert(move.arrivalAction.kind==FleetArrivalActionKind::SupplyOutpost && move.arrivalAction.reserveFuel==75.25);
    assert(same_position(move.destination,Position{60,0}) && move.targetFleet==0);
    const auto forecast=w.selectedFleetRouteProgramForecast();
    MainWindowTestAccess::hidden(w);
    assert(tree->topLevelItem(0)->text(3).startsWith("120.0") && tree->topLevelItem(0)->text(4)=="0%");
    assert(w.selectedFleetRouteProgramForecast()==forecast);
    // The route editor preserves and changes fractional tanker reserves.
    auto* routes=w.findChild<QTreeWidget*>("routeProgramQueue");
    assert(routes && routes->topLevelItemCount()==1); routes->setCurrentItem(routes->topLevelItem(0));
    auto* fuel=w.findChild<QDoubleSpinBox*>("routeFuelReserveSpin"); assert(fuel && fuel->value()==75.25 && fuel->isEnabled());
    fuel->setValue(92.5); assert(w.selectedFleetRouteProgramLegAction(0)->reserveFuel==92.5);
    MainWindowTestAccess::clear(w);
    w.findChild<QDoubleSpinBox*>("outpostBuildX")->setValue(250); w.findChild<QDoubleSpinBox*>("outpostBuildY")->setValue(80);
    auto* build=w.findChild<QPushButton*>("buildOutpostAtCoordinates"); assert(build->isEnabled()); build->click();
    const auto& construction=std::get<MoveFleetOrder>(MainWindowTestAccess::orders(w).back());
    assert(construction.arrivalAction.kind==FleetArrivalActionKind::DeployOutpost && same_position(construction.destination,Position{250,80}));
    assert(MainWindowTestAccess::unchanged(w)); MainWindowTestAccess::clear(w);
    auto* deploy=w.findChild<QPushButton*>("deployOutpostHere"); assert(deploy->isEnabled()); deploy->click();
    assert(std::get<SetFleetTaskOrder>(MainWindowTestAccess::orders(w).back()).task==FleetTask::DeployOutpost);
    assert(MainWindowTestAccess::unchanged(w)); overlay->setChecked(original);
}
