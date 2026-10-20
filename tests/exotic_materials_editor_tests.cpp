#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
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
void add_mobile_base(GameState& s)
{
    s.shipDesigns.push_back({200, 1, "Expedition", ShipHullType::ExpeditionBase,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5, ShipComponentType::QuickJump5,
         ShipComponentType::QuickJump5, ShipComponentType::ExpeditionDock, ShipComponentType::RelayArray}});
    Fleet base; base.id = 2; base.owner = 1; base.design = 200; base.ships = {{200, 1}};
    base.position = s.fleets[0].position; base.xenocrystal = 8; base.fuel = 5000;
    base.telemetry = confirmed_fleet_telemetry(s, base); s.fleets.push_back(base); s.nextFleetId = 3; s.nextShipDesignId = 201;
}
SaveGameData save_fixture()
{ SaveGameData save; save.state = fixture(); save.pendingOrders = {1, {}}; save.campaignId = 17; save.turnToken = 29; return save; }
QByteArray bytes(const QString& path) { QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll(); }
void writeBytes(const QString& path, const QByteArray& bytes)
{ QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(bytes) == bytes.size()); }
GameState resonance_network()
{
    auto s = fixture(); s.stars.resize(2); s.planets.resize(2); s.orbitalStations.clear();
    s.players[0].technology.levels.fill(18); s.fleets[0].xenocrystal = 0;
    s.stars[0].position = {0, 0}; s.stars[1].position = {250, 0};
    s.fleets[0].position = s.stars[0].position;
    s.fleets[0].telemetry = {};
    s.fleets[0].telemetry = confirmed_fleet_telemetry(s, s.fleets[0]);
    for (auto& p : s.planets) {
        p.owner = 1; p.population = 1000000; p.productionQueue.clear(); p.mines = 0;
        s.orbitalStations.push_back({p.id, 1, p.id, "Paid gate", OrbitalStationHullType::Starbase,
            {OrbitalStationModule::Shipyard}, {ShipComponentType::JumpGate}});
    }
    s.nextOrbitalStationId = 3; s.planets[0].xenocrystal.surface = 8;
    for (auto& p : s.planets) set_survey_level(s, 1, p.star, SurveyLevel::DeepSurvey, s.turn);
    return s;
}
void resonant_gate_cannot_use_later_base_delivery()
{
    auto s = resonance_network(); s.planets[0].xenocrystal.surface = 0; add_mobile_base(s);
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    PlayerOrders orders{1, {TransferXenocrystalOrder{{0, 2}, {1, 0}, 8}, GateFleetOrder{1, 1, 2, true}}};
    const auto preview = projected_cargo_order_state(make_player_view(s, 1).state, orders);
    assert(!preview.planets[0].xenocrystal.surface && preview.fleets[0].position == s.stars[0].position);
    const auto after = TurnProcessor{}.process(s, {orders});
    assert(after.planets[0].xenocrystal.surface == 8 && after.fleets[0].position == s.stars[0].position);
    const auto later = TurnProcessor{}.process(after, {{1, {GateFleetOrder{1, 1, 2, true}}}});
    assert(!later.planets[0].xenocrystal.surface && later.fleets[0].position == s.stars[1].position);
}

void resonant_gate_persistence_and_exchange()
{
    QTemporaryDir dir; QString error; SaveGameData loaded;
    auto save = save_fixture(); save.state = resonance_network(); save.pendingOrders = {1, {GateFleetOrder{1, 1, 2, true}}};
    save.pendingDescriptions = {"Resonant jump"};
    const auto path = dir.filePath("resonance.suns");
    const auto roundTrip = write_save_game_file(path, save, error) && read_save_game_file(path, loaded, error);
    if (!roundTrip) std::cerr << error.toStdString() << "\n";
    assert(roundTrip);
    assert(std::get<GateFleetOrder>(loaded.pendingOrders.orders[0]).resonant);
    const auto expected = TurnProcessor{}.process(save.state, {save.pendingOrders});
    const auto resumed = TurnProcessor{}.process(loaded.state, {loaded.pendingOrders});
    assert(resumed.fleets[0].position == expected.fleets[0].position && resumed.planets[0].xenocrystal.surface == 0);
    save.mode = SessionMode::Host; save.playerTokens = {{1, 31}};
    TurnOrderFileData packet{save.campaignId, save.state.turn, 31, save.pendingOrders, {"Resonant jump"}}, received;
    save.state.players.push_back({2, "Host"}); save.playerTokens[2] = 41;
    save.pendingOrders = {2, {}}; save.pendingDescriptions.clear();
    const auto ordersPath = dir.filePath("resonance.sunsorders");
    assert(write_turn_order_file(ordersPath, packet, error) && read_turn_order_file(ordersPath, received, error));
    assert(validate_turn_submission(save, received).isEmpty() && std::get<GateFleetOrder>(received.orders.orders[0]).resonant);
    const auto exchanged = TurnProcessor{}.process(save.state, {received.orders});
    assert(exchanged.fleets[0].position == expected.fleets[0].position && !exchanged.planets[0].xenocrystal.surface);
    // v89/v35 gate payloads end at destination: absence of the new byte defaults to an ordinary jump.
    QByteArray marker; QDataStream signature(&marker, QIODevice::WriteOnly);
    signature << quint8{24} << quint32{1} << quint32{1} << quint32{2};
    for (const auto& name : {path, ordersPath}) {
        auto current = bytes(name); const auto index = current.indexOf(marker);
        assert(index >= 0 && current.indexOf(marker, index + 1) < 0);
        auto malformed = current; malformed[index + marker.size()] = char(2); writeBytes(name, malformed);
        if (name == path) assert(!read_save_game_file(name, loaded, error));
        else assert(!read_turn_order_file(name, received, error));
        current.remove(index + marker.size(), 1);
        QDataStream legacy(&current, QIODevice::ReadWrite); legacy.device()->seek(4);
        legacy << quint32(name == path ? 89 : 35); writeBytes(name, current);
        if (name == path) {
            assert(read_save_game_file(name, loaded, error));
            assert(!std::get<GateFleetOrder>(loaded.pendingOrders.orders[0]).resonant);
        } else {
            assert(read_turn_order_file(name, received, error));
            assert(!std::get<GateFleetOrder>(received.orders.orders[0]).resonant);
        }
    }
    const auto playerPath = dir.filePath("resonance.sunsturn");
    assert(write_save_game_file(playerPath, make_player_turn(save, 1), error)); const auto frozen = bytes(playerPath);
    save.state.planets[0].xenocrystal.surface = 0; save.state.planets[1].xenocrystal.remaining = 0;
    assert(write_save_game_file(playerPath, make_player_turn(save, 1), error) && bytes(playerPath) == frozen);
}

void persistence_and_migration()
{
    QTemporaryDir dir; const auto path = dir.filePath("exotics.suns"); QString error; SaveGameData loaded;
    auto save = save_fixture(); auto& s = save.state; auto& f = s.fleets[0];
    f.position = {900, 0}; f.telemetry.xenocrystal = 4;
    auto packet = f.telemetry; packet.xenocrystal = 7; f.telemetryInTransit.push_back({15, packet});
    s.planets[0].xenocrystal.surface = 8; assert(apply_resonance_order(s, s.planets[0], {1}));
    (void)resolve_colony_production(s, s.planets[0], true);
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    assert(apply_resonance_order(s, s.planets[0], {1, ResonanceAction::Pause}));
    s.pendingMobileBaseTransfers.push_back({1, 2, 1, 2, 10, 16, 0, {}, 0, 0, false, 8});
    assert(write_save_game_file(path, save, error) && read_save_game_file(path, loaded, error));
    assert(loaded.state.fleets[0].xenocrystal == 8 && loaded.state.fleets[0].telemetry.xenocrystal == 4);
    assert(loaded.state.fleets[0].telemetryInTransit[0].telemetry.xenocrystal == 7);
    assert(loaded.state.planets[0].resonanceLab.work == 60 && loaded.state.planets[0].resonanceLab.crystalsPaid == 8);
    assert(!loaded.state.planets[0].resonanceLab.active && known_xenocrystal(loaded.state, 1, 2)->site.remaining == 20);
    assert(loaded.state.players[0].surveyKnowledge[0].planetSnapshot->resonanceQueueIndex == 0);
    assert(loaded.state.pendingMobileBaseTransfers.size() == 1 && loaded.state.pendingMobileBaseTransfers[0].xenocrystal == 8);
    const auto original = bytes(path); auto truncated = original; truncated.chop(1); writeBytes(path, truncated);
    assert(!read_save_game_file(path, loaded, error));
    for (int defect = 0; defect < 8; ++defect) {
        auto bad = save;
        if (defect == 0) bad.state.planets[1].xenocrystal.remaining = 25;
        if (defect == 1) bad.state.fleets[0].xenocrystal = kMaximumXenocrystal + 1;
        if (defect == 2) bad.state.planets[0].resonanceLab.crystalsPaid = 0;
        if (defect == 3) bad.state.fleets[0].xenocrystal = 100;
        if (defect == 4) bad.state.players[0].surveyKnowledge[0].planetSnapshot->xenocrystal->observedTurn = s.turn + 1;
        if (defect == 5) bad.state.planets[0].resonanceLab.invested.ironium = 0;
        if (defect == 6) bad.state.pendingMobileBaseTransfers[0].xenocrystal = kMaximumXenocrystal + 1;
        if (defect == 7) bad.state.pendingMobileBaseTransfers[0].minerals.ironium = 1;
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
void owned_laboratory_exports_use_received_project_and_queue()
{
    QTemporaryDir dir; const auto path = dir.filePath("lab.sunsturn"); QString error; SaveGameData loaded;
    auto host = save_fixture(); host.mode = SessionMode::Host; host.playerTokens = {{1, 31}};
    auto& s = host.state; auto& p = s.planets[0]; p.xenocrystal.surface = 8;
    assert(apply_resonance_order(s, p, {1})); (void)resolve_colony_production(s, p, true);
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); const auto received = bytes(path);
    // Only the project and its queue change. Pending reports remain host-only.
    for (int year = 1; year < 10; ++year) {
        const auto minerals = p.minerals;
        (void)resolve_colony_production(s, p, true); p.minerals = minerals;
        p.resonanceLab.operational = resonance_operational(s, p);
        assert(write_save_game_file(path, make_player_turn(host, 1), error)); assert(bytes(path) == received);
    }
    assert(p.resonanceLab.work == kResonanceWork && p.productionQueue.empty());
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.planets[0].resonanceLab.work == 60 && resonance_scheduled(loaded.state.planets[0]));
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    assert(write_save_game_file(path, make_player_turn(host, 1), error)); assert(bytes(path) != received);
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.planets[0].resonanceLab.work == kResonanceWork && !resonance_scheduled(loaded.state.planets[0]));
    s.players[0].surveyKnowledge.clear();
    assert(write_save_game_file(path, make_player_turn(host, 1), error));
    assert(read_save_game_file(path, loaded, error) && !loaded.state.planets[0].resonanceLab.started);
}
void remote_base_transfer_survives_save_and_order_exchange()
{
    QTemporaryDir dir; const auto path = dir.filePath("remote.suns"); QString error; SaveGameData loaded;
    auto host = save_fixture(); host.mode = SessionMode::Host; host.playerTokens = {{1, 31}};
    add_mobile_base(host.state); for (auto& f : host.state.fleets) f.position = {900, 0};
    host.state.players.push_back({2, "Host"}); host.playerTokens[2] = 41; host.pendingOrders = {2, {}};
    host.state.stars[1].position = {900, 0}; auto& remote = host.state.planets[1];
    remote.owner = 1; remote.population = 100000; remote.mines = 0; remote.productionQueue.clear();
    remote.minerals = {10000, 10000, 10000};
    host.state.fleets[1].xenocrystal = 0;
    const auto initialIronium = fleet_cargo_capacity(host.state, host.state.fleets[1]) - 8;
    host.state.fleets[1].minerals = {initialIronium, 0, 0};
    const auto ordersPath = dir.filePath("remote.sunsorders");
    TurnOrderFileData packet{host.campaignId, host.state.turn, 31,
        {1, {TransferXenocrystalOrder{{0, 1}, {0, 2}, 8}}}, {"Deliver crystals"}}, received;
    assert(write_turn_order_file(ordersPath, packet, error) && read_turn_order_file(ordersPath, received, error));
    assert(validate_turn_submission(host, received).isEmpty());
    host.state = TurnProcessor{}.process(host.state, {received.orders});
    assert(host.state.pendingMobileBaseTransfers.size() == 1 && host.state.fleets[0].xenocrystal == 8);
    const auto due = host.state.pendingMobileBaseTransfers[0].deliveryTurn;
    const auto roundTrip = write_save_game_file(path, host, error) && read_save_game_file(path, loaded, error);
    if (!roundTrip) std::cerr << error.toStdString() << '\n';
    assert(roundTrip);
    while (loaded.state.turn < due) loaded.state = TurnProcessor{}.process(loaded.state, {});
    loaded.state = TurnProcessor{}.process(loaded.state, {{1, {TransferCargoOrder{{2, 0}, {0, 2}, 0, {5, 0, 0}}}}});
    assert(loaded.state.fleets[1].xenocrystal == 8 && !loaded.state.fleets[0].xenocrystal);
    assert(loaded.state.fleets[1].minerals.ironium == initialIronium);
    assert(loaded.state.pendingMobileBaseTransfers.empty());
    loaded.state = TurnProcessor{}.process(loaded.state, {});
    assert(loaded.state.fleets[1].xenocrystal == 8 && !loaded.state.fleets[0].xenocrystal);
}
}
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w, GameState s, std::vector<Order> orders = {}) {
        w.state_ = std::move(s); w.pendingOrders_ = {1, std::move(orders)}; w.pendingDescriptions_.clear(); w.selection_.fleet = 1; w.selection_.star = 1; w.rebuildScene();
    }
    static QString systemInfo(const MainWindow& w) { return w.selectionLabel_->text(); }
    static void transit(MainWindow& w) { w.openGateTransit(); }
    static const auto& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
    static const auto& state(const MainWindow& w) { return w.state_; }
};
}
namespace {
void cargo_and_system_gate_forecasts_ignore_unreceived_station_loss(MainWindow& w)
{
    auto intact = resonance_network();
    auto observer = intact.fleets[0]; observer.id = intact.nextFleetId++; observer.name = "Observer transport";
    intact.fleets.push_back(observer);
    auto lost = intact;
    lost.pendingStationContacts.push_back({lost.orbitalStations.back(), lost.turn, lost.turn + 4});
    lost.orbitalStations.pop_back();
    assert(known_orbital_station(lost, 1, 2) && known_orbital_station(intact, 1, 2));
    const std::vector<Order> orders{GateFleetOrder{1, 1, 2, true}, TransferCargoOrder{{1, 0}, {0, 1}, 100, {}}};
    QStringList expectedContainers; QString expectedInfo;
    for (const auto* world : {&intact, &lost}) {
        MainWindowTestAccess::load(w, *world, orders);
        const auto info = MainWindowTestAccess::systemInfo(w);
        assert(!info.contains("Queued cargo:"));
        QStringList containers;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
            assert(dialog->objectName() == "cargoTransferDialog");
            const auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo"); assert(source);
            for (int i = 0; i < source->count(); ++i) containers.push_back(source->itemText(i));
            assert(containers.size() == 2 && containers[1].contains("Observer transport"));
            dialog->reject();
        });
        w.openCargoManifestDialog();
        if (world == &intact) { expectedContainers = containers; expectedInfo = info; }
        else { assert(containers == expectedContainers); assert(info == expectedInfo); }
    }
    const auto resolved = TurnProcessor{}.process(lost, {{1, {orders.front()}}});
    assert(resolved.planets[0].xenocrystal.surface == 8 && resolved.fleets[0].position == intact.stars[0].position);
}
void gate_dialog_requires_received_manifest_after_split(MainWindow& w)
{
    for (int evidence = 0; evidence < 3; ++evidence) {
        auto s = resonance_network(); std::vector<Order> orders;
        if (!evidence) {
            s.fleets[0].position = {900, 0}; s.fleets[0].telemetry = {};
        } else {
            s.fleets[0].ships[0].count = 2; s.fleets[0].telemetry.ships = s.fleets[0].ships;
            s.fleets[0].telemetry.observedTurn = evidence == 1 ? s.turn - 1 : s.turn;
            if (evidence == 1) s.fleets[0].position = {900, 0};
            orders.push_back(SplitFleetOrder{1, {{s.fleets[0].design, 1}}});
        }
        MainWindowTestAccess::load(w, s, orders);
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
            dialog->findChild<QCheckBox*>("resonantGateTransit")->setChecked(true);
            assert(dialog->findChild<QComboBox*>("stargateDestinations")->count() == (evidence == 2 ? 1 : 0));
            assert(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled() == (evidence == 2));
            dialog->reject();
        });
        MainWindowTestAccess::transit(w);
    }
}

void gate_dialog_respects_prior_route_and_clear(MainWindow& w)
{
    const auto s = resonance_network();
    MoveFleetOrder move{1, {0, 50}, 5}; MoveFleetOrder clear{1, {0, 0}, 0}; clear.clearRoute = true;
    for (bool clearRoute : {false, true}) {
        MainWindowTestAccess::load(w, s, clearRoute ? std::vector<Order>{move, clear} : std::vector<Order>{move});
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
            dialog->findChild<QCheckBox*>("resonantGateTransit")->setChecked(true);
            const auto count = dialog->findChild<QComboBox*>("stargateDestinations")->count();
            assert(count == (clearRoute ? 1 : 0));
            assert(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled() == clearRoute);
            dialog->reject();
        });
        MainWindowTestAccess::transit(w);
    }
}

void resonant_gate_ui_uses_current_received_payment(MainWindow& w)
{
    auto s = resonance_network(); MainWindowTestAccess::load(w, s);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
        auto* destinations = dialog->findChild<QComboBox*>("stargateDestinations");
        auto* pulse = dialog->findChild<QCheckBox*>("resonantGateTransit");
        auto* details = dialog->findChild<QLabel*>("gateResonanceDetails");
        assert(pulse && pulse->isEnabled() && !pulse->isChecked() && destinations->count() == 0);
        assert(details->text().contains("8 units") && details->text().contains("year 10 (age 0)"));
        pulse->setChecked(true); assert(destinations->count() == 1 && destinations->currentData().toUInt() == 2);
        assert(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        dialog->accept();
    });
    MainWindowTestAccess::transit(w);
    assert(MainWindowTestAccess::orders(w).size() == 1 && std::get<GateFleetOrder>(MainWindowTestAccess::orders(w)[0]).resonant);
    assert(MainWindowTestAccess::state(w).planets[0].xenocrystal.surface == 8); // Queueing never pays physically.
    auto stale = s;
    stale.players[0].surveyKnowledge[0].planetSnapshot->xenocrystal->observedTurn = stale.turn - 1;
    MainWindowTestAccess::load(w, stale);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
        dialog->findChild<QCheckBox*>("resonantGateTransit")->setChecked(true);
        assert(dialog->findChild<QComboBox*>("stargateDestinations")->count() == 0);
        assert(!dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        dialog->reject();
    });
    MainWindowTestAccess::transit(w); assert(MainWindowTestAccess::orders(w).empty());
    // The same supplies cannot be promised to a second fleet in this planning turn.
    auto second = s.fleets[0]; second.id = s.nextFleetId++; s.fleets.push_back(second);
    MainWindowTestAccess::load(w, s, {GateFleetOrder{second.id, 1, 2, true}});
    assert(w.findChild<QTreeWidget*>("exoticInventory")->topLevelItem(0)->text(2).contains("8 units received → 0 planned"));
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
        dialog->findChild<QCheckBox*>("resonantGateTransit")->setChecked(true);
        assert(dialog->findChild<QComboBox*>("stargateDestinations")->count() == 0);
        assert(dialog->findChild<QLabel*>("gateResonanceDetails")->text().contains("0 units"));
        dialog->reject();
    });
    MainWindowTestAccess::transit(w);
}

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
    auto baseWorld = fixture(); add_mobile_base(baseWorld);
    MainWindowTestAccess::load(w, baseWorld);
    from->setCurrentIndex(from->findData(QVariant::fromValue(qulonglong(2))));
    to->setCurrentIndex(to->findData(QVariant::fromValue(qulonglong(1) << 32))); amount->setValue(8);
    assert(transfer->isEnabled());
    MainWindowTestAccess::load(w, baseWorld, {MoveFleetOrder{2, {24, 0}, 3}});
    assert(!transfer->isEnabled());
    MainWindowTestAccess::load(w, baseWorld, {TransferXenocrystalOrder{{0, 2}, {1, 0}, 8}, SetResonanceLaboratoryOrder{1}});
    select_planet(w, 1); assert(details->text().contains("Awaiting 8 physically delivered xenocrystals"));
    assert(!details->text().contains("Forecast completion: year"));
}
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv); persistence_and_migration(); hidden_exports_and_order_exchange();
    owned_laboratory_exports_use_received_project_and_queue();
    remote_base_transfer_survives_save_and_order_exchange();
    resonant_gate_persistence_and_exchange();
    resonant_gate_cannot_use_later_base_delivery();
    MainWindow w; w.installExoticMaterials(); ui_commands_and_previews(w);
    resonant_gate_ui_uses_current_received_payment(w);
    cargo_and_system_gate_forecasts_ignore_unreceived_station_loss(w);
    gate_dialog_respects_prior_route_and_clear(w);
    gate_dialog_requires_received_manifest_after_split(w);
    std::cout << "Exotic material UI, frozen exports, saves and PBEM passed\n";
}
