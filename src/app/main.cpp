#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QProgressBar>
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
    window.installMiningInfrastructure();

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

            // Touch the new dashboard gauges in the offscreen path too; their
            // values are refreshed asynchronously from the selected fleet.
            if (auto* fuel = window.findChild<QProgressBar*>("fleetFuelBar")) {
                fuel->update();
            }
            if (auto* cargo = window.findChild<QProgressBar*>("fleetCargoBar")) {
                cargo->update();
            }
        });
        // Let deferred selection redraw, map-mode restyling, planet portrait,
        // panel-layout recovery, fleet gauges and route-program timers run.
        QTimer::singleShot(350, &window, [&window] { window.close(); });
        QTimer::singleShot(3000, &app, &QApplication::quit);
    }

    return app.exec();
}
