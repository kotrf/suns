#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

#include <QApplication>
#include <QPushButton>
#include <QShortcut>
#include <QTreeWidget>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.state_.players.front().technology.researchAllocationPercent = 100;
        w.state_.planets.front().productionQueue = {{ProductionKind::Factory, 2, 0}};
        w.pendingOrders_ = {1, {QueueProductionOrder{1, ProductionKind::Mine}}};
        w.pendingDescriptions_ = {"Build mine"};
        w.selectedStarId_ = w.state_.planets.front().star;
        w.rebuildScene();
        w.refreshProductionQueue();
    }
    static void advance(MainWindow& w)
    {
        w.endTurn();
        w.selectedStarId_ = w.state_.planets.front().star;
        w.rebuildScene();
        w.refreshProductionQueue();
    }
    static const Planet& colony(const MainWindow& w) { return w.state_.planets.front(); }
    static void moveUp(MainWindow& w) { w.moveSelectedProductionItem(-1); }
    static void dockFixture(MainWindow& w)
    {
        w.state_.orbitalStations.clear();
        w.state_.planets.front().productionQueue = {{ProductionKind::OrbitalStation, 3, 0}};
        w.rebuildScene();
        w.refreshProductionQueue();
    }
    static void freshDesigner(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.selectedStarId_ = w.state_.planets.front().star;
        w.refreshShipDesignChoices();
        w.rebuildScene();
        w.openShipDesigner();
    }
    static void queueDesign(MainWindow& w) { assert(w.buildShipButton_->isEnabled()); w.queueShipDesign(); w.refreshProductionQueue(); }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static bool canQueueDock(const MainWindow& w) { return w.buildOrbitalDockButton_->isEnabled(); }
};
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    window.installUiPolish();
    window.installProductionQueue();
    suns::MainWindowTestAccess::install(window);
    auto* tree = window.findChild<QTreeWidget*>("productionQueueTree");
    auto* remove = window.findChild<QPushButton*>("productionRemoveButton");
    assert(tree && remove && tree->topLevelItemCount() == 2);
    tree->setCurrentItem(tree->topLevelItem(1));
    suns::MainWindowTestAccess::moveUp(window);
    assert(tree->topLevelItem(0)->text(1) == "Mine");
    remove->click(); // newly queued and reordered mine
    assert(tree->topLevelItemCount() == 1 && tree->topLevelItem(0)->text(1) == "Factory");
    suns::MainWindowTestAccess::advance(window);
    assert(suns::MainWindowTestAccess::colony(window).productionQueue.size() == 1);
    auto* shortcut = tree->findChild<QShortcut*>();
    assert(shortcut && shortcut->key() == QKeySequence(Qt::Key_Delete));
    assert(shortcut->context() == Qt::WidgetWithChildrenShortcut);
    assert(QMetaObject::invokeMethod(shortcut, "activated", Qt::DirectConnection));
    assert(tree->topLevelItemCount() == 0 && !remove->isEnabled());
    suns::MainWindowTestAccess::advance(window);
    assert(suns::MainWindowTestAccess::colony(window).productionQueue.empty());
    suns::MainWindowTestAccess::dockFixture(window);
    assert(!suns::MainWindowTestAccess::canQueueDock(window));
    remove->click();
    assert(suns::MainWindowTestAccess::canQueueDock(window));

    suns::MainWindowTestAccess::freshDesigner(window);
    auto* catalog = window.findChild<QListWidget*>("shipComponentCatalog");
    auto* details = window.findChild<QLabel*>("shipComponentDetails");
    auto* name = window.findChild<QLineEdit*>("shipDesignName");
    auto* save = window.findChild<QPushButton*>("saveShipDesign");
    assert(catalog && details && name && save);
    assert(details->text().contains("W8:"));
    assert(details->text().contains("Minerals (kt)"));
    for (int row = 0; row < catalog->count(); ++row) {
        const auto component = static_cast<suns::ShipComponentType>(catalog->item(row)->data(Qt::UserRole).toInt());
        if (component == suns::ShipComponentType::PenetratingScanner) {
            catalog->setCurrentRow(row);
            assert(details->text().contains("Penetrating: surveys planets"));
            assert(details->text().contains("Locked — requires Electronics 3"));
            auto* fit = window.findChild<QPushButton*>("fitSelectedComponent");
            assert(fit && !fit->isEnabled());
        }
    }
    name->setText("Immediate Scout");
    save->click();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);
    suns::MainWindowTestAccess::queueDesign(window);
    assert(tree->topLevelItemCount() == 1);
    assert(tree->topLevelItem(0)->text(1) == "Immediate Scout");
    assert(tree->topLevelItem(0)->text(3).startsWith("Turn "));
    auto* minerals = window.findChild<QLabel*>("productionMineralDetails");
    assert(minerals && minerals->text().contains("I 6.0 / B 4.0 / G 5.0 kt"));
    const auto& queued = std::get<suns::QueueShipDesignOrder>(suns::MainWindowTestAccess::orders(window).orders.back());
    assert(queued.design == 0 && queued.pendingDesignName == "Immediate Scout");
    suns::MainWindowTestAccess::advance(window);
    assert(suns::MainWindowTestAccess::colony(window).productionQueue.size() == 1);
    assert(tree->topLevelItem(0)->text(1) == "Immediate Scout");
}
