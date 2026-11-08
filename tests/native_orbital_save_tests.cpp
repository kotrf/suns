#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/combat.hpp"
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;
namespace {
bool same_minerals(const MineralCargo& a, const MineralCargo& b)
{
    return std::abs(a.ironium-b.ironium) < 1e-9 && std::abs(a.boranium-b.boranium) < 1e-9
        && std::abs(a.germanium-b.germanium) < 1e-9;
}
SaveGameData envelope(GameState state)
{
    SaveGameData save; save.state = std::move(state); save.campaignId = 115; save.turnToken = 17;
    save.pendingOrders = {1, {}};
    return save;
}
void resident_truth_and_pending_evidence_round_trip()
{
    auto state = test::native_orbital_fixture();
    queue_survey_report(state, 1, 1, 0, state.stars.front().position, state.turn, SurveyLevel::DeepSurvey, true);
    assert(!state.players.front().pendingSurveyReports.empty());
    const auto before = state.planets.front().nativeOrbit;
    QTemporaryDir directory; QString error; SaveGameData loaded;
    const auto path = directory.filePath("native.suns");
    assert(write_save_game_file(path, envelope(state), error));
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.planets.front().nativeOrbit == before);
    const auto& player = loaded.state.players.front();
    assert(player.surveyKnowledge.front().planetSnapshot->habitat->nativeOrbit == before);
    assert(player.pendingSurveyReports.back().planetSnapshot->habitat->nativeOrbit == before);
    const PlayerOrders cargo{1, {TransferCargoOrder{{0, loaded.state.fleets.front().id}, {1, 0, true}, 100, {}}}};
    assert(TurnProcessor{}.process(state, {cargo}).planets.front().nativeOrbit
        == TurnProcessor{}.process(loaded.state, {cargo}).planets.front().nativeOrbit);
    for (int defect = 0; defect < 6; ++defect) {
        auto bad = state;
        if (defect == 0) bad.planets.front().nativeOrbit.station = 0;
        if (defect == 1) bad.planets.front().nativeOrbit.population = 100'001;
        if (defect == 2) bad.planets.front().population = 1;
        if (defect == 3) bad.orbitalStations.front().owner = 2;
        if (defect == 4) bad.players.front().race.primaryTrait = PrimaryRaceTrait::Generalist;
        if (defect == 5) {
            auto& observation = *bad.players.front().surveyKnowledge.front().planetSnapshot->habitat;
            observation.nativeOrbit = {}; observation.surfacePopulation = 1;
        }
        assert(write_save_game_file(path, envelope(bad), error));
        assert(!read_save_game_file(path, loaded, error));
    }
    assert(write_save_game_file(path, envelope(state), error));
    QFile file(path); assert(file.open(QIODevice::ReadOnly)); auto truncated = file.readAll(); file.close();
    truncated.chop(1); assert(file.open(QIODevice::WriteOnly)); assert(file.write(truncated) == truncated.size()); file.close();
    assert(!read_save_game_file(path, loaded, error));
}
void hidden_loss_is_frozen_in_export_and_delivered_loss_survives_reload()
{
    auto state = test::native_orbital_fixture();
    const auto station = state.orbitalStations.front().id;
    const auto residents = state.planets.front().nativeOrbit;
    destroy_native_orbital_population(state, station); state.orbitalStations.clear();
    auto host = envelope(state); host.mode = SessionMode::Host; host.playerTokens = {{1, 17}};
    auto exported = make_player_turn(host, 1);
    assert(exported.state.planets.front().nativeOrbit == residents);
    assert(exported.state.players.front().pendingSurveyReports.empty());
    QTemporaryDir directory; QString error; SaveGameData loaded;
    const auto path = directory.filePath("native.sunsturn");
    assert(write_save_game_file(path, exported, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.planets.front().nativeOrbit == residents);

    PendingPlayerReport report; report.kind = PlayerReportKind::SpaceBattle;
    report.observedTurn = state.turn+1; report.battle.emplace();
    SpaceBattleUnit unit; unit.owner = 1; unit.station = station; unit.initialShips = 1; unit.survivingShips = 0;
    report.battle->units.push_back(unit);
    state.turn = report.observedTurn;
    apply_received_station_battle(state.players.front(), report);
    assert(known_planet_owner(state, 1, 1) == 0);
    assert(write_save_game_file(path, envelope(state), error) && read_save_game_file(path, loaded, error));
    assert(known_planet_owner(loaded.state, 1, 1) == 0);
    assert(!make_player_view(loaded.state, 1).state.planets.front().nativeOrbit.population);
}
void real_pbem_decoding_keeps_guards_and_shared_unloading()
{
    const auto state = test::native_orbital_fixture(); const auto fleet = state.fleets.front().id;
    QTemporaryDir directory; QString error; const auto path = directory.filePath("native.sunsorders");
    for (const auto target : {0U, 600U}) {
        TurnOrderFileData packet{115, state.turn, 17, {1, {SetFleetColonistsOrder{1, fleet, target}}}, {"Set cargo"}};
        assert(write_turn_order_file(path, packet, error)); TurnOrderFileData decoded;
        assert(read_turn_order_file(path, decoded, error));
        const auto next = TurnProcessor{}.process(state, {decoded.orders});
        assert(next.planets.front().nativeOrbit == state.planets.front().nativeOrbit);
        assert(!next.planets.front().population && next.fleets.front().colonists == 500);
    }
    TurnOrderFileData packet{115, state.turn, 17,
        {1, {MoveFleetOrder{fleet, state.fleets.front().position, 5,
            {FleetArrivalActionKind::UnloadAll, 0, FleetCargoKind::All}}}}, {"Unload all"}};
    assert(write_turn_order_file(path, packet, error)); TurnOrderFileData decoded;
    assert(read_turn_order_file(path, decoded, error));
    const auto next = TurnProcessor{}.process(state, {decoded.orders});
    assert(next.planets.front().nativeOrbit.population == 10'500 && !next.planets.front().population);
    assert(next.fleets.front().colonists == 0 && next.fleets.front().minerals.ironium == 0);
    packet.orders = {1, {TransferCargoOrder{{0, fleet}, {1, 0, true}, 100, {}}}};
    packet.descriptions = {"Station transfer"};
    assert(write_turn_order_file(path, packet, error) && read_turn_order_file(path, decoded, error));
    const auto exact = TurnProcessor{}.process(state, {decoded.orders});
    assert(exact.planets.front().nativeOrbit.population == 10'100 && exact.fleets.front().colonists == 400);
    assert(!exact.planets.front().population && exact.fleets.front().minerals.ironium == 3);
}
void genuine_v114_has_no_native_residents()
{
    QString error; SaveGameData legacy;
    assert(read_save_game_file(QString::fromUtf8(SUNS_SOURCE_DIR "/tests/fixtures/native-orbital-v114.suns"), legacy, error));
    assert(legacy.state.planets.front().population > 0);
    for (const auto& planet : legacy.state.planets) assert(planet.nativeOrbit == NativeOrbitalPopulation{});
    for (const auto& player : legacy.state.players) for (const auto& h : player.history) assert(h.assetTotalsSynchronized);
    const auto next = TurnProcessor{}.process(legacy.state, {});
    const auto expected = legacy.state.planets.front().population
        + projected_population_growth(legacy.state, legacy.state.planets.front(), legacy.state.turn);
    assert(next.planets.front().population == expected && next.planets.front().owner == 1);
}
void unloading_history_waits_for_received_cargo()
{
    for (const auto residents : {10'000U, 99'900U, 100'000U}) {
        auto state = test::native_orbital_fixture();
        auto& planet = state.planets.front();
        planet.nativeOrbit.population = residents;
        planet.minerals = {100, 100, 100};
        auto& fleet = state.fleets.front();
        fleet.destination = fleet.position;
        fleet.arrivalAction = {FleetArrivalActionKind::UnloadAll, 0, FleetCargoKind::All};
        fleet.telemetry = {};
        fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
        // Removing the capital receiver creates a real one-year report delay.
        planet.radar = PlanetaryRadarLevel::None;
        set_survey_level(state, 1, planet.star, SurveyLevel::DeepSurvey, state.turn);
        assert(communication_delay_turns(state, 1, fleet.position) == 1);
        state.players.front().history.clear();
        record_empire_turn_statistics(state);
        const auto initialPopulation = residents+500;
        const auto unloaded = std::min(500U, 100'000U-residents);
        auto result = TurnProcessor{}.process_with_events(state, {});
        state = result.state;
        assert(state.planets.front().nativeOrbit.population == residents+unloaded);
        assert(state.fleets.front().colonists == 500-unloaded && state.fleets.front().minerals.ironium == 0);
        const auto manifest = std::find_if(state.players.front().pendingPlayerReports.begin(),
            state.players.front().pendingPlayerReports.end(), [](const auto& r) { return r.kind == PlayerReportKind::FreightDelivered; });
        assert(manifest != state.players.front().pendingPlayerReports.end());
        assert(manifest->observedTurn == state.turn && manifest->deliveryTurn == state.turn+1);
        const auto receipt = manifest->deliveryTurn;
        assert(std::none_of(result.events.begin(), result.events.end(), [](const auto& e) { return e.kind == GameEventKind::FreightDelivered; }));
        QTemporaryDir directory; QString error; SaveGameData loaded;
        const auto path = directory.filePath("unload.suns");
        const auto checkHistory = [&](const GameState& observed, bool delivered) {
            const auto& history = observed.players.front().history.back();
            assert(history.population == initialPopulation);
            assert(same_minerals(history.minerals, {103, 102, 101}));
            assert(history.freightRecorded);
            assert(history.colonistsDelivered == (delivered ? unloaded : 0));
            assert(same_minerals(history.freightDelivered, delivered ? MineralCargo{3, 2, 1} : MineralCargo{}));
            assert(history.colonyHistory.front().colonistsDelivered == history.colonistsDelivered);
            assert(same_minerals(history.colonyHistory.front().freightDelivered, history.freightDelivered));
        };
        const auto checkExport = [&](bool delivered) {
            auto host = envelope(state); host.mode = SessionMode::Host; host.playerTokens = {{1, 17}};
            const auto exported = make_player_turn(host, 1);
            assert(write_save_game_file(path, exported, error) && read_save_game_file(path, loaded, error));
            assert(loaded.state.planets.front().nativeOrbit.population == residents+(delivered ? unloaded : 0));
            assert(loaded.state.fleets.front().colonists == 500-(delivered ? unloaded : 0));
            assert(loaded.state.fleets.front().minerals.ironium == (delivered ? 0 : 3));
            checkHistory(loaded.state, delivered);
            // Rebuilding a received boundary must agree with its exported totals.
            const auto rebuilt = empire_turn_statistics(loaded.state, 1);
            assert(rebuilt.population == initialPopulation && same_minerals(rebuilt.minerals, {103, 102, 101}));
        };
        checkExport(false);
        // Preserve the actual pending manifest and telemetry across host reload.
        assert(write_save_game_file(path, envelope(state), error) && read_save_game_file(path, loaded, error));
        state = loaded.state;
        auto overdue = state;
        result = TurnProcessor{}.process_with_events(state, {});
        state = result.state;
        assert(state.turn == receipt);
        assert(std::count_if(result.events.begin(), result.events.end(), [](const auto& e) { return e.kind == GameEventKind::FreightDelivered; }) == 1);
        checkExport(true);
        record_empire_turn_statistics(state);
        checkHistory(state, true);
        state = TurnProcessor{}.process(state, {});
        checkHistory(state, false);
        std::uint64_t deliveredTotal = 0;
        for (const auto& h : state.players.front().history) deliveredTotal += h.colonistsDelivered;
        assert(deliveredTotal == unloaded);
        const auto previous = std::find_if(state.players.front().history.begin(), state.players.front().history.end(),
            [&](const auto& h) { return h.turn == receipt-1; });
        assert(previous != state.players.front().history.end() && previous->population == initialPopulation
            && previous->colonistsDelivered == 0 && same_minerals(previous->freightDelivered, {}));
        // A packet already due when a saved host resumes belongs to the
        // receiving boundary, not the new year resolved by process().
        overdue.turn = receipt;
        assert(write_save_game_file(path, envelope(overdue), error) && read_save_game_file(path, loaded, error));
        const auto resumed = TurnProcessor{}.process(loaded.state, {});
        const auto received = std::find_if(resumed.players.front().history.begin(), resumed.players.front().history.end(),
            [&](const auto& h) { return h.turn == receipt; });
        assert(received != resumed.players.front().history.end() && received->colonistsDelivered == unloaded
            && same_minerals(received->freightDelivered, {3, 2, 1}));
        assert(resumed.players.front().history.back().colonistsDelivered == 0
            && same_minerals(resumed.players.front().history.back().freightDelivered, {}));
    }
    // A connected unload is already received this year and is counted once.
    auto local = test::native_orbital_fixture();
    local.fleets.front().destination = local.fleets.front().position;
    local.fleets.front().arrivalAction = {FleetArrivalActionKind::UnloadAll, 0, FleetCargoKind::All};
    const auto immediate = TurnProcessor{}.process(local, {});
    assert(immediate.players.front().history.back().colonistsDelivered == 500);
    assert(same_minerals(immediate.players.front().history.back().freightDelivered, {3, 2, 1}));
}
void received_missing_fleet_is_counted_once()
{
    auto state = test::native_orbital_fixture();
    state.planets.front().minerals = {100, 100, 100};
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    state.pendingFleetLossContacts.push_back({state.fleets.front(), state.turn+1});
    state.fleets.clear();
    const auto history = empire_turn_statistics(state, 1);
    assert(history.fleets == 1 && history.ships == 3 && history.population == 10'500);
    assert(same_minerals(history.minerals, {103, 102, 101}));
    ++state.turn;
    const auto afterReceipt = empire_turn_statistics(state, 1);
    assert(afterReceipt.fleets == 0 && afterReceipt.population == 10'000);
    assert(same_minerals(afterReceipt.minerals, {100, 100, 100}));
}
void asynchronous_cargo_dates_survive_host_and_pbem()
{
    auto state = test::asynchronous_native_unload_fixture();
    QTemporaryDir directory; QString error; SaveGameData loaded, exported;
    const auto path = directory.filePath("asynchronous.suns");
    for (int year = 1; year <= 4; ++year) {
        std::uint64_t physicalPeople = 0; double physicalIronium = 0;
        for (const auto& p : state.planets) if (p.owner == 1) {
            physicalPeople += total_colony_population(p); physicalIronium += p.minerals.ironium;
        }
        for (const auto& f : state.fleets) if (f.owner == 1) {
            physicalPeople += f.colonists; physicalIronium += f.minerals.ironium;
        }
        assert(physicalPeople == 20'500 && physicalIronium == 203);
        auto host = envelope(state); host.mode = SessionMode::Host; host.playerTokens = {{1, 17}};
        assert(write_save_game_file(path, host, error) && read_save_game_file(path, loaded, error));
        state = loaded.state;
        assert(write_save_game_file(path, make_player_turn(loaded, 1), error) && read_save_game_file(path, exported, error));
        const auto& h = exported.state.players.front().history.back();
        assert(h.assetTotalsSynchronized == (year == 1));
        assert(h.newestAssetObservation == std::uint64_t(year));
        if (year == 3) {
            assert(h.population == 20'000 && h.minerals.ironium == 200 && h.oldestAssetObservation == 1);
            assert(!h.colonistsDelivered && h.freightDelivered.ironium == 0);
            assert(exported.state.fleets.front().colonists == 0 && exported.state.planets[1].nativeOrbit.population == 10'000);
            // Neither hidden truth nor the contents/schedule of undelivered
            // packets may turn this received subtotal into a certified total.
            auto altered = state;
            altered.planets[1].nativeOrbit.population = 1; altered.planets[1].minerals = {};
            altered.players.front().pendingSurveyReports.clear(); altered.players.front().pendingPlayerReports.clear();
            const auto same = empire_turn_statistics(altered, 1);
            assert(!same.assetTotalsSynchronized && same.population == h.population
                && same.minerals.ironium == h.minerals.ironium && same.oldestAssetObservation == h.oldestAssetObservation);
        }
        if (year == 4) {
            assert(h.population == 20'500 && h.minerals.ironium == 203 && h.oldestAssetObservation == 3);
            assert(h.colonistsDelivered == 500 && h.freightDelivered.ironium == 3);
            const auto& old = exported.state.players.front().history[2];
            assert(old.turn == 3 && !old.assetTotalsSynchronized && old.oldestAssetObservation == 1
                && old.newestAssetObservation == 3 && !old.colonistsDelivered);
        } else state = TurnProcessor{}.process(state, {});
    }
}
void history_date_bounds_and_genuine_v116_migration()
{
    QTemporaryDir directory; QString error; SaveGameData loaded;
    const auto path = directory.filePath("dates.suns");
    auto state = test::asynchronous_native_unload_fixture();
    for (int defect = 0; defect < 3; ++defect) {
        auto bad = state; auto& h = bad.players.front().history.back();
        if (defect == 0) h.oldestAssetObservation = h.newestAssetObservation+1;
        if (defect == 1) { h.assetTotalsSynchronized = false; h.newestAssetObservation = bad.turn+1; }
        if (defect == 2) h.oldestAssetObservation = 0; // A true flag cannot certify differing dates.
        assert(write_save_game_file(path, envelope(bad), error) && !read_save_game_file(path, loaded, error));
    }
    assert(write_save_game_file(path, envelope(state), error));
    QFile file(path); assert(file.open(QIODevice::ReadOnly)); auto raw = file.readAll(); file.close();
    raw[raw.size()-17] = char(2); // Last row's strict boolean, followed by two u64 dates.
    assert(file.open(QIODevice::WriteOnly) && file.write(raw) == raw.size()); file.close();
    assert(!read_save_game_file(path, loaded, error));
    assert(read_save_game_file(QString::fromUtf8(SUNS_SOURCE_DIR "/tests/fixtures/native-orbital-v116-asynchronous.suns"), loaded, error));
    assert(loaded.state.turn == 3 && loaded.state.players.front().history.back().population == 20'000);
    for (const auto& h : loaded.state.players.front().history)
        assert(!h.assetTotalsSynchronized && !h.oldestAssetObservation && !h.newestAssetObservation);
    assert(write_save_game_file(path, loaded, error) && read_save_game_file(path, loaded, error));
    const auto next = TurnProcessor{}.process(loaded.state, {});
    assert(next.players.front().history.back().population == 20'500);
    assert(next.players.front().history.back().colonistsDelivered == 500);
    assert(!next.players.front().history[2].assetTotalsSynchronized);
}
void native_mining_manifests_and_v117_migration()
{
    auto state = test::native_orbital_miner_fixture();
    state.planets.front().radar = PlanetaryRadarLevel::None;
    state.fleets.front().task = state.fleets.front().telemetry.task = FleetTask::RemoteMining;
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    state = TurnProcessor{}.process(state, {});
    assert(state.planets.front().minerals.ironium == 108);
    assert(state.players.front().history.back().remoteExtraction.ironium == 0);
    const auto mining = std::find_if(state.players.front().pendingPlayerReports.begin(), state.players.front().pendingPlayerReports.end(),
        [](const auto& r) { return r.kind == PlayerReportKind::RemoteMiningOutput; });
    assert(mining != state.players.front().pendingPlayerReports.end() && mining->deliveredMinerals.ironium == 8);
    assert(mining->deliveryTurn == state.turn+1);
    QTemporaryDir directory; QString error; SaveGameData loaded, reopened;
    const auto path = directory.filePath("mining.suns");
    auto host = envelope(state); host.mode = SessionMode::Host; host.playerTokens = {{1, 17}};
    assert(write_save_game_file(path, host, error) && read_save_game_file(path, loaded, error));
    const auto checkExport = [&](const SaveGameData& data, double expected) {
        assert(write_save_game_file(path, make_player_turn(data, 1), error) && read_save_game_file(path, reopened, error));
        assert(reopened.state.players.front().pendingPlayerReports.empty());
        assert(reopened.state.players.front().history.back().remoteExtraction.ironium == expected);
    };
    checkExport(loaded, 0);
    auto dueOnLoad = loaded.state; ++dueOnLoad.turn;
    const auto overdue = TurnProcessor{}.process(dueOnLoad, {});
    assert(overdue.players.front().history[2].turn == 3 && overdue.players.front().history[2].remoteExtraction.ironium == 8);
    assert(overdue.players.front().history.back().remoteExtraction.ironium == 0);
    const auto received = TurnProcessor{}.process_with_events(loaded.state, {});
    host.state = received.state; host.strategicMessages = received.events;
    checkExport(host, 8);
    assert(std::any_of(reopened.strategicMessages.begin(), reopened.strategicMessages.end(), [](const auto& e) {
        return e.kind == GameEventKind::RemoteMiningOutput && e.deliveredMinerals.ironium == 8 && e.observedTurn == 2 && e.turn == 3;
    }));
    assert(reopened.state.players.front().history[1].remoteExtraction.ironium == 0);
    // Order wire format remains v49; the decoder feeds the same physical rule.
    TurnOrderFileData packet{118, 1, 17, {1, {SetRemoteMiningOrder{1, true}}}, {"Remote mining"}};
    const auto orderPath = directory.filePath("mining.sunsorders"); TurnOrderFileData decoded;
    assert(write_turn_order_file(orderPath, packet, error) && read_turn_order_file(orderPath, decoded, error));
    assert(TurnProcessor{}.process(test::native_orbital_miner_fixture(), {decoded.orders}).planets.front().minerals.ironium == 108);

    assert(read_save_game_file(QString::fromUtf8(SUNS_SOURCE_DIR "/tests/fixtures/native-mining-v117.suns"), loaded, error));
    assert(loaded.state.players.front().history.back().remoteExtraction.ironium == 8);
    assert(!loaded.state.players.front().history.back().remoteExtractionRecorded);
    assert(write_save_game_file(path, loaded, error) && read_save_game_file(path, loaded, error));
    const auto continued = TurnProcessor{}.process(loaded.state, {});
    assert(continued.players.front().history.back().remoteExtractionRecorded);
    assert(continued.players.front().history.back().remoteExtraction.ironium == 8);
    assert(!continued.players.front().history[1].remoteExtractionRecorded);
}
GameState battle_fixture(bool stationSnapshot)
{
    auto state = test::native_orbital_fixture();
    auto& transport = state.fleets.front();
    transport.position = {-1000, -1000};
    transport.telemetry = confirmed_fleet_telemetry(state, transport);
    state.players.push_back({2, "Attacker"});
    state.shipDesigns.push_back({10, 2, "Attacker", ShipHullType::Destroyer,
        {ShipComponentType::FuelMizer, ShipComponentType::AntiMatterPulverizer, ShipComponentType::Jammer50}});
    state.nextShipDesignId = 11;
    Fleet attacker; attacker.id = state.nextFleetId++; attacker.owner = 2;
    attacker.design = 10; attacker.ships = {{10, 100}}; attacker.position = state.stars.front().position;
    attacker.fuel = fleet_fuel_capacity(state, attacker);
    attacker.telemetry = confirmed_fleet_telemetry(state, attacker);
    state.fleets.push_back(attacker);
    if (!stationSnapshot) {
        queue_survey_report(state, 1, 1, 0, state.stars.front().position, state.turn+1, SurveyLevel::DeepSurvey);
        state.players.front().pendingSurveyReports.back().deliveryTurn = state.turn+3;
        state.players.front().surveyKnowledge.front().planetSnapshot->habitat.reset();
    }
    for (auto& player : state.players) player.history.clear();
    record_empire_turn_statistics(state);
    return state;
}
void history_and_pbem_wait_for_station_loss_receipt()
{
    auto state = battle_fixture(true);
    assert(state.players.front().history.back().population == 10'500);
    state = TurnProcessor{}.process(state, {});
    assert(!state.planets.front().owner && state.orbitalStations.empty());
    const auto report = std::find_if(state.players.front().pendingPlayerReports.begin(), state.players.front().pendingPlayerReports.end(),
        [](const auto& r) { return r.kind == PlayerReportKind::SpaceBattle; });
    assert(report != state.players.front().pendingPlayerReports.end() && report->deliveryTurn > state.turn);
    const auto receipt = report->deliveryTurn;
    QTemporaryDir directory; QString error; SaveGameData loaded;
    const auto path = directory.filePath("history.sunsturn");
    const auto checkExport = [&](std::uint64_t population, std::uint32_t colonies) {
        auto host = envelope(state); host.mode = SessionMode::Host; host.playerTokens = {{1, 17}};
        const auto exported = make_player_turn(host, 1);
        assert(write_save_game_file(path, exported, error) && read_save_game_file(path, loaded, error));
        const auto& history = loaded.state.players.front().history.back();
        assert(loaded.state.players.front().stationDestructions.empty() == bool(colonies));
        assert(history.population == population && history.colonies == colonies);
        assert(history.colonyHistory.size() == colonies);
        if (colonies) {
            assert(loaded.state.planets.front().nativeOrbit.population == 10'000);
            assert(history.colonyHistory.front().population == 10'000);
            assert(history.minerals.ironium == 103);
        }
    };
    checkExport(10'500, 1);
    while (state.turn < receipt) state = TurnProcessor{}.process(state, {});
    checkExport(500, 0);
    // Receiving loss changes this boundary; it must not rewrite the older graph.
    const auto old = std::find_if(loaded.state.players.front().history.begin(), loaded.state.players.front().history.end(),
        [&](const auto& h) { return h.turn == receipt-1; });
    assert(old != loaded.state.players.front().history.end() && old->population == 10'500 && old->colonies == 1);
}
void destruction_before_first_snapshot_survives_reload()
{
    auto state = TurnProcessor{}.process(battle_fixture(false), {});
    const auto report = std::find_if(state.players.front().pendingPlayerReports.begin(), state.players.front().pendingPlayerReports.end(),
        [](const auto& r) { return r.kind == PlayerReportKind::SpaceBattle; });
    assert(report != state.players.front().pendingPlayerReports.end());
    const auto receipt = report->deliveryTurn;
    while (state.turn < receipt) state = TurnProcessor{}.process(state, {});
    assert(!state.planets.front().owner && state.orbitalStations.empty());
    assert(state.players.front().stationDestructions.size() == 1);
    assert(!state.players.front().pendingSurveyReports.empty());
    const auto late = state.players.front().pendingSurveyReports.back().deliveryTurn;
    assert(late > state.turn);
    QTemporaryDir directory; QString error; SaveGameData loaded;
    const auto path = directory.filePath("before-first-snapshot.suns");
    assert(write_save_game_file(path, envelope(state), error) && read_save_game_file(path, loaded, error));
    state = loaded.state;
    while (state.turn < late) state = TurnProcessor{}.process(state, {});
    assert(!state.planets.front().owner && state.orbitalStations.empty());
    assert(known_planet_owner(state, 1, 1) == 0);
    const auto view = make_player_view(state, 1).state;
    assert(view.orbitalStations.empty() && !view.planets.front().nativeOrbit.population);
    assert(write_save_game_file(path, envelope(view), error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.orbitalStations.empty() && !loaded.state.planets.front().nativeOrbit.population);
}
void destruction_identity_is_monotonic_and_does_not_kill_a_replacement()
{
    auto state = test::native_orbital_fixture();
    auto& player = state.players.front();
    PendingPlayerReport battle; battle.observedTurn = state.turn; battle.planet = 1; battle.battle.emplace();
    SpaceBattleUnit unit; unit.owner = 1; unit.station = 1; unit.initialShips = 1;
    battle.battle->units.push_back(unit);
    apply_received_station_battle(player, battle);
    apply_received_station_battle(player, battle);
    assert(player.stationDestructions.size() == 1 && player.stationDestructions.front().observedTurn == state.turn);
    // A later paid founding/replacement has a different station identity.
    ++state.turn; state.orbitalStations.front().id = 2; state.nextOrbitalStationId = 3;
    state.planets.front().nativeOrbit.station = 2;
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    const auto replacement = capture_planet_survey(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    auto incoming = replacement;
    merge_received_habitat_station(incoming, std::nullopt, player);
    assert(incoming->habitat->station->id == 2 && incoming->habitat->nativeOrbit.population == 10'000);
    apply_received_station_battle(player, battle);
    assert(known_planet_owner(state, 1, 1) == 1 && make_player_view(state, 1).state.orbitalStations.front().id == 2);
}
void ledger_round_trip_bounds_and_v115_migration()
{
    QTemporaryDir directory; QString error; SaveGameData loaded;
    const auto path = directory.filePath("receipts.suns");
    auto state = test::native_orbital_fixture();
    remember_station_destruction(state.players.front(), {99, 1, state.turn});
    assert(write_save_game_file(path, envelope(state), error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.players.front().stationDestructions == state.players.front().stationDestructions);
    for (int defect = 0; defect < 4; ++defect) {
        auto invalid = state;
        auto& facts = invalid.players.front().stationDestructions;
        if (defect == 0) facts.front().observedTurn = state.turn+1;
        if (defect == 1) facts.front().station = 0;
        if (defect == 2) facts.push_back(facts.front());
        if (defect == 3) { facts.front().station = 100; facts.push_back({99, 1, state.turn}); }
        assert(write_save_game_file(path, envelope(invalid), error) && !read_save_game_file(path, loaded, error));
    }
    assert(read_save_game_file(QString::fromUtf8(SUNS_SOURCE_DIR "/tests/fixtures/native-orbital-v115-loss.suns"), loaded, error));
    assert(loaded.state.players.front().stationDestructions.size() == 1);
    assert(!loaded.state.players.front().pendingSurveyReports.empty());
    assert(loaded.state.players.front().pendingSurveyReports.back().planetSnapshot->habitat->nativeOrbit.population == 10'000);
    // The migrated receipt is independent of the current cached fit too.
    loaded.state.players.front().surveyKnowledge.front().planetSnapshot->habitat.reset();
    // A later survey envelope carrying that old fit must be merged, rather
    // than skipped as a duplicate of the existing same-date survey record.
    loaded.state.players.front().pendingSurveyReports.back().observedTurn = loaded.state.turn;
    loaded.state.turn = loaded.state.players.front().pendingSurveyReports.back().deliveryTurn;
    (void)deliver_due_survey_reports(loaded.state);
    assert(known_planet_owner(loaded.state, 1, 1) == 0);
    assert(!make_player_view(loaded.state, 1).state.planets.front().nativeOrbit.population);
}
}
int main(int argc, char** argv)
{
    if (argc == 2 && QString::fromLatin1(argv[1]) == "asynchronous") { asynchronous_cargo_dates_survive_host_and_pbem(); history_date_bounds_and_genuine_v116_migration(); return 0; }
    if (argc == 2 && QString::fromLatin1(argv[1]) == "freight") { unloading_history_waits_for_received_cargo(); return 0; }
    if (argc == 2 && QString::fromLatin1(argv[1]) == "history") { history_and_pbem_wait_for_station_loss_receipt(); return 0; }
    if (argc == 2 && QString::fromLatin1(argv[1]) == "first-snapshot") { destruction_before_first_snapshot_survives_reload(); return 0; }
    resident_truth_and_pending_evidence_round_trip();
    hidden_loss_is_frozen_in_export_and_delivered_loss_survives_reload();
    real_pbem_decoding_keeps_guards_and_shared_unloading();
    genuine_v114_has_no_native_residents();
    unloading_history_waits_for_received_cargo();
    received_missing_fleet_is_counted_once();
    asynchronous_cargo_dates_survive_host_and_pbem();
    history_date_bounds_and_genuine_v116_migration();
    native_mining_manifests_and_v117_migration();
    history_and_pbem_wait_for_station_loss_receipt();
    destruction_before_first_snapshot_survives_reload();
    destruction_identity_is_monotonic_and_does_not_kill_a_replacement();
    ledger_round_trip_bounds_and_v115_migration();
}
