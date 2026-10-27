#include "save_game.hpp"
#include "orbital_base_fixture.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <cassert>
#include <iostream>
#include <utility>
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
    v97.resize(at97+8+size97); // Strip later bounded extensions before constructing a genuine v97 fixture.
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
// These layouts are the exact independent bounded tails written by the two
// pre-integration branches. Migrate each without inventing the other's state.
void parallel_branch_migrations(const QString& p) {
    const auto same_request = [](const auto& a, const auto& b) {
        assert(a && b);
        assert(a->action==b->action && a->item==b->item && a->receiver==b->receiver
            && a->station==b->station && a->planet==b->planet && a->design==b->design && a->active==b->active);
    };
    const auto tail = [](const QByteArray& raw, quint32 magic) {
        QByteArray marker;
        { QDataStream out(&marker,QIODevice::WriteOnly); out << magic; }
        const auto at=raw.lastIndexOf(marker);assert(at>0);
        quint32 size{};
        { QDataStream in(raw);assert(in.device()->seek(at+4));in >> size; }
        assert(at+8+size<=raw.size());
        return std::pair{at,qsizetype(8+size)};
    };
    const auto store = [&](QByteArray raw, quint32 version) {
        { QDataStream out(&raw,QIODevice::ReadWrite);assert(out.device()->seek(4));out << version; }
        QFile file(p);assert(file.open(QIODevice::WriteOnly));assert(file.write(raw)==raw.size());
    };
    auto s=world(5000);deploy(s);recovery_transport(s);
    apply_orbital_base_command(s,base(s),retirement(true));
    tick(s);acknowledge(s);
    assert(submit_orbital_base_command(s,1,1,{OrbitalBaseAction::CancelRetirement,0,0,100}));
    assert(!base(s).telemetryInTransit.empty());
    QString error;SaveGameData loaded;
    assert(write_save_game_file(p,data(s),error));
    auto v99=bytes(p);const auto [conditionsAt,conditionsSize]=tail(v99,0x43434f4e);
    v99.remove(conditionsAt,conditionsSize);store(v99,99);
    assert(read_save_game_file(p,loaded,error));
    const auto& migrated=base(loaded.state);
    same_request(migrated.orbitalBase.retirement,base(s).orbitalBase.retirement);
    same_request(migrated.telemetry.orbitalBase.retirement,base(s).telemetry.orbitalBase.retirement);
    same_request(migrated.telemetryInTransit.back().telemetry.orbitalBase.retirement,base(s).orbitalBase.retirement);
    assert(migrated.pendingCommands.back().orbitalBase->action==OrbitalBaseAction::CancelRetirement);
    // Continuing from the migrated intent must keep the same physical result.
    auto control=TurnProcessor{}.process(s,{});
    auto restored=TurnProcessor{}.process(loaded.state,{});
    assert(base(restored).colonists==base(control).colonists);
    same_request(base(restored).orbitalBase.retirement,base(control).orbitalBase.retirement);
    assert(write_save_game_file(p,data(loaded.state),error));
    assert(read_save_game_file(p,loaded,error));assert(base(loaded.state).orbitalBase.retirement);

    auto main=world(5000);deploy(main);acknowledge(main);
    main.planets[0].defenses=17;main.planets[0].warDamage.contamination=3;
    main.planets[0].warDamage.lastBombardmentTurn=main.turn;
    set_survey_level(main,1,1,SurveyLevel::DeepSurvey,main.turn);
    const auto known=make_player_view(main,1).state;
    assert(find_planet(known,1)->defenses==17 && find_planet(known,1)->warDamage.contamination==3);
    assert(write_save_game_file(p,data(main),error));
    auto v100=bytes(p);const auto [lifecycleAt,lifecycleSize]=tail(v100,0x4f524c43);
    v100.remove(lifecycleAt,lifecycleSize);store(v100,100);
    assert(read_save_game_file(p,loaded,error));
    const auto& records=loaded.state.players[0].surveyKnowledge;
    const auto evidence=std::find_if(records.begin(),records.end(),[](const auto& r){return r.star==1;});
    assert(evidence!=records.end() && evidence->planetSnapshot && evidence->planetSnapshot->habitat
        && evidence->planetSnapshot->habitat->conditions);
    assert(evidence->planetSnapshot->habitat->conditions->defenses==17
        && evidence->planetSnapshot->habitat->conditions->damage.contamination==3);
    const auto received=make_player_view(loaded.state,1).state;
    assert(find_planet(received,1)->defenses==17 && find_planet(received,1)->warDamage.contamination==3);
    assert(!base(loaded.state).orbitalBase.retirement);
    // Current files require both tails, rather than silently dropping either.
    store(v100,101);assert(!read_save_game_file(p,loaded,error));
    store(v99,101);assert(!read_save_game_file(p,loaded,error));
}
void retirement_confirmation_follows_cargo(const QString& p) {
    for (bool mergeRecoveryTransport : {false,true}) {
        auto s=world(5000);deploy(s);recovery_transport(s);
        base(s).colonists=0;base(s).fuel=20;
        if (mergeRecoveryTransport) {
            auto final=s.fleets.back();final.id=s.nextFleetId++;final.position={5001,0};
            s.fleets.push_back(final);
        }
        acknowledge(s);
        apply_orbital_base_command(s,base(s),retirement());service_orbital_bases(s,s.turn);
        assert(s.pendingFleetLossContacts.size()==1);
        const auto old=make_player_view(s,1).state;
        const auto contact=std::find_if(old.fleets.begin(),old.fleets.end(),[](const auto& f){return f.id==1;});
        assert(contact!=old.fleets.end() && !contact->orbitalBase.retirement); // Hidden completion is not received intent.
        if (mergeRecoveryTransport) {
            auto& receiver=s.fleets.front();assert(receiver.id==2);
            receiver.destination=Position{5001,0};receiver.targetFleet=3;receiver.warp=1;
            receiver.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
            tick(s);
            assert(s.fleets.size()==1 && s.fleets.front().id==3);
            assert(s.pendingFleetLossContacts.size()==2);
        }
        QString error;SaveGameData loaded;
        assert(write_save_game_file(p,data(s),error));assert(read_save_game_file(p,loaded,error));
        s=std::move(loaded.state);
        assert(s.fleets.size()==1);
        const auto actual=s.fleets.front().minerals;
        s.fleets.front().position={0,0};++s.turn;publish_fleet_telemetry(s,s.turn);
        (void)deliver_due_player_reports(s);
        const auto received=make_player_view(s,1).state;
        assert(s.pendingFleetLossContacts.empty() && received.fleets.size()==1);
        assert(received.fleets.front().minerals.ironium==actual.ironium
            && received.fleets.front().minerals.boranium==actual.boranium
            && received.fleets.front().minerals.germanium==actual.germanium);
        assert(write_save_game_file(p,make_player_turn(data(s),1),error));
        assert(read_save_game_file(p,loaded,error));assert(loaded.state.fleets.size()==1);
    }
}
void lifecycle_persistence_and_player_exports(const QString& p) {
    auto s=world(5000);deploy(s);recovery_transport(s);base(s).fuel=20;acknowledge(s);
    const auto request=retirement(true);
    assert(submit_orbital_base_command(s,1,1,request));
    auto host=data(s);QString error;SaveGameData loaded;
    assert(write_save_game_file(p,host,error));assert(read_save_game_file(p,loaded,error));
    const auto& pending=base(loaded.state).pendingCommands.back().orbitalBase;
    assert(pending && pending->receiver==2 && pending->station==request.station && pending->item==request.item);
    // The accepted, blocked mission is distinct from an in-flight packet and survives its own tail.
    apply_orbital_base_command(s,base(s),request);base(s).pendingCommands.clear();
    host=data(s);assert(write_save_game_file(p,host,error));assert(read_save_game_file(p,loaded,error));
    assert(base(loaded.state).orbitalBase.retirement->station==request.station);
    assert(!base(loaded.state).telemetry.orbitalBase.retirement);
    tick(s);tick(loaded.state);
    assert(base(s).colonists==base(loaded.state).colonists);
    assert(base(s).orbitalBase.retirement->item==base(loaded.state).orbitalBase.retirement->item);
    assert(write_save_game_file(p,data(s),error));assert(read_save_game_file(p,loaded,error));
    assert(!base(loaded.state).telemetry.orbitalBase.retirement);
    assert(std::any_of(base(loaded.state).telemetryInTransit.begin(),base(loaded.state).telemetryInTransit.end(),
        [](const auto& packet){return packet.telemetry.orbitalBase.retirement.has_value();}));
    acknowledge(s);assert(write_save_game_file(p,data(s),error));assert(read_save_game_file(p,loaded,error));
    assert(base(loaded.state).telemetry.orbitalBase.retirement->item==request.item);
    auto corrupt=bytes(p);QByteArray lifecycleMarker;
    {QDataStream out(&lifecycleMarker,QIODevice::WriteOnly);out<<quint32{0x4f524c43};}
    const auto lifecycleAt=corrupt.lastIndexOf(lifecycleMarker);assert(lifecycleAt>0);
    corrupt[lifecycleAt+16]=char(2); // Invalid retirement-presence flag in the bounded tail.
    QFile bad(p+".bad-lifecycle");assert(bad.open(QIODevice::WriteOnly));bad.write(corrupt);bad.close();
    assert(!read_save_game_file(p+".bad-lifecycle",loaded,error));
    auto missingReceipt=s;base(missingReceipt).orbitalBase.acceptedItems.clear();base(missingReceipt).orbitalBase.lastItemId=0;
    assert(write_save_game_file(p,data(missingReceipt),error));assert(!read_save_game_file(p,loaded,error));
    // v98 ends at ORBI, before both CCON and ORLC; it gets no retirement intent.
    auto legacy=data(world());assert(write_save_game_file(p,legacy,error));auto raw=bytes(p);
    QByteArray marker;{QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32{0x4f524249};}
    const auto at=raw.lastIndexOf(marker);assert(at>0);quint32 size{};
    {QDataStream in(raw);assert(in.device()->seek(at+4));in>>size;}
    raw.resize(at+8+size);
    {QDataStream out(&raw,QIODevice::ReadWrite);assert(out.device()->seek(4));out<<quint32{98};}
    QFile old(p+".v98");assert(old.open(QIODevice::WriteOnly));old.write(raw);old.close();
    assert(read_save_game_file(p+".v98",loaded,error));assert(!base(loaded.state).orbitalBase.retirement);
    // PBEM preserves stable IDs, physical recipients, station fits and overtaking cancellation.
    TurnOrderFileData packet;packet.campaignId=55;packet.turn=s.turn;packet.turnToken=77;packet.orders.player=1;
    packet.orders.orders={OrbitalBaseOrder{1,request},OrbitalBaseOrder{1,retirement(false,101)},
        OrbitalBaseOrder{1,{OrbitalBaseAction::CancelRetirement,0,0,request.item}}};
    packet.descriptions={"Rebuild","Dismantle","Cancel retirement"};
    assert(write_turn_order_file(p+".orders",packet,error));TurnOrderFileData parsed;
    assert(read_turn_order_file(p+".orders",parsed,error));
    const auto& c=std::get<OrbitalBaseOrder>(parsed.orders.orders[0]).command;
    assert(c.station==request.station && c.receiver==2 && c.item==100);
    assert(std::get<OrbitalBaseOrder>(parsed.orders.orders[2]).command.action==OrbitalBaseAction::CancelRetirement);
    // Hidden completion and changed transport stocks cannot alter the old player export.
    auto remote=world(5000);deploy(remote);recovery_transport(remote);base(remote).colonists=0;base(remote).fuel=20;acknowledge(remote);
    auto before=make_player_turn(data(remote),1);assert(write_save_game_file(p+".before",before,error));
    apply_orbital_base_command(remote,base(remote),retirement());service_orbital_bases(remote,remote.turn);
    assert(!orbital_base_at(remote,2) && remote.pendingFleetLossContacts.size()==1);
    auto after=make_player_turn(data(remote),1);assert(write_save_game_file(p+".after",after,error));
    assert(bytes(p+".before")==bytes(p+".after"));
    assert(write_save_game_file(p,data(remote),error));assert(read_save_game_file(p,loaded,error));
    assert(loaded.state.pendingFleetLossContacts.size()==1 && !orbital_base_at(loaded.state,2));
    const auto receipt=remote.pendingFleetLossContacts[0].deliveryTurn;
    while(remote.turn<receipt){tick(remote);tick(loaded.state);}
    assert(remote.pendingFleetLossContacts.empty() && loaded.state.pendingFleetLossContacts.empty());
    assert(remote.fleets[0].minerals.ironium==loaded.state.fleets[0].minerals.ironium);
}

}
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir dir;assert(dir.isValid());retirement_confirmation_follows_cargo(dir.filePath("recovery-confirmation.suns"));parallel_branch_migrations(dir.filePath("parallel.suns"));lifecycle_persistence_and_player_exports(dir.filePath("lifecycle.suns"));roundtrip(dir.filePath("roundtrip.suns"));ai_intent_survives_reload(dir.filePath("ai.suns"));migration_and_corruption(dir.filePath("migration.suns"));hidden_exports_and_inflight(dir.filePath("remote.suns"));}
