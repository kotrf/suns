#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/computer_player.hpp"
#include "suns/science_samples.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/player_knowledge.hpp"
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
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
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
void completion_receipts_and_surface_evidence_survive_save_load()
{
    auto save = save_fixture(); auto& s = save.state;
    s.players[0].technology.levels.fill(10); s.players[0].technology.progress.fill(0);
    assert(process_science_sample(s, 1, {1, 8, false}));
    const auto packet = s.players[0].pendingPlayerReports.back();
    s.turn = packet.observedTurn; // Persist the resolved annual boundary, not a half-processed turn.
    queue_survey_report(s, 1, 1, 0, s.stars[0].position, s.turn, SurveyLevel::DeepSurvey, true);
    QTemporaryDir dir; const auto path = dir.filePath("receipts.suns"); QString error; SaveGameData loaded;
    assert(write_save_game_file(path, save, error));
    if (!read_save_game_file(path, loaded, error)) { std::cerr << error.toStdString() << '\n'; assert(false); }
    assert(loaded.state.players[0].pendingPlayerReports.back().scientificDataId == packet.scientificDataId);
    const auto& known = loaded.state.players[0].surveyKnowledge[0].planetSnapshot;
    assert(known && known->scienceSamples && known->scienceSamples->size() == 1); // Historical inventory.
    assert(loaded.state.players[0].pendingSurveyReports.back().planetSnapshot->scienceSamples->empty());
    loaded.state.turn = packet.deliveryTurn;
    (void)deliver_due_player_reports(loaded.state);
    const auto field = std::size_t(ResearchField::Energy);
    assert(loaded.state.players[0].technology.progress[field] == 96);
    save = loaded;
    assert(write_save_game_file(path, save, error) && read_save_game_file(path, loaded, error));
    auto replay = packet; replay.deliveryTurn = ++loaded.state.turn;
    loaded.state.players[0].pendingPlayerReports.push_back(replay);
    assert(deliver_due_player_reports(loaded.state).empty());
    assert(loaded.state.players[0].technology.progress[field] == 96);
    assert(loaded.state.players[0].receivedScientificData.size() == 1);

    // The new bounded tail rejects corrupt receipts and truncation.
    for (const auto receipts : {std::vector<std::uint64_t>{0}, {2, 1}, {1, 1}}) {
        auto bad = save; bad.state.players[0].receivedScientificData = receipts;
        assert(write_save_game_file(path, bad, error) && !read_save_game_file(path, loaded, error));
    }
    assert(write_save_game_file(path, save, error));
    auto original = bytes(path); auto truncated = original; truncated.chop(1); writeBytes(path, truncated);
    assert(!read_save_game_file(path, loaded, error));
    QByteArray magic; { QDataStream out(&magic, QIODevice::WriteOnly); out << quint32{0x53434943}; }
    const auto at = original.lastIndexOf(magic); assert(at >= 0);
    auto oversized = original;
    { QDataStream out(&oversized, QIODevice::ReadWrite); assert(out.device()->seek(at + 4)); out << quint32{33 * 1024 * 1024}; }
    writeBytes(path, oversized); assert(!read_save_game_file(path, loaded, error));

    // v92 preserves physical cargo, but cannot invent historical surface evidence.
    save = save_fixture(); assert(process_science_sample(save.state, 1, {1, 8, false}));
    save.state.turn = save.state.players[0].pendingPlayerReports.back().observedTurn;
    assert(write_save_game_file(path, save, error)); auto old = bytes(path); old.truncate(old.lastIndexOf(magic));
    { QDataStream out(&old, QIODevice::ReadWrite); assert(out.device()->seek(4)); out << quint32{92}; }
    writeBytes(path, old); assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets[0].scienceSamples.size() == 1);
    assert(!loaded.state.players[0].surveyKnowledge[0].planetSnapshot->scienceSamples);
    assert(loaded.state.players[0].pendingPlayerReports.back().scientificDataId != 0);

    // A real novel wreck produces the same durable once-only award as samples.
    save = save_fixture(); auto& wreckState = save.state;
    wreckState.players[0].technology.levels.fill(10); wreckState.players[0].technology.progress.fill(0);
    auto& salvager = wreckState.fleets[0]; salvager.position = {900, 0}; salvager.task = FleetTask::Salvage;
    wreckState.wrecks = {{1, salvager.position, 30, {10, 10, 10, 10, 10, 12}}}; wreckState.nextWreckId = 2;
    run_strategic_tasks(wreckState, wreckState.turn);
    assert(wreckState.wrecks.empty() && wreckState.players[0].pendingPlayerReports.size() == 1);
    const auto wreckPacket = wreckState.players[0].pendingPlayerReports.back();
    assert(wreckPacket.quantity == 24);
    assert(write_save_game_file(path, save, error) && read_save_game_file(path, loaded, error));
    loaded.state.turn = wreckPacket.deliveryTurn; (void)deliver_due_player_reports(loaded.state);
    save = loaded;
    assert(write_save_game_file(path, save, error) && read_save_game_file(path, loaded, error));
    loaded.state.players[0].pendingPlayerReports.push_back(wreckPacket);
    assert(deliver_due_player_reports(loaded.state).empty());
    assert(loaded.state.players[0].technology.progress[std::size_t(ResearchField::Weapons)] == 24);
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
void competing_recovery_and_destroyed_cargo_stay_finite_after_reload()
{
    auto save = save_fixture(); auto& s = save.state;
    auto fleet = s.fleets[0]; fleet.position = {900, 0}; fleet.scienceSamples.clear();
    fleet.task = FleetTask::RecoverSample;
    auto rival = fleet; rival.id = 2;
    s.fleets = {rival, fleet}; s.nextFleetId = 3; s.unknownObjects[0].sampleRecovered = false;
    run_strategic_tasks(s, s.turn);
    assert(s.fleets[0].scienceSamples.empty() && s.fleets[1].scienceSamples.size() == 1);
    assert(s.unknownObjects[0].sampleRecovered); // Lower FleetId wins despite reversed storage order.
    s.fleets.clear(); // Destruction removes the only recovered specimen.
    QTemporaryDir dir; QString error; const auto path = dir.filePath("depleted.suns"); SaveGameData loaded;
    assert(write_save_game_file(path, save, error) && read_save_game_file(path, loaded, error));
    loaded.state.fleets.push_back(fleet);
    run_strategic_tasks(loaded.state, loaded.state.turn);
    assert(loaded.state.fleets[0].scienceSamples.empty() && loaded.state.unknownObjects[0].sampleRecovered);
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
void colonist_only_split_produces_a_reloadable_save()
{
    auto save = save_fixture(); auto& s = save.state; auto& f = s.fleets[0];
    s.shipDesigns[1].hull = ShipHullType::LightTransport;
    s.shipDesigns[1].components = {ShipComponentType::FuelMizer}; s.shipDesigns[1].placements.clear();
    f.ships = {{f.design, 1}, {s.shipDesigns[1].id, 9}}; f.colonists = 11'000;
    s.nextFleetId = 2;
    save.state = TurnProcessor{}.process(s, {{1, {SplitFleetOrder{1, {{f.design, 1}, {s.shipDesigns[1].id, 1}}}}}});
    assert(save.state.fleets.size() == 2);
    QTemporaryDir dir; QString error; const auto path = dir.filePath("split.suns");
    assert(write_save_game_file(path, save, error)); SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets.size() == 2);
    assert(loaded.state.fleets[0].colonists + loaded.state.fleets[1].colonists == 11'000);
    for (const auto& part : loaded.state.fleets)
        assert(fleet_cargo_used(loaded.state, part) <= fleet_cargo_capacity(loaded.state, part) + 1e-6);
}
void merged_historical_manifests_stay_reloadable()
{
    for (const auto onArrival : {false, true}) {
        auto save = save_fixture(); auto& s = save.state; auto& f = s.fleets[0];
        f.position = {900, 0}; f.telemetry.position = f.position;
        auto target = f; target.id = 2; target.scienceSamples.clear();
        // Older reports can name specimens no longer physically aboard either fleet.
        for (UnknownObjectId id = 9; id < 73; ++id) {
            f.telemetry.scienceSamples.push_back({id, UnknownObjectKind::AncientProbe, false});
            target.telemetry.scienceSamples.push_back({id + 64, UnknownObjectKind::AncientProbe, false});
        }
        if (onArrival) {
            f.targetFleet = 2; f.destination = target.position;
            f.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
        }
        s.nextFleetId = 3; s.fleets.push_back(target);
        save.state = TurnProcessor{}.process(s, onArrival ? std::vector<PlayerOrders>{}
            : std::vector<PlayerOrders>{{1, {MergeFleetsOrder{2, 1}}}});
        assert(save.state.fleets.size() == 1 && save.state.fleets[0].scienceSamples.size() == 1);
        assert(save.state.fleets[0].telemetry.scienceSamples.size() == 64);
        QTemporaryDir dir; QString error; const auto path = dir.filePath("merged.suns");
        assert(write_save_game_file(path, save, error)); SaveGameData loaded;
        assert(read_save_game_file(path, loaded, error));
        assert(loaded.state.fleets[0].scienceSamples[0].source == 7);
    }
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
    static void orders(MainWindow& w, std::vector<Order> orders) {
        w.pendingOrders_.orders = std::move(orders); w.rebuildScene();
    }
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
    set_survey_level(boosted,1,1,SurveyLevel::DeepSurvey,boosted.turn);
    MainWindowTestAccess::load(w, boosted); w.installResearch();
    assert(MainWindowTestAccess::researchSummary(w).contains("includes 4 from retained artifacts"));
}
void remote_surface_changes_wait_for_receipts_in_ui_ai_and_exports(MainWindow& w)
{
    auto host = save_fixture(); auto& s = host.state;
    s.players[0].controller = PlayerController::Computer;
    s.stars[0].position = {900, 0}; s.planets[0].homeworld = false;
    s.stars[1].position = {0, 0}; s.planets[1].owner = 1; s.planets[1].population = 1000000;
    s.planets[1].homeworld = true; s.planets[1].radar = PlanetaryRadarLevel::Full;
    s.players[0].technology.researchActive = true;
    s.players[0].technology.focus = ResearchField::Energy;
    s.players[0].technology.researchAllocationPercent = 50;
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    ++s.turn;
    host.mode = SessionMode::Host; host.playerTokens = {{1, 31}};
    QTemporaryDir dir; QString error; const auto path = dir.filePath("remote.sunsturn");
    const auto orderPath = dir.filePath("ai.sunsorders");
    const auto exportBytes = [&] {
        auto humanHost = host; humanHost.state.players[0].controller = PlayerController::Human;
        assert(write_save_game_file(path, make_player_turn(humanHost, 1), error)); return bytes(path);
    };
    const auto aiBytes = [&] {
        const auto plan = plan_computer_turn(make_player_view(s, 1));
        TurnOrderFileData packet{17, s.turn, 31, plan.orders, {}};
        for (std::size_t i = 0; i < plan.orders.orders.size(); ++i) packet.descriptions << "AI order";
        assert(write_turn_order_file(orderPath, packet, error)); return std::pair{bytes(orderPath), plan.nextState};
    };
    const auto compareHidden = [&] (const auto& change) {
        const auto exported = exportBytes();
        const auto planned = aiBytes(); MainWindowTestAccess::load(w, s);
        auto* tree = w.findChild<QTreeWidget*>("scienceSampleInventory");
        const auto actionState = [&] {
            for (int i = 0; i < tree->topLevelItemCount(); ++i)
                if (tree->topLevelItem(i)->data(0, Qt::UserRole).toUInt() == 8)
                    tree->setCurrentItem(tree->topLevelItem(i));
            return std::array{w.findChild<QPushButton*>("transferScienceSample")->isEnabled(),
                w.findChild<QPushButton*>("retainScienceSample")->isEnabled(),
                w.findChild<QPushButton*>("analyzeScienceSample")->isEnabled()};
        };
        const auto actions = actionState();
        QStringList rows; for (int i = 0; i < tree->topLevelItemCount(); ++i)
            rows << tree->topLevelItem(i)->text(0) + tree->topLevelItem(i)->text(1) + tree->topLevelItem(i)->text(3);
        const auto summary = MainWindowTestAccess::researchSummary(w);
        change();
        assert(exportBytes() == exported);
        assert(aiBytes() == planned); MainWindowTestAccess::load(w, s);
        QStringList after; for (int i = 0; i < tree->topLevelItemCount(); ++i)
            after << tree->topLevelItem(i)->text(0) + tree->topLevelItem(i)->text(1) + tree->topLevelItem(i)->text(3);
        assert(rows == after && actionState() == actions && MainWindowTestAccess::researchSummary(w) == summary);
    };
    compareHidden([&] { s.planets[0].scienceSamples[0].retained = true; });
    queue_survey_report(s, 1, 1, 0, s.stars[0].position, s.turn, SurveyLevel::DeepSurvey, true);
    const auto due = s.players[0].pendingSurveyReports.back().deliveryTurn; assert(due > s.turn);
    s.turn = due; (void)deliver_due_survey_reports(s); MainWindowTestAccess::load(w, s);
    assert(MainWindowTestAccess::researchSummary(w).contains("includes 4 from retained artifacts"));
    auto* tree = w.findChild<QTreeWidget*>("scienceSampleInventory");
    assert(tree->topLevelItem(0)->text(1).contains("observed year 11"));
    compareHidden([&] { s.planets[0].scienceSamples.clear(); }); // Loss cannot rewrite a received report.
    queue_survey_report(s, 1, 1, 0, s.stars[0].position, s.turn, SurveyLevel::DeepSurvey, true);
    s.turn = s.players[0].pendingSurveyReports.back().deliveryTurn; (void)deliver_due_survey_reports(s);
    MainWindowTestAccess::load(w, s);
    assert(!MainWindowTestAccess::researchSummary(w).contains("includes 4 from retained artifacts"));
}
void stale_manifest_cannot_transfer_or_create_a_phantom_artifact(MainWindow& w)
{
    auto s = fixture(); auto& f = s.fleets[0];
    f.telemetry.scienceSamples = f.scienceSamples; f.telemetry.observedTurn = s.turn - 1;
    f.position = {900, 0};
    s.players[0].technology.researchActive = true;
    s.players[0].technology.focus = ResearchField::Electronics;
    s.players[0].technology.researchAllocationPercent = 50;
    MainWindowTestAccess::load(w, s);
    auto* tree = w.findChild<QTreeWidget*>("scienceSampleInventory");
    auto* transfer = w.findChild<QPushButton*>("transferScienceSample");
    auto* analyze = w.findChild<QPushButton*>("analyzeScienceSample");
    const auto select = [&](UnknownObjectId id) {
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toUInt() == id) {
                tree->setCurrentItem(tree->topLevelItem(i)); return;
            }
        assert(false);
    };
    select(7); assert(!transfer->isEnabled() && !analyze->isEnabled());
    assert(tree->currentItem()->text(1).contains("observed year 9"));
    transfer->click(); assert(MainWindowTestAccess::orders(w).empty());
    select(8); assert(!transfer->isEnabled()); // Stale fleet cannot receive local cargo either.
    MainWindowTestAccess::orders(w, {TransferScienceSampleOrder{{0, 1}, {1, 0}, 7},
        ProcessScienceSampleOrder{1, 7, true}});
    select(7); assert(!analyze->isEnabled());
    assert(tree->currentItem()->data(1, Qt::UserRole).toULongLong() == 1); // Still aboard fleet #1.
    assert(!MainWindowTestAccess::researchSummary(w).contains("includes 4 from retained artifacts"));
    assert(MainWindowTestAccess::state(w).fleets[0].scienceSamples.size() == 1);
    assert(MainWindowTestAccess::state(w).planets[0].scienceSamples.size() == 1);
}
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    persistence_and_migration(); completion_receipts_and_surface_evidence_survive_save_load();
    competing_recovery_and_destroyed_cargo_stay_finite_after_reload();
    exports_and_order_exchange(); colonist_only_split_produces_a_reloadable_save();
    merged_historical_manifests_stay_reloadable();
    MainWindow window; window.installUnknownObjects(); window.installScienceSamples();
    editor_authors_orders_without_mutating_physical_inventory(window);
    stale_manifest_cannot_transfer_or_create_a_phantom_artifact(window);
    remote_surface_changes_wait_for_receipts_in_ui_ai_and_exports(window);
    std::cout << "Science samples UI, saves, migrations and player exchange passed\n";
}
