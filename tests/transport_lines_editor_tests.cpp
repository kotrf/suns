#include "main_window.hpp"
#include "save_game.hpp"
#include "transport_lines_fixture.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QFile>
#include <QDataStream>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void set(MainWindow& w,GameState s){w.state_=std::move(s);w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.rebuildScene();}
    static const auto& pending(MainWindow& w){return w.pendingOrders_;}
    static void draft(MainWindow& w){w.pendingOrders_.orders={MoveFleetOrder{1,{10,0},5},SetTransportLineOrder{line_spec()},AssignTransportLineOrder{1,1}};w.pendingDescriptions_={"Move","Define","Assign"};w.selectFleetForRouteProgram(1);w.rebuildScene();}
    static int overlays(MainWindow& w){int count=0;for(const auto* i:w.scene_->items())count+=i->data(0).toString()=="transportLine";return count;}
};
}
QByteArray raw_file(const QString& path){QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
SaveGameData host(GameState s){SaveGameData d;d.state=std::move(s);d.mode=SessionMode::Host;d.campaignId=55;d.turnToken=66;d.playerTokens={{1,77}};d.pendingOrders.player=1;return d;}
void persistence(const QString& path) {
    auto s=transport_world(900);s.fleets[0].position={900,0};acknowledge(s);
    assert(set_transport_line(s,1,{line_spec()}));assert(assign_transport_line(s,1,{1,1}));
    auto save=host(s);save.pendingOrders.orders={SetTransportLineOrder{line_spec()},AssignTransportLineOrder{1,2}};
    save.pendingDescriptions={"Define line","Assign"};QString error;SaveGameData loaded;
    assert(write_save_game_file(path,save,error));assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.players[0].transportLines[0].assignments[0].fleet==1);
    assert(loaded.state.players[0].transportLines[0].spec.name=="Mine shuttle");
    auto control=s,restored=loaded.state;
    for(int i=0;i<12;++i){tick(control);tick(restored);assert(same_position(control.fleets[0].position,restored.fleets[0].position));assert(control.fleets[0].minerals.ironium==restored.fleets[0].minerals.ironium);}
    TurnOrderFileData packet;packet.campaignId=55;packet.turn=s.turn;packet.turnToken=77;packet.orders=save.pendingOrders;packet.descriptions=save.pendingDescriptions;
    assert(write_turn_order_file(path+".orders",packet,error));TurnOrderFileData parsed;
    assert(read_turn_order_file(path+".orders",parsed,error));
    assert(std::get<SetTransportLineOrder>(parsed.orders.orders[0]).value.name=="Mine shuttle");
    assert(std::get<AssignTransportLineOrder>(parsed.orders.orders[1]).fleet==2);
    auto oldPacket=raw_file(path+".orders");
    {QDataStream out(&oldPacket,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{41};}
    {QFile f(path+".old-orders");assert(f.open(QIODevice::WriteOnly));f.write(oldPacket);}
    assert(!read_turn_order_file(path+".old-orders",parsed,error)); // New tags cannot masquerade as v41.
    auto duplicate=s;auto otherLine=duplicate.players[0].transportLines.front();otherLine.spec.id=2;
    duplicate.players[0].lastTransportLineId=2;duplicate.players[0].transportLines.push_back(otherLine);
    assert(write_save_game_file(path,host(duplicate),error));assert(!read_save_game_file(path,loaded,error));
    assert(write_save_game_file(path+".before",make_player_turn(host(s),1),error));
    s.planets[1].minerals={};s.planets[1].owner=2; // No delivered updates.
    assert(write_save_game_file(path+".after",make_player_turn(host(s),1),error));
    assert(raw_file(path+".before")==raw_file(path+".after"));
    Player other;other.id=2;other.name="Other";s.players.push_back(other);
    assert(make_player_view(s,2).state.players[0].transportLines.empty());
    assert(write_save_game_file(path,host(transport_world()),error));
    auto old=raw_file(path);QByteArray marker;{QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32{0x4c494e45};}
    const auto at=old.lastIndexOf(marker);assert(at>0);old.resize(at);
    {QDataStream out(&old,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{101};}
    {QFile f(path);assert(f.open(QIODevice::WriteOnly));f.write(old);}
    assert(read_save_game_file(path,loaded,error));assert(loaded.state.players[0].transportLines.empty());
    assert(write_save_game_file(path,host(restored),error));auto bad=raw_file(path);bad.chop(1);
    {QFile f(path);assert(f.open(QIODevice::WriteOnly));f.write(bad);}
    assert(!read_save_game_file(path,loaded,error));
}
void manual_route_editor_override() {
    MainWindow w;MainWindowTestAccess::set(w,transport_world());MainWindowTestAccess::draft(w);
    const auto rows=w.selectedFleetRouteProgramRows();assert(rows.size()==2);
    assert(rows[0].arrivalAction.contains("Load all available Ironium"));
    assert(w.setSelectedFleetRouteProgramLegWarp(0,4));
    const auto& pending=MainWindowTestAccess::pending(w);
    assert(pending.orders.size()==4 && std::holds_alternative<MoveFleetOrder>(pending.orders.back()));
    assert(std::get<MoveFleetOrder>(pending.orders.back()).warp==4);
    const auto result=TurnProcessor{}.process(transport_world(),{pending});
    assert(result.fleets[0].warp==4 && result.players[0].transportLines[0].assignments.empty());
}
void controls() {
    MainWindow w;MainWindowTestAccess::set(w,transport_world());w.installTransportLines();
    w.findChild<QLineEdit*>("transportLineName")->setText("Ore shuttle");
    auto* source=w.findChild<QComboBox*>("transportLineSource");source->setCurrentIndex(source->findData(2u));
    auto* target=w.findChild<QComboBox*>("transportLineDestination");target->setCurrentIndex(target->findData(1u));
    w.findChild<QPushButton*>("transportLineSave")->click();
    assert(MainWindowTestAccess::pending(w).orders.size()==1);
    const auto& define=std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders[0]);assert(define.value.id==1 && define.value.source==2);
    w.findChild<QPushButton*>("transportLineAssign")->click();
    assert(std::get<AssignTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).line==1);
    assert(w.findChild<QListWidget*>("transportLineRoster")->count()==1);
    const auto route=pending_transport_route(transport_world(),MainWindowTestAccess::pending(w),1);
    assert(route && route->repeatOrders && route->queuedWaypoints.size()==1);
    assert(route->arrivalAction.kind==FleetArrivalActionKind::LoadAllAvailable);
    w.findChild<QSpinBox*>("transportLineWarp")->setValue(6);w.findChild<QPushButton*>("transportLineSave")->click();
    assert(std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).value.revision==2);
    w.findChild<QPushButton*>("transportLinePause")->click();
    assert(!std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).value.active);
    auto completed=TurnProcessor{}.process(transport_world(),{MainWindowTestAccess::pending(w)});
    MainWindowTestAccess::set(w,completed);assert(MainWindowTestAccess::overlays(w)==1);
    w.findChild<QPushButton*>("transportLineDelete")->click();
    assert(std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).remove);
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir dir;assert(dir.isValid());persistence(dir.filePath("lines.suns"));controls();manual_route_editor_override();std::cout<<"Transport line UI/save tests passed\n";}
