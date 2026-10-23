#include "route_program_dock.hpp"

#include "main_window.hpp"
#include "colony_plan_combo.hpp"

#include <QBrush>
#include <QColor>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPen>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QShortcut>
#include <QStyle>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

namespace suns {

namespace {

constexpr int kRouteOverlayDataKey = 73;

class RouteFuelDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        auto styled = option;
        if (index.sibling(index.row(), 0).data(Qt::UserRole + 1).toBool()) {
            styled.palette.setColor(QPalette::Highlight, QColor("#763b43"));
            styled.palette.setColor(QPalette::HighlightedText, QColor("#ffcece"));
        }
        QStyledItemDelegate::paint(painter, styled, index);
    }
};

class WarpSelector final : public QProgressBar {
public:
    explicit WarpSelector(QWidget* parent = nullptr) : QProgressBar(parent)
    {
        setObjectName("routeWarpSelector");
        setRange(1, kMaxWarp);
        setValue(1);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(23);
        connect(this, &QProgressBar::valueChanged, this, [this] { updateAppearance(); });
        updateAppearance();
    }

    void setSafeWarp(int warp)
    {
        safeWarp_ = std::clamp(warp, 0, static_cast<int>(kMaxWarp));
        updateAppearance();
    }

    void setDamageRate(double damageRate)
    {
        damageRate_ = std::max(0.0, damageRate);
        updateAppearance();
    }

    void setFuelWarning(const QString& warning)
    {
        fuelWarning_ = warning;
        updateAppearance();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (!isEnabled() || event->button() != Qt::LeftButton) {
            QProgressBar::mousePressEvent(event);
            return;
        }
        const auto width = std::max(1, contentsRect().width());
        const auto fraction = std::clamp(
            (event->position().x() - contentsRect().left()) / static_cast<double>(width),
            0.0, 1.0);
        setValue(std::clamp(
            minimum() + static_cast<int>(std::lround(fraction * (maximum() - minimum()))),
            minimum(), maximum()));
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Down: setValue(std::max(minimum(), value() - 1)); return;
        case Qt::Key_Right:
        case Qt::Key_Up: setValue(std::min(maximum(), value() + 1)); return;
        case Qt::Key_Home: setValue(minimum()); return;
        case Qt::Key_End: setValue(maximum()); return;
        default: QProgressBar::keyPressEvent(event); return;
        }
    }

private:
    void updateAppearance()
    {
        const bool unsafe = damageRate_ > 1e-8;
        const bool fuelLimited = !fuelWarning_.isEmpty();
        if (property("unsafe").toBool() != unsafe || property("fuelLimited").toBool() != fuelLimited) {
            setProperty("unsafe", unsafe);
            setProperty("fuelLimited", fuelLimited);
            style()->unpolish(this);
            style()->polish(this);
        }
        setFormat((unsafe
            ? QString("W%1 • +%2% damage/turn").arg(value()).arg(damageRate_, 0, 'f', 0)
            : QString("W%1 • base W%2").arg(value()).arg(safeWarp_))
            + (fuelLimited ? " • fuel!" : ""));
        setToolTip((unsafe
            ? QString("Hull risk at the reported position: base rating W%1, selected W%2. Check the route forecast for clouds along the path.")
                  .arg(safeWarp_).arg(value())
            : QString("Selected W%1; base rating W%2. No local risk in received profiles; check the complete route forecast.").arg(value()).arg(safeWarp_))
            + (fuelLimited ? "\n" + fuelWarning_ : "\nFuel preview covers up to 96 years; no warning does not guarantee arrival."));
    }

    int safeWarp_{1};
    double damageRate_{};
    QString fuelWarning_;
};

void clearRouteOverlay(QGraphicsScene* scene)
{
    if (!scene) return;
    const auto items = scene->items();
    for (auto* item : items) {
        if (item->data(kRouteOverlayDataKey).toBool()) delete item;
    }
}

void drawRouteOverlay(MainWindow& window)
{
    auto* scene = window.routeProgramScene();
    if (!scene) return;

    clearRouteOverlay(scene);
    const auto points = window.selectedFleetRouteProgramPolyline();
    if (points.size() < 2) return;

    QPen pen(QColor(110, 190, 255, 175));
    pen.setWidthF(1.6);
    pen.setStyle(Qt::DashDotLine);

    for (std::size_t index = 1; index < points.size(); ++index) {
        const auto& from = points[index - 1];
        const auto& to = points[index];
        auto* line = scene->addLine(from.x, from.y, to.x, to.y, pen);
        line->setData(kRouteOverlayDataKey, true);
        line->setZValue(-10.0);

        auto* label = scene->addText(QString("%1").arg(static_cast<qulonglong>(index)));
        label->setDefaultTextColor(QColor(150, 215, 255, 210));
        label->setScale(0.68);
        label->setPos(to.x + 5.0, to.y + 3.0);
        label->setData(kRouteOverlayDataKey, true);
        label->setZValue(12.0);
    }
}

FleetArrivalAction actionFromControls(
    QComboBox* actionCombo, QComboBox* cargoCombo, QSpinBox* reserveSpin, QDoubleSpinBox* fuelReserveSpin, ColonyPlanCombo* colonyPlan)
{
    FleetArrivalAction action;
    action.kind = static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt());
    action.reservePopulation = static_cast<std::uint64_t>(std::max(0, reserveSpin->value()));
    action.cargo = static_cast<FleetCargoKind>(cargoCombo->currentData().toInt());
    if (action.kind == FleetArrivalActionKind::Colonize) action.colonyPlan = colonyPlan->plan();
    if (action.kind == FleetArrivalActionKind::SupplyOutpost) action.reserveFuel = fuelReserveSpin->value();
    return action;
}

} // namespace

void attachRouteProgramDock(MainWindow& window)
{
    auto* dock = new QDockWidget("Fleet Route Program", &window);
    dock->setObjectName("fleetRouteProgramDock");
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* panel = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    panel->setObjectName("routeProgramPanel");

    const auto helpText = QString(
        "Select the fleet to program, click Pick target on map, then click a star or another fleet. "
        "Choose Warp and an arrival action, then add the waypoint. Esc cancels map-target mode. "
        "For a faster order, right-click a star or fleet to append it immediately with the current settings. "
        "Selecting a system lists it and all visible orbiting fleets in Destination; enemies are red.\n\n"
        "Select a route row to change that leg's Warp. New waypoint returns to adding route points. "
        "Fuel used and Fuel left are projected per leg; red marks the first leg with insufficient fuel at its ordered Warp. "
        "Each leg keeps its own Warp and arrival action. Friendly moving targets are resolved every turn. Merge absorbs the pursuing fleet into the target fleet. "
        "Mining, mine laying/sweeping, bombardment, field research and wreck recovery are persistent final-waypoint tasks. They begin on arrival without another command. Stop/new travel cancels work without changing emissions. Load and unload use the real surface stockpile on arrival. Repeat Orders repeats the whole route; terminal actions cannot be repeated. "
        "Colonization dismantles the entire fleet and recovers 33% of its ship minerals. Dockside loading and refuelling are in Fleet Logistics.");
    auto* headingRow = new QHBoxLayout;
    auto* heading = new QLabel("Program orders for one fleet", panel);
    heading->setObjectName("routeProgramHeading");
    auto* helpButton = new QToolButton(panel);
    helpButton->setObjectName("routeProgramHelpButton");
    helpButton->setText("?");
    helpButton->setToolTip(helpText);
    helpButton->setAccessibleName("Route Program help");
    headingRow->addWidget(heading);
    headingRow->addStretch(1);
    headingRow->addWidget(helpButton);
    layout->addLayout(headingRow);

    auto* sourceFleetCombo = new QComboBox(panel);
    sourceFleetCombo->setObjectName("routeSourceFleetCombo");
    auto* sourceForm = new QFormLayout;
    sourceForm->addRow("Fleet to program", sourceFleetCombo);
    layout->addLayout(sourceForm);

    auto* routeGroup = new QGroupBox("Current program", panel);
    routeGroup->setObjectName("routeSummaryGroup");
    auto* routeLayout = new QVBoxLayout(routeGroup);
    routeLayout->setContentsMargins(6, 6, 6, 6);
    routeLayout->setSpacing(3);
    auto* routeTree = new QTreeWidget(routeGroup);
    routeTree->setObjectName("routeProgramQueue");
    routeTree->setItemDelegate(new RouteFuelDelegate(routeTree));
    routeTree->setColumnCount(6);
    routeTree->setHeaderLabels({"#", "Destination", "Warp", "ETA", "Fuel", "Left"});
    routeTree->headerItem()->setToolTip(3, "Estimated arrival in years from the planning turn");
    routeTree->setRootIsDecorated(false);
    routeTree->setAlternatingRowColors(true);
    routeTree->setSelectionMode(QAbstractItemView::SingleSelection);
    routeTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeTree->header()->setStretchLastSection(false);
    routeTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    routeTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    routeTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    routeTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    routeTree->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    routeTree->setTextElideMode(Qt::ElideMiddle);
    routeTree->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    routeTree->headerItem()->setToolTip(4, "Estimated movement fuel on this leg (mg). A range covers uncertain medium density; hover for the received-profile estimate. ≥ means an incomplete forecast.");
    routeTree->headerItem()->setToolTip(5, "Projected fuel remaining at arrival (mg), including any orbital refuelling");
    routeTree->setFixedHeight(100);
    routeLayout->addWidget(routeTree);
    auto* selectedAction = new QLabel("Select a waypoint to view its arrival action.", routeGroup);
    selectedAction->setObjectName("routeSelectedArrivalAction");
    selectedAction->setWordWrap(true);
    selectedAction->setTextFormat(Qt::PlainText);
    routeLayout->addWidget(selectedAction);
    QObject::connect(routeTree, &QTreeWidget::itemSelectionChanged, panel, [=] {
        const auto selected = routeTree->selectedItems();
        selectedAction->setText(selected.isEmpty()
            ? "Select a waypoint to view its arrival action."
            : QString("On arrival: %1").arg(selected.front()->data(0, Qt::UserRole).toString()));
    });
    auto* routeButtons = new QHBoxLayout;
    auto* moveUpButton = new QPushButton("Move up", routeGroup);
    auto* moveDownButton = new QPushButton("Move down", routeGroup);
    auto* removeButton = new QPushButton("Remove", routeGroup);
    auto* newWaypointButton = new QPushButton("New waypoint", routeGroup);
    newWaypointButton->setObjectName("routeNewWaypointButton");
    newWaypointButton->setToolTip("Leave route-leg editing and prepare a new waypoint");
    routeButtons->addWidget(moveUpButton);
    routeButtons->addWidget(moveDownButton);
    routeButtons->addWidget(removeButton);
    routeButtons->addWidget(newWaypointButton);
    moveUpButton->setEnabled(false);
    moveDownButton->setEnabled(false);
    removeButton->setEnabled(false);
    routeLayout->addLayout(routeButtons);
    auto* routeLabel = new QLabel(routeGroup);
    routeLabel->setWordWrap(true);
    routeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // The forecast dialog contains the full summary without consuming the
    // working area needed for waypoint controls.
    routeLabel->hide();
    routeLayout->addWidget(routeLabel);
    auto* forecastButton = new QPushButton("Route forecast…", routeGroup);
    routeLayout->addWidget(forecastButton);
    QObject::connect(forecastButton, &QPushButton::clicked, panel, [&window] {
        QMessageBox dialog(&window);
        dialog.setWindowTitle("Route forecast");
        dialog.setTextFormat(Qt::RichText);
        dialog.setText(window.selectedFleetRouteProgramForecast());
        dialog.exec();
    });
    layout->addWidget(routeGroup);

    auto* waypointGroup = new QGroupBox("Add waypoint", panel);
    waypointGroup->setObjectName("routeWaypointGroup");
    auto* waypointLayout = new QVBoxLayout(waypointGroup);
    waypointLayout->setContentsMargins(6, 6, 6, 6);
    waypointLayout->setSpacing(3);

    auto* warpSelector = new WarpSelector(waypointGroup);
    QObject::connect(warpSelector, &QProgressBar::valueChanged, panel,
        [&window, warpSelector](int warp) {
            warpSelector->setDamageRate(window.selectedFleetOverdriveDamageForRouteProgram(
                static_cast<std::uint8_t>(warp)));
        });
    warpSelector->setStyleSheet(R"(
        QProgressBar#routeWarpSelector {
            color: #eef6ff;
            background: #0b131d;
            border: 1px solid #3b5870;
            border-radius: 3px;
            text-align: center;
        }
        QProgressBar#routeWarpSelector::chunk { background: #409bc5; }
        QProgressBar#routeWarpSelector[unsafe="true"]::chunk { background: #c94747; }
        QProgressBar#routeWarpSelector[fuelLimited="true"]::chunk { background: #c94747; }
    )");
    auto* warpHelpButton = new QToolButton(waypointGroup);
    warpHelpButton->setText("?");
    warpHelpButton->setAccessibleName("Unsafe Warp explanation");
    warpHelpButton->setToolTip("Why does the Warp selector turn red?");
    auto* warpRow = new QWidget(waypointGroup);
    auto* warpRowLayout = new QHBoxLayout(warpRow);
    warpRowLayout->setContentsMargins(0, 0, 0, 0);
    warpRowLayout->addWidget(warpSelector, 1);
    warpRowLayout->addWidget(warpHelpButton);
    auto* targetTypeCombo = new QComboBox(waypointGroup);
    targetTypeCombo->setObjectName("routeTargetTypeCombo");
    targetTypeCombo->addItem("Selected star", 0);
    targetTypeCombo->addItem("Friendly fleet", 1);
    targetTypeCombo->addItem("Enemy fleet position", 2);
    auto* targetFleetCombo = new QComboBox(waypointGroup);
    targetFleetCombo->setObjectName("routeTargetFleetCombo");
    targetFleetCombo->setEnabled(false);
    // Destination/map picking drives these compatibility selectors. Unmanaged
    // visible children otherwise appear at (0,0) over the group title.
    targetTypeCombo->hide();
    targetFleetCombo->hide();
    auto* destinationCombo = new QComboBox(waypointGroup);
    destinationCombo->setObjectName("routeDestinationCombo");
    destinationCombo->setToolTip(
        "The selected system and all visible fleets currently orbiting it; enemy fleets are red");
    auto* actionCombo = new QComboBox(waypointGroup);
    actionCombo->setObjectName("routeArrivalActionCombo");
    actionCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    actionCombo->setMinimumContentsLength(16);
    actionCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    actionCombo->addItem("No action", static_cast<int>(FleetArrivalActionKind::None));
    actionCombo->addItem("Load all available", static_cast<int>(FleetArrivalActionKind::LoadAllAvailable));
    actionCombo->addItem("Unload all", static_cast<int>(FleetArrivalActionKind::UnloadAll));
    actionCombo->addItem("Colonize world", static_cast<int>(FleetArrivalActionKind::Colonize));
    actionCombo->addItem("Remote Mining (persistent)", static_cast<int>(FleetArrivalActionKind::RemoteMining));
    actionCombo->addItem("Merge with fleet", static_cast<int>(FleetArrivalActionKind::MergeWithFleet));
    actionCombo->addItem("Lay minefields (persistent)", static_cast<int>(FleetArrivalActionKind::LayMinefields));
    actionCombo->addItem("Sweep hostile mines (persistent)", static_cast<int>(FleetArrivalActionKind::SweepMinefields));
    actionCombo->addItem("Bombard enemy colony (persistent)", static_cast<int>(FleetArrivalActionKind::BombardColony));
    actionCombo->addItem("Field research (persistent)", static_cast<int>(FleetArrivalActionKind::FieldResearch));
    actionCombo->addItem("Recover wreckage (persistent)", static_cast<int>(FleetArrivalActionKind::RecoverWreckage));
    actionCombo->addItem("Refuel at depot", int(FleetArrivalActionKind::Refuel));
    actionCombo->addItem("Deploy outpost (permanent)", int(FleetArrivalActionKind::DeployOutpost));
    actionCombo->addItem("Supply fuel to outpost", int(FleetArrivalActionKind::SupplyOutpost));
    actionCombo->addItem("Recover scientific specimen", int(FleetArrivalActionKind::RecoverSample));
    const auto workTip = [&](FleetArrivalActionKind kind, const QString& text) {
        actionCombo->setItemData(actionCombo->findData(int(kind)), text, Qt::ToolTipRole);
    };
    workTip(FleetArrivalActionKind::LayMinefields, "Requires a mine layer. Lays at the final waypoint every year until stopped; the new field affects later movement years.");
    workTip(FleetArrivalActionKind::SweepMinefields, "Requires beams. Sweeps hostile fields within 10 ly of their boundary after movement; it does not protect the approach this year.");
    workTip(FleetArrivalActionKind::BombardColony, "Requires bombs and a known enemy colony. Starts after arrival/combat, repeats until stopped; friendly or empty worlds are never bombed.");
    workTip(FleetArrivalActionKind::FieldResearch, "Requires a scanner and active sensors when collecting data. One study per empire/objective: star Energy 12; deep planet Biology 12 (artifact site 32); wormhole Propulsion 24. Rewards wait for report delivery. The order does not change emissions.");
    workTip(FleetArrivalActionKind::RecoverWreckage, "Requires a scanner, cargo space and reported wreckage within 10 ly. Recovers one nearest specimen per year; only unfamiliar technology yields science. Targets can expire before arrival.");
    workTip(FleetArrivalActionKind::DeployOutpost, "One ship with one paid kit, no colonists, at least 5 ly from stars and 1 ly from another installation. Permanent; last waypoint only, no repeat.");
    workTip(FleetArrivalActionKind::SupplyOutpost, "Transfer actual fuel to an owned outpost at these coordinates, retaining the specified reserve. Receiver capacity and stock can change before arrival.");
    auto* cargoCombo = new QComboBox(waypointGroup);
    cargoCombo->setObjectName("routeCargoCombo");
    cargoCombo->addItem("Orbital colonists",static_cast<int>(FleetCargoKind::OrbitalColonists));
    cargoCombo->addItem("Colonists", static_cast<int>(FleetCargoKind::Colonists));
    cargoCombo->addItem("Ironium", static_cast<int>(FleetCargoKind::Ironium));
    cargoCombo->addItem("Boranium", static_cast<int>(FleetCargoKind::Boranium));
    cargoCombo->addItem("Germanium", static_cast<int>(FleetCargoKind::Germanium));
    cargoCombo->addItem("All cargo", static_cast<int>(FleetCargoKind::All));
    cargoCombo->addItem("Xenocrystal (1 kt/unit)", static_cast<int>(FleetCargoKind::Xenocrystal));
    cargoCombo->setEnabled(false);
    auto* reserveSpin = new QSpinBox(waypointGroup);
    reserveSpin->setObjectName("routeReserveSpin");
    reserveSpin->setRange(0, 2'000'000'000);
    reserveSpin->setValue(1000);
    reserveSpin->setEnabled(false);

    auto* fuelReserveSpin = new QDoubleSpinBox(waypointGroup);
    fuelReserveSpin->setObjectName("routeFuelReserveSpin");
    fuelReserveSpin->setRange(0, 1e12); fuelReserveSpin->setDecimals(2); fuelReserveSpin->setSuffix(" mg");
    fuelReserveSpin->setValue(100); fuelReserveSpin->setEnabled(false);
    auto* form = new QFormLayout;
    form->setVerticalSpacing(3);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow("Destination", destinationCombo);
    form->addRow("Warp", warpRow);
    form->addRow("On arrival", actionCombo);
    auto* colonyPlan = new ColonyPlanCombo(waypointGroup);
    colonyPlan->setObjectName("routeColonyPlanCombo");
    colonyPlan->setTemplates(window.colonyTemplatesForOrders());
    auto* planRow = new QWidget(waypointGroup);
    auto* planLayout = new QHBoxLayout(planRow);
    planLayout->setContentsMargins(0, 0, 0, 0);
    planLayout->addWidget(colonyPlan, 1);
    auto* editPlans = new QToolButton(planRow);
    editPlans->setText("Edit…");
    editPlans->setObjectName("routeEditColonyPlans");
    editPlans->setToolTip("Open Colony Development Templates in a separate window");
    planLayout->addWidget(editPlans);
    form->addRow("Colony plan", planRow);
    QObject::connect(editPlans, &QToolButton::clicked, panel, [&window] { window.openColonyTemplates(); });
    actionCombo->setToolTip("Select a route row to edit its arrival order, or New waypoint to choose an action before adding a destination. Persistent work must be on the final waypoint; Stop cancels work and travel without changing emissions.");
    form->addRow("Cargo", cargoCombo);
    form->addRow("Leave population", reserveSpin);
    form->addRow("Keep tanker fuel", fuelReserveSpin);
    waypointLayout->addLayout(form);

    auto* pickTargetButton = new QPushButton("Pick target on map…", waypointGroup);
    pickTargetButton->setObjectName("routePickTargetButton");
    pickTargetButton->setCheckable(true);
    pickTargetButton->setToolTip(
        "Keep the source fleet selected while choosing a star or another fleet on the map");
    waypointLayout->addWidget(pickTargetButton);
    auto* pickedTargetLabel = new QLabel(
        "Tip: use this button when the destination is another fleet.", waypointGroup);
    pickedTargetLabel->setObjectName("routePickedTargetLabel");
    pickedTargetLabel->setWordWrap(true);
    pickedTargetLabel->hide();
    waypointLayout->addWidget(pickedTargetLabel);

    auto* appendButton = new QPushButton("Add waypoint", waypointGroup);
    appendButton->setObjectName("routeAddButton");
    appendButton->setToolTip("Append the selected destination with its Warp and On arrival order.");
    auto* waypointActions = new QHBoxLayout;
    waypointActions->setSpacing(4);
    waypointActions->addWidget(appendButton, 1);
    auto* alternativesButton = new QPushButton("Compare routes…", waypointGroup);
    alternativesButton->setObjectName("routeAlternativesButton");
    alternativesButton->setToolTip("Compare the direct route and coarse cloud bypasses to the selected star, then append your choice.");
    waypointLayout->addWidget(alternativesButton);
    auto* workHereButton = new QPushButton("Start work here", waypointGroup);
    workHereButton->setObjectName("routeWorkHereButton");
    workHereButton->setToolTip("Use the selected On arrival work at this stationary fleet's reported position. Queues an ordinary route order with normal command delay; emissions stay unchanged.");
    waypointActions->addWidget(workHereButton, 1);
    waypointLayout->addLayout(waypointActions);
    layout->addWidget(waypointGroup);

    auto* repeatCheck = new QCheckBox("Repeat Orders", panel);
    repeatCheck->setToolTip("Restart the complete route after its final waypoint");
    auto* programActions = new QHBoxLayout;
    programActions->addWidget(repeatCheck);

    auto* clearButton = new QPushButton("Stop work / route", panel);
    clearButton->setObjectName("routeStopButton");
    clearButton->setToolTip("Stop travel and persistent work when this command arrives. Emission settings stay unchanged.");
    programActions->addWidget(clearButton);
    layout->addLayout(programActions);

    layout->addStretch(1);

    dock->setWidget(panel);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);

    const auto updateCargoControls = [=, &window] {
        const auto action = static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt());
        fuelReserveSpin->setEnabled(window.selectedFleetForRouteProgram() != 0 && action == FleetArrivalActionKind::SupplyOutpost);
        fuelReserveSpin->setVisible(action == FleetArrivalActionKind::SupplyOutpost);
        form->labelForField(fuelReserveSpin)->setVisible(action == FleetArrivalActionKind::SupplyOutpost);
        planRow->setVisible(action == FleetArrivalActionKind::Colonize);
        form->labelForField(planRow)->setVisible(action == FleetArrivalActionKind::Colonize);
        colonyPlan->setEnabled(window.selectedFleetForRouteProgram() != 0 && action == FleetArrivalActionKind::Colonize);
        auto cargo = static_cast<FleetCargoKind>(cargoCombo->currentData().toInt());
        if (action == FleetArrivalActionKind::LoadAllAvailable && cargo == FleetCargoKind::All) {
            const QSignalBlocker blocker(cargoCombo);
            cargoCombo->setCurrentIndex(
                cargoCombo->findData(static_cast<int>(FleetCargoKind::Colonists)));
            cargo = FleetCargoKind::Colonists;
        }
        const auto transfersCargo = action == FleetArrivalActionKind::LoadAllAvailable
            || action == FleetArrivalActionKind::UnloadAll;
        workHereButton->setEnabled(window.selectedFleetForRouteProgram() != 0 && arrival_work_task(action).has_value());
        cargoCombo->setEnabled(transfersCargo);
        reserveSpin->setEnabled(
            action == FleetArrivalActionKind::LoadAllAvailable && (cargo == FleetCargoKind::Colonists || cargo==FleetCargoKind::OrbitalColonists));
    };
    QObject::connect(actionCombo, &QComboBox::currentIndexChanged, panel, [=](int) { updateCargoControls(); });
    QObject::connect(cargoCombo, &QComboBox::currentIndexChanged, panel, [=](int) { updateCargoControls(); });
    QObject::connect(targetTypeCombo, &QComboBox::currentIndexChanged, panel, [=](int) {
        const auto targetType = targetTypeCombo->currentData().toInt();
        const bool fleetTarget = targetType != 0;
        const bool friendlyFleetTarget = targetType == 1;
        targetFleetCombo->setEnabled(fleetTarget);
        appendButton->setText(targetType == 2
            ? "Add enemy position"
            : fleetTarget ? "Add fleet target" : "Add waypoint");
        const auto currentAction = static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt());
        if (friendlyFleetTarget) {
            actionCombo->setCurrentIndex(
                actionCombo->findData(static_cast<int>(FleetArrivalActionKind::MergeWithFleet)));
        } else if (targetType == 2
            || currentAction == FleetArrivalActionKind::MergeWithFleet) {
            actionCombo->setCurrentIndex(
                actionCombo->findData(static_cast<int>(FleetArrivalActionKind::None)));
        }
    });
    const auto applyDestinationChoice = [=] {
        const auto index = destinationCombo->currentIndex();
        if (index < 0) return;
        const auto kind = destinationCombo->itemData(index, Qt::UserRole).toInt();
        const auto id = destinationCombo->itemData(index, Qt::UserRole + 1).toUInt();
        targetTypeCombo->setCurrentIndex(
            targetTypeCombo->findData(kind == 3 ? 2 : kind == 2 ? 1 : 0));
        if (kind == 2 || kind == 3) {
            targetFleetCombo->setCurrentIndex(
                targetFleetCombo->findData(static_cast<quint32>(id)));
        }
    };
    const auto rebuildDestinationChoices =
        [=, &window](int preferredKind, std::uint32_t preferredId) {
            const QSignalBlocker blocker(destinationCombo);
            destinationCombo->clear();
            const auto options = window.routeProgramTargetsAtSelectedSystem();
            for (const auto& option : options) {
                destinationCombo->addItem(option.label);
                const auto index = destinationCombo->count() - 1;
                destinationCombo->setItemData(index, option.kind, Qt::UserRole);
                destinationCombo->setItemData(
                    index, static_cast<quint32>(option.id), Qt::UserRole + 1);
                if (option.enemy) {
                    destinationCombo->setItemData(
                        index, QBrush(QColor("#e05252")), Qt::ForegroundRole);
                }
            }

            auto preferredIndex = -1;
            for (auto index = 0; index < destinationCombo->count(); ++index) {
                const auto kind = destinationCombo->itemData(index, Qt::UserRole).toInt();
                const auto id = destinationCombo->itemData(index, Qt::UserRole + 1).toUInt();
                if (kind == preferredKind
                    && (preferredKind == 1 || id == preferredId)) {
                    preferredIndex = index;
                    break;
                }
            }
            if ((preferredKind == 2 || preferredKind == 3)
                && preferredId != 0 && preferredIndex < 0) {
                destinationCombo->addItem(QString(preferredKind == 3
                    ? "Enemy fleet — %1" : "Fleet — %1")
                    .arg(window.routeProgramMapTargetName(2, preferredId)));
                preferredIndex = destinationCombo->count() - 1;
                destinationCombo->setItemData(preferredIndex, preferredKind, Qt::UserRole);
                destinationCombo->setItemData(
                    preferredIndex, static_cast<quint32>(preferredId), Qt::UserRole + 1);
                if (preferredKind == 3) {
                    destinationCombo->setItemData(
                        preferredIndex, QBrush(QColor("#e05252")), Qt::ForegroundRole);
                }
            }
            destinationCombo->setCurrentIndex(
                preferredIndex >= 0 ? preferredIndex
                                    : (destinationCombo->count() > 0 ? 0 : -1));
            applyDestinationChoice();
        };
    QObject::connect(destinationCombo, &QComboBox::currentIndexChanged, panel,
        [=](int) { applyDestinationChoice(); });
    // These controls describe an unsent waypoint, not a global arrival policy.
    // Keep the complete draft under its source FleetId, and switch it before
    // another UI action can run (the map itself may redraw later).
    struct WaypointDraft {
        int warp{1};
        FleetArrivalAction action{FleetArrivalActionKind::None, 1000};
        int targetType{};
        FleetId targetFleet{};
    };
    struct EditorState {
        FleetId fleet{};
        std::unordered_map<FleetId, WaypointDraft> drafts;
        bool editingLeg{};
        FleetArrivalAction draftAction{FleetArrivalActionKind::None, 1000};
        int draftWarp{1};
    };
    const auto editor = std::make_shared<EditorState>();
    const auto syncEditor = [=, &window](bool resetDrafts) {
        colonyPlan->setTemplates(window.colonyTemplatesForOrders());
        const auto fleet = window.selectedFleetForRouteProgram();
        const auto sources = window.availableOwnedFleetsForRouteProgram();
        if (resetDrafts) {
            editor->drafts.clear();
            editor->fleet = 0;
        } else if (editor->fleet != 0 && !editor->editingLeg) {
            editor->drafts[editor->fleet] = {
                editor->editingLeg ? editor->draftWarp : warpSelector->value(),
                actionFromControls(actionCombo, cargoCombo, reserveSpin, fuelReserveSpin, colonyPlan),
                targetTypeCombo->currentData().toInt(),
                static_cast<FleetId>(targetFleetCombo->currentData().toUInt())};
        }
        std::erase_if(editor->drafts, [&](const auto& entry) {
            return std::find(sources.begin(), sources.end(), entry.first) == sources.end();
        });

        const QSignalBlocker sourceBlocker(sourceFleetCombo);
        sourceFleetCombo->clear();
        const auto localSources = window.ownedFleetsAtSelectedSystem();
        const auto addHeading = [sourceFleetCombo](const QString& text) {
            const auto index = sourceFleetCombo->count();
            sourceFleetCombo->addItem(text);
            if (auto* model = qobject_cast<QStandardItemModel*>(sourceFleetCombo->model()))
                model->item(index)->setFlags(Qt::NoItemFlags);
        };
        const auto addSource = [sourceFleetCombo, &window](FleetId source) {
            sourceFleetCombo->addItem(window.fleetSourceNameForRouteProgram(source),
                static_cast<quint32>(source));
            const auto index = sourceFleetCombo->count() - 1;
            sourceFleetCombo->setItemData(index, sourceFleetCombo->itemText(index), Qt::ToolTipRole);
        };
        addHeading(window.selectedSystemFleetHeading());
        for (const auto source : localSources) addSource(source);
        if (!window.routeProgramTargetsAtSelectedSystem().empty() && sources.size() > localSources.size())
            addHeading("Other fleets");
        for (const auto source : sources)
            if (std::find(localSources.begin(), localSources.end(), source) == localSources.end()) addSource(source);
        sourceFleetCombo->setToolTip(window.selectedSystemFleetHeading()
            + "\nOrbiting fleets appear first. Choosing a system keeps the current source fleet.");
        if (auto* context = window.findChild<QLabel*>("fleetSystemContext"))
            context->setText(window.selectedSystemFleetHeading());
        sourceFleetCombo->setCurrentIndex(sourceFleetCombo->findData(static_cast<quint32>(fleet)));
        sourceFleetCombo->setPlaceholderText(sources.empty() ? "No friendly fleets" : "Select a fleet");
        sourceFleetCombo->setEnabled(!sources.empty());

        if (resetDrafts || fleet != editor->fleet) {
            const QSignalBlocker rowBlocker(routeTree);
            routeTree->clearSelection();
            routeTree->setCurrentItem(nullptr);
            editor->editingLeg = false;
            auto draft = WaypointDraft{};
            draft.warp = window.selectedFleetSuggestedWarpForRouteProgram();
            if (const auto found = editor->drafts.find(fleet); found != editor->drafts.end()) {
                draft = found->second;
            }
            const QSignalBlocker actionBlocker(actionCombo);
            const QSignalBlocker cargoBlocker(cargoCombo);
            const QSignalBlocker reserveBlocker(reserveSpin);
            const QSignalBlocker fuelBlocker(fuelReserveSpin);
            const QSignalBlocker targetTypeBlocker(targetTypeCombo);
            const QSignalBlocker targetBlocker(targetFleetCombo);
            const QSignalBlocker warpBlocker(warpSelector);
            actionCombo->setCurrentIndex(actionCombo->findData(static_cast<int>(draft.action.kind)));
            cargoCombo->setCurrentIndex(cargoCombo->findData(static_cast<int>(draft.action.cargo)));
            reserveSpin->setValue(static_cast<int>(draft.action.reservePopulation));
            fuelReserveSpin->setValue(draft.action.reserveFuel);
            colonyPlan->setPlan(draft.action.colonyPlan);
            targetTypeCombo->setCurrentIndex(targetTypeCombo->findData(draft.targetType));
            targetFleetCombo->clear();
            for (const auto target : window.availableFleetTargetsForRouteProgram()) {
                targetFleetCombo->addItem(window.fleetTargetNameForRouteProgram(target),
                    static_cast<quint32>(target));
            }
            targetFleetCombo->setCurrentIndex(targetFleetCombo->findData(static_cast<quint32>(draft.targetFleet)));
            warpSelector->setValue(draft.warp);
            editor->draftWarp = draft.warp;
            editor->fleet = fleet;
            if (draft.targetType == 1 && draft.targetFleet != 0) {
                pickedTargetLabel->setText(QString("Target: <b>%1</b>")
                    .arg(window.routeProgramMapTargetName(2, draft.targetFleet).toHtmlEscaped()));
            } else {
                pickedTargetLabel->setText(
                    "Tip: use Pick target on map when the destination is another fleet.");
            }
        }
        const auto targetType = targetTypeCombo->currentData().toInt();
        const auto preferredKind = targetType == 2 ? 3 : targetType == 1 ? 2 : 1;
        const auto preferredId = preferredKind == 2 || preferredKind == 3
            ? static_cast<std::uint32_t>(targetFleetCombo->currentData().toUInt())
            : 0U;
        rebuildDestinationChoices(preferredKind, preferredId);
        const auto currentTargetType = targetTypeCombo->currentData().toInt();
        const bool fleetTarget = currentTargetType != 0;
        targetFleetCombo->setEnabled(fleet != 0 && fleetTarget && targetFleetCombo->count() > 0);
        appendButton->setText(currentTargetType == 2
            ? "Add enemy position"
            : fleetTarget ? "Add fleet target" : "Add waypoint");
        updateCargoControls();
        clearButton->setEnabled(fleet != 0);
        repeatCheck->setEnabled(fleet != 0);
        {
            const QSignalBlocker repeatBlocker(repeatCheck);
            repeatCheck->setChecked(window.selectedFleetRepeatOrdersForRouteProgram());
        }
        actionCombo->setEnabled(fleet != 0);
        targetTypeCombo->setEnabled(fleet != 0);
        destinationCombo->setEnabled(fleet != 0 && destinationCombo->count() > 0);
        pickTargetButton->setEnabled(fleet != 0);
        if (fleet == 0) {
            cargoCombo->setEnabled(false);
            reserveSpin->setEnabled(false);
        }
        const auto maxWarp = window.selectedFleetMaxWarpForRouteProgram();
        warpSelector->setSafeWarp(maxWarp);
        warpSelector->setDamageRate(window.selectedFleetOverdriveDamageForRouteProgram(
            static_cast<std::uint8_t>(warpSelector->value())));
        warpSelector->setEnabled(fleet != 0 && maxWarp > 0);
        appendButton->setEnabled(fleet != 0 && maxWarp > 0
            && destinationCombo->currentIndex() >= 0
            && (!fleetTarget || targetFleetCombo->currentIndex() >= 0));
        if (editor->editingLeg) {
            actionCombo->setEnabled(true);
            cargoCombo->setEnabled(false);
            reserveSpin->setEnabled(false);
            destinationCombo->setEnabled(false);
            pickTargetButton->setEnabled(false);
            appendButton->setEnabled(false);
        }
        waypointGroup->setTitle(editor->editingLeg ? "Edit waypoint — Warp / On arrival" : "Add waypoint");
    };
    QObject::connect(&window, &MainWindow::routeProgramContextChanged, panel, syncEditor);
    syncEditor(false);

    const auto refreshFuelWarning = [=, &window] {
        if (editor->editingLeg) {
            const auto row = routeTree->currentIndex().row();
            const auto rows = window.selectedFleetRouteProgramRows();
            warpSelector->setFuelWarning(row >= 0 && static_cast<std::size_t>(row) < rows.size()
                && rows[static_cast<std::size_t>(row)].fuelShortage
                ? rows[static_cast<std::size_t>(row)].fuelTooltip : QString{});
            return;
        }
        const bool fleetTarget = targetTypeCombo->currentData().toInt() != 0;
        if (window.selectedFleetForRouteProgram() == 0 || destinationCombo->currentIndex() < 0
            || (fleetTarget && targetFleetCombo->currentIndex() < 0)) {
            warpSelector->setFuelWarning({});
            return;
        }
        warpSelector->setFuelWarning(window.selectedFleetWaypointFuelWarning(
            static_cast<std::uint8_t>(warpSelector->value()),
            fleetTarget ? static_cast<FleetId>(targetFleetCombo->currentData().toUInt()) : 0,
            actionFromControls(actionCombo, cargoCombo, reserveSpin, fuelReserveSpin, colonyPlan)));
    };
    QObject::connect(warpSelector, &QProgressBar::valueChanged, panel, refreshFuelWarning);
    QObject::connect(actionCombo, &QComboBox::currentIndexChanged, panel, refreshFuelWarning);
    QObject::connect(cargoCombo, &QComboBox::currentIndexChanged, panel, refreshFuelWarning);
    QObject::connect(reserveSpin, &QSpinBox::valueChanged, panel, refreshFuelWarning);
    QObject::connect(fuelReserveSpin, &QDoubleSpinBox::valueChanged, panel, refreshFuelWarning);
    QObject::connect(destinationCombo, &QComboBox::currentIndexChanged, panel, refreshFuelWarning);
    QObject::connect(targetFleetCombo, &QComboBox::currentIndexChanged, panel, refreshFuelWarning);
    QObject::connect(&window, &MainWindow::routeProgramContextChanged, panel, refreshFuelWarning);
    refreshFuelWarning();

    QObject::connect(pickTargetButton, &QPushButton::clicked, panel,
        [&window, pickTargetButton](bool enabled) {
            if (enabled) {
                if (!window.beginRouteProgramMapTargetPick()) {
                    const QSignalBlocker blocker(pickTargetButton);
                    pickTargetButton->setChecked(false);
                }
            } else {
                window.cancelRouteProgramMapTargetPick();
            }
        });
    QObject::connect(&window, &MainWindow::routeProgramMapTargetPickChanged, panel,
        [pickTargetButton, pickedTargetLabel](bool active) {
            const QSignalBlocker blocker(pickTargetButton);
            pickTargetButton->setChecked(active);
            pickTargetButton->setText(active ? "Cancel target picking" : "Pick target on map…");
            if (active) {
                pickedTargetLabel->show();
                pickedTargetLabel->setText(
                    "Click a star or another fleet on the map. Esc cancels.");
            } else if (pickedTargetLabel->text().startsWith("Click a star")) {
                pickedTargetLabel->setText("Target picking canceled; current target unchanged.");
            }
        });
    QObject::connect(&window, &MainWindow::routeProgramMapTargetPicked, panel,
        [=, &window](int kind, std::uint32_t id) {
            rebuildDestinationChoices(kind, id);
            pickedTargetLabel->show();
            pickedTargetLabel->setText(QString("Target: <b>%1</b>")
                .arg(window.routeProgramMapTargetName(kind, id).toHtmlEscaped()));
        });
    auto* cancelTargetShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), panel);
    QObject::connect(cancelTargetShortcut, &QShortcut::activated, panel,
        [&window] { window.cancelRouteProgramMapTargetPick(); });
    QObject::connect(dock, &QDockWidget::visibilityChanged, panel,
        [&window](bool visible) {
            if (!visible) window.cancelRouteProgramMapTargetPick();
        });

    QObject::connect(warpHelpButton, &QToolButton::clicked, panel, [&window] {
        QMessageBox::information(
            &window,
            "Unsafe Warp overdrive",
            "Every fleet has a base safe Warp rating set by the least tolerant fitted engine. "
            "Cloud density shifts safe Warp by up to one: down for conventional engines, up for scoops. "
            "You may still order any speed through Warp 10. Above the safe limit the selector turns red: "
            "fuel use rises sharply and hull damage accumulates on every turn spent moving. "
            "The exact damage rate is determined by the fitted engine; at 100% damage the fleet is immobilized. "
            "A friendly orbital shipyard repairs 20% damage per year.\n\n"
            "Red also warns when the next waypoint's route preview predicts a fuel shortage. "
            "Hover over the selector for the predicted turn and waypoint. The preview includes earlier route legs and refuelling, up to 96 years.");
    });

    const auto selectedRouteIndex = [&window, routeTree]() -> std::optional<std::size_t> {
        if (routeTree->property("fleetId").toUInt() != window.selectedFleetForRouteProgram()) {
            return std::nullopt;
        }
        const auto selected = routeTree->selectedItems();
        if (selected.isEmpty()) return std::nullopt;
        const auto row = routeTree->indexOfTopLevelItem(selected.front());
        return row >= 0 ? std::optional<std::size_t>{static_cast<std::size_t>(row)} : std::nullopt;
    };
    const auto syncRouteSelection = [=, &window] {
        const auto row = selectedRouteIndex();
        const auto rows = window.selectedFleetRouteProgramRows();
        const QSignalBlocker warpBlocker(warpSelector);
        if (row && *row < rows.size()) {
            if (!editor->editingLeg) {
                editor->draftWarp = warpSelector->value();
                editor->draftAction = actionFromControls(actionCombo, cargoCombo, reserveSpin, fuelReserveSpin, colonyPlan);
            }
            editor->editingLeg = true;
            warpSelector->setValue(rows[*row].warp);
            if (const auto selected = window.selectedFleetRouteProgramLegAction(*row)) {
                const QSignalBlocker actionBlocker(actionCombo);
                const QSignalBlocker cargoBlocker(cargoCombo);
                const QSignalBlocker reserveBlocker(reserveSpin);
                const QSignalBlocker fuelBlocker(fuelReserveSpin);
                actionCombo->setCurrentIndex(actionCombo->findData(static_cast<int>(selected->kind)));
                cargoCombo->setCurrentIndex(cargoCombo->findData(static_cast<int>(selected->cargo)));
                reserveSpin->setValue(static_cast<int>(selected->reservePopulation));
                fuelReserveSpin->setValue(selected->reserveFuel);
                colonyPlan->setPlan(selected->colonyPlan);
            }
        } else if (editor->editingLeg) {
            editor->editingLeg = false;
            warpSelector->setValue(editor->draftWarp);
            const QSignalBlocker actionBlocker(actionCombo);
            const QSignalBlocker cargoBlocker(cargoCombo);
            const QSignalBlocker reserveBlocker(reserveSpin);
            const QSignalBlocker fuelBlocker(fuelReserveSpin);
            actionCombo->setCurrentIndex(actionCombo->findData(static_cast<int>(editor->draftAction.kind)));
            cargoCombo->setCurrentIndex(cargoCombo->findData(static_cast<int>(editor->draftAction.cargo)));
            reserveSpin->setValue(static_cast<int>(editor->draftAction.reservePopulation));
            fuelReserveSpin->setValue(editor->draftAction.reserveFuel);
            colonyPlan->setPlan(editor->draftAction.colonyPlan);
        }
        syncEditor(false);
        refreshFuelWarning();
    };
    QObject::connect(routeTree, &QTreeWidget::itemSelectionChanged, panel, syncRouteSelection);
    const auto editLegAction = [=, &window](int) {
            if (!editor->editingLeg) return;
            const auto row = selectedRouteIndex();
            if (!row) return;
            if (!window.setSelectedFleetRouteProgramLegAction(*row,
                    actionFromControls(actionCombo, cargoCombo, reserveSpin, fuelReserveSpin, colonyPlan))) {
                if (const auto original = window.selectedFleetRouteProgramLegAction(*row)) {
                    const QSignalBlocker blocked(actionCombo);
                    actionCombo->setCurrentIndex(actionCombo->findData(static_cast<int>(original->kind)));
                    colonyPlan->setPlan(original->colonyPlan);
                }
            }
        };
    QObject::connect(actionCombo, &QComboBox::currentIndexChanged, panel, editLegAction);
    QObject::connect(fuelReserveSpin, &QDoubleSpinBox::valueChanged, panel, editLegAction);
    QObject::connect(colonyPlan, &QComboBox::currentIndexChanged, panel, editLegAction);
    QObject::connect(newWaypointButton, &QPushButton::clicked, panel, [=] {
        routeTree->clearSelection();
        routeTree->setCurrentItem(nullptr);
        syncRouteSelection();
    });
    const auto refreshRouteTable = [=, &window, lastRouteSignature = std::make_shared<QString>()] {
        const auto selectedFleet = window.selectedFleetForRouteProgram();
        const auto rows = window.selectedFleetRouteProgramRows();
        QString signature = QString::number(selectedFleet) + ':';
        for (const auto& row : rows) {
            signature += QString("%1\x1f%2\x1f%3\x1f%4\x1f%5\x1f%6\x1f%7\x1f%8\x1f%9\x1e")
                .arg(row.destination, row.arrivalAction).arg(row.warp).arg(row.active)
                .arg(row.eta, row.fuelUsed, row.fuelRemaining).arg(row.fuelShortage).arg(row.fuelTooltip);
        }
        if (signature == *lastRouteSignature) return;
        const auto previousRow = routeTree->property("fleetId").toUInt() == selectedFleet
            && !routeTree->selectedItems().isEmpty() ? routeTree->currentIndex().row() : -1;
        {
            const QSignalBlocker blocker(routeTree);
            routeTree->clear();
            routeTree->setProperty("fleetId", static_cast<quint32>(selectedFleet));
            for (std::size_t index = 0; index < rows.size(); ++index) {
                const auto& row = rows[index];
                auto* item = new QTreeWidgetItem(routeTree, {
                    row.active ? QString("%1 ▶").arg(static_cast<qulonglong>(index + 1))
                               : QString::number(static_cast<qulonglong>(index + 1)),
                    row.destination, QString("W%1").arg(row.warp), row.eta,
                    row.fuelUsed, row.fuelRemaining,
                });
                item->setData(0, Qt::UserRole, row.arrivalAction);
                item->setData(0, Qt::UserRole + 1, row.fuelShortage);
                item->setToolTip(0, row.active
                    ? "Current route leg; edits replace the fleet program when delivered" : "Select to edit this leg's Warp");
                item->setToolTip(1, row.destination + "\nOn arrival: " + row.arrivalAction);
                item->setToolTip(3, "Years from the current planning turn, including previous waypoints and command delivery. "
                    "Whole game turns assuming no further orders; — means no arrival predicted within 96 years.");
                item->setToolTip(4, row.fuelTooltip);
                item->setToolTip(5, row.fuelTooltip);
                if (row.warp > window.selectedFleetMaxWarpForRouteProgram()) item->setForeground(2, QColor("#ff8787"));
                if (row.fuelShortage) for (int column = 0; column < routeTree->columnCount(); ++column) {
                    item->setBackground(column, QBrush(QColor("#592b31")));
                    item->setForeground(column, QBrush(QColor("#ffb3b3")));
                    item->setToolTip(column, item->toolTip(column) + "\n" + row.fuelTooltip);
                }
            }
            if (previousRow >= 0 && !rows.empty()) routeTree->setCurrentItem(routeTree->topLevelItem(
                std::clamp(previousRow, 0, static_cast<int>(rows.size()) - 1)));
        }
        *lastRouteSignature = signature;
        syncRouteSelection();
        const auto selected = routeTree->selectedItems();
        selectedAction->setText(selected.isEmpty() ? "Select a waypoint to view its arrival action."
            : QString("On arrival: %1").arg(selected.front()->data(0, Qt::UserRole).toString()));
    };
    QObject::connect(&window, &MainWindow::routeProgramContextChanged, panel, refreshRouteTable);
    QObject::connect(warpSelector, &QProgressBar::valueChanged, panel,
        [=, &window](int warp) {
            if (!editor->editingLeg) return;
            if (const auto row = selectedRouteIndex())
                window.setSelectedFleetRouteProgramLegWarp(*row, static_cast<std::uint8_t>(warp));
        });
    refreshRouteTable();
    const auto appendCurrentTarget =
        [=, &window] {
            window.cancelRouteProgramMapTargetPick();
            const auto source = static_cast<FleetId>(sourceFleetCombo->currentData().toUInt());
            if (source == 0 || source != window.selectedFleetForRouteProgram()) return;
            const auto warp = static_cast<std::uint8_t>(warpSelector->value());
            const auto action = actionFromControls(actionCombo, cargoCombo, reserveSpin, fuelReserveSpin, colonyPlan);
            const bool appended = targetTypeCombo->currentData().toInt() != 0
                ? window.appendFleetTargetWaypoint(
                    static_cast<FleetId>(targetFleetCombo->currentData().toUInt()), warp, action)
                : window.appendSelectedStarWaypoint(warp, action);
            if (!appended) return;

            // Edits immediately after adding a destination belong to that
            // leg, including a quick right-click on the map. Do not leave
            // Colonize/Warp editing an unsent draft until the refresh timer.
            refreshRouteTable();
            auto* added = routeTree->topLevelItem(routeTree->topLevelItemCount() - 1);
            if (!added) return;
            routeTree->setCurrentItem(added);
            routeTree->scrollToItem(added);
        };
    QObject::connect(appendButton, &QPushButton::clicked, panel, appendCurrentTarget);
    QObject::connect(alternativesButton, &QPushButton::clicked, panel, [=, &window] {
        if (targetTypeCombo->currentData().toInt()!=0) {
            QMessageBox::information(&window,"Route alternatives","Choose a star destination to compare routes."); return;
        }
        window.appendSelectedStarWaypoint(static_cast<std::uint8_t>(warpSelector->value()),
            actionFromControls(actionCombo,cargoCombo,reserveSpin,fuelReserveSpin,colonyPlan),true);
        refreshRouteTable();
    });
    QObject::connect(workHereButton, &QPushButton::clicked, panel, [=, &window] {
        window.startSelectedFleetWorkHere(actionFromControls(actionCombo, cargoCombo, reserveSpin, fuelReserveSpin, colonyPlan));
    });
    QObject::connect(&window, &MainWindow::routeProgramQuickTargetRequested, panel,
        [appendCurrentTarget, newWaypointButton](int, std::uint32_t) {
            newWaypointButton->click();
            appendCurrentTarget();
        });
    QObject::connect(moveUpButton, &QPushButton::clicked, panel, [&window, routeTree, selectedRouteIndex] {
        if (const auto row = selectedRouteIndex(); row && window.moveSelectedFleetRouteProgramLeg(*row, -1)) {
            routeTree->setCurrentItem(routeTree->topLevelItem(static_cast<int>(*row) - 1));
        }
    });
    QObject::connect(moveDownButton, &QPushButton::clicked, panel, [&window, routeTree, selectedRouteIndex] {
        if (const auto row = selectedRouteIndex(); row && window.moveSelectedFleetRouteProgramLeg(*row, 1)) {
            routeTree->setCurrentItem(routeTree->topLevelItem(static_cast<int>(*row) + 1));
        }
    });
    QObject::connect(removeButton, &QPushButton::clicked, panel, [&window, selectedRouteIndex] {
        if (const auto row = selectedRouteIndex()) window.removeSelectedFleetRouteProgramLeg(*row);
    });

    QObject::connect(repeatCheck, &QCheckBox::clicked, panel, [&window, sourceFleetCombo](bool enabled) {
        const auto source = static_cast<FleetId>(sourceFleetCombo->currentData().toUInt());
        if (source == 0 || source != window.selectedFleetForRouteProgram()) return;
        window.setSelectedFleetRepeatOrdersForRouteProgram(enabled);
    });

    QObject::connect(clearButton, &QPushButton::clicked, panel, [&window, sourceFleetCombo] {
        const auto source = static_cast<FleetId>(sourceFleetCombo->currentData().toUInt());
        if (source == 0 || source != window.selectedFleetForRouteProgram()) return;
        window.clearSelectedFleetRouteProgram();
    });

    QObject::connect(sourceFleetCombo, &QComboBox::currentIndexChanged, panel,
        [&window, sourceFleetCombo](int) {
            window.cancelRouteProgramMapTargetPick();
            window.selectFleetForRouteProgram(
                static_cast<FleetId>(sourceFleetCombo->currentData().toUInt()));
        });

    // Refresh must survive moving this panel into the shared fleet dock.
    auto* timer = new QTimer(panel);
    timer->setObjectName("routeProgramRefreshTimer");
    timer->setInterval(180);
    QObject::connect(timer, &QTimer::timeout, panel,
        [&window, routeLabel, routeTree, moveUpButton, moveDownButton, removeButton,
            warpSelector, appendButton, clearButton, repeatCheck,
            targetTypeCombo, targetFleetCombo,
            destinationCombo, refreshFuelWarning, refreshRouteTable, editor, newWaypointButton,
            lastTargets = std::vector<FleetId>{}]() mutable {
            const auto selectedFleet = window.selectedFleetForRouteProgram();
            const auto maxWarp = window.selectedFleetMaxWarpForRouteProgram();

            warpSelector->setSafeWarp(maxWarp);
            warpSelector->setDamageRate(window.selectedFleetOverdriveDamageForRouteProgram(
                static_cast<std::uint8_t>(warpSelector->value())));
            warpSelector->setEnabled(selectedFleet != 0 && maxWarp > 0);
            const auto targets = window.availableFleetTargetsForRouteProgram();
            if (targets != lastTargets) {
                const auto previous = static_cast<FleetId>(targetFleetCombo->currentData().toUInt());
                const QSignalBlocker blocker(targetFleetCombo);
                targetFleetCombo->clear();
                for (const auto target : targets) {
                    targetFleetCombo->addItem(
                        window.fleetTargetNameForRouteProgram(target), static_cast<quint32>(target));
                }
                const auto previousIndex = targetFleetCombo->findData(static_cast<quint32>(previous));
                targetFleetCombo->setCurrentIndex(previousIndex);
                lastTargets = targets;
            }
            const bool wantsFleetTarget = targetTypeCombo->currentData().toInt() != 0;
            targetFleetCombo->setEnabled(wantsFleetTarget && !targets.empty());
            appendButton->setEnabled(selectedFleet != 0 && maxWarp > 0
                && !editor->editingLeg
                && destinationCombo->currentIndex() >= 0
                && (!wantsFleetTarget || targetFleetCombo->currentIndex() >= 0));
            clearButton->setEnabled(selectedFleet != 0);
            repeatCheck->setEnabled(selectedFleet != 0);
            {
                const QSignalBlocker blocker(repeatCheck);
                repeatCheck->setChecked(window.selectedFleetRepeatOrdersForRouteProgram());
            }
            refreshRouteTable();
            newWaypointButton->setEnabled(selectedFleet != 0);
            const auto selectedRow = routeTree->currentIndex().row();
            const auto rowCount = routeTree->topLevelItemCount();
            moveUpButton->setEnabled(selectedRow > 0);
            moveDownButton->setEnabled(selectedRow >= 0 && selectedRow + 1 < rowCount);
            removeButton->setEnabled(selectedRow >= 0);
            routeLabel->setText(window.selectedFleetRouteProgramSummary());
            refreshFuelWarning();
            drawRouteOverlay(window);
        });
    timer->start();

    routeLabel->setText(window.selectedFleetRouteProgramSummary());
    drawRouteOverlay(window);
}

} // namespace suns
