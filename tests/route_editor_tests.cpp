#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLabel>
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
        other.position.x += 20.0;
        window.state_.fleets.push_back(other);
        window.pendingOrders_ = PlayerOrders{1, {}};
        window.pendingDescriptions_.clear();
        window.selectedFleetId_ = 1;
        window.selectedStarId_ = window.state_.stars.at(1).id;
        emit window.routeProgramContextChanged(true);
        window.rebuildScene();
    }

    static const PlayerOrders& orders(const MainWindow& window) { return window.pendingOrders_; }
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
    auto* pickTarget = window.findChild<QPushButton*>("routePickTargetButton");
    auto* pickedTarget = window.findChild<QLabel*>("routePickedTargetLabel");
    assert(source && action && cargo && reserve && warp && add && targetType && target
        && pickTarget && pickedTarget);
    const auto none = static_cast<int>(suns::FleetArrivalActionKind::None);
    const auto load = static_cast<int>(suns::FleetArrivalActionKind::LoadAllAvailable);
    const auto unload = static_cast<int>(suns::FleetArrivalActionKind::UnloadAll);

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

    std::cout << "route editor tests passed\n";
}
