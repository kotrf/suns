#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/production.hpp"
#include "suns/communications.hpp"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>
using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w) {
        w.state_=make_demo_game();w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();
        w.selection_.star=1;w.selection_.fleet=1;
        w.rebuildScene();emit w.routeProgramContextChanged(true);
    }
    static Position click(MainWindow& w,Position position,Qt::MouseButton button=Qt::LeftButton) {
        w.view_->setTransform(QTransform::fromScale(1.5,1.5));w.view_->centerOn(position.x,position.y);
        QApplication::processEvents();const auto point=w.view_->mapFromScene(position.x,position.y);
        const auto picked=w.view_->mapToScene(point);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),QPointF(w.view_->viewport()->mapToGlobal(point)),button,button,Qt::NoModifier);
        assert(w.eventFilter(w.view_->viewport(),&press));
        QApplication::processEvents();return {picked.x(),picked.y()};
    }
    static void navigation(MainWindow& w) {
        load(w);auto* add=w.findChild<QPushButton*>("routeNewWaypointButton");assert(add);
        add->click();assert(w.routeProgramMapTargetPickActive());click(w,w.state_.stars[1].position);
        assert(w.selectedFleetRouteProgramRows().size()==1 && !w.routeProgramMapTargetPickActive());
        add->click();const auto coordinate=click(w,{52,67});
        assert(w.selectedFleetRouteProgramRows().size()==2);
        add->click();click(w,w.state_.stars[2].position,Qt::RightButton);
        assert(w.selectedFleetRouteProgramRows().size()==3); // A right-click adds exactly once.
        const auto& order=std::get<MoveFleetOrder>(w.pendingOrders_.orders.back());
        assert(same_position(order.destination,w.state_.stars[1].position));
        assert(order.queuedWaypoints.size()==2 && same_position(order.queuedWaypoints[0].destination,coordinate));
        add->click();w.cancelRouteProgramMapTargetPick();click(w,w.state_.stars[3].position);
        assert(w.selectedFleetRouteProgramRows().size()==3);
        QTemporaryDir dir;QString error;TurnOrderFileData packet{1,w.state_.turn,1,w.pendingOrders_,w.pendingDescriptions_},decoded;
        assert(write_turn_order_file(dir.filePath("route.sunsorders"),packet,error));
        assert(read_turn_order_file(dir.filePath("route.sunsorders"),decoded,error));
        const auto actual=TurnProcessor{}.process(w.state_,{decoded.orders});
        const auto control=TurnProcessor{}.process(w.state_,{w.pendingOrders_});
        assert(same_position(actual.fleets[0].position,control.fleets[0].position));
        assert(actual.fleets[0].waypointQueue.size()==control.fleets[0].waypointQueue.size());
        load(w);auto* pick=w.findChild<QPushButton*>("routePickTargetButton");assert(pick);
        pick->click();click(w,w.state_.stars[1].position);
        assert(w.pendingOrders_.orders.empty() && w.selectedFleetRouteProgramRows().empty()); // Legacy draft picking.
    }
    static void queueBudget(MainWindow& w) {
        load(w);w.state_.players[0].technology.levels.fill(26);w.state_.players[0].technology.researchActive=false;
        auto& p=w.state_.planets[0];p.population=333*kPopulationPerProductionUnit;p.industry=0;p.mines=0;
        const ProductionItem batch{ProductionKind::AdvancedMine,production_cost(ProductionKind::AdvancedMine),0,ProductionAutomation::None,30};
        p.productionQueue={batch,batch};const auto unit=production_item_mineral_cost(w.state_,batch);
        const auto refresh=[&](double units){p.minerals={unit.ironium*units,unit.boranium*units,unit.germanium*units};set_survey_level(w.state_,1,1,SurveyLevel::DeepSurvey,w.state_.turn);w.rebuildScene();w.refreshProductionQueue();};
        refresh(60);assert(w.productionQueueTree_->topLevelItemCount()==2);
        auto* second=w.productionQueueTree_->topLevelItem(1);
        assert(second->text(3).contains("(+2)") && second->toolTip(1).contains("Whole batch (30 units)"));
        refresh(45);second=w.productionQueueTree_->topLevelItem(1);
        assert(second->text(3)==QString::fromUtf8("—"));
        assert(second->toolTip(1).contains("Available after earlier queue items"));
        const auto missing=QString("Shortfall for this batch: I %1 / B %2 / G %3 kt")
            .arg(unit.ironium*15,0,'f',1).arg(unit.boranium*15,0,'f',1).arg(unit.germanium*15,0,'f',1);
        assert(second->toolTip(1).contains(missing));
    }
    static void radiation(MainWindow& w) {
        load(w);auto& design=w.state_.shipDesigns[1];
        for(auto& component:design.components)if(component==ShipComponentType::FusionDrive)component=ShipComponentType::RadiatingRamScoopDrive;
        design.placements=autoplace_ship_components(design.hull,design.components);assert(ship_design_valid(design));
        auto& fleet=w.state_.fleets[0];fleet.design=design.id;fleet.ships={{design.id,1}};
        auto& race=w.state_.players[0].race;race.radiationTolerance=0;race.radiationImmune=false;
        assert(fleet_radiation_hazard(w.state_,fleet)>0 && !fleet_radiation_safe(w.state_,fleet));
        const auto refresh=[&](std::uint64_t colonists){fleet.colonists=colonists;fleet.telemetry={};fleet.telemetry=confirmed_fleet_telemetry(w.state_,fleet);w.rebuildScene();};
        refresh(0);assert(!w.selectedFleetPanelSummary().contains("projected loss"));
        refresh(1500);assert(w.selectedFleetPanelSummary().contains("projected loss"));
    }
    static void split(MainWindow& w) {
        load(w);w.state_.shipDesigns[0].name="Scout";auto& fleet=w.state_.fleets[0];fleet.ships={{fleet.design,2}};
        fleet.telemetry={};fleet.telemetry=confirmed_fleet_telemetry(w.state_,fleet);w.rebuildScene();
        QTimer::singleShot(0,&w,[]{auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget());assert(d);auto* amount=d->findChild<QSpinBox*>();assert(amount);amount->setValue(1);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
        w.openSplitFleetDialog();assert(w.pendingOrders_.orders.size()==1);
        assert(w.selectedFleetPanelSummary().contains("Scout #1"));
        QTemporaryDir dir;QString error;TurnOrderFileData packet{1,w.state_.turn,1,w.pendingOrders_,w.pendingDescriptions_},decoded;
        assert(write_turn_order_file(dir.filePath("split.sunsorders"),packet,error));
        assert(read_turn_order_file(dir.filePath("split.sunsorders"),decoded,error));
        const auto next=TurnProcessor{}.process(w.state_,{decoded.orders});
        assert(next.fleets[0].name=="Scout #1" && next.fleets.back().name=="Scout #2");
    }
};
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);MainWindow w;w.installUiPolish();w.installFleetReadabilityPolish();w.installProductionQueue();w.installPlanetPolish();
    MainWindowTestAccess::load(w);attachRouteProgramDock(w);w.resize(1400,1000);w.show();QApplication::processEvents();
    MainWindowTestAccess::navigation(w);MainWindowTestAccess::queueBudget(w);MainWindowTestAccess::radiation(w);MainWindowTestAccess::split(w);
}
