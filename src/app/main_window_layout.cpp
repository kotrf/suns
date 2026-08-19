#include "main_window.hpp"

#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QScrollArea>
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
    makePanelScrollable(findChild<QScrollArea*>("routeProgramScrollArea"), 280, 520);

    if (auto* routeDock = findChild<QDockWidget*>("fleetRouteProgramDock")) {
        routeDock->setAllowedAreas(Qt::RightDockWidgetArea);
        routeDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
        routeDock->setMinimumWidth(280);
        routeDock->setMaximumWidth(520);
    }

    auto* reset = viewMenu(menuBar())->addAction("Reset panel layout");
    reset->setObjectName("resetPanelLayoutAction");
    reset->setToolTip("Restore the command and Fleet Route Program panels to their default widths and positions");
    connect(reset, &QAction::triggered, this, &MainWindow::resetPanelLayout);
}

void MainWindow::resetPanelLayout()
{
    if (auto* command = findChild<QScrollArea*>("commandScrollArea")) {
        command->setMinimumWidth(300);
        command->setMaximumWidth(520);
        command->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        command->show();
    }

    if (auto* routeDock = findChild<QDockWidget*>("fleetRouteProgramDock")) {
        routeDock->setFloating(false);
        routeDock->setAllowedAreas(Qt::RightDockWidgetArea);
        routeDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
        addDockWidget(Qt::RightDockWidgetArea, routeDock);
        routeDock->show();
        routeDock->raise();
        routeDock->setMinimumWidth(280);
        routeDock->setMaximumWidth(520);
        resizeDocks({routeDock}, {335}, Qt::Horizontal);
    }

    if (auto* routeScroll = findChild<QScrollArea*>("routeProgramScrollArea")) {
        routeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        routeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    statusBar()->showMessage("Panel layout restored", 1800);
}

} // namespace suns
