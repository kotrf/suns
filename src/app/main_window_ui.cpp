#include "main_window.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace suns {

namespace {

QScrollArea* makeVerticalScrollArea(QWidget* content, QWidget* parent, const char* objectName)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setObjectName(objectName);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);

    content->setMinimumWidth(0);
    content->setMaximumWidth(QWIDGETSIZE_MAX);
    content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    content->setParent(nullptr);
    scroll->setWidget(content);
    return scroll;
}

} // namespace

void MainWindow::installUiPolish()
{
    // The command panel used to be fixed at 425 px. Together with the route
    // dock that made the whole window effectively demand more space than a
    // Full-HD desktop could comfortably provide. Both panels now scroll
    // vertically and are allowed to contract while the map gets the remainder.
    if (auto* central = centralWidget()) {
        if (auto* layout = qobject_cast<QHBoxLayout*>(central->layout()); layout && layout->count() >= 2) {
            if (auto* commandPanel = layout->itemAt(1)->widget();
                commandPanel && !qobject_cast<QScrollArea*>(commandPanel)) {
                layout->removeWidget(commandPanel);
                auto* scroll = makeVerticalScrollArea(commandPanel, central, "commandScrollArea");
                scroll->setMinimumWidth(315);
                scroll->setMaximumWidth(405);
                layout->addWidget(scroll, 0);
            }
        }
    }

    if (auto* dock = findChild<QDockWidget*>("fleetRouteProgramDock")) {
        dock->setMinimumWidth(270);
        dock->setMaximumWidth(390);
        dock->setFeatures(
            QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable
            | QDockWidget::DockWidgetFloatable);

        if (auto* routePanel = dock->widget(); routePanel && !qobject_cast<QScrollArea*>(routePanel)) {
            // Detach the original panel before replacing the dock's widget so
            // QDockWidget never owns two wrappers around the same panel.
            dock->setWidget(nullptr);
            auto* scroll = makeVerticalScrollArea(routePanel, dock, "routeProgramScrollArea");
            dock->setWidget(scroll);
        }
    }

    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
    setMinimumSize(900, 600);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    QFont compactFont = font();
    if (compactFont.pointSizeF() > 0.0) {
        compactFont.setPointSizeF(std::max(8.5, compactFont.pointSizeF() - 0.5));
        setFont(compactFont);
    }

    // Keep the restrained, information-dense look of the map but extend it to
    // controls. Accent colors identify interaction without turning the UI into
    // a modern dashboard full of decorative chrome.
    setStyleSheet(R"(
        QMainWindow, QDockWidget {
            background: #0d141d;
            color: #d8e3ef;
        }
        QWidget {
            color: #d8e3ef;
        }
        QScrollArea, QScrollArea > QWidget > QWidget {
            background: #111a25;
        }
        QLabel {
            background: transparent;
            color: #cfdae7;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            min-height: 22px;
            padding: 2px 5px;
            color: #e6eef7;
            background: #0b121b;
            border: 1px solid #2a4056;
            border-radius: 3px;
            selection-background-color: #315f88;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #58a6e7;
        }
        QPushButton {
            min-height: 23px;
            padding: 3px 7px;
            color: #dbe8f4;
            background: #192838;
            border: 1px solid #304a63;
            border-radius: 3px;
        }
        QPushButton:hover {
            background: #20384d;
            border-color: #5ba8e6;
        }
        QPushButton:pressed {
            background: #142333;
        }
        QPushButton:disabled {
            color: #617286;
            background: #101821;
            border-color: #202f3d;
        }
        QCheckBox {
            spacing: 6px;
            color: #cbd8e5;
        }
        QMenuBar, QMenu, QToolBar, QStatusBar {
            background: #101925;
            color: #d8e3ef;
        }
        QMenuBar::item:selected, QMenu::item:selected {
            background: #20384d;
        }
        QToolBar {
            spacing: 3px;
            padding: 3px;
            border-bottom: 1px solid #233446;
        }
        QToolButton {
            padding: 3px 8px;
            color: #dce8f4;
            background: #182635;
            border: 1px solid #2d4358;
            border-radius: 3px;
        }
        QToolButton:hover {
            background: #24415a;
            border-color: #5ba8e6;
        }
        QDockWidget::title {
            padding: 5px 7px;
            background: #152231;
            border-bottom: 1px solid #294057;
        }
        QScrollBar:vertical {
            width: 10px;
            margin: 0;
            background: #0c131c;
        }
        QScrollBar::handle:vertical {
            min-height: 28px;
            border-radius: 4px;
            background: #344b61;
        }
        QScrollBar::handle:vertical:hover {
            background: #496b89;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QToolTip {
            color: #eef6ff;
            background: #182635;
            border: 1px solid #4b789c;
        }
    )");

    if (view_) {
        view_->viewport()->installEventFilter(this);
        view_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
        view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }

    auto* mapToolbar = addToolBar("Map view");
    mapToolbar->setObjectName("mapViewToolbar");
    mapToolbar->setMovable(false);

    auto* zoomOut = mapToolbar->addAction("−");
    zoomOut->setToolTip("Zoom out (mouse wheel also zooms)");
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, this, [this] { zoomMap(1.0 / 1.22); });

    auto* zoomIn = mapToolbar->addAction("+");
    zoomIn->setToolTip("Zoom in (mouse wheel also zooms)");
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, this, [this] { zoomMap(1.22); });

    auto* fit = mapToolbar->addAction("Fit galaxy");
    fit->setToolTip("Fit the whole generated galaxy into the map viewport");
    fit->setShortcut(Qt::Key_Home);
    connect(fit, &QAction::triggered, this, [this] { fitGalaxyView(); });
}

void MainWindow::zoomMap(double factor)
{
    if (!view_ || factor <= 0.0) return;

    const double current = std::abs(view_->transform().m11());
    if (current <= 0.000001) return;

    constexpr double kMinScale = 0.18;
    constexpr double kMaxScale = 8.0;
    const double target = std::clamp(current * factor, kMinScale, kMaxScale);
    if (std::abs(target - current) < 0.000001) return;

    view_->scale(target / current, target / current);
    statusBar()->showMessage(QString("Map zoom ×%1").arg(target, 0, 'f', 2), 1000);
}

void MainWindow::fitGalaxyView()
{
    if (!view_ || !scene_) return;
    view_->fitInView(scene_->sceneRect().adjusted(-12.0, -12.0, 12.0, 12.0), Qt::KeepAspectRatio);
    statusBar()->showMessage("Galaxy fitted to map viewport", 1000);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (!shuttingDown_ && view_ && watched == view_->viewport() && event->type() == QEvent::Wheel) {
        const auto* wheel = static_cast<QWheelEvent*>(event);
        const int delta = wheel->angleDelta().y() != 0
            ? wheel->angleDelta().y()
            : wheel->pixelDelta().y();
        if (delta != 0) {
            zoomMap(delta > 0 ? 1.18 : 1.0 / 1.18);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Timers in the route-program dock continuously inspect the map. Stop them
    // before child widgets/scenes begin disappearing, and suppress any queued
    // deferred selection redraw. This makes shutdown deterministic instead of
    // depending on QObject child destruction order.
    shuttingDown_ = true;
    mapSelectionRebuildPending_ = false;

    const auto timers = findChildren<QTimer*>();
    for (auto* timer : timers) timer->stop();

    if (scene_) scene_->disconnect(this);
    if (view_) {
        view_->viewport()->removeEventFilter(this);
        view_->setScene(nullptr);
    }

    QMainWindow::closeEvent(event);
}

} // namespace suns
