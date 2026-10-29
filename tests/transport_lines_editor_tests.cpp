#include "main_window.hpp"
#include "save_game.hpp"
#include "transport_lines_fixture.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QDockWidget>
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
QByteArray without_target_catalog(QByteArray raw) {
    QByteArray marker;{QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32{0x4c494e45};}
    const auto at=raw.lastIndexOf(marker);assert(at>0);quint32 size{};qint64 targetAt{};
    {QDataStream in(raw);assert(in.device()->seek(at+4));in>>size;
        quint32 players{},owner{},last{},lines{},id{},source{},destination{},reserve{},minimum{};quint64 revision{};QString name;quint8 warp{},cargo{},active{};
        in>>players>>owner>>last>>lines;assert(players==1 && lines==1);
        in>>id>>revision>>name>>source>>destination>>warp>>cargo>>active>>reserve>>minimum;targetAt=in.device()->pos();}
    raw.remove(targetAt,4);{QDataStream out(&raw,QIODevice::ReadWrite);assert(out.device()->seek(at+4));out<<quint32(size-4);}
    return raw;
}
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
void load_policy_persistence(const QString& path) {
    auto s=transport_world(900);s.fleets.resize(1);s.fleets[0].position={900,0};s.fleets[0].electronics.mode=EmissionMode::Passive;
    s.planets[1].minerals.ironium=130;acknowledge(s);
    auto d=line_spec();d.sourceReserve=100;d.departureMinimum=80;
    assert(set_transport_line(s,1,{d}) && assign_transport_line(s,1,{1,1}));
    QString error;SaveGameData loaded;
    assert(write_save_game_file(path,host(s),error));assert(read_save_game_file(path,loaded,error));
    const auto& pending=loaded.state.fleets[0].pendingCommands.front().program.arrivalAction;
    assert(pending.kind==FleetArrivalActionKind::LoadMinerals && pending.mineralReserve==100 && pending.mineralMinimum==80);
    const auto due=s.fleets[0].pendingCommands.front().deliveryTurn;
    while(s.turn<=due)tick(s);
    assert(s.fleets[0].minerals.ironium==30 && s.fleets[0].arrivalAction);
    assert(write_save_game_file(path,host(s),error));assert(read_save_game_file(path,loaded,error));s=std::move(loaded.state);
    assert(s.fleets[0].arrivalAction->mineralMinimum==80 && s.players[0].transportLines[0].spec.sourceReserve==100);
    assert(write_save_game_file(path+".old-view",make_player_turn(host(s),1),error));
    auto hidden=s;hidden.planets[1].minerals.ironium+=500;hidden.fleets[0].minerals.ironium=79;
    assert(write_save_game_file(path+".hidden",make_player_turn(host(hidden),1),error));
    assert(raw_file(path+".old-view")==raw_file(path+".hidden"));
    s.fleets[0].electronics.mode=EmissionMode::Standard;
    for(int i=0;i<8;++i)tick(s);
    const auto view=make_player_view(s,1);
    assert(view.state.fleets[0].arrivalAction && view.state.fleets[0].arrivalAction->mineralMinimum==80);
    assert(same_position(view.state.fleets[0].position,{900,0}));
    const auto& line=s.players[0].transportLines[0];
    assert(transport_assignment_status(view,line,line.assignments[0]).find("Waiting at source")!=std::string::npos);
    assert(write_save_game_file(path,make_player_turn(host(s),1),error));assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets[0].arrivalAction->mineralReserve==100);
    MainWindow w;MainWindowTestAccess::set(w,s);assert(w.selectFleetForRouteProgram(1));
    assert(w.selectedFleetRouteProgramRows()[0].arrivalAction.contains("leave 100 kt"));
    assert(w.selectedFleetRouteProgramForecast().contains("Load policy waiting"));
    auto control=s;s.planets[1].minerals.ironium+=50;control.planets[1].minerals.ironium+=50;
    tick(s);tick(control);assert(s.fleets[0].minerals.ironium==80 && s.planets[1].minerals.ironium==100);
    assert(same_position(*s.fleets[0].destination,{0,0}));
    TurnOrderFileData packet;packet.campaignId=55;packet.turn=s.turn;packet.turnToken=77;packet.orders={1,{SetTransportLineOrder{d}}};packet.descriptions={"Reserve and wait"};
    assert(write_turn_order_file(path+".orders",packet,error));TurnOrderFileData parsed;
    assert(read_turn_order_file(path+".orders",parsed,error));
    const auto& spec=std::get<SetTransportLineOrder>(parsed.orders.orders[0]).value;
    assert(spec.sourceReserve==100 && spec.departureMinimum==80);
}
void legacy_v102_line(const QString& path) {
    auto s=transport_world();assert(set_transport_line(s,1,{line_spec()}));QString error;SaveGameData loaded;
    assert(write_save_game_file(path,host(s),error));auto raw=raw_file(path);QByteArray marker;
    {QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32{0x4c494e45};}
    const auto at=raw.lastIndexOf(marker);assert(at>0);quint32 size{};qint64 policyAt{};
    {QDataStream in(raw);assert(in.device()->seek(at+4));in>>size;
        quint32 players{},id{},last{},lines{},line{},source{},destination{};quint64 revision{};QString name;quint8 warp{},cargo{},active{};
        in>>players>>id>>last>>lines;assert(players==1 && lines==1);
        in>>line>>revision>>name>>source>>destination>>warp>>cargo>>active;policyAt=in.device()->pos();}
    raw.remove(policyAt,12);raw.resize(at+size-4); // v102 omits all three policy fields and TLST.
    {QDataStream out(&raw,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{102};assert(out.device()->seek(at+4));out<<quint32(size-12);}
    {QFile file(path);assert(file.open(QIODevice::WriteOnly));file.write(raw);}
    assert(read_save_game_file(path,loaded,error));
    const auto& d=loaded.state.players[0].transportLines[0].spec;assert(!d.sourceReserve && !d.departureMinimum);
}
void computer_line_persistence(const QString& path) {
    auto s=computer_transport_world(5100);s.fleets[0].position={5100,0};
    for(auto& d:s.shipDesigns)if(d.id==202)d.components={C::FuelMizer,C::FuelMizer};acknowledge(s);
    Player human;human.id=2;human.name="Human observer";s.players.push_back(human);
    s=resolve_controlled_campaign_turn(s,{{2,{}}}).state;
    assert(s.players[0].transportLines.size()==1 && !s.fleets[0].pendingCommands.empty());
    auto save=host(s);save.pendingOrders={2,{}};save.playerTokens={{2,77}};
    QString error;SaveGameData loaded;
    assert(write_save_game_file(path,save,error));assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.players[0].computer==s.players[0].computer);
    assert(loaded.state.players[0].lastTransportLineId==1);
    auto control=s,restored=loaded.state;
    const auto due=s.fleets[0].pendingCommands.front().deliveryTurn;
    assert(due>s.turn+20);
    while(control.turn<=due+8) {
        for(const auto* state:{&control,&restored}) {
            const auto plan=plan_computer_turn(make_player_view(*state,1));
            assert(std::none_of(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){
                return std::holds_alternative<SetTransportLineOrder>(o) || std::holds_alternative<AssignTransportLineOrder>(o);
            }));
        }
        control=resolve_controlled_campaign_turn(control,{{2,{}}}).state;
        restored=resolve_controlled_campaign_turn(restored,{{2,{}}}).state;
        assert(control.players[0].computer==restored.players[0].computer);
        assert(same_position(control.fleets[0].position,restored.fleets[0].position));
        assert(control.fleets[0].fuel==restored.fleets[0].fuel && control.fleets[0].minerals.ironium==restored.fleets[0].minerals.ironium);
        assert(restored.players[0].transportLines.size()==1 && restored.players[0].transportLines[0].assignments.size()==1);
    }
    assert(restored.fleets[0].repeatOrders);
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
void received_statistics_roundtrip(const QString& path) {
    auto s=computer_transport_world(5100);s.players[0].controller=PlayerController::Human;s.fleets.resize(1);
    auto d=line_spec();d.source=1;d.destination=2;assert(set_transport_line(s,1,{d}));
    const auto unload=transport_line_program(s.players[0].transportLines[0]).queuedWaypoints[0].arrivalAction;
    s.fleets[0].position={5100,0};s.fleets[0].destination=s.fleets[0].position;s.fleets[0].arrivalAction=unload;
    s.fleets[0].minerals.ironium=70;acknowledge(s);tick(s);
    assert(s.players[0].transportReceipts.empty());
    const auto pending=*std::find_if(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::TransportLineDelivery;});
    QString error;SaveGameData loaded;assert(write_save_game_file(path,host(s),error));assert(read_save_game_file(path,loaded,error));
    const auto packet=*std::find_if(loaded.state.players[0].pendingPlayerReports.begin(),loaded.state.players[0].pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::TransportLineDelivery;});
    assert(packet.transportLine==1 && packet.transportRevision==1 && packet.deliveredMinerals.ironium==70);
    assert(write_save_game_file(path+".received",make_player_turn(host(s),1),error));
    auto hidden=s;for(auto& r:hidden.players[0].pendingPlayerReports)if(r.kind==PlayerReportKind::TransportLineDelivery)r.deliveredMinerals.ironium=999;
    hidden.planets[1].minerals={9999,0,0};hidden.planets[1].owner=2;hidden.orbitalStations.clear();
    hidden.fleets[0].position={9999,0};hidden.fleets[0].minerals={9999,0,0};
    assert(write_save_game_file(path+".hidden",make_player_turn(host(hidden),1),error));
    assert(raw_file(path+".received")==raw_file(path+".hidden"));
    auto control=s,restored=loaded.state;
    while(control.turn<pending.deliveryTurn){tick(control);tick(restored);}
    assert(control.players[0].transportReceipts.size()==1 && restored.players[0].transportReceipts.size()==1);
    assert(restored.players[0].transportReceipts[0].minerals.ironium==70);
    assert(write_save_game_file(path,host(restored),error));assert(read_save_game_file(path,loaded,error));
    auto duplicate=pending;duplicate.deliveryTurn=loaded.state.turn;loaded.state.players[0].pendingPlayerReports.push_back(duplicate);
    (void)deliver_due_player_reports(loaded.state);assert(loaded.state.players[0].transportReceipts.size()==1);
    assert(write_save_game_file(path,make_player_turn(host(loaded.state),1),error));assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.players[0].transportReceipts[0].receivedTurn==pending.deliveryTurn);
    MainWindow w;MainWindowTestAccess::set(w,restored);w.installTransportLines();
    auto* dock=w.findChild<QDockWidget*>("transportLinesDock");dock->show();
    auto* selection=w.findChild<QComboBox*>("transportLineSelection");selection->setCurrentIndex(selection->findData(1u));
    const auto shown=w.findChild<QLabel*>("transportLineMetrics")->text();
    assert(shown.contains("1 unloads") && shown.contains("70 / 0 / 0") && shown.contains("Calculated flight-only"));
    assert(w.findChild<QListWidget*>("transportLineReceipts")->count()==1);
    // Bounded statistics reject duplicated identities and truncated extensions.
    auto bad=restored;bad.players[0].transportReceipts.push_back(bad.players[0].transportReceipts.front());
    assert(write_save_game_file(path,host(bad),error));assert(!read_save_game_file(path,loaded,error));
    assert(write_save_game_file(path,host(restored),error));auto bytes=raw_file(path);bytes.chop(1);
    {QFile f(path);assert(f.open(QIODevice::WriteOnly));f.write(bytes);}assert(!read_save_game_file(path,loaded,error));
}
void statistics_programme_orders_and_v103(const QString& path) {
    auto s=transport_world(900);s.fleets[0].position={900,0};acknowledge(s);
    assert(set_transport_line(s,1,{line_spec()}) && assign_transport_line(s,1,{1,1}));
    QString error;SaveGameData loaded;assert(write_save_game_file(path,host(s),error));assert(read_save_game_file(path,loaded,error));
    const auto& unload=loaded.state.fleets[0].pendingCommands[0].program.queuedWaypoints[0].arrivalAction;
    assert(unload.kind==FleetArrivalActionKind::UnloadTransportLine && unload.transportLine==1 && unload.transportRevision==1 && unload.transportDestination==1);
    const auto programme=transport_line_program(s.players[0].transportLines[0]);
    TurnOrderFileData packet;packet.campaignId=55;packet.turn=s.turn;packet.turnToken=77;
    packet.orders={1,{MoveFleetOrder{1,programme.destination,programme.warp,programme.arrivalAction,programme.queuedWaypoints,true}}};packet.descriptions={"Keep frozen line attribution"};
    assert(write_turn_order_file(path+".orders",packet,error));TurnOrderFileData read;
    assert(read_turn_order_file(path+".orders",read,error));assert(std::get<MoveFleetOrder>(read.orders.orders[0]).queuedWaypoints[0].arrivalAction==unload);
    auto forged=raw_file(path+".orders");{QDataStream out(&forged,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{43};}
    {QFile f(path+".old-orders");assert(f.open(QIODevice::WriteOnly));f.write(forged);}assert(!read_turn_order_file(path+".old-orders",read,error));
    // A real v103-shaped catalog has neither the new action nor TLST; it keeps
    // its unchanged legacy programme and starts with no invented statistics.
    auto old=transport_world();assert(set_transport_line(old,1,{line_spec()}) && assign_transport_line(old,1,{1,1}));
    for(auto& f:old.fleets) {
        const auto legacy=[](FleetArrivalAction& a){if(a.kind==FleetArrivalActionKind::UnloadTransportLine){a.kind=FleetArrivalActionKind::UnloadAll;a.transportLine=0;a.transportRevision=0;a.transportDestination=0;}};
        for(auto& w:f.waypointQueue)legacy(w.arrivalAction);for(auto& w:f.routeTemplate)legacy(w.arrivalAction);
    }
    assert(write_save_game_file(path,host(old),error));auto raw=without_target_catalog(raw_file(path));QByteArray marker;{QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32{0x544c5354};}
    const auto at=raw.lastIndexOf(marker);assert(at>0);raw.resize(at);{QDataStream out(&raw,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{103};}
    {QFile f(path);assert(f.open(QIODevice::WriteOnly));f.write(raw);}assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.players[0].transportReceipts.empty());
    tick(loaded.state);assert(transport_assignment_status(make_player_view(loaded.state,1),loaded.state.players[0].transportLines[0],loaded.state.players[0].transportLines[0].assignments[0]).find("Legacy")!=std::string::npos);
}
void destination_target_persistence_and_ui(const QString& path) {
    auto s=stock_target_world();auto d=line_spec();d.destinationTarget=100;
    assert(set_transport_line(s,1,{d}) && assign_transport_line(s,1,{1,1}));for(int i=0;i<3;++i)tick(s);
    assert(s.fleets[0].minerals.ironium==50 && s.fleets[0].arrivalAction->destinationTarget==100);
    QString error;SaveGameData loaded;auto saved=host(s);auto edit=d;++edit.revision;edit.destinationTarget=150;
    saved.pendingOrders.orders={SetTransportLineOrder{edit}};saved.pendingDescriptions={"Change received destination target"};
    assert(write_save_game_file(path,saved,error));assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets[0].arrivalAction->destinationTarget==100 && loaded.state.fleets[0].minerals.ironium==50);
    assert(std::get<SetTransportLineOrder>(loaded.pendingOrders.orders[0]).value.destinationTarget==150);
    auto control=s,restored=loaded.state;tick(control,{QueueProductionOrder{1,ProductionKind::Factory}});tick(restored,{QueueProductionOrder{1,ProductionKind::Factory}});
    tick(control);tick(restored);assert(control.fleets[0].minerals.ironium==48 && restored.fleets[0].minerals.ironium==48);
    assert(control.players[0].transportReceipts.size()==restored.players[0].transportReceipts.size());
    MainWindow w;MainWindowTestAccess::set(w,s);w.installTransportLines();
    w.findChild<QDockWidget*>("transportLinesDock")->show();
    auto* selection=w.findChild<QComboBox*>("transportLineSelection");selection->setCurrentIndex(selection->findData(1u));
    auto* target=w.findChild<QSpinBox*>("transportLineTarget");assert(target && target->value()==100);
    assert(w.findChild<QListWidget*>("transportLineRoster")->item(0)->text().contains("Waiting at destination"));
    assert(w.selectFleetForRouteProgram(1));assert(w.selectedFleetRouteProgramForecast().contains("Destination target waiting"));
    target->setValue(150);w.findChild<QPushButton*>("transportLineSave")->click();
    assert(std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).value.destinationTarget==150);
    assert(pending_transport_route(s,MainWindowTestAccess::pending(w),1)->queuedWaypoints[0].arrivalAction.destinationTarget==150);
    assert(w.selectedFleetRouteProgramForecast().contains("150 kt"));
}
void destination_target_remote_orders(const QString& path) {
    auto s=computer_transport_world(5100);s.players[0].controller=PlayerController::Human;s.fleets.resize(1);
    for(auto& p:s.planets)p.observedConcentration=MineralCargo{};s.planets[1].minerals.ironium=100;
    auto d=line_spec();d.source=1;d.destination=2;d.destinationTarget=100;assert(set_transport_line(s,1,{d}));
    const auto programme=transport_line_program(s.players[0].transportLines[0]);auto& f=s.fleets[0];
    f.position={5100,0};f.destination=f.position;f.arrivalAction=programme.queuedWaypoints[0].arrivalAction;f.minerals.ironium=50;
    f.repeatOrders=true;f.routeTemplate={{programme.destination,programme.warp,programme.arrivalAction},programme.queuedWaypoints[0]};
    s.players[0].transportLines[0].assignments={{1,s.turn}};acknowledge(s);tick(s);
    ++d.revision;d.destinationTarget=200;assert(set_transport_line(s,1,{d}));
    QString error;SaveGameData loaded;assert(write_save_game_file(path,host(s),error));assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets[0].arrivalAction->destinationTarget==100);
    assert(loaded.state.fleets[0].pendingCommands.back().program.queuedWaypoints[0].arrivalAction.destinationTarget==200);
    assert(write_save_game_file(path+".known",make_player_turn(host(s),1),error));
    auto hidden=s;hidden.planets[1].minerals.ironium=0;hidden.fleets[0].minerals.ironium=0;hidden.fleets[0].arrivalAction->destinationTarget=999;
    assert(write_save_game_file(path+".hidden",make_player_turn(host(hidden),1),error));assert(raw_file(path+".known")==raw_file(path+".hidden"));
    auto control=s,restored=loaded.state;const auto due=s.fleets[0].pendingCommands.back().deliveryTurn;
    while(control.turn<=due){tick(control);tick(restored);assert(control.fleets[0].minerals.ironium==restored.fleets[0].minerals.ironium);}
    assert(restored.fleets[0].routeTemplate[1].arrivalAction.destinationTarget==200);
    const auto updated=transport_line_program(s.players[0].transportLines[0]);
    TurnOrderFileData packet;packet.campaignId=55;packet.turn=s.turn;packet.turnToken=77;
    packet.orders={1,{SetTransportLineOrder{d},MoveFleetOrder{1,updated.destination,updated.warp,updated.arrivalAction,updated.queuedWaypoints,true}}};packet.descriptions={"Target 200","Frozen target programme"};
    assert(write_turn_order_file(path+".orders",packet,error));TurnOrderFileData read;assert(read_turn_order_file(path+".orders",read,error));
    assert(std::get<SetTransportLineOrder>(read.orders.orders[0]).value.destinationTarget==200);
    assert(std::get<MoveFleetOrder>(read.orders.orders[1]).queuedWaypoints[0].arrivalAction.destinationTarget==200);
    auto old=raw_file(path+".orders");{QDataStream out(&old,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{44};}
    {QFile file(path+".old-orders");assert(file.open(QIODevice::WriteOnly));file.write(old);}assert(!read_turn_order_file(path+".old-orders",read,error));
    auto trailing=raw_file(path+".orders");trailing.append(char(0));{QFile file(path+".trailing");assert(file.open(QIODevice::WriteOnly));file.write(trailing);}
    assert(!read_turn_order_file(path+".trailing",read,error));
}
void destination_target_v104(const QString& path) {
    auto s=stock_target_world();assert(set_transport_line(s,1,{line_spec()}) && assign_transport_line(s,1,{1,1}));
    QString error;assert(write_save_game_file(path,host(s),error));auto raw=raw_file(path);
    QByteArray modern;{QDataStream out(&modern,QIODevice::WriteOnly);out<<quint8(FleetArrivalActionKind::UnloadTransportLine)<<quint64{1}
        <<quint8(FleetCargoKind::Ironium)<<quint32{0}<<quint8{0}<<quint32{1}<<quint64{1}<<quint32{1}<<quint32{0};}
    auto legacy=modern;legacy.chop(4);assert(raw.count(modern)>=2);raw.replace(modern,legacy);
    raw=without_target_catalog(raw);{QDataStream out(&raw,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{104};}
    {QFile file(path);assert(file.open(QIODevice::WriteOnly));file.write(raw);}SaveGameData loaded;assert(read_save_game_file(path,loaded,error));
    assert(loaded.state.players[0].transportLines[0].spec.destinationTarget==0);
    assert(loaded.state.fleets[0].routeTemplate[1].arrivalAction.destinationTarget==0);
    for(int i=0;i<3;++i)tick(loaded.state);
    assert(loaded.state.planets[0].minerals.ironium==150 && loaded.state.fleets[0].minerals.ironium==0);
}
void controls() {
    MainWindow w;MainWindowTestAccess::set(w,transport_world());w.installTransportLines();
    w.findChild<QLineEdit*>("transportLineName")->setText("Ore shuttle");
    w.findChild<QSpinBox*>("transportLineReserve")->setValue(10);
    w.findChild<QSpinBox*>("transportLineMinimum")->setValue(40);
    auto* source=w.findChild<QComboBox*>("transportLineSource");source->setCurrentIndex(source->findData(2u));
    auto* target=w.findChild<QComboBox*>("transportLineDestination");target->setCurrentIndex(target->findData(1u));
    w.findChild<QPushButton*>("transportLineSave")->click();
    assert(MainWindowTestAccess::pending(w).orders.size()==1);
    const auto& define=std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders[0]);assert(define.value.id==1 && define.value.source==2 && define.value.sourceReserve==10 && define.value.departureMinimum==40);
    w.findChild<QPushButton*>("transportLineAssign")->click();
    assert(std::get<AssignTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).line==1);
    assert(w.findChild<QListWidget*>("transportLineRoster")->count()==1);
    const auto route=pending_transport_route(transport_world(),MainWindowTestAccess::pending(w),1);
    assert(route && route->repeatOrders && route->queuedWaypoints.size()==1);
    assert(route->arrivalAction.kind==FleetArrivalActionKind::LoadMinerals && route->arrivalAction.mineralMinimum==40);
    w.findChild<QSpinBox*>("transportLineWarp")->setValue(6);w.findChild<QPushButton*>("transportLineSave")->click();
    assert(std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).value.revision==2);
    w.findChild<QPushButton*>("transportLinePause")->click();
    assert(!std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).value.active);
    auto completed=TurnProcessor{}.process(transport_world(),{MainWindowTestAccess::pending(w)});
    MainWindowTestAccess::set(w,completed);assert(MainWindowTestAccess::overlays(w)==1);
    w.findChild<QPushButton*>("transportLineDelete")->click();
    assert(std::get<SetTransportLineOrder>(MainWindowTestAccess::pending(w).orders.back()).remove);
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir dir;assert(dir.isValid());destination_target_persistence_and_ui(dir.filePath("target.suns"));destination_target_remote_orders(dir.filePath("target-remote.suns"));destination_target_v104(dir.filePath("target-v104.suns"));received_statistics_roundtrip(dir.filePath("statistics.suns"));statistics_programme_orders_and_v103(dir.filePath("statistics-orders.suns"));persistence(dir.filePath("lines.suns"));load_policy_persistence(dir.filePath("policy.suns"));legacy_v102_line(dir.filePath("legacy102.suns"));computer_line_persistence(dir.filePath("computer.suns"));controls();manual_route_editor_override();std::cout<<"Transport line UI/save tests passed\n";}
