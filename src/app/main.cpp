#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    window.installDeferredMapSelectionHandler();
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
    window.installPanelLayoutFixes();

    window.show();

    if (app.arguments().contains("--smoke-test")) {
        QTimer::singleShot(0, &window, [&window] {
            window.showMaximized();
            if (auto* scene = window.routeProgramScene()) {
                for (auto* item : scene->items()) {
                    if (item->data(1).toInt() == 1) {
                        item->setSelected(true);
                        break;
                    }
                }
            }
            if (auto* mode = window.findChild<QComboBox*>("mapDisplayModeCombo")) {
                mode->setCurrentIndex(1);
                mode->setCurrentIndex(2);
                mode->setCurrentIndex(0);
            }
            // Exercise an intentionally disturbed dock width and the same reset
            // action exposed to players through View → Reset panel layout.
            if (auto* dock = window.findChild<QDockWidget*>("fleetRouteProgramDock")) {
                window.resizeDocks({dock}, {500}, Qt::Horizontal);
            }
            if (auto* reset = window.findChild<QAction*>("resetPanelLayoutAction")) {
                reset->trigger();
            }

            // Touch the dashboard gauges plus generated portrait/mining widgets
            // in the offscreen path; all refresh from the selected game state.
            if (auto* fuel = window.findChild<QProgressBar*>("fleetFuelBar")) fuel->update();
            if (auto* cargo = window.findChild<QProgressBar*>("fleetCargoBar")) cargo->update();
            if (auto* temperature = window.findChild<QProgressBar*>("planetTemperatureBar")) temperature->update();
            if (auto* routeSource = window.findChild<QComboBox*>("routeSourceFleetCombo")) routeSource->update();
            if (auto* routeHelp = window.findChild<QToolButton*>("routeProgramHelpButton")) routeHelp->update();
            if (auto* clearMessages = window.findChild<QPushButton*>("clearTurnMessages")) clearMessages->click();
            if (auto* portrait = window.findChild<QLabel*>("fleetPortrait")) portrait->update();
            if (auto* mine = window.findChild<QPushButton*>("queueMineButton")) mine->update();
            if (auto* comm = window.findChild<QLabel*>("fleetCommunicationSummary")) comm->update();
            if (auto* messages = window.findChild<QDockWidget*>("turnMessagesDock")) messages->update();
            if (auto* research = window.findChild<QDialog*>("researchDialog")) research->update();

            // Open the non-modal graphical Ship Designer, select an empty
            // general slot and fit a Fuel Tank through the keyboard-accessible
            // button path. Drag/drop uses the same placement operation.
            if (auto* openDesigner = window.findChild<QPushButton*>("openShipDesignerButton")) {
                openDesigner->click();
                if (auto* hull = window.findChild<QComboBox*>("shipHullCatalog")) {
                    hull->setCurrentIndex(hull->findData(static_cast<int>(suns::ShipHullType::Utility)));
                }
                if (auto* catalog = window.findChild<QListWidget*>("shipComponentCatalog")) {
                    catalog->setCurrentRow(10); // Fuel Tank
                }
                if (auto* slot = window.findChild<QToolButton*>("shipSlot_201")) slot->click();
                if (auto* fit = window.findChild<QPushButton*>("fitSelectedComponent")) fit->click();
                if (auto* save = window.findChild<QPushButton*>("saveShipDesign")) save->update();
            }
        });
        // Let deferred selection redraw, map-mode restyling, portraits, mining,
        // panel recovery, gauges and route-program timers run.
        QTimer::singleShot(350, &window, [&window] { window.close(); });
        QTimer::singleShot(3000, &app, &QApplication::quit);
    }

    return app.exec();
}
