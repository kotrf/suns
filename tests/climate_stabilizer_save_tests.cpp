#include "climate_stabilizer_fixture.hpp"
#include "save_game.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <cassert>
using namespace suns;using E=PlanetaryEngineeringKind;using A=EngineeringAction;
namespace {
QByteArray bytes(const QString& path){QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
void write(const QString& path,const QByteArray& data){QFile f(path);assert(f.open(QIODevice::WriteOnly));assert(f.write(data)==data.size());}
SaveGameData host()
{
    SaveGameData h;h.state=test::climate_fixture();h.mode=SessionMode::Host;h.campaignId=122;h.turnToken=17;h.playerTokens={{1,17}};
    h.pendingOrders={1,{}};return h;
}
void physical_received_pending_and_drafted_targets_survive_reload()
{
    auto h=host();auto& s=h.state;auto& p=s.planets.front();
    assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::StartOrResume,10,47}));
    (void)resolve_colony_production(s,p,true);++s.turn;set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    ++s.turn;assert(apply_planetary_engineering_order(s,p,{1,E::ClimateStabilizer,A::SetOutput,8,62}));
    PendingSurveyReport report;report.star=1;report.level=SurveyLevel::DeepSurvey;report.observedTurn=s.turn;
    report.deliveryTurn=s.turn+4;report.observedOwner=1;report.planetSnapshot=capture_planet_survey(s,1,1,report.level,s.turn);
    assert(report.planetSnapshot && report.planetSnapshot->engineering.targetTemperature==62);
    s.players.front().pendingSurveyReports.push_back(report);
    h.pendingOrders={1,{SetPlanetaryEngineeringOrder{1,E::ClimateStabilizer,A::SetOutput,7,44}}};h.pendingDescriptions={"Set target temperature"};
    Player mailbox;mailbox.id=2;mailbox.name="Remote player";mailbox.race=s.players.front().race;mailbox.technology=s.players.front().technology;
    s.players.push_back(mailbox);h.playerTokens[2]=19;s.planets[1].owner=2;s.planets[1].population=100000;
    h.inbox={{2,{SetPlanetaryEngineeringOrder{s.planets[1].id,E::ClimateStabilizer,A::StartOrResume,6,39}}}};
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto a=dir.filePath("a.sunsturn"),b=dir.filePath("b.sunsturn"),path=dir.filePath("host.suns");
    assert(write_save_game_file(a,make_player_turn(h,1),error));
    p.engineering.targetTemperature=70;(void)resolve_colony_production(s,p,true);s.orbitalStations.clear();refresh_planetary_engineering(s);
    assert(write_save_game_file(b,make_player_turn(h,1),error) && bytes(a)==bytes(b));
    if(!write_save_game_file(path,h,error) || !read_save_game_file(path,loaded,error)){qCritical()<<error;assert(false);}
    assert(loaded.state.planets.front().engineering.targetTemperature==70);
    assert(loaded.state.players.front().surveyKnowledge.front().planetSnapshot->engineering.targetTemperature==47);
    assert(loaded.state.players.front().pendingSurveyReports.back().planetSnapshot->engineering.targetTemperature==62);
    assert(std::get<SetPlanetaryEngineeringOrder>(loaded.pendingOrders.orders.front()).targetTemperature==44);
    assert(std::get<SetPlanetaryEngineeringOrder>(loaded.inbox.front().orders.front()).targetTemperature==39);
    assert(loaded.inbox.front().player==2);
    const auto oldTurn=make_player_turn(loaded,1);assert(oldTurn.state.planets.front().engineering.targetTemperature==47);
    assert(write_save_game_file(b,oldTurn,error) && read_save_game_file(b,loaded,error));
    assert(loaded.state.planets.front().engineering.targetTemperature==47);
    const auto future=report.deliveryTurn;h.state.turn=future;(void)deliver_due_survey_reports(h.state);
    assert(make_player_view(h.state,1).state.planets.front().engineering.targetTemperature==62);
    auto complete=host();test::finish_climate(complete.state,73);set_survey_level(complete.state,1,1,SurveyLevel::DeepSurvey,complete.state.turn);
    assert(write_save_game_file(path,complete,error) && read_save_game_file(path,loaded,error));
    const auto next=TurnProcessor{}.process(loaded.state,{});
    assert(next.planets.front().engineering.targetTemperature==73 && next.planets.front().engineering.operational);
    // A foreign orbital survey carries the visible setting, without leaking
    // subsequent private reconfiguration or station loss into a player file.
    auto foreign=host();test::finish_climate(foreign.state,73);
    auto other=foreign.state.players.front();other.id=2;other.name="Other";other.surveyKnowledge.clear();other.pendingSurveyReports.clear();
    foreign.state.players.push_back(other);foreign.playerTokens[2]=19;
    foreign.state.planets.front().owner=2;foreign.state.orbitalStations.front().owner=2;
    set_survey_level(foreign.state,1,1,SurveyLevel::OrbitalSurvey,foreign.state.turn);
    assert(write_save_game_file(a,make_player_turn(foreign,1),error));
    foreign.state.planets.front().engineering.targetTemperature=14;foreign.state.orbitalStations.clear();refresh_planetary_engineering(foreign.state);
    assert(write_save_game_file(b,make_player_turn(foreign,1),error) && bytes(a)==bytes(b));
    assert(read_save_game_file(b,loaded,error) && loaded.state.planets.front().engineering.targetTemperature==73);
    assert(loaded.state.planets.front().engineering.invested.ironium==0);
}
void versioned_orders_and_unchanged_legacy_records()
{
    QTemporaryDir dir;QString error;TurnOrderFileData decoded;
    TurnOrderFileData data{122,3,17,{1,{SetPlanetaryEngineeringOrder{1,E::ClimateStabilizer,A::StartOrResume,10,0},
        SetPlanetaryEngineeringOrder{1,E::ClimateStabilizer,A::SetOutput,6,100}}},{"Start climate","Retarget climate"}};
    const auto path=dir.filePath("climate.sunsorders");assert(write_turn_order_file(path,data,error) && read_turn_order_file(path,decoded,error));
    assert(std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.front()).targetTemperature==0);
    assert(std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.back()).targetTemperature==100);
    auto old=bytes(path);assert(quint8(old[7])==55);old[7]=char(53);write(path,old);assert(!read_turn_order_file(path,decoded,error));
    for(bool wrongKind:{false,true}) {
        auto bad=data;auto& order=std::get<SetPlanetaryEngineeringOrder>(bad.orders.orders.front());
        if(wrongKind){order.kind=E::OrbitalMirrors;order.targetTemperature=48;}else order.targetTemperature=101;
        assert(!write_turn_order_file(path,bad,error) || !read_turn_order_file(path,decoded,error));
    }
    // Non-climate records have exactly the previous layout. No new field is
    // inserted when an old v125/v53 engineering kind is written.
    auto h=host();assert(apply_planetary_engineering_order(h.state,h.state.planets.front(),{1,E::OrbitalMirrors,A::StartOrResume,7}));
    h.pendingOrders={1,{SetPlanetaryEngineeringOrder{1,E::OrbitalMirrors,A::SetOutput,5}}};h.pendingDescriptions={"Old output"};
    const auto save=dir.filePath("legacy-layout.suns");SaveGameData loaded;
    assert(write_save_game_file(save,h,error));old=bytes(save);old[7]=char(125);write(save,old);
    assert(read_save_game_file(save,loaded,error) && loaded.state.planets.front().engineering.targetTemperature==50);
    data.orders.orders={SetPlanetaryEngineeringOrder{1,E::OrbitalMirrors,A::SetOutput,5}};data.descriptions={"Old output"};
    assert(write_turn_order_file(path,data,error));old=bytes(path);old[7]=char(53);write(path,old);
    assert(read_turn_order_file(path,decoded,error) && std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.front()).targetTemperature==50);
    h=host();test::finish_climate(h.state,70);assert(write_save_game_file(save,h,error));
    old=bytes(save);old[7]=char(125);write(save,old);assert(!read_save_game_file(save,loaded,error));
    h.state.planets.front().engineering.targetTemperature=101;assert(!write_save_game_file(save,h,error));
}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);physical_received_pending_and_drafted_targets_survive_reload();versioned_orders_and_unchanged_legacy_records();
}
