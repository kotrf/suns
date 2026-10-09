#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/hulls.hpp"
#include "suns/scanners.hpp"
#include "suns/mining.hpp"

#include <QByteArray>
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
    original.state.fleets.front().damagePercent = 23.5;
    original.state.stars[1].variability = {13, 12, 7};
    original.state.planets.front().mines = 17;
    original.state.planets.front().productionWaitingForMinerals = true;
    original.state.planets.front().productionWaitingForShipyard = true;
    original.state.planets.front().environment = {63, 47, 29};
    original.state.planets[1].habitability = -45;
    original.state.planets[1].observedHabitability = -35;
    original.state.planets[1].precursorArtifacts = {true, true, 1, 13};
    original.state.orbitalStations.front().name = "Sol Prime Orbital Dock";
    original.state.players.front().surveyKnowledge.push_back({2, SurveyLevel::DeepSurvey, 75, PlayerId{0}});
    original.state.players.front().pendingSurveyReports.push_back({
        2, 1, 76, 79, SurveyLevel::DeepSurvey, PlayerId{2},
    });
    original.state.players.front().pendingPlayerReports.push_back({
        PlayerReportKind::ColonyLost,
        76,
        79,
        2,
        2,
        1,
        kScoutDesignId,
        ProductionKind::ColonyShip,
        {420.0, 10.0},
        0,
        ResearchField::Biology,
        3,
    });
    original.state.players.front().pendingPlayerReports.back().deliveredMinerals = {2.5, 1.0, 0.25};
    original.state.players.front().pendingPlayerReports.back().deliveredColonists = 1234;
    auto enemyReport = original.state.players.front().pendingPlayerReports.back();
    enemyReport.kind = PlayerReportKind::EnemyFleetDetected;
    enemyReport.fleet = 42;
    enemyReport.contactPosition = {425.0, 15.0};
    enemyReport.contactOwner = 2;
    original.state.players.front().pendingPlayerReports.push_back(enemyReport);
    auto recoveryReport = enemyReport;
    recoveryReport.kind = PlayerReportKind::FleetMobilityRestored;
    recoveryReport.fleet = 1;
    original.state.players.front().pendingPlayerReports.push_back(recoveryReport);
    original.state.players.front().observedEnemyFleets.push_back({42, 2, {425.0, 15.0}, 1, 0});
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
    original.state.players.front().race.habitableTemperature = {18, 64};
    original.state.players.front().race.habitableGravity = {27, 73};
    original.state.players.front().race.habitableRadiation = {4, 52};
    record_empire_turn_statistics(original.state);
    original.state.players.front().history.back().extraction = {3.25, 2.5, 1.75};
    original.state.players.front().history.back().extractionRecorded = true;
    original.state.players.front().history.back().colonyHistory.front().extraction = {3.25, 2.5, 1.75};
    original.state.players.front().history.back().freightDelivered = {4.5, 3.25, 2.0};
    original.state.players.front().history.back().colonistsDelivered = 12000;
    original.state.players.front().history.back().freightRecorded = true;
    original.state.players.front().history.back().colonyHistory.front().freightDelivered = {4.5, 3.25, 2.0};
    original.state.players.front().history.back().colonyHistory.front().colonistsDelivered = 12000;
    original.state.players.front().history.back().remoteExtraction = {6.25, 5.5, 4.75};
    original.state.players.front().history.back().remoteExtractionRecorded = true;
    original.state.players.front().history.back().remoteMineHistory.push_back({2, {6.25, 5.5, 4.75}});
    original.state.players.front().history.back().fleetHistory.push_back({
        998, 3, 42.5, 17.25, {1.25, 2.5, 3.75}, 5500, 64.25});
    original.state.players.front().history.back().milestones.push_back({
        12345, 77, HistoryMilestoneKind::ResearchCompleted, 0, ResearchField::Electronics, 2});
    original.state.players.front().history.back().milestones.push_back({
        12346, 77, HistoryMilestoneKind::FleetStalledForFuel, 0, ResearchField::Electronics, 0, 1});
    original.state.players.front().history.back().milestones.push_back({
        12347, 75, HistoryMilestoneKind::EnemyFleetDetected, 0, ResearchField::Electronics, 0, 42});
    original.state.players.front().history.back().milestones.push_back({
        12348, 77, HistoryMilestoneKind::GroundInvasionLost, 2});
    original.state.shipDesigns.push_back({
        original.state.nextShipDesignId++, 1, "Relay", ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::RelayArray},
        {{100, ShipComponentType::FusionDrive}, {200, ShipComponentType::RelayArray}}});
    original.state.shipDesigns.push_back({
        original.state.nextShipDesignId++, 1, "Repair", ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::FieldRepairBay},
        {{100, ShipComponentType::FusionDrive}, {200, ShipComponentType::FieldRepairBay}}});
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
    scout.telemetry.damagePercent = 7.5;
    scout.telemetryInTransit.front().telemetry.damagePercent = 18.0;
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
        CreateShipDesignOrder{
            "Repair Scout", ShipHullType::Scout,
            {ShipComponentType::FusionDrive, ShipComponentType::FieldRepairBay},
            {{100, ShipComponentType::FusionDrive}, {200, ShipComponentType::FieldRepairBay}},
        },
    }};
    original.pendingDescriptions = {
        "move", "mine", "research plan", "research allocation", "stop remote mining", "transfer cargo",
        "merge fleets", "split fleet", "reorder production",
        "create placed design", "create repair design",
    };
    original.selectedStar = 2;
    original.selectedFleet = scout.id;
    original.showSensorRanges = false;
    GameEvent archivedMessage;
    archivedMessage.id = 0x12345678ULL;
    archivedMessage.turn = 76;
    archivedMessage.observedTurn = 75;
    archivedMessage.recipient = 1;
    archivedMessage.kind = GameEventKind::ProductionCompleted;
    archivedMessage.star = 1;
    archivedMessage.planet = 1;
    archivedMessage.shipDesign = kScoutDesignId;
    archivedMessage.productionKind = ProductionKind::ColonyShip;
    archivedMessage.position = {0.0, 0.0};
    original.strategicMessages.push_back(archivedMessage);
    auto deepSurveyMessage = archivedMessage;
    deepSurveyMessage.id += 1;
    deepSurveyMessage.kind = GameEventKind::SystemSurveyed;
    deepSurveyMessage.surveyLevel = SurveyLevel::DeepSurvey;
    deepSurveyMessage.precursorArtifactHint = true;
    original.strategicMessages.push_back(deepSurveyMessage);
    auto invasionMessage = archivedMessage;
    invasionMessage.id += 2;
    invasionMessage.kind = GameEventKind::GroundInvasionWon;
    invasionMessage.planet = 2;
    invasionMessage.quantity = 731;
    original.strategicMessages.push_back(invasionMessage);
    auto enemyMessage = archivedMessage;
    enemyMessage.id += 3;
    enemyMessage.kind = GameEventKind::EnemyFleetDetected;
    enemyMessage.fleet = 42;
    enemyMessage.contactOwner = 2;
    enemyMessage.position = {425.0, 15.0};
    original.strategicMessages.push_back(enemyMessage);
    auto recoveryMessage = archivedMessage;
    recoveryMessage.id += 4;
    recoveryMessage.kind = GameEventKind::FleetMobilityRestored;
    recoveryMessage.fleet = 1;
    original.strategicMessages.push_back(recoveryMessage);
    original.readStrategicMessageIds.push_back(archivedMessage.id);

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
    assert(loaded.state.fleets.front().damagePercent == 23.5);
    assert(loaded.state.stars[1].variability.periodTurns == 13);
    assert(loaded.state.stars[1].variability.amplitudePercent == 12);
    assert(loaded.state.stars[1].variability.phaseOffset == 7);
    assert(loaded.state.planets.front().mines == 17);
    assert(loaded.state.planets.front().productionWaitingForMinerals);
    assert(loaded.state.planets.front().productionWaitingForShipyard);
    assert(loaded.state.planets.front().environment.temperature == 63);
    assert(loaded.state.planets.front().environment.gravity == 47);
    assert(loaded.state.planets.front().environment.radiation == 29);
    assert(loaded.state.planets[1].habitability == -45);
    assert(loaded.state.planets[1].observedHabitability == -35);
    assert(loaded.state.planets[1].precursorArtifacts.present);
    assert(loaded.state.planets[1].precursorArtifacts.claimed);
    assert(loaded.state.planets[1].precursorArtifacts.discoveredBy == 1);
    assert(loaded.state.planets[1].precursorArtifacts.researchPoints == 13);
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
    assert(savedKnowledge->level == SurveyLevel::DeepSurvey);
    assert(savedKnowledge->observedTurn == 75);
    assert(savedKnowledge->observedOwner == PlayerId{0});
    assert(loaded.state.players.front().pendingSurveyReports.front().star == 2);
    assert(loaded.state.players.front().pendingSurveyReports.front().sourceFleet == 1);
    assert(loaded.state.players.front().pendingSurveyReports.front().observedTurn == 76);
    assert(loaded.state.players.front().pendingSurveyReports.front().deliveryTurn == 79);
    assert(loaded.state.players.front().pendingSurveyReports.front().level == SurveyLevel::DeepSurvey);
    assert(loaded.state.players.front().pendingSurveyReports.front().observedOwner == PlayerId{2});
    assert(loaded.state.players.front().pendingPlayerReports.size() == 3);
    const auto& report = loaded.state.players.front().pendingPlayerReports.front();
    assert(report.kind == PlayerReportKind::ColonyLost);
    assert(report.observedTurn == 76);
    assert(report.deliveryTurn == 79);
    assert(report.fleet == 1);
    assert(same_position(report.position, {420.0, 10.0}));
    assert(report.researchField == ResearchField::Biology);
    assert(report.technologyLevel == 3);
    assert(report.deliveredMinerals.ironium == 2.5);
    assert(report.deliveredColonists == 1234);
    assert(loaded.state.players.front().pendingPlayerReports[1].kind == PlayerReportKind::EnemyFleetDetected);
    assert(loaded.state.players.front().pendingPlayerReports[1].contactOwner == 2);
    assert(same_position(loaded.state.players.front().pendingPlayerReports[1].contactPosition,
        {425.0, 15.0}));
    assert(loaded.state.players.front().pendingPlayerReports.back().kind == PlayerReportKind::FleetMobilityRestored);
    assert(loaded.state.players.front().observedEnemyFleets.size() == 1);
    assert(loaded.state.players.front().observedEnemyFleets.front().fleet == 42);
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
    assert(loaded.state.players.front().race.habitableTemperature.minimum == 18);
    assert(loaded.state.players.front().race.habitableTemperature.maximum == 64);
    assert(loaded.state.players.front().race.habitableGravity.minimum == 27);
    assert(loaded.state.players.front().race.habitableGravity.maximum == 73);
    assert(loaded.state.players.front().race.habitableRadiation.minimum == 4);
    assert(loaded.state.players.front().race.habitableRadiation.maximum == 52);
    assert(loaded.state.players.front().history.size() == 2);
    assert(loaded.state.players.front().history.front().turn == 1);
    assert(loaded.state.players.front().history.back().turn == 77);
    assert(loaded.state.players.front().history.back().population == 1'000'000);
    assert(loaded.state.players.front().history.back().colonyHistory.size() == 1);
    assert(loaded.state.players.front().history.back().colonyHistory.front().planet
        == original.state.planets.front().id);
    assert(loaded.state.players.front().history.back().colonyHistory.front().mines == 17);
    assert(loaded.state.players.front().history.back().extraction.ironium == 3.25);
    assert(loaded.state.players.front().history.back().extractionRecorded);
    assert(loaded.state.players.front().history.back().colonyHistory.front().extraction.germanium == 1.75);
    assert(loaded.state.players.front().history.back().freightRecorded);
    assert(loaded.state.players.front().history.back().freightDelivered.ironium == 4.5);
    assert(loaded.state.players.front().history.back().colonistsDelivered == 12000);
    assert(loaded.state.players.front().history.back().colonyHistory.front().freightDelivered.boranium == 3.25);
    assert(loaded.state.players.front().history.back().remoteExtractionRecorded);
    assert(loaded.state.players.front().history.back().remoteExtraction.boranium == 5.5);
    assert(loaded.state.players.front().history.back().remoteMineHistory.size() == 1);
    assert(loaded.state.players.front().history.back().remoteMineHistory.front().planet == 2);
    assert(loaded.state.players.front().history.back().remoteMineHistory.front().extraction.germanium == 4.75);
    assert(loaded.state.players.front().history.back().fleetHistory.size()
        == original.state.players.front().history.back().fleetHistory.size());
    assert(loaded.state.players.front().history.back().fleetHistory.back().fleet == 998);
    assert(loaded.state.players.front().history.back().fleetHistory.back().ships == 3);
    assert(loaded.state.players.front().history.back().fleetHistory.back().grossMass == 42.5);
    assert(loaded.state.players.front().history.back().fleetHistory.back().fuel == 17.25);
    assert(loaded.state.players.front().history.back().fleetCargoRecorded);
    assert(loaded.state.players.front().history.back().fleetHistory.back().minerals.germanium == 3.75);
    assert(loaded.state.players.front().history.back().fleetHistory.back().colonists == 5500);
    assert(loaded.state.players.front().history.back().fleetDamageRecorded);
    assert(loaded.state.players.front().history.back().fleetHistory.back().damagePercent == 64.25);
    assert(loaded.state.players.front().history.back().milestones.size() == 4);
    assert(loaded.state.players.front().history.back().milestones.front().eventId == 12345);
    assert(loaded.state.players.front().history.back().milestones.front().technologyLevel == 2);
    assert(loaded.state.players.front().history.back().milestones[1].fleet == 1);
    assert(loaded.state.players.front().history.back().milestones[2].fleet == 42);
    assert(loaded.state.players.front().history.back().milestones[2].observedTurn == 75);
    assert(loaded.state.players.front().history.back().milestones.back().kind
        == HistoryMilestoneKind::GroundInvasionLost);
    assert(loaded.state.planets.front().productionQueue.empty());
    assert(loaded.state.shipDesigns.back().components.front() == ShipComponentType::AdvancedFusionDrive);
    assert(loaded.state.shipDesigns.back().components[1] == ShipComponentType::AdvancedFusionDrive);
    assert(loaded.state.shipDesigns.back().components[2] == ShipComponentType::ExtendedRangeScanner);
    assert(loaded.state.shipDesigns.back().components.back() == ShipComponentType::RemoteMiningModule);
    const auto relay = std::find_if(loaded.state.shipDesigns.begin(), loaded.state.shipDesigns.end(),
        [](const ShipDesign& design) { return design.name == "Relay"; });
    assert(relay != loaded.state.shipDesigns.end());
    assert(relay->placements.back().component == ShipComponentType::RelayArray);
    const auto repair = std::find_if(loaded.state.shipDesigns.begin(), loaded.state.shipDesigns.end(),
        [](const ShipDesign& design) { return design.name == "Repair"; });
    assert(repair != loaded.state.shipDesigns.end());
    assert(repair->placements.back().component == ShipComponentType::FieldRepairBay);
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
    assert(fleet.telemetry.damagePercent == 7.5);
    assert(fleet.telemetryInTransit.front().telemetry.damagePercent == 18.0);
    assert(fleet.telemetryInTransit.front().telemetry.ships.size() == 2);

    assert(loaded.pendingOrders.orders.size() == 11);
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
    const auto* repairOrder = std::get_if<CreateShipDesignOrder>(&loaded.pendingOrders.orders[10]);
    assert(repairOrder && repairOrder->name == "Repair Scout");
    assert(repairOrder->placements.back().component == ShipComponentType::FieldRepairBay);
    assert(loaded.pendingDescriptions == original.pendingDescriptions);
    assert(loaded.selectedStar == original.selectedStar);
    assert(loaded.selectedFleet == original.selectedFleet);
    assert(!loaded.showSensorRanges);
    assert(loaded.strategicMessages.size() == 5);
    assert(loaded.strategicMessages.front().id == archivedMessage.id);
    assert(loaded.strategicMessages.front().kind == GameEventKind::ProductionCompleted);
    assert(loaded.strategicMessages.front().planet == 1);
    assert(loaded.strategicMessages.front().shipDesign == kScoutDesignId);
    assert(loaded.strategicMessages[1].surveyLevel == SurveyLevel::DeepSurvey);
    assert(loaded.strategicMessages[1].precursorArtifactHint);
    assert(loaded.strategicMessages[2].kind == GameEventKind::GroundInvasionWon);
    assert(loaded.strategicMessages[3].kind == GameEventKind::EnemyFleetDetected);
    assert(loaded.strategicMessages[3].contactOwner == 2);
    assert(loaded.strategicMessages.back().kind == GameEventKind::FleetMobilityRestored);
    assert(loaded.strategicMessages[2].quantity == 731);
    assert(loaded.readStrategicMessageIds == original.readStrategicMessageIds);
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

void high_warp_component_round_trips()
{
    QTemporaryDir directory;
    assert(directory.isValid());
    QString error;
    SaveGameData original;
    original.galaxyConfig = {20260925, 24, 940.0, 700.0, 50.0};
    original.state = generate_game(original.galaxyConfig);
    original.campaignId = 5;
    original.turnToken = 7;
    original.pendingOrders = {1, {}};
    const auto id = original.state.nextShipDesignId++;
    original.state.shipDesigns.push_back({id, 1, "Warp Ten Scout",
        ShipHullType::Scout, {ShipComponentType::HighWarpDrive}});
    const auto savePath = directory.filePath("warp-ten.suns");
    assert(write_save_game_file(savePath, original, error));
    SaveGameData loaded;
    assert(read_save_game_file(savePath, loaded, error));
    assert(find_ship_design(loaded.state, id)->components.front() == ShipComponentType::HighWarpDrive);

    TurnOrderFileData packet{5, 1, 7,
        {1, {CreateShipDesignOrder{"Warp Ten Scout", ShipHullType::Scout,
            {ShipComponentType::HighWarpDrive}}}}, {"Design Warp Ten Scout"}};
    const auto ordersPath = directory.filePath("warp-ten.sunsorders");
    assert(write_turn_order_file(ordersPath, packet, error));
    TurnOrderFileData imported;
    assert(read_turn_order_file(ordersPath, imported, error));
    const auto& design = std::get<CreateShipDesignOrder>(imported.orders.orders.front());
    assert(design.components.front() == ShipComponentType::HighWarpDrive);
}

void heavy_transport_round_trips()
{
    QTemporaryDir directory;
    assert(directory.isValid());
    QString error;
    SaveGameData original;
    original.galaxyConfig = {20260925, 24, 940.0, 700.0, 50.0};
    original.state = generate_game(original.galaxyConfig);
    original.campaignId = 15;
    original.turnToken = 9;
    original.pendingOrders = {1, {}};
    original.state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 2;
    const auto id = original.state.nextShipDesignId++;
    original.state.shipDesigns.push_back({id, 1, "Bulk Hauler", ShipHullType::HeavyTransport,
        {ShipComponentType::FusionDrive, ShipComponentType::FusionDrive, ShipComponentType::FusionDrive}});
    const auto savePath = directory.filePath("heavy-transport.suns");
    assert(write_save_game_file(savePath, original, error));
    SaveGameData loaded;
    assert(read_save_game_file(savePath, loaded, error));
    const auto* savedDesign = find_ship_design(loaded.state, id);
    assert(savedDesign && savedDesign->hull == ShipHullType::HeavyTransport);
    assert(ship_design_valid(*savedDesign));
    assert(ship_design_available_to_player(loaded.state, 1, *savedDesign));

    TurnOrderFileData packet{15, loaded.state.turn, 9,
        {1, {CreateShipDesignOrder{"New Bulk Hauler", ShipHullType::HeavyTransport,
            {ShipComponentType::FusionDrive, ShipComponentType::FusionDrive,
             ShipComponentType::FusionDrive}}}}, {"Design new bulk hauler"}};
    const auto ordersPath = directory.filePath("heavy-transport.sunsorders");
    assert(write_turn_order_file(ordersPath, packet, error));
    TurnOrderFileData imported;
    assert(read_turn_order_file(ordersPath, imported, error));
    const auto& design = std::get<CreateShipDesignOrder>(imported.orders.orders.front());
    assert(design.hull == ShipHullType::HeavyTransport);
    assert(design.components.size() == 3);

    // Older packet versions reject the new hull ordinal rather than misreading it.
    {
        QFile file(ordersPath);
        assert(file.open(QIODevice::ReadWrite) && file.seek(4));
        QDataStream stream(&file);
        stream << quint32{7};
    }
    assert(!read_turn_order_file(ordersPath, imported, error));
}

void population_migration_and_clear_orders()
{
    QTemporaryDir directory;
    QString error;
    SaveGameData legacy;
    legacy.campaignId = 12;
    legacy.turnToken = 34;
    legacy.state = make_demo_game();
    // Build a v31-shaped fixture from the current writer. Keep newer survey
    // payloads empty and strip v37's colony-history count plus v38's
    // extraction/freight totals before downgrading
    // the header; the historical empire snapshot itself must still migrate.
    legacy.state.players[0].surveyKnowledge.clear();
    legacy.state.players[0].pendingSurveyReports.clear();
    legacy.state.planets[0].population = 1000;
    legacy.state.players[0].history[0].population = 1000;
    legacy.state.players[0].history[0].colonyHistory.clear();
    legacy.state.players[0].history[0].fleetHistory.clear();
    legacy.state.players[0].history[0].technologyProgress.back() = 0xF00DF00D;
    legacy.state.fleets[0].design = kColonyShipDesignId;
    legacy.state.fleets[0].colonists = 300;
    legacy.state.fleets[0].telemetry.colonists = 200;
    legacy.state.fleets[0].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::LoadAllAvailable, 100};
    legacy.pendingOrders = {1, {SetFleetColonistsOrder{1, 1, 400}}};
    legacy.pendingDescriptions = {"Load legacy cargo"};
    const auto path = directory.filePath("v31.suns");
    assert(write_save_game_file(path, legacy, error));
    // v31 and v32 layouts are identical for this no-MoveFleetOrder fixture.
    {
        QFile file(path);
        assert(file.open(QIODevice::ReadWrite));
        auto bytes = file.readAll();
        const auto marker = QByteArray::fromHex("f00df00d00000000");
        const auto markerPosition = bytes.indexOf(marker);
        assert(markerPosition >= 0 && bytes.indexOf(marker, markerPosition + 1) < 0);
        bytes.remove(markerPosition + 4, 4 + 3 * 8 + 1 + 4 + 3 * 8 + 8 + 1 + 3 * 8 + 1 + 4 + 4 + 1 + 1);
        // v46 adds the host's observed-contact count after environmentBased.
        bytes.remove(markerPosition + 5, 4);
        // v54 adds a 4-byte mouth ID to this single arrival action and a
        // 112-byte empty WH section (one player) after GameState.
        const auto actionMarker = QByteArray::fromHex("0100000000000000640000000000");
        const auto actionPosition = bytes.indexOf(actionMarker);
        assert(actionPosition >= 0 && bytes.indexOf(actionMarker, actionPosition + 1) < 0);
        bytes.remove(actionPosition + 10, 4);
        QByteArray whMarker;
        QDataStream whStream(&whMarker, QIODevice::WriteOnly);
        whStream << quint32{1} << legacy.state.wormholeRules.width << legacy.state.wormholeRules.height;
        const auto whPosition = bytes.indexOf(whMarker);
        assert(whPosition >= 0 && bytes.indexOf(whMarker, whPosition + 1) < 0);
        bytes.remove(whPosition, 112 + 14); // Empty WH plus v55 propulsion and v56 hull access for one player.
        // v37 colony records through v47 fleet count, v49 cargo and v50 damage flags.
        assert(file.resize(0) && file.seek(0));
        assert(file.write(bytes) == bytes.size() && file.seek(4));
        QDataStream stream(&file);
        stream << quint32{31};
    }
    SaveGameData migrated;
    assert(read_save_game_file(path, migrated, error));
    assert(migrated.migratedPopulation);
    assert(migrated.state.planets[0].population == 1'000'000);
    assert(migrated.state.players[0].history[0].population == 1'000'000);
    assert(migrated.state.players[0].history[0].colonyHistory.size() == 1);
    assert(migrated.state.players[0].history[0].colonyHistory[0].population == 1'000'000);
    assert(migrated.state.players[0].history[0].extraction.ironium == 0.0);
    assert(!migrated.state.players[0].history[0].extractionRecorded);
    assert(migrated.state.players[0].history[0].milestones.empty());
    assert(!migrated.state.players[0].history[0].freightRecorded);
    assert(!migrated.state.players[0].history[0].remoteExtractionRecorded);
    assert(migrated.state.players[0].history[0].fleetHistory.empty());
    assert(migrated.state.fleets[0].colonists == 30'000);
    assert(colonist_cargo_mass(migrated.state.fleets[0].colonists) == 3.0);
    assert(migrated.state.fleets[0].telemetry.colonists == 20'000);
    assert(migrated.state.fleets[0].arrivalAction->reservePopulation == 100'000);
    assert(std::get<SetFleetColonistsOrder>(migrated.pendingOrders.orders[0]).colonists == 40'000);
    assert(write_save_game_file(path, migrated, error));
    assert(read_save_game_file(path, migrated, error));
    assert(!migrated.migratedPopulation && migrated.state.planets[0].population == 1'000'000);

    const auto ordersPath = directory.filePath("orders.sunsorders");
    TurnOrderFileData packet{12, 1, 34, {1, {SetFleetColonistsOrder{1, 1, 400}}}, {"Legacy load"}};
    assert(write_turn_order_file(ordersPath, packet, error));
    {
        QFile file(ordersPath);
        assert(file.open(QIODevice::ReadWrite) && file.seek(4));
        QDataStream stream(&file);
        stream << quint32{2};
    }
    assert(read_turn_order_file(ordersPath, packet, error));
    assert(std::get<SetFleetColonistsOrder>(packet.orders.orders[0]).colonists == 40'000);
    MoveFleetOrder stop{1, {999, 999}, 5};
    stop.clearRoute = true;
    packet.orders.orders = {stop};
    packet.descriptions = {"Stop"};
    assert(write_turn_order_file(ordersPath, packet, error));
    assert(read_turn_order_file(ordersPath, packet, error));
    assert(std::get<MoveFleetOrder>(packet.orders.orders[0]).clearRoute);
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

void cancellation_round_trips_in_save_and_turn_packet()
{
    QTemporaryDir directory;
    QString error;
    SaveGameData saved;
    saved.campaignId = 12;
    saved.turnToken = 34;
    saved.state = make_demo_game();
    saved.pendingOrders = {1, {CancelProductionOrder{1, 2}, QueueShipDesignOrder{1, 0, "New Surveyor"},
        RenameFleetOrder{1, "Trailblazer"}}};
    saved.pendingDescriptions = {"Cancel third build", "Queue new Surveyor", "Rename fleet"};
    const auto path = directory.filePath("cancel.suns");
    assert(write_save_game_file(path, saved, error));
    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    const auto savedCancel = std::get<CancelProductionOrder>(loaded.pendingOrders.orders.at(0));
    assert(savedCancel.colony == 1 && savedCancel.index == 2);
    assert(std::get<QueueShipDesignOrder>(loaded.pendingOrders.orders.at(1)).pendingDesignName == "New Surveyor");
    assert(std::get<RenameFleetOrder>(loaded.pendingOrders.orders.at(2)).name == "Trailblazer");
    TurnOrderFileData packet{12, saved.state.turn, 34, saved.pendingOrders, saved.pendingDescriptions};
    const auto ordersPath = directory.filePath("cancel.sunsorders");
    assert(write_turn_order_file(ordersPath, packet, error));
    TurnOrderFileData read;
    assert(read_turn_order_file(ordersPath, read, error));
    const auto packetCancel = std::get<CancelProductionOrder>(read.orders.orders.at(0));
    assert(packetCancel.colony == 1 && packetCancel.index == 2);
    assert(std::get<QueueShipDesignOrder>(read.orders.orders.at(1)).pendingDesignName == "New Surveyor");
    assert(std::get<RenameFleetOrder>(read.orders.orders.at(2)).name == "Trailblazer");
}

void wormholes_round_trip_and_keep_player_exports_private()
{
    SaveGameData host;
    host.mode = SessionMode::Host;
    host.campaignId = 123;
    host.turnToken = 456;
    host.playerTokens = {{1, 456}};
    host.pendingOrders = {1, {}};
    host.state = make_demo_game();
    host.state.nextWormholeEndpointId = 21;
    host.state.wormholes.push_back({{{{19, {25, 0}, WormholeSignature::Strong},
        {20, {-400, 200}, WormholeSignature::Weak}}}, 1, 16, 0.6});
    host.state.players.front().wormholeKnowledge.push_back({19, {20, 0}, 1, WormholeStability::Variable});
    host.state.players.front().pendingWormholeReports.push_back({WormholeReportKind::Observation, 2, 4, 1,
        {20, {-400, 200}, 2, WormholeStability::Variable, 19}});
    auto action = FleetArrivalAction{};
    action.kind = FleetArrivalActionKind::EnterWormhole;
    action.wormholeEndpoint = 19;
    host.pendingOrders.orders.push_back(MoveFleetOrder{1, {20, 0}, 8, action});
    host.pendingDescriptions = {"Enter WH 19"};
    host.state.fleets.front().arrivalAction = action;
    host.state.wormholeTransits.push_back({host.state.fleets.front(), 19, 1, 12, 17});
    host.state.wormholeRules.driftPerTurn = 4.5;
    GameEvent event;
    event.id = 55;
    event.turn = 1;
    event.recipient = 1;
    event.kind = GameEventKind::WormholeEntered;
    event.wormholeEndpoint = 19;
    host.strategicMessages.push_back(event);
    QTemporaryDir dir;
    QString error;
    const auto path = dir.filePath("wormholes.suns");
    assert(write_save_game_file(path, host, error));
    SaveGameData read;
    assert(read_save_game_file(path, read, error));
    assert(read.state.wormholes.size() == 1 && read.state.nextWormholeEndpointId == 21);
    assert(read.state.wormholes.front().endpoints[1].signature == WormholeSignature::Weak);
    assert(read.state.wormholes.front().collapseTurn == 16);
    assert(read.state.wormholeRules.driftPerTurn == 4.5);
    assert(read.state.players.front().wormholeKnowledge.front().lastPosition.x == 20);
    assert(read.state.players.front().pendingWormholeReports.front().knowledge.linkedEndpoint == 19);
    assert(read.state.wormholeTransits.front().presumedLostTurn == 17);
    assert(read.strategicMessages.front().wormholeEndpoint == 19);
    assert(std::get<MoveFleetOrder>(read.pendingOrders.orders.front()).arrivalAction.wormholeEndpoint == 19);
    TurnOrderFileData orders{123, 1, 456, host.pendingOrders, host.pendingDescriptions};
    const auto ordersPath = dir.filePath("wormholes.sunsorders");
    assert(write_turn_order_file(ordersPath, orders, error));
    TurnOrderFileData readOrders;
    assert(read_turn_order_file(ordersPath, readOrders, error));
    assert(std::get<MoveFleetOrder>(readOrders.orders.orders.front()).arrivalAction.kind == FleetArrivalActionKind::EnterWormhole);
    assert(std::get<MoveFleetOrder>(readOrders.orders.orders.front()).arrivalAction.wormholeEndpoint == 19);
    const auto packet = make_player_turn(host, 1);
    assert(packet.state.wormholes.empty() && packet.state.galaxySeed == 0);
    assert(packet.state.wormholeTransits.empty());
    assert(packet.state.players.front().pendingWormholeReports.empty());
    const auto packetPath = dir.filePath("a.sunsturn");
    assert(write_save_game_file(packetPath, packet, error));
    auto hidden = host;
    hidden.state.nextWormholeEndpointId = 700;
    hidden.state.wormholes.front().endpoints[0].position = {91, 88};
    hidden.state.wormholes.front().endpoints[1].position = {-999, 700};
    hidden.state.wormholes.front().stability = 0.01;
    hidden.state.wormholes.front().collapseTurn = 100;
    hidden.state.wormholeRules.minimumLossChance = 0.2;
    hidden.state.wormholeTransits.front().presumedLostTurn = 999;
    const auto hiddenPath = dir.filePath("b.sunsturn");
    assert(write_save_game_file(hiddenPath, make_player_turn(hidden, 1), error));
    QFile a(packetPath), b(hiddenPath);
    assert(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly));
    assert(a.readAll() == b.readAll());
    assert(read_save_game_file(packetPath, read, error));
    assert(read.state.wormholes.empty() && read.state.players.front().wormholeKnowledge.size() == 1);
    // Loss before an entry report preserves the exact same owner packet as a live, remote fleet.
    host.state.fleets.front().arrivalAction.reset();
    host.state.wormholeTransits.front().lastContact = host.state.fleets.front();
    const auto alivePath = dir.filePath("alive.sunsturn");
    assert(write_save_game_file(alivePath, make_player_turn(host, 1), error));
    host.state.fleets.clear();
    const auto lostPath = dir.filePath("lost.sunsturn");
    assert(write_save_game_file(lostPath, make_player_turn(host, 1), error));
    QFile alive(alivePath), lost(lostPath);
    assert(alive.open(QIODevice::ReadOnly) && lost.open(QIODevice::ReadOnly));
    assert(alive.readAll() == lost.readAll());
}

void pre_wormhole_formats_remain_readable()
{
    QTemporaryDir dir;
    SaveGameData old;
    old.campaignId = 21;
    old.turnToken = 34;
    old.state = make_demo_game();
    old.pendingOrders = {1, {}};
    QString error;
    const auto path = dir.filePath("v53.suns");
    assert(write_save_game_file(path, old, error));
    {
        QFile file(path);
        assert(file.open(QIODevice::ReadWrite));
        auto bytes = file.readAll();
        QByteArray marker;
        QDataStream shape(&marker, QIODevice::WriteOnly);
        shape << quint32{1} << old.state.wormholeRules.width << old.state.wormholeRules.height;
        const auto offset = bytes.indexOf(marker);
        assert(offset >= 0 && bytes.indexOf(marker, offset + 1) < 0);
        bytes.remove(offset, 112 + 14); // Empty WH and propulsion/hull extensions with one player.
        assert(file.resize(0) && file.seek(0) && file.write(bytes) == bytes.size() && file.seek(4));
        QDataStream header(&file);
        header << quint32{53};
    }
    SaveGameData read;
    assert(read_save_game_file(path, read, error));
    assert(read.state.wormholes.empty() && read.state.wormholeTransits.empty());
    assert(read.state.fleets.size() == old.state.fleets.size());
    FleetArrivalAction action;
    action.reservePopulation = 1094; // Distinct marker for the single old action.
    TurnOrderFileData orders{21, 1, 34, {1, {MoveFleetOrder{1, {20, 30}, 7, action}}}, {"Ordinary flight"}};
    const auto orderPath = dir.filePath("v11.sunsorders");
    assert(write_turn_order_file(orderPath, orders, error));
    {
        QFile file(orderPath);
        assert(file.open(QIODevice::ReadWrite));
        auto bytes = file.readAll();
        QByteArray marker;
        QDataStream shape(&marker, QIODevice::WriteOnly);
        shape << quint8{0} << quint64{1094} << quint8{0} << quint32{0};
        const auto offset = bytes.indexOf(marker);
        assert(offset >= 0 && bytes.indexOf(marker, offset + 1) < 0);
        bytes.remove(offset + 10, 4);
        assert(file.resize(0) && file.seek(0) && file.write(bytes) == bytes.size() && file.seek(4));
        QDataStream header(&file);
        header << quint32{11};
    }
    TurnOrderFileData readOrders;
    assert(read_turn_order_file(orderPath, readOrders, error));
    assert(std::get<MoveFleetOrder>(readOrders.orders.orders.front()).arrivalAction.wormholeEndpoint == 0);
    assert(std::get<MoveFleetOrder>(readOrders.orders.orders.front()).destination.x == 20);
}

void moving_transit_outcomes_have_identical_undelivered_packets()
{
    SaveGameData initial;
    initial.mode = SessionMode::Host;
    initial.campaignId = 21;
    initial.turnToken = 34;
    initial.playerTokens = {{1, 34}};
    initial.pendingOrders = {1, {}};
    initial.state = make_demo_game();
    auto& state = initial.state;
    state.turn = 3;
    state.wormholeRules.spawnChancePerTurn = 0;
    state.wormholeRules.driftPerTurn = 0;
    state.wormholeRules.relocationChance = 0;
    state.wormholeRules.instabilityLossChance = 0;
    state.nextWormholeEndpointId = 21;
    state.wormholes.push_back({{{{19, {300, 0}, WormholeSignature::Strong},
        {20, {-400, 200}, WormholeSignature::Weak}}}, 1, 50, 0.7});
    state.players.front().wormholeKnowledge.push_back({19, {300, 0}, 1, WormholeStability::Variable});
    auto& moving = state.fleets.front();
    moving.position = {236, 0};
    moving.telemetry.position = moving.position;
    moving.destination = Position{300, 0};
    moving.telemetry.destination = moving.destination;
    moving.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::EnterWormhole, 1, FleetCargoKind::Colonists, 19};
    moving.telemetry.arrivalAction = moving.arrivalAction;
    auto inFlight = moving.telemetry;
    inFlight.observedTurn = 2;
    inFlight.position = {250, 0};
    inFlight.fuel = 123;
    moving.telemetryInTransit.push_back({5, inFlight});
    auto other = moving;
    other.id = 2;
    other.name = "Second scout";
    other.position = other.telemetry.position = {0, 0};
    other.destination.reset(); other.telemetry.destination.reset();
    other.arrivalAction.reset(); other.telemetry.arrivalAction.reset();
    other.telemetryInTransit.clear();
    state.fleets.push_back(other);
    state.nextFleetId = 3;
    auto lost = initial, alive = initial;
    lost.state.wormholeRules.minimumLossChance = 1;
    alive.state.wormholeRules.minimumLossChance = 0;
    lost.state = TurnProcessor{}.process(lost.state, {});
    alive.state = TurnProcessor{}.process(alive.state, {});
    assert(lost.state.fleets.size() == 1 && alive.state.fleets.size() == 2);
    assert(lost.state.players.front().history.back().fleets == 2);
    const auto lostPacket = make_player_turn(lost, 1), alivePacket = make_player_turn(alive, 1);
    assert(lostPacket.state.fleets[0].id == 1 && alivePacket.state.fleets[0].id == 1);
    assert(lostPacket.state.fleets[0].position.x == 300 && alivePacket.state.fleets[0].position.x == 300);
    assert(lostPacket.state.wormholeTransits.empty() && alivePacket.state.wormholeTransits.empty());
    QTemporaryDir dir;
    QString error;
    const auto aPath = dir.filePath("moving-alive.sunsturn"), bPath = dir.filePath("moving-lost.sunsturn");
    assert(write_save_game_file(aPath, alivePacket, error));
    assert(write_save_game_file(bPath, lostPacket, error));
    QFile a(aPath), b(bPath);
    assert(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly));
    assert(a.readAll() == b.readAll());
    // Undelivered cargo changes after emergence cannot affect player statistics.
    alive.state.fleets.front().colonists = 9000;
    alive.state.fleets.front().minerals = {10, 20, 30};
    const auto known = empire_turn_statistics(alive.state, 1);
    const auto missing = empire_turn_statistics(lost.state, 1);
    assert(known.population == missing.population && known.fleetMass == missing.fleetMass);
    assert(known.minerals.ironium == missing.minerals.ironium);
    for (int year = 5; year <= 6; ++year) {
        alive.state = TurnProcessor{}.process(alive.state, {});
        lost.state = TurnProcessor{}.process(lost.state, {});
        assert(alive.state.turn == static_cast<std::uint64_t>(year));
        const auto live = make_player_turn(alive, 1), gone = make_player_turn(lost, 1);
        assert(live.state.fleets.front().telemetry.observedTurn == (year == 5 ? 2 : 4));
        assert(gone.state.fleets.front().telemetry.observedTurn == live.state.fleets.front().telemetry.observedTurn);
        assert(live.state.fleets.front().fuel == 123 && gone.state.fleets.front().fuel == 123);
        a.close(); b.close();
        assert(write_save_game_file(aPath, live, error) && write_save_game_file(bPath, gone, error));
        assert(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly));
        assert(a.readAll() == b.readAll());
        for (const auto& transit : gone.state.wormholeTransits) assert(transit.lastContact.telemetryInTransit.empty());
    }
}

void stars_propulsion_and_access_round_trip()
{
    using namespace suns;
    QTemporaryDir dir;
    QString error;
    SaveGameData save;
    save.campaignId = 21;
    save.turnToken = 34;
    save.mode = SessionMode::Host;
    save.playerTokens = {{1, 34}};
    save.state = generate_campaign(GalaxyConfig{}, {{"Engines", RacePreset::Terran, true, false, true}});
    save.pendingOrders = {1, {}};
    for (const auto& engine : propulsion_technologies()) {
        const auto hull = engine.component == ShipComponentType::SettlersDelight
            ? ShipHullType::MiniColonyShip : ShipHullType::Scout;
        save.state.shipDesigns.push_back({save.state.nextShipDesignId++, 1, engine.name, hull, {engine.component}});
        save.pendingOrders.orders.emplace_back(CreateShipDesignOrder{engine.name, hull, {engine.component}});
        save.pendingDescriptions << engine.name;
    }
    const auto path = dir.filePath("engines.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.players.front().race.improvedFuelEfficiency);
    assert(!loaded.state.players.front().race.noRamScoopEngines);
    assert(loaded.state.players.front().race.settlerEngineAccess);
    assert(loaded.state.shipDesigns.size() == save.state.shipDesigns.size());
    for (std::size_t i = 0; i < save.state.shipDesigns.size(); ++i)
        assert(loaded.state.shipDesigns[i].components == save.state.shipDesigns[i].components);
    const auto turn = make_player_turn(save, 1);
    assert(write_save_game_file(path, turn, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.players.front().race.settlerEngineAccess);
    TurnOrderFileData packet{21, save.state.turn, 34, save.pendingOrders, save.pendingDescriptions};
    const auto orders = dir.filePath("engines.sunsorders");
    assert(write_turn_order_file(orders, packet, error));
    TurnOrderFileData read;
    assert(read_turn_order_file(orders, read, error));
    assert(read.orders.orders.size() == 15);
    for (std::size_t i = 0; i < read.orders.orders.size(); ++i)
        assert(std::get<CreateShipDesignOrder>(read.orders.orders[i]).components
            == std::get<CreateShipDesignOrder>(packet.orders.orders[i]).components);
    // A pre-catalog client must reject new IDs rather than reinterpret them.
    QFile file(orders);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream header(&file);
    header << quint32{12};
    file.close();
    assert(!read_turn_order_file(orders, read, error));

    save.state = make_demo_game();
    save.pendingOrders = {1, {}};
    save.pendingDescriptions.clear();
    assert(write_save_game_file(path, save, error));
    file.setFileName(path);
    assert(file.open(QIODevice::ReadWrite));
    auto bytes = file.readAll();
    QByteArray marker;
    QDataStream shape(&marker, QIODevice::WriteOnly);
    shape << quint32{1} << save.state.wormholeRules.width << save.state.wormholeRules.height;
    const auto offset = bytes.indexOf(marker);
    assert(offset >= 0 && bytes.indexOf(marker, offset + 1) < 0);
    bytes.remove(offset + 112, 14); // v54 has WH state but no propulsion/hull access extension.
    assert(file.resize(0) && file.seek(0) && file.write(bytes) == bytes.size() && file.seek(4));
    QDataStream oldHeader(&file);
    oldHeader << quint32{54};
    file.close();
    assert(read_save_game_file(path, loaded, error));
    assert(!loaded.state.players.front().race.improvedFuelEfficiency);
    assert(!loaded.state.players.front().race.noRamScoopEngines);
    assert(!loaded.state.players.front().race.settlerEngineAccess);
    assert(loaded.state.shipDesigns.front().components.front() == ShipComponentType::FusionDrive);
    assert(ship_design_max_warp(loaded.state.shipDesigns.front()) == 8);
}

void stars_hulls_round_trip_and_previous_format_migration()
{
    QTemporaryDir dir;
    QString error;
    SaveGameData save;
    save.campaignId = 21; save.turnToken = 34;
    save.mode = SessionMode::Host;
    save.state = generate_campaign({}, {
        {"War", RacePreset::Terran, false, false, false, HullAccess::WarMonger, true, false},
        {"Supply", RacePreset::Terran, false, false, false, HullAccess::InnerStrength, false, true}});
    save.playerTokens = {{1, 34}, {2, 35}};
    save.pendingOrders = {1, {}};
    for (const auto& hull : reference_hulls()) {
        ShipDesign design{save.state.nextShipDesignId++, 1, hull.name, hull.type,
            std::vector<ShipComponentType>(hull.requiredEngines, ShipComponentType::QuickJump5)};
        normalize_ship_design_placement(design);
        assert(ship_design_valid(design));
        save.state.shipDesigns.push_back(design);
        save.pendingOrders.orders.emplace_back(CreateShipDesignOrder{design.name, design.hull, design.components, design.placements});
        save.pendingDescriptions << QString::fromStdString(design.name);
    }
    const auto path = dir.filePath("hulls.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.players[0].race.hullAccess == HullAccess::WarMonger);
    assert(loaded.state.players[0].race.advancedRemoteMining && !loaded.state.players[0].race.basicRemoteMining);
    assert(loaded.state.players[1].race.hullAccess == HullAccess::InnerStrength);
    assert(!loaded.state.players[1].race.advancedRemoteMining && loaded.state.players[1].race.basicRemoteMining);
    assert(loaded.state.shipDesigns.size() == save.state.shipDesigns.size());
    for (std::size_t i = 0; i < save.state.shipDesigns.size(); ++i) {
        assert(loaded.state.shipDesigns[i].hull == save.state.shipDesigns[i].hull);
        assert(loaded.state.shipDesigns[i].placements == save.state.shipDesigns[i].placements);
    }
    assert(write_save_game_file(path, make_player_turn(save, 1), error));
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.players.size() == 1 && loaded.state.players[0].race.advancedRemoteMining);
    assert(loaded.state.players[0].race.hullAccess == HullAccess::WarMonger);
    assert(loaded.state.shipDesigns.back().hull == ShipHullType::MetaMorph);
    const auto orders = dir.filePath("hulls.sunsorders");
    TurnOrderFileData packet{21, save.state.turn, 34, save.pendingOrders, save.pendingDescriptions};
    assert(write_turn_order_file(orders, packet, error));
    TurnOrderFileData read;
    assert(read_turn_order_file(orders, read, error));
    assert(read.orders.orders.size() == reference_hulls().size());
    for (std::size_t i = 0; i < read.orders.orders.size(); ++i) {
        const auto& before = std::get<CreateShipDesignOrder>(packet.orders.orders[i]);
        const auto& after = std::get<CreateShipDesignOrder>(read.orders.orders[i]);
        assert(after.hull == before.hull && after.placements == before.placements);
    }
    QFile file(orders);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream oldOrderHeader(&file); oldOrderHeader << quint32{13}; file.close();
    assert(!read_turn_order_file(orders, read, error)); // Hull IDs require order v14.
    packet.orders.orders = {CreateShipDesignOrder{"Legacy", ShipHullType::Scout, {ShipComponentType::QuickJump5}}};
    packet.descriptions = {"Legacy"};
    assert(write_turn_order_file(orders, packet, error));
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream compatibleOrderHeader(&file); compatibleOrderHeader << quint32{13}; file.close();
    assert(read_turn_order_file(orders, read, error));
    assert(std::get<CreateShipDesignOrder>(read.orders.orders.front()).hull == ShipHullType::Scout);

    save.state = make_demo_game();
    save.state.players[0].race.improvedFuelEfficiency = true;
    save.pendingOrders = {1, {}}; save.pendingDescriptions.clear();
    save.playerTokens = {{1, 34}};
    assert(write_save_game_file(path, save, error));
    file.setFileName(path);
    assert(file.open(QIODevice::ReadOnly));
    const auto original = file.readAll(); file.close();
    QByteArray marker;
    QDataStream shape(&marker, QIODevice::WriteOnly);
    shape << quint32{1} << save.state.wormholeRules.width << save.state.wormholeRules.height;
    const auto offset = original.indexOf(marker);
    assert(offset >= 0 && original.indexOf(marker, offset + 1) < 0);
    const auto accessOffset = offset + 112 + 4 + 4 + 3;
    const auto writeBytes = [&](const QByteArray& bytes) {
        assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        assert(file.write(bytes) == bytes.size()); file.close();
    };
    auto malformed = original;
    malformed[accessOffset] = char(6);
    writeBytes(malformed);
    assert(!read_save_game_file(path, loaded, error));
    malformed = original;
    malformed[accessOffset + 1] = char(1); malformed[accessOffset + 2] = char(1);
    writeBytes(malformed);
    assert(!read_save_game_file(path, loaded, error));
    auto previous = original;
    previous.remove(accessOffset, 3); // v55 retains propulsion access, without hull access.
    QDataStream previousHeader(&previous, QIODevice::ReadWrite);
    assert(previousHeader.device()->seek(4)); previousHeader << quint32{55};
    writeBytes(previous);
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.players[0].race.improvedFuelEfficiency);
    assert(loaded.state.players[0].race.hullAccess == HullAccess::Standard);
    assert(!loaded.state.players[0].race.advancedRemoteMining && !loaded.state.players[0].race.basicRemoteMining);
    assert(loaded.state.shipDesigns[0].hull == ShipHullType::Scout);
    assert(ship_design_fuel_capacity(loaded.state.shipDesigns[0]) == ship_design_fuel_capacity(save.state.shipDesigns[0]));
}

void scanners_round_trip_and_version_boundaries()
{
    QTemporaryDir dir;
    QString error;
    SaveGameData save;
    save.campaignId = 21; save.turnToken = 34;
    save.mode = SessionMode::Host;
    save.state = generate_campaign({}, {{"Scanner", RacePreset::Terran, false, false, false, HullAccess::SuperStealth}});
    save.pendingOrders = {1, {}};
    save.playerTokens = {{1, 34}};
    for (const auto& scanner : scanner_technologies()) {
        ShipDesign design{save.state.nextShipDesignId++, 1, scanner.name, ShipHullType::StarsScout,
            {ShipComponentType::QuickJump5, scanner.component}};
        normalize_ship_design_placement(design);
        assert(ship_design_valid(design));
        save.state.shipDesigns.push_back(design);
        save.pendingOrders.orders.emplace_back(CreateShipDesignOrder{design.name, design.hull, design.components, design.placements});
        save.pendingDescriptions << QString::fromStdString(design.name);
    }
    const auto path = dir.filePath("scanners.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData loaded;
    if (!read_save_game_file(path, loaded, error)) {
        std::cerr << error.toStdString() << std::endl;
        assert(false);
    }
    for (std::size_t i = 0; i < save.state.shipDesigns.size(); ++i) {
        const auto& before = save.state.shipDesigns[i];
        const auto& after = loaded.state.shipDesigns[i];
        assert(after.components == before.components && after.placements == before.placements);
        assert(ship_design_ordinary_sensor_range(after) == ship_design_ordinary_sensor_range(before));
        assert(ship_design_penetrating_sensor_range(after) == ship_design_penetrating_sensor_range(before));
    }
    QFile file(path);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream header(&file); header << quint32{56}; file.close();
    assert(!read_save_game_file(path, loaded, error));
    const auto orders = dir.filePath("scanners.sunsorders");
    TurnOrderFileData packet{21, save.state.turn, 34, save.pendingOrders, save.pendingDescriptions};
    assert(write_turn_order_file(orders, packet, error));
    TurnOrderFileData read;
    assert(read_turn_order_file(orders, read, error));
    assert(read.orders.orders.size() == 16);
    for (std::size_t i = 0; i < read.orders.orders.size(); ++i)
        assert(std::get<CreateShipDesignOrder>(read.orders.orders[i]).components
            == std::get<CreateShipDesignOrder>(packet.orders.orders[i]).components);
    file.setFileName(orders);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream orderHeader(&file); orderHeader << quint32{14}; file.close();
    assert(!read_turn_order_file(orders, read, error));
    save.state = make_demo_game();
    save.pendingOrders = {1, {}}; save.pendingDescriptions.clear();
    assert(write_save_game_file(path, save, error));
    file.setFileName(path);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream compatible(&file); compatible << quint32{56}; file.close();
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.shipDesigns.front().components.back() == ShipComponentType::LongRangeScanner);
    assert(fleet_sensor_range(loaded.state, loaded.state.fleets.front()) == 50);
}

void mining_robots_round_trip_and_version_boundaries()
{
    QTemporaryDir dir;
    QString error;
    SaveGameData save;
    save.campaignId = 21; save.turnToken = 34;
    save.mode = SessionMode::Host;
    save.state = generate_campaign({}, {{"Miners", RacePreset::Terran, false, false, false,
        HullAccess::Standard, true}});
    save.pendingOrders = {1, {}};
    save.playerTokens = {{1, 34}};
    for (const auto& robot : mining_technologies()) {
        ShipDesign miner{save.state.nextShipDesignId++, 1, robot.name, ShipHullType::MidgetMiner,
            {ShipComponentType::QuickJump5, robot.component}};
        normalize_ship_design_placement(miner);
        assert(ship_design_valid(miner));
        save.state.shipDesigns.push_back(miner);
        save.pendingOrders.orders.emplace_back(CreateShipDesignOrder{miner.name, miner.hull, miner.components, miner.placements});
        save.pendingDescriptions << QString::fromStdString(miner.name);
    }
    const auto path = dir.filePath("robots.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    for (std::size_t i = 0; i < save.state.shipDesigns.size(); ++i) {
        const auto& before = save.state.shipDesigns[i];
        const auto& after = loaded.state.shipDesigns[i];
        assert(after.components == before.components && after.placements == before.placements);
        assert(ship_design_remote_mining_rate(after) == ship_design_remote_mining_rate(before));
    }
    assert(loaded.pendingOrders.orders.size() == 6);
    QFile file(path);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream header(&file); header << quint32{57}; file.close();
    assert(!read_save_game_file(path, loaded, error));
    const auto orders = dir.filePath("robots.sunsorders");
    TurnOrderFileData packet{21, save.state.turn, 34, save.pendingOrders, save.pendingDescriptions};
    assert(write_turn_order_file(orders, packet, error));
    TurnOrderFileData read;
    assert(read_turn_order_file(orders, read, error));
    for (std::size_t i = 0; i < read.orders.orders.size(); ++i) {
        const auto& before = std::get<CreateShipDesignOrder>(packet.orders.orders[i]);
        const auto& after = std::get<CreateShipDesignOrder>(read.orders.orders[i]);
        assert(after.components == before.components && after.placements == before.placements);
    }
    assert(read.orders.orders.size() == 6);
    file.setFileName(orders);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream orderHeader(&file); orderHeader << quint32{15}; file.close();
    assert(!read_turn_order_file(orders, read, error));

    // v57 has the same record layout but cannot contain appended robot IDs.
    save.state = make_demo_game();
    ShipDesign old{90, 1, "Old remote miner", ShipHullType::RemoteMiner,
        {ShipComponentType::FusionDrive, ShipComponentType::FusionDrive, ShipComponentType::RemoteMiningModule}};
    normalize_ship_design_placement(old);
    save.state.shipDesigns.push_back(old);
    save.pendingOrders = {1, {CreateShipDesignOrder{old.name, old.hull, old.components, old.placements}}};
    save.pendingDescriptions = {"Old remote miner"};
    assert(write_save_game_file(path, save, error));
    file.setFileName(path);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream compatible(&file); compatible << quint32{57}; file.close();
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.shipDesigns.back().components == old.components);
    assert(ship_design_remote_mining_rate(loaded.state.shipDesigns.back()) == 1.25);
    packet.orders = save.pendingOrders; packet.descriptions = save.pendingDescriptions;
    assert(write_turn_order_file(orders, packet, error));
    file.setFileName(orders);
    assert(file.open(QIODevice::ReadWrite) && file.seek(4));
    QDataStream oldOrderHeader(&file); oldOrderHeader << quint32{15}; file.close();
    assert(read_turn_order_file(orders, read, error));
    assert(std::get<CreateShipDesignOrder>(read.orders.orders.front()).components == old.components);
}

} // namespace

int main()
{
    mining_robots_round_trip_and_version_boundaries();
    scanners_round_trip_and_version_boundaries();
    stars_hulls_round_trip_and_previous_format_migration();
    stars_propulsion_and_access_round_trip();
    round_trip_preserves_communications_and_planning();
    turn_order_file_round_trip_preserves_envelope_and_orders();
    wormholes_round_trip_and_keep_player_exports_private();
    pre_wormhole_formats_remain_readable();
    moving_transit_outcomes_have_identical_undelivered_packets();
    high_warp_component_round_trips();
    heavy_transport_round_trips();
    old_format_is_rejected_cleanly();
    population_migration_and_clear_orders();
    cancellation_round_trips_in_save_and_turn_packet();
    std::cout << "save game tests passed\n";
    return 0;
}
