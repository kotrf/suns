#include "save_game.hpp"
#include "suns/campaign.hpp"

#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <algorithm>

using namespace suns;

static QByteArray bytes(const QString& path)
{
    QFile file(path); assert(file.open(QIODevice::ReadOnly)); return file.readAll();
}

static void put(const QString& path, const QByteArray& data)
{
    QFile file(path); assert(file.open(QIODevice::WriteOnly)); assert(file.write(data) == data.size());
}

static void patch32(QByteArray& data, qsizetype offset, quint32 value)
{
    QDataStream out(&data, QIODevice::ReadWrite);
    assert(out.device()->seek(offset)); out << value;
}

int main()
{
    QTemporaryDir directory; assert(directory.isValid());
    const auto path = directory.filePath("workforce.suns");
    QString error;
    SaveGameData original;
    original.campaignId = 1; original.turnToken = 2;
    original.state = make_demo_game();
    original.state.planets.front().population = 25'000;
    original.state.planets.front().industry = 100;
    original.state.planets.front().mines = 80;
    original.pendingOrders = {1, {}};
    assert(write_save_game_file(path, original, error));
    auto current = bytes(path);
    { QDataStream in(current); quint32 magic{}, version{}; in >> magic >> version; assert(version == 108); }
    const auto extension = current.size() - 14; // marker/count + one fixed-size racial record
    { QDataStream in(current.mid(extension)); quint32 magic{}, count{}; in >> magic >> count; assert(magic == 0x5746524b && count == 1); }

    // The pre-extension bytes are checked against a real v107 writer, built
    // before this change. This is a genuine migration layout, not a renamed v108.
    auto legacy = current.left(extension);
    patch32(legacy, 4, 107);
    assert(QCryptographicHash::hash(legacy, QCryptographicHash::Sha256).toHex()
        == "1942413ca3b9c64524b9cb18e9a5ec845605ef2572d2bd56d63d8a4adb997dc8");
    put(path, legacy);
    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    const auto& race = loaded.state.players.front().race;
    assert(race.factoriesPerTenThousand == 10 && race.minesPerTenThousand == 10);
    assert(loaded.state.planets.front().industry == 100 && loaded.state.planets.front().mines == 80);
    assert(colony_workforce(loaded.state, loaded.state.planets.front()).operatingFactories == 25);
    assert(colony_workforce(loaded.state, loaded.state.planets.front()).operatingMines == 25);
    const auto continuation = TurnProcessor{}.process(loaded.state, {});
    const auto control = TurnProcessor{}.process(original.state, {});
    assert(continuation.planets.front().population == control.planets.front().population);
    assert(continuation.planets.front().minerals.ironium == control.planets.front().minerals.ironium);
    assert(continuation.players.front().history.back().productionOutput == control.players.front().history.back().productionOutput);

    EmpireSetup first{"First", RacePreset::Terran}, second{"Second", RacePreset::Cryophile};
    first.factoriesPerTenThousand = 5; first.minesPerTenThousand = 25;
    second.factoriesPerTenThousand = 25; second.minesPerTenThousand = 5;
    auto host = original;
    host.state = generate_campaign(host.galaxyConfig, {first, second});
    host.mode = SessionMode::Host;
    host.playerTokens = {{1, 11}, {2, 22}}; host.turnToken = 11;
    assert(write_save_game_file(path, host, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.players[0].race.factoriesPerTenThousand == 5 && loaded.state.players[0].race.minesPerTenThousand == 25);
    assert(loaded.state.players[1].race.factoriesPerTenThousand == 25 && loaded.state.players[1].race.minesPerTenThousand == 5);
    auto exported = make_player_turn(host, 2);
    assert(write_save_game_file(path, exported, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.players.size() == 1 && loaded.state.players.front().id == 2);
    assert(loaded.state.players.front().race.factoriesPerTenThousand == 25);
    assert(loaded.state.players.front().race.minesPerTenThousand == 5);

    auto publicTurn = make_player_turn(host, 1);
    assert(write_save_game_file(path, publicTurn, error)); const auto before = bytes(path);
    host.state.players[1].race.factoriesPerTenThousand = 7;
    host.state.players[1].race.minesPerTenThousand = 23;
    assert(write_save_game_file(path, make_player_turn(host, 1), error));
    assert(bytes(path) == before); // Opponents' hidden economic settings stay private.

    TurnOrderFileData packet;
    packet.campaignId = host.campaignId; packet.turn = host.state.turn; packet.turnToken = 22;
    const auto colony = std::find_if(host.state.planets.begin(), host.state.planets.end(), [](const auto& p) { return p.owner == 2; });
    assert(colony != host.state.planets.end());
    packet.orders = {2, {QueueProductionOrder{colony->id, ProductionKind::Factory}}};
    packet.descriptions = {"Build factory"};
    assert(write_turn_order_file(path + ".orders", packet, error));
    TurnOrderFileData parsed;
    assert(read_turn_order_file(path + ".orders", parsed, error));
    assert(validate_turn_submission(host, parsed).isEmpty()); // Ordinary order format remains v47.

    assert(write_save_game_file(path, host, error));
    const auto valid = bytes(path);
    const auto offset = valid.size() - 20; // two racial records
    for (int which = 0; which < 9; ++which) {
        auto corrupt = valid;
        switch (which) {
        case 0: corrupt.chop(1); break;
        case 1: corrupt.chop(20); break;
        case 2: patch32(corrupt, offset, 0); break;
        case 3: patch32(corrupt, offset + 4, 1); break;
        case 4: patch32(corrupt, offset + 4, 0xffffffffu); break;
        case 5: patch32(corrupt, offset + 8, 77); break;
        case 6: corrupt[offset + 12] = char(4); break;
        case 7: corrupt[offset + 13] = char(26); break;
        case 8: patch32(corrupt, offset + 14, 1); break;
        }
        put(path, corrupt);
        assert(!read_save_game_file(path, loaded, error));
    }
    for (int invalid : {0, 4, 26, 255}) {
        auto bad = host;
        bad.state.players.front().race.factoriesPerTenThousand = std::uint8_t(invalid);
        assert(!write_save_game_file(path, bad, error));
        bad = host;
        bad.state.players.front().race.minesPerTenThousand = std::uint8_t(invalid);
        assert(!write_save_game_file(path, bad, error));
    }
}
