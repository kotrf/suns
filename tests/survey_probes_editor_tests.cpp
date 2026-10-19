#include "main_window.hpp"
#include "save_game.hpp"
#include "ship_designer_dialog.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/survey_probes.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDataStream>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cassert>
#include <iostream>
using namespace suns;
namespace {
SaveGameData fixture()
{
    SaveGameData host; host.mode = SessionMode::Host; host.campaignId = 17; host.turnToken = 29; host.playerTokens = {{1, 31}, {2, 41}}; host.pendingOrders = {2, {}};
    auto& s = host.state; s = make_demo_game(); s.turn = 10; s.wormholeRules.spawnChancePerTurn = 0;
    s.players.push_back({2, "Host"}); s.players[0].technology.levels.fill(26); s.players[0].technology.researchActive = false;
    s.planets[0].productionQueue.clear(); s.planets[0].minerals = {10000, 10000, 10000};
    s.shipDesigns.push_back(basic_survey_probe_design(1, 200)); s.nextShipDesignId = 201; s.fleets.resize(1);
    auto& f = s.fleets[0]; f.design = 200; f.ships = {{200, 1}}; f.position = {900, 0}; f.destination.reset();
    f.colonists = 0; f.minerals = {}; f.scienceSamples.clear(); f.xenocrystal = 0; f.fuel = fleet_fuel_capacity(s, f);
    f.telemetry = {}; f.telemetry.observedTurn = s.turn; f.telemetry.position = f.position; f.telemetry.fuel = f.fuel; f.telemetry.ships = f.ships;
    s.stars[1].position = {900, 0}; s.nextFleetId = 2;
    return host;
}
QByteArray bytes(const QString& path) { QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll(); }
void put(const QString& path, const QByteArray& value) { QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(value) == value.size()); }
void saves_notebooks_telemetry_and_pbem()
{
    auto host = fixture(); auto& s = host.state; auto& f = s.fleets[0]; ProbeProgram p; p.center = {900, 0};
    apply_probe_command(s, f, {ProbeAction::Configure, p, {{1, SurveyLevel::DeepSurvey, s.turn}}});
    record_probe_survey(f, 2, SurveyLevel::OrbitalSurvey, s.turn);
    auto report = f.telemetry; report.probe = f.probe; report.probe.surveys.clear(); f.telemetryInTransit.push_back({16, report});
    assert(submit_probe_command(s, 1, {1, ProbeAction::Stop})); assert(f.pendingCommands[0].probe);
    QTemporaryDir dir; const auto path = dir.filePath("probe.suns"), again = dir.filePath("again.suns"); QString error; SaveGameData loaded;
    assert(write_save_game_file(path, host, error) && read_save_game_file(path, loaded, error));
    assert(probe_survey_level(loaded.state.fleets[0], 2) == SurveyLevel::OrbitalSurvey);
    assert(loaded.state.fleets[0].telemetry.probe.surveys.empty() && loaded.state.fleets[0].pendingCommands[0].probe->action == ProbeAction::Stop);
    for (int year = 0; year < 10; ++year) {
        host.state = TurnProcessor{}.process(host.state, {}); loaded.state = TurnProcessor{}.process(loaded.state, {});
        assert(write_save_game_file(path, host, error) && write_save_game_file(again, loaded, error)); assert(bytes(path) == bytes(again));
    }
    const auto design = basic_survey_probe_design(1, 201);
    TurnOrderFileData packet{17, 10, 31, {1, {CreateShipDesignOrder{design.name, design.hull, design.components},
        SetProbeProgramOrder{1, ProbeAction::Configure, p}, SetProbeProgramOrder{1, ProbeAction::Stop},
        SetProbeProgramOrder{1, ProbeAction::Resume}, SetProbeProgramOrder{1, ProbeAction::UpdateKnowledge}}}, {"Design", "Configure", "Stop", "Resume", "Update"}}, imported;
    const auto orders = dir.filePath("probe.sunsorders");
    assert(write_turn_order_file(orders, packet, error) && read_turn_order_file(orders, imported, error));
    assert(validate_turn_submission(fixture(), imported).isEmpty());
    assert(std::get<CreateShipDesignOrder>(imported.orders.orders[0]).hull == ShipHullType::SurveyProbe);
    assert(std::get<SetProbeProgramOrder>(imported.orders.orders[4]).action == ProbeAction::UpdateKnowledge);
    auto foreign = fixture(); foreign.state = TurnProcessor{}.process(foreign.state, {{2, {SetProbeProgramOrder{1, ProbeAction::Configure, p}}}});
    assert(!foreign.state.fleets[0].probe.configured);
}
void immutable_exports_validation_and_v87_migration()
{
    auto host = fixture(); QTemporaryDir dir; const auto path = dir.filePath("player.sunsturn"); QString error; SaveGameData loaded;
    auto other = host.state.fleets[0]; other.id = 2; host.state.fleets.push_back(other); host.state.nextFleetId = 3;
    host.state.unknownObjects = {{7, {1500, 0}, UnknownObjectKind::AncientProbe}}; host.state.nextUnknownObjectId = 8;
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); const auto before = bytes(path);
    auto& s = host.state; auto& f = s.fleets[0]; ProbeProgram p; p.center = {900, 0}; apply_probe_command(s, f, {ProbeAction::Configure, p});
    record_probe_survey(f, 2, SurveyLevel::DeepSurvey, s.turn); f.probe.target = 2; f.probe.status = ProbeStatus::Travelling;
    f.position = {950, 0}; f.fuel = 1; s.planets[1].xenocrystal = {48, 0, 48, 4};
    record_probe_survey(s.fleets[1], 2, SurveyLevel::DeepSurvey, s.turn);
    s.unknownObjects[0].position = {1600, 0}; s.unknownObjects[0].kind = UnknownObjectKind::CompactRemnant; s.unknownObjects[0].sampleRecovered = true;
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); assert(bytes(path) == before);
    for (int defect = 0; defect < 6; ++defect) {
        auto bad = host; auto& mission = bad.state.fleets[0].probe;
        if (defect == 0) mission.program.radius = -1;
        if (defect == 1) mission.surveys.push_back({99999, SurveyLevel::DeepSurvey, s.turn});
        if (defect == 2) mission.startedTurn = s.turn + 1;
        if (defect == 3) mission.surveys.push_back(mission.surveys[0]);
        if (defect == 4) bad.state.fleets[0].telemetry.probe = mission;
        if (defect == 5) mission.completed = {2, 2};
        assert(write_save_game_file(path, bad, error)); assert(!read_save_game_file(path, loaded, error));
    }
    f.telemetry.probe = f.probe; f.telemetry.probe.surveys.clear();
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); assert(bytes(path) != before);
    assert(read_save_game_file(path, loaded, error) && loaded.state.fleets[0].probe.surveys.empty());
    auto legacy = fixture(); legacy.state.shipDesigns.pop_back(); auto& old = legacy.state.fleets[0]; old.design = legacy.state.shipDesigns[0].id;
    old.ships = {{old.design, 1}}; old.fuel = fleet_fuel_capacity(legacy.state, old); old.telemetry = {};
    assert(write_save_game_file(path, legacy, error)); auto data = bytes(path); QByteArray marker;
    { QDataStream out(&marker, QIODevice::WriteOnly); out << quint32{0x50524f42}; }
    const auto extension = data.lastIndexOf(marker); assert(extension > 0); data.truncate(extension);
    { QDataStream out(&data, QIODevice::ReadWrite); assert(out.device()->seek(4)); out << quint32{87}; }
    put(path, data); assert(read_save_game_file(path, loaded, error)); assert(!loaded.state.fleets[0].probe.configured);
}
}
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w, GameState s) { w.state_ = std::move(s); w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear(); w.rebuildScene(); }
    static const auto& orders(MainWindow& w) { return w.pendingOrders_.orders; }
    static const auto& state(MainWindow& w) { return w.state_; }
};
}
namespace {
void ui_and_catalogs(MainWindow& w)
{
    auto host = fixture(); MainWindowTestAccess::load(w, host.state);
    auto* fleets = w.findChild<QComboBox*>("probeFleet"); assert(fleets && fleets->count() == 1);
    w.findChild<QPushButton*>("loadProbeProgram")->click(); assert(w.findChild<QDoubleSpinBox*>("probeCenterX")->value() == 900);
    w.findChild<QPushButton*>("sendProbeProgram")->click();
    assert(std::holds_alternative<SetProbeProgramOrder>(MainWindowTestAccess::orders(w).back()));
    assert(!MainWindowTestAccess::state(w).fleets[0].probe.configured);
    auto* status = w.findChild<QLabel*>("probeMissionStatus"); const auto text = status->text();
    auto hidden = host.state; ProbeProgram p; apply_probe_command(hidden, hidden.fleets[0], {ProbeAction::Configure, p});
    hidden.fleets[0].probe.target = 2; hidden.fleets[0].probe.status = ProbeStatus::Surveying; MainWindowTestAccess::load(w, hidden); assert(status->text() == text);
    hidden.fleets[0].telemetry.probe = hidden.fleets[0].probe; hidden.fleets[0].telemetry.probe.surveys.clear(); MainWindowTestAccess::load(w, hidden);
    w.findChild<QPushButton*>("stopProbeProgram")->click(); assert(std::get<SetProbeProgramOrder>(MainWindowTestAccess::orders(w).back()).action == ProbeAction::Stop);
    assert(MainWindowTestAccess::state(w).fleets[0].probe.enabled);
    ShipDesignerDialog designer(host.state, 1); auto* hulls = designer.findChild<QComboBox*>("shipHullCatalog");
    assert(hulls->findData(int(ShipHullType::SurveyProbe)) >= 0); hulls->setCurrentIndex(hulls->findData(int(ShipHullType::SurveyProbe)));
    auto* fits = designer.findChild<QCheckBox*>("shipComponentFitsHull"); if (fits) fits->setChecked(false);
    const auto* catalog = designer.findChild<QListWidget*>("shipComponentCatalog"); assert(catalog);
    assert(!catalog->findItems("Survey Autopilot", Qt::MatchContains).isEmpty());
    host.state.shipDesigns.pop_back(); host.state.fleets.clear(); MainWindowTestAccess::load(w, host.state);
    w.findChild<QPushButton*>("buildSurveyProbe")->click();
    assert(MainWindowTestAccess::orders(w).size() == 2 && std::holds_alternative<CreateShipDesignOrder>(MainWindowTestAccess::orders(w)[0]));
    const auto& created = std::get<CreateShipDesignOrder>(MainWindowTestAccess::orders(w)[0]);
    assert(created.hull == ShipHullType::SurveyProbe && std::find(created.components.begin(), created.components.end(), ShipComponentType::SurveyAutopilot) != created.components.end());
}
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv); saves_notebooks_telemetry_and_pbem(); immutable_exports_validation_and_v87_migration();
    MainWindow w; w.installSurveyProbes(); ui_and_catalogs(w);
    std::cout << "Probe UI, catalogs, frozen exports, save/load and PBEM passed\n";
}
