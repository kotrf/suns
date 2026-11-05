#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/intergalactic_visitors.hpp"
#include "suns/communications.hpp"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cassert>
using namespace suns;
namespace {
QByteArray bytes(const QString& path) { QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll(); }
void put(const QString& path,const QByteArray& data) { QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(data)==data.size()); }
SaveGameData fixture(VisitorProfile profile=VisitorProfile::Freight)
{
    SaveGameData d; d.state=make_visitor_demo(profile); d.mode=SessionMode::Host; d.pendingOrders={1,{}};
    d.campaignId=17; d.turnToken=29; d.playerTokens={{1,31},{2,41}}; d.state.players.push_back({2,"Remote human"}); return d;
}
void generated_oblique_path_survives_save_and_continuation()
{
    SaveGameData original; original.campaignId=46; original.turnToken=2;
    original.mode=SessionMode::Host; original.playerTokens={{1,2}};
    original.galaxyConfig={771,16,1600,240,15};
    original.state=generate_campaign(original.galaxyConfig,{{"Terrans",RacePreset::Terran}}); original.pendingOrders={1,{}};
    auto& s=original.state; s.turn=59;
    while(s.turn<10000 && s.visitors.empty()) { advance_visitors(s); ++s.turn; }
    assert(s.visitors.size()==1 && s.visitors[0].velocity.x!=0 && s.visitors[0].velocity.y!=0);
    const auto track=s.visitors[0];
    QTemporaryDir dir; const auto a=dir.filePath("oblique.suns"), b=dir.filePath("continued.suns");
    QString error; SaveGameData loaded;
    assert(write_save_game_file(a,original,error) && read_save_game_file(a,loaded,error));
    assert(loaded.state.visitors[0].position==track.position && loaded.state.visitors[0].velocity==track.velocity);
    assert(loaded.state.visitors[0].departureTurn==track.departureTurn);
    // A player file cannot reveal an undetected route, even after its hidden
    // physical velocity and endpoint change.
    assert(write_save_game_file(a,make_player_turn(original,1),error)); const auto hidden=bytes(a);
    auto other=original; other.state.visitors[0].velocity={0,110};
    assert(write_save_game_file(a,make_player_turn(other,1),error) && bytes(a)==hidden);
    const auto years=track.departureTurn-track.arrivalTurn+2;
    for(std::uint64_t n=0;n<years;++n) {
        original.state=TurnProcessor{}.process(original.state,{}); loaded.state=TurnProcessor{}.process(loaded.state,{});
        assert(write_save_game_file(a,original,error) && write_save_game_file(b,loaded,error));
        assert(bytes(a)==bytes(b));
    }
}
void profiles_round_trip_and_genuine_v112_defaults()
{
    QTemporaryDir dir; const auto path=dir.filePath("profiles.suns"); QString error; SaveGameData loaded;
    for(const auto& offer:visitor_offers()) {
        auto d=fixture(offer.profile); auto& s=d.state; auto& f=s.fleets[0]; f.position.x+=900;
        assert(submit_visitor_command(s,1,{f.id,1,8}));
        f.visitor={s.players[0].visitorKnowledge[0],8}; f.telemetry.visitor=f.visitor;
        auto obs=s.players[0].visitorKnowledge[0]; obs.receivedTurn=0;
        s.players[0].pendingVisitorReports.push_back({obs,VisitorReportKind::Exchanged,60,62,0});
        for(const auto& o:visitor_offers()) s.players[0].acquiredEquipment.push_back(o.gift);
        GameEvent event; event.id=46; event.recipient=1; event.turn=event.observedTurn=60;
        event.kind=GameEventKind::VisitorExchangeCompleted; event.quantity=1; event.visitorProfile=offer.profile;
        d.strategicMessages={event};
        assert(write_save_game_file(path,d,error) && read_save_game_file(path,loaded,error));
        assert(loaded.state.visitors[0].profile==offer.profile);
        assert(loaded.state.players[0].visitorKnowledge[0].profile==offer.profile);
        assert(loaded.state.players[0].pendingVisitorReports[0].observation.profile==offer.profile);
        assert(loaded.state.players[0].acquiredEquipment.size()==3);
        assert(loaded.state.fleets[0].visitor.track.profile==offer.profile);
        assert(loaded.state.fleets[0].telemetry.visitor.track.profile==offer.profile);
        assert(loaded.state.fleets[0].pendingCommands[0].visitor->track.profile==offer.profile);
        assert(loaded.strategicMessages[0].visitorProfile==offer.profile);
        assert(write_save_game_file(path,make_player_turn(d,1),error)); const auto visible=bytes(path);
        s.visitors[0].profile=VisitorProfile::Shieldwright; s.players[0].pendingVisitorReports[0].observation.profile=VisitorProfile::Freight;
        assert(write_save_game_file(path,make_player_turn(d,1),error) && bytes(path)==visible);
        assert(read_save_game_file(path,loaded,error) && loaded.state.players[0].visitorKnowledge[0].profile==offer.profile);
    }
    const auto legacy=QByteArray::fromHex(bytes(QString::fromUtf8(SUNS_TEST_FIXTURE_DIR "/visitor-v112.hex")));
    put(path,legacy); assert(read_save_game_file(path,loaded,error));
    assert(loaded.campaignId==46112 && loaded.state.visitors[0].profile==VisitorProfile::Freight);
    assert(loaded.state.players[0].visitorKnowledge[0].profile==VisitorProfile::Freight);
    assert(loaded.state.players[0].pendingVisitorReports[0].observation.profile==VisitorProfile::Freight);
    assert(loaded.state.fleets[0].visitor.track.profile==VisitorProfile::Freight);
    assert(loaded.state.fleets[0].pendingCommands[0].visitor->track.profile==VisitorProfile::Freight);
    assert(loaded.strategicMessages[0].visitorProfile==VisitorProfile::Freight);
    assert(loaded.state.players[0].acquiredEquipment==std::vector<ShipComponentType>{kVisitorGift});
    assert(write_save_game_file(path,loaded,error)); SaveGameData roundTrip;
    assert(read_save_game_file(path,roundTrip,error) && roundTrip.state.fleets[0].visitor.track.profile==VisitorProfile::Freight);
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
void invalid_visitor_enums_and_truncated_payloads_are_rejected()
{
    QTemporaryDir dir; const auto path=dir.filePath("corrupt-visitors.suns"); QString error; SaveGameData loaded;
    auto bad=fixture(); auto observation=bad.state.players[0].visitorKnowledge[0]; observation.receivedTurn=0;
    bad.state.players[0].pendingVisitorReports.push_back({observation,static_cast<VisitorReportKind>(255),60,61,0});
    assert(write_save_game_file(path,bad,error)); assert(!read_save_game_file(path,loaded,error));
    bad=fixture(); bad.state.players[0].acquiredEquipment={static_cast<ShipComponentType>(255)};
    assert(write_save_game_file(path,bad,error)); assert(!read_save_game_file(path,loaded,error));
    bad=fixture(); bad.state.visitors[0].profile=static_cast<VisitorProfile>(255);
    assert(write_save_game_file(path,bad,error)); assert(!read_save_game_file(path,loaded,error));
    bad=fixture(); bad.state.players[0].visitorKnowledge[0].profile=static_cast<VisitorProfile>(255);
    assert(write_save_game_file(path,bad,error)); assert(!read_save_game_file(path,loaded,error));
    bad=fixture(); bad.state.players[0].acquiredEquipment={kVisitorGift,kVisitorGift};
    assert(write_save_game_file(path,bad,error)); assert(!read_save_game_file(path,loaded,error));
    bad=fixture(); GameEvent invalid; invalid.id=46; invalid.turn=invalid.observedTurn=60; invalid.recipient=1;
    invalid.kind=GameEventKind::VisitorExchangeCompleted; invalid.visitorProfile=static_cast<VisitorProfile>(255);
    bad.strategicMessages={invalid};
    assert(write_save_game_file(path,bad,error)); assert(!read_save_game_file(path,loaded,error));

    auto valid=fixture(); valid.state.players[0].acquiredEquipment={kVisitorGift};
    valid.state.players[0].pendingVisitorReports.push_back({observation,VisitorReportKind::Sighted,60,61,0});
    assert(write_save_game_file(path,valid,error) && read_save_game_file(path,loaded,error));
    const auto data=bytes(path); QByteArray magic;
    { QDataStream out(&magic,QIODevice::WriteOnly); out<<quint32{0x56495349}; }
    const auto extension=data.lastIndexOf(magic), start=extension+8; assert(extension>0 && start<data.size());
    // Adjust the outer size to remain valid: every shorter inner payload must
    // fail even when reading its raw bytes from the outer stream succeeds.
    for(qsizetype cut=0;cut<data.size()-start;++cut) {
        auto truncated=data.first(start+cut);
        { QDataStream out(&truncated,QIODevice::ReadWrite); assert(out.device()->seek(extension+4)); out<<quint32(cut); }
        put(path,truncated); assert(!read_save_game_file(path,loaded,error));
    }
}
}
namespace suns {
struct MainWindowTestAccess {
    static const auto& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
    static const auto& state(const MainWindow& w) { return w.state_; }
    static void hidden(MainWindow& w) { w.state_.visitors[0].position={9999,9999}; w.state_.visitors[0].customer=1;
        w.state_.visitors[0].profile=VisitorProfile::Shieldwright; w.rebuildScene(); }
    static void endTurn(MainWindow& w) { w.endTurn(); }
    static void announcedWindowEnded(MainWindow& w) { w.state_.turn=66; w.rebuildScene(); }
    static void departureObserved(MainWindow& w) {
        auto& x=w.state_.players[0].visitorKnowledge[0];
        x.available=false; x.departed=true; x.observedTurn=x.receivedTurn=66; w.rebuildScene();
    }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); invalid_visitor_enums_and_truncated_payloads_are_rejected(); save_orders_and_private_exports();
    profiles_round_trip_and_genuine_v112_defaults();
    generated_oblique_path_survives_save_and_continuation();
    MainWindow w; w.installVisitors(); w.startVisitorDemo();
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
    w.installTurnMessages();
    for(const auto& offer:visitor_offers()) {
        w.startVisitorDemo(offer.profile);
        auto* label=w.findChild<QLabel*>("visitorOffer");
        assert(label->text().contains(offer.name) && label->text().contains(QString::fromStdString(component_spec(offer.gift).name)));
        assert(label->text().contains("Offer available at observation year 60"));
        assert(!w.findChild<QLabel*>("visitorPortrait")->pixmap().isNull());
        const auto text=label->text(), advertised=tree->topLevelItem(0)->text(5);
        MainWindowTestAccess::hidden(w); assert(label->text()==text && tree->topLevelItem(0)->text(5)==advertised);
        w.startVisitorDemo(offer.profile); send->click(); MainWindowTestAccess::endTurn(w);
        assert(MainWindowTestAccess::state(w).players[0].acquiredEquipment==std::vector<ShipComponentType>{offer.gift});
        assert(label->text().contains("Offer withdrawn at observation"));
        assert(label->text().contains(advertised)); // Keep the price and blueprint as historical terms.
        assert(!label->text().contains("One physical exchange before"));
        assert(!send->isEnabled());
        auto* messages=w.findChild<QListWidget*>("turnMessagesList"); bool correct=false;
        for(int i=0;i<messages->count();++i) correct |= messages->item(i)->text().contains(QString::fromStdString(component_spec(offer.gift).name));
        assert(correct);
        MainWindowTestAccess::announcedWindowEnded(w);
        assert(label->text().contains("Offer withdrawn at observation") && label->text().contains("Departure not observed"));
        // An announced deadline is not a delivered observation of departure.
        w.startVisitorDemo(offer.profile); MainWindowTestAccess::announcedWindowEnded(w);
        assert(label->text().contains("announced exchange window has ended"));
        assert(label->text().contains("Departure not observed"));
        assert(label->text().contains(advertised) && !send->isEnabled());
        const auto expired=label->text(); MainWindowTestAccess::hidden(w); assert(label->text()==expired);
        MainWindowTestAccess::departureObserved(w);
        assert(label->text().contains("Departure observed in year 66"));
        assert(!label->text().contains("Departure not observed"));
        assert(label->text().contains(advertised) && !send->isEnabled());
    }
}
