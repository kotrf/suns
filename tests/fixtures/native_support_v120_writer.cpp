// Genuine synthetic v120 files from the unmodified writer and headers at
// 99b47dedb7fe6beb107d4c8507095aabecb35d2c. Rebuild against that revision,
// never the current writer. No life-support kits or history are prepaid here.
#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include <QDir>
#include <cstdio>
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const QDir directory(QString::fromLocal8Bit(argv[1]));
    suns::SaveGameData host;
    host.state = suns::test::empty_native_orbital_start();
    host.state.stars.front().stellarClass = suns::StarClass::Yellow;
    if (!suns::initialize_native_orbital_start(host.state, 1, host.state.planets.front().id,
            suns::native_orbital_start_budget())) return 3;
    host.campaignId = 121; host.turnToken = 17; host.playerTokens = {{1, 17}};
    host.mode = suns::SessionMode::Host; host.pendingOrders = {1, {}};
    QString error;
    if (!suns::write_save_game_file(directory.filePath("native-support-v120-host.suns"), host, error)
        || !suns::write_save_game_file(directory.filePath("native-support-v120-player.suns"), suns::make_player_turn(host, 1), error)) {
        std::fprintf(stderr, "%s\n", qPrintable(error)); return 4;
    }
}
