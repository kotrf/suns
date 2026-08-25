#include "save_game.hpp"

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>

namespace {
using namespace suns;

void round_trip_preserves_communications_and_planning()
{
    SaveGameData original;
    original.galaxyConfig = GalaxyConfig{20260825, 24, 940.0, 700.0, 50.0};
    original.state = generate_game(original.galaxyConfig);
    original.state.turn = 77;
    original.state.planets.front().mines = 17;

    auto& scout = original.state.fleets.front();
    scout.position = {420.0, 10.0};
    scout.destination = Position{600.0, 20.0};
    scout.warp = 8;
    scout.fuel = 217.75;
    scout.colonists = 1234;
    scout.minerals = {1.25, 2.5, 3.75};
    scout.pendingCommands.push_back({
        77,
        80,
        {{0.0, 0.0}, 7, {FleetArrivalActionKind::Refuel, 1}, {{{90.0, 30.0}, 6, {}}}, true},
    });
    scout.telemetry = {
        75,
        {300.0, 10.0},
        Position{600.0, 20.0},
        8,
        250.0,
        900,
        std::nullopt,
        {{{600.0, 20.0}, 8, {}}},
        {4.0, 5.0, 6.0},
    };
    scout.telemetryInTransit.push_back({
        79,
        {76, {360.0, 10.0}, Position{600.0, 20.0}, 8, 230.0, 950,
         std::nullopt, {}, {7.0, 8.0, 9.0}},
    });

    MoveFleetOrder move;
    move.fleet = scout.id;
    move.destination = {200.0, -100.0};
    move.warp = 8;
    original.pendingOrders = {1, {move, QueueProductionOrder{1, ProductionKind::Mine}}};
    original.pendingDescriptions = {"move", "mine"};
    original.selectedStar = 2;
    original.selectedFleet = scout.id;
    original.showSensorRanges = false;

    QTemporaryDir directory;
    assert(directory.isValid());
    const auto path = directory.filePath("campaign.suns");
    QString error;
    assert(write_save_game_file(path, original, error));

    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(error.isEmpty());
    assert(loaded.state.turn == 77);
    assert(loaded.state.planets.front().mines == 17);

    const auto& fleet = loaded.state.fleets.front();
    assert(same_position(fleet.position, {420.0, 10.0}));
    assert(fleet.pendingCommands.size() == 1);
    assert(fleet.pendingCommands.front().issuedTurn == 77);
    assert(fleet.pendingCommands.front().deliveryTurn == 80);
    assert(fleet.pendingCommands.front().program.warp == 7);
    assert(fleet.pendingCommands.front().program.queuedWaypoints.size() == 1);
    assert(fleet.pendingCommands.front().program.clearRoute);
    assert(fleet.telemetry.observedTurn == 75);
    assert(same_position(fleet.telemetry.position, {300.0, 10.0}));
    assert(fleet.telemetry.destination.has_value());
    assert(fleet.telemetry.colonists == 900);
    assert(fleet.telemetry.minerals.germanium == 6.0);
    assert(fleet.telemetryInTransit.size() == 1);
    assert(fleet.telemetryInTransit.front().deliveryTurn == 79);
    assert(fleet.telemetryInTransit.front().telemetry.observedTurn == 76);

    assert(loaded.pendingOrders.orders.size() == 2);
    const auto* mine = std::get_if<QueueProductionOrder>(&loaded.pendingOrders.orders[1]);
    assert(mine && mine->kind == ProductionKind::Mine);
    assert(loaded.pendingDescriptions == original.pendingDescriptions);
    assert(loaded.selectedStar == original.selectedStar);
    assert(loaded.selectedFleet == original.selectedFleet);
    assert(!loaded.showSensorRanges);
}

void old_format_is_rejected_cleanly()
{
    QTemporaryDir directory;
    assert(directory.isValid());
    const auto path = directory.filePath("old.suns");
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_4);
    stream << quint32{0x53554E53u} << quint32{2};
    file.close();

    SaveGameData loaded;
    QString error;
    assert(!read_save_game_file(path, loaded, error));
    assert(error.contains("Unsupported Suns! save version"));
}

} // namespace

int main()
{
    round_trip_preserves_communications_and_planning();
    old_format_is_rejected_cleanly();
    std::cout << "save game tests passed\n";
    return 0;
}
