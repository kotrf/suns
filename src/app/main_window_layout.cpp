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
    makePanelScrollable(findChild<QScrollArea*>("commandScrollArea"), 280, 520);
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
        if (dock->objectName() == "overviewDock" || dock->objectName() == "productionDock") {
            dock->setMinimumWidth(280);
            dock->setMaximumWidth(520);
        }
    }

    panels->addSeparator();
    auto* reset = panels->addAction("Reset panel layout");
    reset->setObjectName("resetPanelLayoutAction");
    reset->setToolTip("Restore the default docked workspace around the galaxy map");
    connect(reset, &QAction::triggered, this, &MainWindow::resetPanelLayout);

    panels->addSeparator();
    panels->addSection("Workspaces");
    // Presets always start from the same recoverable layout. They change only
    // window placement and visibility, never campaign state or orders.
    const auto preset = [this](int choice) {
        resetPanelLayout();
        auto* overview = findChild<QDockWidget*>("overviewDock");
        auto* production = findChild<QDockWidget*>("productionDock");
        auto* fleet = findChild<QDockWidget*>("fleetDock");
        auto* route = findChild<QDockWidget*>("fleetRouteProgramDock");
        auto* history = findChild<QDockWidget*>("empireHistoryDock");
        const auto show = [](QDockWidget* dock, bool visible) {
            if (dock) dock->setVisible(visible);
        };
        for (auto* dock : findChildren<QDockWidget*>()) dock->hide();
        switch (choice) {
        case 0: // Unobstructed map, with every panel recoverable through View.
            break;
        case 1: // Fleet operations.
            show(fleet, true);
            show(route, true);
            if (fleet) fleet->raise();
            break;
        case 2: // Empire management and reports.
            show(overview, true);
            show(production, true);
            show(turnMessagesDock_, true);
            show(history, true);
            if (turnMessagesDock_) turnMessagesDock_->raise();
            break;
        case 3: // An engineering desk with the non-modal designer.
            openShipDesigner();
            break;
        }
    };
    const auto addPreset = [this, panels, &preset](const char* label, const char* name, int choice) {
        auto* action = panels->addAction(label);
        action->setObjectName(name);
        connect(action, &QAction::triggered, this, [preset, choice] { preset(choice); });
    };
    addPreset("Map", "mapWorkspaceAction", 0);
    addPreset("Fleet Operations", "fleetWorkspaceAction", 1);
    addPreset("Empire", "empireWorkspaceAction", 2);
    addPreset("Ship Design", "designWorkspaceAction", 3);

    panels->addSeparator();
    auto* saveCustom = panels->addAction("Save current as Custom");
    saveCustom->setObjectName("saveCustomWorkspaceAction");
    auto* restoreCustom = panels->addAction("Restore Custom");
    restoreCustom->setObjectName("restoreCustomWorkspaceAction");
    const auto hasCustom = [] {
        QSettings settings("SunsProject", "Suns");
        return !settings.value("workspace/customDocks").toByteArray().isEmpty();
    };
    restoreCustom->setEnabled(hasCustom());
    connect(saveCustom, &QAction::triggered, this, [this, restoreCustom] {
        QSettings settings("SunsProject", "Suns");
        settings.setValue("workspace/customGeometry", saveGeometry());
        settings.setValue("workspace/customDocks", saveState(2));
        restoreCustom->setEnabled(true);
        statusBar()->showMessage("Custom workspace saved", 1800);
    });
    connect(restoreCustom, &QAction::triggered, this, [this] {
        QSettings settings("SunsProject", "Suns");
        const auto docks = settings.value("workspace/customDocks").toByteArray();
        if (docks.isEmpty()) return;
        const auto geometry = settings.value("workspace/customGeometry").toByteArray();
        if (!geometry.isEmpty()) restoreGeometry(geometry);
        statusBar()->showMessage(restoreState(docks, 2)
            ? "Custom workspace restored" : "Saved workspace could not be restored", 2400);
    });

    if (!QCoreApplication::arguments().contains("--smoke-test")) {
        QSettings settings("SunsProject", "Suns");
        restoreGeometry(settings.value("workspace/geometry").toByteArray());
        // Version 2 discards the old asymmetric left-column geometry once.
        restoreState(settings.value("workspace/docks").toByteArray(), 2);
    }

    // Layout setup is the last module allowed to create a top-level menu.
    // Reinsert Help here so it remains last regardless of construction order.
    if (auto* help = menuBar()->findChild<QMenu*>("sunsHelpMenu")) {
        menuBar()->removeAction(help->menuAction());
        menuBar()->addAction(help->menuAction());
    }
}

void MainWindow::resetPanelLayout()
{
    if (auto* command = findChild<QScrollArea*>("commandScrollArea")) {
        command->setMinimumWidth(280);
        command->setMaximumWidth(520);
        command->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        command->show();
    }

    auto* overview = findChild<QDockWidget*>("overviewDock");
    auto* production = findChild<QDockWidget*>("productionDock");
    auto* fleet = findChild<QDockWidget*>("fleetDock");
    auto* route = findChild<QDockWidget*>("fleetRouteProgramDock");
    for (auto* dock : {overview, production, fleet, route, turnMessagesDock_, historyDock_}) {
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
    if (overview && production) {
        resizeDocks({overview, production}, {340, 340}, Qt::Horizontal);
        resizeDocks({overview, production}, {360, 240}, Qt::Vertical);
    } else if (overview) {
        resizeDocks({overview}, {340}, Qt::Horizontal);
    }
    if (fleet) resizeDocks({fleet}, {350}, Qt::Horizontal);
    if (turnMessagesDock_) addDockWidget(Qt::BottomDockWidgetArea, turnMessagesDock_);
    if (historyDock_) {
        addDockWidget(Qt::BottomDockWidgetArea, historyDock_);
        if (turnMessagesDock_) tabifyDockWidget(turnMessagesDock_, historyDock_);
    }
    if (turnMessagesDock_) turnMessagesDock_->raise();

    if (auto* routeScroll = findChild<QScrollArea*>("routeProgramScrollArea")) {
        routeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        routeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    statusBar()->showMessage("Dockable workspace restored", 1800);
}

} // namespace suns
