#include "save_game.hpp"

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
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
    original.state.planets.front().productionWaitingForMinerals = true;
    original.state.players.front().surveyKnowledge.push_back({2, SurveyLevel::OrbitalSurvey, 75});
    original.state.players.front().pendingSurveyReports.push_back({
        2, 1, 76, 79, SurveyLevel::GeologicalSurvey,
    });
    original.state.players.front().pendingPlayerReports.push_back({
        PlayerReportKind::FleetStalledForFuel,
        76,
        79,
        2,
        2,
        1,
        kScoutDesignId,
        ProductionKind::ColonyShip,
        {420.0, 10.0},
        0,
    });
    original.state.players.front().technology.levels[3] = 1;
    original.state.players.front().technology.progress[3] = 7;
    original.state.players.front().technology.focus = ResearchField::Electronics;
    original.state.players.front().technology.nextFocus = ResearchField::Propulsion;
    original.state.planets.front().productionQueue.push_back({ProductionKind::Research, 0, 0});
    original.state.shipDesigns.push_back({
        original.state.nextShipDesignId++,
        1,
        "Compact Relay",
        ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::CompactLongRangeScanner,
         ShipComponentType::RemoteMiningModule},
    });

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
    scout.pendingCommands.push_back({78, 81, {}, FleetTask::None});
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
    scout.telemetry.task = FleetTask::RemoteMining;
    scout.fuelStalled = true;
    scout.task = FleetTask::RemoteMining;

    MoveFleetOrder move;
    move.fleet = scout.id;
    move.destination = {200.0, -100.0};
    move.warp = 8;
    original.pendingOrders = {1, {
        move,
        QueueProductionOrder{1, ProductionKind::Mine},
        SetResearchPlanOrder{ResearchField::Electronics, ResearchField::Propulsion},
        SetColonyResearchOrder{1, false},
        SetRemoteMiningOrder{scout.id, false},
    }};
    original.pendingDescriptions = {"move", "mine", "research plan", "stop research", "stop remote mining"};
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
    assert(loaded.state.planets.front().productionWaitingForMinerals);
    assert(loaded.state.players.front().pendingSurveyReports.size() == 1);
    assert(loaded.state.players.front().surveyKnowledge.size() >= 2);
    const auto savedKnowledge = std::find_if(
        loaded.state.players.front().surveyKnowledge.begin(),
        loaded.state.players.front().surveyKnowledge.end(),
        [](const SystemSurveyKnowledge& entry) { return entry.star == 2; });
    assert(savedKnowledge != loaded.state.players.front().surveyKnowledge.end());
    assert(savedKnowledge->level == SurveyLevel::OrbitalSurvey);
    assert(savedKnowledge->observedTurn == 75);
    assert(loaded.state.players.front().pendingSurveyReports.front().star == 2);
    assert(loaded.state.players.front().pendingSurveyReports.front().sourceFleet == 1);
    assert(loaded.state.players.front().pendingSurveyReports.front().observedTurn == 76);
    assert(loaded.state.players.front().pendingSurveyReports.front().deliveryTurn == 79);
    assert(loaded.state.players.front().pendingSurveyReports.front().level == SurveyLevel::GeologicalSurvey);
    assert(loaded.state.players.front().pendingPlayerReports.size() == 1);
    const auto& report = loaded.state.players.front().pendingPlayerReports.front();
    assert(report.kind == PlayerReportKind::FleetStalledForFuel);
    assert(report.observedTurn == 76);
    assert(report.deliveryTurn == 79);
    assert(report.fleet == 1);
    assert(same_position(report.position, {420.0, 10.0}));
    const auto& technology = loaded.state.players.front().technology;
    assert(technology.levels[3] == 1);
    assert(technology.progress[3] == 7);
    assert(technology.focus == ResearchField::Electronics);
    assert(technology.nextFocus == ResearchField::Propulsion);
    assert(loaded.state.planets.front().productionQueue.size() == 1);
    assert(loaded.state.planets.front().productionQueue.front().kind == ProductionKind::Research);
    assert(loaded.state.shipDesigns.back().components.back() == ShipComponentType::RemoteMiningModule);

    const auto& fleet = loaded.state.fleets.front();
    assert(same_position(fleet.position, {420.0, 10.0}));
    assert(fleet.pendingCommands.size() == 2);
    assert(fleet.pendingCommands.front().issuedTurn == 77);
    assert(fleet.pendingCommands.front().deliveryTurn == 80);
    assert(fleet.pendingCommands.front().program.warp == 7);
    assert(fleet.pendingCommands.front().program.queuedWaypoints.size() == 1);
    assert(fleet.pendingCommands.front().program.clearRoute);
    assert(!fleet.pendingCommands.front().task);
    assert(fleet.pendingCommands[1].task == FleetTask::None);
    assert(fleet.telemetry.observedTurn == 75);
    assert(same_position(fleet.telemetry.position, {300.0, 10.0}));
    assert(fleet.telemetry.destination.has_value());
    assert(fleet.telemetry.colonists == 900);
    assert(fleet.telemetry.minerals.germanium == 6.0);
    assert(fleet.telemetry.task == FleetTask::RemoteMining);
    assert(fleet.telemetryInTransit.size() == 1);
    assert(fleet.telemetryInTransit.front().deliveryTurn == 79);
    assert(fleet.telemetryInTransit.front().telemetry.observedTurn == 76);
    assert(fleet.fuelStalled);
    assert(fleet.task == FleetTask::RemoteMining);

    assert(loaded.pendingOrders.orders.size() == 5);
    const auto* mine = std::get_if<QueueProductionOrder>(&loaded.pendingOrders.orders[1]);
    assert(mine && mine->kind == ProductionKind::Mine);
    const auto* plan = std::get_if<SetResearchPlanOrder>(&loaded.pendingOrders.orders[2]);
    assert(plan && plan->focus == ResearchField::Electronics);
    assert(plan->nextFocus == ResearchField::Propulsion);
    const auto* stop = std::get_if<SetColonyResearchOrder>(&loaded.pendingOrders.orders[3]);
    assert(stop && stop->colony == 1 && !stop->enabled);
    const auto* stopMining = std::get_if<SetRemoteMiningOrder>(&loaded.pendingOrders.orders[4]);
    assert(stopMining && stopMining->fleet == scout.id && !stopMining->enabled);
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
    stream << quint32{0x53554E53u} << quint32{6};
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
