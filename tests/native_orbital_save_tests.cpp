#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/orbital_habitats.hpp"
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cassert>
using namespace suns;
namespace {
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
    const auto next = TurnProcessor{}.process(legacy.state, {});
    const auto expected = legacy.state.planets.front().population
        + projected_population_growth(legacy.state, legacy.state.planets.front(), legacy.state.turn);
    assert(next.planets.front().population == expected && next.planets.front().owner == 1);
}
}
int main()
{
    resident_truth_and_pending_evidence_round_trip();
    hidden_loss_is_frozen_in_export_and_delivered_loss_survives_reload();
    real_pbem_decoding_keeps_guards_and_shared_unloading();
    genuine_v114_has_no_native_residents();
}
