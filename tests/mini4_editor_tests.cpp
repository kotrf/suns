#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/production.hpp"
#include "suns/communications.hpp"
#include "suns/ship_refit.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>
#include <algorithm>
#include <cmath>
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
    static void fleetArrivalChoice(MainWindow& w) {
        for (const auto action : {FleetArrivalActionKind::SupplyOutpost, FleetArrivalActionKind::None,
                 FleetArrivalActionKind::MergeWithFleet}) {
            load(w);
            auto source=w.state_.fleets.front();
            source.position={50,-50};source.destination.reset();source.arrivalAction.reset();
            source.waypointQueue.clear();source.pendingCommands.clear();source.repeatOrders=false;
            source.ships={{source.design,1}};source.fuel=fleet_fuel_capacity(w.state_,source);
            auto target=source;target.id=2;target.name="Fuel recipient";target.position={60,-50};target.fuel=0;
            for (auto* f : {&source,&target}) {f->telemetry={};f->telemetry=confirmed_fleet_telemetry(w.state_,*f);}
            w.state_.fleets={source,target};w.state_.nextFleetId=3;w.state_.wormholeRules.spawnChancePerTurn=0;
            w.rebuildScene();emit w.routeProgramContextChanged(true);
            auto* actions=w.findChild<QComboBox*>("routeArrivalActionCombo");assert(actions);
            actions->setCurrentIndex(actions->findData(int(action)));
            const auto reserve=source.fuel/4;
            w.findChild<QDoubleSpinBox*>("routeFuelReserveSpin")->setValue(reserve);
            w.findChild<QPushButton*>("routeNewWaypointButton")->click();click(w,target.position);
            assert(w.pendingOrders_.orders.size()==1);
            const auto& order=std::get<MoveFleetOrder>(w.pendingOrders_.orders.front());
            assert(order.fleet==source.id && order.targetFleet==target.id);
            assert(order.arrivalAction.kind==action);
            if (action==FleetArrivalActionKind::SupplyOutpost) assert(std::abs(order.arrivalAction.reserveFuel-reserve)<1e-6);
            QTemporaryDir dir;QString error;TurnOrderFileData packet{1,w.state_.turn,1,w.pendingOrders_,w.pendingDescriptions_},decoded;
            assert(write_turn_order_file(dir.filePath("arrival.sunsorders"),packet,error));
            assert(read_turn_order_file(dir.filePath("arrival.sunsorders"),decoded,error));
            const auto next=TurnProcessor{}.process(w.state_,{decoded.orders});
            const auto find=[&](FleetId id){return std::find_if(next.fleets.begin(),next.fleets.end(),[&](const auto& f){return f.id==id;});};
            const auto recipient=find(target.id);assert(recipient!=next.fleets.end());
            if(action==FleetArrivalActionKind::MergeWithFleet) {
                assert(find(source.id)==next.fleets.end() && fleet_ship_count(*recipient,target.design)==2);
            } else {
                const auto sender=find(source.id);assert(sender!=next.fleets.end());
                assert(same_position(sender->position,recipient->position));
                assert(fleet_ship_count(*sender,source.design)==1 && fleet_ship_count(*recipient,target.design)==1);
                if(action==FleetArrivalActionKind::SupplyOutpost) {
                    assert(recipient->fuel>0 && std::abs(sender->fuel-reserve)<1e-6);
                } else assert(recipient->fuel==0);
            }
        }
    }
    static void paidRefitMinerals(MainWindow& w) {
        load(w);w.state_.players[0].technology.levels.fill(26);w.state_.players[0].technology.researchActive=false;
        auto from=w.state_.shipDesigns.front();from.id=w.state_.nextShipDesignId++;from.name="Refit source";
        from.components={ShipComponentType::FusionDrive,ShipComponentType::LongRangeScanner};
        from.placements=autoplace_ship_components(from.hull,from.components);assert(ship_design_valid(from));
        auto to=from;to.id=w.state_.nextShipDesignId++;to.name="Refit target";
        to.components.back()=ShipComponentType::PenetratingScanner;
        to.placements=autoplace_ship_components(to.hull,to.components);assert(ship_design_valid(to));
        w.state_.shipDesigns={from,to};auto& f=w.state_.fleets.front();f.design=from.id;f.ships={{from.id,1}};f.fuel=10;
        f.destination.reset();f.pendingCommands.clear();f.telemetry={};f.telemetry=confirmed_fleet_telemetry(w.state_,f);
        auto& p=w.state_.planets.front();p.population=1000000;p.industry=1000;p.mines=0;p.minerals={};p.productionQueue.clear();
        assert(queue_ship_refit(w.state_,1,{p.id,f.id,from.id,to.id}));
        w.state_=TurnProcessor{}.process(w.state_,{});
        auto& waiting=w.state_.planets.front();assert(waiting.productionQueue.size()==1);
        assert(waiting.productionQueue.front().remainingCost==0 && waiting.productionWaitingForMinerals);
        const auto cost=production_item_mineral_cost(w.state_,waiting.productionQueue.front());
        assert(cost.boranium>0 && cost.germanium>0);
        set_survey_level(w.state_,1,waiting.star,SurveyLevel::DeepSurvey,w.state_.turn);
        w.rebuildScene();w.refreshProductionQueue();
        auto* row=w.productionQueueTree_->topLevelItem(0);assert(row && row->text(2)=="minerals");
        assert(row->text(3)==QString::fromUtf8("—"));
        const auto deficit=QString("Shortfall for this batch: I %1 / B %2 / G %3 kt")
            .arg(std::max(0.0,cost.ironium-waiting.minerals.ironium),0,'f',1)
            .arg(std::max(0.0,cost.boranium-waiting.minerals.boranium),0,'f',1)
            .arg(std::max(0.0,cost.germanium-waiting.minerals.germanium),0,'f',1);
        assert(row->toolTip(1).contains(deficit) && !row->toolTip(1).contains("are uncertain"));
        waiting.minerals={std::max(0.0,cost.ironium),std::max(0.0,cost.boranium),std::max(0.0,cost.germanium)};
        set_survey_level(w.state_,1,waiting.star,SurveyLevel::DeepSurvey,w.state_.turn);w.rebuildScene();w.refreshProductionQueue();
        assert(w.productionQueueTree_->topLevelItem(0)->text(2)=="0");
        const auto next=TurnProcessor{}.process(w.state_,{});
        assert(next.planets.front().productionQueue.empty() && fleet_ship_count(next.fleets.front(),to.id)==1);
    }
    static void selectedLegMapAppend(MainWindow& w,bool newWaypoint) {
        load(w);auto source=w.state_.fleets.front();
        source.position={10,-5};source.destination.reset();source.arrivalAction.reset();
        source.waypointQueue.clear();source.pendingCommands.clear();source.repeatOrders=false;
        source.fuel=fleet_fuel_capacity(w.state_,source);source.minerals={0,3,0};
        auto target=source;target.id=2;target.name="Next rendezvous";target.position={35,-20};target.minerals={};
        for(auto* f : {&source,&target}) {f->telemetry={};f->telemetry=confirmed_fleet_telemetry(w.state_,*f);}
        w.state_.fleets={source,target};w.state_.nextFleetId=3;w.state_.wormholeRules.spawnChancePerTurn=0;
        w.rebuildScene();emit w.routeProgramContextChanged(true);
        auto* action=w.findChild<QComboBox*>("routeArrivalActionCombo");
        auto* cargo=w.findChild<QComboBox*>("routeCargoCombo");
        auto* warp=w.findChild<QProgressBar*>("routeWarpSelector");
        auto* rows=w.findChild<QTreeWidget*>("routeProgramQueue");assert(action && cargo && warp && rows);
        action->setCurrentIndex(action->findData(int(FleetArrivalActionKind::UnloadAll)));
        cargo->setCurrentIndex(cargo->findData(int(FleetCargoKind::Boranium)));warp->setValue(5);
        w.findChild<QPushButton*>("routeAddButton")->click();
        assert(rows->topLevelItemCount()==1);rows->setCurrentItem(rows->topLevelItem(0));
        const auto first=std::get<MoveFleetOrder>(w.pendingOrders_.orders.front());
        assert(first.arrivalAction.kind==FleetArrivalActionKind::UnloadAll && first.arrivalAction.cargo==FleetCargoKind::Boranium);
        const auto control=TurnProcessor{}.process(w.state_,{w.pendingOrders_});
        assert(control.fleets.front().minerals.boranium==0); // The original route really unloads.
        if(newWaypoint) {
            w.findChild<QPushButton*>("routeNewWaypointButton")->click();
            rows->setCurrentItem(rows->topLevelItem(0)); // Reselect while map picking is still active.
            assert(w.routeProgramMapTargetPickActive());
        }
        click(w,target.position,newWaypoint ? Qt::LeftButton : Qt::RightButton);
        assert(w.pendingOrders_.orders.size()==1);
        const auto& order=std::get<MoveFleetOrder>(w.pendingOrders_.orders.front());
        assert(order.arrivalAction==first.arrivalAction && order.warp==first.warp);
        assert(order.targetFleet==0 && same_position(order.destination,first.destination));
        assert(order.queuedWaypoints.size()==1 && order.queuedWaypoints.front().targetFleet==target.id);
        assert(same_position(order.queuedWaypoints.front().destination,target.position));
        assert(order.queuedWaypoints.front().arrivalAction.kind==FleetArrivalActionKind::None);
        assert(rows->topLevelItemCount()==2 && rows->currentItem()==rows->topLevelItem(1));
        warp->setValue(3);assert(std::get<MoveFleetOrder>(w.pendingOrders_.orders.front()).warp==5);
        assert(std::get<MoveFleetOrder>(w.pendingOrders_.orders.front()).queuedWaypoints.front().warp==3);
        QTemporaryDir dir;QString error;TurnOrderFileData packet{1,w.state_.turn,1,w.pendingOrders_,w.pendingDescriptions_},decoded;
        assert(write_turn_order_file(dir.filePath("selected-leg.sunsorders"),packet,error));
        assert(read_turn_order_file(dir.filePath("selected-leg.sunsorders"),decoded,error));
        const auto next=TurnProcessor{}.process(w.state_,{decoded.orders});
        assert(next.fleets.size()==2 && next.fleets.front().minerals.boranium==0);
        assert(std::abs(next.planets.front().minerals.boranium-control.planets.front().minerals.boranium)<1e-8);
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
    if(argc==2) {
        if(std::string(argv[1])=="arrival") {MainWindowTestAccess::fleetArrivalChoice(w);return 0;}
        if(std::string(argv[1])=="refit") {MainWindowTestAccess::paidRefitMinerals(w);return 0;}
        if(std::string(argv[1])=="selected-quick") {MainWindowTestAccess::selectedLegMapAppend(w,false);return 0;}
        if(std::string(argv[1])=="selected-new") {MainWindowTestAccess::selectedLegMapAppend(w,true);return 0;}
    }
    MainWindowTestAccess::navigation(w);MainWindowTestAccess::queueBudget(w);MainWindowTestAccess::radiation(w);MainWindowTestAccess::split(w);
    MainWindowTestAccess::fleetArrivalChoice(w);MainWindowTestAccess::paidRefitMinerals(w);
    MainWindowTestAccess::selectedLegMapAppend(w,false);MainWindowTestAccess::selectedLegMapAppend(w,true);
}
