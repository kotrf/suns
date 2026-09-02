#include "route_program_dock.hpp"

#include "main_window.hpp"

#include <QColor>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QLabel>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstdint>

namespace suns {

namespace {

constexpr int kRouteOverlayDataKey = 73;

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
    QComboBox* actionCombo, QComboBox* cargoCombo, QSpinBox* reserveSpin)
{
    FleetArrivalAction action;
    action.kind = static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt());
    action.reservePopulation = static_cast<std::uint64_t>(std::max(0, reserveSpin->value()));
    action.cargo = static_cast<FleetCargoKind>(cargoCombo->currentData().toInt());
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

    const auto helpText = QString(
        "Select the fleet to program, select a destination star, choose Warp and an arrival action, then add the waypoint.\n\n"
        "Each leg keeps its own Warp and arrival action. Fleet targets are resolved every turn. Merge absorbs the pursuing fleet into the target fleet. "
        "Remote Mining is a persistent terminal task. Load and unload use the real surface stockpile on arrival. Repeat Orders repeats the whole route; terminal actions cannot be repeated. "
        "Colonization dismantles the entire fleet and recovers 33% of its ship minerals. Dockside loading and refuelling are in Fleet Logistics.");
    auto* headingRow = new QHBoxLayout;
    auto* heading = new QLabel("Program orders for one fleet", panel);
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
    auto* routeLabel = new QLabel(routeGroup);
    routeLabel->setWordWrap(true);
    routeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    routeLayout->addWidget(routeLabel);
    layout->addWidget(routeGroup);

    auto* waypointGroup = new QGroupBox("Add waypoint", panel);
    waypointGroup->setObjectName("routeWaypointGroup");
    auto* waypointLayout = new QVBoxLayout(waypointGroup);

    auto* warpSpin = new QSpinBox(waypointGroup);
    warpSpin->setRange(1, kMaxWarp);
    auto* targetTypeCombo = new QComboBox(waypointGroup);
    targetTypeCombo->addItem("Selected star", 0);
    targetTypeCombo->addItem("Friendly fleet", 1);
    auto* targetFleetCombo = new QComboBox(waypointGroup);
    targetFleetCombo->setEnabled(false);
    auto* actionCombo = new QComboBox(waypointGroup);
    actionCombo->addItem("No action", static_cast<int>(FleetArrivalActionKind::None));
    actionCombo->addItem("Load all available", static_cast<int>(FleetArrivalActionKind::LoadAllAvailable));
    actionCombo->addItem("Unload all", static_cast<int>(FleetArrivalActionKind::UnloadAll));
    actionCombo->addItem("Colonize world", static_cast<int>(FleetArrivalActionKind::Colonize));
    actionCombo->addItem("Remote Mining (persistent)", static_cast<int>(FleetArrivalActionKind::RemoteMining));
    actionCombo->addItem("Merge with fleet", static_cast<int>(FleetArrivalActionKind::MergeWithFleet));
    auto* cargoCombo = new QComboBox(waypointGroup);
    cargoCombo->addItem("Colonists", static_cast<int>(FleetCargoKind::Colonists));
    cargoCombo->addItem("Ironium", static_cast<int>(FleetCargoKind::Ironium));
    cargoCombo->addItem("Boranium", static_cast<int>(FleetCargoKind::Boranium));
    cargoCombo->addItem("Germanium", static_cast<int>(FleetCargoKind::Germanium));
    cargoCombo->setEnabled(false);
    auto* reserveSpin = new QSpinBox(waypointGroup);
    reserveSpin->setRange(0, 2'000'000'000);
    reserveSpin->setValue(1000);
    reserveSpin->setEnabled(false);

    auto* form = new QFormLayout;
    form->addRow("Target type", targetTypeCombo);
    form->addRow("Target fleet", targetFleetCombo);
    form->addRow("Waypoint Warp", warpSpin);
    form->addRow("On arrival", actionCombo);
    form->addRow("Cargo", cargoCombo);
    form->addRow("Leave on colony", reserveSpin);
    waypointLayout->addLayout(form);

    auto* appendButton = new QPushButton("Add selected star to route", waypointGroup);
    appendButton->setObjectName("routeAddButton");
    waypointLayout->addWidget(appendButton);
    layout->addWidget(waypointGroup);

    auto* repeatCheck = new QCheckBox("Repeat Orders", panel);
    repeatCheck->setToolTip("Restart the complete route after its final waypoint");
    layout->addWidget(repeatCheck);

    auto* clearButton = new QPushButton("Clear route / set No Task", panel);
    layout->addWidget(clearButton);

    layout->addStretch(1);

    dock->setWidget(panel);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);

    const auto updateCargoControls = [=] {
        const auto action = static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt());
        const auto cargo = static_cast<FleetCargoKind>(cargoCombo->currentData().toInt());
        const auto transfersCargo = action == FleetArrivalActionKind::LoadAllAvailable
            || action == FleetArrivalActionKind::UnloadAll;
        cargoCombo->setEnabled(transfersCargo);
        reserveSpin->setEnabled(
            action == FleetArrivalActionKind::LoadAllAvailable && cargo == FleetCargoKind::Colonists);
    };
    QObject::connect(actionCombo, &QComboBox::currentIndexChanged, panel, [=](int) { updateCargoControls(); });
    QObject::connect(cargoCombo, &QComboBox::currentIndexChanged, panel, [=](int) { updateCargoControls(); });
    QObject::connect(targetTypeCombo, &QComboBox::currentIndexChanged, panel, [=](int) {
        const bool fleetTarget = targetTypeCombo->currentData().toInt() == 1;
        targetFleetCombo->setEnabled(fleetTarget);
        appendButton->setText(fleetTarget ? "Add fleet target to route" : "Add selected star to route");
        const auto currentAction = static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt());
        if (fleetTarget) {
            actionCombo->setCurrentIndex(
                actionCombo->findData(static_cast<int>(FleetArrivalActionKind::MergeWithFleet)));
        } else if (currentAction == FleetArrivalActionKind::MergeWithFleet) {
            actionCombo->setCurrentIndex(
                actionCombo->findData(static_cast<int>(FleetArrivalActionKind::None)));
        }
    });

    QObject::connect(appendButton, &QPushButton::clicked, panel,
        [&window, sourceFleetCombo, warpSpin, actionCombo, cargoCombo, reserveSpin, targetTypeCombo, targetFleetCombo] {
            const auto source = static_cast<FleetId>(sourceFleetCombo->currentData().toUInt());
            if (!window.selectFleetForRouteProgram(source)) return;
            const auto warp = static_cast<std::uint8_t>(warpSpin->value());
            const auto action = actionFromControls(actionCombo, cargoCombo, reserveSpin);
            if (targetTypeCombo->currentData().toInt() == 1) {
                window.appendFleetTargetWaypoint(
                    static_cast<FleetId>(targetFleetCombo->currentData().toUInt()), warp, action);
            } else {
                window.appendSelectedStarWaypoint(warp, action);
            }
    });

    QObject::connect(repeatCheck, &QCheckBox::clicked, panel, [&window, sourceFleetCombo](bool enabled) {
        if (!window.selectFleetForRouteProgram(
                static_cast<FleetId>(sourceFleetCombo->currentData().toUInt()))) return;
        window.setSelectedFleetRepeatOrdersForRouteProgram(enabled);
    });

    QObject::connect(clearButton, &QPushButton::clicked, panel, [&window, sourceFleetCombo] {
        if (!window.selectFleetForRouteProgram(
                static_cast<FleetId>(sourceFleetCombo->currentData().toUInt()))) return;
        window.clearSelectedFleetRouteProgram();
    });

    QObject::connect(sourceFleetCombo, &QComboBox::currentIndexChanged, panel,
        [&window, sourceFleetCombo](int) {
            window.selectFleetForRouteProgram(
                static_cast<FleetId>(sourceFleetCombo->currentData().toUInt()));
        });

    auto* timer = new QTimer(dock);
    timer->setInterval(180);
    QObject::connect(timer, &QTimer::timeout, dock,
        [&window, routeLabel, warpSpin, appendButton, clearButton, repeatCheck,
            targetTypeCombo, targetFleetCombo, sourceFleetCombo,
            lastFleet = FleetId{}, lastSources = std::vector<FleetId>{}, lastTargets = std::vector<FleetId>{}]() mutable {
            const auto selectedFleet = window.selectedFleetForRouteProgram();
            const auto maxWarp = window.selectedFleetMaxWarpForRouteProgram();

            const auto sources = window.availableOwnedFleetsForRouteProgram();
            if (sources != lastSources) {
                const QSignalBlocker blocker(sourceFleetCombo);
                sourceFleetCombo->clear();
                for (const auto source : sources) {
                    sourceFleetCombo->addItem(
                        window.fleetTargetNameForRouteProgram(source), static_cast<quint32>(source));
                }
                lastSources = sources;
            }
            if (const auto selectedIndex = sourceFleetCombo->findData(static_cast<quint32>(selectedFleet));
                selectedIndex >= 0 && selectedIndex != sourceFleetCombo->currentIndex()) {
                const QSignalBlocker blocker(sourceFleetCombo);
                sourceFleetCombo->setCurrentIndex(selectedIndex);
            }

            if (selectedFleet != lastFleet) {
                lastFleet = selectedFleet;
                const auto suggested = window.selectedFleetSuggestedWarpForRouteProgram();
                warpSpin->setValue(std::max<int>(1, suggested));
            }

            warpSpin->setMaximum(std::max<int>(1, maxWarp));
            warpSpin->setEnabled(selectedFleet != 0 && maxWarp > 0);
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
                if (previousIndex >= 0) targetFleetCombo->setCurrentIndex(previousIndex);
                lastTargets = targets;
            }
            const bool wantsFleetTarget = targetTypeCombo->currentData().toInt() == 1;
            targetFleetCombo->setEnabled(wantsFleetTarget && !targets.empty());
            appendButton->setEnabled(selectedFleet != 0 && maxWarp > 0
                && (!wantsFleetTarget || !targets.empty()));
            clearButton->setEnabled(selectedFleet != 0);
            repeatCheck->setEnabled(selectedFleet != 0);
            {
                const QSignalBlocker blocker(repeatCheck);
                repeatCheck->setChecked(window.selectedFleetRepeatOrdersForRouteProgram());
            }
            routeLabel->setText(window.selectedFleetRouteProgramSummary());
            drawRouteOverlay(window);
        });
    timer->start();

    routeLabel->setText(window.selectedFleetRouteProgramSummary());
    drawRouteOverlay(window);
}

} // namespace suns
