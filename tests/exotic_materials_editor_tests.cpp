#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/production.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cassert>
#include <iostream>
using namespace suns;
namespace {
GameState fixture()
{
    auto s = make_demo_game(); s.turn = 10; s.wormholeRules.spawnChancePerTurn = 0;
    s.players[0].technology.levels.fill(16); s.players[0].technology.researchActive = false;
    s.planets[0].industry = 1000; s.planets[0].minerals = {10000, 10000, 10000}; s.planets[0].productionQueue.clear();
    s.orbitalStations[0].hull = OrbitalStationHullType::Starbase;
    s.shipDesigns[0].hull = ShipHullType::SmallFreighter;
    s.shipDesigns[0].components = {ShipComponentType::FuelMizer, ShipComponentType::MoleScanner}; s.shipDesigns[0].placements.clear();
    s.fleets.resize(1); auto& f = s.fleets[0]; f.ships = {{f.design, 1}}; f.colonists = 0; f.minerals = {}; f.destination.reset();
    f.xenocrystal = 8; f.telemetry = {}; f.telemetryInTransit.clear(); f.telemetry = confirmed_fleet_telemetry(s, f);
    s.nextFleetId = 2; set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    s.planets[1].xenocrystal = {24, 20, 4, 1}; set_survey_level(s, 1, s.planets[1].star, SurveyLevel::DeepSurvey, s.turn);
    return s;
}
SaveGameData save_fixture()
{ SaveGameData save; save.state = fixture(); save.pendingOrders = {1, {}}; save.campaignId = 17; save.turnToken = 29; return save; }
QByteArray bytes(const QString& path) { QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll(); }
void writeBytes(const QString& path, const QByteArray& bytes)
{ QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(bytes) == bytes.size()); }
void persistence_and_migration()
{
    QTemporaryDir dir; const auto path = dir.filePath("exotics.suns"); QString error; SaveGameData loaded;
    auto save = save_fixture(); auto& s = save.state; auto& f = s.fleets[0];
    f.position = {900, 0}; f.telemetry.xenocrystal = 4;
    auto packet = f.telemetry; packet.xenocrystal = 7; f.telemetryInTransit.push_back({15, packet});
    s.planets[0].xenocrystal.surface = 8; assert(apply_resonance_order(s, s.planets[0], {1}));
    (void)resolve_colony_production(s, s.planets[0], true);
    assert(apply_resonance_order(s, s.planets[0], {1, ResonanceAction::Pause}));
    assert(write_save_game_file(path, save, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets[0].xenocrystal == 8 && loaded.state.fleets[0].telemetry.xenocrystal == 4);
    assert(loaded.state.fleets[0].telemetryInTransit[0].telemetry.xenocrystal == 7);
    assert(loaded.state.planets[0].resonanceLab.work == 60 && loaded.state.planets[0].resonanceLab.crystalsPaid == 8);
    assert(!loaded.state.planets[0].resonanceLab.active && known_xenocrystal(loaded.state, 1, 2)->site.remaining == 20);
    const auto original = bytes(path); auto truncated = original; truncated.chop(1); writeBytes(path, truncated);
    assert(!read_save_game_file(path, loaded, error));
    for (int defect = 0; defect < 6; ++defect) {
        auto bad = save;
        if (defect == 0) bad.state.planets[1].xenocrystal.remaining = 25;
        if (defect == 1) bad.state.fleets[0].xenocrystal = kMaximumXenocrystal + 1;
        if (defect == 2) bad.state.planets[0].resonanceLab.crystalsPaid = 0;
        if (defect == 3) bad.state.fleets[0].xenocrystal = 100;
        if (defect == 4) bad.state.players[0].surveyKnowledge[0].planetSnapshot->xenocrystal->observedTurn = s.turn + 1;
        if (defect == 5) bad.state.planets[0].resonanceLab.invested.ironium = 0;
        assert(write_save_game_file(path, bad, error)); assert(!read_save_game_file(path, loaded, error));
    }
    auto old = original; QByteArray magic;
    { QDataStream out(&magic, QIODevice::WriteOnly); out << quint32{0x45584f54}; }
    const auto extension = old.lastIndexOf(magic); assert(extension >= 0); old.truncate(extension);
    { QDataStream out(&old, QIODevice::ReadWrite); assert(out.device()->seek(4)); out << quint32{86}; }
    writeBytes(path, old); assert(read_save_game_file(path, loaded, error));
    assert(!loaded.state.fleets[0].xenocrystal && !loaded.state.planets[1].xenocrystal.original && !loaded.state.planets[0].resonanceLab.started);
}
void hidden_exports_and_order_exchange()
{
    QTemporaryDir dir; const auto path = dir.filePath("player.sunsturn"); QString error;
    auto host = save_fixture(); host.mode = SessionMode::Host; host.playerTokens = {{1, 31}, {2, 41}}; host.state.players.push_back({2, "Other"});
    host.state.fleets[0].position = {900, 0};
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); const auto own = bytes(path);
    assert(write_save_game_file(path, make_player_turn(host, 2), error)); const auto other = bytes(path);
    host.state.fleets[0].xenocrystal = 1; host.state.planets[1].xenocrystal = {24, 0, 0, 0}; host.state.planets[0].xenocrystal.surface = 99;
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); assert(bytes(path) == own);
    assert(write_save_game_file(path, make_player_turn(host, 2), error)); assert(bytes(path) == other);
    TurnOrderFileData file{17, 10, 41, {2, {TransferXenocrystalOrder{{0, 1}, {1, 0}, 1}, SetResonanceLaboratoryOrder{1},
        SetResonanceLaboratoryOrder{1, ResonanceAction::Pause}, MoveFleetOrder{1, {900, 0}, 1,
        {FleetArrivalActionKind::LoadAllAvailable, 1, FleetCargoKind::Xenocrystal}}}}, {"Transfer", "Start", "Pause", "Load"}}, loaded;
    const auto orders = dir.filePath("exotics.sunsorders");
    assert(write_turn_order_file(orders, file, error) && read_turn_order_file(orders, loaded, error));
    assert(loaded.orders.orders.size() == 4 && validate_turn_submission(host, loaded).isEmpty());
    assert(std::get<MoveFleetOrder>(loaded.orders.orders[3]).arrivalAction.cargo == FleetCargoKind::Xenocrystal);
    const auto resolved = TurnProcessor{}.process(host.state, {loaded.orders});
    assert(!resolved.planets[0].resonanceLab.started && resolved.fleets[0].xenocrystal == 1);
}
}
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w, GameState s, std::vector<Order> orders = {}) {
        w.state_ = std::move(s); w.pendingOrders_ = {1, std::move(orders)}; w.pendingDescriptions_.clear(); w.selection_.fleet = 1; w.rebuildScene();
    }
    static const auto& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
    static const auto& state(const MainWindow& w) { return w.state_; }
};
}
namespace {
void select_planet(MainWindow& w, PlanetId id)
{
    auto* tree = w.findChild<QTreeWidget*>("exoticInventory");
    for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->data(0, Qt::UserRole).toULongLong() == (qulonglong(id) << 32)) {
        tree->setCurrentItem(tree->topLevelItem(i)); return;
    }
    assert(false);
}
void ui_commands_and_previews(MainWindow& w)
{
    auto s = fixture(); MainWindowTestAccess::load(w, s);
    auto* from = w.findChild<QComboBox*>("exoticTransferSource"); auto* to = w.findChild<QComboBox*>("exoticTransferDestination");
    auto* amount = w.findChild<QSpinBox*>("exoticTransferAmount"); auto* transfer = w.findChild<QPushButton*>("transferXenocrystal");
    assert(from && to && amount && transfer);
    from->setCurrentIndex(from->findData(QVariant::fromValue(qulonglong(1))));
    to->setCurrentIndex(to->findData(QVariant::fromValue(qulonglong(1) << 32))); amount->setValue(8);
    assert(transfer->isEnabled()); transfer->click();
    assert(std::holds_alternative<TransferXenocrystalOrder>(MainWindowTestAccess::orders(w).back()));
    assert(MainWindowTestAccess::state(w).fleets[0].xenocrystal == 8 && !MainWindowTestAccess::state(w).planets[0].xenocrystal.surface);
    select_planet(w, 1); auto* start = w.findChild<QPushButton*>("startResonanceLab"); assert(start->isEnabled()); start->click();
    auto* details = w.findChild<QLabel*>("exoticProjectDetails"); assert(details->text().contains("Forecast completion: year"));
    assert(!MainWindowTestAccess::state(w).planets[0].resonanceLab.started);
    s.fleets[0].position = {900, 0}; s.fleets[0].telemetry.observedTurn = s.turn - 1; s.planets[0].xenocrystal.surface = 99;
    MainWindowTestAccess::load(w, s); select_planet(w, 1);
    from->setCurrentIndex(from->findData(QVariant::fromValue(qulonglong(1))));
    to->setCurrentIndex(to->findData(QVariant::fromValue(qulonglong(1) << 32))); amount->setValue(8); assert(!transfer->isEnabled());
    MainWindowTestAccess::load(w, s, {TransferXenocrystalOrder{{0, 1}, {1, 0}, 8}, SetResonanceLaboratoryOrder{1}});
    select_planet(w, 1); assert(details->text().contains("Awaiting 8 physically delivered xenocrystals"));
    assert(!details->text().contains("Forecast completion: year"));
    MainWindowTestAccess::load(w, fixture()); select_planet(w, 2);
    auto* survey = w.findChild<QPushButton*>("surveyExoticSite"); assert(survey->isEnabled()); survey->click();
    assert(std::get<MoveFleetOrder>(MainWindowTestAccess::orders(w).back()).arrivalAction.kind == FleetArrivalActionKind::None);
    MainWindowTestAccess::load(w, fixture()); select_planet(w, 2);
    auto* haul = w.findChild<QPushButton*>("haulExoticStock"); assert(haul->isEnabled()); haul->click();
    const auto& route = std::get<MoveFleetOrder>(MainWindowTestAccess::orders(w).back());
    assert(route.arrivalAction.kind == FleetArrivalActionKind::LoadAllAvailable && route.arrivalAction.cargo == FleetCargoKind::Xenocrystal);
    assert(route.queuedWaypoints.size() == 1 && route.queuedWaypoints[0].arrivalAction.cargo == FleetCargoKind::Xenocrystal);
}
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv); persistence_and_migration(); hidden_exports_and_order_exchange();
    MainWindow w; w.installExoticMaterials(); ui_commands_and_previews(w);
    std::cout << "Exotic material UI, frozen exports, saves and PBEM passed\n";
}
