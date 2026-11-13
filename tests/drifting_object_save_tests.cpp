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
    h.state.fleets.front().task=FleetTask::RecoverSample;
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
    loaded.state.fleets.front().scienceSamples.clear();loaded.state.fleets.front().task=FleetTask::RecoverSample;
    assert(write_save_game_file(path,loaded,error) && read_save_game_file(path,h,error));
    h.state=TurnProcessor{}.process(h.state,{});
    assert(h.state.unknownObjects.front().sampleRecovered && h.state.fleets.front().scienceSamples.empty());
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
    new_tags_cannot_masquerade_as_v123_and_invalid_courses_reject();
}
