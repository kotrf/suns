#include "main_window.hpp"

#include "suns/communications.hpp"
#include "ship_designer_dialog.hpp"
#include "star_item.hpp"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

namespace suns {

namespace {

constexpr int kMapItemStar = 1;
constexpr int kMapItemFleet = 2;

QString productionName(ProductionKind kind)
{
    switch (kind) {
    case ProductionKind::ColonyShip: return "Colony Ship";
    case ProductionKind::Factory: return "Factory";
    case ProductionKind::Mine: return "Mine";
    case ProductionKind::Research: return "Research";
    case ProductionKind::OrbitalStation: return "Orbital Dock";
    }
    return "Production";
}

QString fleetRoleName(FleetRole role)
{
    return role == FleetRole::Scout ? "Scout" : "Colony Ship";
}

QString productionSummary(const GameState& state, const Planet& planet)
{
    if (planet.productionQueue.empty()) return "Idle";

    const auto& item = planet.productionQueue.front();
    const auto total = production_item_cost(state, item);
    const auto completed = total >= item.remainingCost ? total - item.remainingCost : 0;

    QString name;
    if (item.kind == ProductionKind::Research) {
        name = "Ongoing Research";
    } else if (item.kind == ProductionKind::Factory) {
        name = "Factory";
    } else if (item.kind == ProductionKind::Mine) {
        name = "Mine";
    } else if (item.kind == ProductionKind::OrbitalStation) {
        name = "Orbital Dock";
    } else {
        const auto designId = item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId;
        const auto* design = find_ship_design(state, designId);
        name = design ? QString::fromStdString(design->name) : "Ship design";
    }

    QString summary = item.kind == ProductionKind::Research
        ? name
        : QString("%1: %2/%3").arg(name).arg(completed).arg(total);
    if (planet.productionQueue.size() > 1) {
        summary += QString(" (+%1 queued)")
                       .arg(static_cast<qulonglong>(planet.productionQueue.size() - 1));
    }
    return summary;
}

QColor starColor(StarClass stellarClass)
{
    switch (stellarClass) {
    case StarClass::BlueWhite: return QColor("#9bc5ff");
    case StarClass::White: return QColor("#e7eeff");
    case StarClass::YellowWhite: return QColor("#fff0b0");
    case StarClass::Yellow: return QColor("#ffd36b");
    case StarClass::Orange: return QColor("#ff9955");
    case StarClass::Red: return QColor("#ff6b62");
    }
    return QColor("#ffd36b");
}

QString starClassName(StarClass stellarClass)
{
    switch (stellarClass) {
    case StarClass::BlueWhite: return "blue-white";
    case StarClass::White: return "white";
    case StarClass::YellowWhite: return "yellow-white";
    case StarClass::Yellow: return "yellow";
    case StarClass::Orange: return "orange";
    case StarClass::Red: return "red";
    }
    return "yellow";
}

QString stellarVariabilitySummary(const GameState& state, const StarSystem& star)
{
    const auto intel = known_stellar_variability(state, 1, star.id);
    if (!intel || !intel->variable) return {};
    if (!intel->characterized) return "Variable star detected — cycle not characterized";
    return QString("Variable star — %1-turn period, ±%2% luminosity, now %3%")
        .arg(intel->periodTurns)
        .arg(intel->amplitudePercent)
        .arg(qRound(stellar_luminosity(star, state.turn) * 100.0));
}

QString turnCount(std::uint32_t turns)
{
    return QString("%1 turn%2").arg(turns).arg(turns == 1 ? "" : "s");
}

QString fuelValue(double fuel)
{
    return QString::number(fuel, 'f', 1);
}

const Fleet* findFleet(const GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    return it == state.fleets.end() ? nullptr : &*it;
}

const StarSystem* findStarAtPosition(const GameState& state, Position position)
{
    const auto it = std::find_if(state.stars.begin(), state.stars.end(), [&](const StarSystem& star) {
        return same_position(star.position, position);
    });
    return it == state.stars.end() ? nullptr : &*it;
}

bool hasPendingMove(const PlayerOrders& pending, FleetId fleetId)
{
    return std::any_of(pending.orders.begin(), pending.orders.end(), [fleetId](const Order& order) {
        if (const auto* move = std::get_if<MoveFleetOrder>(&order)) return move->fleet == fleetId;
        return false;
    });
}

bool hasPendingDesignName(const PlayerOrders& pending, const std::string& name)
{
    return std::any_of(pending.orders.begin(), pending.orders.end(), [&](const Order& order) {
        if (const auto* create = std::get_if<CreateShipDesignOrder>(&order)) return create->name == name;
        return false;
    });
}

QString arrivalActionSummary(const FleetArrivalAction& action)
{
    const auto cargo = [&] {
        switch (action.cargo) {
        case FleetCargoKind::Colonists: return QString("colonists");
        case FleetCargoKind::Ironium: return QString("Ironium");
        case FleetCargoKind::Boranium: return QString("Boranium");
        case FleetCargoKind::Germanium: return QString("Germanium");
        }
        return QString("cargo");
    };
    switch (action.kind) {
    case FleetArrivalActionKind::None:
        return "none";
    case FleetArrivalActionKind::LoadAllAvailable:
        return action.cargo == FleetCargoKind::Colonists
            ? QString("load all %1, leave %2")
                  .arg(cargo())
                  .arg(static_cast<qulonglong>(action.reservePopulation))
            : QString("load all available %1").arg(cargo());
    case FleetArrivalActionKind::UnloadAll:
        return QString("unload all %1").arg(cargo());
    case FleetArrivalActionKind::Refuel:
        return "refuel";
    case FleetArrivalActionKind::Colonize:
        return "colonize";
    case FleetArrivalActionKind::RemoteMining:
        return "begin remote mining";
    case FleetArrivalActionKind::MergeWithFleet:
        return "merge with fleet";
    }
    return "none";
}

GameState movementPhasePreviewState(
    const GameState& state,
    const PlayerOrders& pending,
    const TurnProcessor& processor)
{
    // Let the real TurnProcessor validate and resolve logistics on a disposable
    // copy. Clearing destinations prevents this preview turn from actually
    // moving fleets; keeping only logistics orders avoids unrelated actions.
    // The processor still applies turn-start onboard fuel generation, exactly
    // as it will before the real orders and movement phase.
    GameState preview = state;
    for (auto& fleet : preview.fleets) fleet.destination.reset();

    PlayerOrders logistics{pending.player, {}};
    for (const auto& order : pending.orders) {
        if (std::holds_alternative<SetFleetColonistsOrder>(order)
            || std::holds_alternative<SetFleetMineralCargoOrder>(order)
            || std::holds_alternative<TransferCargoOrder>(order)
            || std::holds_alternative<RefuelFleetOrder>(order)) {
            logistics.orders.push_back(order);
        }
    }

    return processor.process(preview, {logistics});
}

QColor fleetColor(FleetRole role, int alpha = 255)
{
    return role == FleetRole::Scout
        ? QColor(101, 166, 255, alpha)
        : QColor(195, 123, 234, alpha);
}

void addTravelLabel(QGraphicsScene* scene, Position from, Position to, const QString& text, const QColor& color)
{
    auto* label = scene->addText(text);
    label->setPos((from.x + to.x) / 2.0 + 5.0, (from.y + to.y) / 2.0 - 10.0);
    label->setDefaultTextColor(color);
    label->setScale(0.72);
    label->setZValue(-8.0);
}

void addSensorRange(
    QGraphicsScene* scene,
    Position center,
    double range,
    const QColor& outline,
    const QColor& fill)
{
    if (range <= 0.0) return;

    QPen pen(outline);
    pen.setWidthF(0.9);
    pen.setStyle(Qt::DashLine);
    auto* circle = scene->addEllipse(
        center.x - range,
        center.y - range,
        range * 2.0,
        range * 2.0,
        pen,
        QBrush(fill));
    circle->setZValue(-60.0);
}

void addBackgroundStars(QGraphicsScene* scene, std::uint64_t galaxySeed)
{
    const auto rect = scene->sceneRect();
    std::uint32_t seed = static_cast<std::uint32_t>(galaxySeed ^ (galaxySeed >> 32U) ^ 0x51A7C0DEu);
    const auto next = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };

    const int backgroundCount = static_cast<int>(std::clamp(rect.width() * rect.height() / 4200.0, 100.0, 260.0));
    for (int i = 0; i < backgroundCount; ++i) {
        const double xUnit = static_cast<double>(next() % 10000u) / 10000.0;
        const double yUnit = static_cast<double>(next() % 10000u) / 10000.0;
        const double size = 0.45 + static_cast<double>(next() % 110u) / 100.0;
        const int alpha = 28 + static_cast<int>(next() % 58u);

        QColor color(185, 205, 235, alpha);
        if ((i % 13) == 0) color = QColor(255, 220, 170, alpha);
        else if ((i % 9) == 0) color = QColor(170, 195, 255, alpha);

        auto* dot = scene->addEllipse(
            rect.left() + xUnit * rect.width(),
            rect.top() + yUnit * rect.height(),
            size,
            size,
            Qt::NoPen,
            QBrush(color));
        dot->setZValue(-100.0);
    }
}

QString componentSummary(const ShipDesign* design)
{
    if (!design || design->components.empty()) return "none";

    QStringList names;
    for (const auto component : design->components) {
        names.push_back(QString::fromStdString(component_spec(component).name));
    }
    return names.join(", ");
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , galaxyConfig_()
    , state_(generate_game(galaxyConfig_))
{
    resetTurnExchangeIdentity();
    setWindowTitle("Suns!");
    resize(1300, 840);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);

    scene_ = new QGraphicsScene(this);
    scene_->setSceneRect(
        -galaxyConfig_.width / 2.0 - 55.0,
        -galaxyConfig_.height / 2.0 - 55.0,
        galaxyConfig_.width + 110.0,
        galaxyConfig_.height + 110.0);
    scene_->setBackgroundBrush(QColor("#070b12"));

    view_ = new QGraphicsView(scene_, central);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setBackgroundBrush(QColor("#070b12"));
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    layout->addWidget(view_, 1);

    auto* sidePanel = new QWidget(central);
    sidePanel->setFixedWidth(425);
    auto* sideLayout = new QVBoxLayout(sidePanel);

    sideLayout->addWidget(new QLabel("<h2>Suns!</h2>", sidePanel));

    auto* help = new QLabel(
        "Select a fleet, choose a Warp, then select a star. Design ships from hulls and fitted components, "
        "load colonists at friendly colonies, manage fuel, or attach a dynamic Load All action to a course.",
        sidePanel);
    help->setWordWrap(true);
    sideLayout->addWidget(help);

    galaxyLabel_ = new QLabel(sidePanel);
    galaxyLabel_->setWordWrap(true);
    sideLayout->addWidget(galaxyLabel_);

    seedEdit_ = new QLineEdit(QString::number(state_.galaxySeed), sidePanel);
    seedEdit_->setPlaceholderText("Unsigned 64-bit seed");
    starCountSpin_ = new QSpinBox(sidePanel);
    starCountSpin_->setRange(8, 64);
    starCountSpin_->setValue(static_cast<int>(state_.stars.size()));
    newGalaxyButton_ = new QPushButton("Generate / Restart Galaxy", sidePanel);
    sensorRangesCheck_ = new QCheckBox("Show sensor ranges", sidePanel);
    sensorRangesCheck_->setChecked(showSensorRanges_);

    auto* galaxyForm = new QFormLayout;
    galaxyForm->addRow("Seed", seedEdit_);
    galaxyForm->addRow("Star systems", starCountSpin_);
    sideLayout->addLayout(galaxyForm);
    sideLayout->addWidget(newGalaxyButton_);
    sideLayout->addWidget(sensorRangesCheck_);

    empireLabel_ = new QLabel(sidePanel);
    empireLabel_->setWordWrap(true);
    sideLayout->addWidget(empireLabel_);

    selectionLabel_ = new QLabel(sidePanel);
    selectionLabel_->setWordWrap(true);
    sideLayout->addWidget(selectionLabel_);

    fleetLabel_ = new QLabel(sidePanel);
    fleetLabel_->setWordWrap(true);
    sideLayout->addWidget(fleetLabel_);

    warpSpin_ = new QSpinBox(sidePanel);
    warpSpin_->setRange(1, kMaxWarp);
    warpSpin_->setValue(kScoutCruiseWarp);
    colonistLoadSpin_ = new QSpinBox(sidePanel);
    colonistLoadSpin_->setRange(0, 0);
    arrivalReserveSpin_ = new QSpinBox(sidePanel);
    arrivalReserveSpin_->setRange(1, 1000000000);
    arrivalReserveSpin_->setValue(100);
    auto* fleetForm = new QFormLayout;
    fleetForm->addRow("Course Warp", warpSpin_);
    fleetForm->addRow("Target colonists", colonistLoadSpin_);
    fleetForm->addRow("Leave on Load All", arrivalReserveSpin_);
    sideLayout->addLayout(fleetForm);

    fleetMoveButton_ = new QPushButton("Plot selected fleet course here", sidePanel);
    fleetLoadAllButton_ = new QPushButton("Plot course + Load All on arrival", sidePanel);
    loadColonistsButton_ = new QPushButton("Set colonists aboard", sidePanel);
    sideLayout->addWidget(fleetMoveButton_);
    sideLayout->addWidget(fleetLoadAllButton_);
    sideLayout->addWidget(loadColonistsButton_);

    shipDesignCombo_ = new QComboBox(sidePanel);
    designShipButton_ = new QPushButton("Design Ship…", sidePanel);
    designShipButton_->setObjectName("openShipDesignerButton");
    buildShipButton_ = new QPushButton("Queue selected ship design", sidePanel);
    buildFactoryButton_ = new QPushButton(QString("Queue Factory (%1)").arg(kFactoryCost), sidePanel);
    buildOrbitalDockButton_ = new QPushButton(
        QString("Queue Orbital Dock (%1)").arg(kOrbitalDockCost), sidePanel);
    colonizeButton_ = new QPushButton("Colonize selected world with selected ship", sidePanel);

    auto* productionForm = new QFormLayout;
    productionForm->addRow("Ship design", shipDesignCombo_);
    sideLayout->addLayout(productionForm);
    sideLayout->addWidget(designShipButton_);
    sideLayout->addWidget(buildShipButton_);
    sideLayout->addWidget(buildFactoryButton_);
    sideLayout->addWidget(buildOrbitalDockButton_);
    sideLayout->addWidget(colonizeButton_);

    ordersLabel_ = new QLabel(sidePanel);
    ordersLabel_->setWordWrap(true);
    sideLayout->addWidget(ordersLabel_);

    endTurnButton_ = new QPushButton("End Turn", sidePanel);
    sideLayout->addWidget(endTurnButton_);
    sideLayout->addStretch(1);

    layout->addWidget(sidePanel);
    setCentralWidget(central);

    refreshShipDesignChoices();

    connect(scene_, &QGraphicsScene::selectionChanged, this, [this] {
        const auto selected = scene_->selectedItems();
        if (selected.isEmpty()) return;

        auto* item = selected.front();
        const auto kind = item->data(1).toInt();
        if (kind == kMapItemStar) selectedStarId_ = static_cast<StarId>(item->data(0).toUInt());
        else if (kind == kMapItemFleet) selectedFleetId_ = static_cast<FleetId>(item->data(0).toUInt());
        rebuildScene();
    });

    connect(sensorRangesCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        showSensorRanges_ = checked;
        rebuildScene();
    });
    connect(warpSpin_, &QSpinBox::valueChanged, this, [this](int) { updateControls(); });
    connect(colonistLoadSpin_, &QSpinBox::valueChanged, this, [this](int) {
        // Dockside cargo changes are planning-time actions: the disposable
        // movement preview makes them effective immediately in this year's UI
        // and the same order is resolved before movement when End Turn is used.
        queueColonists();
    });
    connect(arrivalReserveSpin_, &QSpinBox::valueChanged, this, [this](int) { updateControls(); });
    connect(shipDesignCombo_, &QComboBox::currentIndexChanged, this, [this](int) { updateControls(); });
    connect(newGalaxyButton_, &QPushButton::clicked, this, [this] { newGalaxy(); });
    connect(fleetMoveButton_, &QPushButton::clicked, this, [this] { queueFleetMove(); });
    connect(fleetLoadAllButton_, &QPushButton::clicked, this, [this] { queueFleetLoadAll(); });
    connect(designShipButton_, &QPushButton::clicked, this, [this] { openShipDesigner(); });
    connect(buildShipButton_, &QPushButton::clicked, this, [this] { queueShipDesign(); });
    connect(buildFactoryButton_, &QPushButton::clicked, this, [this] { queueProduction(ProductionKind::Factory); });
    connect(buildOrbitalDockButton_, &QPushButton::clicked, this, [this] {
        queueProduction(ProductionKind::OrbitalStation);
    });
    connect(loadColonistsButton_, &QPushButton::clicked, this, [this] { queueColonists(); });
    connect(colonizeButton_, &QPushButton::clicked, this, [this] { queueColonize(); });
    connect(endTurnButton_, &QPushButton::clicked, this, [this] { endTurn(); });

    rebuildScene();
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    statusBar()->showMessage("Turn 1 — Scout 1 selected; choose Warp and a destination");
}

const StarSystem* MainWindow::selectedStar() const
{
    if (!selectedStarId_) return nullptr;
    return find_star(state_, *selectedStarId_);
}

const Planet* MainWindow::selectedPlanet() const
{
    const auto* star = selectedStar();
    return star ? find_planet_at_star(state_, star->id) : nullptr;
}

const Fleet* MainWindow::selectedFleet() const
{
    if (!selectedFleetId_) return nullptr;
    const auto* fleet = findFleet(state_, *selectedFleetId_);
    return fleet && fleet->owner == 1 ? fleet : nullptr;
}

const Fleet* MainWindow::selectedColonyShipAtSelectedStar() const
{
    const auto* star = selectedStar();
    const auto* fleet = selectedFleet();
    if (!star || !fleet) return nullptr;
    return fleet_can_colonize(state_, *fleet) && same_position(fleet->position, star->position) ? fleet : nullptr;
}

std::optional<Fleet> MainWindow::selectedFleetPlanningView() const
{
    const auto* fleet = selectedFleet();
    if (!fleet) return std::nullopt;
    auto visible = fleet_player_view(state_, *fleet);
    if (!fleet_has_instant_link(state_, *fleet)) return visible;
    const auto preview = movementPhasePreviewState(state_, pendingOrders_, processor_);
    if (const auto* planned = findFleet(preview, fleet->id)) visible = *planned;
    return visible;
}

const Planet* MainWindow::selectedFriendlyColonyForFleet() const
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const auto* fleet = selectedFleet();
    if (!star || !planet || !fleet || planet->owner != fleet->owner) return nullptr;
    return same_position(fleet->position, star->position) ? planet : nullptr;
}

void MainWindow::refreshShipDesignChoices()
{
    if (!shipDesignCombo_) return;

    const auto previous = shipDesignCombo_->currentData().toUInt();
    const QSignalBlocker blocker(shipDesignCombo_);
    shipDesignCombo_->clear();

    int restoreIndex = -1;
    for (const auto& design : state_.shipDesigns) {
        if (design.owner != 1) continue;
        const auto index = shipDesignCombo_->count();
        const auto hull = hull_spec(design.hull);
        const auto text = QString("%1 — %2, cost %3, W%4, fuel %5, cargo %6")
                              .arg(QString::fromStdString(design.name))
                              .arg(QString::fromStdString(hull.name))
                              .arg(ship_design_cost(design))
                              .arg(ship_design_max_warp(design))
                              .arg(ship_design_fuel_capacity(design), 0, 'f', 0)
                              .arg(ship_design_cargo_capacity(design), 0, 'f', 0);
        shipDesignCombo_->addItem(text, static_cast<unsigned int>(design.id));
        if (design.id == previous) restoreIndex = index;
    }

    if (restoreIndex >= 0) shipDesignCombo_->setCurrentIndex(restoreIndex);
    else if (shipDesignCombo_->count() > 0) shipDesignCombo_->setCurrentIndex(0);
}

void MainWindow::rebuildScene()
{
    const auto selectionToRestore = selectedStarId_;

    if (!selectedFleet()) {
        const auto fallback = std::find_if(state_.fleets.begin(), state_.fleets.end(), [](const Fleet& fleet) {
            return fleet.owner == 1;
        });
        selectedFleetId_ = fallback == state_.fleets.end()
            ? std::optional<FleetId>{}
            : std::optional<FleetId>{fallback->id};
        warpControlFleetId_.reset();
        logisticsControlFleetId_.reset();
    }

    const QSignalBlocker blocker(scene_);
    scene_->clear();
    selectedStarId_ = selectionToRestore;
    addBackgroundStars(scene_, state_.galaxySeed);

    if (showSensorRanges_) {
        for (const auto& planet : state_.planets) {
            if (planet.owner != 1) continue;
            if (const auto* sourceStar = find_star(state_, planet.star)) {
                addSensorRange(scene_, sourceStar->position, kColonySensorRange,
                    QColor(100, 220, 155, 105), QColor(100, 220, 155, 12));
            }
        }
        for (const auto& fleet : state_.fleets) {
            if (fleet.owner != 1) continue;
            const auto visibleFleet = fleet_player_view(state_, fleet);
            const auto range = fleet_sensor_range(state_, visibleFleet);
            if (range > 0.0) {
                addSensorRange(scene_, visibleFleet.position, range,
                    QColor(90, 165, 255, 115), QColor(90, 165, 255, 10));
            }
        }
    }

    for (const auto& fleet : state_.fleets) {
        const auto visibleFleet = fleet_player_view(state_, fleet);
        if (!visibleFleet.destination || hasPendingMove(pendingOrders_, fleet.id)) continue;
        auto routeDestination = *visibleFleet.destination;
        if (visibleFleet.targetFleet != 0) {
            if (const auto* target = findFleet(state_, visibleFleet.targetFleet)) {
                routeDestination = fleet_player_view(state_, *target).position;
            }
        }
        const auto routeColor = fleetColor(fleet.role, 105);
        QPen routePen(routeColor);
        routePen.setWidthF(1.15);
        routePen.setStyle(Qt::DotLine);
        auto* route = scene_->addLine(visibleFleet.position.x, visibleFleet.position.y,
            routeDestination.x, routeDestination.y, routePen);
        route->setZValue(-20.0);
        QString routeText = visibleFleet.targetFleet != 0
            ? QString("W%1 • intercept ETA ~%2").arg(visibleFleet.warp).arg(turnCount(fleet_eta(visibleFleet)))
            : QString("W%1 • ETA %2").arg(visibleFleet.warp).arg(turnCount(fleet_eta(visibleFleet)));
        if (visibleFleet.arrivalAction) routeText += QString(" • %1").arg(arrivalActionSummary(*visibleFleet.arrivalAction));
        if (visibleFleet.targetFleet != 0) routeText += QString(" • pursuing Fleet %1").arg(visibleFleet.targetFleet);
        addTravelLabel(scene_, visibleFleet.position, routeDestination, routeText, fleetColor(fleet.role, 155));
    }

    for (const auto& order : pendingOrders_.orders) {
        std::visit([&](const auto& concreteOrder) {
            using T = std::decay_t<decltype(concreteOrder)>;
            if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                const auto* fleet = findFleet(state_, concreteOrder.fleet);
                if (!fleet) return;
                const auto visibleFleet = fleet_player_view(state_, *fleet);
                auto routeDestination = concreteOrder.destination;
                if (concreteOrder.targetFleet != 0) {
                    if (const auto* target = findFleet(state_, concreteOrder.targetFleet)) {
                        routeDestination = fleet_player_view(state_, *target).position;
                    }
                }
                const auto routeColor = fleetColor(fleet->role, 190);
                QPen routePen(routeColor);
                routePen.setWidthF(1.45);
                routePen.setStyle(Qt::DashLine);
                auto* route = scene_->addLine(visibleFleet.position.x, visibleFleet.position.y,
                    routeDestination.x, routeDestination.y, routePen);
                route->setZValue(-18.0);
                const auto routeWarp = concreteOrder.warp == 0 ? visibleFleet.warp : concreteOrder.warp;
                const auto eta = travel_turns(visibleFleet.position, routeDestination, warp_distance(routeWarp));
                QString routeText = concreteOrder.targetFleet != 0
                    ? QString("course W%1 • intercept ETA ~%2").arg(routeWarp).arg(turnCount(eta))
                    : QString("course W%1 • %2").arg(routeWarp).arg(turnCount(eta));
                if (concreteOrder.targetFleet != 0) routeText += QString(" • pursuing Fleet %1").arg(concreteOrder.targetFleet);
                if (concreteOrder.arrivalAction.kind != FleetArrivalActionKind::None) {
                    routeText += QString(" • %1").arg(arrivalActionSummary(concreteOrder.arrivalAction));
                }
                addTravelLabel(scene_, visibleFleet.position, routeDestination, routeText, fleetColor(fleet->role, 210));
            }
        }, order);
    }

    for (const auto& star : state_.stars) {
        const bool surveyed = is_surveyed(state_, 1, star.id);
        const auto* planet = find_planet_at_star(state_, star.id);
        const bool colony = planet && planet->owner == 1;

        auto* marker = new StarItem(star.id, starColor(star.stellarClass), surveyed, colony);
        marker->setData(1, kMapItemStar);
        marker->setPos(star.position.x, star.position.y);
        marker->setZValue(0.0);
        scene_->addItem(marker);
        if (selectionToRestore && *selectionToRestore == star.id) marker->setSelected(true);

        QString tooltip = QString("%1\n%2 star")
                              .arg(QString::fromStdString(star.name))
                              .arg(starClassName(star.stellarClass));
        QString mapLabel = QString::fromStdString(star.name);
        const auto variability = stellarVariabilitySummary(state_, star);
        if (!variability.isEmpty()) {
            tooltip += QString("\n%1").arg(variability);
            mapLabel += "  [VAR]";
        }
        if (!surveyed) {
            if (survey_level(state_, 1, star.id) >= SurveyLevel::SystemScan) {
                tooltip += "\nOrdinary scanner contact — planetary parameters unknown";
                mapLabel += "  [SCAN]";
            } else {
                tooltip += "\nUnsurveyed system — outside all sensor history";
                mapLabel += "  [?]";
            }
        } else if (planet) {
            const auto knownHabitability = known_planet_habitability(state_, 1, planet->id).value_or(0);
            const auto estimated = survey_level(state_, 1, star.id) == SurveyLevel::BasicScan;
            tooltip += QString("\n%1 — habitability %2%3% — %4 capacity %5")
                           .arg(QString::fromStdString(planet->name))
                           .arg(estimated ? "~" : "")
                           .arg(knownHabitability)
                           .arg(estimated ? "estimated" : "potential")
                           .arg(static_cast<qulonglong>(knownHabitability) * 25ULL);
            if (colony) {
                mapLabel += "  [COLONY]";
                tooltip += QString("\nOutput %1 / turn — %2\nColony sensor range %3")
                               .arg(colony_output(*planet))
                               .arg(productionSummary(state_, *planet))
                               .arg(kColonySensorRange, 0, 'f', 0);
            }
        }
        marker->setToolTip(tooltip);

        auto* label = scene_->addText(mapLabel);
        label->setPos(star.position.x + 12.0, star.position.y - 16.0);
        label->setDefaultTextColor(colony ? QColor("#8fdaa9") : surveyed ? QColor("#d1d9e6") : QColor("#727c8c"));
        label->setScale(state_.stars.size() > 36 ? 0.72 : state_.stars.size() > 24 ? 0.82 : 0.92);
        label->setZValue(5.0);
    }

    int fleetOffset = 0;
    for (const auto& fleet : state_.fleets) {
        const auto visibleFleet = fleet_player_view(state_, fleet);
        const double y = visibleFleet.position.y + 15.0 + fleetOffset * 13.0;
        const auto color = fleetColor(fleet.role);
        const bool selected = selectedFleetId_ && *selectedFleetId_ == fleet.id;

        QPolygonF shape;
        if (fleet.role == FleetRole::Scout) {
            shape << QPointF(visibleFleet.position.x + 6.0, y)
                  << QPointF(visibleFleet.position.x - 5.0, y - 4.0)
                  << QPointF(visibleFleet.position.x - 2.0, y)
                  << QPointF(visibleFleet.position.x - 5.0, y + 4.0);
        } else {
            shape << QPointF(visibleFleet.position.x, y - 5.0)
                  << QPointF(visibleFleet.position.x + 5.0, y)
                  << QPointF(visibleFleet.position.x, y + 5.0)
                  << QPointF(visibleFleet.position.x - 5.0, y);
        }

        if (selected) {
            QPen selectionPen(color.lighter(150));
            selectionPen.setWidthF(1.4);
            auto* ring = scene_->addEllipse(visibleFleet.position.x - 9.0, y - 9.0, 18.0, 18.0, selectionPen, Qt::NoBrush);
            ring->setZValue(9.0);
        }

        QPen fleetPen(selected ? color.lighter(160) : color.lighter(135));
        fleetPen.setWidthF(selected ? 2.0 : 1.0);
        auto* marker = scene_->addPolygon(shape, fleetPen, QBrush(color));
        marker->setFlag(QGraphicsItem::ItemIsSelectable);
        marker->setData(0, static_cast<unsigned int>(fleet.id));
        marker->setData(1, kMapItemFleet);
        marker->setCursor(QCursor(Qt::PointingHandCursor));

        QString tooltip = QString::fromStdString(fleet.name);
        tooltip += QString("\n%1 — Warp %2 (%3 ly/turn)")
                       .arg(fleetRoleName(fleet.role)).arg(visibleFleet.warp).arg(warp_distance(visibleFleet.warp), 0, 'f', 0);
        tooltip += QString("\nFuel %1 / %2 — gross mass %3")
                       .arg(fuelValue(visibleFleet.fuel)).arg(fuelValue(fleet_fuel_capacity(state_, visibleFleet)))
                       .arg(fleet_gross_mass(state_, visibleFleet), 0, 'f', 1);
        if (const auto range = fleet_sensor_range(state_, visibleFleet); range > 0.0) {
            tooltip += QString("\nSensor range %1").arg(range, 0, 'f', 0);
        }
        if (visibleFleet.destination) tooltip += QString("\nIn transit — %1 remaining").arg(turnCount(fleet_eta(visibleFleet)));
        if (visibleFleet.arrivalAction) tooltip += QString("\nArrival action: %1").arg(arrivalActionSummary(*visibleFleet.arrivalAction));
        marker->setToolTip(tooltip);
        marker->setZValue(10.0);

        QString fleetText = QString::fromStdString(fleet.name);
        if (selected) fleetText = QString("▶ %1").arg(fleetText);
        if (visibleFleet.destination) fleetText += QString("  [W%1 • %2]").arg(visibleFleet.warp).arg(turnCount(fleet_eta(visibleFleet)));
        auto* label = scene_->addText(fleetText);
        label->setPos(visibleFleet.position.x + 9.0, y - 8.0);
        label->setDefaultTextColor(selected ? color.lighter(165) : color.lighter(135));
        label->setScale(0.85);
        label->setZValue(11.0);
        ++fleetOffset;
    }

    updateControls();
}

void MainWindow::updateControls()
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    const bool surveyed = star && is_surveyed(state_, 1, star->id);
    const auto movementPreview = movementPhasePreviewState(state_, pendingOrders_, processor_);
    const auto* plannedFleet = authoritativeFleet ? findFleet(movementPreview, authoritativeFleet->id) : nullptr;
    const bool instantLink = authoritativeFleet && fleet_has_instant_link(state_, *authoritativeFleet);
    const auto* effectiveFleet = instantLink && plannedFleet ? plannedFleet : fleet;

    galaxyLabel_->setText(QString("<b>Galaxy seed:</b> %1 &nbsp; <b>Systems:</b> %2<br>"
                                  "Map units: light-years &nbsp; Movement: Warp² / turn")
        .arg(static_cast<qulonglong>(state_.galaxySeed))
        .arg(static_cast<qulonglong>(state_.stars.size())));

    std::size_t colonies = 0;
    std::uint64_t population = 0;
    std::uint32_t output = 0;
    for (const auto& candidate : state_.planets) {
        if (candidate.owner == 1) {
            ++colonies;
            population += candidate.population;
            output += colony_output(candidate);
        }
    }

    const auto colonyShips = static_cast<std::size_t>(std::count_if(state_.fleets.begin(), state_.fleets.end(), [&](const Fleet& candidate) {
        return candidate.owner == 1 && fleet_can_colonize(state_, candidate);
    }));
    const auto inTransit = static_cast<std::size_t>(std::count_if(state_.fleets.begin(), state_.fleets.end(), [](const Fleet& candidate) {
        return candidate.owner == 1 && candidate.destination.has_value();
    }));
    const auto* player = find_player(state_, 1);
    const auto surveyedCount = player ? player->surveyedStars.size() : 0;
    empireLabel_->setText(QString("<b>Terrans</b><br>Surveyed: %1 / %2 &nbsp; Colonies: %3<br>"
                                  "Population: %4 &nbsp; Output: %5 / turn<br>"
                                  "Designs: %6 &nbsp; Colonizers: %7 &nbsp; In transit: %8")
        .arg(static_cast<qulonglong>(surveyedCount))
        .arg(static_cast<qulonglong>(state_.stars.size()))
        .arg(static_cast<qulonglong>(colonies))
        .arg(static_cast<qulonglong>(population))
        .arg(output)
        .arg(static_cast<qulonglong>(state_.shipDesigns.size()))
        .arg(static_cast<qulonglong>(colonyShips))
        .arg(static_cast<qulonglong>(inTransit)));

    std::uint8_t selectedWarp = 1;
    if (fleet) {
        const auto maxWarp = fleet_max_warp(state_, *fleet);
        if (!warpControlFleetId_ || *warpControlFleetId_ != fleet->id) {
            const QSignalBlocker blocker(warpSpin_);
            warpSpin_->setRange(1, std::max<int>(1, maxWarp));
            warpSpin_->setValue(std::clamp<int>(fleet->warp, 1, std::max<int>(1, maxWarp)));
            warpControlFleetId_ = fleet->id;
        } else {
            const QSignalBlocker blocker(warpSpin_);
            warpSpin_->setMaximum(std::max<int>(1, maxWarp));
        }
        warpSpin_->setEnabled(maxWarp > 0);
        selectedWarp = static_cast<std::uint8_t>(warpSpin_->value());

        const auto colonistCapacity = std::max(
            0.0, fleet_cargo_capacity(state_, *fleet) - mineral_cargo_mass(effectiveFleet->minerals));
        const auto maxColonists = static_cast<int>(std::floor(
            colonistCapacity * kColonistsPerCargoUnit + 0.000001));
        if (!logisticsControlFleetId_ || *logisticsControlFleetId_ != fleet->id) {
            const QSignalBlocker blocker(colonistLoadSpin_);
            colonistLoadSpin_->setRange(0, std::max(0, maxColonists));
            const auto plannedColonists = effectiveFleet ? effectiveFleet->colonists : fleet->colonists;
            colonistLoadSpin_->setValue(static_cast<int>(std::min<std::uint64_t>(plannedColonists,
                static_cast<std::uint64_t>(std::max(0, maxColonists)))));
            logisticsControlFleetId_ = fleet->id;
        } else {
            const QSignalBlocker blocker(colonistLoadSpin_);
            colonistLoadSpin_->setMaximum(std::max(0, maxColonists));
        }
        colonistLoadSpin_->setEnabled(maxColonists > 0);
    } else {
        warpSpin_->setEnabled(false);
        colonistLoadSpin_->setEnabled(false);
        warpControlFleetId_.reset();
        logisticsControlFleetId_.reset();
    }

    const auto selectedEta = star && fleet ? travel_turns(fleet->position, star->position, warp_distance(selectedWarp)) : 0;

    QString routeFuelLine;
    if (star && fleet && effectiveFleet) {
        auto preview = *effectiveFleet;
        preview.warp = selectedWarp;
        const auto routeDistance = distance_between(fleet->position, star->position);
        const auto fuelChange = fleet_fuel_change_for_distance(state_, preview, routeDistance);
        if (fuelChange > 0.000001) {
            routeFuelLine = QString("<br>Direct-route fuel: <b>%1</b>; fuel at movement phase: %2.")
                                .arg(fuelValue(fuelChange)).arg(fuelValue(preview.fuel));
            if (fuelChange > preview.fuel + 0.000001) routeFuelLine += " <b>Insufficient at movement phase.</b>";
        } else if (fuelChange < -0.000001) {
            routeFuelLine = QString("<br>Ram-scoop gain on direct route: <b>+%1</b> fuel.").arg(fuelValue(-fuelChange));
        } else {
            routeFuelLine = "<br>Direct route is fuel-neutral at this Warp.";
        }
    }

    const bool dynamicLoadTarget = surveyed && planet && fleet
        && planet->owner == fleet->owner && fleet_cargo_capacity(state_, *fleet) > 0.0;
    const QString dynamicArrivalLine = dynamicLoadTarget
        ? QString("<br><b>Load All on arrival:</b> fill free cargo from the colony while leaving at least %1. "
                  "Exact load is dynamic and resolved when the ship actually arrives.")
              .arg(arrivalReserveSpin_->value())
        : QString{};

    if (star && !surveyed) {
        const bool systemContact = survey_level(state_, 1, star->id) >= SurveyLevel::SystemScan;
        QString travelLine;
        if (fleet) {
            travelLine = QString("<br>%1 at Warp %2: <b>%3</b> (%4 ly/turn).%5")
                             .arg(QString::fromStdString(fleet->name)).arg(selectedWarp)
                             .arg(turnCount(selectedEta)).arg(warp_distance(selectedWarp), 0, 'f', 0).arg(routeFuelLine);
            if (fleet_sensor_range(state_, *fleet) <= 0.0) travelLine += " This ship has no survey scanner.";
        }
        selectionLabel_->setText(QString("<hr><b>%1</b><br><b>%2</b><br>Planetary data unknown.%3<br>%4")
            .arg(QString::fromStdString(star->name))
            .arg(systemContact ? "SYSTEM CONTACT" : "UNSURVEYED")
            .arg(travelLine)
            .arg(systemContact
                    ? "Enter orbit or use a penetrating scanner to study the planet."
                    : "The system is detected as soon as it enters friendly sensor coverage."));
    } else if (star && planet) {
        const auto knownHabitability = known_planet_habitability(state_, 1, planet->id).value_or(0);
        const auto estimated = survey_level(state_, 1, star->id) == SurveyLevel::BasicScan;
        const auto variability = stellarVariabilitySummary(state_, *star);
        const auto variabilityLine = variability.isEmpty()
            ? QString{}
            : QString("<br><b>%1</b>").arg(variability);
        const auto habitabilityLine = QString("Habitability: <b>%1%2%</b>%3")
            .arg(estimated ? "~" : "")
            .arg(knownHabitability)
            .arg(variabilityLine);
        const QString owner = planet->owner == 1 ? "Terran colony" : "Uncolonized";
        const auto artifactLine = planet->precursorArtifacts.claimed
            ? QString("<br><span style='color:#d8bd72'><b>History:</b> precursor site excavated (+%1 RP)</span>")
                  .arg(planet->precursorArtifacts.researchPoints)
            : QString{};
        QString deepSurveyLine;
        if (const auto artifactHint = known_precursor_artifact_hint(state_, 1, planet->id)) {
            deepSurveyLine = *artifactHint
                ? "<br><span style='color:#d8bd72'><b>Deep survey:</b> possible artificial structures</span>"
                : "<br>Deep survey complete — no unexamined unusual sites";
        }
        QString populationLine;
        if (planet->owner == 1) {
            populationLine = QString("Population: %1 / %2 (+%3 next turn)<br>")
                                 .arg(static_cast<qulonglong>(planet->population))
                                 .arg(static_cast<qulonglong>(population_capacity(state_, *planet, state_.turn)))
                                 .arg(static_cast<qulonglong>(
                                     projected_population_growth(state_, *planet, state_.turn)));
        } else {
            populationLine = QString("%1 population capacity: %2<br>")
                                 .arg(estimated ? "Estimated" : "Potential")
                                 .arg(static_cast<qulonglong>(knownHabitability) * 25ULL);
        }
        QString travelLine;
        if (fleet) {
            travelLine = QString("<br><b>Route with %1:</b> Warp %2, %3.%4%5")
                             .arg(QString::fromStdString(fleet->name)).arg(selectedWarp)
                             .arg(turnCount(selectedEta)).arg(routeFuelLine).arg(dynamicArrivalLine);
        }
        selectionLabel_->setText(QString("<hr><b>%1</b><br>%2<br>%3<br>Status: %4<br>"
                                          "%5Infrastructure: %6<br>Economic output: %7 / turn<br>"
                                          "Production: <b>%8</b>%9%10%11")
            .arg(QString::fromStdString(star->name)).arg(QString::fromStdString(planet->name))
            .arg(habitabilityLine).arg(owner).arg(populationLine).arg(planet->industry)
            .arg(colony_output(*planet)).arg(productionSummary(state_, *planet)).arg(travelLine)
            .arg(artifactLine).arg(deepSurveyLine));
    } else {
        selectionLabel_->setText("<hr>Select a star system.");
    }

    const auto* logisticsColony = instantLink ? selectedFriendlyColonyForFleet() : nullptr;
    if (fleet) {
        const auto* design = fleet_design(state_, *fleet);
        QString status = "Stationary";
        if (fleet->task == FleetTask::RemoteMining) status = "Remote Mining assigned";
        else if (std::any_of(fleet->pendingCommands.begin(), fleet->pendingCommands.end(), [](const PendingFleetCommand& command) {
                     return command.task == FleetTask::RemoteMining;
                 })) {
            status = "Remote Mining command in flight";
        }
        if (fleet->destination) {
            const auto* destinationStar = findStarAtPosition(state_, *fleet->destination);
            status = QString("In transit to %1 — Warp %2, %3 remaining")
                         .arg(destinationStar ? QString::fromStdString(destinationStar->name) : "course target")
                         .arg(fleet->warp).arg(turnCount(fleet_eta(*fleet)));
            if (fleet->arrivalAction) status += QString("; on arrival: %1").arg(arrivalActionSummary(*fleet->arrivalAction));
        }

        const auto sensor = fleet_sensor_range(state_, *fleet);
        const auto penetratingSensor = fleet_penetrating_sensor_range(state_, *fleet);
        const auto fuelCapacity = fleet_fuel_capacity(state_, *fleet);
        const auto cargoCapacity = fleet_cargo_capacity(state_, *fleet);
        const auto cargoUsed = colonist_cargo_mass(fleet->colonists);
        const auto maxWarp = fleet_max_warp(state_, *fleet);
        const auto hullName = design ? QString::fromStdString(hull_spec(design->hull).name) : "unknown";
        QString radiationLine;
        if (fleet_radiation_hazard(state_, *fleet) > 0.0) {
            radiationLine = "<br><b>Radiating drive fitted</b> — colonist effect pending race-tolerance rules.";
        }
        const bool refuelingAvailable = logisticsColony
            && colony_has_orbital_service(
                state_, logisticsColony->id, fleet->owner, OrbitalStationModule::RefuelingDepot);
        const QString dockedLine = logisticsColony
            ? QString("<br>Docked at <b>%1</b>: loading available; %2.")
                  .arg(QString::fromStdString(logisticsColony->name))
                  .arg(refuelingAvailable
                          ? "automatic refueling active"
                          : "no automatic refueling service")
            : "<br>Logistics: select the friendly colony under this fleet to load cargo.";

        QString movementPlanLine;
        if (effectiveFleet && (std::abs(effectiveFleet->fuel - fleet->fuel) > 0.000001
                               || effectiveFleet->colonists != fleet->colonists)) {
            movementPlanLine = QString("<br><b>Movement-phase plan:</b> fuel %1, colonists %2, gross mass %3 kt.")
                                   .arg(fuelValue(effectiveFleet->fuel))
                                   .arg(static_cast<qulonglong>(effectiveFleet->colonists))
                                   .arg(fleet_gross_mass(state_, *effectiveFleet), 0, 'f', 1);
        }

        fleetLabel_->setText(QString("<hr><b>Selected fleet:</b> %1<br>Design: %2 &nbsp; Hull: %3<br>"
                                     "Warp now: %4 (max %5) &nbsp; Planned: <b>W%6</b> = %7 ly/turn<br>"
                                     "Fuel: %8 / %9 &nbsp; Gross mass: %10 kt<br>"
                                     "Colonists: %11 &nbsp; Cargo: %12 / %13<br>"
                                     "Survey sensor: %14<br>Components: %15<br>Status: %16%17%18%19")
            .arg(QString::fromStdString(fleet->name))
            .arg(design ? QString::fromStdString(design->name) : "unknown")
            .arg(hullName).arg(fleet->warp).arg(maxWarp).arg(selectedWarp)
            .arg(warp_distance(selectedWarp), 0, 'f', 0).arg(fuelValue(fleet->fuel)).arg(fuelValue(fuelCapacity))
            .arg(fleet_gross_mass(state_, *fleet), 0, 'f', 1).arg(static_cast<qulonglong>(fleet->colonists))
            .arg(cargoUsed, 0, 'f', 1).arg(cargoCapacity, 0, 'f', 1)
            .arg(sensor <= 0.0
                    ? "none"
                    : penetratingSensor > 0.0
                        ? QString("%1 ly detection / %2 ly penetrating")
                              .arg(sensor, 0, 'f', 0).arg(penetratingSensor, 0, 'f', 0)
                        : QString("%1 ly detection").arg(sensor, 0, 'f', 0))
            .arg(componentSummary(design)).arg(status).arg(dockedLine).arg(movementPlanLine).arg(radiationLine));
    } else {
        fleetLabel_->setText("<hr><b>Selected fleet:</b> none<br>Click a ship marker on the map.");
    }

    const bool validCourse = star && fleet && fleet_warp_valid(state_, *fleet, selectedWarp);
    fleetMoveButton_->setEnabled(validCourse);
    fleetMoveButton_->setText(validCourse
        ? QString("Plot %1 course at Warp %2 (%3)").arg(QString::fromStdString(fleet->name)).arg(selectedWarp).arg(turnCount(selectedEta))
        : "Plot selected fleet course here");

    arrivalReserveSpin_->setEnabled(dynamicLoadTarget);
    fleetLoadAllButton_->setEnabled(validCourse && dynamicLoadTarget);
    fleetLoadAllButton_->setText(validCourse && dynamicLoadTarget
        ? QString("Plot course + Load All (leave %1)").arg(arrivalReserveSpin_->value())
        : "Plot course + Load All on arrival");

    const bool ownedColony = surveyed && planet != nullptr && planet->owner == 1;
    shipDesignCombo_->setEnabled(ownedColony && shipDesignCombo_->count() > 0);
    buildFactoryButton_->setEnabled(ownedColony);
    designShipButton_->setEnabled(true);

    const bool stationExists = ownedColony && find_orbital_station_at_planet(state_, planet->id);
    const bool stationQueued = ownedColony
        && std::any_of(planet->productionQueue.begin(), planet->productionQueue.end(), [](const ProductionItem& item) {
               return item.kind == ProductionKind::OrbitalStation;
           });
    const bool stationPending = ownedColony
        && std::any_of(pendingOrders_.orders.begin(), pendingOrders_.orders.end(), [planet](const Order& order) {
               const auto* queued = std::get_if<QueueProductionOrder>(&order);
               return queued && queued->colony == planet->id
                   && queued->kind == ProductionKind::OrbitalStation;
           });
    buildOrbitalDockButton_->setEnabled(ownedColony && !stationExists && !stationQueued && !stationPending);
    buildOrbitalDockButton_->setText(stationExists
        ? "Orbital Dock already operational"
        : stationQueued || stationPending
            ? "Orbital Dock already planned"
            : QString("Queue Orbital Dock (%1)").arg(kOrbitalDockCost));

    const auto designId = static_cast<ShipDesignId>(shipDesignCombo_->currentData().toUInt());
    const auto* buildDesign = find_ship_design(state_, designId);
    buildShipButton_->setEnabled(ownedColony && buildDesign != nullptr && buildDesign->owner == 1);
    buildShipButton_->setToolTip(stationExists
        ? "Add this ship to the selected colony's production queue"
        : "The ship may be queued now, but production waits until an Orbital Dock is completed first");
    buildShipButton_->setText(buildDesign
        ? QString("Queue %1 (%2)").arg(QString::fromStdString(buildDesign->name)).arg(ship_design_cost(*buildDesign))
        : "Queue selected ship design");

    const bool canLoad = logisticsColony && fleet && effectiveFleet && fleet_cargo_capacity(state_, *fleet) > 0.0;
    loadColonistsButton_->setEnabled(false);
    loadColonistsButton_->setText(canLoad ? "Cargo updates immediately" : "Dock at a friendly colony to load");

    const auto* colonizer = selectedColonyShipAtSelectedStar();
    colonizeButton_->setEnabled(star != nullptr
        && survey_level(state_, 1, star->id) >= SurveyLevel::OrbitalSurvey
        && planet != nullptr && planet->owner == 0
        && colonizer != nullptr && colonizer->colonists > 0);

    if (pendingOrders_.orders.empty()) ordersLabel_->setText("<b>Orders this turn:</b> none");
    else {
        auto safeDescriptions = pendingDescriptions_;
        for (auto& description : safeDescriptions) description = description.toHtmlEscaped();
        ordersLabel_->setText(QString("<b>Orders this turn (%1):</b><br>%2")
            .arg(static_cast<qulonglong>(pendingOrders_.orders.size())).arg(safeDescriptions.join("<br>")));
    }

    endTurnButton_->setText(QString("End Turn %1").arg(static_cast<qulonglong>(state_.turn)));
    refreshResearchPanel();
}

void MainWindow::appendPendingOrder(Order order, const QString& description)
{
    pendingOrders_.orders.push_back(std::move(order));
    pendingDescriptions_.push_back(description);
    rebuildScene();
    statusBar()->showMessage(description);
}

void MainWindow::replacePendingFleetMove(
    FleetId fleet,
    Position destination,
    std::uint8_t warp,
    FleetArrivalAction arrivalAction,
    const QString& description)
{
    for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
        if (const auto* move = std::get_if<MoveFleetOrder>(&pendingOrders_.orders[index]); move && move->fleet == fleet) {
            pendingOrders_.orders[index] = MoveFleetOrder{fleet, destination, warp, arrivalAction};
            pendingDescriptions_[static_cast<int>(index)] = description;
            rebuildScene();
            statusBar()->showMessage(description);
            return;
        }
    }
    appendPendingOrder(MoveFleetOrder{fleet, destination, warp, arrivalAction}, description);
}

void MainWindow::openShipDesigner()
{
    if (shipDesigner_) {
        shipDesigner_->raise();
        shipDesigner_->activateWindow();
        return;
    }

    auto* dialog = new ShipDesignerDialog(state_, 1, this);
    shipDesigner_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        const auto draft = dialog->draft();
        const auto duplicateInState = std::any_of(
            state_.shipDesigns.begin(), state_.shipDesigns.end(), [&](const ShipDesign& design) {
                return design.owner == 1 && design.name == draft.name;
            });
        if (duplicateInState || hasPendingDesignName(pendingOrders_, draft.name)) {
            statusBar()->showMessage("A ship design with that name already exists or is pending");
            return;
        }
        appendPendingOrder(
            CreateShipDesignOrder{draft.name, draft.hull, draft.components, draft.placements},
            QString("Save ship design %1 — available after End Turn")
                .arg(QString::fromStdString(draft.name)));
    });
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    connect(dialog, &QObject::destroyed, this, [this] { shipDesigner_.clear(); });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::queueFleetMove()
{
    const auto* star = selectedStar();
    const auto* fleet = selectedFleet();
    if (!star || !fleet) return;

    const auto warp = static_cast<std::uint8_t>(warpSpin_->value());
    if (!fleet_warp_valid(state_, *fleet, warp)) return;

    const auto eta = travel_turns(fleet->position, star->position, warp_distance(warp));
    const auto movementPreview = movementPhasePreviewState(state_, pendingOrders_, processor_);
    const auto* plannedFleet = findFleet(movementPreview, fleet->id);
    auto preview = plannedFleet ? *plannedFleet : *fleet;
    preview.warp = warp;
    const auto fuelChange = fleet_fuel_change_for_distance(state_, preview, distance_between(fleet->position, star->position));

    QString fuelText;
    if (fuelChange > 0.000001) {
        fuelText = QString(", fuel %1, available %2").arg(fuelValue(fuelChange)).arg(fuelValue(preview.fuel));
        if (fuelChange > preview.fuel + 0.000001) fuelText += " [INSUFFICIENT]";
    } else if (fuelChange < -0.000001) {
        fuelText = QString(", scoop +%1 fuel").arg(fuelValue(-fuelChange));
    } else {
        fuelText = ", fuel-neutral";
    }

    replacePendingFleetMove(fleet->id, star->position, warp, {},
        QString("Plot %1 course to %2 — W%3, %4%5")
            .arg(QString::fromStdString(fleet->name)).arg(QString::fromStdString(star->name))
            .arg(warp).arg(turnCount(eta)).arg(fuelText));
}

void MainWindow::queueFleetLoadAll()
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const auto* fleet = selectedFleet();
    if (!star || !planet || !fleet || !is_surveyed(state_, 1, star->id)
        || planet->owner != fleet->owner || fleet_cargo_capacity(state_, *fleet) <= 0.0) {
        return;
    }

    const auto warp = static_cast<std::uint8_t>(warpSpin_->value());
    if (!fleet_warp_valid(state_, *fleet, warp)) return;

    const auto eta = travel_turns(fleet->position, star->position, warp_distance(warp));
    const FleetArrivalAction action{
        FleetArrivalActionKind::LoadAllAvailable,
        static_cast<std::uint64_t>(arrivalReserveSpin_->value()),
    };

    replacePendingFleetMove(
        fleet->id,
        star->position,
        warp,
        action,
        QString("Plot %1 course to %2 — W%3, %4; on arrival load to capacity, leave %5 [DYNAMIC]")
            .arg(QString::fromStdString(fleet->name))
            .arg(QString::fromStdString(star->name))
            .arg(warp)
            .arg(turnCount(eta))
            .arg(arrivalReserveSpin_->value()));
}

void MainWindow::queueShipDesign()
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    if (!star || !is_surveyed(state_, 1, star->id) || !planet || planet->owner != 1) return;

    const auto designId = static_cast<ShipDesignId>(shipDesignCombo_->currentData().toUInt());
    const auto* design = find_ship_design(state_, designId);
    if (!design || design->owner != 1) return;

    appendPendingOrder(QueueShipDesignOrder{planet->id, design->id},
        QString("Queue %1 at %2 — cost %3")
            .arg(QString::fromStdString(design->name)).arg(QString::fromStdString(planet->name)).arg(ship_design_cost(*design)));
}

void MainWindow::queueProduction(ProductionKind kind)
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    if (!star || !is_surveyed(state_, 1, star->id) || !planet || planet->owner != 1) return;

    appendPendingOrder(QueueProductionOrder{planet->id, kind},
        QString("Queue %1 at %2").arg(productionName(kind)).arg(QString::fromStdString(planet->name)));
}

void MainWindow::queueColonists()
{
    const auto* planet = selectedFriendlyColonyForFleet();
    const auto* fleet = selectedFleet();
    if (!planet || !fleet) return;

    const auto target = static_cast<std::uint64_t>(colonistLoadSpin_->value());
    const auto description = QString("Current-year cargo: %1 colonists aboard %2 at %3")
                                 .arg(static_cast<qulonglong>(target))
                                 .arg(QString::fromStdString(fleet->name))
                                 .arg(QString::fromStdString(planet->name));

    // A fleet can have only one final cargo target for the turn. Replacing the
    // previous order avoids artificial load-then-unload sequences and makes the
    // preview represent the player's latest intent.
    for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
        if (const auto* load = std::get_if<SetFleetColonistsOrder>(&pendingOrders_.orders[index]);
            load && load->fleet == fleet->id) {
            pendingOrders_.orders[index] = SetFleetColonistsOrder{planet->id, fleet->id, target};
            pendingDescriptions_[static_cast<int>(index)] = description;
            rebuildScene();
            statusBar()->showMessage(description);
            return;
        }
    }

    appendPendingOrder(SetFleetColonistsOrder{planet->id, fleet->id, target}, description);
}

void MainWindow::queueColonize()
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const auto* ship = selectedColonyShipAtSelectedStar();
    if (!star || survey_level(state_, 1, star->id) < SurveyLevel::OrbitalSurvey
        || !planet || planet->owner != 0 || !ship || ship->colonists == 0) return;
    if (!confirmFleetColonization(*ship, *planet, false)) return;

    appendPendingOrder(ColonizePlanetOrder{ship->id, planet->id},
        QString("Colonize %1 with %2 (%3 colonists; entire fleet dismantled)")
            .arg(QString::fromStdString(planet->name)).arg(QString::fromStdString(ship->name))
            .arg(static_cast<qulonglong>(ship->colonists)));
}

bool MainWindow::confirmFleetColonization(
    const Fleet& fleet, const Planet& planet, bool scheduledRoute)
{
    QStringList composition;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state_, stack.design);
        composition << QString("%1× %2")
            .arg(stack.count)
            .arg(design ? QString::fromStdString(design->name) : QString("Design %1").arg(stack.design));
    }

    const auto salvage = fleet_colonization_salvage(state_, fleet);
    const MineralCargo delivered{
        fleet.minerals.ironium + salvage.ironium,
        fleet.minerals.boranium + salvage.boranium,
        fleet.minerals.germanium + salvage.germanium,
    };

    QMessageBox warning(this);
    warning.setIcon(QMessageBox::Warning);
    warning.setWindowTitle("Confirm fleet colonization");
    warning.setText(QString("Colonize %1 with the entire fleet %2?")
        .arg(QString::fromStdString(planet.name))
        .arg(QString::fromStdString(fleet.name)));
    warning.setInformativeText(
        QString("All %1 ship(s) will be dismantled and Fleet %2 will cease to exist.\n\n"
                "Ships: %3\n"
                "Colonists landed: %4\n"
                "Cargo deposited: I %5 / B %6 / G %7\n"
                "33% ship salvage: I %8 / B %9 / G %10\n"
                "Total added to colony: I %11 / B %12 / G %13\n\n"
                "Fuel and production points are not recovered.%14")
            .arg(fleet_ship_count(fleet))
            .arg(fleet.id)
            .arg(composition.join(", "))
            .arg(static_cast<qulonglong>(fleet.colonists))
            .arg(fleet.minerals.ironium, 0, 'f', 0)
            .arg(fleet.minerals.boranium, 0, 'f', 0)
            .arg(fleet.minerals.germanium, 0, 'f', 0)
            .arg(salvage.ironium, 0, 'f', 0)
            .arg(salvage.boranium, 0, 'f', 0)
            .arg(salvage.germanium, 0, 'f', 0)
            .arg(delivered.ironium, 0, 'f', 0)
            .arg(delivered.boranium, 0, 'f', 0)
            .arg(delivered.germanium, 0, 'f', 0)
            .arg(scheduledRoute
                ? "\n\nThis is a route preview; cargo and colonists may change before arrival."
                : ""));
    warning.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    warning.setDefaultButton(QMessageBox::Cancel);
    return warning.exec() == QMessageBox::Yes;
}

void MainWindow::endTurn()
{
    auto result = processor_.process_with_events(state_, {pendingOrders_});
    state_ = std::move(result.state);
    rotateTurnExchangeToken();
    pendingOrders_.orders.clear();
    pendingDescriptions_.clear();
    selectedStarId_.reset();
    logisticsControlFleetId_.reset();
    refreshShipDesignChoices();
    rebuildScene();
    appendTurnMessages(result.events);
    statusBar()->showMessage(QString("Turn %1 — orders, designs, logistics, Warp travel, arrival actions, sensors and economy resolved")
        .arg(static_cast<qulonglong>(state_.turn)));
}

void MainWindow::newGalaxy()
{
    bool ok = false;
    const auto parsedSeed = seedEdit_->text().trimmed().toULongLong(&ok);
    if (!ok) {
        statusBar()->showMessage("Invalid seed — enter an unsigned integer");
        return;
    }

    GalaxyConfig requested = galaxyConfig_;
    requested.seed = static_cast<std::uint64_t>(parsedSeed);
    requested.starCount = static_cast<std::size_t>(starCountSpin_->value());

    try {
        auto generated = generate_game(requested);
        galaxyConfig_ = requested;
        state_ = std::move(generated);
        resetTurnExchangeIdentity();
    } catch (const std::exception& error) {
        statusBar()->showMessage(QString("Galaxy generation failed: %1").arg(error.what()));
        return;
    }

    pendingOrders_ = PlayerOrders{1, {}};
    resetTurnMessages();
    pendingDescriptions_.clear();
    selectedStarId_.reset();
    selectedFleetId_ = 1;
    warpControlFleetId_.reset();
    logisticsControlFleetId_.reset();
    refreshShipDesignChoices();

    scene_->setSceneRect(-galaxyConfig_.width / 2.0 - 55.0, -galaxyConfig_.height / 2.0 - 55.0,
        galaxyConfig_.width + 110.0, galaxyConfig_.height + 110.0);
    rebuildScene();
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);

    statusBar()->showMessage(QString("New galaxy: seed %1, %2 systems — Scout 1 selected at Warp %3")
        .arg(static_cast<qulonglong>(state_.galaxySeed)).arg(static_cast<qulonglong>(state_.stars.size())).arg(kScoutCruiseWarp));
}

} // namespace suns
