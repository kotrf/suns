#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "ship_designer_dialog.hpp"
#include "save_game.hpp"
#include "mineral_stock_panel.hpp"
#include "suns/production.hpp"
#include "suns/communications.hpp"
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
#include <QFile>
#include <QDataStream>
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
    static void geologyReport(MainWindow& w) {
        load(w);
        auto& planet = w.state_.planets[1];
        planet.name = "Target <II> & moons";
        set_survey_level(w.state_, 1, planet.star, SurveyLevel::BasicScan, w.state_.turn);
        GameEvent report;
        report.id = 1001;
        report.recipient = 1;
        report.kind = GameEventKind::RemoteGeologyObserved;
        report.turn = w.state_.turn;
        report.observedTurn = report.turn;
        report.star = planet.star;
        report.planet = planet.id;
        report.fleet = w.state_.fleets.front().id;
        report.deliveredMinerals = {20, 80, 90};
        w.turnMessages_ = {report};
        w.refreshTurnMessages();
        w.turnMessagesList_->setCurrentRow(0);
        const auto body = w.turnMessageBody_->toPlainText();
        assert(body.contains("Target <II> & moons — remote geology\n"));
        assert(body.count('\n') == 2 && body.contains("Observed by fleet #1") && !body.contains("<br>"));
        w.turnMessageShowOnMapButton_->click();
        QApplication::processEvents();
        assert(w.selection_.star == report.star);
        assert(w.currentDistanceSelectionKind_ == 1 && w.currentDistanceSelectionId_ == report.star);
        load(w);
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
    static void remoteQueueEditing(MainWindow& w) {
        auto state=make_demo_game();state.players[0].technology.researchAllocationPercent=0;state.players.push_back({2,"Host"});
        const auto star=state.stars[1].id;state.stars[1].position={300,0};state.fleets.clear();state.planets[0].radar=PlanetaryRadarLevel::Local;
        auto& p=state.planets[1];const auto id=p.id;p.owner=1;p.population=10000;p.industry=0;p.habitability=0;p.radar=PlanetaryRadarLevel::Local;p.minerals={10000,10000,10000};
        p.productionQueue={{ProductionKind::Factory,production_cost(ProductionKind::Factory),0,ProductionAutomation::Annual,1},{ProductionKind::Mine,1000000}};
        assert(communication_delay_turns(state,1,state.stars[1].position)==2);
        set_survey_level(state,1,star,SurveyLevel::DeepSurvey,state.turn);
        const auto originalSetup=state;
        for(int year=0;year<8;++year)state=TurnProcessor{}.process(state,{});
        QTemporaryDir dir;assert(dir.isValid());QString error;
        SaveGameData host;host.mode=SessionMode::Host;host.campaignId=1;host.turnToken=2;host.playerTokens={{1,1},{2,2}};host.pendingOrders={2,{}};
        const auto bytes=[](const QString& path){QFile file(path);assert(file.open(QIODevice::ReadOnly));return file.readAll();};
        const auto openPlayer=[&](const SaveGameData& h) {
            auto player=make_player_turn(h,1);player.selectedStar=star;
            assert(write_save_game_file(dir.filePath("player.sunsturn"),player,error));
            assert(w.loadGameFromPath(dir.filePath("player.sunsturn")));
            w.selection_.star=star;w.rebuildScene();w.refreshProductionQueue();
            assert(w.productionQueueTree_->topLevelItemCount()==2);
            w.productionQueueTree_->setCurrentItem(w.productionQueueTree_->topLevelItem(0));
        };
        for(int year=0;year<3;++year) {
            host.state=state;
            assert(write_save_game_file(dir.filePath("host.suns"),host,error));
            SaveGameData loaded;assert(read_save_game_file(dir.filePath("host.suns"),loaded,error));
            assert(loaded.state.planets[1].productionQueue==host.state.planets[1].productionQueue);
            openPlayer(loaded);
            const auto& received=w.receivedPlayerState();
            const auto report=std::find_if(received.players[0].surveyKnowledge.begin(),received.players[0].surveyKnowledge.end(),[&](const auto& k){return k.star==star;});
            assert(report!=received.players[0].surveyKnowledge.end() && report->observedTurn+2==received.turn);
            assert(received.planets[1].productionQueue[0].queueId==loaded.state.planets[1].productionQueue[0].queueId);
            assert(received.planets[1].productionQueue[0].queueId);
            w.productionRemoveButton_->click();QApplication::processEvents();
            assert(w.productionQueueTree_->topLevelItemCount()==1 && w.pendingOrders_.orders.size()==1);
            assert(std::holds_alternative<CancelProductionOrder>(w.pendingOrders_.orders[0]));
            TurnOrderFileData packet{w.campaignId_,w.state_.turn,w.turnToken_,w.pendingOrders_,w.pendingDescriptions_},decoded;
            assert(write_turn_order_file(dir.filePath("remove.sunsorders"),packet,error));
            assert(read_turn_order_file(dir.filePath("remove.sunsorders"),decoded,error));
            assert(validate_turn_submission(loaded,decoded).isEmpty());
            const auto next=TurnProcessor{}.process(loaded.state,{decoded.orders});
            assert(next.planets[1].industry==loaded.state.planets[1].industry);
            assert(next.planets[1].productionQueue.size()==1 && next.planets[1].productionQueue[0].kind==ProductionKind::Mine);
            // Hidden physical allocations cannot change the exported turn bytes.
            auto hidden=loaded;hidden.state.planets[1].nextProductionQueueId+=100;
            assert(write_save_game_file(dir.filePath("hidden.sunsturn"),make_player_turn(hidden,1),error));
            assert(write_save_game_file(dir.filePath("visible.sunsturn"),make_player_turn(loaded,1),error));
            assert(bytes(dir.filePath("hidden.sunsturn"))==bytes(dir.filePath("visible.sunsturn")));
            state=TurnProcessor{}.process(loaded.state,{});
        }
        // An empty queue must retain its allocator through a host save/reload.
        auto initial=originalSetup;auto& colony=initial.planets[1];colony.population=6000;colony.industry=0;
        colony.productionQueue={{ProductionKind::Factory,4}};detail::assign_production_queue_ids(initial);
        const auto completedId=colony.productionQueue[0].queueId;
        set_survey_level(initial,1,star,SurveyLevel::DeepSurvey,initial.turn);
        auto empty=host;empty.state=TurnProcessor{}.process(initial,{});assert(empty.state.planets[1].productionQueue.empty());
        assert(write_save_game_file(dir.filePath("empty.suns"),empty,error));
        SaveGameData restored;assert(read_save_game_file(dir.filePath("empty.suns"),restored,error));
        assert(restored.state.planets[1].nextProductionQueueId>completedId);
        const auto successor=TurnProcessor{}.process(restored.state,{{1,{QueueProductionBatchOrder{id,ProductionKind::Factory,1}}}});
        assert(successor.planets[1].productionQueue.size()==1 && successor.planets[1].productionQueue[0].remainingCost>0);
        assert(successor.planets[1].productionQueue[0].queueId>completedId);
        assert(make_player_view(successor,1).state.planets[1].productionQueue[0].queueId==completedId);
        const auto retained=TurnProcessor{}.process(successor,{{1,{CancelProductionOrder{id,0}}}});
        assert(retained.planets[1].industry==2);
        // The base v127 bytes are unchanged; absent PQID must not invent old IDs.
        host.state=state;assert(write_save_game_file(dir.filePath("host.suns"),host,error));
        auto old=bytes(dir.filePath("host.suns"));const auto extension=old.lastIndexOf("PQID");assert(extension>0);
        old.truncate(extension);{QDataStream out(&old,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32(127);}
        {QFile file(dir.filePath("legacy.suns"));assert(file.open(QIODevice::WriteOnly));assert(file.write(old)==old.size());}
        SaveGameData legacy;assert(read_save_game_file(dir.filePath("legacy.suns"),legacy,error));
        assert(!legacy.state.planets[1].productionQueue[0].queueId && legacy.state.planets[1].nextProductionQueueId==1);
        openPlayer(legacy);w.productionRemoveButton_->click();QApplication::processEvents();
        assert(w.pendingOrders_.orders.empty() && w.productionQueueTree_->topLevelItemCount()==2);
        assert(w.productionQueueSummary_->text().contains("newer colony report"));
        w.moveSelectedProductionItem(1);assert(w.pendingOrders_.orders.empty());
        w.appendPendingOrder(QueueProductionOrder{id,ProductionKind::Mine},"Build mine");w.refreshProductionQueue();
        w.productionQueueTree_->setCurrentItem(w.productionQueueTree_->topLevelItem(2));w.productionRemoveButton_->click();
        assert(w.pendingOrders_.orders.size()==2 && w.productionQueueTree_->topLevelItemCount()==2);
        // The next physical reports introduce IDs after migration, even with delay.
        for(int year=0;year<8;++year)legacy.state=TurnProcessor{}.process(legacy.state,{});
        openPlayer(legacy);w.productionRemoveButton_->click();assert(w.pendingOrders_.orders.size()==1);
        assert(TurnProcessor{}.process(legacy.state,{w.pendingOrders_}).planets[1].industry==legacy.state.planets[1].industry);
        // A duplicate persisted identity must be rejected instead of aliasing edits.
        auto corrupt=bytes(dir.filePath("host.suns"));QDataStream edit(&corrupt,QIODevice::ReadWrite);
        assert(edit.device()->seek(corrupt.lastIndexOf("PQID")+8));quint32 planets{};edit>>planets;bool changed=false;
        for(quint32 i=0;i<planets;++i) {
            quint32 planet{},count{};quint64 next{};edit>>planet>>next>>count;
            if(planet==id){assert(count==2);quint64 first{};edit>>first;edit<<first;changed=true;break;}
            assert(edit.device()->seek(edit.device()->pos()+8*count));
        }
        assert(changed);{QFile file(dir.filePath("corrupt.suns"));assert(file.open(QIODevice::WriteOnly));assert(file.write(corrupt)==corrupt.size());}
        SaveGameData rejected;assert(!read_save_game_file(dir.filePath("corrupt.suns"),rejected,error));
    }
    static void staleQueuePacket() {
        for(const auto quantity: {1u,10u}) {
            auto state=make_demo_game();state.players[0].technology.researchAllocationPercent=0;state.players.push_back({2,"Host"});
            const auto star=state.stars[1].id;state.stars[1].position={2000,2000};
            auto& p=state.planets[1];const auto id=p.id;p.owner=1;p.population=10000;p.industry=0;p.habitability=0;
            p.radar=PlanetaryRadarLevel::None;p.minerals={10000,10000,10000};p.productionQueue={{ProductionKind::Factory,4}};
            detail::assign_production_queue_ids(state);
            set_survey_level(state,1,star,SurveyLevel::DeepSurvey,state.turn);
            SaveGameData host;host.mode=SessionMode::Host;host.campaignId=1;host.turnToken=1;host.playerTokens={{1,1},{2,2}};host.pendingOrders={2,{}};
            host.state=TurnProcessor{}.process(state,{{1,{QueueProductionBatchOrder{id,ProductionKind::Factory,quantity}}}});
            const auto control=TurnProcessor{}.process(host.state,{});
            QTemporaryDir dir;assert(dir.isValid());QString error;
            // Persist both sides; the player's queue still describes the completed row.
            assert(write_save_game_file(dir.filePath("host.suns"),host,error));
            SaveGameData loaded;assert(read_save_game_file(dir.filePath("host.suns"),loaded,error));
            assert(write_save_game_file(dir.filePath("player.sunsturn"),make_player_turn(host,1),error));
            SaveGameData player;assert(read_save_game_file(dir.filePath("player.sunsturn"),player,error));
            assert(player.state.planets[1].productionQueue.size()==1 && player.state.planets[1].productionQueue[0].quantity==1);
            const PlayerOrders orders{1,{CancelProductionOrder{id,0}}};
            assert(planned_production_state(player.state,orders).planets[1].productionQueue.empty());
            TurnOrderFileData packet{player.campaignId,player.state.turn,player.turnToken,orders,{"Remove factory"}},decoded;
            assert(write_turn_order_file(dir.filePath("cancel.sunsorders"),packet,error));
            assert(read_turn_order_file(dir.filePath("cancel.sunsorders"),decoded,error));
            assert(validate_turn_submission(loaded,decoded).isEmpty());
            const auto next=TurnProcessor{}.process(loaded.state,{decoded.orders});
            assert(next.planets[1].industry==2 && next.planets[1].industry==control.planets[1].industry);
            assert(next.planets[1].productionQueue==control.planets[1].productionQueue);
        }
    }
    static void rejectedQueuePacket() {
        for(int mode=0;mode<3;++mode) {
            const bool replacement=mode==1,fromTemplate=mode==2;
            auto state=make_demo_game();state.players[0].technology.levels.fill(26);state.players[0].technology.researchAllocationPercent=0;
            auto design=state.shipDesigns[0];design.id=state.nextShipDesignId++;design.name="Remote project";state.shipDesigns.push_back(design);
            auto& p=state.planets[0];p.productionQueue={{ProductionKind::ColonyShip,1000000,design.id},{ProductionKind::Factory,1000000},{ProductionKind::Mine,1000000,0,ProductionAutomation::None,10}};
            if(replacement)p.productionQueue={{ProductionKind::Mine,1000000,0,ProductionAutomation::None,10}};
            if(fromTemplate){const ProductionItem rule{ProductionKind::Mine,production_cost(ProductionKind::Mine),0,ProductionAutomation::Annual,10};p.productionQueue={rule,{ProductionKind::ColonyShip,1000000,design.id}};state.players[0].productionTemplates={{"Replacement",{rule,{ProductionKind::Factory,production_cost(ProductionKind::Factory)}}}};}
            set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
            auto hidden=state.fleets[0];hidden.id=state.nextFleetId++;hidden.design=design.id;hidden.ships={{design.id,1}};hidden.telemetry={};hidden.orbitalShipyardBirth=true;hidden.position={2000,2000};state.fleets.push_back(hidden);
            PlayerOrders orders{1,{DeleteShipDesignOrder{design.id}}};
            if(replacement){orders.orders.push_back(CreateShipDesignOrder{design.name,design.hull,design.components,design.placements});orders.orders.push_back(QueueShipDesignOrder{p.id,0,design.name});orders.orders.push_back(ReorderProductionQueueOrder{p.id,1,0});}
            if(fromTemplate)orders.orders.push_back(ApplyProductionTemplateOrder{p.id,"Replacement"});
            orders.orders.push_back(CancelProductionOrder{p.id,fromTemplate?1u:0u});
            QStringList descriptions;for(const auto& order:orders.orders){(void)order;descriptions<<"Queue edit";}
            QTemporaryDir dir;QString error;TurnOrderFileData packet{1,state.turn,1,orders,descriptions},decoded;
            assert(write_turn_order_file(dir.filePath("queue.sunsorders"),packet,error));
            assert(read_turn_order_file(dir.filePath("queue.sunsorders"),decoded,error));
            const auto next=TurnProcessor{}.process(state,{decoded.orders});const auto& queue=next.planets[0].productionQueue;
            assert(queue.size()==std::size_t(replacement?1:2));
            assert(queue.back().kind==ProductionKind::Mine && queue.back().quantity==10);
            if(!replacement)assert(queue.front().shipDesign==design.id);
        }
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
    MainWindowTestAccess::geologyReport(w);
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
    w.findChild<QPushButton*>("refitFleetShip")->click();QApplication::processEvents();
    const auto refits=w.findChildren<QDialog*>("shipRefitDialog");assert(!refits.empty());
    assert(refits.back()->findChild<QComboBox*>("refitTargetDesign")->findText("Disposable")==-1);
    MainWindowTestAccess::rejectedQueuePacket();
    MainWindowTestAccess::staleQueuePacket();
    MainWindowTestAccess::remoteQueueEditing(w);
    QTemporaryDir dir;assert(dir.isValid());QString error;SaveGameData save;save.campaignId=1;save.turnToken=1;save.state=s;
    save.pendingOrders={1,{DeleteShipDesignOrder{extra.id},RefitShipOrder{1,s.fleets.front().id,s.shipDesigns[0].id,0,"Deferred target"}}};
    save.pendingDescriptions={"Delete design","Refit ship"};
    auto& p=save.state.planets.front();ProductionItem item{ProductionKind::ShipRefit,5,s.shipDesigns[1].id};item.refit=ShipRefitJob{s.fleets.front().id,s.shipDesigns[0].id,s.shipDesigns[1].id};p.productionQueue={item};
    assert(write_save_game_file(dir.filePath("mini3.suns"),save,error));SaveGameData loaded;const auto loadedOk=read_save_game_file(dir.filePath("mini3.suns"),loaded,error);if(!loadedOk)qWarning("Save load: %s",qPrintable(error));assert(loadedOk);
    assert(loaded.state.shipDesigns.back().retired && loaded.state.planets.front().productionQueue.front().refit==item.refit);
    assert(std::holds_alternative<DeleteShipDesignOrder>(loaded.pendingOrders.orders[0]) && std::holds_alternative<RefitShipOrder>(loaded.pendingOrders.orders[1]));
    TurnOrderFileData packet{1,s.turn,1,save.pendingOrders,save.pendingDescriptions};assert(write_turn_order_file(dir.filePath("mini3.sunsorders"),packet,error));TurnOrderFileData decoded;assert(read_turn_order_file(dir.filePath("mini3.sunsorders"),decoded,error));assert(decoded.orders.orders.size()==2);assert(std::get<RefitShipOrder>(decoded.orders.orders[1]).pendingTargetName=="Deferred target");
}
