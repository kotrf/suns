#include "race_file.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cassert>
#include <vector>

using namespace suns;

static QByteArray bytesAt(const QString& path)
{
    QFile file(path); assert(file.open(QIODevice::ReadOnly)); return file.readAll();
}
static void put(const QString& path, const QByteArray& bytes)
{
    QFile file(path); assert(file.open(QIODevice::WriteOnly)); assert(file.write(bytes) == bytes.size());
}
static void sameRace(const EmpireSetup& a, const EmpireSetup& b)
{
    assert(a.name == b.name && a.race == b.race && a.primaryTrait == b.primaryTrait);
    assert(a.hullAccess == b.hullAccess && a.improvedFuelEfficiency == b.improvedFuelEfficiency);
    assert(a.noRamScoopEngines == b.noRamScoopEngines && a.settlerEngineAccess == b.settlerEngineAccess);
    assert(a.advancedRemoteMining == b.advancedRemoteMining && a.basicRemoteMining == b.basicRemoteMining);
    assert(a.factoriesPerTenThousand == b.factoriesPerTenThousand && a.minesPerTenThousand == b.minesPerTenThousand);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory; assert(directory.isValid());
    const auto path = directory.filePath("player.sunsrace");
    QString error;
    // Every supported choice survives a portable round trip. The controller is
    // deliberately excluded: the host, not the file, decides who plays a slot.
    for (int preset = 0; preset < 3; ++preset) for (int hull = 0; hull < 6; ++hull)
        for (int trait = 0; trait < 2; ++trait) for (int mining = 0; mining < 3; ++mining) {
            EmpireSetup original{"Игрок Ω"};
            original.race = static_cast<RacePreset>(preset);
            original.hullAccess = static_cast<HullAccess>(hull);
            original.primaryTrait = static_cast<PrimaryRaceTrait>(trait);
            original.improvedFuelEfficiency = hull % 2;
            original.noRamScoopEngines = preset % 2;
            original.settlerEngineAccess = trait;
            original.advancedRemoteMining = mining == 1;
            original.basicRemoteMining = mining == 2;
            original.factoriesPerTenThousand = 5;
            original.minesPerTenThousand = 25;
            original.controller = PlayerController::Computer;
            error = "stale error";
            assert(write_race_file(path, original, error) && error.isEmpty());
            EmpireSetup loaded{"Unchanged"};
            assert(read_race_file(path, loaded, error) && error.isEmpty());
            sameRace(original, loaded);
            assert(loaded.controller == PlayerController::Human);
            const auto json = QJsonDocument::fromJson(bytesAt(path)).object();
            assert(json.size() == 12 && json.value("format").toString() == "suns-race");
            for (const auto* key : {"controller", "seed", "player_id", "token", "technology"}) assert(!json.contains(key));
        }

    EmpireSetup original{"Ready"};
    assert(write_race_file(path, original, error));
    const auto validBytes = bytesAt(path);
    const auto valid = QJsonDocument::fromJson(validBytes).object();
    const auto reject = [&](const QByteArray& bytes) {
        put(path, bytes);
        EmpireSetup untouched{"Sentinel"}; untouched.controller = PlayerController::Computer;
        const auto before = untouched;
        assert(!read_race_file(path, untouched, error) && !error.isEmpty());
        sameRace(before, untouched); assert(untouched.controller == before.controller);
    };
    for (const auto* key : {"format", "version", "name", "environment", "primary_trait", "fuel_efficiency",
            "no_ram_scoops", "settler_engines", "hull_family", "remote_mining", "factories_per_10000", "mines_per_10000"}) {
        auto missing = valid; missing.remove(key); reject(QJsonDocument(missing).toJson());
        auto null = valid; null.insert(key, QJsonValue::Null); reject(QJsonDocument(null).toJson());
    }
    for (const auto& [key, value] : std::vector<std::pair<QString, QJsonValue>>{
            {"format", "other-game"}, {"version", 2}, {"version", 1.5}, {"version", true},
            {"name", 10}, {"name", ""}, {"name", "\n"}, {"name", "A\nB"}, {"name", QString(81, 'a')},
            {"environment", "unlimited"}, {"primary_trait", "habitat-civilization"},
            {"hull_family", -1}, {"remote_mining", "both"}, {"fuel_efficiency", 1},
            {"no_ram_scoops", "false"}, {"settler_engines", 0},
            {"factories_per_10000", 4}, {"factories_per_10000", 26}, {"factories_per_10000", 10.5},
            {"mines_per_10000", "10"}, {"mines_per_10000", true}, {"mines_per_10000", -1}}) {
        auto bad = valid; bad.insert(key, value); reject(QJsonDocument(bad).toJson());
    }
    auto privileged = valid; privileged.insert("starting_technology", 26); reject(QJsonDocument(privileged).toJson());
    for (const auto& bytes : {QByteArray{}, validBytes.left(validBytes.size() / 2), QByteArray("[]"), QByteArray(65537, ' ')}) reject(bytes);

    // Invalid settings and failed writes do not replace a previously valid file.
    put(path, validBytes);
    for (int failure = 0; failure < 7; ++failure) {
        auto invalid = original;
        switch (failure) {
        case 0: invalid.race = static_cast<RacePreset>(90); break;
        case 1: invalid.hullAccess = static_cast<HullAccess>(90); break;
        case 2: invalid.primaryTrait = PrimaryRaceTrait::HabitatCivilization; break;
        case 3: invalid.advancedRemoteMining = invalid.basicRemoteMining = true; break;
        case 4: invalid.factoriesPerTenThousand = 0; break;
        case 5: invalid.name = std::string("A\0B", 3); break;
        case 6: invalid.name = std::string("\xff", 1); break;
        }
        assert(!write_race_file(path, invalid, error));
        assert(bytesAt(path) == validBytes);
    }
    assert(!write_race_file(directory.path(), original, error));
    assert(bytesAt(path) == validBytes);
    EmpireSetup loaded;
    assert(read_race_file(path, loaded, error) && error.isEmpty());
    sameRace(original, loaded);
    assert(!read_race_file(directory.filePath("missing.sunsrace"), loaded, error));
    sameRace(original, loaded);
}
