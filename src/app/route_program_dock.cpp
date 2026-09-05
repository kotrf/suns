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
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace suns {

namespace {

constexpr int kRouteOverlayDataKey = 73;

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
        const bool unsafe = safeWarp_ > 0 && value() > safeWarp_;
        if (property("unsafe").toBool() != unsafe) {
            setProperty("unsafe", unsafe);
            style()->unpolish(this);
            style()->polish(this);
        }
        setFormat(unsafe
            ? QString("Warp %1 • +%2% damage/turn").arg(value()).arg(damageRate_, 0, 'f', 0)
            : QString("Warp %1 • safe to W%2").arg(value()).arg(safeWarp_));
        setToolTip(unsafe
            ? QString("Unsafe overdrive: rated W%1, selected W%2. Hull damage accumulates while moving.")
                  .arg(safeWarp_).arg(value())
            : QString("Selected W%1; fleet safe limit W%2.").arg(value()).arg(safeWarp_));
    }

    int safeWarp_{1};
    double damageRate_{};
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
    auto* routeTree = new QTreeWidget(routeGroup);
    routeTree->setObjectName("routeProgramQueue");
    routeTree->setColumnCount(4);
    routeTree->setHeaderLabels({"#", "Destination", "Warp", "On arrival"});
    routeTree->setRootIsDecorated(false);
    routeTree->setAlternatingRowColors(true);
    routeTree->setSelectionMode(QAbstractItemView::SingleSelection);
    routeTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    routeTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    routeTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    routeTree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    routeTree->header()->resizeSection(3, 130);
    routeTree->setMinimumHeight(118);
    routeLayout->addWidget(routeTree);
    auto* routeButtons = new QHBoxLayout;
    auto* moveUpButton = new QPushButton("Move up", routeGroup);
    auto* moveDownButton = new QPushButton("Move down", routeGroup);
    auto* removeButton = new QPushButton("Remove", routeGroup);
    routeButtons->addWidget(moveUpButton);
    routeButtons->addWidget(moveDownButton);
    routeButtons->addWidget(removeButton);
    moveUpButton->setEnabled(false);
    moveDownButton->setEnabled(false);
    removeButton->setEnabled(false);
    routeLayout->addLayout(routeButtons);
    auto* routeLabel = new QLabel(routeGroup);
    routeLabel->setWordWrap(true);
    routeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
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
    form->addRow("Waypoint Warp", warpRow);
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
    QObject::connect(warpHelpButton, &QToolButton::clicked, panel, [&window] {
        QMessageBox::information(
            &window,
            "Unsafe Warp overdrive",
            "Every fleet has a safe Warp limit set by the least tolerant engine in its ships. "
            "You may still order any speed through Warp 10. Above the safe limit the selector turns red: "
            "fuel use rises sharply and hull damage accumulates on every turn spent moving. "
            "The exact damage rate is determined by the fitted engine; at 100% damage the fleet is immobilized.");
    });

    QObject::connect(appendButton, &QPushButton::clicked, panel,
        [&window, sourceFleetCombo, warpSelector, actionCombo, cargoCombo, reserveSpin, targetTypeCombo, targetFleetCombo] {
            const auto source = static_cast<FleetId>(sourceFleetCombo->currentData().toUInt());
            if (!window.selectFleetForRouteProgram(source)) return;
            const auto warp = static_cast<std::uint8_t>(warpSelector->value());
            const auto action = actionFromControls(actionCombo, cargoCombo, reserveSpin);
            if (targetTypeCombo->currentData().toInt() == 1) {
                window.appendFleetTargetWaypoint(
                    static_cast<FleetId>(targetFleetCombo->currentData().toUInt()), warp, action);
            } else {
                window.appendSelectedStarWaypoint(warp, action);
            }
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
        [&window, routeLabel, routeTree, moveUpButton, moveDownButton, removeButton,
            warpSelector, appendButton, clearButton, repeatCheck,
            targetTypeCombo, targetFleetCombo, sourceFleetCombo,
            lastFleet = FleetId{}, lastSources = std::vector<FleetId>{},
            lastTargets = std::vector<FleetId>{}, lastRouteSignature = QString{}]() mutable {
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
                warpSelector->setValue(std::max<int>(1, suggested));
            }

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
            const auto rows = window.selectedFleetRouteProgramRows();
            QString signature = QString::number(selectedFleet) + ':';
            for (const auto& row : rows) {
                signature += QString("%1\x1f%2\x1f%3\x1f%4\x1e")
                    .arg(row.destination, row.arrivalAction)
                    .arg(row.warp)
                    .arg(row.active);
            }
            if (signature != lastRouteSignature) {
                const auto previousRow = routeTree->currentIndex().row();
                routeTree->clear();
                routeTree->setProperty("fleetId", static_cast<quint32>(selectedFleet));
                for (std::size_t index = 0; index < rows.size(); ++index) {
                    const auto& row = rows[index];
                    auto* item = new QTreeWidgetItem(routeTree, {
                        row.active ? QString("%1 ▶").arg(static_cast<qulonglong>(index + 1))
                                   : QString::number(static_cast<qulonglong>(index + 1)),
                        row.destination,
                        QString("W%1").arg(row.warp),
                        row.arrivalAction,
                    });
                    if (row.active) {
                        item->setToolTip(0, "Current route leg; edits replace the fleet program");
                    }
                    item->setToolTip(1, row.destination);
                    item->setToolTip(3, row.arrivalAction);
                    if (row.warp > maxWarp) item->setForeground(2, QColor("#ff8787"));
                }
                if (!rows.empty()) {
                    routeTree->setCurrentItem(routeTree->topLevelItem(
                        std::clamp(previousRow, 0, static_cast<int>(rows.size()) - 1)));
                }
                lastRouteSignature = signature;
            }
            const auto selectedRow = routeTree->currentIndex().row();
            const auto rowCount = routeTree->topLevelItemCount();
            moveUpButton->setEnabled(selectedRow > 0);
            moveDownButton->setEnabled(selectedRow >= 0 && selectedRow + 1 < rowCount);
            removeButton->setEnabled(selectedRow >= 0);
            routeLabel->setText(window.selectedFleetRouteProgramSummary());
            drawRouteOverlay(window);
        });
    timer->start();

    routeLabel->setText(window.selectedFleetRouteProgramSummary());
    drawRouteOverlay(window);
}

} // namespace suns
