#include "suns/game_state.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/turn_processor.hpp"
#include "suns/player_knowledge.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace suns;

const GameEvent* find_event(const std::vector<GameEvent>& events, GameEventKind kind);

GameState survey_fixture(Position scoutPosition, Position targetPosition, std::uint64_t turn = 1)
{
    auto state = make_demo_game();
    state.turn = turn;
    state.stars.resize(2);
    state.planets.resize(2);
    state.fleets.resize(1);

    state.stars[0] = {1, "Home", {0.0, 0.0}, StarClass::Yellow};
    state.stars[1] = {2, "Target", targetPosition, StarClass::Orange};
    state.planets[0] = {1, 1, "Home I", 100, 1, 1000, 4, {}};
    state.planets[0].radar = PlanetaryRadarLevel::Full;
    state.planets[0].homeworld = true;
    state.planets[1] = {2, 2, "Target II", 82, 0, 0, 1, {}};
    state.players.front().surveyedStars = {1};
    state.players.front().surveyKnowledge = {
        {1, SurveyLevel::GeologicalSurvey, turn},
    };
    state.players.front().pendingSurveyReports.clear();

    auto& scout = state.fleets.front();
    scout.position = scoutPosition;
    scout.destination.reset();
    scout.pendingCommands.clear();
    scout.telemetryInTransit.clear();
    return state;
}

void ordinary_scanner_reports_system_contact_without_planet_data()
{
    const auto initial = survey_fixture({200.0, 0.0}, {250.0, 0.0});
    const TurnProcessor processor;

    const auto first = processor.process_with_events(initial, {});
    const auto replay = processor.process_with_events(initial, {});

    assert(first.state.turn == 2);
    assert(!is_surveyed(first.state, 1, 2));
    assert(survey_level(first.state, 1, 2) == SurveyLevel::SystemScan);
    assert(!known_planet_habitability(first.state, 1, 2).has_value());
    assert(!planet_geology_known(first.state, 1, 2));
    assert(first.state.players.front().pendingSurveyReports.empty());
    assert(first.events.size() == 1);
    assert(first.events.front().kind == GameEventKind::SystemSurveyed);
    assert(first.events.front().recipient == 1);
    assert(first.events.front().star == 2);
    assert(first.events.front().planet == 2);
    assert(first.events.front().fleet == 1);
    assert(first.events.front().observedTurn == 2);
    assert(first.events.front().turn == 2);
    assert(first.events.front().surveyLevel == SurveyLevel::SystemScan);
    assert(first.events.front().quantity == 0);
    assert(first.events.front().id == replay.events.front().id);
}

void penetrating_scanner_estimates_planet_during_a_connected_flyby()
{
    auto state = survey_fixture({0.0, 0.0}, {50.0, 65.0});
    state.shipDesigns.push_back({
        99,
        1,
        "Deep Surveyor",
        ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::PenetratingScanner},
    });
    state.fleets.front().design = 99;
    state.fleets.front().ships = {{99, 1}};
    state.fleets.front().destination = Position{200.0, 0.0};
    state.fleets.front().fuel = 300.0;
    const TurnProcessor processor;

    const auto result = processor.process_with_events(state, {});
    assert(survey_level(result.state, 1, 2) == SurveyLevel::BasicScan);
    assert(is_surveyed(result.state, 1, 2));
    assert(known_planet_habitability(result.state, 1, 2).has_value());
    assert(!planet_geology_known(result.state, 1, 2));
    assert(result.state.players.front().pendingSurveyReports.empty());
    assert(result.events.size() == 1);
    assert(result.events.front().surveyLevel == SurveyLevel::BasicScan);
    assert(result.events.front().quantity == *known_planet_habitability(result.state, 1, 2));
}

void remote_geology_uses_the_actual_flyby_and_delivers_coarse_evidence()
{
    const Position start{487.5, 0}, end{512.5, 0}, target{500, 79.5};
    auto initial = survey_fixture(start, target, 10);
    initial.players.front().technology.levels.fill(16);
    initial.planets[1].observedConcentration = MineralCargo{23, 88, 100};
    initial.shipDesigns.push_back({99, 1, "Geology Scout", ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5, ShipComponentType::GeologicalScanner}});
    auto& scout = initial.fleets.front();
    scout.design = 99;
    scout.ships = {{99, 1}};
    scout.destination = end;
    scout.warp = 5;
    scout.fuel = fleet_fuel_capacity(initial, scout);
    assert(distance_between(start, target) > 80 && distance_between(end, target) > 80);

    auto passive = initial;
    passive.fleets.front().electronics.mode = EmissionMode::Passive;
    passive = TurnProcessor{}.process(passive, {});
    assert(std::none_of(passive.players.front().pendingPlayerReports.begin(), passive.players.front().pendingPlayerReports.end(),
        [](const auto& report) { return report.kind == PlayerReportKind::RemoteGeologyObserved; }));

    auto observed = TurnProcessor{}.process(initial, {});
    assert(same_position(observed.fleets.front().position, end));
    const auto& reports = observed.players.front().pendingPlayerReports;
    const auto survey = std::find_if(reports.begin(), reports.end(),
        [](const auto& report) { return report.kind == PlayerReportKind::RemoteGeologyObserved; });
    assert(survey != reports.end() && survey->planet == 2 && survey->deliveryTurn > observed.turn);
    assert(!known_remote_geology(make_player_view(observed, 1).state, 1, 2));
    const auto packet = *survey;

    observed.planets[1].observedConcentration = MineralCargo{90, 10, 30};
    observed.turn = packet.deliveryTurn;
    const auto events = deliver_due_player_reports(observed);
    assert(find_event(events, GameEventKind::RemoteGeologyObserved));
    const auto view = make_player_view(observed, 1).state;
    const auto* estimate = known_remote_geology(view, 1, 2);
    assert(find_planet(view, 2) && !known_planet_owner(view, 1, 2));
    assert(estimate && estimate->source == scout.id && estimate->observedTurn == packet.observedTurn);
    assert(estimate->bandLower.ironium == 20 && estimate->bandLower.boranium == 80
        && estimate->bandLower.germanium == 90);
    assert(!planet_geology_known(view, 1, 2) && !known_surface_minerals(view, 1, 2));

    set_survey_level(observed, 1, 2, SurveyLevel::GeologicalSurvey, observed.turn);
    auto later = packet;
    later.observedTurn = later.deliveryTurn = observed.turn + 1;
    later.deliveredMinerals = {0, 0, 0};
    observed.players.front().pendingPlayerReports.push_back(later);
    ++observed.turn;
    assert(!find_event(deliver_due_player_reports(observed), GameEventKind::RemoteGeologyObserved));
    assert(planet_geology_known(observed, 1, 2));
    assert(known_remote_geology(observed, 1, 2)->observedTurn == packet.observedTurn);
}

void remote_geology_finds_clear_parts_of_an_interfered_flyby()
{
    const Position start{445, 0}, end{526, 0};
    auto initial = survey_fixture(start, {500, 76}, 10);
    initial.players.front().technology.levels.fill(16);
    initial.wormholeRules.spawnChancePerTurn = 0;
    initial.shipDesigns.push_back({99, 1, "Geology Scout", ShipHullType::StarsScout,
        {ShipComponentType::TransGalacticDrive, ShipComponentType::GeologicalScanner}});
    auto& scout = initial.fleets.front();
    scout.design = 99;
    scout.ships = {{99, 1}};
    scout.destination = end;
    scout.warp = 9;
    scout.fuel = fleet_fuel_capacity(initial, scout);
    initial.unknownObjects = {{1, {500, 0}, UnknownObjectKind::IonVortex}};

    const auto reported = [&](const GameState& state) {
        const auto result = TurnProcessor{}.process_with_events(state, {});
        assert(same_position(result.state.fleets.front().position, end));
        return std::any_of(result.state.players.front().pendingPlayerReports.begin(),
            result.state.players.front().pendingPlayerReports.end(), [](const auto& report) {
                return report.kind == PlayerReportKind::RemoteGeologyObserved && report.planet == 2;
            });
    };
    assert(reported(initial));
    auto narrow = initial;
    narrow.stars[1].position.y = std::sqrt(80.0 * 80.0 - 18.001 * 18.001);
    assert(reported(narrow)); // Only one thousandth of a light-year lies outside each edge of the vortex.
    auto blocked = initial;
    blocked.stars[1].position.y = 79;
    assert(!reported(blocked));
    blocked.unknownObjects.clear();
    assert(reported(blocked));

    auto cloud = initial;
    cloud.unknownObjects.clear();
    cloud.interstellarMediumEnabled = true;
    cloud.interstellarClouds = {{1, {500, 0}, 1, 5, 1}};
    assert(reported(cloud));
}

void flyby_uses_area_jamming_at_the_observation_point()
{
    const Position start{460, 88}, end{541, 88};
    auto state = survey_fixture(start, {500, 137}, 10);
    state.wormholeRules.spawnChancePerTurn = 0;
    state.players.push_back({2, "Opposition"});
    state.shipDesigns.push_back({99, 1, "Flyby scout", ShipHullType::StarsScout,
        {ShipComponentType::TransGalacticDrive, ShipComponentType::RhinoScanner}});
    state.shipDesigns.push_back({100, 2, "Jammer", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::AreaJammer}});
    auto& observer = state.fleets.front();
    observer.design = 99;
    observer.ships = {{99, 1}};
    observer.warp = 9;
    observer.destination = end;
    observer.fuel = fleet_fuel_capacity(state, observer);
    Fleet jammer;
    jammer.id = state.nextFleetId++;
    jammer.owner = 2;
    jammer.design = 100;
    jammer.position = {500, 0};
    state.fleets.push_back(jammer);
    const auto surveyed = [&](const GameState& input) {
        const auto next = TurnProcessor{}.process(input, {});
        assert(same_position(next.fleets.front().position, end));
        const auto& reports = next.players.front().pendingSurveyReports;
        return std::any_of(reports.begin(), reports.end(), [](const auto& report) {
            return report.star == 2 && report.level == SurveyLevel::SystemScan;
        });
    };
    assert(!surveyed(state)); // The only in-range part of the flight is jammed.
    auto clear = state;
    clear.fleets.back().electronics.mode = EmissionMode::Passive;
    assert(surveyed(clear));
    state.stars[1].position.y = 130;
    assert(surveyed(state)); // The scanner can reach the star from outside the field.
}

void flyby_dispatch_uses_the_jammers_completed_movement()
{
    const Position start{60, 0}, end{85, 0};
    const Position jammerClear{130, 90}, jammerNear{130, 65};
    auto state = survey_fixture(start, {60, 45}, 10);
    state.stars.front().position = {110, 0};
    state.planets.front().radar = PlanetaryRadarLevel::Local;
    state.wormholeRules.spawnChancePerTurn = 0;
    state.players.push_back({2, "Opposition"});
    state.shipDesigns.push_back({99, 1, "Scout", ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5, ShipComponentType::RhinoScanner}});
    state.shipDesigns.push_back({100, 2, "Jammer", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::AreaJammer}});
    auto& observer = state.fleets.front();
    observer.design = 99;
    observer.ships = {{99, 1}};
    observer.warp = 5;
    observer.fuel = fleet_fuel_capacity(state, observer);
    observer.destination = end;
    Fleet jammer;
    jammer.id = state.nextFleetId++;
    jammer.owner = 2;
    jammer.design = 100;
    jammer.warp = 5;
    jammer.fuel = fleet_fuel_capacity(state, jammer);
    state.fleets.push_back(jammer);

    const TurnProcessor processor;
    for (const bool swapIds : {false, true}) {
        for (const bool jamAtEnd : {false, true}) {
            auto input = state;
            auto& source = input.fleets.front();
            auto& hostile = input.fleets.back();
            if (swapIds) std::swap(source.id, hostile.id);
            hostile.position = jamAtEnd ? jammerClear : jammerNear;
            hostile.destination = jamAtEnd ? jammerNear : jammerClear;
            const auto after = processor.process_with_events(input, {});
            assert(after.state.fleets.front().position == end);
            assert(after.state.fleets.back().position == *hostile.destination);
            assert((survey_level(after.state, 1, 2) >= SurveyLevel::SystemScan) == !jamAtEnd);
            if (!jamAtEnd) continue;

            const auto& reports = after.state.players.front().pendingSurveyReports;
            assert(std::any_of(reports.begin(), reports.end(), [&](const auto& report) {
                return report.star == 2 && report.sourceFleet == source.id
                    && report.observedTurn == 11 && report.deliveryTurn == 12;
            }));
            const auto delivered = processor.process(after.state, {});
            assert(survey_level(delivered, 1, 2) >= SurveyLevel::SystemScan);
        }
    }
}

GameState rendezvous_survey_fixture()
{
    const Position scoutEnd{85, 0}, jammerEnd{165, 25};
    auto state = survey_fixture({60, 0}, {74, 49.8}, 10);
    state.stars.front().position = {64, 0};
    state.planets.front().radar = PlanetaryRadarLevel::Local;
    state.wormholeRules.spawnChancePerTurn = 0;
    state.players.push_back({2, "Opposition"});
    state.shipDesigns.push_back({99, 1, "Observer", ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5, ShipComponentType::RhinoScanner}});
    state.shipDesigns.push_back({100, 2, "Jammer", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::AreaJammer}});
    state.shipDesigns.push_back({101, 2, "Courier", ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5}});
    auto& observer = state.fleets.front();
    observer.design = 99;
    observer.ships = {{99, 1}};
    observer.warp = 5;
    observer.fuel = fleet_fuel_capacity(state, observer);
    observer.destination = scoutEnd;
    Fleet jammer;
    jammer.id = state.nextFleetId++;
    jammer.owner = 2;
    jammer.design = 100;
    jammer.position = {165, 50};
    jammer.destination = jammerEnd;
    jammer.warp = 5;
    jammer.fuel = fleet_fuel_capacity(state, jammer);
    state.fleets.push_back(jammer);
    return state;
}

void add_survey_pursuer(GameState& state, PlayerId owner, FleetId target, ShipDesignId design, Position position)
{
    Fleet pursuer;
    pursuer.id = state.nextFleetId++;
    pursuer.owner = owner;
    pursuer.design = design;
    pursuer.position = position;
    pursuer.destination = position;
    pursuer.targetFleet = target;
    pursuer.warp = 2;
    pursuer.fuel = fleet_fuel_capacity(state, pursuer);
    pursuer.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
    state.fleets.push_back(pursuer);
}

void flyby_dispatch_waits_for_movement_after_rendezvous()
{
    const auto state = rendezvous_survey_fixture();
    const auto observerId = state.fleets.front().id;
    const auto jammerId = state.fleets.back().id;
    const auto scoutEnd = *state.fleets.front().destination;
    const auto jammerEnd = *state.fleets.back().destination;
    const TurnProcessor processor;
    for (const bool mergeScout : {true, false}) {
        for (const bool enemyFirst : {false, true}) {
            auto input = state;
            if (enemyFirst) add_survey_pursuer(input, 2, jammerId, 101, {165, 48});
            if (mergeScout) add_survey_pursuer(input, 1, observerId, 99, {62, 0});
            if (!enemyFirst) add_survey_pursuer(input, 2, jammerId, 101, {165, 48});
            const auto result = processor.process(input, {});
            assert(result.fleets.size() == 2);
            assert(same_position(result.fleets.front().position, scoutEnd)
                && same_position(result.fleets.back().position, jammerEnd));
            assert(survey_level(result, 1, 2) < SurveyLevel::SystemScan);
            const auto& reports = result.players.front().pendingSurveyReports;
            assert(std::any_of(reports.begin(), reports.end(), [&](const auto& report) {
                return report.star == 2 && report.sourceFleet == observerId
                    && report.observedTurn == 11 && report.deliveryTurn == 12;
            }));
            assert(survey_level(processor.process(result, {}), 1, 2) >= SurveyLevel::SystemScan);
        }
    }

    // The new scanner can observe the continued leg, starting at the meeting.
    // Only the earlier leg crosses the coverage of this system.
    auto laterFit = state;
    laterFit.stars[1].position = {60, 49.98};
    laterFit.shipDesigns.push_back({102, 1, "Scannerless lead ship", ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5}});
    laterFit.fleets.front().design = 102;
    laterFit.fleets.front().ships = {{102, 1}};
    add_survey_pursuer(laterFit, 1, observerId, 99, {62, 0});
    const auto merged = processor.process(laterFit, {});
    assert(merged.fleets.size() == 2 && merged.fleets.front().ships.size() == 2);
    assert(survey_level(merged, 1, 2) < SurveyLevel::SystemScan);
    assert(std::none_of(merged.players.front().pendingSurveyReports.begin(), merged.players.front().pendingSurveyReports.end(),
        [](const auto& report) { return report.star == 2; }));
}

void continued_flyby_keeps_interference_from_before_arrival()
{
    auto state = rendezvous_survey_fixture();
    const auto scoutId = state.fleets.front().id;
    const auto jammerId = state.fleets[1].id;
    const auto scoutEnd = *state.fleets.front().destination;
    state.fleets[1].position = {140, -12.5};
    state.fleets[1].destination = Position{140, 12.5};
    add_survey_pursuer(state, 1, scoutId, 99, {62, 0});
    const auto scan = [&](const GameState& game) {
        const auto& reports = game.players.front().pendingSurveyReports;
        return std::find_if(reports.begin(), reports.end(), [&](const auto& report) {
            return report.star == 2 && report.sourceFleet == scoutId;
        });
    };
    const TurnProcessor processor;
    for (const bool mergeJammer : {false, true}) {
        auto input = state;
        if (mergeJammer) add_survey_pursuer(input, 2, jammerId, 101, {140, -10.5});
        const auto jammed = processor.process(input, {});
        assert(jammed.fleets.front().position == scoutEnd && jammed.fleets.front().ships.front().count == 2);
        assert(survey_level(jammed, 1, 2) < SurveyLevel::SystemScan);
        assert(scan(jammed) == jammed.players.front().pendingSurveyReports.end());

        auto scrap = input;
        scrap.fleets[1].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::ScrapFleet};
        const auto scrapped = processor.process(scrap, {});
        assert(std::none_of(scrapped.fleets.begin(), scrapped.fleets.end(),
            [&](const auto& fleet) { return fleet.id == jammerId; }));
        assert(communication_delay_turns(scrapped, 1, scoutEnd) == 0);
        assert(survey_level(scrapped, 1, 2) < SurveyLevel::SystemScan);
        assert(scan(scrapped) == scrapped.players.front().pendingSurveyReports.end());

        input.fleets[1].electronics.mode = EmissionMode::RadioSilence;
        input.fleets[1].electronics.resumeTurn = 20;
        assert(survey_level(processor.process(input, {}), 1, 2) == SurveyLevel::SystemScan);

        input.fleets[1].electronics.wakeOnArrival = true;
        const auto woke = processor.process(input, {});
        assert(communication_delay_turns(woke, 1, scoutEnd) == 1);
        assert(survey_level(woke, 1, 2) < SurveyLevel::SystemScan);
        const auto pending = scan(woke);
        assert(pending != woke.players.front().pendingSurveyReports.end()
            && pending->observedTurn == 11 && pending->deliveryTurn == 12);
        assert(survey_level(processor.process(woke, {}), 1, 2) == SurveyLevel::SystemScan);
    }

    state.fleets[1].destination = state.fleets[1].position;
    state.fleets[1].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::ScrapFleet};
    assert(survey_level(processor.process(state, {}), 1, 2) == SurveyLevel::SystemScan);
}

void continued_flyby_uses_outpost_power_from_before_fuel_arrivals()
{
    auto state = rendezvous_survey_fixture();
    const auto scoutId = state.fleets.front().id;
    const auto scoutEnd = *state.fleets.front().destination;
    const Position depotPosition{140, 0};
    state.shipDesigns.push_back({102, 2, "Jamming depot", ShipHullType::Scout,
        {ShipComponentType::QuickJump5, ShipComponentType::FuelDepotKit, ShipComponentType::AreaJammer}});
    auto& depot = state.fleets[1];
    const auto depotId = depot.id;
    depot.design = 102;
    depot.ships = {{102, 1}};
    depot.position = depotPosition;
    depot.destination.reset();
    depot.outpost = OutpostKind::FuelDepot;
    depot.fuel = 0;
    add_survey_pursuer(state, 1, scoutId, 99, {62, 0});

    Fleet tanker;
    tanker.id = state.nextFleetId++;
    tanker.owner = 2;
    tanker.design = 101;
    tanker.position = {140, -25};
    tanker.destination = depotPosition;
    tanker.warp = 5;
    tanker.fuel = 50;
    const auto tankerIndex = state.fleets.size();
    state.fleets.push_back(tanker);

    const auto depot_fuel = [&](const GameState& result) {
        const auto station = std::find_if(result.fleets.begin(), result.fleets.end(),
            [&](const Fleet& fleet) { return fleet.id == depotId; });
        assert(station != result.fleets.end());
        return station->fuel;
    };
    const auto report = [&](const GameState& result) {
        const auto& surveys = result.players.front().pendingSurveyReports;
        return std::find_if(surveys.begin(), surveys.end(), [&](const auto& reading) {
            return reading.star == 2 && reading.sourceFleet == scoutId;
        });
    };
    const TurnProcessor processor;
    const auto noTransfer = processor.process(state, {});
    assert(depot_fuel(noTransfer) == 0 && survey_level(noTransfer, 1, 2) == SurveyLevel::SystemScan);

    auto shipTarget = state;
    shipTarget.fleets[tankerIndex].targetFleet = depotId;
    shipTarget.fleets[tankerIndex].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::SupplyOutpost};
    auto coordinates = state;
    coordinates.fleets[tankerIndex].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::SupplyOutpost};
    const auto suppliedById = processor.process(shipTarget, {});
    const auto suppliedAtPoint = processor.process(coordinates, {});
    assert(depot_fuel(suppliedById) > 0 && depot_fuel(suppliedAtPoint) == depot_fuel(suppliedById));
    for (const auto* supplied : {&suppliedById, &suppliedAtPoint}) {
        assert(communication_delay_turns(*supplied, 1, scoutEnd) == 1);
        const auto pending = report(*supplied);
        assert(pending != supplied->players.front().pendingSurveyReports.end()
            && pending->observedTurn == 11 && pending->deliveryTurn == 12);
        assert(survey_level(processor.process(*supplied, {}), 1, 2) == SurveyLevel::SystemScan);
    }

    auto refuel = state;
    refuel.fleets[1].fuel = 12;
    refuel.fleets[tankerIndex].fuel = 20;
    refuel.fleets[tankerIndex].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Refuel};
    const auto emptied = processor.process(refuel, {});
    assert(depot_fuel(emptied) == 0 && communication_delay_turns(emptied, 1, scoutEnd) == 0);
    assert(survey_level(emptied, 1, 2) < SurveyLevel::SystemScan);
    assert(report(emptied) == emptied.players.front().pendingSurveyReports.end());

    shipTarget.fleets[tankerIndex].position = depotPosition;
    const auto suppliedBeforeFlight = processor.process(shipTarget, {});
    assert(depot_fuel(suppliedBeforeFlight) > 0);
    assert(survey_level(suppliedBeforeFlight, 1, 2) < SurveyLevel::SystemScan);
    assert(report(suppliedBeforeFlight) == suppliedBeforeFlight.players.front().pendingSurveyReports.end());
}

void flyby_keeps_pre_battle_sensor_readings()
{
    const Position scoutEnd{85, 0}, jammerPosition{140, 0};
    auto state = survey_fixture({60, 0}, {60, 45}, 10);
    state.stars.front().position = {90, 0};
    state.planets.front().radar = PlanetaryRadarLevel::Local;
    state.stars.push_back({3, "Close enough through jamming", {34.5, 0}});
    state.wormholeRules.spawnChancePerTurn = 0;
    state.players.push_back({2, "Opposition"});
    state.shipDesigns.push_back({99, 1, "Observer", ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5, ShipComponentType::RhinoScanner}});
    state.shipDesigns.push_back({100, 2, "Jammer", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::AreaJammer}});
    state.shipDesigns.push_back({101, 1, "Patrol", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::AntiMatterPulverizer}});
    auto& observer = state.fleets.front();
    observer.design = 99;
    observer.ships = {{99, 1}};
    observer.destination = scoutEnd;
    observer.warp = 5;
    observer.fuel = fleet_fuel_capacity(state, observer);
    const auto observerId = observer.id;

    Fleet jammer;
    jammer.id = state.nextFleetId++;
    jammer.owner = 2;
    jammer.design = 100;
    jammer.position = jammerPosition;
    state.fleets.push_back(jammer);
    const TurnProcessor processor;
    const auto scout_scans = [&](const GameState& game, StarId star) {
        const auto& reports = game.players.front().pendingSurveyReports;
        return std::find_if(reports.begin(), reports.end(), [&](const auto& report) {
            return report.star == star && report.sourceFleet == observerId;
        });
    };

    const auto jammed = processor.process(state, {});
    const auto jammedEnd = jammed.players.front().pendingSurveyReports.end();
    assert(scout_scans(jammed, 2) == jammedEnd);
    const auto slow = scout_scans(jammed, 3);
    assert(slow != jammedEnd && slow->observedTurn == 11 && slow->deliveryTurn == 12);

    auto clear = state;
    clear.fleets.pop_back();
    assert(survey_level(processor.process(clear, {}), 1, 2) == SurveyLevel::SystemScan);

    Fleet patrol;
    patrol.id = state.nextFleetId++;
    patrol.owner = 1;
    patrol.design = 101;
    patrol.position = {115, 0};
    patrol.destination = jammerPosition;
    patrol.warp = 5;
    patrol.fuel = fleet_fuel_capacity(state, patrol);
    state.fleets.push_back(patrol);
    const auto after = processor.process(state, {});
    assert(after.fleets.front().position == scoutEnd);
    assert(std::none_of(after.fleets.begin(), after.fleets.end(),
        [&](const auto& fleet) { return fleet.id == jammer.id; }));
    assert(survey_level(after, 1, 2) < SurveyLevel::SystemScan);
    assert(scout_scans(after, 2) == after.players.front().pendingSurveyReports.end());
    assert(survey_level(after, 1, 3) == SurveyLevel::SystemScan); // Sends over the restored fast link.
}

void remote_report_remains_in_flight_until_delivery()
{
    auto state = survey_fixture({420.0, 0.0}, {450.0, 0.0}, 10);
    const TurnProcessor processor;

    const auto turn11 = processor.process_with_events(state, {});
    assert(turn11.state.turn == 11);
    assert(!is_surveyed(turn11.state, 1, 2));
    assert(find_event(turn11.events, GameEventKind::SystemSurveyed) == nullptr);
    assert(turn11.state.players.front().pendingSurveyReports.size() == 1);
    const auto& pending = turn11.state.players.front().pendingSurveyReports.front();
    assert(pending.star == 2);
    assert(pending.sourceFleet == 1);
    assert(pending.observedTurn == 11);
    assert(pending.deliveryTurn == 14);
    assert(pending.level == SurveyLevel::SystemScan);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    assert(!is_surveyed(turn12.state, 1, 2));
    assert(find_event(turn12.events, GameEventKind::SystemSurveyed) == nullptr);
    assert(turn12.state.players.front().pendingSurveyReports.size() == 1);
    assert(turn12.state.players.front().pendingSurveyReports.front().observedTurn == 11);

    const auto turn13 = processor.process_with_events(turn12.state, {});
    assert(!is_surveyed(turn13.state, 1, 2));
    assert(find_event(turn13.events, GameEventKind::SystemSurveyed) == nullptr);

    const auto turn14 = processor.process_with_events(turn13.state, {});
    assert(!is_surveyed(turn14.state, 1, 2));
    assert(survey_level(turn14.state, 1, 2) == SurveyLevel::SystemScan);
    assert(turn14.state.players.front().pendingSurveyReports.empty());
    const auto* survey = find_event(turn14.events, GameEventKind::SystemSurveyed);
    assert(survey);
    assert(survey->observedTurn == 11);
    assert(survey->turn == 14);

    const auto turn15 = processor.process_with_events(turn14.state, {});
    assert(find_event(turn15.events, GameEventKind::SystemSurveyed) == nullptr);
}

void arrival_and_dwell_progress_through_all_survey_stages()
{
    auto state = survey_fixture({0.0, 0.0}, {50.0, 0.0});
    state.planets[1].precursorArtifacts = {true, false, 0, 11};
    auto& scout = state.fleets.front();
    scout.destination = Position{50.0, 0.0};
    scout.fuel = 300.0;
    const TurnProcessor processor;

    const auto arrival = processor.process_with_events(state, {});
    assert(survey_level(arrival.state, 1, 2) == SurveyLevel::OrbitalSurvey);
    assert(known_planet_habitability(arrival.state, 1, 2) == 82);
    assert(!planet_geology_known(arrival.state, 1, 2));
    assert(!known_precursor_artifact_hint(arrival.state, 1, 2).has_value());
    const auto* orbital = find_event(arrival.events, GameEventKind::SystemSurveyed);
    assert(orbital);
    assert(orbital->surveyLevel == SurveyLevel::OrbitalSurvey);
    assert(orbital->quantity == 82);

    const auto dwell = processor.process_with_events(arrival.state, {});
    assert(survey_level(dwell.state, 1, 2) == SurveyLevel::GeologicalSurvey);
    assert(planet_geology_known(dwell.state, 1, 2));
    const auto* geological = find_event(dwell.events, GameEventKind::SystemSurveyed);
    assert(geological);
    assert(geological->surveyLevel == SurveyLevel::GeologicalSurvey);
    assert(!known_precursor_artifact_hint(dwell.state, 1, 2).has_value());

    const auto deep = processor.process_with_events(dwell.state, {});
    assert(survey_level(deep.state, 1, 2) == SurveyLevel::DeepSurvey);
    assert(known_precursor_artifact_hint(deep.state, 1, 2) == true);
    const auto* deepSurvey = find_event(deep.events, GameEventKind::SystemSurveyed);
    assert(deepSurvey);
    assert(deepSurvey->surveyLevel == SurveyLevel::DeepSurvey);
    assert(deepSurvey->precursorArtifactHint);

    const auto complete = processor.process_with_events(deep.state, {});
    assert(survey_level(complete.state, 1, 2) == SurveyLevel::DeepSurvey);
    assert(find_event(complete.events, GameEventKind::SystemSurveyed) == nullptr);
}

const GameEvent* find_event(const std::vector<GameEvent>& events, GameEventKind kind)
{
    const auto it = std::find_if(events.begin(), events.end(), [&](const GameEvent& event) {
        return event.kind == kind;
    });
    return it == events.end() ? nullptr : &*it;
}

void remote_route_completion_obeys_communications_delay()
{
    auto state = survey_fixture({420.0, 0.0}, {450.0, 0.0}, 10);
    state.players.front().surveyedStars.push_back(2);
    auto& scout = state.fleets.front();
    scout.destination = Position{450.0, 0.0};
    scout.fuel = 300.0;

    const TurnProcessor processor;
    const auto turn11 = processor.process_with_events(state, {});
    assert(find_event(turn11.events, GameEventKind::RouteCompleted) == nullptr);
    assert(turn11.state.players.front().pendingPlayerReports.size() == 1);
    const auto& pending = turn11.state.players.front().pendingPlayerReports.front();
    assert(pending.kind == PlayerReportKind::RouteCompleted);
    assert(pending.observedTurn == 11);
    assert(pending.deliveryTurn == 14);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    const auto turn13 = processor.process_with_events(turn12.state, {});
    const auto turn14 = processor.process_with_events(turn13.state, {});
    const auto* event = find_event(turn14.events, GameEventKind::RouteCompleted);
    assert(event);
    assert(event->turn == 14);
    assert(event->observedTurn == 11);
    assert(event->fleet == scout.id);
    assert(event->star == 2);
    assert(same_position(event->position, {450.0, 0.0}));
    assert(turn14.state.players.front().pendingPlayerReports.empty());
}

void intermediate_waypoint_emits_arrival_not_completion()
{
    auto state = survey_fixture({0.0, 0.0}, {50.0, 0.0});
    state.players.front().surveyedStars.push_back(2);
    auto& scout = state.fleets.front();
    scout.destination = Position{50.0, 0.0};
    scout.waypointQueue.push_back({{100.0, 0.0}, 8, {}});
    scout.fuel = 300.0;

    const TurnProcessor processor;
    const auto result = processor.process_with_events(state, {});
    const auto* arrived = find_event(result.events, GameEventKind::FleetArrived);
    assert(arrived);
    assert(find_event(result.events, GameEventKind::RouteCompleted) == nullptr);
    assert(arrived->star == 2);
    assert(result.state.fleets.front().destination.has_value());
    assert(same_position(*result.state.fleets.front().destination, {100.0, 0.0}));
}

void fuel_stall_warns_once_and_only_after_delivery()
{
    auto state = survey_fixture({420.0, 0.0}, {500.0, 0.0}, 10);
    state.players.front().surveyedStars.push_back(2);
    auto& scout = state.fleets.front();
    scout.destination = Position{500.0, 0.0};
    scout.fuel = 0.0;

    const TurnProcessor processor;
    const auto turn11 = processor.process_with_events(state, {});
    assert(turn11.state.fleets.front().fuelStalled);
    assert(find_event(turn11.events, GameEventKind::FleetStalledForFuel) == nullptr);
    assert(turn11.state.players.front().pendingPlayerReports.size() == 1);
    assert(turn11.state.players.front().pendingPlayerReports.front().deliveryTurn == 14);

    const auto turn12 = processor.process_with_events(turn11.state, {});
    assert(turn12.state.players.front().pendingPlayerReports.size() == 1);
    const auto turn13 = processor.process_with_events(turn12.state, {});
    assert(find_event(turn13.events, GameEventKind::FleetStalledForFuel) == nullptr);
    const auto turn14 = processor.process_with_events(turn13.state, {});
    const auto* warning = find_event(turn14.events, GameEventKind::FleetStalledForFuel);
    assert(warning);
    assert(warning->severity == GameEventSeverity::Warning);

    const auto turn15 = processor.process_with_events(turn14.state, {});
    assert(find_event(turn15.events, GameEventKind::FleetStalledForFuel) == nullptr);
    assert(turn15.state.players.front().pendingPlayerReports.empty());
}

void local_production_completion_is_immediate_and_deterministic()
{
    auto state = make_demo_game();
    state.planets.front().productionQueue.push_back({ProductionKind::Factory, 0, 0});
    const TurnProcessor processor;

    const auto first = processor.process_with_events(state, {});
    const auto replay = processor.process_with_events(state, {});
    const auto* event = find_event(first.events, GameEventKind::ProductionCompleted);
    const auto* replayEvent = find_event(replay.events, GameEventKind::ProductionCompleted);
    assert(event && replayEvent);
    assert(event->turn == 2);
    assert(event->observedTurn == 2);
    assert(event->recipient == 1);
    assert(event->planet == 1);
    assert(event->star == 1);
    assert(event->productionKind == ProductionKind::Factory);
    assert(event->quantity == 5);
    assert(event->id == replayEvent->id);

    auto shipState = make_demo_game();
    shipState.planets.front().productionQueue.push_back({
        ProductionKind::ColonyShip,
        0,
        kColonyShipDesignId,
    });
    const auto shipResult = processor.process_with_events(shipState, {});
    const auto* shipEvent = find_event(shipResult.events, GameEventKind::ProductionCompleted);
    assert(shipEvent);
    assert(shipEvent->productionKind == ProductionKind::ColonyShip);
    assert(shipEvent->shipDesign == kColonyShipDesignId);
    assert(shipEvent->fleet == 2);
    assert(shipEvent->quantity == 2);
}

void colony_founding_emits_a_player_event()
{
    auto state = make_demo_game();
    set_survey_level(state, 1, 2, SurveyLevel::OrbitalSurvey, state.turn);
    const auto* target = find_star(state, 2);
    assert(target);
    auto& colonyShip = state.fleets.front();
    colonyShip.position = target->position;
    colonyShip.design = kColonyShipDesignId;
    colonyShip.ships = {{kColonyShipDesignId, 1}};
    colonyShip.role = FleetRole::ColonyShip;
    colonyShip.colonists = 500;

    PlayerOrders orders;
    orders.player = 1;
    orders.orders.push_back(ColonizePlanetOrder{colonyShip.id, 2});
    const TurnProcessor processor;
    const auto result = processor.process_with_events(state, {orders});

    const auto* event = find_event(result.events, GameEventKind::ColonyFounded);
    assert(event);
    assert(event->recipient == 1);
    assert(event->star == 2);
    assert(event->planet == 2);
    assert(event->fleet == 1);
    assert(event->shipDesign == kColonyShipDesignId);
    assert(result.state.planets[1].owner == 1);
    assert(result.state.fleets.empty());
}

void mineral_shortage_warns_once_per_blocked_transition()
{
    auto state = make_demo_game();
    auto& earth = state.planets.front();
    earth.population = 1;
    earth.industry = 1;
    earth.minerals = {};
    earth.productionQueue.push_back({ProductionKind::Factory, 0, 0});
    const TurnProcessor processor;

    const auto first = processor.process_with_events(state, {});
    const auto replay = processor.process_with_events(state, {});
    const auto* warning = find_event(first.events, GameEventKind::ProductionWaitingForMinerals);
    const auto* replayWarning = find_event(replay.events, GameEventKind::ProductionWaitingForMinerals);
    assert(warning && replayWarning);
    assert(warning->severity == GameEventSeverity::Warning);
    assert(warning->planet == 1);
    assert(warning->productionKind == ProductionKind::Factory);
    assert(warning->id == replayWarning->id);
    assert(first.state.planets.front().productionWaitingForMinerals);

    auto stillBlocked = first.state;
    stillBlocked.planets.front().minerals = {};
    const auto second = processor.process_with_events(stillBlocked, {});
    assert(find_event(second.events, GameEventKind::ProductionWaitingForMinerals) == nullptr);
    assert(second.state.planets.front().productionWaitingForMinerals);
}

void enemy_contacts_follow_sensor_transitions_and_remote_delivery()
{
    auto state = survey_fixture({420.0, 0.0}, {450.0, 0.0}, 10);
    state.players.push_back({2, "Opposition"});
    auto enemy = state.fleets.front();
    enemy.id = 22;
    enemy.owner = 2;
    enemy.name = "Secret enemy name";
    enemy.position = {450.0, 0.0};
    enemy.design = kColonyShipDesignId;
    enemy.ships = {{kColonyShipDesignId, 1}};
    state.fleets.push_back(enemy);
    const TurnProcessor processor;
    const auto first = processor.process_with_events(state, {});
    const auto replay = processor.process_with_events(state, {});
    assert(find_event(first.events, GameEventKind::EnemyFleetDetected) == nullptr);
    assert(first.state.players[0].observedEnemyFleets.size() == 1);
    const auto& pending = first.state.players[0].pendingPlayerReports;
    const auto contact = std::find_if(pending.begin(), pending.end(), [](const auto& report) {
        return report.kind == PlayerReportKind::EnemyFleetDetected;
    });
    assert(contact != pending.end() && contact->deliveryTurn == 14);
    assert(contact->contactOwner == 2 && contact->contact && distance_between(contact->contactPosition, {450.0, 0.0}) <= contact->contact->uncertainty);
    assert(contact->contactPosition.x == replay.state.players[0].pendingPlayerReports.back().contactPosition.x);
    const auto view = make_player_view(first.state, 1);
    assert(view.state.players.front().observedEnemyFleets.empty());
    assert(view.state.players.front().pendingPlayerReports.empty());
    assert(std::none_of(view.state.fleets.begin(), view.state.fleets.end(), [](const Fleet& fleet) {
        return fleet.owner == 2;
    }));

    auto departed = first.state;
    departed.fleets.back().position = {900.0, 0.0};
    const auto second = processor.process_with_events(departed, {});
    assert(find_event(second.events, GameEventKind::EnemyFleetLost) == nullptr);
    assert(second.state.players[0].observedEnemyFleets.empty());
    assert(processor.process_with_events(second.state, {}).state.players[0].pendingPlayerReports.size() == 2);
    const auto third = processor.process_with_events(second.state, {});
    const auto fourth = processor.process_with_events(third.state, {});
    const auto* detected = find_event(fourth.events, GameEventKind::EnemyFleetDetected);
    assert(detected && detected->recipient == 1 && detected->fleet == 22);
    assert(detected->contactOwner == 2 && detected->observedTurn == 11 && detected->turn == 14);
    assert(detected->contact && same_position(detected->position, contact->contactPosition));
    assert(find_event(fourth.events, GameEventKind::EnemyFleetLost) == nullptr);
    const auto fifth = processor.process_with_events(fourth.state, {});
    const auto* lost = find_event(fifth.events, GameEventKind::EnemyFleetLost);
    assert(lost && lost->observedTurn == 12 && lost->turn == 15);
    assert(same_position(lost->position, detected->position));

    auto local = survey_fixture({0.0, 0.0}, {250.0, 0.0});
    local.players.push_back({2, "Opposition"});
    enemy.position = {20.0, 0.0};
    local.fleets.push_back(enemy);
    const auto immediate = processor.process_with_events(local, {});
    assert(find_event(immediate.events, GameEventKind::EnemyFleetDetected));
    const auto quiet = processor.process_with_events(immediate.state, {});
    assert(find_event(quiet.events, GameEventKind::EnemyFleetDetected) == nullptr);
    auto away = quiet.state;
    away.fleets.back().position = {500.0, 0.0};
    const auto disappeared = processor.process_with_events(away, {});
    assert(find_event(disappeared.events, GameEventKind::EnemyFleetLost));
}

} // namespace

int main()
{
    // An older exact survey arriving after a newer rough scan has its own
    // observation date. It must not retain the rough snapshot under a deep level.
    {
        auto state = survey_fixture({450,0}, {450,0}, 4);
        observe_current_sensor_coverage(state, 4);
        auto exact = state.players[0].pendingSurveyReports.back();
        exact.level = SurveyLevel::DeepSurvey;
        exact.deliveryTurn = 7;
        set_survey_level(state, 1, 2, SurveyLevel::BasicScan, 6);
        state.players[0].pendingSurveyReports = {exact};
        state.turn = 7;
        (void)deliver_due_survey_reports(state);
        const auto& intel = state.players[0].surveyKnowledge.back();
        assert(intel.level == SurveyLevel::DeepSurvey && intel.planetSnapshot->environment);
        assert(intel.observedTurn == 4 && known_planet_habitability(state, 1, 2) == 82);
    }
    ordinary_scanner_reports_system_contact_without_planet_data();
    penetrating_scanner_estimates_planet_during_a_connected_flyby();
    remote_geology_uses_the_actual_flyby_and_delivers_coarse_evidence();
    remote_geology_finds_clear_parts_of_an_interfered_flyby();
    flyby_uses_area_jamming_at_the_observation_point();
    flyby_dispatch_uses_the_jammers_completed_movement();
    flyby_dispatch_waits_for_movement_after_rendezvous();
    continued_flyby_keeps_interference_from_before_arrival();
    continued_flyby_uses_outpost_power_from_before_fuel_arrivals();
    flyby_keeps_pre_battle_sensor_readings();
    remote_report_remains_in_flight_until_delivery();
    arrival_and_dwell_progress_through_all_survey_stages();
    remote_route_completion_obeys_communications_delay();
    intermediate_waypoint_emits_arrival_not_completion();
    fuel_stall_warns_once_and_only_after_delivery();
    local_production_completion_is_immediate_and_deterministic();
    colony_founding_emits_a_player_event();
    mineral_shortage_warns_once_per_blocked_transition();
    enemy_contacts_follow_sensor_transitions_and_remote_delivery();
    std::cout << "player knowledge tests passed\n";
    return 0;
}
