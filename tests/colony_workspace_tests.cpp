#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QEventLoop>
#include <QGraphicsView>
#include <QGroupBox>
#include <QProgressBar>
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cassert>
#include <cmath>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        auto& planet = w.state_.planets.front();
        planet.population = 25000;
        planet.industry = 10;
        planet.mines = 10;
        planet.minerals = {123, 45, 67};
        for (int i = 0; i < 8; ++i) planet.productionQueue.push_back({ProductionKind::Factory, 50, 0});
        w.selection_.star = 1;
        w.selection_.fleet = 1;
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.rebuildScene();
        w.refreshProductionQueue();
        w.refreshColonyWorkspace();
    }
    static void unknown(MainWindow& w)
    {
        w.selection_.star = 2;
        w.state_.planets[1].owner = 2;
        w.state_.planets[1].population = 999999;
        w.state_.planets[1].minerals = {9876, 8765, 7654};
        w.state_.players.front().surveyedStars.clear();
        w.state_.players.front().surveyKnowledge.clear();
        w.rebuildScene();
        w.refreshProductionQueue();
        w.refreshColonyWorkspace();
    }
    static void deselect(MainWindow& w)
    {
        w.selection_.star.reset();
        w.rebuildScene();
        w.refreshProductionQueue();
        w.refreshColonyWorkspace();
    }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
};
} // namespace suns

static void settle()
{
    QEventLoop loop;
    QTimer::singleShot(220, &loop, &QEventLoop::quit);
    loop.exec();
}

static bool fullyVisible(QWidget* widget, QWidget* container)
{
    return widget->isVisible() && container->rect().contains(QRect(widget->mapTo(container, QPoint{}), widget->size()));
}

static void key(QWidget* widget, int code)
{
    QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier);
    QApplication::sendEvent(widget, &release);
    settle();
}

static void installWorkspace(suns::MainWindow& window)
{
    using namespace suns;
    window.installDeferredMapSelectionHandler();
    attachRouteProgramDock(window);
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
    window.installWormholes();
    window.installPanelLayoutFixes();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settingsDirectory;
    assert(settingsDirectory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDirectory.path());
    // A v4 workspace must acquire the second details column on first launch.
    {
        QSettings settings("SunsProject", "Suns");
        suns::MainWindow legacy;
        legacy.installUiPolish();
        legacy.findChild<QDockWidget*>("productionDock")->hide();
        settings.setValue("workspace/docks", legacy.saveState(4));
    }
    using namespace suns;
    MainWindow window;
    installWorkspace(window);
    MainWindowTestAccess::setup(window);
    window.show();
    settle();
    auto* overview = window.findChild<QDockWidget*>("overviewDock");
    auto* production = window.findChild<QDockWidget*>("productionDock");
    auto* details = window.findChild<QDockWidget*>("systemDetailsDock");
    auto* detailsScroll = window.findChild<QScrollArea*>("systemDetailsScrollArea");
    auto* planetInfo = window.findChild<QLabel*>("selectedPlanetDetailsSummary");
    auto* temperature = window.findChild<QProgressBar*>("planetTemperatureBar");
    auto* geology = window.findChild<QProgressBar*>("ironiumConcentration");
    auto* fleet = window.findChild<QDockWidget*>("fleetDock");
    auto* reports = window.findChild<QDockWidget*>("turnMessagesDock");
    auto* research = window.findChild<QDockWidget*>("researchDock");
    auto* status = window.findChild<QLabel*>("colonyStatusSummary");
    auto* minerals = window.findChild<QTreeWidget*>("colonyMineralsTable");
    auto* queue = window.findChild<QTreeWidget*>("productionQueueTree");
    auto* view = window.findChild<QGraphicsView*>();
    auto* expand = window.findChild<QAction*>("expandMapAction");
    assert(details && detailsScroll && planetInfo && temperature && geology);
    assert(details->isAncestorOf(planetInfo) && details->isAncestorOf(temperature) && details->isAncestorOf(geology));
    assert(!window.findChild<QDialog*>("systemDetailsDialog"));
    auto* detailsColumn = qobject_cast<QVBoxLayout*>(detailsScroll->widget()->layout());
    assert(detailsColumn && detailsColumn->itemAt(0)->widget() == window.findChild<QGroupBox*>("planetGroup"));
    assert(overview && production && fleet && reports && research && status && minerals && queue && view && expand);
    assert(!window.findChild<QScrollArea*>("productionScrollArea"));
    assert(!window.tabifiedDockWidgets(production).contains(research));
    assert(window.corner(Qt::BottomLeftCorner) == Qt::LeftDockWidgetArea);
    assert(status->text().contains("25000") && status->text().contains("Resources:"));
    assert(minerals->topLevelItem(0)->text(1) == "123.0");
    assert(minerals->topLevelItem(1)->text(1) == "45.0");
    assert(queue->topLevelItemCount() == 8);
    for (const auto size : {QSize(1366, 900), QSize(1000, 700)}) {
        window.resize(size);
        window.resetPanelLayout();
        settle();
        if (app.arguments().contains("--capture")) window.grab().save(
            size.width() == 1366 ? "/tmp/suns-system-details-column.png" : "/tmp/suns-system-details-column-small.png");
        assert(window.width() >= window.minimumSizeHint().width());
        assert(fullyVisible(status, overview) && fullyVisible(minerals, overview));
        assert(fullyVisible(queue, production));
        assert(queue->height() >= 96 && queue->visualItemRect(queue->topLevelItem(0)).isValid());
        assert(production->isVisible() && overview->isVisible() && details->isVisible());
        assert(window.tabifiedDockWidgets(details).isEmpty());
        assert(overview->geometry().right() < details->geometry().left());
        assert(production->geometry().right() < details->geometry().left());
        const auto mapRect = QRect(view->mapTo(&window, QPoint{}), view->size());
        assert(details->geometry().right() < mapRect.left());
        assert(details->geometry().top() == overview->geometry().top());
        assert(details->geometry().bottom() >= production->geometry().bottom());
        assert(view->viewport()->width() >= (size.width() == 1366 ? 300 : 100));
        assert(planetInfo->text().contains("Earth") && temperature->isVisible());
        assert(fullyVisible(reports, &window) && reports->geometry().right() < fleet->geometry().left());
        auto* mapToolbar = window.findChild<QToolBar*>("mapViewToolbar");
        assert(fullyVisible(mapToolbar->widgetForAction(expand), &window));
        auto* mode = window.findChild<QComboBox*>("mapDisplayModeCombo");
        assert(fullyVisible(mode, &window) && mode->width() >= 100);
    }
    window.findChild<QAction*>("openResearchToolAction")->trigger();
    settle();
    assert(fullyVisible(queue, production) && fullyVisible(minerals, overview));
    research->hide();
    // The periodic refresh must not pull a long queue back to its first row.
    queue->setCurrentItem(queue->topLevelItem(0));
    auto* scroll = queue->verticalScrollBar();
    scroll->setValue(scroll->maximum());
    const auto queueScroll = scroll->value();
    assert(queueScroll > 0);
    settle();
    assert(scroll->value() == queueScroll && queue->currentItem() == queue->topLevelItem(0));
    // The details column is visible without a button or popup. It can be
    // hidden and restored using the same View toggle as other docks.
    details->toggleViewAction()->trigger();
    assert(details->isHidden());
    details->toggleViewAction()->trigger();
    settle();
    assert(details->isVisible() && planetInfo->isVisible());
    assert(fullyVisible(queue, production) && fullyVisible(minerals, overview));

    // Production controls still operate on the same persistent queue.
    queue->setCurrentItem(queue->topLevelItem(7));
    window.findChild<QPushButton*>("productionRemoveButton")->click();
    settle();
    assert(queue->topLevelItemCount() == 7 && !MainWindowTestAccess::orders(window).orders.empty());

    // Round-trip a deliberately customized workspace, including a floating
    // panel and a hidden report. Fullscreen must be temporary.
    window.resize(1000, 700);
    window.resetPanelLayout();
    reports->hide();
    fleet->setFloating(true);
    fleet->resize(410, 600);
    fleet->show();
    settle();
    window.findChild<QAction*>("saveCustomWorkspaceAction")->trigger();
    assert(QSettings("SunsProject", "Suns").value("workspace/customVersion").toInt() == 5);
    details->hide();
    window.findChild<QAction*>("restoreCustomWorkspaceAction")->trigger();
    settle();
    assert(details->isVisible() && fleet->isFloating() && !reports->isVisible());
    view->resetTransform();
    view->scale(4, 4);
    view->centerOn(12, -9);
    const auto center = view->mapToScene(view->viewport()->rect().center());
    const auto zoom = view->transform().m11();
    const auto previousState = window.windowState();
    expand->trigger();
    settle();
    assert(window.isFullScreen() && expand->isChecked());
    assert(!overview->isVisible() && !production->isVisible() && !details->isVisible() && !fleet->isVisible());
    assert(window.findChild<QToolBar*>("mapViewToolbar")->isVisible());
    assert(window.findChild<QToolBar*>("mapDisplayToolbar")->isVisible());
    assert(!window.findChild<QToolBar*>("dialogToolbar")->isVisible());
    assert(!window.menuBar()->isVisible() && !window.statusBar()->isVisible());
    assert(view->transform().m11() == zoom);
    if (app.arguments().contains("--capture")) window.grab().save("/tmp/suns-map-fullscreen.png");
    window.activateWindow();
    view->setFocus();
    settle();
    key(view, Qt::Key_Escape);
    assert(!expand->isChecked() && window.windowState() == previousState);
    assert(overview->isVisible() && production->isVisible() && details->isVisible() && fleet->isVisible() && fleet->isFloating());
    assert(!reports->isVisible());
    assert(window.menuBar()->isVisible() && window.statusBar()->isVisible());
    assert(view->transform().m11() == zoom);
    const auto restoredCenter = view->mapToScene(view->viewport()->rect().center());
    assert(std::abs(restoredCenter.x() - center.x()) < 3 && std::abs(restoredCenter.y() - center.y()) < 3);
    key(view, Qt::Key_F11);
    assert(expand->isChecked());
    key(view, Qt::Key_F11);
    assert(!expand->isChecked());
    expand->trigger();
    window.resetPanelLayout();
    settle();
    assert(!expand->isChecked() && !fleet->isFloating());
    window.findChild<QAction*>("fleetWorkspaceAction")->trigger();
    settle();
    assert(overview->isVisible() && production->isVisible() && details->isVisible() && fleet->isVisible());

    MainWindowTestAccess::unknown(window);
    settle();
    assert(planetInfo->text().contains("never scanned") && !planetInfo->text().contains("999999"));
    assert(!planetInfo->text().contains("9876") && !temperature->isVisible() && !geology->isEnabled());
    assert(!status->text().contains("999999"));
    assert(status->text().contains("never scanned"));
    for (int row = 0; row < 3; ++row) {
        assert(minerals->topLevelItem(row)->text(1) == "—");
        assert(minerals->topLevelItem(row)->text(3) == "?");
    }
    assert(queue->topLevelItemCount() == 0);
    MainWindowTestAccess::deselect(window);
    settle();
    assert(planetInfo->text().contains("No system selected"));
    assert(status->text().contains("No system selected"));
    details->hide();
    expand->trigger();
    window.close(); // Save the normal workspace, rather than fullscreen-hidden docks.
    QSettings settings("SunsProject", "Suns");
    MainWindow restored;
    installWorkspace(restored);
    assert(restored.restoreState(settings.value("workspace/docks").toByteArray(), 5));
    assert(restored.findChild<QDockWidget*>("systemDetailsDock")->isHidden());
    assert(!restored.findChild<QDockWidget*>("productionDock")->isHidden());
}
