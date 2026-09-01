#include "main_window.hpp"

#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QCoreApplication>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>

namespace suns {

namespace {

QMenu* viewMenu(QMenuBar* menuBar)
{
    if (auto* existing = menuBar->findChild<QMenu*>("sunsViewMenu")) return existing;
    auto* menu = menuBar->addMenu("&View");
    menu->setObjectName("sunsViewMenu");
    return menu;
}

void makePanelScrollable(QScrollArea* scroll, int minimumWidth, int maximumWidth)
{
    if (!scroll) return;
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setMinimumWidth(minimumWidth);
    scroll->setMaximumWidth(maximumWidth);
}

} // namespace

void MainWindow::installPanelLayoutFixes()
{
    // The first responsive pass intentionally disabled horizontal scrollbars.
    // That works for prose labels, but technical forms and compact controls can
    // still have a real minimum width. In a narrow panel those controls were
    // clipped with no way for the player to reach their right-hand side.
    makePanelScrollable(findChild<QScrollArea*>("commandScrollArea"), 300, 520);
    makePanelScrollable(findChild<QScrollArea*>("fleetScrollArea"), 300, 560);
    makePanelScrollable(findChild<QScrollArea*>("routeProgramScrollArea"), 280, 520);

    auto* panels = viewMenu(menuBar());
    panels->addSection("Panels");
    for (auto* dock : findChildren<QDockWidget*>()) {
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        if (!panels->actions().contains(dock->toggleViewAction())) {
            panels->addAction(dock->toggleViewAction());
        }
        if (dock->objectName() == "fleetRouteProgramDock") {
            dock->setMinimumWidth(280);
            dock->setMaximumWidth(520);
        }
    }

    panels->addSeparator();
    auto* reset = panels->addAction("Reset panel layout");
    reset->setObjectName("resetPanelLayoutAction");
    reset->setToolTip("Restore the default docked workspace around the galaxy map");
    connect(reset, &QAction::triggered, this, &MainWindow::resetPanelLayout);

    if (!QCoreApplication::arguments().contains("--smoke-test")) {
        QSettings settings("SunsProject", "Suns");
        restoreGeometry(settings.value("workspace/geometry").toByteArray());
        restoreState(settings.value("workspace/docks").toByteArray(), 1);
    }
}

void MainWindow::resetPanelLayout()
{
    if (auto* command = findChild<QScrollArea*>("commandScrollArea")) {
        command->setMinimumWidth(300);
        command->setMaximumWidth(520);
        command->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        command->show();
    }

    auto* overview = findChild<QDockWidget*>("overviewDock");
    auto* production = findChild<QDockWidget*>("productionDock");
    auto* fleet = findChild<QDockWidget*>("fleetDock");
    auto* route = findChild<QDockWidget*>("fleetRouteProgramDock");
    for (auto* dock : {overview, production, fleet, route, turnMessagesDock_}) {
        if (!dock) continue;
        dock->setFloating(false);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        dock->show();
    }

    if (overview) addDockWidget(Qt::LeftDockWidgetArea, overview);
    if (production) {
        addDockWidget(Qt::LeftDockWidgetArea, production);
        if (overview) splitDockWidget(overview, production, Qt::Vertical);
    }
    if (fleet) addDockWidget(Qt::RightDockWidgetArea, fleet);
    if (route) {
        addDockWidget(Qt::RightDockWidgetArea, route);
        route->setMinimumWidth(280);
        route->setMaximumWidth(520);
        if (fleet) tabifyDockWidget(fleet, route);
    }
    if (fleet) fleet->raise();
    if (overview) resizeDocks({overview}, {330}, Qt::Horizontal);
    if (fleet) resizeDocks({fleet}, {350}, Qt::Horizontal);

    if (auto* routeScroll = findChild<QScrollArea*>("routeProgramScrollArea")) {
        routeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        routeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    statusBar()->showMessage("Dockable workspace restored", 1800);
}

} // namespace suns
