#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/mobile_base.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDataStream>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
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
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); MainWindow w; w.installMobileBases(); MainWindowTestAccess::setup(w);
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
}
