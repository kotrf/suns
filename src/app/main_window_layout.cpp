#include "main_window.hpp"

#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QCoreApplication>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>

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
    installFleetOperationsPanel();
    if (auto* production = findChild<QDockWidget*>("productionDock")) {
        if (auto* content = production->widget(); content && !qobject_cast<QScrollArea*>(content)) {
            auto* scroll = new QScrollArea(production);
            scroll->setObjectName("productionScrollArea");
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            scroll->setWidget(content);
            production->setWidget(scroll);
        }
    }
    // The first responsive pass intentionally disabled horizontal scrollbars.
    // That works for prose labels, but technical forms and compact controls can
    // still have a real minimum width. In a narrow panel those controls were
    // clipped with no way for the player to reach their right-hand side.
    makePanelScrollable(findChild<QScrollArea*>("commandScrollArea"), 260, 420);
    makePanelScrollable(findChild<QScrollArea*>("productionScrollArea"), 260, 420);
    makePanelScrollable(findChild<QScrollArea*>("fleetOrdersScrollArea"), 340, 620);

    auto* panels = viewMenu(menuBar());
    panels->addSection("Panels");
    for (auto* dock : findChildren<QDockWidget*>()) {
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        if (!panels->actions().contains(dock->toggleViewAction())) {
            panels->addAction(dock->toggleViewAction());
        }
        if (dock->objectName() == "overviewDock" || dock->objectName() == "productionDock") {
            dock->setMinimumWidth(260);
            dock->setMaximumWidth(420);
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
        auto* history = findChild<QDockWidget*>("empireHistoryDock");
        auto* research = findChild<QDockWidget*>("researchDock");
        const auto show = [](QDockWidget* dock, bool visible) {
            if (dock) dock->setVisible(visible);
        };
        for (auto* dock : findChildren<QDockWidget*>()) dock->hide();
        switch (choice) {
        case 0: // Unobstructed map, with every panel recoverable through View.
            break;
        case 1: // Fleet operations.
            show(fleet, true);
            if (fleet) fleet->raise();
            break;
        case 2: // Empire management and reports.
            show(overview, true);
            show(production, true);
            show(research, true);
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
        return settings.value("workspace/customVersion").toInt() == 3
            && !settings.value("workspace/customDocks").toByteArray().isEmpty();
    };
    restoreCustom->setEnabled(hasCustom());
    connect(saveCustom, &QAction::triggered, this, [this, restoreCustom] {
        QSettings settings("SunsProject", "Suns");
        settings.setValue("workspace/customGeometry", saveGeometry());
        settings.setValue("workspace/customDocks", saveState(3));
        settings.setValue("workspace/customVersion", 3);
        restoreCustom->setEnabled(true);
        statusBar()->showMessage("Custom workspace saved", 1800);
    });
    connect(restoreCustom, &QAction::triggered, this, [this] {
        QSettings settings("SunsProject", "Suns");
        const auto docks = settings.value("workspace/customDocks").toByteArray();
        if (docks.isEmpty()) return;
        const auto geometry = settings.value("workspace/customGeometry").toByteArray();
        if (!geometry.isEmpty()) restoreGeometry(geometry);
        statusBar()->showMessage(restoreState(docks, 3)
            ? "Custom workspace restored" : "Saved workspace could not be restored", 2400);
    });

    if (!QCoreApplication::arguments().contains("--smoke-test")) {
        QSettings settings("SunsProject", "Suns");
        restoreGeometry(settings.value("workspace/geometry").toByteArray());
        // Version 3 replaces the separate fleet/route tabs with one panel.
        restoreState(settings.value("workspace/docks").toByteArray(), 3);
    }
    // The constructor fits before docks have their final dimensions. Fit once
    // after the first layout so the initial galaxy is not reduced to a dot.
    QTimer::singleShot(0, this, [this] { if (!shuttingDown_) fitGalaxyView(); });

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
        command->setMinimumWidth(260);
        command->setMaximumWidth(420);
        command->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        command->show();
    }

    auto* overview = findChild<QDockWidget*>("overviewDock");
    auto* production = findChild<QDockWidget*>("productionDock");
    auto* fleet = findChild<QDockWidget*>("fleetDock");
    for (auto* dock : {overview, production, fleet, turnMessagesDock_, historyDock_, researchDock_}) {
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
    if (researchDock_) {
        addDockWidget(Qt::LeftDockWidgetArea, researchDock_);
        if (production) tabifyDockWidget(production, researchDock_);
        researchDock_->hide();
    }
    if (fleet) addDockWidget(Qt::RightDockWidgetArea, fleet);
    if (fleet) fleet->raise();
    if (overview && production) {
        resizeDocks({overview, production}, {290, 290}, Qt::Horizontal);
        resizeDocks({overview, production}, {360, 240}, Qt::Vertical);
    } else if (overview) {
        resizeDocks({overview}, {290}, Qt::Horizontal);
    }
    if (fleet) resizeDocks({fleet}, {400}, Qt::Horizontal);
    if (turnMessagesDock_) addDockWidget(Qt::BottomDockWidgetArea, turnMessagesDock_);
    if (historyDock_) {
        addDockWidget(Qt::BottomDockWidgetArea, historyDock_);
        if (turnMessagesDock_) tabifyDockWidget(turnMessagesDock_, historyDock_);
    }
    if (turnMessagesDock_) turnMessagesDock_->raise();

    if (auto* routeScroll = findChild<QScrollArea*>("fleetOrdersScrollArea")) {
        routeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        routeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    statusBar()->showMessage("Dockable workspace restored", 1800);
}

} // namespace suns
