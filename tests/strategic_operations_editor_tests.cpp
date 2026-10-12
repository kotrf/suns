#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/reconnaissance.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QDataStream>
#include <QListWidget>
#include <QTextBrowser>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w, const GameState& s) {
        w.state_ = s; w.pendingOrders_ = {1,{}}; w.pendingDescriptions_.clear(); w.selection_.fleet = s.fleets.front().id; w.rebuildScene();
    }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static void messages(MainWindow& w,const std::vector<GameEvent>& events) {
        w.resetTurnMessages(); w.appendTurnMessages(events);
    }
};
}
void settle() { QEventLoop loop; QTimer::singleShot(30,&loop,&QEventLoop::quit); loop.exec(); }

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
    PendingPlayerReport report; report.kind = PlayerReportKind::ScientificData; report.observedTurn = 1; report.deliveryTurn = 3;
    report.quantity = 12; report.researchField = ResearchField::Energy; state.players[0].pendingPlayerReports.push_back(report);
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
    QFile file(path); assert(file.open(QIODevice::ReadWrite)); auto bytes = file.readAll();
    QByteArray magic; QDataStream marker(&magic,QIODevice::WriteOnly); marker << quint32(0x53545241u);
    const auto offset = bytes.indexOf(magic); assert(offset >= 0); bytes[offset] = char(0);
    assert(file.resize(0) && file.seek(0) && file.write(bytes) == bytes.size()); file.close();
    assert(!read_save_game_file(path,rejected,error));

    MainWindow window; window.installDeferredMapSelectionHandler(); attachRouteProgramDock(window);
    window.installUiPolish(); window.installFleetReadabilityPolish(); window.installPanelLayoutFixes();
    MainWindowTestAccess::install(window,make_demo_game()); window.installStrategicOperations(); window.show(); settle();
    auto* task = window.findChild<QComboBox*>("strategicTaskCombo");
    auto* assign = window.findChild<QPushButton*>("assignStrategicTask");
    auto* mode = window.findChild<QComboBox*>("fleetEmissionModeCombo");
    auto* setMode = window.findChild<QPushButton*>("setFleetEmissionMode");
    assert(task && assign && mode && setMode);
    task->setCurrentIndex(task->findData(int(FleetTask::FieldResearch))); settle(); assert(assign->isEnabled()); assign->click();
    mode->setCurrentIndex(2); settle(); setMode->click();
    const auto& pending = MainWindowTestAccess::orders(window);
    assert(std::any_of(pending.orders.begin(),pending.orders.end(),[](const auto& o) { return std::holds_alternative<SetFleetTaskOrder>(o); }));
    assert(std::any_of(pending.orders.begin(),pending.orders.end(),[](const auto& o) { return std::holds_alternative<SetFleetElectronicsOrder>(o); }));
    task->setCurrentIndex(task->findData(int(FleetTask::Bombardment))); settle(); assert(!assign->isEnabled());
    window.installTurnMessages();
    auto* messages = window.findChild<QListWidget*>("turnMessagesList");
    auto* messageBody = window.findChild<QTextBrowser*>("turnMessageBody");
    assert(messages && messageBody);
    for (const auto kind : {GameEventKind::Bombardment,GameEventKind::MineStrike,
                           GameEventKind::ScientificData,GameEventKind::EmissionDetected}) {
        GameEvent e; e.id = 100+std::uint64_t(kind); e.recipient = 1; e.kind = kind;
        e.observedTurn = 1; e.turn = 2; e.position = {80,120}; e.quantity = 12;
        MainWindowTestAccess::messages(window,{e}); messages->setCurrentRow(0); settle();
        const auto text = messageBody->toPlainText();
        assert(text.contains("Observed turn 1; received turn 2") && !text.contains("Natural wormholes"));
        if (kind == GameEventKind::ScientificData) assert(text.contains("research points have been delivered"));
        if (kind == GameEventKind::EmissionDetected) assert(text.contains("unverified"));
    }

    auto* interval=window.findChild<QSpinBox*>("emissionBurstInterval");
    auto* scan=window.findChild<QCheckBox*>("activeEmissionBurst");
    auto* arrival=window.findChild<QCheckBox*>("emissionWakeOnArrival");
    assert(interval && scan && arrival); interval->setValue(2); settle();
    assert(scan->isEnabled()); scan->setChecked(true); arrival->setChecked(true); setMode->click();
    const auto& program=std::get<SetFleetElectronicsOrder>(MainWindowTestAccess::orders(window).orders.back()).program;
    assert(program.burstInterval==2 && program.firstBurstTurn==3 && program.activeBurst && program.wakeOnArrival);
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
    window.close(); settle();
}
