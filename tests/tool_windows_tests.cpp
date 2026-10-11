#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "ship_designer_dialog.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>

#include <algorithm>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static ShipDesignerDialog* designer(const MainWindow& w) { return w.shipDesigner_; }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static void nextTurn(MainWindow& w) { w.endTurn(); }
    static std::uint64_t latestHistoryTurn(const MainWindow& w)
    {
        return w.state_.players.front().history.back().turn;
    }
    static PlanetId firstColony(const MainWindow& w)
    {
        const auto colony = std::find_if(w.state_.planets.begin(), w.state_.planets.end(),
            [&](const auto& planet) { return planet.owner == w.pendingOrders_.player; });
        assert(colony != w.state_.planets.end());
        return colony->id;
    }
};
} // namespace suns

static void settle()
{
    QEventLoop loop;
    QTimer::singleShot(50, &loop, &QEventLoop::quit);
    loop.exec();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

static void installWorkspace(suns::MainWindow& window)
{
    suns::attachRouteProgramDock(window);
    window.installUiPolish();
    window.installMapDisplayModes();
    window.installPlanetPolish();
    window.installProductionQueue();
    window.installFleetReadabilityPolish();
    window.installFleetPortraitPolish();
    window.installMiningInfrastructure();
    window.installCommunicationStatus();
    window.installTurnMessages();
    window.installResearch();
    window.installEmpireHistory();
    window.installPanelLayoutFixes();
}

int main(int argc, char** argv)
{
    using namespace suns;
    QApplication app(argc, argv);
    QTemporaryDir settingsDir;
    assert(settingsDir.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
    {
        // Reproduce a saved layout with History floating and a hidden colony
        // panel. The new workspace must not restore either old arrangement.
        QMainWindow oldWindow;
        auto* oldHistory = new QDockWidget("Empire History", &oldWindow);
        oldHistory->setObjectName("empireHistoryDock");
        oldWindow.addDockWidget(Qt::BottomDockWidgetArea, oldHistory);
        oldHistory->setFloating(true);
        auto* production = new QDockWidget("Production", &oldWindow);
        production->setObjectName("productionDock");
        oldWindow.addDockWidget(Qt::LeftDockWidgetArea, production);
        production->hide();
        QSettings settings("SunsProject", "Suns");
        settings.setValue("workspace/docks", oldWindow.saveState(5));
        settings.setValue("workspace/customDocks", oldWindow.saveState(5));
        settings.setValue("workspace/customVersion", 5);
    }

    MainWindow window;
    installWorkspace(window);
    window.show();
    settle();
    assert(!window.findChild<QDockWidget*>("empireHistoryDock"));
    assert(window.findChild<QDockWidget*>("productionDock")->isVisible());
    assert(!window.findChild<QAction*>("restoreCustomWorkspaceAction")->isEnabled());
    assert(!window.findChild<QAction*>("designWorkspaceAction"));
    const auto dockState = window.saveState(6);

    auto* designerTool = window.findChild<QAction*>("openShipDesignerToolAction");
    auto* designerMenu = window.findChild<QAction*>("openShipDesignerAction");
    auto* historyTool = window.findChild<QAction*>("openEmpireHistoryToolAction");
    auto* historyMenu = window.findChild<QAction*>("openEmpireHistoryAction");
    auto* toolbar = window.findChild<QToolBar*>("dialogToolbar");
    auto* viewMenu = window.menuBar()->findChild<QMenu*>("sunsViewMenu");
    assert(designerTool && designerMenu && historyTool && historyMenu);
    assert(toolbar->actions().contains(historyTool) && viewMenu->actions().contains(historyMenu));
    assert(!historyMenu->isCheckable());

    designerTool->trigger();
    settle();
    QPointer<ShipDesignerDialog> first = MainWindowTestAccess::designer(window);
    assert(first && first->isWindow() && first->isVisible() && !first->isModal());
    first->findChild<QLineEdit*>("shipDesignName")->setText("Keep this draft");
    designerMenu->trigger();
    assert(MainWindowTestAccess::designer(window) == first);
    assert(first->findChild<QLineEdit*>("shipDesignName")->text() == "Keep this draft");
    first->showMinimized();
    designerTool->trigger();
    assert(first->isVisible() && !first->isMinimized());
    assert(window.saveState(6) == dockState);

    // Reopen before Qt delivers deferred deletion of the old dialog. Its
    // destruction must not erase the new live draft or open extra windows.
    first->reject();
    designerTool->trigger();
    auto* second = MainWindowTestAccess::designer(window);
    assert(second && second != first && second->isVisible());
    settle();
    assert(!first);
    designerMenu->trigger();
    assert(MainWindowTestAccess::designer(window) == second);
    assert(window.findChildren<QDialog*>("shipDesignerDialog").size() == 1);
    second->findChild<QLineEdit*>("shipDesignName")->setText("Separate window scout");
    auto* save = second->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save);
    assert(save->isEnabled());
    if (app.arguments().contains("--capture")) second->grab().save("/tmp/suns-designer-window.png");
    save->click();
    assert(!MainWindowTestAccess::designer(window));
    const auto& orders = MainWindowTestAccess::orders(window).orders;
    assert(std::any_of(orders.begin(), orders.end(), [](const auto& order) {
        const auto* design = std::get_if<CreateShipDesignOrder>(&order);
        return design && design->name == "Separate window scout";
    }));
    settle();

    historyTool->trigger();
    settle();
    auto* history = window.findChild<QDialog*>("empireHistoryDialog");
    assert(history && history->isWindow() && history->isVisible() && !history->isModal());
    assert(!QApplication::activeModalWidget());
    auto* metric = history->findChild<QComboBox*>("historyMetric");
    auto* scope = history->findChild<QComboBox*>("historyScope");
    auto* compare = history->findChild<QComboBox*>("historyCompare");
    metric->setCurrentIndex(3);
    const auto colonyIndex = scope->findData(static_cast<quint32>(MainWindowTestAccess::firstColony(window)));
    assert(colonyIndex >= 0);
    scope->setCurrentIndex(colonyIndex);
    compare->setCurrentIndex(compare->findData(0));
    const auto scopeId = scope->currentData();
    const auto comparisonId = compare->currentData();
    historyMenu->trigger();
    assert(window.findChildren<QDialog*>("empireHistoryDialog").size() == 1);
    history->showMinimized();
    historyMenu->trigger();
    assert(history->isVisible() && !history->isMinimized());
    history->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close)->click();
    assert(history->isHidden());
    const auto previousYear = history->findChild<QSpinBox*>("historyLastTurn")->value();
    // Advance while the window is closed, then reopen current observations
    // with the same explicit colony and comparison filters.
    MainWindowTestAccess::nextTurn(window);
    MainWindowTestAccess::nextTurn(window);
    const auto latest = MainWindowTestAccess::latestHistoryTurn(window);
    assert(latest > static_cast<std::uint64_t>(previousYear));
    historyTool->trigger();
    settle();
    assert(window.findChild<QDialog*>("empireHistoryDialog") == history);
    assert(metric->currentIndex() == 3 && scope->currentData() == scopeId);
    assert(compare->currentData() == comparisonId);
    assert(history->findChild<QLabel*>("historySummary")->text().contains(
        QString("latest: %1").arg(static_cast<qulonglong>(latest))));
    assert(history->findChild<QSpinBox*>("historyLastTurn")->value() == static_cast<int>(latest));
    assert(window.saveState(6) == dockState);
    if (app.arguments().contains("--capture")) history->grab().save("/tmp/suns-history-window.png");

    // Window visibility is independent of panel presets, resets and the
    // temporary fullscreen map. The tools never become dock tabs.
    window.resetPanelLayout();
    window.findChild<QAction*>("empireWorkspaceAction")->trigger();
    assert(history->isVisible());
    window.setMapExpanded(true);
    settle();
    assert(history->isVisible());
    window.setMapExpanded(false);
    settle();
    assert(history->isVisible() && !window.findChild<QDockWidget*>("empireHistoryDock"));
    window.findChild<QAction*>("saveCustomWorkspaceAction")->trigger();
    history->close();
    window.findChild<QAction*>("restoreCustomWorkspaceAction")->trigger();
    assert(history->isHidden());
    window.findChild<QAction*>("empireWorkspaceAction")->trigger();
    assert(history->isHidden());
    window.close();

    MainWindow restored;
    installWorkspace(restored);
    restored.show();
    settle();
    assert(!restored.findChild<QDockWidget*>("empireHistoryDock"));
    assert(restored.findChild<QDialog*>("empireHistoryDialog")->isHidden());
    assert(restored.findChild<QAction*>("restoreCustomWorkspaceAction")->isEnabled());
    restored.findChild<QAction*>("openEmpireHistoryAction")->trigger();
    assert(restored.findChild<QDialog*>("empireHistoryDialog")->isVisible());
    restored.close();
}
