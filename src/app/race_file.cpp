#include "race_file.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <array>
#include <cmath>
#include <optional>

namespace suns {
namespace {
constexpr qint64 kMaximumRaceFileSize = 64 * 1024;
constexpr std::array environments{
    std::pair{RacePreset::Terran, "terran"},
    std::pair{RacePreset::Cryophile, "cryophile"},
    std::pair{RacePreset::Radiotroph, "radiotroph"}};
constexpr std::array traits{
    std::pair{PrimaryRaceTrait::Generalist, "generalist"},
    std::pair{PrimaryRaceTrait::StargateSpecialist, "stargate-specialist"}};
constexpr std::array hulls{
    std::pair{HullAccess::Standard, "standard"},
    std::pair{HullAccess::InnerStrength, "inner-strength"},
    std::pair{HullAccess::SuperStealth, "super-stealth"},
    std::pair{HullAccess::WarMonger, "war-monger"},
    std::pair{HullAccess::SpaceDemolition, "space-demolition"},
    std::pair{HullAccess::HyperExpansion, "hyper-expansion"}};

template<class T, std::size_t N>
const char* keyFor(T value, const std::array<std::pair<T, const char*>, N>& entries)
{
    for (const auto& [candidate, key] : entries) if (candidate == value) return key;
    return nullptr;
}

template<class T, std::size_t N>
std::optional<T> valueFor(const QJsonValue& value, const std::array<std::pair<T, const char*>, N>& entries)
{
    if (!value.isString()) return std::nullopt;
    for (const auto& [candidate, key] : entries) if (value.toString() == key) return candidate;
    return std::nullopt;
}
} // namespace

QString race_setup_error(const EmpireSetup& setup)
{
    const auto name = QString::fromUtf8(setup.name.data(), static_cast<int>(setup.name.size()));
    if (setup.name.empty() || setup.name.size() > 80 || name.trimmed().isEmpty()
        || name.toUtf8().toStdString() != setup.name)
        return "Empire name must contain 1–80 UTF-8 bytes.";
    for (const auto character : name) if (character.unicode() < 32 || character.unicode() == 127)
        return "Empire name must be a single line without control characters.";
    if (!keyFor(setup.race, environments)) return "Unknown environment preset.";
    if (!keyFor(setup.primaryTrait, traits)) return "This primary trait is not playable.";
    if (!keyFor(setup.hullAccess, hulls)) return "Unknown hull family.";
    if (setup.advancedRemoteMining && setup.basicRemoteMining)
        return "Advanced and Basic remote mining cannot both be selected.";
    if (setup.factoriesPerTenThousand < kMinWorkforceRate || setup.factoriesPerTenThousand > kMaxWorkforceRate
        || setup.minesPerTenThousand < kMinWorkforceRate || setup.minesPerTenThousand > kMaxWorkforceRate)
        return "Factory and mine operation rates must each be between 5 and 25 per 10,000 colonists.";
    return {};
}

bool write_race_file(const QString& path, const EmpireSetup& setup, QString& error)
{
    error = race_setup_error(setup);
    if (!error.isEmpty()) return false;
    const QJsonObject object{
        {"format", "suns-race"}, {"version", 1},
        {"name", QString::fromStdString(setup.name).trimmed()},
        {"environment", keyFor(setup.race, environments)},
        {"primary_trait", keyFor(setup.primaryTrait, traits)},
        {"fuel_efficiency", setup.improvedFuelEfficiency},
        {"no_ram_scoops", setup.noRamScoopEngines},
        {"settler_engines", setup.settlerEngineAccess},
        {"hull_family", keyFor(setup.hullAccess, hulls)},
        {"remote_mining", setup.advancedRemoteMining ? "advanced" : setup.basicRemoteMining ? "basic" : "standard"},
        {"factories_per_10000", setup.factoriesPerTenThousand},
        {"mines_per_10000", setup.minesPerTenThousand}};
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
    if (file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
    return true;
}

bool read_race_file(const QString& path, EmpireSetup& setup, QString& error)
{
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
    const auto bytes = file.read(kMaximumRaceFileSize + 1);
    if (file.error() != QFileDevice::NoError) { error = file.errorString(); return false; }
    if (bytes.size() > kMaximumRaceFileSize) { error = "Race file exceeds 64 KiB."; return false; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = "Not a valid Suns! race file.";
        return false;
    }
    const auto object = document.object();
    if (object.value("format") != QJsonValue("suns-race") || object.value("version") != QJsonValue(1)) {
        error = "Unsupported race file format or version.";
        return false;
    }
    const QStringList keys{"format", "version", "name", "environment", "primary_trait", "fuel_efficiency",
        "no_ram_scoops", "settler_engines", "hull_family", "remote_mining", "factories_per_10000", "mines_per_10000"};
    if (object.size() != keys.size()) { error = "Race file has missing or unsupported fields."; return false; }
    for (const auto& key : keys) if (!object.contains(key)) {
        error = "Race file has missing or unsupported fields.";
        return false;
    }
    const auto environment = valueFor(object.value("environment"), environments);
    const auto trait = valueFor(object.value("primary_trait"), traits);
    const auto hull = valueFor(object.value("hull_family"), hulls);
    const auto mining = object.value("remote_mining");
    if (!object.value("name").isString() || !environment || !trait || !hull || !mining.isString()
        || (mining.toString() != "standard" && mining.toString() != "advanced" && mining.toString() != "basic")) {
        error = "Race file contains an invalid name, preset, trait, hull family or mining choice.";
        return false;
    }
    for (const auto* key : {"fuel_efficiency", "no_ram_scoops", "settler_engines"})
        if (!object.value(key).isBool()) { error = QString("%1 must be true or false.").arg(key); return false; }
    for (const auto* key : {"factories_per_10000", "mines_per_10000"}) {
        const auto value = object.value(key);
        const auto number = value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number
            || number < kMinWorkforceRate || number > kMaxWorkforceRate) {
            error = QString("%1 must be a whole number between 5 and 25.").arg(key);
            return false;
        }
    }
    EmpireSetup loaded;
    loaded.name = object.value("name").toString().trimmed().toUtf8().toStdString();
    loaded.race = *environment;
    loaded.primaryTrait = *trait;
    loaded.hullAccess = *hull;
    loaded.improvedFuelEfficiency = object.value("fuel_efficiency").toBool();
    loaded.noRamScoopEngines = object.value("no_ram_scoops").toBool();
    loaded.settlerEngineAccess = object.value("settler_engines").toBool();
    loaded.advancedRemoteMining = mining.toString() == "advanced";
    loaded.basicRemoteMining = mining.toString() == "basic";
    loaded.factoriesPerTenThousand = static_cast<std::uint8_t>(object.value("factories_per_10000").toInt());
    loaded.minesPerTenThousand = static_cast<std::uint8_t>(object.value("mines_per_10000").toInt());
    error = race_setup_error(loaded);
    if (!error.isEmpty()) return false;
    setup = std::move(loaded);
    return true;
}
} // namespace suns
