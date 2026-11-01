#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "map_marker_items.hpp"
#include "save_game.hpp"
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QProgressBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>
#include <cmath>

namespace suns {
struct MainWindowTestAccess {
    static void splitSetup(MainWindow& w)
    {
        w.state_ = make_demo_game(); w.state_.orbitalStations.clear();
        w.state_.fleets[0].ships = {{kScoutDesignId, 2}};
        w.state_.fleets[0].fuel = 120; w.state_.fleets[0].telemetry = {};
        auto hidden = w.state_.fleets.front(); hidden.id = 99; hidden.owner = 2; hidden.position = {5000, 5000};
        w.state_.fleets.push_back(hidden); w.state_.nextFleetId = 100;
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.selection_.star = 1; w.rebuildScene(); emit w.routeProgramContextChanged(true);
    }
    static const PlayerOrders& orders(MainWindow& w) { return w.pendingOrders_; }
    static const GameState& raw(MainWindow& w) { return w.state_; }
    static const GameState& plan(MainWindow& w) { return w.fleetPlanningState(); }
    static const GameState& movementPlan(MainWindow& w) { return w.movementPlanningPreview(); }
    static SaveGameData snapshot(MainWindow& w) { return w.campaignSnapshot(); }
    static bool load(MainWindow& w, const QString& path) { return w.loadGameFromPath(path); }
    static void advance(MainWindow& w) { w.endTurn(); }
    static void orbitMenu(MainWindow& w)
    {
        w.view_->resetTransform(); w.view_->centerOn(0, 0);
        const auto point = w.view_->mapFromScene(QPointF{0, 0}) + QPoint(0, -10);
        QMouseEvent event(QEvent::MouseButtonPress, QPointF(point), QPointF(w.view_->viewport()->mapToGlobal(point)),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        assert(w.eventFilter(w.view_->viewport(), &event));
    }
    static void deliverySetup(MainWindow& w)
    {
        w.state_ = make_demo_game(); w.state_.orbitalStations.clear();
        auto& truck = w.state_.fleets[0]; truck.name = "Fuel truck"; truck.position = {25, 0};
        truck.fuel = 200; truck.telemetry = {};
        auto scout = truck; scout.id = 2; scout.name = "Scout"; scout.position = {0, 0}; scout.fuel = 0;
        w.state_.fleets.push_back(scout); w.state_.nextFleetId = 3;
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.selection_.star = 3; w.rebuildScene(); emit w.routeProgramContextChanged(true);
    }
    static void deepSpaceSetup(MainWindow& w)
    {
        deliverySetup(w);
        for (auto& f : w.state_.fleets) f.position = {10, 10};
        w.rebuildScene(); emit w.routeProgramContextChanged(true);
    }
    static void remoteCommandSetup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        auto& f = w.state_.fleets.front();
        f.position = {5000,0}; f.ships = {{kScoutDesignId,2}};
        f.telemetry.position = f.position; f.telemetry.ships = f.ships;
        w.pendingOrders_ = {1,{MoveFleetOrder{1,{5025,0},5}}};
        w.pendingDescriptions_ = {"Send remote course"}; w.selection_.fleet = 1;
        w.endTurn(); // Real issue/propagation, not a synthetic busy flag.
        assert(w.state_.turn == 2 && w.state_.fleets.front().pendingCommands.size() == 1);
        assert(w.state_.fleets.front().pendingCommands.front().deliveryTurn == 35);
        assert(!w.state_.fleets.front().destination);
    }
    static void addIdlePartner(MainWindow& w)
    {
        auto partner = w.state_.fleets.front(); partner.id = 2; partner.name = "Idle partner";
        partner.pendingCommands.clear(); w.state_.fleets.push_back(partner); w.state_.nextFleetId = 3;
        w.selection_.fleet = 2; w.rebuildScene();
    }
    static void oldReorganisationOrders(MainWindow& w)
    {
        // An older client/save may already contain the invalid Split. It must
        // not shift the valid second split in either disposable preview.
        w.pendingOrders_ = {1,{SplitFleetOrder{1,{{kScoutDesignId,1}}},
            SplitFleetOrder{2,{{kScoutDesignId,1}}},RenameFleetOrder{3,"Valid child"}}};
        w.pendingDescriptions_ = {"Busy split","Idle split","Rename child"}; w.rebuildScene();
    }
};
}
using namespace suns;
static const Fleet& fleet(const GameState& state, FleetId id)
{ for (const auto& f : state.fleets) if (f.id == id) return f; assert(false); return state.fleets.front(); }
static bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
static int named(QComboBox* combo, const QString& text)
{ for (int i = 0; i < combo->count(); ++i) if (combo->itemText(i).contains(text)) return i; assert(false); return -1; }
static void rename(MainWindow& window, const QString& name)
{
    QTimer::singleShot(0, [name] { auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()); assert(dialog); dialog->setTextValue(name); dialog->accept(); });
    window.openRenameFleetDialog();
}
static void fuel(MainWindow& window, double amount, bool accept = true, bool receivingSlider = false, bool capture = false)
{
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog && dialog->objectName() == "cargoTransferDialog");
        auto* from = dialog->findChild<QComboBox*>("cargoSourceCombo");
        auto* to = dialog->findChild<QComboBox*>("cargoDestinationCombo");
        from->setCurrentIndex(named(from, "Fuel truck")); to->setCurrentIndex(named(to, "Scout"));
        auto* spin = dialog->findChild<QDoubleSpinBox*>("cargoFuelSpin"); assert(spin && spin->isEnabled());
        if (receivingSlider) dialog->findChild<QSlider*>("cargoDestinationFuelSlider")->setValue(int(amount * 100));
        else spin->setValue(amount);
        auto* buttons = dialog->findChild<QDialogButtonBox*>(); assert(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        assert(dialog->findChild<QLabel*>("cargoDestinationAfterLabel")->text().contains("fuel"));
        if (capture) dialog->grab().save("/tmp/suns-fleet-fuel-transfer.png");
        buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
    });
    window.openCargoManifestDialog();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv); QTemporaryDir settings; assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    MainWindow window; window.installDeferredMapSelectionHandler(); attachRouteProgramDock(window);
    window.installUiPolish(); window.installFleetReadabilityPolish(); window.installFleetOperationsPanel();
    window.resize(1366, 900); window.show(); app.processEvents();
    MainWindowTestAccess::splitSetup(window);
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog && dialog->windowTitle() == "Split fleet");
        dialog->findChild<QSpinBox*>()->setValue(1); dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    window.openSplitFleetDialog();
    const FleetId child = 2;
    assert(window.availableOwnedFleetsForRouteProgram().size() == 2);
    assert(fleet_ship_count(fleet(MainWindowTestAccess::raw(window), 1)) == 2);
    assert(fleet_ship_count(fleet(MainWindowTestAccess::plan(window), 1)) == 1);
    assert(fleet_ship_count(fleet(MainWindowTestAccess::plan(window), child)) == 1);
    assert(window.selectFleetForRouteProgram(child)); rename(window, "Scout");
    assert(window.selectFleetForRouteProgram(1)); rename(window, "Fuel truck");
    assert(window.fleetTargetNameForRouteProgram(child).contains("Scout"));
    MainWindowTestAccess::orbitMenu(window);
    auto* menu = window.findChild<QMenu*>("mapFleetPicker"); assert(menu);
    int choices = 0;
    for (auto* action : menu->actions()) if (action->data().toUInt()) {
        ++choices;
        assert(action->text() == (action->data().toUInt() == child ? "Scout" : "Fuel truck"));
    }
    assert(choices == 2); menu->close(); app.processEvents();
    fuel(window, 40, true, false, app.arguments().contains("--capture"));
    assert(near(fleet(MainWindowTestAccess::plan(window), 1).fuel, 20));
    assert(near(fleet(MainWindowTestAccess::plan(window), child).fuel, 100));
    assert(near(fleet(MainWindowTestAccess::raw(window), 1).fuel, 120));
    fuel(window, 60, true, true); // Return the planned 40 mg through the receiving slider.
    assert(near(fleet(MainWindowTestAccess::plan(window), child).fuel, 60));
    const auto count = MainWindowTestAccess::orders(window).orders.size();
    fuel(window, 40, false); assert(MainWindowTestAccess::orders(window).orders.size() == count);
    fuel(window, 40);
    assert(window.selectFleetForRouteProgram(child));
    assert(window.appendCoordinateWaypoint({25, 0}, 5, {}));
    assert(!window.selectedFleetRouteProgramRows().empty());

    // Save/reload and the actual PBEM order-file codec preserve the temporary
    // detachment reference; the host binds it to its private allocator.
    QString error;
    const auto snapshot = MainWindowTestAccess::snapshot(window);
    const auto savePath = settings.filePath("planned-split.suns");
    assert(write_save_game_file(savePath, snapshot, error));
    assert(MainWindowTestAccess::load(window, savePath));
    assert(window.selectFleetForRouteProgram(child));
    assert(window.fleetTargetNameForRouteProgram(child).contains("Scout"));
    const auto orderPath = settings.filePath("split.sunsorders");
    TurnOrderFileData packet{snapshot.campaignId, snapshot.state.turn, snapshot.turnToken, snapshot.pendingOrders, snapshot.pendingDescriptions};
    assert(write_turn_order_file(orderPath, packet, error));
    TurnOrderFileData received;
    assert(read_turn_order_file(orderPath, received, error));
    const auto hosted = TurnProcessor{}.process(snapshot.state, {received.orders});
    assert(fleet(hosted, 100).name == "Scout" && same_position(fleet(hosted, 100).position, {25, 0}));
    assert(near(fleet(hosted, 1).fuel, 20));
    MainWindowTestAccess::advance(window);
    const auto& actual = MainWindowTestAccess::raw(window);
    assert(fleet(actual, 100).name == "Scout" && same_position(fleet(actual, 100).position, {25, 0}));
    assert(near(fleet(actual, 1).fuel, 20));

    MainWindowTestAccess::deliverySetup(window);
    auto* destination = window.findChild<QComboBox*>("routeDestinationCombo");
    destination->setCurrentIndex(named(destination, "Scout")); // Target is away from the selected system.
    auto* action = window.findChild<QComboBox*>("routeArrivalActionCombo");
    assert(action->findData(int(FleetArrivalActionKind::MergeWithFleet)) >= 0);
    action->setCurrentIndex(action->findData(int(FleetArrivalActionKind::SupplyOutpost)));
    window.findChild<QDoubleSpinBox*>("routeFuelReserveSpin")->setValue(50);
    window.findChild<QProgressBar*>("routeWarpSelector")->setValue(5);
    window.findChild<QPushButton*>("routeAddButton")->click();
    const auto& route = std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front());
    assert(route.targetFleet == 2 && route.arrivalAction.kind == FleetArrivalActionKind::SupplyOutpost);
    assert(near(route.arrivalAction.reserveFuel, 50));
    MainWindowTestAccess::advance(window);
    assert(MainWindowTestAccess::raw(window).fleets.size() == 2);
    assert(near(fleet(MainWindowTestAccess::raw(window), 1).fuel, 50));
    assert(fleet(MainWindowTestAccess::raw(window), 2).fuel > 0);

    MainWindowTestAccess::deepSpaceSetup(window);
    fuel(window, 40);
    assert(near(fleet(MainWindowTestAccess::plan(window), 1).fuel, 160));
    assert(near(fleet(MainWindowTestAccess::plan(window), 2).fuel, 40));

    MainWindowTestAccess::remoteCommandSetup(window);
    const auto blocked = [&](bool split, const QString& reason) {
        const auto count = MainWindowTestAccess::orders(window).orders.size();
        QTimer::singleShot(0,[&] {
            auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            assert(message && message->text().contains(reason));
            message->accept();
        });
        if (split) window.openSplitFleetDialog(); else window.openMergeFleetsDialog();
        assert(MainWindowTestAccess::orders(window).orders.size() == count);
    };
    blocked(true,"commands in flight");
    blocked(false,"commands in flight");
    assert(window.availableOwnedFleetsForRouteProgram().size() == 1);
    const auto& known = fleet(MainWindowTestAccess::plan(window),1);
    assert(known.pendingCommands.size() == 1 && known.pendingCommands.front().deliveryTurn == 35);
    assert(known.telemetry.observedTurn == 1 && known.position == Position(5000,0));
    assert(!known.destination && fleet_ship_count(known) == 2);
    const auto remoteSave = settings.filePath("remote-command.suns");
    assert(write_save_game_file(remoteSave,MainWindowTestAccess::snapshot(window),error));
    assert(MainWindowTestAccess::load(window,remoteSave));
    blocked(true,"commands in flight");
    assert(MainWindowTestAccess::plan(window).fleets.size() == 1);
    MainWindowTestAccess::addIdlePartner(window);
    blocked(false,"No other stationary"); // A busy source cannot be merged into an idle destination.
    assert(MainWindowTestAccess::plan(window).fleets.size() == 2);
    MainWindowTestAccess::oldReorganisationOrders(window);
    for (const auto* preview : {&MainWindowTestAccess::plan(window),&MainWindowTestAccess::movementPlan(window)}) {
        assert(preview->fleets.size() == 3 && fleet_ship_count(fleet(*preview,1)) == 2);
        assert(fleet(*preview,3).name == "Valid child" && fleet_ship_count(fleet(*preview,3)) == 1);
    }
}
