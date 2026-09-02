#include "main_window.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEvent>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
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

void clearLayoutPreservingWidgets(QLayout* layout)
{
    if (!layout) return;
    while (auto* item = layout->takeAt(0)) {
        if (auto* childLayout = item->layout()) {
            clearLayoutPreservingWidgets(childLayout);
            delete childLayout;
            continue;
        }
        if (auto* widget = item->widget()) widget->hide();
        delete item;
    }
}

QGroupBox* makeGroup(const QString& title, const char* objectName, QWidget* parent)
{
    auto* group = new QGroupBox(title, parent);
    group->setObjectName(objectName);
    return group;
}

} // namespace

void MainWindow::installUiPolish()
{
    QWidget* commandPanel = nullptr;
    QWidget* fleetPanel = nullptr;
    QWidget* productionPanel = nullptr;
    QLabel* planetInfo = nullptr;
    QLabel* fleetInfo = nullptr;

    // Rebuild the chronological side panel as a dockable workspace. Map state,
    // fleet operations and production each get a stable thematic home; the
    // Fleet and Route Program docks share one tabbed area by default.
    if (auto* central = centralWidget()) {
        if (auto* layout = qobject_cast<QHBoxLayout*>(central->layout()); layout && layout->count() >= 2) {
            commandPanel = layout->itemAt(1)->widget();
            if (commandPanel && !qobject_cast<QScrollArea*>(commandPanel)) {
                if (auto* sideLayout = qobject_cast<QVBoxLayout*>(commandPanel->layout())) {
                    clearLayoutPreservingWidgets(sideLayout);
                    sideLayout->setContentsMargins(8, 8, 8, 8);
                    sideLayout->setSpacing(8);

                    auto* title = new QLabel("<h2>Suns!</h2><span style='color:#8394a8'>Command console</span>", commandPanel);
                    sideLayout->addWidget(title);

                    auto* empireGroup = makeGroup("Empire", "empireGroup", commandPanel);
                    auto* empireLayout = new QVBoxLayout(empireGroup);
                    galaxyLabel_->show();
                    empireLabel_->show();
                    empireLayout->addWidget(galaxyLabel_);
                    empireLayout->addWidget(empireLabel_);
                    sideLayout->addWidget(empireGroup);

                    // Orders stay near the top and use a warm accent so the
                    // player always knows where to look before ending a turn.
                    auto* ordersGroup = makeGroup("Orders this turn", "ordersGroup", commandPanel);
                    auto* ordersLayout = new QVBoxLayout(ordersGroup);
                    ordersLabel_->show();
                    endTurnButton_->show();
                    endTurnButton_->setText("End Turn — resolve orders");
                    endTurnButton_->setObjectName("primaryTurnButton");
                    ordersLayout->addWidget(ordersLabel_);
                    ordersLayout->addWidget(endTurnButton_);
                    sideLayout->addWidget(ordersGroup);

                    auto* planetGroup = makeGroup("Selected system / planet", "planetGroup", commandPanel);
                    auto* planetLayout = new QVBoxLayout(planetGroup);
                    planetInfo = new QLabel(planetGroup);
                    planetInfo->setWordWrap(true);
                    planetInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
                    planetLayout->addWidget(planetInfo);
                    planetEnvironmentPanel_ = new QWidget(planetGroup);
                    auto* environmentLayout = new QFormLayout(planetEnvironmentPanel_);
                    environmentLayout->setContentsMargins(0, 2, 0, 0);
                    environmentLayout->setVerticalSpacing(4);
                    const auto makeEnvironmentBar = [this](const char* objectName) {
                        auto* bar = new QProgressBar(planetEnvironmentPanel_);
                        bar->setObjectName(objectName);
                        bar->setRange(0, 100);
                        bar->setTextVisible(false);
                        bar->setFixedHeight(9);
                        return bar;
                    };
                    planetTemperatureBar_ = makeEnvironmentBar("planetTemperatureBar");
                    planetGravityBar_ = makeEnvironmentBar("planetGravityBar");
                    planetRadiationBar_ = makeEnvironmentBar("planetRadiationBar");
                    environmentLayout->addRow("Temperature", planetTemperatureBar_);
                    environmentLayout->addRow("Gravity", planetGravityBar_);
                    environmentLayout->addRow("Radiation", planetRadiationBar_);
                    planetLayout->addWidget(planetEnvironmentPanel_);
                    sideLayout->addWidget(planetGroup);

                    fleetPanel = new QWidget(this);
                    auto* fleetPanelLayout = new QVBoxLayout(fleetPanel);
                    auto* fleetGroup = makeGroup("Selected fleet", "fleetGroup", fleetPanel);
                    auto* fleetLayout = new QVBoxLayout(fleetGroup);
                    fleetInfo = new QLabel(fleetGroup);
                    fleetInfo->setWordWrap(true);
                    fleetInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
                    fleetLayout->addWidget(fleetInfo);

                    auto* colonistForm = new QFormLayout;
                    colonistLoadSpin_->show();
                    colonistForm->addRow("Colonists aboard", colonistLoadSpin_);
                    fleetLayout->addLayout(colonistForm);

                    loadColonistsButton_->hide();
                    colonistLoadSpin_->setToolTip(
                        "Changing this value updates the current-year dockside cargo plan immediately");

                    auto* quickRow = new QHBoxLayout;
                    auto* loadMaxButton = new QPushButton("Load to capacity", fleetGroup);
                    loadMaxButton->setToolTip("Fill remaining cargo with colonists, leaving at least one colonist on the colony");
                    auto* unloadButton = new QPushButton("Unload colonists", fleetGroup);
                    quickRow->addWidget(loadMaxButton);
                    quickRow->addWidget(unloadButton);
                    fleetLayout->addLayout(quickRow);

                    refuelButton_->show();
                    refuelButton_->setText("Refuel now");
                    fleetLayout->addWidget(refuelButton_);

                    auto* cargoButton = new QPushButton("Transfer cargo…", fleetGroup);
                    cargoButton->setToolTip(
                        "Transfer colonists and minerals between the planetary surface and friendly fleets at this system");
                    fleetLayout->addWidget(cargoButton);

                    auto* organizationRow = new QHBoxLayout;
                    auto* mergeButton = new QPushButton("Merge fleets…", fleetGroup);
                    mergeButton->setToolTip("Merge another co-located stationary friendly fleet into this FleetId");
                    auto* splitButton = new QPushButton("Split fleet…", fleetGroup);
                    splitButton->setToolTip("Move selected ships into a newly allocated FleetId");
                    organizationRow->addWidget(mergeButton);
                    organizationRow->addWidget(splitButton);
                    fleetLayout->addLayout(organizationRow);

                    colonizeButton_->show();
                    colonizeButton_->setText("Colonize selected world…");
                    colonizeButton_->setToolTip("Dismantles the entire selected fleet and recovers one third of its construction minerals");
                    fleetLayout->addWidget(colonizeButton_);
                    designShipButton_->show();
                    designShipButton_->setText("Ship designer…");
                    fleetLayout->addWidget(designShipButton_);
                    fleetPanelLayout->addWidget(fleetGroup);
                    fleetPanelLayout->addStretch(1);

                    productionPanel = new QWidget(this);
                    auto* productionPanelLayout = new QVBoxLayout(productionPanel);
                    auto* productionGroup = makeGroup("Add to queue", "productionGroup", productionPanel);
                    auto* productionLayout = new QVBoxLayout(productionGroup);
                    auto* productionForm = new QFormLayout;
                    shipDesignCombo_->show();
                    productionForm->addRow("Ship design", shipDesignCombo_);
                    productionLayout->addLayout(productionForm);
                    buildShipButton_->show();
                    buildFactoryButton_->show();
                    buildOrbitalDockButton_->show();
                    buildShipButton_->setText("Queue selected ship");
                    productionLayout->addWidget(buildShipButton_);
                    productionLayout->addWidget(buildFactoryButton_);
                    productionLayout->addWidget(buildOrbitalDockButton_);
                    productionPanelLayout->addWidget(productionGroup);

                    auto* viewGroup = makeGroup("Map display", "viewGroup", commandPanel);
                    auto* viewLayout = new QVBoxLayout(viewGroup);
                    sensorRangesCheck_->show();
                    viewLayout->addWidget(sensorRangesCheck_);
                    sideLayout->addWidget(viewGroup);
                    sideLayout->addStretch(1);

                    // Legacy summary/course widgets remain alive because the
                    // original updateControls implementation still maintains
                    // them, but they are no longer player-facing. This avoids
                    // stale 'planned Warp' text competing with Route Program.
                    selectionLabel_->hide();
                    fleetLabel_->hide();
                    warpSpin_->hide();
                    arrivalReserveSpin_->hide();
                    fleetMoveButton_->hide();
                    fleetLoadAllButton_->hide();
                    seedEdit_->hide();
                    starCountSpin_->hide();
                    newGalaxyButton_->hide();

                    connect(loadMaxButton, &QPushButton::clicked, this, [this] {
                        const auto* fleet = selectedFleet();
                        const auto* colony = selectedFriendlyColonyForFleet();
                        if (!fleet || !colony) {
                            statusBar()->showMessage("Select a fleet docked at the selected friendly colony first", 2500);
                            return;
                        }

                        const auto freeForColonists = std::max(
                            0.0,
                            fleet_cargo_capacity(state_, *fleet) - mineral_cargo_mass(fleet->minerals));
                        const auto capacity = static_cast<std::uint64_t>(
                            std::floor(freeForColonists * kColonistsPerCargoUnit + 0.000001));
                        const auto colonyAvailable = colony->population > 1 ? colony->population - 1 : 0;
                        const auto available = fleet->colonists + colonyAvailable;
                        const auto target = std::min(capacity, available);
                        colonistLoadSpin_->setValue(static_cast<int>(target));
                    });

                    connect(unloadButton, &QPushButton::clicked, this, [this] {
                        if (!selectedFriendlyColonyForFleet()) {
                            statusBar()->showMessage("Select a fleet docked at the selected friendly colony first", 2500);
                            return;
                        }
                        colonistLoadSpin_->setValue(0);
                    });
                    connect(cargoButton, &QPushButton::clicked, this, [this] { openCargoManifestDialog(); });
                    connect(mergeButton, &QPushButton::clicked, this, [this] { openMergeFleetsDialog(); });
                    connect(splitButton, &QPushButton::clicked, this, [this] { openSplitFleetDialog(); });
                }

                layout->removeWidget(commandPanel);
                auto* scroll = makeVerticalScrollArea(commandPanel, central, "commandScrollArea");
                auto* overviewDock = new QDockWidget("Overview", this);
                overviewDock->setObjectName("overviewDock");
                overviewDock->setAllowedAreas(Qt::AllDockWidgetAreas);
                overviewDock->setFeatures(QDockWidget::DockWidgetClosable
                    | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
                overviewDock->setWidget(scroll);
                addDockWidget(Qt::LeftDockWidgetArea, overviewDock);

                if (fleetPanel) {
                    auto* fleetDock = new QDockWidget("Fleet — Overview & Logistics", this);
                    fleetDock->setObjectName("fleetDock");
                    fleetDock->setAllowedAreas(Qt::AllDockWidgetAreas);
                    fleetDock->setFeatures(QDockWidget::DockWidgetClosable
                        | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
                    fleetDock->setWidget(makeVerticalScrollArea(fleetPanel, fleetDock, "fleetScrollArea"));
                    addDockWidget(Qt::RightDockWidgetArea, fleetDock);
                }

                if (productionPanel) {
                    auto* productionDock = new QDockWidget("Production", this);
                    productionDock->setObjectName("productionDock");
                    productionDock->setAllowedAreas(Qt::AllDockWidgetAreas);
                    productionDock->setFeatures(QDockWidget::DockWidgetClosable
                        | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
                    productionDock->setWidget(productionPanel);
                    addDockWidget(Qt::LeftDockWidgetArea, productionDock);
                    splitDockWidget(overviewDock, productionDock, Qt::Vertical);
                }
            }
        }
    }

    auto* routeDock = findChild<QDockWidget*>("fleetRouteProgramDock");
    if (routeDock) {
        routeDock->setMinimumWidth(270);
        routeDock->setMaximumWidth(390);
        routeDock->setFeatures(
            QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable
            | QDockWidget::DockWidgetFloatable);

        if (auto* routePanel = routeDock->widget(); routePanel && !qobject_cast<QScrollArea*>(routePanel)) {
            routeDock->setWidget(nullptr);
            auto* scroll = makeVerticalScrollArea(routePanel, routeDock, "routeProgramScrollArea");
            routeDock->setWidget(scroll);
        }
        if (auto* fleetDock = findChild<QDockWidget*>("fleetDock")) {
            tabifyDockWidget(fleetDock, routeDock);
            fleetDock->raise();
        }
    }

    setDockOptions(QMainWindow::AnimatedDocks
        | QMainWindow::AllowTabbedDocks
        | QMainWindow::AllowNestedDocks);
    setMinimumSize(900, 600);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    QFont compactFont = font();
    if (compactFont.pointSizeF() > 0.0) {
        compactFont.setPointSizeF(std::max(8.5, compactFont.pointSizeF() - 0.5));
        setFont(compactFont);
    }

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
        QGroupBox {
            margin-top: 10px;
            padding: 7px 5px 5px 5px;
            border: 1px solid #2a4056;
            border-radius: 5px;
            font-weight: 600;
            background: #101925;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 9px;
            padding: 0 5px;
            color: #a9bdd0;
        }
        QGroupBox#empireGroup { border-color: #4d5f82; }
        QGroupBox#planetGroup { border-color: #477660; }
        QGroupBox#fleetGroup { border-color: #43749a; }
        QGroupBox#productionGroup { border-color: #665b86; }
        QGroupBox#ordersGroup {
            border-color: #a37748;
            background: #171a20;
        }
        QGroupBox#ordersGroup::title { color: #e4b77d; }
        QGroupBox#routeSummaryGroup { border-color: #3f6684; }
        QGroupBox#routeWaypointGroup { border-color: #4a7797; }
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
        QPushButton#primaryTurnButton {
            min-height: 29px;
            font-weight: 700;
            color: #fff0dc;
            background: #5a3d25;
            border-color: #a8794b;
        }
        QPushButton#primaryTurnButton:hover {
            background: #704b2d;
            border-color: #d09b62;
        }
        QPushButton#routeAddButton {
            font-weight: 700;
            background: #183a52;
            border-color: #4f91bd;
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
        QMainWindow QTabBar {
            background: #090f16;
        }
        QMainWindow QTabBar::tab {
            min-width: 120px;
            min-height: 25px;
            padding: 5px 12px;
            margin-right: 2px;
            color: #9fb1c3;
            background: #111c28;
            border: 1px solid #2d4358;
            border-bottom: 2px solid #2d4358;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QMainWindow QTabBar::tab:hover:!selected {
            color: #e5eff8;
            background: #1b3042;
            border-color: #4f7899;
        }
        QMainWindow QTabBar::tab:selected {
            color: #ffffff;
            font-weight: 700;
            background: #274d69;
            border-color: #70b9e8;
            border-bottom: 3px solid #70b9e8;
        }
        QLabel#homeworldDistance {
            padding: 2px 8px;
            color: #f0d59d;
            border-left: 1px solid #45566a;
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

    // Rare galaxy creation controls live in a menu rather than occupying prime
    // command-panel space for the entire game.
    auto* gameMenu = menuBar()->addMenu("&Game");
    auto* newGalaxyAction = gameMenu->addAction("New galaxy…");
    newGalaxyAction->setShortcut(QKeySequence::New);
    connect(newGalaxyAction, &QAction::triggered, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle("New Galaxy");
        auto* layout = new QVBoxLayout(&dialog);
        auto* form = new QFormLayout;
        auto* seed = new QLineEdit(seedEdit_->text(), &dialog);
        auto* systems = new QSpinBox(&dialog);
        systems->setRange(8, 64);
        systems->setValue(starCountSpin_->value());
        form->addRow("Seed", seed);
        form->addRow("Star systems", systems);
        layout->addLayout(form);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) return;
        bool seedOk = false;
        seed->text().toULongLong(&seedOk);
        if (!seedOk) {
            statusBar()->showMessage("Galaxy seed must be an unsigned 64-bit integer", 3000);
            return;
        }
        seedEdit_->setText(seed->text());
        starCountSpin_->setValue(systems->value());
        newGalaxy();
        fitGalaxyView();
    });

    auto* restartAction = gameMenu->addAction("Restart current galaxy");
    connect(restartAction, &QAction::triggered, this, [this] {
        newGalaxy();
        fitGalaxyView();
    });

    auto* fleetMenu = menuBar()->addMenu("&Fleet");
    auto* cargoAction = fleetMenu->addAction("Transfer cargo…");
    connect(cargoAction, &QAction::triggered, this, [this] { openCargoManifestDialog(); });
    auto* mergeAction = fleetMenu->addAction("Merge fleets…");
    connect(mergeAction, &QAction::triggered, this, [this] { openMergeFleetsDialog(); });
    auto* splitAction = fleetMenu->addAction("Split fleet…");
    connect(splitAction, &QAction::triggered, this, [this] { openSplitFleetDialog(); });
    fleetMenu->addSeparator();
    auto* designerAction = fleetMenu->addAction("Ship designer…");
    connect(designerAction, &QAction::triggered, this, [this] { openShipDesigner(); });

    auto* dialogToolbar = addToolBar("Dialogs");
    dialogToolbar->setObjectName("dialogToolbar");
    dialogToolbar->setMovable(false);
    dialogToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto* designerToolAction = dialogToolbar->addAction("Designer");
    designerToolAction->setObjectName("openShipDesignerToolAction");
    designerToolAction->setToolTip("Open the graphical Ship Designer");
    connect(designerToolAction, &QAction::triggered, this, [this] { openShipDesigner(); });

    auto* researchToolAction = dialogToolbar->addAction("Research");
    researchToolAction->setObjectName("openResearchToolAction");
    researchToolAction->setToolTip("Edit empire research allocation and the ordered technology plan");
    connect(researchToolAction, &QAction::triggered, this, [this] { openResearchDialog(); });

    dialogToolbar->addSeparator();

    auto* cargoToolAction = dialogToolbar->addAction("Cargo");
    cargoToolAction->setToolTip("Transfer colonists and minerals at the selected system");
    connect(cargoToolAction, &QAction::triggered, this, [this] { openCargoManifestDialog(); });

    auto* mergeToolAction = dialogToolbar->addAction("Merge");
    mergeToolAction->setToolTip("Merge co-located friendly fleets");
    connect(mergeToolAction, &QAction::triggered, this, [this] { openMergeFleetsDialog(); });

    auto* splitToolAction = dialogToolbar->addAction("Split");
    splitToolAction->setToolTip("Split ships from the selected fleet");
    connect(splitToolAction, &QAction::triggered, this, [this] { openSplitFleetDialog(); });

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

    // Stars! style homeworld range stays in one fixed status-bar location so
    // selecting systems never makes the player hunt for the distance readout.
    auto* homeworldDistance = new QLabel(statusBar());
    homeworldDistance->setObjectName("homeworldDistance");
    statusBar()->addPermanentWidget(homeworldDistance);

    auto updateCommandContext = [this, homeworldDistance, planetInfo, fleetInfo] {
        if (planetInfo) planetInfo->setText(selectedPlanetPanelSummary());
        if (fleetInfo) fleetInfo->setText(selectedFleetPanelSummary());

        const auto* environmentPlanet = selectedPlanet();
        const auto* environmentStar = selectedStar();
        const bool environmentKnown = environmentPlanet && environmentStar
            && is_surveyed(state_, 1, environmentStar->id);
        if (planetEnvironmentPanel_) planetEnvironmentPanel_->setVisible(environmentKnown);
        if (environmentKnown) {
            const auto setEnvironmentBar = [](QProgressBar* bar, std::uint8_t value, const QString& text) {
                if (!bar) return;
                bar->setValue(value);
                bar->setToolTip(QString("%1: %2 / 100").arg(text).arg(static_cast<int>(value)));
            };
            setEnvironmentBar(planetTemperatureBar_, environmentPlanet->environment.temperature,
                "Temperature (50 is temperate)");
            setEnvironmentBar(planetGravityBar_, environmentPlanet->environment.gravity,
                "Gravity (50 is Earth-like)");
            setEnvironmentBar(planetRadiationBar_, environmentPlanet->environment.radiation,
                "Radiation (higher is more severe)");
        }

        const auto homePlanet = std::find_if(state_.planets.begin(), state_.planets.end(), [](const Planet& planet) {
            return planet.owner == 1;
        });
        if (homePlanet == state_.planets.end()) {
            homeworldDistance->setText("Homeworld: —");
            return;
        }
        const auto* homeStar = find_star(state_, homePlanet->star);
        if (!homeStar) {
            homeworldDistance->setText("Homeworld: —");
            return;
        }

        const auto* star = selectedStar();
        QString text = QString("Homeworld: %1 / %2")
                           .arg(QString::fromStdString(homePlanet->name))
                           .arg(QString::fromStdString(homeStar->name));
        if (star) {
            text += QString("  •  %1 ly to %2")
                        .arg(distance_between(homeStar->position, star->position), 0, 'f', 1)
                        .arg(QString::fromStdString(star->name));
        }
        homeworldDistance->setText(text);
    };
    updateCommandContext();
    auto* contextTimer = new QTimer(this);
    contextTimer->setInterval(120);
    connect(contextTimer, &QTimer::timeout, this, updateCommandContext);
    contextTimer->start();
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
    shuttingDown_ = true;
    mapSelectionRebuildPending_ = false;

    const auto timers = findChildren<QTimer*>();
    for (auto* timer : timers) timer->stop();

    if (scene_) scene_->disconnect(this);
    if (view_) {
        view_->viewport()->removeEventFilter(this);
        view_->setScene(nullptr);
    }

    if (!QCoreApplication::arguments().contains("--smoke-test")) {
        QSettings settings("SunsProject", "Suns");
        settings.setValue("workspace/geometry", saveGeometry());
        settings.setValue("workspace/docks", saveState(2));
    }

    QMainWindow::closeEvent(event);
}

} // namespace suns
