#include "main_window.hpp"

#include <QApplication>
#include <QPushButton>
#include <QTreeWidget>

#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& window)
    {
        window.state_ = make_demo_game();
        auto& scout = window.state_.fleets.front();
        scout.ships = {{kScoutDesignId, 1}};
        Fleet colony = scout;
        colony.id = 2;
        colony.name = "Colony Group";
        colony.design = kColonyShipDesignId;
        colony.role = FleetRole::ColonyShip;
        colony.ships = {{kColonyShipDesignId, 1}};
        colony.colonists = 1000;
        window.state_.fleets.push_back(colony);
        window.state_.nextFleetId = 3;
        window.pendingOrders_ = {1, {}};
        window.pendingDescriptions_.clear();
        window.selection_.fleet = 1;
        window.selection_.star = window.state_.planets.front().star;
        window.rebuildScene();
        window.refreshFleetCompositionTable();
    }

    static void queueMerge(MainWindow& window)
    {
        window.pendingOrders_.orders.emplace_back(MergeFleetsOrder{1, 2});
        window.pendingDescriptions_ << "Merge Colony Group into Scout 1";
        window.rebuildScene();
        window.refreshFleetCompositionTable();
    }

    static void advance(MainWindow& window)
    {
        window.endTurn();
        window.refreshFleetCompositionTable();
    }

    static const GameState& state(const MainWindow& window) { return window.state_; }
};
} // namespace suns

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    window.installUiPolish();
    suns::MainWindowTestAccess::install(window);

    auto* table = window.findChild<QTreeWidget*>("fleetCompositionTree");
    auto* rename = window.findChild<QPushButton*>("renameFleetButton");
    assert(rename && rename->text().contains("Rename"));
    assert(table && table->topLevelItemCount() == 1);
    assert(table->topLevelItem(0)->text(0) == "Scout");
    assert(table->topLevelItem(0)->text(1) == "1");

    suns::MainWindowTestAccess::queueMerge(window);
    assert(table->topLevelItemCount() == 2);
    assert(table->toolTip().contains("2 ships"));
    assert(table->toolTip().contains("current-year merge"));

    suns::MainWindowTestAccess::advance(window);
    const auto& state = suns::MainWindowTestAccess::state(window);
    assert(state.fleets.size() == 1 && state.fleets.front().id == 1);
    assert(suns::fleet_ship_count(state.fleets.front()) == 2);
    assert(suns::fleet_ship_count(state.fleets.front(), suns::kScoutDesignId) == 1);
    assert(suns::fleet_ship_count(state.fleets.front(), suns::kColonyShipDesignId) == 1);
    assert(table->topLevelItemCount() == 2);
}
