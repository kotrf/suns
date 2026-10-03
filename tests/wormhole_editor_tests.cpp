#include "main_window.hpp"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>

#include <cassert>
#include <iostream>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w) {
        w.state_ = make_demo_game();
        w.state_.wormholeRules.spawnChancePerTurn = 0;
        w.state_.wormholes.push_back({{{{19, {30, 0}, WormholeSignature::Strong},
            {20, {-400, 200}, WormholeSignature::Weak}}}, 1, 20, 0.7});
        w.state_.nextWormholeEndpointId = 21;
        w.state_.players.front().wormholeKnowledge.push_back({19, {20, 0}, 1});
        w.pendingOrders_ = {1, {}};
        w.selection_.fleet = 1;
        w.rebuildScene();
    }
    static void classify(MainWindow& w) {
        w.state_.players.front().wormholeKnowledge.front().stability = WormholeStability::Variable;
        w.rebuildScene();
    }
    static const MoveFleetOrder& move(MainWindow& w) { return std::get<MoveFleetOrder>(w.pendingOrders_.orders.front()); }
    static void hidePhysicalFleet(MainWindow& w) {
        w.state_.wormholeTransits.push_back({w.state_.fleets.front(), 19, 2, 12, 17});
        w.state_.fleets.clear();
        w.rebuildScene();
    }
    static void installState(MainWindow& w, GameState state) {
        w.state_ = std::move(state); w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.rebuildScene();
    }
    static QString fleetPanel(MainWindow& w) { return w.fleetLabel_->text(); }
    static QString empirePanel(MainWindow& w) { return w.empireLabel_->text(); }
    static Position planningPosition(MainWindow& w) { return w.selectedFleetPlanningView()->position; }
    static bool hasSelectedFleet(MainWindow& w) { return w.selectedFleet() != nullptr; }
    static void setTransitStatus(MainWindow& w, WormholeTransitStatus status) {
        w.state_.wormholeTransits.front().status = status;
        w.rebuildScene();
    }
};
} // namespace suns

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    suns::MainWindowTestAccess::setup(window);
    window.installWormholes();
    auto* contacts = window.findChild<QTreeWidget*>("wormholeContacts");
    auto* enter = window.findChild<QPushButton*>("enterWormholeButton");
    auto* approach = window.findChild<QPushButton*>("approachWormholeButton");
    assert(contacts && enter && approach);
    assert(contacts->topLevelItemCount() == 1);
    contacts->setCurrentItem(contacts->topLevelItem(0));
    assert(!enter->isEnabled() && approach->isEnabled());
    assert(contacts->topLevelItem(0)->text(1).contains("20.0"));
    assert(contacts->topLevelItem(0)->text(3) == "Unknown");
    assert(!window.queueWormholeApproach(19, true));
    approach->click();
    assert(suns::MainWindowTestAccess::move(window).arrivalAction.kind == suns::FleetArrivalActionKind::None);
    suns::MainWindowTestAccess::classify(window);
    assert(enter->isEnabled());
    enter->click();
    const auto& move = suns::MainWindowTestAccess::move(window);
    assert(move.arrivalAction.kind == suns::FleetArrivalActionKind::EnterWormhole);
    assert(move.arrivalAction.wormholeEndpoint == 19);
    assert(move.destination.x == 20); // Last known coordinate, not host truth 30.
    assert(window.selectedFleetRouteProgramForecast().contains("cannot be forecast"));
    assert(!window.selectedFleetRouteProgramForecast().contains("-400"));
    suns::MainWindowTestAccess::hidePhysicalFleet(window);
    assert(suns::MainWindowTestAccess::hasSelectedFleet(window));
    auto* status = window.findChild<QLabel*>("wormholeFleetStatus");
    assert(status && status->text().isEmpty());
    bool fleetMarker = false;
    for (const auto* item : window.routeProgramScene()->items())
        if (item->data(1).toInt() == 2 && item->data(0).toUInt() == 1) fleetMarker = true;
    assert(fleetMarker);
    suns::MainWindowTestAccess::setTransitStatus(window, suns::WormholeTransitStatus::Overdue);
    assert(status->text().contains("NO CONTACT"));
    suns::MainWindowTestAccess::setTransitStatus(window, suns::WormholeTransitStatus::PresumedLost);
    assert(status->text().contains("PRESUMED LOST"));
    assert(!suns::MainWindowTestAccess::hasSelectedFleet(window));
    auto source = suns::make_demo_game();
    source.wormholeRules.spawnChancePerTurn = source.wormholeRules.driftPerTurn = source.wormholeRules.relocationChance = 0;
    source.wormholeRules.instabilityLossChance = 0;
    source.wormholes.push_back({{{{19, {300, 0}, suns::WormholeSignature::Strong},
        {20, {-400, 200}, suns::WormholeSignature::Weak}}}, 1, 30, 0.7});
    auto& moving = source.fleets.front();
    moving.position = moving.telemetry.position = {236, 0};
    moving.destination = moving.telemetry.destination = suns::Position{300, 0};
    moving.arrivalAction = moving.telemetry.arrivalAction = suns::FleetArrivalAction{
        suns::FleetArrivalActionKind::EnterWormhole, 1, suns::FleetCargoKind::Colonists, 19};
    auto lost = source, alive = source;
    lost.wormholeRules.minimumLossChance = 1;
    alive.wormholeRules.minimumLossChance = 0;
    lost = suns::TurnProcessor{}.process(lost, {});
    alive = suns::TurnProcessor{}.process(alive, {});
    suns::MainWindow liveWindow, lostWindow;
    suns::MainWindowTestAccess::installState(liveWindow, alive);
    suns::MainWindowTestAccess::installState(lostWindow, lost);
    assert(suns::MainWindowTestAccess::planningPosition(liveWindow).x == 300);
    assert(suns::MainWindowTestAccess::planningPosition(lostWindow).x == 300);
    assert(suns::MainWindowTestAccess::fleetPanel(liveWindow) == suns::MainWindowTestAccess::fleetPanel(lostWindow));
    assert(suns::MainWindowTestAccess::empirePanel(liveWindow) == suns::MainWindowTestAccess::empirePanel(lostWindow));
    assert(liveWindow.availableOwnedFleetsForRouteProgram() == lostWindow.availableOwnedFleetsForRouteProgram());
    std::cout << "wormhole editor tests passed\n";
}
