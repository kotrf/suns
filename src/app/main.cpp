#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    window.installDeferredMapSelectionHandler();
    suns::attachRouteProgramDock(window);
    window.installUiPolish();
    window.installMapDisplayModes();
    window.installPlanetPolish();
    window.installPanelLayoutFixes();
    window.installFleetReadabilityPolish();
    window.installFleetPortraitPolish();
    window.installMiningInfrastructure();
    window.installCommunicationStatus();
    window.installTurnMessages();

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
            if (auto* portrait = window.findChild<QLabel*>("fleetPortrait")) portrait->update();
            if (auto* mine = window.findChild<QPushButton*>("queueMineButton")) mine->update();
            if (auto* comm = window.findChild<QLabel*>("fleetCommunicationSummary")) comm->update();
            if (auto* messages = window.findChild<QDockWidget*>("turnMessagesDock")) messages->update();
        });
        // Let deferred selection redraw, map-mode restyling, portraits, mining,
        // panel recovery, gauges and route-program timers run.
        QTimer::singleShot(350, &window, [&window] { window.close(); });
        QTimer::singleShot(3000, &app, &QApplication::quit);
    }

    return app.exec();
}
