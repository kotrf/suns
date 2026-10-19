#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/intergalactic_visitors.hpp"
#include "suns/communications.hpp"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cassert>
using namespace suns;
namespace {
QByteArray bytes(const QString& path) { QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll(); }
SaveGameData fixture()
{
    SaveGameData d; d.state=make_visitor_demo(); d.mode=SessionMode::Host; d.pendingOrders={1,{}};
    d.campaignId=17; d.turnToken=29; d.playerTokens={{1,31},{2,41}}; d.state.players.push_back({2,"Remote human"}); return d;
}
void save_orders_and_private_exports()
{
    auto host=fixture(); QTemporaryDir dir; const auto a=dir.filePath("a.suns"), b=dir.filePath("b.suns"); QString error; SaveGameData loaded;
    auto& s=host.state; auto& f=s.fleets[0]; f.position.x+=900;
    assert(submit_visitor_command(s,1,{f.id,1,8}));
    assert(write_save_game_file(a,host,error)); assert(read_save_game_file(a,loaded,error));
    assert(loaded.state.fleets[0].pendingCommands[0].visitor && loaded.state.visitors.size()==1);
    for(int n=0;n<8;++n) {
        host.state=TurnProcessor{}.process(host.state,{}); loaded.state=TurnProcessor{}.process(loaded.state,{});
        assert(write_save_game_file(a,host,error) && write_save_game_file(b,loaded,error)); assert(bytes(a)==bytes(b));
    }
    host=fixture(); assert(write_save_game_file(a,make_player_turn(host,1),error)); const auto before=bytes(a);
    host.state.visitors[0].customer=2; host.state.visitors[0].position={9999,9999};
    host.state.players[0].pendingVisitorReports.push_back({host.state.players[0].visitorKnowledge[0],VisitorReportKind::Exchanged,60,66,1});
    apply_visitor_command(host.state,host.state.fleets[0],{host.state.players[0].visitorKnowledge[0],9});
    assert(write_save_game_file(a,make_player_turn(host,1),error) && bytes(a)==before);
    assert(read_save_game_file(a,loaded,error) && loaded.state.visitors.empty() && loaded.state.players[0].pendingVisitorReports.empty());
    TurnOrderFileData packet{17,60,31,{1,{EncounterVisitorOrder{1,1,8},EncounterVisitorOrder{1,0,8}}},{"Intercept","Cancel"}}, imported;
    const auto path=dir.filePath("visitor.sunsorders");
    assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,imported,error));
    assert(std::get<EncounterVisitorOrder>(imported.orders.orders[0]).visitor==1 && !std::get<EncounterVisitorOrder>(imported.orders.orders[1]).visitor);
    auto foreign=fixture(); assert(!submit_visitor_command(foreign.state,2,{1,1,8}));
    auto legacy=fixture(); legacy.state.visitors.clear(); legacy.state.nextVisitorId=1; legacy.state.players[0].visitorKnowledge.clear();
    assert(write_save_game_file(a,legacy,error)); auto data=bytes(a); QByteArray magic;
    { QDataStream out(&magic,QIODevice::WriteOnly); out<<quint32{0x56495349}; }
    const auto extension=data.lastIndexOf(magic); assert(extension>0); data.truncate(extension);
    { QDataStream out(&data,QIODevice::ReadWrite); assert(out.device()->seek(4)); out<<quint32{88}; }
    { QFile file(a); assert(file.open(QIODevice::WriteOnly)); assert(file.write(data)==data.size()); }
    assert(read_save_game_file(a,loaded,error) && loaded.state.visitors.empty() && loaded.state.players[0].visitorKnowledge.empty());

    auto paid=fixture(); auto& courier=paid.state.fleets[0]; courier.position.x+=900; paid.state.visitors[0].position.x+=900;
    paid.state.players[0].visitorKnowledge[0].position.x+=900;
    apply_visitor_command(paid.state,courier,{paid.state.players[0].visitorKnowledge[0],8});
    paid.state=TurnProcessor{}.process(paid.state,{});
    assert(paid.state.visitors[0].customer==1 && paid.state.players[0].acquiredEquipment.empty());
    assert(write_save_game_file(a,paid,error) && read_save_game_file(a,loaded,error));
    for(int n=0;n<8;++n) { paid.state=TurnProcessor{}.process(paid.state,{}); loaded.state=TurnProcessor{}.process(loaded.state,{}); }
    assert(paid.state.players[0].acquiredEquipment==loaded.state.players[0].acquiredEquipment && loaded.state.players[0].acquiredEquipment.size()==1);
    auto bad=fixture(); bad.state.visitors[0].velocity={0,0};
    assert(write_save_game_file(a,bad,error) && !read_save_game_file(a,loaded,error));
    bad=fixture(); bad.state.players[0].visitorKnowledge.push_back(bad.state.players[0].visitorKnowledge[0]);
    assert(write_save_game_file(a,bad,error) && !read_save_game_file(a,loaded,error));
}
}
namespace suns {
struct MainWindowTestAccess {
    static const auto& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
    static const auto& state(const MainWindow& w) { return w.state_; }
    static void hidden(MainWindow& w) { w.state_.visitors[0].position={9999,9999}; w.state_.visitors[0].customer=1; w.rebuildScene(); }
    static void endTurn(MainWindow& w) { w.endTurn(); }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); save_orders_and_private_exports(); MainWindow w; w.installVisitors(); w.startVisitorDemo();
    auto* tree=w.findChild<QTreeWidget*>("visitorContacts"); auto* send=w.findChild<QPushButton*>("interceptVisitor");
    assert(tree && tree->topLevelItemCount()==1 && send->isEnabled());
    const auto measured=tree->topLevelItem(0)->text(2), forecast=tree->topLevelItem(0)->text(3);
    MainWindowTestAccess::hidden(w); assert(tree->topLevelItem(0)->text(2)==measured && tree->topLevelItem(0)->text(3)==forecast);
    w.startVisitorDemo(); send->click(); assert(MainWindowTestAccess::orders(w).size()==1);
    assert(std::get<EncounterVisitorOrder>(MainWindowTestAccess::orders(w).back()).visitor==1 && MainWindowTestAccess::state(w).fleets[0].minerals.germanium==50);
    MainWindowTestAccess::endTurn(w);
    assert(MainWindowTestAccess::state(w).players[0].acquiredEquipment.size()==1);
    assert(w.findChild<QLabel*>("visitorStatus")->text().contains("blueprint received"));
    w.startVisitorDemo(); w.findChild<QPushButton*>("cancelVisitorEncounter")->click();
    assert(std::get<EncounterVisitorOrder>(MainWindowTestAccess::orders(w).back()).visitor==0);
}
