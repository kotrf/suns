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
    original.campaignId = 0x123456789abcdef0ULL;
    original.turnToken = 0x0fedcba987654321ULL;
    original.galaxyConfig = GalaxyConfig{20260825, 24, 940.0, 700.0, 50.0};
    original.state = generate_game(original.galaxyConfig);
    original.state.turn = 77;
    original.state.stars[1].variability = {13, 12, 7};
    original.state.planets.front().mines = 17;
    original.state.planets.front().productionWaitingForMinerals = true;
    original.state.planets.front().productionWaitingForShipyard = true;
    original.state.planets.front().environment = {63, 47, 29};
    original.state.orbitalStations.front().name = "Sol Prime Orbital Dock";
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
    original.state.players.front().technology.queuedFocuses = {
        ResearchField::Propulsion,
        ResearchField::Construction,
    };
    original.state.players.front().technology.researchAllocationPercent = 35;
    original.state.players.front().race.primaryTrait = PrimaryRaceTrait::HabitatCivilization;
    original.state.players.front().race.radiationTolerance = 0.91;
    original.state.players.front().race.radiationImmune = true;
    record_empire_turn_statistics(original.state);
    original.state.shipDesigns.push_back({
        original.state.nextShipDesignId++,
        1,
        "Remote Miner",
        ShipHullType::RemoteMiner,
        {ShipComponentType::AdvancedFusionDrive,
         ShipComponentType::AdvancedFusionDrive,
         ShipComponentType::ExtendedRangeScanner,
         ShipComponentType::RemoteMiningModule},
        {{100, ShipComponentType::AdvancedFusionDrive},
         {101, ShipComponentType::AdvancedFusionDrive},
         {200, ShipComponentType::ExtendedRangeScanner},
         {300, ShipComponentType::RemoteMiningModule}},
    });

    auto& scout = original.state.fleets.front();
    scout.position = {420.0, 10.0};
    scout.destination = Position{600.0, 20.0};
    scout.warp = 8;
    scout.fuel = 217.75;
    scout.colonists = 1234;
    scout.minerals = {1.25, 2.5, 3.75};
    scout.ships = {{kScoutDesignId, 2}, {3, 1}};
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
    scout.telemetry.repeatOrders = true;
    scout.telemetry.routeTemplate = {
        {{600.0, 20.0}, 8, {}},
        {{300.0, 10.0}, 8, {}},
    };
    scout.telemetry.ships = scout.ships;
    scout.telemetryInTransit.front().telemetry.ships = scout.ships;
    scout.fuelStalled = true;
    scout.task = FleetTask::RemoteMining;
    scout.repeatOrders = true;
    scout.routeTemplate = scout.telemetry.routeTemplate;
    scout.targetFleet = 2;
    scout.pendingCommands.front().program.targetFleet = 3;
    scout.pendingCommands.front().program.queuedWaypoints.front().targetFleet = 4;
    scout.telemetry.targetFleet = 5;
    scout.telemetry.routeTemplate.front().targetFleet = 6;
    scout.telemetryInTransit.front().telemetry.targetFleet = 7;
    scout.routeTemplate.front().targetFleet = 8;

    MoveFleetOrder move;
    move.fleet = scout.id;
    move.destination = {200.0, -100.0};
    move.warp = 8;
    move.arrivalAction.kind = FleetArrivalActionKind::LoadAllAvailable;
    move.arrivalAction.cargo = FleetCargoKind::Germanium;
    move.queuedWaypoints.push_back({{300.0, -100.0}, 7, {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Germanium}});
    move.repeatOrders = true;
    move.targetFleet = 9;
    move.queuedWaypoints.front().targetFleet = 10;
    original.pendingOrders = {1, {
        move,
        QueueProductionOrder{1, ProductionKind::Mine},
        SetResearchPlanOrder{
            ResearchField::Electronics,
            {ResearchField::Propulsion, ResearchField::Construction},
        },
        SetResearchAllocationOrder{35},
        SetRemoteMiningOrder{scout.id, false},
        TransferCargoOrder{{1, 0}, {0, scout.id}, 100, {1.0, 2.0, 3.0}},
        MergeFleetsOrder{scout.id, 2},
        SplitFleetOrder{scout.id, {{kScoutDesignId, 1}}},
        ReorderProductionQueueOrder{1, 2, 0},
        CreateShipDesignOrder{
            "Utility Tender",
            ShipHullType::Utility,
            {ShipComponentType::FusionDrive, ShipComponentType::FusionDrive,
             ShipComponentType::CargoPod},
            {{100, ShipComponentType::FusionDrive},
             {101, ShipComponentType::FusionDrive},
             {201, ShipComponentType::CargoPod}},
        },
    }};
    original.pendingDescriptions = {
        "move", "mine", "research plan", "research allocation", "stop remote mining", "transfer cargo",
        "merge fleets", "split fleet", "reorder production",
        "create placed design",
    };
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
    assert(loaded.campaignId == original.campaignId);
    assert(loaded.turnToken == original.turnToken);
    assert(loaded.state.turn == 77);
    assert(loaded.state.stars[1].variability.periodTurns == 13);
    assert(loaded.state.stars[1].variability.amplitudePercent == 12);
    assert(loaded.state.stars[1].variability.phaseOffset == 7);
    assert(loaded.state.planets.front().mines == 17);
    assert(loaded.state.planets.front().productionWaitingForMinerals);
    assert(loaded.state.planets.front().productionWaitingForShipyard);
    assert(loaded.state.planets.front().environment.temperature == 63);
    assert(loaded.state.planets.front().environment.gravity == 47);
    assert(loaded.state.planets.front().environment.radiation == 29);
    assert(loaded.state.nextOrbitalStationId == original.state.nextOrbitalStationId);
    assert(loaded.state.orbitalStations.size() == 1);
    assert(loaded.state.orbitalStations.front().id == 1);
    assert(loaded.state.orbitalStations.front().owner == 1);
    assert(loaded.state.orbitalStations.front().planet == 1);
    assert(loaded.state.orbitalStations.front().name == "Sol Prime Orbital Dock");
    assert(orbital_station_has_module(
        loaded.state.orbitalStations.front(), OrbitalStationModule::Shipyard));
    assert(orbital_station_has_module(
        loaded.state.orbitalStations.front(), OrbitalStationModule::RefuelingDepot));
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
    assert(technology.researchActive);
    assert(technology.queuedFocuses.size() == 2);
    assert(technology.queuedFocuses[0] == ResearchField::Propulsion);
    assert(technology.queuedFocuses[1] == ResearchField::Construction);
    assert(technology.researchAllocationPercent == 35);
    assert(loaded.state.players.front().race.primaryTrait == PrimaryRaceTrait::HabitatCivilization);
    assert(loaded.state.players.front().race.radiationTolerance == 0.91);
    assert(loaded.state.players.front().race.radiationImmune);
    assert(loaded.state.players.front().history.size() == 2);
    assert(loaded.state.players.front().history.front().turn == 1);
    assert(loaded.state.players.front().history.back().turn == 77);
    assert(loaded.state.players.front().history.back().population == 1000);
    assert(loaded.state.planets.front().productionQueue.empty());
    assert(loaded.state.shipDesigns.back().components.front() == ShipComponentType::AdvancedFusionDrive);
    assert(loaded.state.shipDesigns.back().components[1] == ShipComponentType::AdvancedFusionDrive);
    assert(loaded.state.shipDesigns.back().components[2] == ShipComponentType::ExtendedRangeScanner);
    assert(loaded.state.shipDesigns.back().components.back() == ShipComponentType::RemoteMiningModule);
    assert(loaded.state.shipDesigns.back().hull == ShipHullType::RemoteMiner);
    assert(loaded.state.shipDesigns.back().placements.size() == 4);
    assert(loaded.state.shipDesigns.back().placements[0].slot == 100);
    assert(loaded.state.shipDesigns.back().placements[1].slot == 101);
    assert(loaded.state.shipDesigns.back().placements[2].slot == 200);
    assert(loaded.state.shipDesigns.back().placements[3].slot == 300);

    const auto& fleet = loaded.state.fleets.front();
    assert(same_position(fleet.position, {420.0, 10.0}));
    assert(fleet.pendingCommands.size() == 2);
    assert(fleet.pendingCommands.front().issuedTurn == 77);
    assert(fleet.pendingCommands.front().deliveryTurn == 80);
    assert(fleet.pendingCommands.front().program.warp == 7);
    assert(fleet.pendingCommands.front().program.queuedWaypoints.size() == 1);
    assert(fleet.pendingCommands.front().program.clearRoute);
    assert(fleet.pendingCommands.front().program.targetFleet == 3);
    assert(fleet.pendingCommands.front().program.queuedWaypoints.front().targetFleet == 4);
    assert(!fleet.pendingCommands.front().task);
    assert(fleet.pendingCommands[1].task == FleetTask::None);
    assert(fleet.telemetry.observedTurn == 75);
    assert(same_position(fleet.telemetry.position, {300.0, 10.0}));
    assert(fleet.telemetry.destination.has_value());
    assert(fleet.telemetry.colonists == 900);
    assert(fleet.telemetry.minerals.germanium == 6.0);
    assert(fleet.telemetry.task == FleetTask::RemoteMining);
    assert(fleet.telemetry.repeatOrders);
    assert(fleet.telemetry.targetFleet == 5);
    assert(fleet.telemetry.routeTemplate.size() == 2);
    assert(fleet.telemetry.routeTemplate.front().targetFleet == 6);
    assert(fleet.telemetryInTransit.size() == 1);
    assert(fleet.telemetryInTransit.front().deliveryTurn == 79);
    assert(fleet.telemetryInTransit.front().telemetry.observedTurn == 76);
    assert(fleet.telemetryInTransit.front().telemetry.targetFleet == 7);
    assert(fleet.fuelStalled);
    assert(fleet.task == FleetTask::RemoteMining);
    assert(fleet.repeatOrders);
    assert(fleet.routeTemplate.size() == 2);
    assert(fleet.routeTemplate.front().targetFleet == 8);
    assert(fleet.targetFleet == 2);
    assert(fleet_ship_count(fleet) == 3);
    assert(fleet_ship_count(fleet, kScoutDesignId) == 2);
    assert(fleet_ship_count(fleet, 3) == 1);
    assert(fleet.telemetry.ships.size() == 2);
    assert(fleet.telemetryInTransit.front().telemetry.ships.size() == 2);

    assert(loaded.pendingOrders.orders.size() == 10);
    const auto* savedMove = std::get_if<MoveFleetOrder>(&loaded.pendingOrders.orders.front());
    assert(savedMove && savedMove->arrivalAction.kind == FleetArrivalActionKind::LoadAllAvailable);
    assert(savedMove->arrivalAction.cargo == FleetCargoKind::Germanium);
    assert(savedMove->repeatOrders);
    assert(savedMove->targetFleet == 9);
    assert(savedMove->queuedWaypoints.size() == 1);
    assert(savedMove->queuedWaypoints.front().targetFleet == 10);
    const auto* mine = std::get_if<QueueProductionOrder>(&loaded.pendingOrders.orders[1]);
    assert(mine && mine->kind == ProductionKind::Mine);
    const auto* plan = std::get_if<SetResearchPlanOrder>(&loaded.pendingOrders.orders[2]);
    assert(plan && plan->focus == ResearchField::Electronics);
    assert(plan->active);
    assert(plan->queuedFocuses.size() == 2);
    assert(plan->queuedFocuses[0] == ResearchField::Propulsion);
    assert(plan->queuedFocuses[1] == ResearchField::Construction);
    const auto* allocation = std::get_if<SetResearchAllocationOrder>(&loaded.pendingOrders.orders[3]);
    assert(allocation && allocation->percent == 35);
    const auto* stopMining = std::get_if<SetRemoteMiningOrder>(&loaded.pendingOrders.orders[4]);
    assert(stopMining && stopMining->fleet == scout.id && !stopMining->enabled);
    const auto* transfer = std::get_if<TransferCargoOrder>(&loaded.pendingOrders.orders[5]);
    assert(transfer);
    assert(transfer->source.planet == 1 && transfer->source.fleet == 0);
    assert(transfer->destination.planet == 0 && transfer->destination.fleet == scout.id);
    assert(transfer->colonists == 100);
    assert(transfer->minerals.ironium == 1.0);
    assert(transfer->minerals.boranium == 2.0);
    assert(transfer->minerals.germanium == 3.0);
    const auto* merge = std::get_if<MergeFleetsOrder>(&loaded.pendingOrders.orders[6]);
    assert(merge && merge->destination == scout.id && merge->source == 2);
    const auto* split = std::get_if<SplitFleetOrder>(&loaded.pendingOrders.orders[7]);
    assert(split && split->source == scout.id && split->ships.size() == 1);
    assert(split->ships.front().design == kScoutDesignId && split->ships.front().count == 1);
    const auto* reorder = std::get_if<ReorderProductionQueueOrder>(&loaded.pendingOrders.orders[8]);
    assert(reorder && reorder->colony == 1 && reorder->fromIndex == 2 && reorder->toIndex == 0);
    const auto* createDesign = std::get_if<CreateShipDesignOrder>(&loaded.pendingOrders.orders[9]);
    assert(createDesign && createDesign->name == "Utility Tender");
    assert(createDesign->hull == ShipHullType::Utility);
    assert(createDesign->placements.size() == 3);
    assert(createDesign->placements[0].slot == 100);
    assert(createDesign->placements[1].slot == 101);
    assert(createDesign->placements[2].slot == 201);
    assert(loaded.pendingDescriptions == original.pendingDescriptions);
    assert(loaded.selectedStar == original.selectedStar);
    assert(loaded.selectedFleet == original.selectedFleet);
    assert(!loaded.showSensorRanges);
}

void turn_order_file_round_trip_preserves_envelope_and_orders()
{
    MoveFleetOrder move;
    move.fleet = 4;
    move.destination = {120.0, -45.0};
    move.warp = 7;
    move.arrivalAction.kind = FleetArrivalActionKind::MergeWithFleet;
    move.targetFleet = 9;
    move.queuedWaypoints.push_back({{240.0, 30.0}, 6, {FleetArrivalActionKind::Refuel, 2}});

    TurnOrderFileData original;
    original.campaignId = 0xa11ce55badf00dULL;
    original.turn = 42;
    original.turnToken = 0x55aa55aa12344321ULL;
    original.orders = {2, {
        move,
        QueueProductionOrder{3, ProductionKind::OrbitalStation},
        SetResearchAllocationOrder{30},
    }};
    original.descriptions = {"intercept and merge", "build orbital dock", "research 30%"};

    QTemporaryDir directory;
    assert(directory.isValid());
    const auto path = directory.filePath("turn-42.sunsorders");
    QString error;
    assert(write_turn_order_file(path, original, error));

    TurnOrderFileData loaded;
    assert(read_turn_order_file(path, loaded, error));
    assert(error.isEmpty());
    assert(loaded.campaignId == original.campaignId);
    assert(loaded.turn == 42);
    assert(loaded.turnToken == original.turnToken);
    assert(loaded.orders.player == 2);
    assert(loaded.orders.orders.size() == 3);
    assert(loaded.descriptions == original.descriptions);

    const auto* loadedMove = std::get_if<MoveFleetOrder>(&loaded.orders.orders[0]);
    assert(loadedMove);
    assert(loadedMove->fleet == 4);
    assert(loadedMove->warp == 7);
    assert(loadedMove->targetFleet == 9);
    assert(loadedMove->arrivalAction.kind == FleetArrivalActionKind::MergeWithFleet);
    assert(loadedMove->queuedWaypoints.size() == 1);
    assert(loadedMove->queuedWaypoints.front().arrivalAction.kind == FleetArrivalActionKind::Refuel);

    const auto* production = std::get_if<QueueProductionOrder>(&loaded.orders.orders[1]);
    assert(production && production->colony == 3
        && production->kind == ProductionKind::OrbitalStation);
    const auto* allocation = std::get_if<SetResearchAllocationOrder>(&loaded.orders.orders[2]);
    assert(allocation && allocation->percent == 30);
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
    turn_order_file_round_trip_preserves_envelope_and_orders();
    old_format_is_rejected_cleanly();
    std::cout << "save game tests passed\n";
    return 0;
}
