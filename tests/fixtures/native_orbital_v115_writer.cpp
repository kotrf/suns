// Fixture provenance: original 1c34a36029257cffff34406ce5def8ce799c2a95
// headers/core/save_game.cpp (v115), before independent destruction knowledge.
// The pending pre-battle snapshot is deliberately delivered after the loss.
#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/player_knowledge.hpp"
#include <cassert>

int main(int argc, char** argv)
{
    assert(argc == 2);
    suns::SaveGameData save;
    save.campaignId = 115; save.turnToken = 17; save.pendingOrders = {1, {}};
    save.state = suns::test::native_orbital_fixture();
    auto& state = save.state;
    suns::queue_survey_report(state, 1, 1, 0, state.stars.front().position,
        state.turn, suns::SurveyLevel::DeepSurvey);
    state.players.front().pendingSurveyReports.back().deliveryTurn = state.turn+2;
    const auto station = state.orbitalStations.front().id;
    suns::destroy_native_orbital_population(state, station); state.orbitalStations.clear();
    suns::PendingPlayerReport battle; battle.kind = suns::PlayerReportKind::SpaceBattle;
    battle.planet = 1; battle.star = 1; battle.observedTurn = ++state.turn; battle.battle.emplace();
    suns::SpaceBattleUnit unit; unit.owner = 1; unit.station = station; unit.initialShips = 1;
    battle.battle->units.push_back(unit);
    suns::apply_received_station_battle(state.players.front(), battle);
    for (auto& player : state.players) player.history.clear();
    suns::record_empire_turn_statistics(state);
    QString error;
    assert(suns::write_save_game_file(QString::fromLocal8Bit(argv[1]), save, error));
}
