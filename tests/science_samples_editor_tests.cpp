#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/science_samples.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>
#include <iostream>
using namespace suns;
namespace {
GameState fixture()
{
    auto s = make_demo_game(); s.turn = 10; s.wormholeRules.spawnChancePerTurn = 0;
    s.players[0].technology.researchActive = false;
    s.unknownObjects = {{7, {900, 0}, UnknownObjectKind::AncientProbe, true},
        {8, {800, 0}, UnknownObjectKind::CompactRemnant, true}};
    s.nextUnknownObjectId = 9;
    s.shipDesigns[0].hull = ShipHullType::SmallFreighter;
    s.shipDesigns[0].components = {ShipComponentType::FuelMizer, ShipComponentType::MoleScanner};
    s.shipDesigns[0].placements.clear();
    s.fleets.resize(1); auto& f = s.fleets[0]; f.ships = {{f.design, 1}};
    f.destination.reset(); f.minerals = {}; f.colonists = 0;
    f.scienceSamples = {{7, UnknownObjectKind::AncientProbe, false}};
    f.telemetry = {}; f.telemetry.observedTurn = s.turn; f.telemetry.position = f.position;
    f.telemetry.ships = f.ships; f.telemetryInTransit.clear();
    s.planets[0].scienceSamples = {{8, UnknownObjectKind::CompactRemnant, false}};
    s.players[0].objectKnowledge = {{7, {900, 0}, 0, ObjectKnowledgeLevel::Studied, UnknownObjectKind::AncientProbe, 9, 9},
        {8, {800, 0}, 0, ObjectKnowledgeLevel::Studied, UnknownObjectKind::CompactRemnant, 9, 9}};
    return s;
}
QByteArray bytes(const QString& path) { QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll(); }
void writeBytes(const QString& path, const QByteArray& data)
{ QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(data) == data.size()); }
SaveGameData save_fixture()
{
    SaveGameData save; save.state = fixture(); save.pendingOrders = {1, {}};
    save.campaignId = 17; save.turnToken = 29; return save;
}
void persistence_and_migration()
{
    QTemporaryDir dir; assert(dir.isValid()); const auto path = dir.filePath("samples.suns"); QString error;
    auto save = save_fixture(); auto& f = save.state.fleets[0]; f.position = {900, 0};
    FleetTelemetry report = f.telemetry; report.scienceSamples = f.scienceSamples; report.position = f.position;
    f.telemetryInTransit.push_back({15, report});
    assert(write_save_game_file(path, save, error)); SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets[0].scienceSamples == f.scienceSamples);
    assert(loaded.state.fleets[0].telemetry.scienceSamples.empty());
    assert(loaded.state.fleets[0].telemetryInTransit[0].telemetry.scienceSamples == f.scienceSamples);
    assert(loaded.state.planets[0].scienceSamples == save.state.planets[0].scienceSamples);
    assert(loaded.state.unknownObjects[0].sampleRecovered);
    const auto original = bytes(path);
    auto truncated = original; truncated.chop(1); writeBytes(path, truncated);
    assert(!read_save_game_file(path, loaded, error));
    for (int defect = 0; defect < 6; ++defect) {
        auto bad = save;
        if (defect == 0) bad.state.fleets[0].scienceSamples[0].source = 0;
        if (defect == 1) bad.state.fleets[0].scienceSamples[0].retained = true;
        if (defect == 2) bad.state.planets[0].scienceSamples.push_back(bad.state.fleets[0].scienceSamples[0]);
        if (defect == 3) bad.state.unknownObjects[0].sampleRecovered = false;
        if (defect == 4) bad.state.fleets[0].scienceSamples.push_back(bad.state.fleets[0].scienceSamples[0]);
        if (defect == 5) {
            bad.state.mediumObserver = 1;
            bad.state.planets[0].scienceSamples.push_back(bad.state.fleets[0].scienceSamples[0]);
        }
        assert(write_save_game_file(path, bad, error)); assert(!read_save_game_file(path, loaded, error));
    }
    QByteArray magic; { QDataStream out(&magic, QIODevice::WriteOnly); out << quint32{0x53414d50}; }
    auto old = original; const auto at = old.lastIndexOf(magic); assert(at >= 0); old.truncate(at);
    { QDataStream out(&old, QIODevice::ReadWrite); assert(out.device()->seek(4)); out << quint32{85}; }
    writeBytes(path, old); assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets[0].scienceSamples.empty() && loaded.state.planets[0].scienceSamples.empty());
    assert(!loaded.state.unknownObjects[0].sampleRecovered);
}
void exports_and_order_exchange()
{
    QTemporaryDir dir; const auto path = dir.filePath("turn.sunsturn"); QString error;
    auto host = save_fixture(); host.mode = SessionMode::Host;
    host.state.players.push_back({2, "Other"}); host.playerTokens = {{1, 31}, {2, 41}};
    host.state.fleets[0].position = {900, 0}; host.state.fleets[0].telemetry.position = {900, 0};
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); const auto own = bytes(path);
    assert(write_save_game_file(path, make_player_turn(host, 2), error)); const auto other = bytes(path);
    host.state.fleets[0].scienceSamples.clear(); host.state.unknownObjects[0].sampleRecovered = false;
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); assert(bytes(path) == own);
    assert(write_save_game_file(path, make_player_turn(host, 2), error)); assert(bytes(path) == other);
    SaveGameData loaded; assert(read_save_game_file(path, loaded, error));
    for (const auto& p : loaded.state.planets) assert(p.scienceSamples.empty());
    TurnOrderFileData packet{17, 10, 41, {2, {
        TransferScienceSampleOrder{{1, 0}, {0, 1}, 8}, ProcessScienceSampleOrder{1, 8, false},
        MoveFleetOrder{1, {900, 0}, 1, {FleetArrivalActionKind::RecoverSample}}, SetFleetTaskOrder{1, FleetTask::RecoverSample}}},
        {"Transfer", "Analyze", "Recover on arrival", "Recover here"}}, orders;
    const auto orderPath = dir.filePath("samples.sunsorders");
    assert(write_turn_order_file(orderPath, packet, error) && read_turn_order_file(orderPath, orders, error));
    assert(orders.orders.orders.size() == 4 && validate_turn_submission(host, orders).isEmpty());
    assert(std::get<MoveFleetOrder>(orders.orders.orders[2]).arrivalAction.kind == FleetArrivalActionKind::RecoverSample);
    const auto resolved = TurnProcessor{}.process(host.state, {orders.orders});
    assert(resolved.planets[0].scienceSamples == host.state.planets[0].scienceSamples);
}
}
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w, GameState s) {
        w.state_ = std::move(s); w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.rebuildScene();
    }
    static const auto& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
    static const auto& state(const MainWindow& w) { return w.state_; }
    static QString researchSummary(MainWindow& w) { w.refreshResearchPanel(); return w.researchSummary_->text(); }
};
}
namespace {
void editor_authors_orders_without_mutating_physical_inventory(MainWindow& w)
{
    auto local = fixture(); local.fleets[0].telemetry.scienceSamples = local.fleets[0].scienceSamples;
    MainWindowTestAccess::load(w, local);
    auto* tree = w.findChild<QTreeWidget*>("scienceSampleInventory");
    auto* transfer = w.findChild<QPushButton*>("transferScienceSample");
    auto* retain = w.findChild<QPushButton*>("retainScienceSample");
    auto* analyze = w.findChild<QPushButton*>("analyzeScienceSample");
    assert(tree && transfer && retain && analyze && tree->topLevelItemCount() == 2);
    const auto select = [&](UnknownObjectId id) {
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toUInt() == id) {
                tree->setCurrentItem(tree->topLevelItem(i)); return;
            }
        assert(false);
    };
    select(7); assert(transfer->isEnabled() && !analyze->isEnabled()); transfer->click();
    assert(std::holds_alternative<TransferScienceSampleOrder>(MainWindowTestAccess::orders(w).front()));
    assert(MainWindowTestAccess::state(w).fleets[0].scienceSamples.size() == 1);
    select(7); assert(retain->isEnabled()); retain->click();
    assert(std::get<ProcessScienceSampleOrder>(MainWindowTestAccess::orders(w).back()).retain);
    assert(MainWindowTestAccess::state(w).planets[0].scienceSamples.size() == 1);
    select(8); assert(analyze->isEnabled());
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); assert(dialog);
        dialog->button(QMessageBox::Yes)->click();
    });
    analyze->click();
    assert(!std::get<ProcessScienceSampleOrder>(MainWindowTestAccess::orders(w).back()).retain);
    assert(MainWindowTestAccess::state(w).planets[0].scienceSamples.size() == 1);
    auto hidden = fixture(); hidden.fleets[0].position = {900, 0}; hidden.fleets[0].telemetry.position = {900, 0};
    MainWindowTestAccess::load(w, hidden); assert(tree->topLevelItemCount() == 1);
    auto* contacts = w.findChild<QTreeWidget*>("unknownObjectContacts");
    contacts->setCurrentItem(contacts->topLevelItem(0));
    auto* recover = w.findChild<QPushButton*>("recoverScienceSample"); assert(recover->isEnabled());
    assert(w.queueUnknownObjectRecovery(7));
    assert(std::get<MoveFleetOrder>(MainWindowTestAccess::orders(w).back()).arrivalAction.kind == FleetArrivalActionKind::RecoverSample);
    auto boosted = fixture(); boosted.planets[0].scienceSamples[0].retained = true;
    boosted.players[0].technology.researchActive = true;
    boosted.players[0].technology.focus = ResearchField::Energy;
    boosted.players[0].technology.researchAllocationPercent = 50;
    MainWindowTestAccess::load(w, boosted); w.installResearch();
    assert(MainWindowTestAccess::researchSummary(w).contains("includes 4 from retained artifacts"));
}
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    persistence_and_migration(); exports_and_order_exchange();
    MainWindow window; window.installUnknownObjects(); window.installScienceSamples();
    editor_authors_orders_without_mutating_physical_inventory(window);
    std::cout << "Science samples UI, saves, migrations and player exchange passed\n";
}
