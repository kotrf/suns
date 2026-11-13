#include "drifting_object_fixture.hpp"
#include "save_game.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/science_samples.hpp"
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cassert>
#include <limits>
using namespace suns;
namespace {
SaveGameData host()
{
    SaveGameData h;h.state=test::drifting_object_fixture();h.mode=SessionMode::Host;
    h.campaignId=55;h.turnToken=17;h.playerTokens={{1,17}};h.pendingOrders={1,{}};return h;
}
QByteArray bytes(const QString& path)
{QFile file(path);assert(file.open(QIODevice::ReadOnly));return file.readAll();}
void physical_course_reports_and_received_archive_stay_separate()
{
    auto h=host();auto& s=h.state;
    auto& object=s.unknownObjects.front();object.position={401.5,0.5};
    auto& f=s.fleets.front();f.position={390,0};f.telemetry.position=f.position;f.telemetry.observedTurn=9;
    f.task=FleetTask::FieldResearch;
    ObjectObservation previous{7,{400,0},0,ObjectKnowledgeLevel::Classified,object.kind,9,9,object.velocity};
    s.players.front().objectKnowledge={previous};research_unknown_objects(s,f,s.turn);publish_fleet_telemetry(s,s.turn);
    assert(!s.players.front().pendingPlayerReports.empty());
    const auto packet=s.players.front().pendingPlayerReports.front();assert(packet.objectObservation && packet.deliveryTurn>s.turn);
    QTemporaryDir dir;QString error;SaveGameData loaded;
    const auto save=dir.filePath("host.suns"),a=dir.filePath("before.sunsturn"),b=dir.filePath("after.sunsturn");
    assert(write_save_game_file(save,h,error) && read_save_game_file(save,loaded,error));
    assert(loaded.state.unknownObjects.front().velocity==object.velocity);
    assert(loaded.state.players.front().pendingPlayerReports.front().objectObservation==packet.objectObservation);
    const auto control=TurnProcessor{}.process(s,{}),resumed=TurnProcessor{}.process(loaded.state,{});
    assert(control.unknownObjects.front().position==resumed.unknownObjects.front().position);
    const auto& x=control.players.front().fieldScience;const auto& y=resumed.players.front().fieldScience;
    assert(x.size()==y.size() && std::equal(x.begin(),x.end(),y.begin(),[](const auto& a,const auto& b){
        return a.key==b.key && a.deliveryTurn==b.deliveryTurn && a.transmissionFleet==b.transmissionFleet;
    }));
    assert(write_save_game_file(a,make_player_turn(h,1),error));
    object.position={777,777};object.velocity={-1.5,0.5};object.sampleRecovered=true;
    assert(write_save_game_file(b,make_player_turn(h,1),error) && bytes(a)==bytes(b));
    s.turn=packet.deliveryTurn;h.strategicMessages=deliver_due_player_reports(s);
    const auto* known=known_unknown_object(s,1,7);
    assert(known && known->position==packet.objectObservation->position && known->velocity==Position(1.5,0.5));
    assert(known->level==ObjectKnowledgeLevel::Studied && known->position!=object.position);
    assert(write_save_game_file(save,h,error) && read_save_game_file(save,loaded,error));
    assert(known_unknown_object(loaded.state,1,7)->velocity==known->velocity);
    assert(!loaded.strategicMessages.empty() && loaded.strategicMessages.front().objectObservation->velocity==known->velocity);
    assert(write_save_game_file(b,make_player_turn(h,1),error) && read_save_game_file(b,loaded,error));
    assert(loaded.state.unknownObjects.empty() && known_unknown_object(loaded.state,1,7)->position==known->position);
}
void the_unique_core_and_onboard_rendezvous_survive_reload()
{
    auto h=host();h.state.players.front().objectKnowledge={test::drifting_observation(h.state)};
    h.state.fleets.front().task=FleetTask::RecoverSample;h.state.fleets.front().recoveryTarget=7;
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("core.suns");
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    h.state=TurnProcessor{}.process(loaded.state,{});
    assert(h.state.unknownObjects.front().sampleRecovered && h.state.fleets.front().scienceSamples.size()==1);
    assert(h.state.fleets.front().position==h.state.unknownObjects.front().position);
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets.front().scienceSamples.front().kind==UnknownObjectKind::DriftingDerelict);
    const auto player=make_player_turn(loaded,1);const auto pbem=dir.filePath("core.sunsturn");
    assert(write_save_game_file(pbem,player,error));SaveGameData received;assert(read_save_game_file(pbem,received,error));
    assert(received.state.fleets.front().scienceSamples==h.state.fleets.front().scienceSamples);
    // Destruction/consumption does not restore the global stock after loading.
    loaded.state.fleets.front().scienceSamples.clear();loaded.state.fleets.front().task=FleetTask::RecoverSample;loaded.state.fleets.front().recoveryTarget=7;
    assert(write_save_game_file(path,loaded,error) && read_save_game_file(path,h,error));
    h.state=TurnProcessor{}.process(h.state,{});
    assert(h.state.unknownObjects.front().sampleRecovered && h.state.fleets.front().scienceSamples.empty());
}
void static_target_survives_silence_save_and_received_telemetry()
{
    auto h=host();h.state.unknownObjects.front().position={119,0};
    h.state.players.front().objectKnowledge={test::drifting_observation(h.state)};
    h.state.unknownObjects.push_back({8,{111,0},UnknownObjectKind::AncientProbe});h.state.nextUnknownObjectId=9;
    h.state.players.front().objectKnowledge.push_back({8,{111,0},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::AncientProbe,10,10});
    ElectronicsProgram silent;silent.mode=EmissionMode::RadioSilence;silent.resumeTurn=12;
    h.state=TurnProcessor{}.process(h.state,{{1,{SetFleetElectronicsOrder{1,silent},
        MoveFleetOrder{1,{111,0},5,{FleetArrivalActionKind::RecoverSample}}}}});
    assert(h.state.fleets.front().recoveryTarget==8 && h.state.fleets.front().scienceSamples.empty());
    QTemporaryDir dir;QString error;SaveGameData loaded;
    const auto a=dir.filePath("received-before.sunsturn"),b=dir.filePath("received-after.sunsturn");
    assert(write_save_game_file(a,make_player_turn(h,1),error));
    h.state.fleets.front().recoveryTarget=7; // Unreceived host intent cannot rewrite the old contact.
    assert(write_save_game_file(b,make_player_turn(h,1),error) && bytes(a)==bytes(b));
    h.state.fleets.front().recoveryTarget=8;
    const auto path=dir.filePath("waiting.suns");assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets.front().recoveryTarget==8);
    const auto fuel=loaded.state.fleets.front().fuel;
    loaded.state=TurnProcessor{}.process(loaded.state,{});
    assert(loaded.state.fleets.front().position==Position(111,0) && loaded.state.fleets.front().fuel==fuel);
    assert(loaded.state.fleets.front().scienceSamples.size()==1 && loaded.state.fleets.front().scienceSamples.front().source==8);
    assert(!loaded.state.unknownObjects.front().sampleRecovered && loaded.state.unknownObjects.back().sampleRecovered);
    assert(loaded.state.fleets.front().recoveryTarget==0);
}
void standing_target_is_received_only_through_frozen_telemetry()
{
    auto h=host();h.state.players.front().objectKnowledge={test::drifting_observation(h.state)};
    auto& fleet=h.state.fleets.front();fleet.task=FleetTask::RecoverSample;fleet.recoveryTarget=7;
    fleet.electronics.mode=EmissionMode::Standard;
    publish_fleet_telemetry(h.state,h.state.turn);
    assert(fleet.telemetry.task==FleetTask::RecoverSample && fleet.telemetry.recoveryTarget==7);
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("intent.sunsturn");
    assert(write_save_game_file(path,make_player_turn(h,1),error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets.front().recoveryTarget==7 && loaded.state.fleets.front().telemetry.recoveryTarget==7);
    fleet.position={390,0};fleet.telemetry.position=fleet.position;
    publish_fleet_telemetry(h.state,h.state.turn+1);++h.state.turn;
    assert(!fleet.telemetryInTransit.empty() && fleet.telemetryInTransit.back().telemetry.recoveryTarget==7);
    fleet.task=FleetTask::None;fleet.recoveryTarget=0;
    const auto hostPath=dir.filePath("outbox.suns");assert(write_save_game_file(hostPath,h,error) && read_save_game_file(hostPath,loaded,error));
    assert(loaded.state.fleets.front().telemetryInTransit.back().telemetry.recoveryTarget==7);
}
void delayed_routes_tasks_and_orders_freeze_their_target()
{
    auto h=host();h.state.fleets.front().position={390,0};h.state.fleets.front().telemetry.position={390,0};
    h.state.unknownObjects.front().position={411,0};
    h.state.players.front().objectKnowledge={test::drifting_observation(h.state)};
    h.state.unknownObjects.push_back({8,{400,0},UnknownObjectKind::AncientProbe});h.state.nextUnknownObjectId=9;
    h.state.players.front().objectKnowledge.push_back({8,{400,0},0,ObjectKnowledgeLevel::Studied,UnknownObjectKind::AncientProbe,10,10});
    FleetArrivalAction recovery{FleetArrivalActionKind::RecoverSample};recovery.recoveryTarget=7;
    const FleetWaypoint final{{414,1},5,recovery};
    auto route=h;
    assert(submit_fleet_route_command(route.state,1,1,{400,0},5,{}, {final}));
    assert(!route.state.fleets.front().pendingCommands.empty());
    assert(route.state.fleets.front().pendingCommands.front().deliveryTurn>route.state.turn);
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("pending.suns");
    assert(write_save_game_file(path,route,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets.front().pendingCommands.front().program.queuedWaypoints.front().arrivalAction.recoveryTarget==7);
    h.state.fleets.front().position={400,0};h.state.fleets.front().telemetry.position={400,0};
    assert(submit_fleet_task_command(h.state,1,1,FleetTask::RecoverSample));
    assert(h.state.fleets.front().pendingCommands.front().recoveryTarget==8);
    // A later received contact at the fleet cannot rebind the old task packet.
    h.state.players.front().objectKnowledge.front().position={400,0};
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    const auto due=loaded.state.fleets.front().pendingCommands.front().deliveryTurn;
    loaded.state.turn=due;deliver_due_fleet_commands(loaded.state);
    assert(loaded.state.fleets.front().task==FleetTask::RecoverSample && loaded.state.fleets.front().recoveryTarget==8);
    TurnOrderFileData data{55,10,17,{1,{MoveFleetOrder{1,{414,1},5,recovery},
        SetFleetTaskOrder{1,FleetTask::RecoverSample,8},MoveFleetOrder{1,{400,0},5,{}, {final}}}},
        {"Recover X7","Work on X8","Route to X7"}};
    const auto orderPath=dir.filePath("targets.sunsorders");TurnOrderFileData decoded;
    assert(write_turn_order_file(orderPath,data,error) && read_turn_order_file(orderPath,decoded,error));
    assert(std::get<MoveFleetOrder>(decoded.orders.orders[0]).arrivalAction.recoveryTarget==7);
    assert(std::get<SetFleetTaskOrder>(decoded.orders.orders[1]).recoveryTarget==8);
    assert(std::get<MoveFleetOrder>(decoded.orders.orders[2]).queuedWaypoints.front().arrivalAction.recoveryTarget==7);
    for(unsigned invalid=0;invalid<4;++invalid) {
        auto bad=data;
        if(invalid==0)std::get<MoveFleetOrder>(bad.orders.orders[0]).arrivalAction.recoveryTarget=0x80000000U;
        if(invalid==1)std::get<MoveFleetOrder>(bad.orders.orders[0]).arrivalAction.kind=FleetArrivalActionKind::None;
        if(invalid==2)std::get<SetFleetTaskOrder>(bad.orders.orders[1]).recoveryTarget=0x80000000U;
        if(invalid==3)std::get<SetFleetTaskOrder>(bad.orders.orders[1]).task=FleetTask::None;
        const bool wrote=write_turn_order_file(orderPath,bad,error);assert(!wrote || !read_turn_order_file(orderPath,decoded,error));
    }
}
void new_tags_cannot_masquerade_as_v123_and_invalid_courses_reject()
{
    auto h=host();h.state.players.front().objectKnowledge={test::drifting_observation(h.state)};
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("course.suns");
    assert(write_save_game_file(path,h,error));QFile file(path);assert(file.open(QIODevice::ReadWrite));auto old=file.readAll();
    QDataStream patch(&old,QIODevice::ReadWrite);assert(patch.device()->seek(4));patch<<quint32{123};
    assert(file.resize(0) && file.seek(0) && file.write(old)==old.size());file.close();
    assert(!read_save_game_file(path,loaded,error));
    for(unsigned invalid=0;invalid<5;++invalid) {
        auto bad=h;
        if(invalid==0)bad.state.unknownObjects.front().velocity={};
        if(invalid==1)bad.state.unknownObjects.front().velocity={3,0};
        if(invalid==2)bad.state.unknownObjects.front().velocity.x=std::numeric_limits<double>::quiet_NaN();
        if(invalid==3)bad.state.players.front().objectKnowledge.front().velocity.reset();
        if(invalid==4)bad.state.players.front().objectKnowledge.front().kind.reset();
        const bool wrote=write_save_game_file(path,bad,error);assert(!wrote || !read_save_game_file(path,loaded,error));
    }
}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    physical_course_reports_and_received_archive_stay_separate();
    the_unique_core_and_onboard_rendezvous_survive_reload();
    static_target_survives_silence_save_and_received_telemetry();
    standing_target_is_received_only_through_frozen_telemetry();
    delayed_routes_tasks_and_orders_freeze_their_target();
    new_tags_cannot_masquerade_as_v123_and_invalid_courses_reject();
}
