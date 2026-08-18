#include "save_game.hpp"

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

using namespace suns;

void round_trip_preserves_campaign_and_planning_phase()
{
    SaveGameData original;
    original.galaxyConfig = GalaxyConfig{18446744073709551557ULL, 24, 940.0, 700.0, 50.0};
    original.state = generate_game(original.galaxyConfig);
    original.state.turn = 9007199254740993ULL; // Deliberately beyond exact JSON double range.
    original.state.players.front().radiationTolerance = 0.83;
    original.state.players.front().radiationImmune = true;
    original.state.planets.front().population = 9007199254740995ULL;
    original.state.planets.front().productionQueue.push_back({ProductionKind::Factory, 4, 0});
    original.state.planets.front().productionQueue.push_back({ProductionKind::ColonyShip, 9, kColonyShipDesignId});
    original.state.shipDesigns.push_back({
        3,
        1,
        "Long Range Colonizer",
        ShipHullType::MediumTransport,
        {ShipComponentType::RamScoopDrive, ShipComponentType::LongRangeScanner,
         ShipComponentType::ColonyModule, ShipComponentType::FuelTank},
    });
    original.state.nextShipDesignId = 4;

    auto& scout = original.state.fleets.front();
    scout.destination = Position{123.5, -77.25};
    scout.warp = 9;
    scout.fuel = 217.75;
    scout.colonists = 1234;
    scout.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Refuel, 1};
    scout.waypointQueue = {
        {{40.0, 10.0}, 8, {FleetArrivalActionKind::None, 1}},
        {{90.0, 20.0}, 7, {FleetArrivalActionKind::UnloadAllColonists, 1}},
    };
    scout.minerals = {1.25, 2.5, 3.75};

    MoveFleetOrder move;
    move.fleet = scout.id;
    move.destination = {200.0, -100.0};
    move.warp = 8;
    move.arrivalAction = {FleetArrivalActionKind::Colonize, 1};
    move.queuedWaypoints.push_back({{250.0, -120.0}, 7, {FleetArrivalActionKind::Refuel, 1}});

    original.pendingOrders.player = 1;
    original.pendingOrders.orders = {
        move,
        QueueProductionOrder{1, ProductionKind::Factory},
        CreateShipDesignOrder{"Pending Surveyor", ShipHullType::Scout,
            {ShipComponentType::FusionDrive, ShipComponentType::LongRangeScanner}},
        QueueShipDesignOrder{1, 3},
        SetFleetColonistsOrder{1, 1, 444},
        SetFleetMineralCargoOrder{1, 1, {4.0, 5.0, 6.0}},
        RefuelFleetOrder{1, 1},
        ColonizePlanetOrder{1, 2},
    };
    original.pendingDescriptions = {
        "move", "factory", "design", "ship", "colonists", "minerals", "refuel", "colonize",
    };
    original.selectedStar = 2;
    original.selectedFleet = 1;
    original.showSensorRanges = false;

    QTemporaryDir directory;
    assert(directory.isValid());
    const auto path = directory.filePath("campaign.suns");

    QString error;
    assert(write_save_game_file(path, original, error));
    assert(error.isEmpty());

    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(error.isEmpty());

    assert(loaded.galaxyConfig.seed == original.galaxyConfig.seed);
    assert(loaded.galaxyConfig.starCount == original.galaxyConfig.starCount);
    assert(loaded.galaxyConfig.width == original.galaxyConfig.width);
    assert(loaded.state.turn == 9007199254740993ULL);
    assert(loaded.state.galaxySeed == original.state.galaxySeed);
    assert(loaded.state.players.front().radiationTolerance == 0.83);
    assert(loaded.state.players.front().radiationImmune);
    assert(loaded.state.planets.front().population == 9007199254740995ULL);
    assert(loaded.state.planets.front().productionQueue.size() == 2);
    assert(loaded.state.shipDesigns.size() == original.state.shipDesigns.size());
    assert(loaded.state.shipDesigns.back().name == "Long Range Colonizer");
    assert(loaded.state.shipDesigns.back().components.size() == 4);
    assert(loaded.state.nextShipDesignId == 4);

    const auto& loadedScout = loaded.state.fleets.front();
    assert(loadedScout.destination.has_value());
    assert(same_position(*loadedScout.destination, {123.5, -77.25}));
    assert(loadedScout.warp == 9);
    assert(loadedScout.fuel == 217.75);
    assert(loadedScout.colonists == 1234);
    assert(loadedScout.arrivalAction.has_value());
    assert(loadedScout.arrivalAction->kind == FleetArrivalActionKind::Refuel);
    assert(loadedScout.waypointQueue.size() == 2);
    assert(loadedScout.waypointQueue[1].arrivalAction.kind == FleetArrivalActionKind::UnloadAllColonists);
    assert(loadedScout.minerals.germanium == 3.75);

    assert(loaded.pendingOrders.player == 1);
    assert(loaded.pendingOrders.orders.size() == 8);
    const auto* loadedMove = std::get_if<MoveFleetOrder>(&loaded.pendingOrders.orders[0]);
    assert(loadedMove);
    assert(loadedMove->warp == 8);
    assert(loadedMove->arrivalAction.kind == FleetArrivalActionKind::Colonize);
    assert(loadedMove->queuedWaypoints.size() == 1);
    assert(std::holds_alternative<QueueProductionOrder>(loaded.pendingOrders.orders[1]));
    assert(std::holds_alternative<CreateShipDesignOrder>(loaded.pendingOrders.orders[2]));
    assert(std::holds_alternative<QueueShipDesignOrder>(loaded.pendingOrders.orders[3]));
    assert(std::holds_alternative<SetFleetColonistsOrder>(loaded.pendingOrders.orders[4]));
    assert(std::holds_alternative<SetFleetMineralCargoOrder>(loaded.pendingOrders.orders[5]));
    assert(std::holds_alternative<RefuelFleetOrder>(loaded.pendingOrders.orders[6]));
    assert(std::holds_alternative<ColonizePlanetOrder>(loaded.pendingOrders.orders[7]));
    assert(loaded.pendingDescriptions == original.pendingDescriptions);
    assert(loaded.selectedStar == original.selectedStar);
    assert(loaded.selectedFleet == original.selectedFleet);
    assert(!loaded.showSensorRanges);
}

void unsupported_version_is_rejected_cleanly()
{
    QTemporaryDir directory;
    assert(directory.isValid());
    const auto path = directory.filePath("future.suns");

    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_4);
    stream << quint32{0x53554E53u} << quint32{999};
    file.close();

    SaveGameData loaded;
    QString error;
    assert(!read_save_game_file(path, loaded, error));
    assert(error.contains("Unsupported Suns! save version"));
}

} // namespace

int main()
{
    round_trip_preserves_campaign_and_planning_phase();
    unsupported_version_is_rejected_cleanly();
    std::cout << "save game tests passed\n";
    return 0;
}
