#include "main_window.hpp"
#include "galaxy_setup_widget.hpp"
#include "route_program_dock.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QDialog>
#include <QEventLoop>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& window)
    {
        auto other = window.state_.fleets.front();
        other.id = window.state_.nextFleetId++;
        other.name = "Second fleet";
        other.position = {50, 0};
        other.telemetry.position = other.position;
        other.fuel = 17;
        other.telemetry.fuel = 17;
        window.state_.fleets.push_back(other);
        window.selection_.fleet = 1;
        window.selection_.star = 2;
        window.rebuildScene();
        emit window.routeProgramContextChanged(true);
    }
    static const PlayerOrders& orders(MainWindow& window) { return window.pendingOrders_; }
    static const GameState& state(MainWindow& window) { return window.state_; }
    static const GalaxyConfig& config(MainWindow& window) { return window.galaxyConfig_; }
};
} // namespace suns

static void settle()
{
    QEventLoop loop;
    QTimer::singleShot(230, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settingsDirectory;
    assert(settingsDirectory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDirectory.path());
    using namespace suns;
    // Both setup dialogs share this widget. Preset changes update counts and
    // dimensions together; an existing custom save is represented losslessly.
    GalaxySetupWidget setup(GalaxyConfig{});
    auto* size = setup.findChild<QComboBox*>("galaxySizeCombo");
    auto* density = setup.findChild<QComboBox*>("galaxyDensityCombo");
    auto* systems = setup.findChild<QSpinBox*>("galaxySystemsSpin");
    assert(size && density && systems);
    assert(size->currentIndex() == 0 && density->currentIndex() == 1 && systems->value() == 32);
    size->setCurrentIndex(4);
    density->setCurrentIndex(3);
    assert(systems->value() == 1000);
    assert(setup.config(777).seed == 777 && setup.config(777).width == 2000);
    GalaxySetupWidget custom({42, 24, 940, 700, 50});
    assert(custom.findChild<QComboBox*>("galaxySizeCombo")->currentIndex() == 5);
    assert(custom.config(42).width == 940 && custom.config(42).height == 700);
    assert(custom.config(42).starCount == 24 && custom.config(42).minimumSeparation == 50);
    GalaxySetupWidget smallCustom({42, 2, 200, 100, 15});
    assert(smallCustom.config(42).starCount == 2);

    MainWindow window;
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
    MainWindowTestAccess::setup(window);
    window.resize(1300, 840);
    window.show();
    window.resetPanelLayout();
    settle();

    auto* fleet = window.findChild<QDockWidget*>("fleetDock");
    auto* header = window.findChild<QWidget*>("fleetOperationsHeader");
    auto* fuel = window.findChild<QProgressBar*>("fleetFuelBar");
    auto* cargo = window.findChild<QProgressBar*>("fleetCargoBar");
    auto* add = window.findChild<QPushButton*>("routeAddButton");
    auto* scroll = window.findChild<QScrollArea*>("fleetOrdersScrollArea");
    auto* details = window.findChild<QToolButton*>("fleetDetailsButton");
    auto* source = window.findChild<QComboBox*>("routeSourceFleetCombo");
    assert(fleet && header && fuel && cargo && add && scroll && details && source);
    assert(!window.findChild<QDockWidget*>("fleetRouteProgramDock"));
    assert(window.tabifiedDockWidgets(fleet).isEmpty());
    assert(window.findChild<QComboBox*>("routeTargetTypeCombo")->isHidden());
    assert(window.findChild<QComboBox*>("routeTargetFleetCombo")->isHidden());
    assert(header->isAncestorOf(fuel) && header->isAncestorOf(cargo));
    auto* map = window.findChild<QGraphicsView*>();
    assert(map && map->viewport()->width() >= 300);
    assert(map->transform().m11() > 0.1);
    assert(window.findChild<QScrollArea*>("productionScrollArea"));
    assert(fleet->isAncestorOf(add) && add->isVisible());
    assert(scroll->viewport()->rect().contains(QRect(add->mapTo(scroll->viewport(), QPoint{}), add->size())));
    // Refresh connections survive destroying the old dock, and the source
    // selector changes the same fleet that supplies the pinned gauges.
    source->setCurrentIndex(source->findData(quint32(2)));
    settle();
    assert(window.selectedFleetForRouteProgram() == 2);
    assert(fuel->format().contains("17.0"));
    add->click();
    settle();
    assert(MainWindowTestAccess::orders(window).orders.size() == 1);
    assert(std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front()).fleet == 2);
    assert(window.findChild<QTreeWidget*>("routeProgramQueue")->topLevelItemCount() == 1);
    if (app.arguments().contains("--capture")) window.grab().save("/tmp/suns-fleet-1300.png");

    window.resize(1000, 700);
    settle();
    details->click();
    settle();
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    QApplication::processEvents();
    assert(fuel->isVisible() && cargo->isVisible() && source->isVisible());
    assert(fleet->rect().contains(QRect(fuel->mapTo(fleet, QPoint{}), fuel->size())));
    assert(header->rect().contains(QRect(source->mapTo(header, QPoint{}), source->size())));
    details->click();
    scroll->verticalScrollBar()->setValue(0);
    settle();
    if (app.arguments().contains("--capture")) window.grab().save("/tmp/suns-fleet-1000.png");
    // Preset and reset actions must never resurrect an empty route tab.
    window.findChild<QAction*>("mapWorkspaceAction")->trigger();
    window.findChild<QAction*>("fleetWorkspaceAction")->trigger();
    settle();
    assert(fleet->isVisible() && fuel->isVisible() && add->isVisible());
    assert(!window.findChild<QDockWidget*>("fleetRouteProgramDock"));
    window.findChild<QPushButton*>("routePickTargetButton")->click();
    assert(window.routeProgramMapTargetPickActive());
    fleet->hide();
    assert(!window.routeProgramMapTargetPickActive());
    // Exercise the real menu dialog, generation and saved configuration, not
    // just the setup widget's independently computed preview.
    QTimer::singleShot(0, &window, [] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        dialog->findChild<QComboBox*>("galaxySizeCombo")->setCurrentIndex(1);
        dialog->findChild<QComboBox*>("galaxyDensityCombo")->setCurrentIndex(1);
        dialog->findChild<QLineEdit*>()->setText("777");
        dialog->accept();
    });
    window.findChild<QAction*>("newGalaxyAction")->trigger();
    settle();
    assert(MainWindowTestAccess::state(window).stars.size() == 128);
    assert(MainWindowTestAccess::state(window).galaxySeed == 777);
    assert(MainWindowTestAccess::config(window).width == 800);
    assert(MainWindowTestAccess::state(window).wormholeRules.width == 800);
    assert(MainWindowTestAccess::state(window).fleets.front().warp == 5);
    window.close();
}
