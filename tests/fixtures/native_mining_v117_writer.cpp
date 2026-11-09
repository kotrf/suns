// Genuine v117 fixture, built with ef71c25 (merged #283) headers/core/save code
// before introducing received native mining manifests. The neutral-site yield
// was recorded physically in the old native ledger without a receipt record.
#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include <cassert>
int main(int argc, char** argv)
{
    assert(argc == 2);
    suns::SaveGameData save; save.campaignId = 117; save.turnToken = 17; save.pendingOrders = {1, {}};
    save.state = suns::test::native_orbital_miner_fixture();
    auto& state = save.state;
    state.stars[1].position = {10, 0}; state.planets[1].minerals = {};
    state.planets[1].observedConcentration = suns::MineralCargo{100, 50, 25};
    state.fleets.front().position = state.stars[1].position;
    state.fleets.front().telemetry.position = state.fleets.front().position;
    state.fleets.front().task = suns::FleetTask::RemoteMining;
    state.fleets.front().telemetry.task = suns::FleetTask::RemoteMining;
    state = suns::TurnProcessor{}.process(state, {});
    assert(state.players.front().history.back().remoteExtraction.ironium == 8);
    QString error; assert(suns::write_save_game_file(QString::fromLocal8Bit(argv[1]), save, error));
}
