#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QToolButton>

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
        w.selection_.star = w.state_.planets.front().star;
        w.rebuildScene();
        w.refreshProductionQueue();
    }
    static void advance(MainWindow& w)
    {
        w.endTurn();
        w.selection_.star = w.state_.planets.front().star;
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
    static void planFreshDesign(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star;
        w.appendPendingOrder(CreateShipDesignOrder{
            "Immediate Scout",
            ShipHullType::Scout,
            {ShipComponentType::FusionDrive, ShipComponentType::LongRangeScanner},
        }, "Save ship design Immediate Scout");
        w.refreshShipDesignChoices();
        w.shipDesignCombo_->setCurrentIndex(w.shipDesignCombo_->findData(w.state_.nextShipDesignId));
        w.updateControls();
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

    {
        suns::ShipDesignerDialog designer(suns::make_demo_game(), 1);
        auto* catalog = designer.findChild<QListWidget*>("shipComponentCatalog");
        auto* details = designer.findChild<QLabel*>("shipComponentDetails");
        assert(catalog && details);
        assert(details->text().contains("W8:"));
        assert(details->text().contains("Minerals (kt)"));
        for (int row = 0; row < catalog->count(); ++row) {
            const auto component = static_cast<suns::ShipComponentType>(catalog->item(row)->data(Qt::UserRole).toInt());
            if (component == suns::ShipComponentType::PenetratingScanner) {
                catalog->setCurrentRow(row);
                assert(details->text().contains("Penetrating: surveys planets"));
                assert(details->text().contains("Locked — requires Electronics 3"));
                auto* fit = designer.findChild<QPushButton*>("fitSelectedComponent");
                assert(fit && !fit->isEnabled());
            }
        }
    }

    {
        // Exercise an actual press/release on a fitted cell. QToolButton::click()
        // skips the mouse handlers that run in the user's designer.
        suns::ShipDesignerDialog designer(suns::make_demo_game(), 1);
        auto* hull = designer.findChild<QComboBox*>("shipHullCatalog");
        assert(hull);
        hull->setCurrentIndex(hull->findData(static_cast<int>(suns::ShipHullType::Utility)));
        designer.show();
        QApplication::processEvents();
        auto* first = designer.findChild<QToolButton*>("shipSlot_100");
        auto* second = designer.findChild<QToolButton*>("shipSlot_101");
        auto* remove = designer.findChild<QPushButton*>("removeSelectedComponent");
        assert(first && second && remove);
        const auto mouseClick = [](QToolButton* button) {
            const QPointF local(button->rect().center());
            const QPointF global(button->mapToGlobal(local.toPoint()));
            QMouseEvent press(QEvent::MouseButtonPress, local, global,
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(button, &press);
            QMouseEvent release(QEvent::MouseButtonRelease, local, global,
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(button, &release);
        };
        mouseClick(second);
        assert(second->isChecked() && !first->isChecked() && remove->isEnabled());
        mouseClick(second);
        assert(second->isChecked());
        mouseClick(first);
        assert(first->isChecked() && !second->isChecked());
    }

    {
        auto state = suns::make_demo_game();
        const suns::ShipDesign archived{
            state.nextShipDesignId++, 1, "Archive", suns::ShipHullType::Scout,
            {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FuelTank,
                suns::ShipComponentType::LongRangeScanner},
            {{100, suns::ShipComponentType::FusionDrive},
                {200, suns::ShipComponentType::FuelTank},
                {201, suns::ShipComponentType::LongRangeScanner}},
        };
        state.shipDesigns.push_back(archived);
        auto foreign = archived;
        foreign.id = state.nextShipDesignId++;
        foreign.owner = 2;
        foreign.name = "Hidden design";
        state.shipDesigns.push_back(foreign);
        suns::ShipDesignerDialog designer(state, 1);
        auto* source = designer.findChild<QComboBox*>("shipDesignTemplate");
        assert(source && source->findData(archived.id) >= 0);
        assert(source->findData(foreign.id) < 0);
        source->setCurrentIndex(source->findData(archived.id));
        const auto draft = designer.draft();
        assert(draft.name == "Archive Copy");
        assert(draft.hull == archived.hull);
        assert(draft.components == archived.components);
        assert(draft.placements == archived.placements);
        assert(state.shipDesigns[state.shipDesigns.size() - 2].name == "Archive");
    }

    suns::MainWindowTestAccess::planFreshDesign(window);
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
