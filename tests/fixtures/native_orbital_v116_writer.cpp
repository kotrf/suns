// Genuine v116 fixture: built with headers/core/save_game.cpp from 6e448e9,
// before adding History observation-date metadata. Only the test scenario is new.
#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include <cassert>
int main(int argc, char** argv)
{
    assert(argc == 2);
    suns::SaveGameData save;
    save.campaignId = 116; save.turnToken = 17; save.pendingOrders = {1, {}};
    save.state = suns::test::asynchronous_native_unload_fixture();
    save.state = suns::TurnProcessor{}.process(save.state, {});
    save.state = suns::TurnProcessor{}.process(save.state, {});
    assert(save.state.turn == 3 && save.state.fleets.front().colonists == 0);
    QString error;
    assert(suns::write_save_game_file(QString::fromLocal8Bit(argv[1]), save, error));
}
