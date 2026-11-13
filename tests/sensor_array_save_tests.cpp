#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/deep_space.hpp"
#include "suns/player_knowledge.hpp"
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cassert>
using namespace suns;
namespace {
QByteArray bytes(const QString& path)
{
    QFile file(path);assert(file.open(QIODevice::ReadOnly));return file.readAll();
}
SaveGameData deployed()
{
    SaveGameData host;host.campaignId=54;host.turnToken=17;host.mode=SessionMode::Host;
    host.pendingOrders={1,{}};host.playerTokens={{1,17}};host.state=make_demo_game();
    auto& s=host.state;s.stars.resize(2);s.planets.resize(2);s.fleets.clear();
    s.stars[0].position={0,0};s.stars[1].position={1040,0};
    auto& player=s.players.front();player.technology.levels.fill(26);
    player.pendingSurveyReports.clear();player.pendingPlayerReports.clear();
    player.surveyedStars={1};std::erase_if(player.surveyKnowledge,[](const auto& k){return k.star!=1;});
    s.shipDesigns.push_back({100,1,"Sensor constructor",ShipHullType::Scout,
        {ShipComponentType::FusionDrive,ShipComponentType::SensorArrayKit}});
    normalize_ship_design_placement(s.shipDesigns.back());s.nextShipDesignId=101;
    Fleet f;f.id=1;f.owner=1;f.name="Distant sensor array";f.design=100;f.ships={{100,1}};
    f.position={1000,0};f.fuel=20;f.task=FleetTask::DeployOutpost;
    f.telemetry.observedTurn=s.turn;f.telemetry.position=f.position;f.telemetry.fuel=f.fuel;f.telemetry.ships=f.ships;
    s.fleets.push_back(f);s.nextFleetId=2;
    deploy_outposts(s,s.turn);publish_fleet_telemetry(s,s.turn);observe_current_sensor_coverage(s,s.turn);
    assert(s.fleets.front().outpost==OutpostKind::SensorArray);
    return host;
}
void delayed_deployment_scans_and_host_player_round_trip()
{
    auto host=deployed();QTemporaryDir dir;QString error;SaveGameData loaded;
    const auto path=dir.filePath("host.suns");
    assert(write_save_game_file(path,host,error) && read_save_game_file(path,loaded,error));host=loaded;
    assert(host.state.fleets.front().outpost==OutpostKind::SensorArray);
    assert(!host.state.fleets.front().telemetryInTransit.empty());
    assert(host.state.fleets.front().telemetryInTransit.front().telemetry.outpost==OutpostKind::SensorArray);
    auto view=make_player_view(host.state,1);
    assert(view.state.fleets.front().outpost==OutpostKind::None && fleet_sensor_range(view.state,view.state.fleets.front())==0);
    assert(survey_level(host.state,1,2)==SurveyLevel::Detected);
    const auto pending=std::find_if(host.state.players.front().pendingSurveyReports.begin(),host.state.players.front().pendingSurveyReports.end(),
        [](const auto& p){return p.star==2;});
    assert(pending!=host.state.players.front().pendingSurveyReports.end() && pending->deliveryTurn>host.state.turn);
    assert(pending->level==SurveyLevel::BasicScan && pending->planetSnapshot);
    assert(!pending->planetSnapshot->environment && !pending->planetSnapshot->surfaceMinerals);
    const auto delivery=pending->deliveryTurn;
    const auto a=dir.filePath("a.sunsturn"),b=dir.filePath("b.sunsturn");
    assert(write_save_game_file(a,make_player_turn(host,1),error));
    auto hidden=host;hidden.state.fleets.front().fuel=0;hidden.state.fleets.front().damagePercent=100;
    hidden.state.fleets.front().outpost=OutpostKind::None;
    assert(write_save_game_file(b,make_player_turn(hidden,1),error) && bytes(a)==bytes(b));
    host.state.turn=delivery;
    (void)deliver_due_survey_reports(host.state);deliver_due_fleet_telemetry(host.state);(void)deliver_due_player_reports(host.state);
    view=make_player_view(host.state,1);
    assert(view.state.fleets.front().outpost==OutpostKind::SensorArray);
    assert(fleet_ordinary_sensor_range(view.state,view.state.fleets.front())==250);
    assert(fleet_penetrating_sensor_range(view.state,view.state.fleets.front())==75);
    assert(survey_level(host.state,1,2)>=SurveyLevel::BasicScan);
    assert(write_save_game_file(b,make_player_turn(host,1),error) && read_save_game_file(b,loaded,error));
    assert(loaded.state.fleets.front().outpost==OutpostKind::SensorArray);
    assert(fleet_sensor_range(loaded.state,loaded.state.fleets.front())==250);
    // Invalid kit/anchor pairings still reject instead of creating a service.
    host.state.shipDesigns.back().components.back()=ShipComponentType::DeepSpaceRelayKit;
    assert(!write_save_game_file(path,host,error));
}
void new_component_orders_and_unchanged_v122_wire_layout()
{
    QTemporaryDir dir;QString error;const auto path=dir.filePath("sensor.sunsorders");
    TurnOrderFileData packet{54,1,17,{1,{CreateShipDesignOrder{"Sensor constructor",ShipHullType::Scout,
        {ShipComponentType::FusionDrive,ShipComponentType::SensorArrayKit}},SetFleetTaskOrder{1,FleetTask::DeployOutpost}}},
        {"Create paid design","Deploy"}},decoded;
    assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,decoded,error));
    assert(std::get<CreateShipDesignOrder>(decoded.orders.orders.front()).components.back()==ShipComponentType::SensorArrayKit);
    QFile file(path);assert(file.open(QIODevice::ReadWrite));auto old= file.readAll();
    QDataStream patch(&old,QIODevice::ReadWrite);assert(patch.device()->seek(4));patch<<quint32{51};
    assert(file.resize(0) && file.seek(0) && file.write(old)==old.size());file.close();
    assert(!read_turn_order_file(path,decoded,error)); // v51 never knew the kit.
    // v123 only extends enum ranges. A record using old identities has the
    // exact v122 layout; this is a wire-layout check, not a historical fixture.
    auto host=deployed();host.state.shipDesigns.back().components.back()=ShipComponentType::DeepSpaceRelayKit;
    host.state.shipDesigns.back().placements.clear();normalize_ship_design_placement(host.state.shipDesigns.back());
    for(auto& f:host.state.fleets) {
        f.outpost=OutpostKind::CommunicationsRelay;
        for(auto& t:f.telemetryInTransit)t.telemetry.outpost=OutpostKind::CommunicationsRelay;
    }
    for(auto& report:host.state.players.front().pendingPlayerReports)
        if(report.kind==PlayerReportKind::OutpostDeployed)report.quantity=std::uint32_t(OutpostKind::CommunicationsRelay);
    const auto save=dir.filePath("old-layout.suns");assert(write_save_game_file(save,host,error));
    SaveGameData current;assert(read_save_game_file(save,current,error));
    QFile source(save);assert(source.open(QIODevice::ReadWrite));old=source.readAll();
    QDataStream header(&old,QIODevice::ReadWrite);assert(header.device()->seek(4));header<<quint32{122};
    assert(source.resize(0) && source.seek(0) && source.write(old)==old.size());source.close();
    SaveGameData loaded;assert(read_save_game_file(save,loaded,error));
    assert(loaded.state.fleets.front().outpost==OutpostKind::CommunicationsRelay);
    assert(loaded.state.shipDesigns.back().components.back()==ShipComponentType::DeepSpaceRelayKit);
}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    delayed_deployment_scans_and_host_player_round_trip();
    new_component_orders_and_unchanged_v122_wire_layout();
}
