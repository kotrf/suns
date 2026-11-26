#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/mobile_base.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <algorithm>
#include <cassert>
using namespace suns;
using C=ShipComponentType;
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w) {
        w.state_=make_demo_game(); w.state_.fleets.resize(1); w.state_.players[0].technology.levels.fill(26);
        w.state_.shipDesigns.push_back({200,1,"Expedition",ShipHullType::ExpeditionBase,
            {C::QuickJump5,C::QuickJump5,C::QuickJump5,C::QuickJump5,C::ExpeditionDock,C::ExpeditionWorkshop}});
        auto& base=w.state_.fleets[0]; base.id=1; base.name="Base"; base.design=200; base.ships={{200,1}};
        base.position={0,0}; base.destination.reset(); base.pendingCommands.clear(); base.fuel=500; base.minerals={10,5,5};
        base.telemetry={}; base.telemetry.observedTurn=w.state_.turn; base.telemetry.position=base.position;
        base.telemetry.ships=base.ships; base.telemetry.fuel=base.fuel; base.telemetry.minerals=base.minerals;
        auto client=base; client.id=2; client.name="Client"; client.design=kScoutDesignId; client.ships={{kScoutDesignId,5}};
        client.damagePercent=50; client.telemetry.ships=client.ships; client.telemetry.damagePercent=50;
        w.state_.fleets.push_back(client); w.state_.nextFleetId=3;
        w.pendingOrders_={1,{}}; w.pendingDescriptions_.clear(); w.selection_.fleet=2; w.rebuildScene();
    }
    static void hidden(MainWindow& w) { auto& b=w.state_.fleets[0]; b.fuel=1; b.minerals={0,0,0}; b.mobileBase.enabled=false; b.lastMovedTurn=999; w.rebuildScene(); }
    static void gates(MainWindow& w) {
        setup(w);
        w.state_.shipDesigns.back().components = {
            C::QuickJump5, C::QuickJump5, C::QuickJump5, C::QuickJump5, C::ExpeditionStargate};
        auto& station = w.state_.orbitalStations.front();
        station.hull = OrbitalStationHullType::Starbase;
        station.components = {C::JumpGate};
        set_survey_level(w.state_, 1, 1, SurveyLevel::DeepSurvey, w.state_.turn);
        auto& client = w.state_.fleets[1];
        client.ships = {{kScoutDesignId, 1}}; client.telemetry.ships = client.ships;
        client.fuel = fleet_fuel_capacity(w.state_, client); client.telemetry.fuel = client.fuel;
        client.minerals = {}; client.telemetry.minerals = {};
        auto remote = w.state_.fleets[0];
        remote.id = 3; remote.name = "Forward base";
        remote.position = {90, 0}; remote.telemetry.position = remote.position;
        w.state_.fleets.push_back(remote); w.state_.nextFleetId = 4;
        ++w.planningRevision_; w.rebuildScene();
    }
    static void transit(MainWindow& w) { w.openGateTransit(); }
    static QString reportText(MainWindow& w, const GameEvent& report) {
        w.installTurnMessages();
        w.turnMessages_ = {report};
        w.refreshTurnMessages();
        w.turnMessagesList_->setCurrentRow(0);
        return w.turnMessageBody_->toPlainText();
    }
    static const auto& orders(const MainWindow& w) {return w.pendingOrders_.orders;}
    static GameState state(const MainWindow& w) {return w.state_;}
};
}
namespace {
QByteArray bytes(const QString& p) {QFile f(p); assert(f.open(QIODevice::ReadOnly)); return f.readAll();}
void persistence(const GameState& initial)
{
    QTemporaryDir dir; assert(dir.isValid()); QString error;
    SaveGameData host; host.state=initial; host.mode=SessionMode::Host; host.playerTokens[1]=77; host.campaignId=9; host.turnToken=8; host.pendingOrders={1,{}};
    auto& base=host.state.fleets[0]; base.mobileBase={true,170.25,{2,999}}; base.lastMovedTurn=1;
    base.telemetry.mobileBase={false,77,{999}}; base.telemetry.lastMovedTurn=0;
    PendingFleetCommand command; command.issuedTurn=1; command.deliveryTurn=4; command.mobileBase=MobileBaseProgram{true,80,{2}};
    base.pendingCommands.push_back(command);
    PendingFleetTelemetry telemetry; telemetry.deliveryTurn=5; telemetry.telemetry=base.telemetry; telemetry.telemetry.mobileBase.fuelReserve=91;
    base.telemetryInTransit.push_back(telemetry);
    auto lost=base; lost.id=3; auto transit=base; transit.id=4;
    host.state.pendingFleetLossContacts.push_back({lost,7});
    host.state.wormholeTransits.push_back({transit,1,1,2,3}); host.state.nextFleetId=5;
    host.state.pendingMobileBaseTransfers.push_back({1,1,2,1,1,7,0,{2,0,0},0,0,false});
    host.pendingOrders.orders={SetMobileBaseOrder{1,{true,123,{2}}},TransferFleetFuelOrder{2,1,10.25,15.5}};
    host.pendingDescriptions={"Policy","Fuel"};
    const auto path=dir.filePath("mobile.suns"); SaveGameData loaded;
    assert(write_save_game_file(path,host,error)); assert(read_save_game_file(path,loaded,error));
    const auto& b=loaded.state.fleets[0];
    assert(b.mobileBase==base.mobileBase && b.telemetry.mobileBase==base.telemetry.mobileBase && b.lastMovedTurn==1);
    assert(b.pendingCommands[0].mobileBase==command.mobileBase && b.telemetryInTransit[0].telemetry.mobileBase.fuelReserve==91);
    assert(loaded.state.pendingFleetLossContacts[0].lastContact.mobileBase==base.mobileBase);
    assert(loaded.state.wormholeTransits[0].lastContact.telemetry.mobileBase==base.telemetry.mobileBase);
    assert(loaded.state.pendingMobileBaseTransfers.size()==1 && loaded.state.pendingMobileBaseTransfers[0].minerals.ironium==2);
    TurnOrderFileData orders{9,1,77,host.pendingOrders,host.pendingDescriptions}, imported;
    const auto packet=dir.filePath("mobile.sunsorders"); assert(write_turn_order_file(packet,orders,error) && read_turn_order_file(packet,imported,error));
    assert(std::get<SetMobileBaseOrder>(imported.orders.orders[0]).program.clients==std::vector<FleetId>{2});
    const auto fuel=std::get<TransferFleetFuelOrder>(imported.orders.orders[1]); assert(fuel.amount==10.25 && fuel.sourceReserve==15.5);
    // Mutating physical state cannot change a player's exported bytes.
    host.state.pendingFleetLossContacts.clear(); host.state.wormholeTransits.clear();
    const auto a=dir.filePath("a.sunsturn"), c=dir.filePath("b.sunsturn");
    assert(make_player_turn(host,1).state.pendingMobileBaseTransfers.empty());
    assert(write_save_game_file(a,make_player_turn(host,1),error));
    host.state.pendingMobileBaseTransfers[0].minerals.ironium=999;
    base.mobileBase={false,9999,{12345}}; base.lastMovedTurn=999; base.lastServiceTurn=999; base.minerals={0,0,0}; base.fuel=0;
    assert(write_save_game_file(c,make_player_turn(host,1),error)); assert(bytes(a)==bytes(c));
    // Corrupt extension headers are rejected rather than silently dropping policies.
    auto damaged=bytes(path); QByteArray tag; QDataStream tagOut(&tag,QIODevice::WriteOnly); tagOut<<quint32{0x4d4f4249u};
    const auto offset=damaged.indexOf(tag); assert(offset>=0); damaged[offset]='X';
    const auto corrupt=dir.filePath("corrupt.suns"); QFile f(corrupt); assert(f.open(QIODevice::WriteOnly)); f.write(damaged); f.close();
    assert(!read_save_game_file(corrupt,loaded,error));
}

void gate_persistence(GameState state)
{
    QTemporaryDir dir;
    QString error;
    SaveGameData host;
    host.mode = SessionMode::Host;
    host.state = std::move(state);
    host.playerTokens[1] = 77;
    host.campaignId = 9;
    host.turnToken = 8;
    const auto issued = host.state.turn;
    const MobileGateOrder remote{2, {{0, 1, {900, 0}}, {0, 3, {1000, 0}}}};
    for (std::size_t i = 0; i < host.state.fleets.size(); ++i) {
        auto& fleet = host.state.fleets[i];
        fleet.position = i < 2 ? remote.transit.source.position : remote.transit.destination.position;
        fleet.telemetry.position = fleet.position;
    }
    auto& traveler = host.state.fleets[1];
    assert(submit_mobile_gate_command(host.state, 1, remote));
    assert(traveler.pendingCommands.size() == 1);
    const auto delivery = traveler.pendingCommands.front().deliveryTurn;
    host.pendingOrders = {1, {remote}};
    host.pendingDescriptions = {"Expedition Stargate"};
    const auto path = dir.filePath("gates.suns");
    SaveGameData loaded;
    assert(write_save_game_file(path, host, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets[1].pendingCommands.front().mobileGate == remote.transit);
    assert(std::get<MobileGateOrder>(loaded.pendingOrders.orders.front()).transit == remote.transit);

    TurnOrderFileData packet{9, issued, 77, host.pendingOrders, host.pendingDescriptions}, imported;
    const auto orderPath = dir.filePath("gate.sunsorders");
    assert(write_turn_order_file(orderPath, packet, error) && read_turn_order_file(orderPath, imported, error));
    assert(std::get<MobileGateOrder>(imported.orders.orders.front()).transit == remote.transit);
    QFile oldOrders(orderPath);
    assert(oldOrders.open(QIODevice::ReadWrite) && oldOrders.seek(4));
    QDataStream orderHeader(&oldOrders); orderHeader << quint32{60}; oldOrders.close();
    assert(!read_turn_order_file(orderPath, imported, error));

    const auto exportPath = dir.filePath("gate.sunsturn");
    auto view = make_player_turn(host, 1);
    assert(view.state.fleets[1].pendingCommands.empty());
    assert(write_save_game_file(exportPath, view, error));
    const auto sentView = bytes(exportPath);
    host.state.turn = delivery;
    deliver_due_fleet_commands(host.state);
    assert(traveler.mobileGate == remote.transit && traveler.pendingCommands.empty());
    assert(write_save_game_file(path, host, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets[1].mobileGate == remote.transit);
    const auto result = TurnProcessor{}.process(loaded.state, {});
    assert(result.fleets[1].position == remote.transit.destination.position && !result.fleets[1].mobileGate);
    auto reportsSave = host;
    reportsSave.state = result;
    assert(write_save_game_file(path, reportsSave, error) && read_save_game_file(path, loaded, error));
    const auto& pending = loaded.state.players.front().pendingPlayerReports;
    const auto report = std::find_if(pending.begin(), pending.end(), [](const auto& r) {
        return r.kind == PlayerReportKind::MobileStargateTransit;
    });
    assert(report != pending.end() && report->quantity == 200 && report->position == remote.transit.destination.position);
    host.state.turn = issued;
    view = make_player_turn(host, 1);
    assert(!view.state.fleets[1].mobileGate);
    assert(write_save_game_file(exportPath, view, error) && bytes(exportPath) == sentView);

    host.state = make_demo_game();
    host.pendingOrders.orders.clear(); host.pendingDescriptions.clear();
    assert(write_save_game_file(path, host, error));
    auto previous = bytes(path);
    const auto extension = previous.lastIndexOf(QByteArray("MGAT"));
    assert(extension >= 0 && quint8(previous[7]) >= 135);
    previous.truncate(extension);
    previous[7] = char(134);
    QFile legacy(path);
    assert(legacy.open(QIODevice::WriteOnly | QIODevice::Truncate));
    assert(legacy.write(previous) == previous.size()); legacy.close();
    assert(read_save_game_file(path, loaded, error) && !loaded.state.fleets.front().mobileGate);
}

void gate_result_messages(MainWindow& w, const MobileGateOrder& jump)
{
    auto state = MainWindowTestAccess::state(w);
    const PlayerOrders orders{1, {jump}};
    const auto success = TurnProcessor{}.process_with_events(state, {orders});
    const auto jumped = std::find_if(success.events.begin(), success.events.end(), [](const auto& e) {
        return e.kind == GameEventKind::MobileStargateTransit;
    });
    assert(jumped != success.events.end() && jumped->quantity == 200 && !jumped->planet);
    auto& receiver = state.fleets.back();
    receiver.fuel = receiver.mobileBase.fuelReserve + 5;
    const auto failure = TurnProcessor{}.process_with_events(state, {orders});
    const auto denied = std::find_if(failure.events.begin(), failure.events.end(), [](const auto& e) {
        return e.kind == GameEventKind::MobileStargateTransitFailed;
    });
    assert(denied != failure.events.end() && denied->quantity == 0);

    SaveGameData host;
    host.mode = SessionMode::Host;
    host.state = success.state;
    host.playerTokens[1] = 77;
    host.campaignId = 9;
    host.turnToken = 8;
    host.pendingOrders = {1, {}};
    host.strategicMessages = {*jumped, *denied};
    QTemporaryDir dir;
    QString error;
    const auto path = dir.filePath("reports.sunsturn");
    SaveGameData read;
    assert(write_save_game_file(path, make_player_turn(host, 1), error) && read_save_game_file(path, read, error));
    assert(read.strategicMessages.size() == 2);
    const auto successText = MainWindowTestAccess::reportText(w, read.strategicMessages[0]);
    assert(successText.contains("(90.0, 0.0) ly") && successText.contains("spent 20 mg of fuel"));
    const auto failureText = MainWindowTestAccess::reportText(w, read.strategicMessages[1]);
    assert(failureText.contains("spent no fuel") && failureText.contains("made one attempt"));
    assert(failureText.contains("Send a new transit order"));
}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); MainWindow w; w.installMobileBases();
    w.findChild<QDockWidget*>("mobileBasesDock")->show();
    MainWindowTestAccess::setup(w);
    auto* bases=w.findChild<QComboBox*>("mobileBaseSelection"); auto* queue=w.findChild<QListWidget*>("mobileBaseClients");
    auto* clients=w.findChild<QComboBox*>("mobileBaseClientSelection"); auto* stocks=w.findChild<QLabel*>("mobileBaseStocks");
    assert(bases && bases->count()==1 && clients->count()==1 && queue->count()==0);
    w.findChild<QPushButton*>("mobileBaseAddClient")->click(); assert(queue->count()==1);
    w.findChild<QDoubleSpinBox*>("mobileBaseReserve")->setValue(170.25);
    w.findChild<QPushButton*>("mobileBaseApply")->click();
    const auto& order=std::get<SetMobileBaseOrder>(MainWindowTestAccess::orders(w).back());
    assert(order.fleet==1 && order.program.fuelReserve==170.25 && order.program.clients==std::vector<FleetId>{2});
    const auto text=stocks->text(); const auto forecast=w.findChild<QLabel*>("mobileBaseForecast")->text();
    const auto initial=MainWindowTestAccess::state(w); MainWindowTestAccess::hidden(w);
    assert(stocks->text()==text && w.findChild<QLabel*>("mobileBaseForecast")->text()==forecast);
    w.findChild<QPushButton*>("mobileBaseApproach")->click();
    assert(std::get<MoveFleetOrder>(MainWindowTestAccess::orders(w).back()).fleet==2);
    persistence(initial);

    MainWindowTestAccess::gates(w);
    assert(w.findChild<QPushButton*>("mobileBaseApply")->isEnabled());
    assert(w.findChild<QLabel*>("mobileBaseForecast")->text().contains("Expedition Stargate"));
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "stargateTransitDialog");
        auto* sources = dialog->findChild<QComboBox*>("stargateSources");
        auto* targets = dialog->findChild<QComboBox*>("stargateDestinations");
        assert(sources->count() == 2 && targets->count() == 1 && targets->currentText().contains("Forward base"));
        sources->setCurrentIndex(1);
        assert(targets->count() == 1 && targets->currentData().toUInt() == 3);
        assert(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        dialog->accept();
    });
    MainWindowTestAccess::transit(w);
    const auto jump = std::get<MobileGateOrder>(MainWindowTestAccess::orders(w).back());
    assert(jump.fleet == 2 && jump.transit.source.base == 1 && jump.transit.destination.base == 3);
    gate_result_messages(w, jump);
    assert(w.clearSelectedFleetRouteProgram());
    assert(MainWindowTestAccess::orders(w).size() == 1 && std::get<MoveFleetOrder>(MainWindowTestAccess::orders(w).front()).clearRoute);
    gate_persistence(MainWindowTestAccess::state(w));
}
