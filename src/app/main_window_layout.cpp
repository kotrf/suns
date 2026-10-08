#include "main_window.hpp"

#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QCoreApplication>
#include <QGraphicsView>
#include <QLabel>
#include <QShortcut>
#include <QSignalBlocker>
#include <QToolBar>
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

void updateWorkspaceMinimumSize(QMainWindow* window)
{
    // An explicit 900 px minimum can let Qt squeeze dock columns below
    // their content minima and overlap neighbours. Honour the live layout.
    window->setMinimumSize(window->isFullScreen() ? QSize(0, 0)
        : window->minimumSizeHint().expandedTo(QSize(900, 600)));
}

} // namespace

void MainWindow::installPanelLayoutFixes()
{
    installFleetOperationsPanel();
    installColonyWorkspace();
    if (auto* production = findChild<QDockWidget*>("productionDock")) {
        if (auto* content = production->widget(); content && !qobject_cast<QScrollArea*>(content)
            && content->objectName() != "colonyProductionPanel") {
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
    makePanelScrollable(findChild<QScrollArea*>("systemDetailsScrollArea"), 240, 380);
    makePanelScrollable(findChild<QScrollArea*>("productionScrollArea"), 260, 420);
    makePanelScrollable(findChild<QScrollArea*>("fleetOrdersScrollArea"), 340, 620);

    // Reports now share the narrower center column below the map. Their
    // wide forms must scroll inside that column rather than overlap the docks.
    for (auto* dock : {turnMessagesDock_, historyDock_, researchDock_}) {
        if (!dock || !dock->widget() || qobject_cast<QScrollArea*>(dock->widget())) continue;
        auto* content = dock->widget();
        content->setParent(nullptr);
        auto* scroll = new QScrollArea(dock);
        scroll->setObjectName(dock->objectName() + "ScrollArea");
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(content);
        dock->setWidget(scroll);
    }

    auto* panels = viewMenu(menuBar());
    auto* expand = panels->addAction("Fullscreen map");
    expand->setObjectName("expandMapAction");
    expand->setCheckable(true);
    expand->setShortcut(Qt::Key_F11);
    expand->setToolTip("Show the map fullscreen; F11 or Esc restores the panels");
    connect(expand, &QAction::toggled, this, &MainWindow::setMapExpanded);
    if (auto* toolbar = findChild<QToolBar*>("mapViewToolbar")) {
        insertToolBarBreak(toolbar);
        toolbar->addSeparator();
        toolbar->addAction(expand);
    }
    if (auto* mode = findChild<QComboBox*>("mapDisplayModeCombo")) mode->setMinimumWidth(100);
    if (auto* legend = findChild<QLabel*>("mapDisplayLegend")) {
        legend->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        legend->setMinimumWidth(0);
    }
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setObjectName("restoreMapPanelsShortcut");
    escape->setEnabled(false);
    connect(escape, &QShortcut::activated, this, [this] { setMapExpanded(false); });
    panels->addSeparator();
    panels->addSection("Panels");
    for (auto* dock : findChildren<QDockWidget*>()) {
        const auto updateMinimum = [this] {
            QTimer::singleShot(0, this, [this] {
                if (!shuttingDown_) updateWorkspaceMinimumSize(this);
            });
        };
        connect(dock, &QDockWidget::visibilityChanged, this, updateMinimum);
        connect(dock, &QDockWidget::topLevelChanged, this, updateMinimum);
        connect(dock, &QDockWidget::dockLocationChanged, this, updateMinimum);
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
        if (choice == 0) {
            setMapExpanded(true);
            return;
        }
        resetPanelLayout();
        auto* overview = findChild<QDockWidget*>("overviewDock");
        auto* production = findChild<QDockWidget*>("productionDock");
        auto* details = findChild<QDockWidget*>("systemDetailsDock");
        auto* fleet = findChild<QDockWidget*>("fleetDock");
        auto* history = findChild<QDockWidget*>("empireHistoryDock");
        auto* research = findChild<QDockWidget*>("researchDock");
        const auto show = [](QDockWidget* dock, bool visible) {
            if (dock) dock->setVisible(visible);
        };
        for (auto* dock : findChildren<QDockWidget*>()) dock->hide();
        switch (choice) {
        case 1: // Fleet operations.
            show(overview, true);
            show(production, true);
            show(details, true);
            show(fleet, true);
            if (fleet) fleet->raise();
            break;
        case 2: // Empire management and reports.
            show(overview, true);
            show(production, true);
            show(details, true);
            show(research, true);
            show(turnMessagesDock_, true);
            show(history, true);
            if (turnMessagesDock_) turnMessagesDock_->raise();
            break;
        case 3: // An engineering desk with the non-modal designer.
            show(overview, true);
            show(production, true);
            show(details, true);
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
        return (settings.value("workspace/customVersion").toInt() == 3
            || settings.value("workspace/customVersion").toInt() == 4
            || settings.value("workspace/customVersion").toInt() == 5)
            && !settings.value("workspace/customDocks").toByteArray().isEmpty();
    };
    restoreCustom->setEnabled(hasCustom());
    connect(saveCustom, &QAction::triggered, this, [this, restoreCustom] {
        setMapExpanded(false);
        QSettings settings("SunsProject", "Suns");
        settings.setValue("workspace/customGeometry", saveGeometry());
        settings.setValue("workspace/customDocks", saveState(5));
        settings.setValue("workspace/customVersion", 5);
        restoreCustom->setEnabled(true);
        statusBar()->showMessage("Custom workspace saved", 1800);
    });
    connect(restoreCustom, &QAction::triggered, this, [this] {
        setMapExpanded(false);
        QSettings settings("SunsProject", "Suns");
        const auto docks = settings.value("workspace/customDocks").toByteArray();
        if (docks.isEmpty()) return;
        const auto geometry = settings.value("workspace/customGeometry").toByteArray();
        if (!geometry.isEmpty()) restoreGeometry(geometry);
        statusBar()->showMessage(restoreState(docks, settings.value("workspace/customVersion", 3).toInt())
            ? "Custom workspace restored" : "Saved workspace could not be restored", 2400);
    });

    resetPanelLayout();
    if (!QCoreApplication::arguments().contains("--smoke-test")) {
        QSettings settings("SunsProject", "Suns");
        restoreGeometry(settings.value("workspace/geometry").toByteArray());
        // Version 5 adds a separate details column beside status and production.
        // Geometry survives the one-time reset of older dock layouts.
        restoreState(settings.value("workspace/docks").toByteArray(), 5);
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
    setMapExpanded(false);
    if (auto* command = findChild<QScrollArea*>("commandScrollArea"); command && !findChild<QWidget*>("colonyStatusPanel")) {
        command->setMinimumWidth(260);
        command->setMaximumWidth(420);
        command->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        command->show();
    }

    auto* overview = findChild<QDockWidget*>("overviewDock");
    auto* production = findChild<QDockWidget*>("productionDock");
    auto* details = findChild<QDockWidget*>("systemDetailsDock");
    auto* fleet = findChild<QDockWidget*>("fleetDock");
    for (auto* dock : {overview, production, details, fleet, turnMessagesDock_, historyDock_, researchDock_}) {
        if (!dock) continue;
        dock->setFloating(false);
        removeDockWidget(dock);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        dock->show();
    }

    if (overview) addDockWidget(Qt::LeftDockWidgetArea, overview);
    // Split the whole left area into columns before stacking the first
    // column's docks, so details spans the height of status and production.
    if (details) {
        addDockWidget(Qt::LeftDockWidgetArea, details);
        if (overview) splitDockWidget(overview, details, Qt::Horizontal);
    }
    if (production) {
        addDockWidget(Qt::LeftDockWidgetArea, production);
        if (overview) splitDockWidget(overview, production, Qt::Vertical);
    }
    if (fleet) addDockWidget(Qt::RightDockWidgetArea, fleet);
    if (fleet) fleet->raise();
    if (overview && production) {
        resizeDocks({overview, production}, {310, 310}, Qt::Horizontal);
        resizeDocks({overview, production}, {295, 330}, Qt::Vertical);
    } else if (overview) {
        resizeDocks({overview}, {310}, Qt::Horizontal);
    }
    if (details) resizeDocks({details}, {270}, Qt::Horizontal);
    if (fleet) resizeDocks({fleet}, {360}, Qt::Horizontal);
    if (turnMessagesDock_) addDockWidget(Qt::BottomDockWidgetArea, turnMessagesDock_);
    if (historyDock_) {
        addDockWidget(Qt::BottomDockWidgetArea, historyDock_);
        if (turnMessagesDock_) tabifyDockWidget(turnMessagesDock_, historyDock_);
    }
    if (researchDock_) {
        addDockWidget(Qt::BottomDockWidgetArea, researchDock_);
        if (turnMessagesDock_) tabifyDockWidget(turnMessagesDock_, researchDock_);
        researchDock_->hide();
    }
    if (turnMessagesDock_) turnMessagesDock_->raise();
    if (turnMessagesDock_) resizeDocks({turnMessagesDock_}, {190}, Qt::Vertical);

    if (auto* routeScroll = findChild<QScrollArea*>("fleetOrdersScrollArea")) {
        routeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        routeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    updateWorkspaceMinimumSize(this);
    statusBar()->showMessage("Dockable workspace restored", 1800);
}

void MainWindow::setMapExpanded(bool expanded)
{
    if (mapExpanded_ == expanded || !view_) return;
    const auto center = view_->mapToScene(view_->viewport()->rect().center());
    mapExpanded_ = expanded;
    cancelRouteProgramMapTargetPick();
    if (expanded) {
        mapWorkspaceState_ = saveState(5);
        mapWorkspaceGeometry_ = saveGeometry();
        mapPreviousWindowState_ = windowState();
        mapMenuWasVisible_ = menuBar()->isVisible();
        mapStatusWasVisible_ = statusBar()->isVisible();
        for (auto* dock : findChildren<QDockWidget*>()) dock->hide();
        for (auto* toolbar : findChildren<QToolBar*>())
            if (toolbar->objectName() != "mapViewToolbar" && toolbar->objectName() != "mapDisplayToolbar") toolbar->hide();
        menuBar()->hide();
        statusBar()->hide();
        setMinimumSize(0, 0);
        showFullScreen();
    } else {
        setWindowState(mapPreviousWindowState_);
        restoreGeometry(mapWorkspaceGeometry_);
        restoreState(mapWorkspaceState_, 5);
        menuBar()->setVisible(mapMenuWasVisible_);
        statusBar()->setVisible(mapStatusWasVisible_);
        mapWorkspaceState_.clear();
        mapWorkspaceGeometry_.clear();
        updateWorkspaceMinimumSize(this);
    }
    if (auto* action = findChild<QAction*>("expandMapAction")) {
        const QSignalBlocker blocker(action);
        action->setChecked(expanded);
        action->setText(expanded ? "Return to panels" : "Fullscreen map");
    }
    if (auto* escape = findChild<QShortcut*>("restoreMapPanelsShortcut")) escape->setEnabled(expanded);
    QTimer::singleShot(0, this, [this, center] {
        if (shuttingDown_) return;
        view_->centerOn(center);
        refreshMapLabels();
    });
}

} // namespace suns
