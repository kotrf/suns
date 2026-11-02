#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cassert>
#include <limits>

using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w,const GameState& state) {
        w.state_=state; w.pendingOrders_={1,{}}; w.selection_.star=1; w.selection_.fleet=1; w.rebuildScene();
    }
    static const PlayerOrders& orders(const MainWindow& w) {return w.pendingOrders_;}
    static const GameState& state(const MainWindow& w) {return w.state_;}
};
}
namespace {
QByteArray bytes(const QString& path) {QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
void put(const QString& path,const QByteArray& b) {QFile f(path);assert(f.open(QIODevice::WriteOnly));assert(f.write(b)==b.size());}
void settle() {QEventLoop loop;QTimer::singleShot(30,&loop,&QEventLoop::quit);loop.exec();}
void persistence(const QString& root)
{
    const auto path=root+"/scrap.suns"; QString error; SaveGameData original,loaded;
    original.state=make_demo_game(); original.campaignId=10;original.turnToken=11;
    original.mode=SessionMode::Host;original.playerTokens={{1,12}};
    original.state.wrecks={{1,{0,0},20,{}, {12.5,25,37.5}}};original.state.nextWreckId=2;
    original.state.players[0].strategicIntel={{1,StrategicObjectKind::Wreck,{0,0},10,0,1,1,20,1}};
    original.pendingOrders={1,{MoveFleetOrder{1,{10,20},5,{FleetArrivalActionKind::ScrapFleet}}}};
    original.pendingDescriptions={"Scrap Fleet at waypoint"};
    original.state.fleets[0].destination=Position{10,20};
    original.state.fleets[0].arrivalAction=FleetArrivalAction{FleetArrivalActionKind::ScrapFleet};
    if(!write_save_game_file(path,original,error) || !read_save_game_file(path,loaded,error)) qFatal("%s",qPrintable(error));
    assert(loaded.state.wrecks[0].minerals.ironium==12.5 && loaded.state.wrecks[0].minerals.germanium==37.5);
    assert(loaded.state.fleets[0].arrivalAction->kind==FleetArrivalActionKind::ScrapFleet);
    assert(std::get<MoveFleetOrder>(loaded.pendingOrders.orders[0]).arrivalAction.kind==FleetArrivalActionKind::ScrapFleet);
    auto exported=make_player_turn(original,1);
    assert(exported.state.wrecks.empty() && exported.state.players[0].strategicIntel.size()==1);
    assert(write_save_game_file(path,exported,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.wrecks.empty() && loaded.state.players[0].strategicIntel[0].kind==1);

    TurnOrderFileData packet;packet.campaignId=10;packet.turn=1;packet.turnToken=12;packet.orders=original.pendingOrders;
    packet.descriptions=original.pendingDescriptions;
    const auto ordersPath=root+"/scrap.sunsorders";TurnOrderFileData incoming;
    assert(write_turn_order_file(ordersPath,packet,error) && read_turn_order_file(ordersPath,incoming,error));
    assert(std::get<MoveFleetOrder>(incoming.orders.orders[0]).arrivalAction.kind==FleetArrivalActionKind::ScrapFleet);

    original.pendingOrders={1,{}};original.pendingDescriptions.clear();
    original.state.fleets[0].destination.reset();original.state.fleets[0].arrivalAction.reset();
    assert(write_save_game_file(path,original,error));const auto current=bytes(path);
    QByteArray marker;{QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32{0x53435250};}
    const auto tail=current.lastIndexOf(marker);assert(tail>0);
    auto legacy=current.left(tail);{QDataStream out(&legacy,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{108};}
    put(path,legacy);assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.wrecks.size()==1 && loaded.state.wrecks[0].minerals.ironium==0); // No invented old stock.
    auto corrupt=current;{QDataStream out(&corrupt,QIODevice::ReadWrite);assert(out.device()->seek(tail+12));out<<std::numeric_limits<double>::quiet_NaN();}
    put(path,corrupt);assert(!read_save_game_file(path,loaded,error));
    put(path,current.left(current.size()-1));assert(!read_save_game_file(path,loaded,error));
}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);QTemporaryDir temp;assert(temp.isValid());
    QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,temp.path());
    persistence(temp.path());
    MainWindow window;
    window.installDeferredMapSelectionHandler();attachRouteProgramDock(window);
    window.installUiPolish();window.installProductionQueue();window.installFleetReadabilityPolish();
    window.installPanelLayoutFixes();MainWindowTestAccess::load(window,make_demo_game());
    window.resize(1366,900);window.show();settle();
    auto* action=window.findChild<QComboBox*>("routeArrivalActionCombo");
    auto* here=window.findChild<QPushButton*>("routeWorkHereButton");
    assert(action && here);
    const auto index=action->findData(int(FleetArrivalActionKind::ScrapFleet));assert(index>=0);
    assert(action->itemData(index,Qt::ToolTipRole).toString().contains("20%"));
    action->setCurrentIndex(index);settle();assert(here->isEnabled() && here->text()=="Scrap here");
    here->click();settle();
    const auto& orders=MainWindowTestAccess::orders(window);assert(orders.orders.size()==1);
    const auto* move=std::get_if<MoveFleetOrder>(&orders.orders[0]);assert(move);
    assert(move->arrivalAction.kind==FleetArrivalActionKind::ScrapFleet && move->queuedWaypoints.empty() && !move->repeatOrders);
    assert(MainWindowTestAccess::state(window).fleets.size()==1); // Planning does not destroy physical ships.
    const auto rows=window.selectedFleetRouteProgramRows();
    assert(rows.size()==1 && rows[0].arrivalAction.contains("Scrap Fleet"));
    if(app.arguments().contains("--capture")) window.grab().save("/tmp/suns-scrap-fleet.png");
}
