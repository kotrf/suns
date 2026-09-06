#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QApplication>
#include <QBrush>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>

#include <cassert>
#include <iostream>

namespace suns {

struct MainWindowTestAccess {
    static void installTwoFleets(MainWindow& window)
    {
        window.state_ = make_demo_game();
        auto other = window.state_.fleets.front();
        other.id = window.state_.nextFleetId++;
        other.name = "Second fleet";
        other.position = window.state_.stars.at(1).position;
        other.telemetry = {};
        other.telemetry.observedTurn = window.state_.turn;
        other.telemetry.position = other.position;
        other.telemetry.warp = other.warp;
        other.telemetry.fuel = other.fuel;
        other.telemetry.ships = other.ships;
        window.state_.fleets.push_back(other);
        auto enemy = other;
        enemy.id = window.state_.nextFleetId++;
        enemy.owner = 2;
        enemy.name = "Enemy raiders";
        enemy.telemetry = {};
        enemy.telemetry.observedTurn = window.state_.turn;
        enemy.telemetry.position = enemy.position;
        enemy.telemetry.warp = enemy.warp;
        enemy.telemetry.fuel = enemy.fuel;
        enemy.telemetry.ships = enemy.ships;
        window.state_.fleets.push_back(enemy);
        window.pendingOrders_ = PlayerOrders{1, {}};
        window.pendingDescriptions_.clear();
        window.selectedFleetId_ = 1;
        window.selectedStarId_ = window.state_.stars.at(1).id;
        emit window.routeProgramContextChanged(true);
        window.rebuildScene();
    }

    static const PlayerOrders& orders(const MainWindow& window) { return window.pendingOrders_; }

    static bool rightClick(MainWindow& window, QGraphicsItem* item)
    {
        assert(window.view_ && item);
        const auto viewportPosition =
            window.view_->mapFromScene(item->sceneBoundingRect().center());
        QMouseEvent press(
            QEvent::MouseButtonPress,
            QPointF(viewportPosition),
            Qt::RightButton,
            Qt::RightButton,
            Qt::NoModifier);
        return window.eventFilter(window.view_->viewport(), &press);
    }
};

} // namespace suns

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    window.installDeferredMapSelectionHandler();
    suns::MainWindowTestAccess::installTwoFleets(window);
    suns::attachRouteProgramDock(window);

    const auto fleets = window.availableOwnedFleetsForRouteProgram();
    assert(fleets.size() == 2);
    auto* source = window.findChild<QComboBox*>("routeSourceFleetCombo");
    auto* action = window.findChild<QComboBox*>("routeArrivalActionCombo");
    auto* cargo = window.findChild<QComboBox*>("routeCargoCombo");
    auto* reserve = window.findChild<QSpinBox*>("routeReserveSpin");
    auto* warp = window.findChild<QProgressBar*>("routeWarpSelector");
    auto* add = window.findChild<QPushButton*>("routeAddButton");
    auto* targetType = window.findChild<QComboBox*>("routeTargetTypeCombo");
    auto* target = window.findChild<QComboBox*>("routeTargetFleetCombo");
    auto* destination = window.findChild<QComboBox*>("routeDestinationCombo");
    auto* pickTarget = window.findChild<QPushButton*>("routePickTargetButton");
    auto* pickedTarget = window.findChild<QLabel*>("routePickedTargetLabel");
    assert(source && action && cargo && reserve && warp && add && targetType && target
        && destination
        && pickTarget && pickedTarget);
    const auto none = static_cast<int>(suns::FleetArrivalActionKind::None);
    const auto load = static_cast<int>(suns::FleetArrivalActionKind::LoadAllAvailable);
    const auto unload = static_cast<int>(suns::FleetArrivalActionKind::UnloadAll);

    // The selected system and all visible orbiting fleets share one combo box.
    // Enemy fleets are explicitly labeled and colored red.
    assert(destination->count() == 3);
    assert(destination->itemData(0, Qt::UserRole).toInt() == 1);
    assert(destination->itemData(0, Qt::UserRole + 1).toUInt() != 0);
    assert(destination->itemData(1, Qt::UserRole).toInt() == 2);
    assert(destination->itemData(1, Qt::UserRole + 1).toUInt() == fleets[1]);
    assert(destination->itemData(2, Qt::UserRole).toInt() == 3);
    assert(destination->itemText(2).contains("Enemy"));
    assert(destination->itemData(2, Qt::ForegroundRole).value<QBrush>().color()
        == QColor("#e05252"));

    // Choosing the red enemy entry through Destination prepares a fixed route
    // to its observed orbital position. Merge is never carried across.
    destination->setCurrentIndex(2);
    assert(targetType->currentData().toInt() == 2);
    assert(action->currentData().toInt() == none);
    add->click();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);
    const auto& comboAttack = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(comboAttack.fleet == fleets[0]);
    assert(comboAttack.targetFleet == 0);

    // Reset before checking that editor drafts remain isolated per source.
    suns::MainWindowTestAccess::installTwoFleets(window);

    action->setCurrentIndex(action->findData(load));
    reserve->setValue(4321);
    warp->setValue(6);

    // Switching and clicking immediately must work before the 180 ms timer.
    source->setCurrentIndex(source->findData(static_cast<quint32>(fleets[1])));
    assert(window.selectedFleetForRouteProgram() == fleets[1]);
    assert(action->currentData().toInt() == none);
    assert(reserve->value() == 1000);
    add->click();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);
    const auto& secondOrder = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(secondOrder.fleet == fleets[1]);
    assert(secondOrder.arrivalAction.kind == suns::FleetArrivalActionKind::None);

    action->setCurrentIndex(action->findData(unload));
    cargo->setCurrentIndex(cargo->findData(static_cast<int>(suns::FleetCargoKind::Germanium)));
    warp->setValue(5);
    source->setCurrentIndex(source->findData(static_cast<quint32>(fleets[0])));
    assert(action->currentData().toInt() == load);
    assert(cargo->currentData().toInt() == static_cast<int>(suns::FleetCargoKind::Colonists));
    assert(reserve->value() == 4321);
    assert(warp->value() == 6);
    add->click();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 2);
    const auto& firstOrder = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.back());
    assert(firstOrder.fleet == fleets[0]);
    assert(firstOrder.arrivalAction.kind == suns::FleetArrivalActionKind::LoadAllAvailable);
    assert(firstOrder.arrivalAction.reservePopulation == 4321);

    // Exercise actual map selection. Its redraw is deferred; editor identity
    // must already be correct before processing any queued events.
    window.routeProgramScene()->clearSelection();
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() == 2 && item->data(0).toUInt() == fleets[1]) {
            item->setSelected(true);
            break;
        }
    }
    assert(window.selectedFleetForRouteProgram() == fleets[1]);
    assert(source->currentData().toUInt() == fleets[1]);
    assert(action->currentData().toInt() == unload);
    assert(cargo->currentData().toInt() == static_cast<int>(suns::FleetCargoKind::Germanium));
    assert(warp->value() == 5);
    add->click();
    const auto& secondUpdated = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(secondUpdated.fleet == fleets[1]);
    assert(secondUpdated.arrivalAction.kind == suns::FleetArrivalActionKind::None);
    assert(secondUpdated.queuedWaypoints.size() == 1);
    assert(secondUpdated.queuedWaypoints.front().arrivalAction.kind == suns::FleetArrivalActionKind::UnloadAll);
    assert(secondUpdated.queuedWaypoints.front().arrivalAction.cargo == suns::FleetCargoKind::Germanium);
    assert(std::get<suns::MoveFleetOrder>(suns::MainWindowTestAccess::orders(window).orders.back())
        .queuedWaypoints.empty());

    targetType->setCurrentIndex(targetType->findData(1));
    target->setCurrentIndex(target->findData(static_cast<quint32>(fleets[0])));
    window.selectFleetForRouteProgram(fleets[0]);
    assert(targetType->currentData().toInt() == 0);
    assert(action->currentData().toInt() == load);
    window.selectFleetForRouteProgram(fleets[1]);
    assert(targetType->currentData().toInt() == 1);
    assert(target->currentData().toUInt() == fleets[0]);
    assert(action->currentData().toInt() == static_cast<int>(suns::FleetArrivalActionKind::MergeWithFleet));

    // Reused IDs after loading/restarting must not inherit old editor drafts.
    suns::MainWindowTestAccess::installTwoFleets(window);
    assert(action->currentData().toInt() == none);
    window.selectFleetForRouteProgram(fleets[1]);
    assert(action->currentData().toInt() == none);
    assert(targetType->currentData().toInt() == 0);
    assert(reserve->value() == 1000);

    // Map target mode keeps the first fleet as the source while a click on
    // another fleet captures that second FleetId as the moving destination.
    window.selectFleetForRouteProgram(fleets[0]);
    pickTarget->click();
    assert(window.routeProgramMapTargetPickActive());
    window.routeProgramScene()->clearSelection();
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() == 2 && item->data(0).toUInt() == fleets[1]) {
            item->setSelected(true);
            break;
        }
    }
    assert(!window.routeProgramMapTargetPickActive());
    assert(!pickTarget->isChecked());
    assert(window.selectedFleetForRouteProgram() == fleets[0]);
    assert(source->currentData().toUInt() == fleets[0]);
    assert(targetType->currentData().toInt() == 1);
    assert(target->currentData().toUInt() == fleets[1]);
    assert(action->currentData().toInt()
        == static_cast<int>(suns::FleetArrivalActionKind::MergeWithFleet));
    assert(pickedTarget->text().contains("Second fleet"));
    add->click();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);
    const auto& pursuit = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(pursuit.fleet == fleets[0]);
    assert(pursuit.targetFleet == fleets[1]);
    assert(pursuit.arrivalAction.kind == suns::FleetArrivalActionKind::MergeWithFleet);

    // A picked star likewise leaves the source fleet intact and uses the
    // ordinary fixed-destination path.
    suns::MainWindowTestAccess::installTwoFleets(window);
    pickTarget->click();
    assert(window.routeProgramMapTargetPickActive());
    window.routeProgramScene()->clearSelection();
    const auto targetStar = window.routeProgramScene()->items();
    bool pickedStar{};
    for (auto* item : targetStar) {
        if (item->data(1).toInt() == 1) {
            item->setSelected(true);
            pickedStar = true;
            break;
        }
    }
    assert(pickedStar);
    assert(!window.routeProgramMapTargetPickActive());
    assert(window.selectedFleetForRouteProgram() == fleets[0]);
    assert(targetType->currentData().toInt() == 0);
    assert(action->currentData().toInt() == none);
    add->click();
    const auto& starRoute = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(starRoute.fleet == fleets[0]);
    assert(starRoute.targetFleet == 0);

    // Right-click is the fast route gesture: it captures the pointed map
    // object and appends it immediately with the editor's current settings.
    suns::MainWindowTestAccess::installTwoFleets(window);
    QGraphicsItem* fleetTargetItem{};
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() == 2 && item->data(0).toUInt() == fleets[1]) {
            fleetTargetItem = item;
            break;
        }
    }
    assert(suns::MainWindowTestAccess::rightClick(window, fleetTargetItem));
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);
    const auto& quickPursuit = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(quickPursuit.fleet == fleets[0]);
    assert(quickPursuit.targetFleet == fleets[1]);
    assert(quickPursuit.arrivalAction.kind == suns::FleetArrivalActionKind::MergeWithFleet);

    // Enemy targets are distinguished from friendly moving targets. The route
    // goes to the enemy's currently observed position with No action; combat
    // resolution remains a game rule rather than an arrival action.
    suns::MainWindowTestAccess::installTwoFleets(window);
    const auto enemyFleet = destination->itemData(2, Qt::UserRole + 1).toUInt();
    QGraphicsItem* enemyTargetItem{};
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() == 2 && item->data(0).toUInt() == enemyFleet) {
            enemyTargetItem = item;
            break;
        }
    }
    assert(suns::MainWindowTestAccess::rightClick(window, enemyTargetItem));
    const auto& quickAttack = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(quickAttack.fleet == fleets[0]);
    assert(quickAttack.targetFleet == 0);
    assert(quickAttack.arrivalAction.kind == suns::FleetArrivalActionKind::None);
    assert(action->currentData().toInt() == none);

    suns::MainWindowTestAccess::installTwoFleets(window);
    QGraphicsItem* starTargetItem{};
    const auto selectedSystem =
        destination->itemData(0, Qt::UserRole + 1).toUInt();
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() == 1
            && item->data(0).toUInt() == selectedSystem) {
            starTargetItem = item;
            break;
        }
    }
    assert(suns::MainWindowTestAccess::rightClick(window, starTargetItem));
    const auto& quickStarRoute = std::get<suns::MoveFleetOrder>(
        suns::MainWindowTestAccess::orders(window).orders.front());
    assert(quickStarRoute.fleet == fleets[0]);
    assert(quickStarRoute.targetFleet == 0);

    std::cout << "route editor tests passed\n";
}
