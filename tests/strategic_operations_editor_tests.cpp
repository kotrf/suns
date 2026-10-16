#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/communications.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QTreeWidget>
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QImage>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QDataStream>
#include <QListWidget>
#include <QTextBrowser>
#include <QStyle>
#include <QStyleOptionButton>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w, const GameState& s) {
        w.state_ = s; w.pendingOrders_ = {1,{}}; w.pendingDescriptions_.clear(); w.selection_.fleet = s.fleets.front().id; w.rebuildScene();
        emit w.routeProgramContextChanged(true); // Match load/new-game session replacement.
    }
    static bool selectSystem(MainWindow& w, StarId star) { return w.selectWorkspaceObject(1,star); }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static void messages(MainWindow& w,const std::vector<GameEvent>& events) {
        w.resetTurnMessages(); w.appendTurnMessages(events);
    }
};
}
void settle() { QEventLoop loop; QTimer::singleShot(30,&loop,&QEventLoop::quit); loop.exec(); }

int brightIndicatorPixels(QCheckBox& box)
{
    QStyleOptionButton option; option.initFrom(&box); option.text=box.text();
    const auto indicator=box.style()->subElementRect(QStyle::SE_CheckBoxIndicator,&option,&box);
    QImage image(box.size(),QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    box.render(&image);
    int count=0;
    for(int y=indicator.top()+2;y<indicator.bottom()-1;++y)
        for(int x=indicator.left()+2;x<indicator.right()-1;++x) {
            const auto c=image.pixelColor(x,y);
            if(c.red()>=200 && c.green()>=210 && c.blue()>=220) ++count;
        }
    return count;
}

int main(int argc,char** argv)
{
    QApplication app(argc,argv); QTemporaryDir dir; assert(dir.isValid());
    QCoreApplication::setOrganizationName("SunsStrategicTests"); QCoreApplication::setApplicationName("Strategic operations");
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
    using namespace suns;
    SaveGameData value; value.campaignId = 1; value.turnToken = 1; value.state = make_demo_game(); value.pendingOrders = {1,{}};
    auto& state = value.state; auto& fleet = state.fleets.front();
    fleet.task = FleetTask::FieldResearch; fleet.telemetry.task = FleetTask::FieldResearch;
    fleet.electronics = {EmissionMode::RadioSilence,state.turn+5}; fleet.telemetry.electronics = {EmissionMode::Passive,0};
    fleet.pendingCommands.push_back({state.turn,state.turn+5,{}, {},ElectronicsProgram{EmissionMode::Standard,0}});
    state.minefields = {{1,1,{100,100},80,2}}; state.nextMinefieldId = 2;
    state.wrecks = {{1,{200,200},20,{0,0,0,0,0,5}}}; state.nextWreckId = 2;
    state.players[0].fieldScience = {{(1ULL<<32)|1,state.turn+2}};
    state.players[0].strategicIntel = {{1,StrategicObjectKind::Minefield,{100,100},std::sqrt(80),1,1,1,80,2}};
    state.players[0].pendingStrategicIntel = {{1,StrategicObjectKind::Wreck,{200,200},10,0,1,3,20,0}};
    ReconContact radar; radar.fleet=fleet.id; radar.owner=1; radar.position={12,0};
    radar.quality=ContactQuality::Confirmed; radar.sourceColony=1;
    radar.observedTurn=radar.lastSeenTurn=radar.deliveryTurn=1;
    state.players[0].friendlyFleetContacts={radar};
    auto pendingRadar=radar; pendingRadar.sourceColony=0; pendingRadar.sourceFleet=99; pendingRadar.deliveryTurn=3;
    state.players[0].pendingFriendlyFleetContacts={pendingRadar};
    PendingPlayerReport report; report.kind = PlayerReportKind::ScientificData; report.observedTurn = 1; report.deliveryTurn = 3;
    report.quantity = 12; report.researchField = ResearchField::Energy; state.players[0].pendingPlayerReports.push_back(report);
    auto unsent=report; unsent.kind=PlayerReportKind::FleetArrived; unsent.quantity=0;
    unsent.deliveryTurn=kAwaitingTransmission; unsent.transmissionFleet=fleet.id;
    state.players[0].pendingPlayerReports.push_back(unsent);
    state.players[0].fieldScience.push_back({(1ULL<<32)|2,kAwaitingTransmission,fleet.id});
    auto lost=fleet_player_view(state,fleet); lost.id=state.nextFleetId++;
    auto sent=fleet.telemetry; sent.fuel=123; sent.damagePercent=7;
    lost.telemetryInTransit.push_back({state.turn+2,sent});
    state.pendingFleetLossContacts.push_back({lost,state.turn+4});
    value.pendingOrders.orders = {SetFleetTaskOrder{fleet.id,FleetTask::LayMines},SetFleetElectronicsOrder{fleet.id,{EmissionMode::RadioSilence,6}}};
    value.pendingDescriptions = {"Lay mines","Radio silence"};
    QString error; const auto path = dir.filePath("strategy.suns");
    assert(write_save_game_file(path,value,error)); SaveGameData restored;
    if (!read_save_game_file(path,restored,error)) std::cerr << error.toStdString() << '\n';
    assert(error.isEmpty());
    const auto& loaded = restored.state;
    assert(loaded.minefields[0].kind == 2 && loaded.minefields[0].mines == 80 && loaded.wrecks[0].technology[5] == 5);
    assert(loaded.fleets[0].electronics.resumeTurn == 6 && loaded.fleets[0].telemetry.electronics.mode == EmissionMode::Passive);
    assert(loaded.fleets[0].pendingCommands.back().electronics && loaded.fleets[0].task == FleetTask::FieldResearch);
    assert(loaded.players[0].fieldScience[0].deliveryTurn == 3 && loaded.players[0].pendingStrategicIntel[0].deliveryTurn == 3);
    assert(loaded.players[0].friendlyFleetContacts.front().sourceColony==1);
    assert(loaded.players[0].pendingFriendlyFleetContacts.front().deliveryTurn==3
        && loaded.players[0].pendingFriendlyFleetContacts.front().sourceFleet==99);
    assert(loaded.players[0].pendingPlayerReports.back().transmissionFleet==fleet.id
        && loaded.players[0].pendingPlayerReports.back().deliveryTurn==kAwaitingTransmission);
    assert(loaded.pendingFleetLossContacts.size()==1);
    const auto& sentAfterLoss=loaded.pendingFleetLossContacts.front().lastContact.telemetryInTransit;
    assert(sentAfterLoss.size()==1 && sentAfterLoss.front().deliveryTurn==3
        && sentAfterLoss.front().telemetry.fuel==123 && sentAfterLoss.front().telemetry.damagePercent==7);
    auto transmitting=loaded; apply_electronics_program(transmitting.fleets.front(),{});
    dispatch_fleet_report_outbox(transmitting);
    assert(transmitting.players[0].pendingPlayerReports.back().deliveryTurn==transmitting.turn
        && !transmitting.players[0].pendingPlayerReports.back().transmissionFleet);
    assert(transmitting.players[0].fieldScience.back().deliveryTurn==transmitting.turn);
    assert(std::holds_alternative<SetFleetTaskOrder>(restored.pendingOrders.orders[0]));
    TurnOrderFileData packet; packet.orders = value.pendingOrders; packet.descriptions = value.pendingDescriptions;
    packet.campaignId = 1; packet.turn = 1; packet.turnToken = 1;
    const auto ordersPath = dir.filePath("strategy.orders"); assert(write_turn_order_file(ordersPath,packet,error));
    TurnOrderFileData readPacket; assert(read_turn_order_file(ordersPath,readPacket,error));
    assert(std::get<SetFleetElectronicsOrder>(readPacket.orders.orders[1]).program.resumeTurn == 6);
    // Invalid enum/policy data and corrupt extension magic must fail loading.
    auto invalid = value; invalid.state.fleets[0].electronics.mode = EmissionMode(255);
    assert(write_save_game_file(dir.filePath("bad.suns"),invalid,error));
    SaveGameData rejected; assert(!read_save_game_file(dir.filePath("bad.suns"),rejected,error));
    auto invalidOutbox=value; invalidOutbox.state.players[0].pendingPlayerReports.back().transmissionFleet=0;
    assert(write_save_game_file(dir.filePath("bad-outbox.suns"),invalidOutbox,error));
    assert(!read_save_game_file(dir.filePath("bad-outbox.suns"),rejected,error));
    auto invalidRadar=value; invalidRadar.state.players[0].friendlyFleetContacts.front().owner=2;
    assert(write_save_game_file(dir.filePath("bad-radar.suns"),invalidRadar,error));
    assert(!read_save_game_file(dir.filePath("bad-radar.suns"),rejected,error));
    auto duplicateRadar=value; duplicateRadar.state.players[0].friendlyFleetContacts.push_back(radar);
    assert(write_save_game_file(dir.filePath("duplicate-radar.suns"),duplicateRadar,error));
    assert(!read_save_game_file(dir.filePath("duplicate-radar.suns"),rejected,error));
    QFile file(path); assert(file.open(QIODevice::ReadWrite)); auto bytes = file.readAll();
    QByteArray magic; QDataStream marker(&magic,QIODevice::WriteOnly); marker << quint32(0x53545241u);
    const auto offset = bytes.indexOf(magic); assert(offset >= 0); bytes[offset] = char(0);
    assert(file.resize(0) && file.seek(0) && file.write(bytes) == bytes.size()); file.close();
    assert(!read_save_game_file(path,rejected,error));

    for (auto kind : {FleetArrivalActionKind::LayMinefields,FleetArrivalActionKind::SweepMinefields,
             FleetArrivalActionKind::BombardColony,FleetArrivalActionKind::FieldResearch,FleetArrivalActionKind::RecoverWreckage}) {
        SaveGameData work; work.campaignId=1; work.turnToken=1; work.state=make_demo_game();
        auto& carrier=work.state.fleets.front(); carrier.destination=Position{32,0};
        carrier.waypointQueue={{{64,0},4,{kind}}};
        carrier.telemetry.destination=carrier.destination; carrier.telemetry.waypointQueue=carrier.waypointQueue;
        carrier.telemetryInTransit.push_back({work.state.turn+2,carrier.telemetry});
        carrier.pendingCommands.push_back({work.state.turn,work.state.turn+2,{{16,0},4,{},{{{64,0},4,{kind}}}}});
        MoveFleetOrder route{carrier.id,{16,0},4,{},{{{64,0},4,{kind}}}};
        work.pendingOrders={1,{route}}; work.pendingDescriptions={"Waypoint work"};
        const auto save=dir.filePath("work.suns");
        assert(write_save_game_file(save,work,error)); SaveGameData roundtrip;
        assert(read_save_game_file(save,roundtrip,error));
        const auto& restored=roundtrip.state.fleets.front();
        assert(restored.waypointQueue.back().arrivalAction.kind==kind);
        assert(restored.telemetry.waypointQueue.back().arrivalAction.kind==kind);
        assert(restored.telemetryInTransit.back().telemetry.waypointQueue.back().arrivalAction.kind==kind);
        assert(restored.pendingCommands.back().program.queuedWaypoints.back().arrivalAction.kind==kind);
        assert(std::get<MoveFleetOrder>(roundtrip.pendingOrders.orders.front()).queuedWaypoints.back().arrivalAction.kind==kind);
        TurnOrderFileData packet{1,work.state.turn,1,work.pendingOrders,work.pendingDescriptions}, received;
        const auto orders=dir.filePath("work.orders");
        assert(write_turn_order_file(orders,packet,error) && read_turn_order_file(orders,received,error));
        assert(std::get<MoveFleetOrder>(received.orders.orders.front()).queuedWaypoints.back().arrivalAction.kind==kind);
        auto host=work; host.mode=SessionMode::Host; host.playerTokens={{1,1}};
        auto exported=make_player_turn(host,1);
        assert(write_save_game_file(save,exported,error) && read_save_game_file(save,roundtrip,error));
        assert(roundtrip.state.fleets.front().telemetry.waypointQueue.back().arrivalAction.kind==kind);
        // A persistent intermediate waypoint must not enter saves or PBEM orders.
        carrier.arrivalAction=FleetArrivalAction{kind};
        assert(write_save_game_file(save,work,error) && !read_save_game_file(save,roundtrip,error));
        auto& invalidRoute=std::get<MoveFleetOrder>(packet.orders.orders.front()); invalidRoute.arrivalAction={kind};
        assert(write_turn_order_file(orders,packet,error) && !read_turn_order_file(orders,received,error));
    }

    MainWindow window; window.installDeferredMapSelectionHandler(); attachRouteProgramDock(window);
    window.installUiPolish(); window.installFleetReadabilityPolish(); window.installPanelLayoutFixes();
    MainWindowTestAccess::install(window,make_demo_game()); window.installStrategicOperations(); window.show(); settle();
    assert(!window.findChild<QComboBox*>("strategicTaskCombo"));
    assert(!window.findChild<QPushButton*>("assignStrategicTask"));
    auto* action = window.findChild<QComboBox*>("routeArrivalActionCombo");
    auto* add = window.findChild<QPushButton*>("routeAddButton");
    auto* mode = window.findChild<QComboBox*>("fleetEmissionModeCombo");
    auto* setMode = window.findChild<QPushButton*>("setFleetEmissionMode");
    auto* stop = window.findChild<QPushButton*>("routeStopButton");
    assert(action && add && mode && setMode && stop);
    for (auto kind : {FleetArrivalActionKind::LayMinefields,FleetArrivalActionKind::SweepMinefields,
             FleetArrivalActionKind::BombardColony,FleetArrivalActionKind::FieldResearch,FleetArrivalActionKind::RecoverWreckage})
        assert(action->findData(int(kind))>=0);
    assert(MainWindowTestAccess::selectSystem(window,1)); settle();
    action->setCurrentIndex(action->findData(int(FleetArrivalActionKind::FieldResearch))); settle();
    assert(add->isEnabled()); add->click(); settle();
    mode->setCurrentIndex(2); settle(); setMode->click();
    const auto& pending = MainWindowTestAccess::orders(window);
    assert(std::any_of(pending.orders.begin(),pending.orders.end(),[](const auto& o) {
        const auto* route=std::get_if<MoveFleetOrder>(&o);
        return route && route->arrivalAction.kind==FleetArrivalActionKind::FieldResearch;
    }));
    assert(std::any_of(pending.orders.begin(),pending.orders.end(),[](const auto& o) { return std::holds_alternative<SetFleetElectronicsOrder>(o); }));
    assert(!window.setSelectedFleetRouteProgramLegAction(0,{FleetArrivalActionKind::BombardColony}));
    assert(window.selectedFleetRouteProgramLegAction(0)->kind==FleetArrivalActionKind::FieldResearch);
    window.installTurnMessages();
    auto* messages = window.findChild<QListWidget*>("turnMessagesList");
    auto* messageBody = window.findChild<QTextBrowser*>("turnMessageBody");
    assert(messages && messageBody);
    for (const auto kind : {GameEventKind::Bombardment,GameEventKind::MineStrike,
                           GameEventKind::ScientificData,GameEventKind::EmissionDetected}) {
        GameEvent e; e.id = 100+std::uint64_t(kind); e.recipient = 1; e.kind = kind;
        e.observedTurn = 1; e.turn = 2; e.position = {80,120}; e.quantity = 12;
        if(kind==GameEventKind::EmissionDetected) e.emission=EmissionObservation{1,SignalChannel::Slow};
        MainWindowTestAccess::messages(window,{e}); messages->setCurrentRow(0); settle();
        const auto text = messageBody->toPlainText();
        assert(text.contains("Observed turn 1; received turn 2") && !text.contains("Natural wormholes"));
        if (kind == GameEventKind::ScientificData) assert(text.contains("research points have been delivered"));
        if (kind == GameEventKind::EmissionDetected) assert(text.contains("unverified") && text.contains("Slow channel") && text.contains("Emitted year 1; intercepted year 1; received year 2") && text.contains("does not track"));
    }

    auto* interval=window.findChild<QSpinBox*>("emissionBurstInterval");
    auto* scan=window.findChild<QCheckBox*>("activeEmissionBurst");
    auto* arrival=window.findChild<QCheckBox*>("emissionWakeOnArrival");
    assert(interval && scan && arrival); interval->setValue(2); settle();
    assert(scan->isEnabled()); scan->setChecked(true); arrival->setChecked(true); setMode->click();
    const auto& program=std::get<SetFleetElectronicsOrder>(MainWindowTestAccess::orders(window).orders.back()).program;
    assert(program.burstInterval==2 && program.firstBurstTurn==3 && program.activeBurst && program.wakeOnArrival);
    // Check the rendered marks, including a checked option that becomes
    // unavailable after changing emission mode. Empty boxes remain distinct.
    for(auto* check:{scan,arrival}) {
        check->setChecked(true); check->setEnabled(true);
        assert(brightIndicatorPixels(*check)>=8);
        check->setEnabled(false);
        assert(brightIndicatorPixels(*check)>=8);
        check->setChecked(false);
        assert(brightIndicatorPixels(*check)==0);
        check->setChecked(true); check->setEnabled(true);
    }
    interval->setValue(20); settle(); assert(!setMode->isEnabled()); interval->setValue(2); settle();
    auto intel=make_demo_game(); intel.turn=5;
    ReconContact c; c.fleet=22; c.owner=2; c.position={200,200}; c.quality=ContactQuality::Identified;
    c.observedTurn=c.lastSeenTurn=3; c.deliveryTurn=4; c.sourceFleet=1; c.massMinimum=150; c.massMaximum=250;
    c.shipCount=3; c.hulls={ShipHullType::SmallFreighter}; intel.players[0].enemyContacts={c};
    auto uncertain=c; uncertain.fleet=23; uncertain.owner=0; uncertain.quality=ContactQuality::Probable;
    uncertain.shipCount=0; uncertain.hulls.clear(); uncertain.uncertainty=7.5; uncertain.position={300,300};
    uncertain.observedTurn=uncertain.lastSeenTurn=uncertain.deliveryTurn=5; intel.players[0].enemyContacts.push_back(uncertain);
    MainWindowTestAccess::install(window,intel);
    auto* open=window.findChild<QPushButton*>("openReconnaissance"); assert(open); open->click(); settle();
    auto* dialog=window.findChild<QDialog*>("intelligenceDialog"); auto* table=window.findChild<QTableWidget*>("reconnaissanceTable");
    assert(dialog && table && table->rowCount()==2);
    assert(table->item(0,2)->text()=="Stale" && table->item(0,5)->text()=="2");
    assert(table->item(0,8)->text().contains("Small Freighter"));
    assert(table->item(1,1)->text()=="Probable" && table->item(1,3)->text()=="Unknown" && table->item(1,8)->text()=="Unknown");
    assert(dialog->grab().save("/tmp/reconnaissance-intelligence.png"));
    GameEvent contact; contact.id=9000; contact.recipient=1; contact.kind=GameEventKind::EnemyFleetReclassified;
    contact.fleet=22; contact.contactOwner=2; contact.observedTurn=3; contact.turn=4; contact.position=c.position; contact.contact=c;
    MainWindowTestAccess::messages(window,{contact}); messages->setCurrentRow(0); settle();
    assert(messageBody->toPlainText().contains("Mass estimate: 150–250 kt") && messageBody->toPlainText().contains("Last position seen in year 3"));
    dialog->hide();
    // A selected Standard draft and a stop order cannot masquerade as a
    // received command cancelling a silent radio.
    auto silent=make_demo_game(); silent.turn=5;
    silent.fleets.front().position={100,0};
    silent.fleets.front().electronics={EmissionMode::RadioSilence,9};
    silent.fleets.front().telemetry.electronics=silent.fleets.front().electronics;
    observe_reconnaissance(silent,5); deliver_reconnaissance(silent);
    MainWindowTestAccess::install(window,silent); settle();
    auto* missionSummary=window.findChild<QLabel*>("strategicMissionSummary"); assert(missionSummary);
    assert(mode->currentIndex()==2 && missionSummary->text().contains("Emissions: Radio silence"));
    mode->setCurrentIndex(0); settle();
    assert(missionSummary->text().contains("selection is a draft"));
    assert(missionSummary->text().contains("Stop cancels work and travel"));
    stop->click(); settle();
    assert(MainWindowTestAccess::orders(window).orders.size()==1
        && std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front()).clearRoute);
    setMode->click(); settle();
    assert(missionSummary->text().contains("Queued emission order: Standard"));
    assert(missionSummary->text().contains("Emissions: Radio silence"));
    // Every work type edits only its selected terminal leg, with emissions separate.
    auto supported=make_demo_game();
    auto& design=supported.shipDesigns.front();
    design.components.insert(design.components.end(),{ShipComponentType::MineDispenser50,
        ShipComponentType::Laser,ShipComponentType::LadyFingerBomb,ShipComponentType::CargoPod});
    supported.players.push_back({2,"Other"}); supported.planets[1].owner=2; supported.planets[1].population=10000;
    set_survey_level(supported,1,supported.stars[1].id,SurveyLevel::OrbitalSurvey,supported.turn);
    supported.players[0].strategicIntel={{1,StrategicObjectKind::Wreck,supported.stars[1].position,10,0,
        supported.turn,supported.turn,20,0}};
    for (auto kind : {FleetArrivalActionKind::LayMinefields,FleetArrivalActionKind::SweepMinefields,
             FleetArrivalActionKind::BombardColony,FleetArrivalActionKind::FieldResearch,FleetArrivalActionKind::RecoverWreckage}) {
        MainWindowTestAccess::install(window,supported); settle();
        // The intermediate waypoint must differ from the source position;
        // No action at the current position is deliberately the Stop intent.
        assert(MainWindowTestAccess::selectSystem(window,supported.stars[2].id));
        assert(window.appendSelectedStarWaypoint(4,{}));
        assert(MainWindowTestAccess::selectSystem(window,supported.stars[1].id));
        assert(window.appendSelectedStarWaypoint(4,{kind})); settle();
        assert(window.selectedFleetRouteProgramLegAction(0)->kind==FleetArrivalActionKind::None);
        assert(window.selectedFleetRouteProgramLegAction(1)->kind==kind);
        assert(!window.setSelectedFleetRouteProgramLegAction(0,{kind}));
        assert(window.selectedFleetRouteProgramLegAction(0)->kind==FleetArrivalActionKind::None);
        assert(!window.appendSelectedStarWaypoint(4,{}));
        assert(!window.setSelectedFleetRepeatOrdersForRouteProgram(true));
        auto* routeTree=window.findChild<QTreeWidget*>("routeProgramQueue"); assert(routeTree);
        routeTree->setCurrentItem(routeTree->topLevelItem(1)); settle();
        assert(action->currentData().toInt()==int(kind));
        assert(window.setSelectedFleetRouteProgramLegAction(1,{}));
        assert(window.selectedFleetRouteProgramLegAction(0)->kind==FleetArrivalActionKind::None);
    }
    auto idle=make_demo_game(); idle.fleets.front().position={200,100};
    idle.fleets.front().telemetry.position=idle.fleets.front().position;
    idle.shipDesigns.front().components.push_back(ShipComponentType::MineDispenser50);
    idle.fleets.front().electronics={EmissionMode::Passive};
    idle.fleets.front().telemetry.electronics=idle.fleets.front().electronics;
    MainWindowTestAccess::install(window,idle); settle();
    auto* here=window.findChild<QPushButton*>("routeWorkHereButton"); assert(here);
    action->setCurrentIndex(action->findData(int(FleetArrivalActionKind::LayMinefields))); settle();
    assert(here->isEnabled()); here->click(); settle();
    const auto& localWork=std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front());
    assert(same_position(localWork.destination,idle.fleets.front().position)
        && localWork.arrivalAction.kind==FleetArrivalActionKind::LayMinefields);
    assert(!window.startSelectedFleetWorkHere({FleetArrivalActionKind::LayMinefields}));
    // A queued stop is not evidence that the moving fleet has stopped already.
    idle.fleets.front().destination=Position{250,100};
    idle.fleets.front().telemetry.destination=idle.fleets.front().destination;
    MainWindowTestAccess::install(window,idle); settle();
    assert(window.clearSelectedFleetRouteProgram());
    assert(!window.startSelectedFleetWorkHere({FleetArrivalActionKind::LayMinefields}));
    assert(std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front()).clearRoute);
    window.close(); settle();
}
