#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "ship_designer_dialog.hpp"
#include "save_game.hpp"
#include "mineral_stock_panel.hpp"
#include "suns/production.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QListWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QLabel>
#include <QMouseEvent>
#include <cassert>
using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w) {
        w.state_=make_demo_game();w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();
        w.selection_.star=w.state_.stars.front().id;w.selection_.fleet=w.state_.fleets.front().id;
        set_survey_level(w.state_,1,w.state_.stars.front().id,SurveyLevel::DeepSurvey,w.state_.turn);
        w.rebuildScene();
    }
    static void mixed(MainWindow& w) {
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();
        auto& f=w.state_.fleets.front();f.destination.reset();
        f.ships={{w.state_.shipDesigns[0].id,2},{w.state_.shipDesigns[1].id,3}};f.telemetry.ships=f.ships;
        auto copy=w.state_.shipDesigns.front();copy.id=w.state_.nextShipDesignId++;copy.name="Refit comparison";
        w.state_.shipDesigns.push_back(copy);w.state_.players.front().technology.levels.fill(26);
        w.rebuildScene();
    }
    static auto& orders(MainWindow& w){return w.pendingOrders_;}
    static auto& state(MainWindow& w){return w.state_;}
    static void emission(MainWindow& w) {
        GameEvent e;e.id=999;e.recipient=1;e.kind=GameEventKind::EmissionDetected;e.turn=w.state_.turn;e.position={100,120};e.quantity=30;
        w.turnMessages_={e};w.refreshTurnMessages();w.turnMessagesList_->setCurrentRow(0);
    }
    static QString laboratory(MainWindow& w) {
        GameEvent e;e.id=1000;e.recipient=1;e.kind=GameEventKind::ResonanceProjectStatus;e.turn=w.state_.turn;e.star=1;e.planet=1;e.quantity=42;
        w.turnMessages_={e};w.refreshTurnMessages();w.turnMessagesList_->setCurrentRow(0);return w.turnMessageBody_->toPlainText();
    }
    static double mapScale(MainWindow& w){return std::abs(w.view_->transform().m11());}
    static QPointF center(MainWindow& w){return w.view_->mapToScene(w.view_->viewport()->rect().center());}
    static QGraphicsView* mapView(MainWindow& w){return w.view_;}
    static void pendingDeletion(MainWindow& w) {
        load(w);
        w.state_.players[0].technology.levels.fill(26);
        auto& p=w.state_.planets[0];p.productionQueue={{ProductionKind::Factory,1000000},{ProductionKind::Mine,1000000},{ProductionKind::ColonyDefense,1000000}};
        const auto d=w.state_.shipDesigns[0];
        auto opponent=w.state_.players[0];opponent.id=2;opponent.name="Other empire";w.state_.players.push_back(opponent);
        // Other empires consume global IDs which the planning view cannot see.
        for(int i=0;i<3;++i){auto foreign=d;foreign.owner=2;foreign.id=w.state_.nextShipDesignId++;foreign.name="Foreign project"+std::to_string(i);w.state_.shipDesigns.push_back(foreign);}
        w.pendingOrders_.orders={CreateShipDesignOrder{"Disposable",d.hull,d.components,d.placements},QueueShipDesignOrder{p.id,0,"Disposable"},ReorderProductionQueueOrder{p.id,3,1},CancelProductionOrder{p.id,2}};
        w.pendingDescriptions_={"Create","Build","Move","Cancel"};
        set_survey_level(w.state_,1,p.star,SurveyLevel::DeepSurvey,w.state_.turn);
        w.rebuildScene();
        w.openShipDesigner();
        auto* dialog=w.shipDesigner_.data();assert(dialog);
        auto* choices=dialog->findChild<QComboBox*>("shipDesignTemplate");choices->setCurrentIndex(choices->findText("Disposable"));
        dialog->findChild<QPushButton*>("deleteShipDesign")->click();
        const auto check=[](const auto& s){const auto& q=s.planets[0].productionQueue;assert(q.size()==2);assert(q[0].kind==ProductionKind::Factory);assert(q[1].kind==ProductionKind::ColonyDefense);};
        const auto v=make_player_view(w.state_,1).state;
        const auto plan=planned_production_state(v,w.pendingOrders_);
        check(plan);
        QTemporaryDir dir;QString error;TurnOrderFileData packet{1,w.state_.turn,1,w.pendingOrders_,w.pendingDescriptions_};
        assert(write_turn_order_file(dir.filePath("delete.sunsorders"),packet,error));
        TurnOrderFileData decoded;assert(read_turn_order_file(dir.filePath("delete.sunsorders"),decoded,error));
        const auto hosted=TurnProcessor{}.process(w.state_,{decoded.orders});check(hosted);
        for(const auto& project:hosted.shipDesigns)if(project.owner==2)assert(!project.retired);
        const auto removed=std::find_if(hosted.shipDesigns.begin(),hosted.shipDesigns.end(),[](const auto& project){return project.name=="Disposable";});
        assert(removed!=hosted.shipDesigns.end() && removed->retired);
        dialog->close();
    }
};
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);MainWindow w;w.installUiPolish();w.installFleetReadabilityPolish();w.installProductionQueue();w.installPlanetPolish();w.installMapDisplayModes();w.installTurnMessages();
    MainWindowTestAccess::load(w);attachRouteProgramDock(w);w.resize(1400,1000);w.show();QApplication::processEvents();
    auto* mode=w.findChild<QComboBox*>("mapDisplayModeCombo");assert(mode && mode->count()==5);
    mode->setCurrentIndex(mode->findData(4));QApplication::processEvents();
    bool bars=false;for(auto* item:w.routeProgramScene()->items())if(item->data(0).toString()=="mineralOverlay" && item->isVisible()) {
        bars=true;assert(item->childItems().size()==3);assert(item->toolTip().contains("Last stock report"));
        const auto* view=MainWindowTestAccess::mapView(w);
        for(auto* child:item->childItems()) {
            auto* bar=dynamic_cast<QGraphicsRectItem*>(child);assert(bar);
            const auto point=bar->deviceTransform(view->viewportTransform()).map(bar->rect().center()).toPoint();
            assert(view->items(point).contains(bar));
            assert(bar->toolTip()==item->toolTip());
            assert(bar->toolTip().contains("I 100.0 / B 100.0 / G 100.0 kt"));
            assert(bar->toolTip().contains(QString("Last stock report: year %1").arg(qulonglong(MainWindowTestAccess::state(w).turn))));
        }
    }
    assert(bars);
    FleetArrivalAction unload{FleetArrivalActionKind::UnloadAll,1,FleetCargoKind::Colonists};
    assert(w.appendSelectedStarWaypoint(5,unload));
    auto* tree=w.findChild<QTreeWidget*>("routeProgramQueue");assert(tree && tree->topLevelItemCount());tree->setCurrentItem(tree->topLevelItem(0));QApplication::processEvents();
    auto* cargo=w.findChild<QComboBox*>("routeCargoCombo");assert(cargo && cargo->isEnabled());cargo->setCurrentIndex(cargo->findData(int(FleetCargoKind::Boranium)));
    assert(w.selectedFleetRouteProgramLegAction(0)->cargo==FleetCargoKind::Boranium);
    auto* action=w.findChild<QComboBox*>("routeArrivalActionCombo");
    if(!action)for(auto* c:w.findChildren<QComboBox*>())if(c->findData(int(FleetArrivalActionKind::Colonize))>=0 && c->findData(int(FleetArrivalActionKind::UnloadAll))>=0 && c!=cargo){action=c;break;}
    assert(action);action->setCurrentIndex(action->findData(int(FleetArrivalActionKind::Colonize)));
    assert(action->currentData().toInt()==int(FleetArrivalActionKind::UnloadAll));
    assert(!w.findChild<QWidget*>("routeColonyPlanCombo")->isEnabled());assert(cargo->isEnabled());
    MainWindowTestAccess::emission(w);auto* show=w.findChild<QPushButton*>("showTurnMessageOnMap");assert(show && show->isEnabled());show->click();QApplication::processEvents();
    const auto center=MainWindowTestAccess::center(w);assert(std::abs(center.x()-100)*MainWindowTestAccess::mapScale(w)<2 && std::abs(center.y()-120)*MainWindowTestAccess::mapScale(w)<2);
    const auto labText=MainWindowTestAccess::laboratory(w);assert(labText.contains("42 / 600") && !labText.contains("waiting for minerals"));
    auto s=make_demo_game();auto extra=s.shipDesigns.front();extra.id=s.nextShipDesignId++;extra.name="ZZ Archive";extra.retired=true;s.shipDesigns.push_back(extra);
    ShipDesignerDialog designer(s,1);auto* templates=designer.findChild<QComboBox*>("shipDesignTemplate");assert(templates->itemText(0)=="New design");assert(templates->findData(extra.id)<0);
    for(int i=2;i<templates->count();++i)assert(QString::compare(templates->itemText(i-1),templates->itemText(i),Qt::CaseInsensitive)<=0);
    templates->setCurrentIndex(1);auto* name=designer.findChild<QLineEdit*>("shipDesignName");assert(name->text()==templates->currentText()+" Gen2");name->setText("Custom name");assert(designer.draft().name=="Custom name");
    designer.showDesign(templates->currentData().toUInt());assert(name->isReadOnly());assert(designer.findChild<QPushButton*>("saveShipDesign")->isHidden());
    w.installFleetPortraitPolish();MainWindowTestAccess::mixed(w);QApplication::processEvents();
    auto* portrait=w.findChild<QLabel*>("fleetPortrait");assert(portrait);
    QMouseEvent click(QEvent::MouseButtonRelease,QPointF(portrait->rect().center()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(portrait,&click);QApplication::processEvents();
    auto* composition=w.findChild<QListWidget*>("fleetCompositionList");assert(composition && composition->count()==2);
    assert(composition->item(0)->text().contains("×2") && composition->item(1)->text().contains("×3"));
    composition->itemClicked(composition->item(0));
    auto* fitting=w.findChild<QLineEdit*>("shipDesignName");assert(fitting && fitting->isReadOnly());
    w.findChild<QPushButton*>("refitFleetShip")->click();QApplication::processEvents();
    auto* refit=w.findChild<QDialog*>("shipRefitDialog");assert(refit);
    assert(refit->findChild<QLabel*>("shipRefitQuote")->text().contains("50%"));
    const auto capture=qEnvironmentVariable("SUNS_MINI3_SCREENSHOT");if(!capture.isEmpty()) {
        assert(refit->grab().save(capture));
        assert(fitting->window()->grab().save(capture+"-fit.png"));
        assert(w.grab().save(capture+"-map.png"));
    }
    MainWindowTestAccess::pendingDeletion(w);
    QTemporaryDir dir;assert(dir.isValid());QString error;SaveGameData save;save.campaignId=1;save.turnToken=1;save.state=s;
    save.pendingOrders={1,{DeleteShipDesignOrder{extra.id},RefitShipOrder{1,s.fleets.front().id,s.shipDesigns[0].id,0,"Deferred target"}}};
    save.pendingDescriptions={"Delete design","Refit ship"};
    auto& p=save.state.planets.front();ProductionItem item{ProductionKind::ShipRefit,5,s.shipDesigns[1].id};item.refit=ShipRefitJob{s.fleets.front().id,s.shipDesigns[0].id,s.shipDesigns[1].id};p.productionQueue={item};
    assert(write_save_game_file(dir.filePath("mini3.suns"),save,error));SaveGameData loaded;const auto loadedOk=read_save_game_file(dir.filePath("mini3.suns"),loaded,error);if(!loadedOk)qWarning("Save load: %s",qPrintable(error));assert(loadedOk);
    assert(loaded.state.shipDesigns.back().retired && loaded.state.planets.front().productionQueue.front().refit==item.refit);
    assert(std::holds_alternative<DeleteShipDesignOrder>(loaded.pendingOrders.orders[0]) && std::holds_alternative<RefitShipOrder>(loaded.pendingOrders.orders[1]));
    TurnOrderFileData packet{1,s.turn,1,save.pendingOrders,save.pendingDescriptions};assert(write_turn_order_file(dir.filePath("mini3.sunsorders"),packet,error));TurnOrderFileData decoded;assert(read_turn_order_file(dir.filePath("mini3.sunsorders"),decoded,error));assert(decoded.orders.orders.size()==2);assert(std::get<RefitShipOrder>(decoded.orders.orders[1]).pendingTargetName=="Deferred target");
}
