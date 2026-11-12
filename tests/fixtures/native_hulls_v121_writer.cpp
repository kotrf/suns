// Genuine synthetic save121/orders50 data. Build with the unmodified headers,
// core and save writer at 311509178aa9cb9a7f3858ea2154e7242b66aeca, never the new writer.
#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include "suns/stations.hpp"
#include <QDir>
#include <cstdio>
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const QDir directory(QString::fromLocal8Bit(argv[1]));
    suns::SaveGameData host; host.mode = suns::SessionMode::Host;
    host.campaignId = 122; host.turnToken = 17; host.playerTokens = {{1,17}};
    host.state = suns::test::empty_native_orbital_start();
    const auto home = host.state.planets.front().id;
    auto budget = suns::native_orbital_start_budget();
    budget.minerals.ironium += 400; budget.minerals.boranium += 400; budget.minerals.germanium += 400;
    const auto started = suns::initialize_native_orbital_start(host.state,1,home,budget);
    if (!started) return 3;
    auto& player = host.state.players.front();
    player.technology.levels[std::size_t(suns::ResearchField::Construction)] = 16;
    player.technology.researchActive = false;
    const suns::StationLoadout starbase{suns::OrbitalStationHullType::Starbase,
        {suns::ShipComponentType::BulkRadiationScreen},true,true};
    const suns::PlayerOrders build{1,{suns::QueueStationOrder{home,{starbase,started->station}}}};
    if (!suns::station_construction_valid(host.state,host.state.planets.front(),{starbase,started->station})) return 6;
    for (unsigned year=0;year<8 && host.state.orbitalStations.front().hull!=starbase.hull;++year)
        host.state = suns::TurnProcessor{}.process(host.state,year ? std::vector<suns::PlayerOrders>{} : std::vector<suns::PlayerOrders>{build});
    if (host.state.orbitalStations.front().hull != starbase.hull) return 4;
    const suns::StationLoadout fortress{suns::OrbitalStationHullType::Fortress,
        {suns::ShipComponentType::BulkRadiationScreen},true,true};
    host.pendingOrders = {1,{suns::QueueStationOrder{home,{fortress,started->station}}}};
    host.pendingDescriptions = {"Refit existing Starbase to Fortress"};
    auto playerTurn = suns::make_player_turn(host,1);
    playerTurn.pendingOrders = host.pendingOrders; playerTurn.pendingDescriptions = host.pendingDescriptions;
    suns::TurnOrderFileData packet{host.campaignId,host.state.turn,host.turnToken,host.pendingOrders,host.pendingDescriptions};
    QString error;
    if (!suns::write_save_game_file(directory.filePath("native-hulls-v121-host.suns"),host,error)
        || !suns::write_save_game_file(directory.filePath("native-hulls-v121-player.suns"),playerTurn,error)
        || !suns::write_turn_order_file(directory.filePath("native-hulls-v50.sunsorders"),packet,error)) {
        std::fprintf(stderr,"%s\n",qPrintable(error)); return 5;
    }
}
