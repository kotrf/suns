#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "suns/communications.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDockWidget>
#include <QEventLoop>
#include <QGraphicsView>
#include <QGroupBox>
#include <QProgressBar>
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTextBrowser>
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
        set_survey_level(w.state_,1,planet.star,SurveyLevel::DeepSurvey,w.state_.turn);
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
    static void localCargo(MainWindow& w)
    {
        setup(w);
        auto& fleet = w.state_.fleets.front();
        fleet.design = kColonyShipDesignId;
        fleet.ships = {{kColonyShipDesignId, 5}};
        fleet.role = FleetRole::ColonyShip;
        fleet.telemetry = {};
        fleet.telemetry = confirmed_fleet_telemetry(w.state_, fleet);
        w.rebuildScene();
    }
    static void verifyCargo(const MainWindow& w, std::uint64_t population, std::uint64_t aboard, double iron)
    {
        assert(w.state_.turn == 1 && w.state_.planets.front().population == 25000);
        assert(w.state_.fleets.front().colonists == 0 && w.state_.planets.front().minerals.ironium == 123);
        const auto planned = projected_cargo_order_state(make_player_view(w.state_, 1).state, w.pendingOrders_);
        assert(planned.planets.front().population == population && planned.fleets.front().colonists == aboard);
        assert(planned.planets.front().minerals.ironium == iron);
        auto physical = w.state_;
        physical.planets.front().productionQueue.clear();
        auto afterCargo = physical.planets.front();
        afterCargo.population = population;
        const auto resolved = TurnProcessor{}.process(physical, {w.pendingOrders_});
        assert(resolved.fleets.front().colonists == aboard);
        assert(resolved.planets.front().population == population + projected_population_growth(physical, afterCargo, physical.turn));
        assert(std::abs(resolved.planets.front().minerals.ironium - iron
            - projected_mineral_mining(physical, physical.planets.front()).ironium) < 0.000001);
    }
    static void manyOrders(MainWindow& w)
    {
        setup(w);
        for (int i = 1; i <= 150; ++i) {
            w.pendingOrders_.orders.push_back(SetFleetColonistsOrder{1, 1, 0});
            w.pendingDescriptions_ << QString("Order %1 — an empire-wide command with a long description for another system").arg(i);
        }
        w.rebuildScene();
    }
    static void environment(MainWindow& w, int temperature)
    {
        setup(w);
        w.state_.players.front().race.environmentBased = true;
        w.state_.planets.front().environment = {std::uint8_t(temperature), 50, 20};
        w.state_.planets.front().naturalEnvironment = w.state_.planets.front().environment;
        w.state_.players.front().surveyKnowledge.clear();
        set_survey_level(w.state_, 1, 1, SurveyLevel::DeepSurvey, w.state_.turn);
        w.rebuildScene();
    }
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
    window.installStrategicOperations();
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
    auto* research = window.findChild<QDialog*>("researchDialog");
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
    assert(!window.findChild<QDockWidget*>("researchDock"));
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
    // A visible table can still clip its last row when its pixel height is fixed.
    // Cover larger fonts and item padding rather than just the outer dock bounds.
    const auto originalFont = minerals->font();
    const auto originalStyle = minerals->styleSheet();
    window.resize(1366, 900);
    window.resetPanelLayout();
    for (int points : {11, 14, 18}) {
        auto font = originalFont;
        font.setPointSize(points);
        minerals->setFont(font);
        minerals->setStyleSheet("QTreeWidget::item { padding-top: 4px; padding-bottom: 4px; }");
        settle();
        for (int row = 0; row < 3; ++row) {
            const auto rect = minerals->visualItemRect(minerals->topLevelItem(row));
            assert(rect.isValid() && rect.top() >= 0 && rect.bottom() < minerals->viewport()->height());
        }
        assert(fullyVisible(minerals, overview));
    }
    minerals->setFont(originalFont);
    minerals->setStyleSheet(originalStyle);
    settle();

    MainWindowTestAccess::environment(window, 50);
    settle();
    auto* terraforming = window.findChild<QLabel*>("planetTerraformingPotential");
    assert(terraforming && terraforming->text() == "Hab 100%");
    MainWindowTestAccess::environment(window, 65);
    settle();
    assert(terraforming->text().contains("with terraforming"));

    // A full ship can return its queued colonists through the receiving slider,
    // including a mixed return/load. Both the main status and mineral stock must
    // agree with the cargo containers without resolving a year.
    MainWindowTestAccess::localCargo(window);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        dialog->findChild<QSpinBox*>("cargoColonistSpin")->setValue(2500);
        auto* apply = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        assert(apply->isEnabled());
        apply->click();
    });
    window.openCargoManifestDialog();
    settle();
    assert(status->text().contains("Population: 22500 /"));
    MainWindowTestAccess::verifyCargo(window, 22500, 2500, 123);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* people = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        assert(people->value() == 2500 && people->minimum() == 0);
        people->setValue(0);
        assert(dialog->findChild<QSpinBox*>("cargoColonistSpin")->value() == -2500);
        dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin0")->setValue(3);
        if (app.arguments().contains("--capture")) dialog->grab().save("/tmp/suns-cargo-return.png");
        auto* apply = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        assert(apply->isEnabled());
        apply->click();
    });
    window.openCargoManifestDialog();
    settle();
    assert(status->text().contains("Population: 25000 /"));
    assert(minerals->topLevelItem(0)->text(1) == "120.0");
    MainWindowTestAccess::verifyCargo(window, 25000, 0, 120);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* iron = dialog->findChild<QSlider*>("cargoDestinationMineralSlider0");
        assert(iron->value() == 300);
        iron->setValue(0);
        auto* apply = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        assert(apply->isEnabled());
        apply->click();
    });
    window.openCargoManifestDialog();
    settle();
    assert(minerals->topLevelItem(0)->text(1) == "123.0");
    MainWindowTestAccess::verifyCargo(window, 25000, 0, 123);
    const auto acceptedCount = MainWindowTestAccess::orders(window).orders.size();
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        dialog->findChild<QSpinBox*>("cargoColonistSpin")->setValue(500);
        dialog->reject();
    });
    window.openCargoManifestDialog();
    assert(MainWindowTestAccess::orders(window).orders.size() == acceptedCount);
    MainWindowTestAccess::verifyCargo(window, 25000, 0, 123);

    MainWindowTestAccess::manyOrders(window);
    auto* ordersAction = window.findChild<QAction*>("openTurnOrdersAction");
    auto* ordersWindow = window.findChild<QDialog*>("turnOrdersDialog");
    auto* ordersSummary = window.findChild<QTextBrowser*>("turnOrdersSummary");
    auto* viewMenu = window.findChild<QMenu*>("sunsViewMenu");
    assert(ordersAction && ordersWindow && ordersSummary && viewMenu);
    assert(viewMenu->actions().contains(ordersAction));
    assert(!details->isAncestorOf(ordersSummary) && !ordersWindow->isVisible());
    ordersAction->trigger();
    settle();
    assert(ordersWindow->isVisible() && ordersSummary->isVisible() && ordersSummary->toPlainText().contains("Order 150"));
    if (app.arguments().contains("--capture")) ordersWindow->grab().save("/tmp/suns-turn-orders.png");
    assert(ordersSummary->verticalScrollBar()->maximum() > 0);
    ordersSummary->verticalScrollBar()->setValue(ordersSummary->verticalScrollBar()->maximum());
    const auto ordersPosition = ordersSummary->verticalScrollBar()->value();
    settle();
    assert(ordersSummary->verticalScrollBar()->value() == ordersPosition);
    ordersWindow->close();
    ordersAction->trigger();
    settle();
    assert(window.findChildren<QDialog*>("turnOrdersDialog").size() == 1);
    ordersWindow->close();
    MainWindowTestAccess::setup(window);
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
    assert(QSettings("SunsProject", "Suns").value("workspace/customVersion").toInt() == 6);
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
    assert(restored.restoreState(settings.value("workspace/docks").toByteArray(), 6));
    assert(restored.findChild<QDockWidget*>("systemDetailsDock")->isHidden());
    assert(!restored.findChild<QDockWidget*>("productionDock")->isHidden());
}
