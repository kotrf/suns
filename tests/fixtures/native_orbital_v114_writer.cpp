// Fixture provenance: original 52003e121e00cef66710abf16a799213f1870182
// headers/core/save_game.cpp (v114), before native population was added.
// Not a current-build target; never synthesize v114 by patching a v115 header.
#include "save_game.hpp"
#include <cassert>

int main(int argc, char** argv)
{
    assert(argc == 2);
    suns::SaveGameData data;
    data.campaignId = 114; data.turnToken = 17;
    data.state = suns::make_demo_game();
    data.pendingOrders = {1, {}};
    QString error;
    assert(suns::write_save_game_file(QString::fromLocal8Bit(argv[1]), data, error));
}
