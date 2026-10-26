#include "save_game.hpp"
#include "orbital_base_fixture.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <cassert>
#include <iostream>
using namespace orbital_test;
namespace {
QByteArray bytes(const QString& p){QFile f(p);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
SaveGameData data(GameState s) {SaveGameData d;d.campaignId=55;d.turnToken=66;d.state=std::move(s);d.pendingOrders.player=1;d.mode=SessionMode::Host;d.playerTokens[1]=77;return d;}
void roundtrip(const QString& p) {
    auto s=world();deploy(s);tick(s);tick(s,{OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive,0,0,0,false}}});
    apply_orbital_base_command(s,base(s),{OrbitalBaseAction::QueueShip,0,201});acknowledge(s);
    auto original=data(s);original.pendingOrders.orders.push_back(OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive}});original.pendingDescriptions.push_back("Resume orbital base");
    QString error;assert(write_save_game_file(p,original,error));SaveGameData loaded;
    if(!read_save_game_file(p,loaded,error)){std::cerr<<error.toStdString()<<"\n";assert(false);}
    assert(base(loaded.state).orbitalBase.assemblyYears==1 && base(loaded.state).orbitalBase.queue.size()==1);
    auto control=TurnProcessor{}.process(original.state,{original.pendingOrders});
    auto restored=TurnProcessor{}.process(loaded.state,{loaded.pendingOrders});
    assert(base(restored).orbitalBase.assemblyYears==base(control).orbitalBase.assemblyYears);
    assert(base(restored).minerals.ironium==base(control).minerals.ironium);
    for(int i=0;i<8;++i){tick(control);tick(restored);}
    assert(restored.fleets.size()==control.fleets.size());
    assert(base(restored).orbitalBase.queue.empty() && base(restored).minerals.ironium==base(control).minerals.ironium);
    apply_orbital_base_command(restored,base(restored),{OrbitalBaseAction::QueueShip,0,201,1});
    assert(base(restored).orbitalBase.queue.empty()); // Durable receipt survived the save/load and launch.
    // Player packets carry only intent, including ordered pause/resume/refit/cancel.
    TurnOrderFileData packet;packet.campaignId=55;packet.turn=s.turn;packet.turnToken=77;packet.orders.player=1;
    packet.orders.orders={OrbitalBaseOrder{1,{OrbitalBaseAction::Deploy,2}},OrbitalBaseOrder{1,{OrbitalBaseAction::SetActive,0,0,0,false}},OrbitalBaseOrder{1,{OrbitalBaseAction::QueueShip,0,201}},OrbitalBaseOrder{1,{OrbitalBaseAction::Cancel,0,0,2}},OrbitalBaseOrder{1,{OrbitalBaseAction::Refit,0,200}}};
    for(std::size_t i=0;i<packet.orders.orders.size();++i)packet.descriptions.push_back("Orbital command");
    assert(write_turn_order_file(p+".orders",packet,error));TurnOrderFileData parsed;
    assert(read_turn_order_file(p+".orders",parsed,error));assert(parsed.orders.orders.size()==5);
    assert(std::get<OrbitalBaseOrder>(parsed.orders.orders[4]).command.action==OrbitalBaseAction::Refit);
    // Truncated tail is rejected.
    auto raw=bytes(p);raw.chop(1);QFile truncated(p+".bad");assert(truncated.open(QIODevice::WriteOnly));truncated.write(raw);truncated.close();
    assert(!read_save_game_file(p+".bad",loaded,error));
}
void migration_and_corruption(const QString& p) {
    auto legacy=data(make_demo_game());QString error;assert(write_save_game_file(p,legacy,error));
    auto raw=bytes(p);QByteArray marker;{QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32{0x4f524249};}
    const auto at=raw.lastIndexOf(marker);assert(at>0);raw.resize(at);
    {QDataStream out(&raw,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{96};}
    QFile old(p+".v96");assert(old.open(QIODevice::WriteOnly));old.write(raw);old.close();SaveGameData loaded;
    assert(read_save_game_file(p+".v96",loaded,error));
    for(const auto& f:loaded.state.fleets)assert(!f.orbitalBase.planet && !f.orbitalShipyardBirth);
    auto previous=world();deploy(previous);acknowledge(previous);assert(write_save_game_file(p,data(previous),error));
    auto v97=bytes(p);const auto at97=v97.lastIndexOf(marker);assert(at97>0);quint32 size97{};
    {QDataStream in(v97);assert(in.device()->seek(at97+4));in>>size97;}
    assert(at97+8+size97==v97.size());
    v97.chop(12); // v98 adds one player-count/id/empty-intent-count block.
    {QDataStream out(&v97,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{97};
        assert(out.device()->seek(at97+4));out<<quint32(size97-12);}
    QFile old97(p+".v97");assert(old97.open(QIODevice::WriteOnly));old97.write(v97);old97.close();
    assert(read_save_game_file(p+".v97",loaded,error));
    assert(base(loaded.state).orbitalBase.planet==2 && loaded.state.players[0].computer.orbitalBuilds.empty());
    auto s=world();deploy(s);acknowledge(s);assert(write_save_game_file(p,data(s),error));raw=bytes(p);
    const auto tail=raw.lastIndexOf(marker);assert(tail>0);
    raw[tail+8+4+4+1+4+1+1]=char(2); // Invalid physical active boolean, inside the bounded tail.
    QFile corrupt(p+".corrupt");assert(corrupt.open(QIODevice::WriteOnly));corrupt.write(raw);corrupt.close();
    assert(!read_save_game_file(p+".corrupt",loaded,error));
    // Two physical stations may never occupy the same planet's station slot.
    auto duplicate=base(s);duplicate.id=2;s.fleets.push_back(duplicate);s.nextFleetId=3;
    assert(write_save_game_file(p,data(s),error));assert(!read_save_game_file(p,loaded,error));
}
void ai_intent_survives_reload(const QString& p) {
    auto s=world(5000);deploy(s);base(s).orbitalBase.assemblyYears=6;base(s).orbitalBase.supported=true;
    s.players[0].controller=PlayerController::Computer;
    Player human;human.id=2;human.name="Human observer";s.players.push_back(human);acknowledge(s);
    std::size_t sent=0;for(int i=0;i<10;++i)sent+=orbital_ai_year(s).size();
    assert(sent==1 && s.players[0].computer.orbitalBuilds.size()==1);
    const auto intent=s.players[0].computer.orbitalBuilds[0];assert(intent.item);
    QString error;auto host=data(s);host.pendingOrders.player=2;host.playerTokens={{2,77}};
    assert(write_save_game_file(p,host,error));SaveGameData loaded;
    if(!read_save_game_file(p,loaded,error)){std::cerr<<error.toStdString()<<"\n";assert(false);}
    assert(loaded.state.players[0].computer==s.players[0].computer);
    for(int i=10;i<39;++i) {
        assert(orbital_ai_year(s).empty());assert(orbital_ai_year(loaded.state).empty());
    }
    assert(base(s).orbitalBase.acceptedItems==std::vector<std::uint64_t>{intent.item});
    assert(base(loaded.state).orbitalBase.acceptedItems==base(s).orbitalBase.acceptedItems);
    assert(s.fleets.size()==2 && loaded.state.fleets.size()==2);
    acknowledge(s);acknowledge(loaded.state);
    const auto next=orbital_ai_year(s),restored=orbital_ai_year(loaded.state);
    assert(next.size()==1 && restored.size()==1 && next[0].command.item==restored[0].command.item && next[0].command.item!=intent.item);
}
void hidden_exports_and_inflight(const QString& p) {
    auto s=world(5000);deploy(s);acknowledge(s);const auto actual=base(s).orbitalBase;
    assert(submit_orbital_base_command(s,1,1,{OrbitalBaseAction::QueueShip,0,201}));
    assert(!base(s).pendingCommands.empty());
    auto host=data(s);QString error;
    assert(write_save_game_file(p,host,error));SaveGameData loaded;assert(read_save_game_file(p,loaded,error));
    assert(base(loaded.state).pendingCommands.back().orbitalBase->design==201);
    assert(base(loaded.state).orbitalBase.queue.empty());
    auto player=make_player_turn(host,1);assert(write_save_game_file(p+".one",player,error));
    auto& f=base(host.state);f.orbitalBase.assemblyYears=6;f.orbitalBase.background=100;f.colonists=1;f.minerals={};
    f.orbitalBase.queue.push_back({201,10});
    auto changed=make_player_turn(host,1);assert(write_save_game_file(p+".two",changed,error));
    assert(bytes(p+".one")==bytes(p+".two"));
    // A remote launch is physical but does not disclose a new fleet before its first report.
    auto launch=world(5000);deploy(launch);acknowledge(launch);
    auto launchHost=data(launch);auto beforeLaunch=make_player_turn(launchHost,1);
    assert(write_save_game_file(p+".before-launch",beforeLaunch,error));
    auto& yard=base(launchHost.state);yard.orbitalBase.assemblyYears=6;
    apply_orbital_base_command(launchHost.state,yard,{OrbitalBaseAction::QueueShip,0,201,101});service_orbital_bases(launchHost.state,launchHost.state.turn);
    assert(launchHost.state.fleets.size()==2 && unreported_orbital_ship(launchHost.state,launchHost.state.fleets.back()));
    auto afterLaunch=make_player_turn(launchHost,1);assert(write_save_game_file(p+".after-launch",afterLaunch,error));
    assert(bytes(p+".before-launch")==bytes(p+".after-launch"));
    assert(read_save_game_file(p+".one",loaded,error));
    assert(base(loaded.state).orbitalBase.assemblyYears==actual.assemblyYears && base(loaded.state).colonists==25000);
}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir dir;assert(dir.isValid());roundtrip(dir.filePath("roundtrip.suns"));ai_intent_survives_reload(dir.filePath("ai.suns"));migration_and_corruption(dir.filePath("migration.suns"));hidden_exports_and_inflight(dir.filePath("remote.suns"));}
