#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include "native_orbital_ui.hpp"
#include "suns/native_orbital_support.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/production.hpp"
#include <QTemporaryDir>
#include <algorithm>
#include <cassert>
using namespace suns;
namespace {
SaveGameData legacy_host()
{
    SaveGameData host; QString error;
    assert(read_save_game_file(QStringLiteral(SUNS_SOURCE_DIR "/tests/fixtures/native-support-v120-host.suns"), host, error));
    return host;
}
void paid_manufacture_waits_for_resupply_after_host_reload()
{
    SaveGameData host; host.mode = SessionMode::Host;
    host.campaignId = 288; host.turnToken = 17; host.playerTokens = {{1, 17}};
    host.pendingOrders = {1, {}}; host.state = test::native_miner_waiting_for_ironium_fixture();
    const auto home = host.state.planets.front().id;
    const auto transport = host.state.fleets.back().id;
    const auto& waiting = host.state.planets.front();
    assert(host.state.turn == 5 && waiting.productionQueue.size() == 1);
    assert(waiting.productionQueue.front().remainingCost == 0 && waiting.productionWaitingForMinerals);
    assert(waiting.minerals.ironium == 42 && waiting.nativeOrbit.reserveUnits == 6);
    const auto paidQueue = waiting.productionQueue;
    const auto boranium = waiting.minerals.boranium;
    QTemporaryDir dir; QString error; SaveGameData loaded;
    const auto save = dir.filePath("waiting.suns"), orders = dir.filePath("resupply.sunsorders");
    assert(write_save_game_file(save, host, error) && read_save_game_file(save, loaded, error)); host = loaded;
    const TransferCargoOrder deliver{{0, transport}, {home, 0}, 0, {20, 0, 0}};
    const TransferCargoOrder removeB{{home, 0}, {0, transport}, 0, {0, boranium, 0}};
    TurnOrderFileData packet{host.campaignId, host.state.turn, host.turnToken,
        {1, {deliver, removeB}}, {"Deliver ironium", "Load boranium"}};
    assert(write_turn_order_file(orders, packet, error)); TurnOrderFileData decoded;
    assert(read_turn_order_file(orders, decoded, error));
    const auto paused = TurnProcessor{}.process_with_events(host.state, {decoded.orders});
    const auto& p = paused.state.planets.front();
    assert(p.nativeOrbit.service == HabitatService::Reserve && p.nativeOrbit.productionAvailable == 0);
    assert(p.nativeOrbit.reserveUnits == 4 && p.minerals.ironium == 62);
    assert(p.productionQueue == paidQueue && paused.state.fleets.size() == 2);
    const auto completed = [&](const auto& result) {
        return std::count_if(result.events.begin(), result.events.end(), [&](const auto& e) {
            return e.kind == GameEventKind::ProductionCompleted && e.planet == home;
        });
    };
    assert(completed(paused) == 0);
    const auto normal = TurnProcessor{}.process_with_events(host.state, {{1, {deliver}}});
    assert(normal.state.planets.front().nativeOrbit.service == HabitatService::Normal);
    assert(normal.state.planets.front().productionQueue.empty() && normal.state.fleets.size() == 3 && completed(normal) == 1);
    host.state = paused.state;
    assert(write_save_game_file(save, host, error) && read_save_game_file(save, loaded, error)); host = loaded;
    assert(write_save_game_file(save, make_player_turn(host, 1), error) && read_save_game_file(save, loaded, error));
    assert(loaded.state.planets.front().productionQueue == paidQueue);
    assert(loaded.state.planets.front().nativeOrbit.service == HabitatService::Reserve);
    const auto stillPaused = TurnProcessor{}.process_with_events(host.state, {});
    assert(stillPaused.state.planets.front().productionQueue == paidQueue && completed(stillPaused) == 0);
    host.state = stillPaused.state;
    assert(write_save_game_file(save, host, error) && read_save_game_file(save, loaded, error)); host = loaded;
    packet.turn = host.state.turn; packet.orders = {1, {TransferCargoOrder{{0, transport}, {home, 0}, 0, {0, 20, 0}}}};
    packet.descriptions = {"Restore life support"};
    assert(write_turn_order_file(orders, packet, error) && read_turn_order_file(orders, decoded, error));
    const auto resumed = TurnProcessor{}.process_with_events(host.state, {decoded.orders});
    assert(resumed.state.planets.front().nativeOrbit.service == HabitatService::Normal);
    assert(resumed.state.planets.front().productionQueue.empty() && resumed.state.fleets.size() == 3 && completed(resumed) == 1);
    assert(resumed.state.planets.front().minerals.ironium == 0); // The preserved work is charged materials exactly once.
    assert(TurnProcessor{}.process(resumed.state, {}).fleets.size() == 3);
}
void legacy_files_pay_their_first_bill_without_free_kits()
{
    for (const auto* name : {"host", "player"}) {
        SaveGameData old; QString error; QTemporaryDir dir;
        assert(read_save_game_file(QStringLiteral(SUNS_SOURCE_DIR "/tests/fixtures/native-support-v120-") + name + ".suns", old, error));
        const auto& native = old.state.planets.front().nativeOrbit;
        assert(native.population == 25'000 && native.reserveUnits == 0 && native.servicedTurn == 0);
        assert(old.state.planets.front().minerals.ironium == 100);
        SaveGameData restored; const auto path = dir.filePath("migrated.suns");
        assert(write_save_game_file(path, old, error) && read_save_game_file(path, restored, error));
        assert(restored.state.planets.front().nativeOrbit == native);
    }
    auto host = legacy_host();
    const auto next = TurnProcessor{}.process(host.state, {});
    const auto& residents = next.planets.front().nativeOrbit;
    assert(residents.population == 28'750 && residents.populationGrown == 3750);
    assert(residents.reserveUnits == 1 && residents.mineralUnitsPaid == 2);
    assert(residents.productionPaid == 4 && residents.productionAvailable == 46);
}
void host_player_and_continuation_keep_the_exact_annual_record()
{
    auto host = legacy_host(); host.state = TurnProcessor{}.process(host.state, {});
    QTemporaryDir dir; QString error;
    for (bool player : {false, true}) {
        const auto data = player ? make_player_turn(host, 1) : host;
        SaveGameData restored; const auto path = dir.filePath("annual.suns");
        assert(write_save_game_file(path, data, error) && read_save_game_file(path, restored, error));
        assert(restored.state.planets.front().nativeOrbit == host.state.planets.front().nativeOrbit);
        assert(native_orbital_support_text(restored.state, restored.state.planets.front()).contains("Life support"));
        if (player) continue;
        const auto actual = TurnProcessor{}.process(restored.state, {});
        const auto control = TurnProcessor{}.process(host.state, {});
        assert(actual.planets.front().nativeOrbit == control.planets.front().nativeOrbit);
        assert(actual.planets.front().minerals.ironium == control.planets.front().minerals.ironium);
        assert(actual.players.front().history.back().population == control.players.front().history.back().population);
    }
    auto hidden = host.state;
    hidden.planets.front().minerals = {}; hidden.planets.front().nativeOrbit.reserveUnits = 0;
    hidden.planets.front().nativeOrbit.service = HabitatService::Failing;
    const auto received = make_player_view(hidden, 1).state;
    const auto visible = make_player_view(host.state, 1).state;
    assert(received.planets.front().nativeOrbit == host.state.planets.front().nativeOrbit);
    assert(native_orbital_support_text(received, received.planets.front())
        == native_orbital_support_text(visible, visible.planets.front()));
}
void malformed_support_records_are_rejected()
{
    auto host = legacy_host(); host.state = TurnProcessor{}.process(host.state, {});
    QTemporaryDir dir; const auto path = dir.filePath("bad.suns"); QString error;
    for (unsigned which = 0; which < 7; ++which) {
        auto bad = host; auto& data = bad.state.planets.front().nativeOrbit;
        switch (which) {
        case 0: data.reserveUnits = kNativeSupportMaxReserveUnits + 1; break;
        case 1: data.service = HabitatService(255); break;
        case 2: data.servicedTurn = bad.state.turn + 1; break;
        case 3: data.growthAppliedTurn = data.servicedTurn + 1; break;
        case 4: data.mineralUnitsPaid = 2 * kNativeSupportMaxAnnualUnits + 1; break;
        case 5: data.productionPaid = 10'000; break;
        case 6: data.service = HabitatService::Reserve; break;
        }
        SaveGameData loaded;
        assert(write_save_game_file(path, bad, error));
        assert(!read_save_game_file(path, loaded, error));
    }
}
void shortages_remain_hidden_until_the_dated_report_arrives()
{
    auto host = legacy_host(); auto& initial = host.state;
    auto& remote = initial.planets[1];
    const auto target = remote.id;
    const auto starId = remote.star;
    initial.stars[1].position = {400, 0}; initial.stars[1].stellarClass = StarClass::Yellow;
    remote.owner = 1; remote.population = 0; remote.industry = 0; remote.mines = 0;
    remote.habitat = {}; remote.engineering = {}; remote.productionQueue.clear();
    remote.radar = PlanetaryRadarLevel::Full; remote.minerals = {100, 100, 100};
    auto station = initial.orbitalStations.front(); station.id = initial.nextOrbitalStationId++; station.planet = target;
    remote.nativeOrbit = {station.id, 25'000}; initial.orbitalStations.push_back(station);
    host.state = TurnProcessor{}.process(initial, {});
    set_survey_level(host.state, 1, starId, SurveyLevel::DeepSurvey, host.state.turn); // Prepared received baseline.
    const auto baseline = make_player_view(host.state, 1).state;
    const auto known = find_planet(baseline, target)->nativeOrbit;
    auto physical = std::find_if(host.state.planets.begin(), host.state.planets.end(), [=](const auto& p) { return p.id == target; });
    assert(physical != host.state.planets.end());
    physical->minerals = {}; physical->nativeOrbit.reserveUnits = 0;
    const auto failure = TurnProcessor{}.process_with_events(host.state, {}); host.state = failure.state;
    const auto observed = host.state.turn;
    const auto* owner = find_player(host.state, 1);
    const auto report = std::find_if(owner->pendingPlayerReports.begin(), owner->pendingPlayerReports.end(), [&](const auto& r) {
        return r.planet == target && r.kind == PlayerReportKind::NativeOrbitalSupportFailure && r.observedTurn == observed;
    });
    assert(report != owner->pendingPlayerReports.end() && report->deliveryTurn > observed);
    const auto delivery = report->deliveryTurn, loss = report->deliveredColonists;
    assert(loss > 0 && find_planet(make_player_view(host.state, 1).state, target)->nativeOrbit == known);
    QTemporaryDir dir; QString error; SaveGameData restored;
    const auto path = dir.filePath("inflight.suns");
    assert(write_save_game_file(path, host, error) && read_save_game_file(path, restored, error)); host = restored;
    auto player = make_player_turn(host, 1); SaveGameData playerRestored;
    assert(write_save_game_file(path, player, error) && read_save_game_file(path, playerRestored, error));
    assert(find_planet(playerRestored.state, target)->nativeOrbit == known);
    bool received = false;
    while (host.state.turn < delivery) {
        const auto turn = TurnProcessor{}.process_with_events(host.state, {}); host.state = turn.state;
        for (const auto& event : turn.events) if (event.kind == GameEventKind::NativeOrbitalSupportFailure
            && event.planet == target && event.observedTurn == observed) {
            assert(event.turn == delivery && event.deliveredColonists == loss); received = true;
            host.strategicMessages.push_back(event);
        }
        if (host.state.turn < delivery)
            assert(find_planet(make_player_view(host.state, 1).state, target)->nativeOrbit.servicedTurn < observed);
    }
    assert(received);
    assert(find_planet(make_player_view(host.state, 1).state, target)->nativeOrbit.servicedTurn >= observed);
    assert(write_save_game_file(path, host, error) && read_save_game_file(path, restored, error));
    assert(restored.strategicMessages.back().deliveredColonists == loss);
}
}
int main(int argc, char** argv)
{
    if (argc == 2 && QString::fromLatin1(argv[1]) == "review-reserve-production") {
        paid_manufacture_waits_for_resupply_after_host_reload(); return 0;
    }
    paid_manufacture_waits_for_resupply_after_host_reload();
    legacy_files_pay_their_first_bill_without_free_kits();
    host_player_and_continuation_keep_the_exact_annual_record();
    malformed_support_records_are_rejected();
    shortages_remain_hidden_until_the_dated_report_arrives();
}
