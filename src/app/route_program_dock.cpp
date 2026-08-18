#include "route_program_dock.hpp"

#include "main_window.hpp"

#include <QColor>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
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

FleetArrivalAction actionFromControls(QComboBox* actionCombo, QSpinBox* reserveSpin)
{
    FleetArrivalAction action;
    action.kind = static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt());
    action.reservePopulation = static_cast<std::uint64_t>(std::max(0, reserveSpin->value()));
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

    auto* help = new QLabel(
        "Select a fleet and a destination star on the map. Append waypoints to build a multi-leg program. "
        "Each leg keeps its own Warp and arrival action.",
        panel);
    help->setWordWrap(true);
    layout->addWidget(help);

    auto* routeLabel = new QLabel(panel);
    routeLabel->setWordWrap(true);
    routeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(routeLabel);

    auto* warpSpin = new QSpinBox(panel);
    warpSpin->setRange(1, kMaxWarp);
    auto* actionCombo = new QComboBox(panel);
    actionCombo->addItem("No action", static_cast<int>(FleetArrivalActionKind::None));
    actionCombo->addItem("Load All on arrival", static_cast<int>(FleetArrivalActionKind::LoadColonistsToCapacity));
    actionCombo->addItem("Unload All on arrival", static_cast<int>(FleetArrivalActionKind::UnloadAllColonists));
    actionCombo->addItem("Refuel on arrival", static_cast<int>(FleetArrivalActionKind::Refuel));
    auto* reserveSpin = new QSpinBox(panel);
    reserveSpin->setRange(0, 2'000'000'000);
    reserveSpin->setValue(1000);
    reserveSpin->setEnabled(false);

    auto* form = new QFormLayout;
    form->addRow("Waypoint Warp", warpSpin);
    form->addRow("Arrival action", actionCombo);
    form->addRow("Leave colonists", reserveSpin);
    layout->addLayout(form);

    auto* appendButton = new QPushButton("Append selected star as waypoint", panel);
    auto* clearButton = new QPushButton("Clear selected fleet route", panel);
    layout->addWidget(appendButton);
    layout->addWidget(clearButton);

    auto* note = new QLabel(
        "Arrival ends that fleet's movement for the current turn. The next waypoint becomes active immediately and starts moving next turn. "
        "Load All is dynamic: the amount is decided from the real colony population on arrival.",
        panel);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);

    dock->setWidget(panel);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);

    QObject::connect(actionCombo, &QComboBox::currentIndexChanged, panel, [=](int) {
        reserveSpin->setEnabled(
            static_cast<FleetArrivalActionKind>(actionCombo->currentData().toInt())
            == FleetArrivalActionKind::LoadColonistsToCapacity);
    });

    QObject::connect(appendButton, &QPushButton::clicked, panel, [&window, warpSpin, actionCombo, reserveSpin] {
        window.appendSelectedStarWaypoint(
            static_cast<std::uint8_t>(warpSpin->value()),
            actionFromControls(actionCombo, reserveSpin));
    });

    QObject::connect(clearButton, &QPushButton::clicked, panel, [&window] {
        window.clearSelectedFleetRouteProgram();
    });

    auto* timer = new QTimer(dock);
    timer->setInterval(180);
    QObject::connect(timer, &QTimer::timeout, dock,
        [&window, routeLabel, warpSpin, appendButton, clearButton, lastFleet = FleetId{}]() mutable {
            const auto selectedFleet = window.selectedFleetForRouteProgram();
            const auto maxWarp = window.selectedFleetMaxWarpForRouteProgram();

            if (selectedFleet != lastFleet) {
                lastFleet = selectedFleet;
                const auto suggested = window.selectedFleetSuggestedWarpForRouteProgram();
                warpSpin->setValue(std::max<int>(1, suggested));
            }

            warpSpin->setMaximum(std::max<int>(1, maxWarp));
            warpSpin->setEnabled(selectedFleet != 0 && maxWarp > 0);
            appendButton->setEnabled(selectedFleet != 0 && maxWarp > 0);
            clearButton->setEnabled(selectedFleet != 0);
            routeLabel->setText(window.selectedFleetRouteProgramSummary());
            drawRouteOverlay(window);
        });
    timer->start();

    routeLabel->setText(window.selectedFleetRouteProgramSummary());
    drawRouteOverlay(window);
}

} // namespace suns
