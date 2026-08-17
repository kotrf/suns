#include "main_window.hpp"
#include "star_item.hpp"

#include <QBrush>
#include <QColor>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

namespace suns {

namespace {

QString productionName(ProductionKind kind)
{
    return kind == ProductionKind::ColonyShip ? "Colony Ship" : "Factory";
}

QString productionSummary(const Planet& planet)
{
    if (planet.productionQueue.empty()) {
        return "Idle";
    }

    const auto& item = planet.productionQueue.front();
    const auto total = production_cost(item.kind);
    const auto completed = total - item.remainingCost;
    QString summary = QString("%1: %2/%3")
                          .arg(productionName(item.kind))
                          .arg(completed)
                          .arg(total);
    if (planet.productionQueue.size() > 1) {
        summary += QString(" (+%1 queued)")
                       .arg(static_cast<qulonglong>(planet.productionQueue.size() - 1));
    }
    return summary;
}

QColor starColor(StarClass stellarClass)
{
    switch (stellarClass) {
    case StarClass::BlueWhite:
        return QColor("#9bc5ff");
    case StarClass::White:
        return QColor("#e7eeff");
    case StarClass::YellowWhite:
        return QColor("#fff0b0");
    case StarClass::Yellow:
        return QColor("#ffd36b");
    case StarClass::Orange:
        return QColor("#ff9955");
    case StarClass::Red:
        return QColor("#ff6b62");
    }
    return QColor("#ffd36b");
}

QString starClassName(StarClass stellarClass)
{
    switch (stellarClass) {
    case StarClass::BlueWhite:
        return "blue-white";
    case StarClass::White:
        return "white";
    case StarClass::YellowWhite:
        return "yellow-white";
    case StarClass::Yellow:
        return "yellow";
    case StarClass::Orange:
        return "orange";
    case StarClass::Red:
        return "red";
    }
    return "yellow";
}

QString turnCount(std::uint32_t turns)
{
    return QString("%1 turn%2").arg(turns).arg(turns == 1 ? "" : "s");
}

const Fleet* findFleet(const GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    return it == state.fleets.end() ? nullptr : &*it;
}

bool hasPendingMove(const PlayerOrders& pending, FleetId fleetId)
{
    return std::any_of(pending.orders.begin(), pending.orders.end(), [fleetId](const Order& order) {
        if (const auto* move = std::get_if<MoveFleetOrder>(&order)) {
            return move->fleet == fleetId;
        }
        return false;
    });
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
        if ((i % 13) == 0) {
            color = QColor(255, 220, 170, alpha);
        } else if ((i % 9) == 0) {
            color = QColor(170, 195, 255, alpha);
        }

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

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , galaxyConfig_()
    , state_(generate_game(galaxyConfig_))
{
    setWindowTitle("Suns!");
    resize(1280, 820);

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
    sidePanel->setFixedWidth(380);
    auto* sideLayout = new QVBoxLayout(sidePanel);

    sideLayout->addWidget(new QLabel("<h2>Suns!</h2>", sidePanel));

    auto* help = new QLabel(
        "Explore a seeded galaxy where distance matters. Fleets keep travelling between turns; "
        "scouts are faster than colony ships, and systems are surveyed only on arrival.",
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

    auto* galaxyForm = new QFormLayout;
    galaxyForm->addRow("Seed", seedEdit_);
    galaxyForm->addRow("Star systems", starCountSpin_);
    sideLayout->addLayout(galaxyForm);
    sideLayout->addWidget(newGalaxyButton_);

    empireLabel_ = new QLabel(sidePanel);
    empireLabel_->setWordWrap(true);
    sideLayout->addWidget(empireLabel_);

    selectionLabel_ = new QLabel(sidePanel);
    selectionLabel_->setWordWrap(true);
    sideLayout->addWidget(selectionLabel_);

    scoutMoveButton_ = new QPushButton("Send Scout 1 here", sidePanel);
    colonyMoveButton_ = new QPushButton("Send Colony Ship here", sidePanel);
    buildColonyButton_ = new QPushButton(
        QString("Queue Colony Ship (%1)").arg(kColonyShipCost), sidePanel);
    buildFactoryButton_ = new QPushButton(
        QString("Queue Factory (%1)").arg(kFactoryCost), sidePanel);
    colonizeButton_ = new QPushButton("Colonize selected world", sidePanel);

    sideLayout->addWidget(scoutMoveButton_);
    sideLayout->addWidget(colonyMoveButton_);
    sideLayout->addWidget(buildColonyButton_);
    sideLayout->addWidget(buildFactoryButton_);
    sideLayout->addWidget(colonizeButton_);

    ordersLabel_ = new QLabel(sidePanel);
    ordersLabel_->setWordWrap(true);
    sideLayout->addWidget(ordersLabel_);

    endTurnButton_ = new QPushButton("End Turn", sidePanel);
    sideLayout->addWidget(endTurnButton_);
    sideLayout->addStretch(1);

    layout->addWidget(sidePanel);
    setCentralWidget(central);

    connect(scene_, &QGraphicsScene::selectionChanged, this, [this] {
        selectedStarId_.reset();
        const auto selected = scene_->selectedItems();
        if (!selected.isEmpty()) {
            selectedStarId_ = static_cast<StarId>(selected.front()->data(0).toUInt());
        }
        updateControls();
    });

    connect(newGalaxyButton_, &QPushButton::clicked, this, [this] { newGalaxy(); });
    connect(scoutMoveButton_, &QPushButton::clicked, this, [this] { queueScoutMove(); });
    connect(colonyMoveButton_, &QPushButton::clicked, this, [this] { queueColonyShipMove(); });
    connect(buildColonyButton_, &QPushButton::clicked, this, [this] {
        queueProduction(ProductionKind::ColonyShip);
    });
    connect(buildFactoryButton_, &QPushButton::clicked, this, [this] {
        queueProduction(ProductionKind::Factory);
    });
    connect(colonizeButton_, &QPushButton::clicked, this, [this] { queueColonize(); });
    connect(endTurnButton_, &QPushButton::clicked, this, [this] { endTurn(); });

    rebuildScene();
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    statusBar()->showMessage("Turn 1 — choose a system and plot the scout's first course");
}

const StarSystem* MainWindow::selectedStar() const
{
    if (!selectedStarId_) {
        return nullptr;
    }
    return find_star(state_, *selectedStarId_);
}

const Planet* MainWindow::selectedPlanet() const
{
    const auto* star = selectedStar();
    return star ? find_planet_at_star(state_, star->id) : nullptr;
}

const Fleet* MainWindow::playerScout() const
{
    const auto it = std::find_if(state_.fleets.begin(), state_.fleets.end(), [](const Fleet& fleet) {
        return fleet.owner == 1 && fleet.role == FleetRole::Scout;
    });
    return it == state_.fleets.end() ? nullptr : &*it;
}

const Fleet* MainWindow::playerColonyShip() const
{
    const auto it = std::find_if(state_.fleets.begin(), state_.fleets.end(), [](const Fleet& fleet) {
        return fleet.owner == 1 && fleet.role == FleetRole::ColonyShip;
    });
    return it == state_.fleets.end() ? nullptr : &*it;
}

const Fleet* MainWindow::colonyShipAtSelectedStar() const
{
    const auto* star = selectedStar();
    if (!star) {
        return nullptr;
    }

    const auto it = std::find_if(state_.fleets.begin(), state_.fleets.end(), [&](const Fleet& fleet) {
        return fleet.owner == 1
            && fleet.role == FleetRole::ColonyShip
            && same_position(fleet.position, star->position);
    });
    return it == state_.fleets.end() ? nullptr : &*it;
}

void MainWindow::rebuildScene()
{
    const auto selectionToRestore = selectedStarId_;
    const QSignalBlocker blocker(scene_);
    scene_->clear();
    selectedStarId_ = selectionToRestore;

    addBackgroundStars(scene_, state_.galaxySeed);

    // Active courses survive End Turn and show the remaining route/ETA. A new
    // pending move replaces the visualized active course for that fleet.
    for (const auto& fleet : state_.fleets) {
        if (!fleet.destination || hasPendingMove(pendingOrders_, fleet.id)) {
            continue;
        }

        const auto routeColor = fleetColor(fleet.role, 105);
        QPen routePen(routeColor);
        routePen.setWidthF(1.15);
        routePen.setStyle(Qt::DotLine);
        auto* route = scene_->addLine(
            fleet.position.x,
            fleet.position.y,
            fleet.destination->x,
            fleet.destination->y,
            routePen);
        route->setZValue(-20.0);
        addTravelLabel(
            scene_, fleet.position, *fleet.destination,
            QString("ETA %1").arg(turnCount(fleet_eta(fleet))),
            fleetColor(fleet.role, 155));
    }

    for (const auto& order : pendingOrders_.orders) {
        std::visit(
            [&](const auto& concreteOrder) {
                using T = std::decay_t<decltype(concreteOrder)>;
                if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                    const auto* fleet = findFleet(state_, concreteOrder.fleet);
                    if (!fleet) {
                        return;
                    }

                    const auto routeColor = fleetColor(fleet->role, 190);
                    QPen routePen(routeColor);
                    routePen.setWidthF(1.45);
                    routePen.setStyle(Qt::DashLine);
                    auto* route = scene_->addLine(
                        fleet->position.x,
                        fleet->position.y,
                        concreteOrder.destination.x,
                        concreteOrder.destination.y,
                        routePen);
                    route->setZValue(-18.0);
                    const auto eta = travel_turns(
                        fleet->position, concreteOrder.destination, fleet_speed(fleet->role));
                    addTravelLabel(
                        scene_, fleet->position, concreteOrder.destination,
                        QString("course: %1").arg(turnCount(eta)),
                        fleetColor(fleet->role, 210));
                }
            },
            order);
    }

    for (const auto& star : state_.stars) {
        const bool surveyed = is_surveyed(state_, 1, star.id);
        const auto* planet = find_planet_at_star(state_, star.id);
        const bool colony = planet && planet->owner == 1;

        auto* marker = new StarItem(star.id, starColor(star.stellarClass), surveyed, colony);
        marker->setPos(star.position.x, star.position.y);
        marker->setZValue(0.0);
        scene_->addItem(marker);

        if (selectionToRestore && *selectionToRestore == star.id) {
            marker->setSelected(true);
        }

        QString tooltip = QString("%1\n%2 star")
                              .arg(QString::fromStdString(star.name))
                              .arg(starClassName(star.stellarClass));
        QString mapLabel = QString::fromStdString(star.name);

        if (!surveyed) {
            tooltip += "\nUnsurveyed system — planetary data unknown";
            mapLabel += "  [?]";
        } else if (planet) {
            tooltip += QString("\n%1 — habitability %2% — capacity %3")
                           .arg(QString::fromStdString(planet->name))
                           .arg(planet->habitability)
                           .arg(static_cast<qulonglong>(population_capacity(*planet)));
            if (colony) {
                mapLabel += "  [COLONY]";
                tooltip += QString("\nOutput %1 / turn — %2")
                               .arg(colony_output(*planet))
                               .arg(productionSummary(*planet));
            }
        }
        marker->setToolTip(tooltip);

        auto* label = scene_->addText(mapLabel);
        label->setPos(star.position.x + 12.0, star.position.y - 16.0);
        label->setDefaultTextColor(
            colony ? QColor("#8fdaa9") : surveyed ? QColor("#d1d9e6") : QColor("#727c8c"));
        label->setScale(state_.stars.size() > 36 ? 0.72 : state_.stars.size() > 24 ? 0.82 : 0.92);
        label->setZValue(5.0);
    }

    int fleetOffset = 0;
    for (const auto& fleet : state_.fleets) {
        const double y = fleet.position.y + 15.0 + fleetOffset * 13.0;
        const auto color = fleetColor(fleet.role);

        QPolygonF shape;
        if (fleet.role == FleetRole::Scout) {
            shape << QPointF(fleet.position.x + 6.0, y)
                  << QPointF(fleet.position.x - 5.0, y - 4.0)
                  << QPointF(fleet.position.x - 2.0, y)
                  << QPointF(fleet.position.x - 5.0, y + 4.0);
        } else {
            shape << QPointF(fleet.position.x, y - 5.0)
                  << QPointF(fleet.position.x + 5.0, y)
                  << QPointF(fleet.position.x, y + 5.0)
                  << QPointF(fleet.position.x - 5.0, y);
        }

        QPen fleetPen(color.lighter(135));
        fleetPen.setWidthF(1.0);
        auto* marker = scene_->addPolygon(shape, fleetPen, QBrush(color));
        QString tooltip = QString::fromStdString(fleet.name);
        if (fleet.destination) {
            tooltip += QString("\nIn transit — %1 remaining").arg(turnCount(fleet_eta(fleet)));
        }
        marker->setToolTip(tooltip);
        marker->setZValue(10.0);

        QString fleetLabel = QString::fromStdString(fleet.name);
        if (fleet.destination) {
            fleetLabel += QString("  [%1]").arg(turnCount(fleet_eta(fleet)));
        }
        auto* label = scene_->addText(fleetLabel);
        label->setPos(fleet.position.x + 9.0, y - 8.0);
        label->setDefaultTextColor(color.lighter(135));
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
    const bool surveyed = star && is_surveyed(state_, 1, star->id);

    galaxyLabel_->setText(
        QString("<b>Galaxy seed:</b> %1 &nbsp; <b>Systems:</b> %2")
            .arg(static_cast<qulonglong>(state_.galaxySeed))
            .arg(static_cast<qulonglong>(state_.stars.size())));

    std::size_t colonies = 0;
    std::uint64_t population = 0;
    std::uint32_t stockpile = 0;
    std::uint32_t output = 0;
    for (const auto& candidate : state_.planets) {
        if (candidate.owner == 1) {
            ++colonies;
            population += candidate.population;
            stockpile += candidate.stockpile;
            output += colony_output(candidate);
        }
    }

    const auto colonyShips = static_cast<std::size_t>(std::count_if(
        state_.fleets.begin(), state_.fleets.end(), [](const Fleet& fleet) {
            return fleet.owner == 1 && fleet.role == FleetRole::ColonyShip;
        }));
    const auto inTransit = static_cast<std::size_t>(std::count_if(
        state_.fleets.begin(), state_.fleets.end(), [](const Fleet& fleet) {
            return fleet.owner == 1 && fleet.destination.has_value();
        }));

    const auto* player = find_player(state_, 1);
    const auto surveyedCount = player ? player->surveyedStars.size() : 0;

    empireLabel_->setText(
        QString("<b>Terrans</b><br>Surveyed: %1 / %2 &nbsp; Colonies: %3<br>"
                "Population: %4 &nbsp; Output: %5 / turn<br>"
                "Stored: %6 &nbsp; Colony ships: %7 &nbsp; In transit: %8")
            .arg(static_cast<qulonglong>(surveyedCount))
            .arg(static_cast<qulonglong>(state_.stars.size()))
            .arg(static_cast<qulonglong>(colonies))
            .arg(static_cast<qulonglong>(population))
            .arg(output)
            .arg(stockpile)
            .arg(static_cast<qulonglong>(colonyShips))
            .arg(static_cast<qulonglong>(inTransit)));

    const auto* scout = playerScout();
    const auto* colonyShip = playerColonyShip();
    const auto scoutEta = star && scout
        ? travel_turns(scout->position, star->position, fleet_speed(scout->role))
        : 0;
    const auto colonyEta = star && colonyShip
        ? travel_turns(colonyShip->position, star->position, fleet_speed(colonyShip->role))
        : 0;

    if (star && !surveyed) {
        QString travelLine;
        if (scout) {
            travelLine = QString("<br>Scout travel time from current position: <b>%1</b>.")
                             .arg(turnCount(scoutEta));
        }
        selectionLabel_->setText(
            QString("<hr><b>%1</b><br><b>UNSURVEYED</b><br>Planetary data unknown.%2<br>"
                    "Survey intel arrives only when Scout 1 reaches the star.")
                .arg(QString::fromStdString(star->name))
                .arg(travelLine));
    } else if (star && planet) {
        const QString owner = planet->owner == 1 ? "Terran colony" : "Uncolonized";
        QString populationLine;
        if (planet->owner == 1) {
            populationLine = QString("Population: %1 / %2 (+%3 next turn)<br>")
                                 .arg(static_cast<qulonglong>(planet->population))
                                 .arg(static_cast<qulonglong>(population_capacity(*planet)))
                                 .arg(static_cast<qulonglong>(projected_population_growth(*planet)));
        } else {
            populationLine = QString("Potential population capacity: %1<br>")
                                 .arg(static_cast<qulonglong>(population_capacity(*planet)));
        }

        selectionLabel_->setText(
            QString("<hr><b>%1</b><br>%2<br>Habitability: <b>%3%</b><br>Status: %4<br>"
                    "%5Infrastructure: %6<br>Economic output: %7 / turn<br>"
                    "Stored production: %8<br>Production: <b>%9</b>")
                .arg(QString::fromStdString(star->name))
                .arg(QString::fromStdString(planet->name))
                .arg(planet->habitability)
                .arg(owner)
                .arg(populationLine)
                .arg(planet->industry)
                .arg(colony_output(*planet))
                .arg(planet->stockpile)
                .arg(productionSummary(*planet)));
    } else {
        selectionLabel_->setText("<hr>Select a star system.");
    }

    scoutMoveButton_->setEnabled(star != nullptr && scout != nullptr);
    colonyMoveButton_->setEnabled(star != nullptr && colonyShip != nullptr);
    scoutMoveButton_->setText(
        star && scout
            ? QString("Plot Scout 1 course here (%1)").arg(turnCount(scoutEta))
            : "Plot Scout 1 course here");
    colonyMoveButton_->setText(
        star && colonyShip
            ? QString("Plot Colony Ship course here (%1)").arg(turnCount(colonyEta))
            : "Plot Colony Ship course here");

    const bool ownedColony = surveyed && planet != nullptr && planet->owner == 1;
    buildColonyButton_->setEnabled(ownedColony);
    buildFactoryButton_->setEnabled(ownedColony);

    colonizeButton_->setEnabled(
        surveyed && planet != nullptr && planet->owner == 0 && colonyShipAtSelectedStar() != nullptr);

    if (pendingOrders_.orders.empty()) {
        ordersLabel_->setText("<b>Orders this turn:</b> none");
    } else {
        ordersLabel_->setText(
            QString("<b>Orders this turn (%1):</b><br>%2")
                .arg(static_cast<qulonglong>(pendingOrders_.orders.size()))
                .arg(pendingDescriptions_.join("<br>")));
    }

    endTurnButton_->setText(QString("End Turn %1").arg(static_cast<qulonglong>(state_.turn)));
}

void MainWindow::appendPendingOrder(Order order, const QString& description)
{
    pendingOrders_.orders.push_back(std::move(order));
    pendingDescriptions_.push_back(description);
    rebuildScene();
    statusBar()->showMessage(description);
}

void MainWindow::queueScoutMove()
{
    const auto* star = selectedStar();
    const auto* scout = playerScout();
    if (!star || !scout) {
        return;
    }

    const auto eta = travel_turns(scout->position, star->position, fleet_speed(scout->role));
    appendPendingOrder(
        MoveFleetOrder{scout->id, star->position},
        QString("Plot Scout 1 course to %1 — %2")
            .arg(QString::fromStdString(star->name))
            .arg(turnCount(eta)));
}

void MainWindow::queueColonyShipMove()
{
    const auto* star = selectedStar();
    const auto* ship = playerColonyShip();
    if (!star || !ship) {
        return;
    }

    const auto eta = travel_turns(ship->position, star->position, fleet_speed(ship->role));
    appendPendingOrder(
        MoveFleetOrder{ship->id, star->position},
        QString("Plot %1 course to %2 — %3")
            .arg(QString::fromStdString(ship->name))
            .arg(QString::fromStdString(star->name))
            .arg(turnCount(eta)));
}

void MainWindow::queueProduction(ProductionKind kind)
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    if (!star || !is_surveyed(state_, 1, star->id) || !planet || planet->owner != 1) {
        return;
    }

    appendPendingOrder(
        QueueProductionOrder{planet->id, kind},
        QString("Queue %1 at %2")
            .arg(productionName(kind))
            .arg(QString::fromStdString(planet->name)));
}

void MainWindow::queueColonize()
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const auto* ship = colonyShipAtSelectedStar();
    if (!star || !is_surveyed(state_, 1, star->id) || !planet || planet->owner != 0 || !ship) {
        return;
    }

    appendPendingOrder(
        ColonizePlanetOrder{ship->id, planet->id},
        QString("Colonize %1 with %2")
            .arg(QString::fromStdString(planet->name))
            .arg(QString::fromStdString(ship->name)));
}

void MainWindow::endTurn()
{
    state_ = processor_.process(state_, {pendingOrders_});
    pendingOrders_.orders.clear();
    pendingDescriptions_.clear();
    selectedStarId_.reset();
    rebuildScene();
    statusBar()->showMessage(
        QString("Turn %1 — fleets advanced, economy and scouting updated")
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
    } catch (const std::exception& error) {
        statusBar()->showMessage(QString("Galaxy generation failed: %1").arg(error.what()));
        return;
    }

    pendingOrders_ = PlayerOrders{1, {}};
    pendingDescriptions_.clear();
    selectedStarId_.reset();

    scene_->setSceneRect(
        -galaxyConfig_.width / 2.0 - 55.0,
        -galaxyConfig_.height / 2.0 - 55.0,
        galaxyConfig_.width + 110.0,
        galaxyConfig_.height + 110.0);
    rebuildScene();
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);

    statusBar()->showMessage(
        QString("New galaxy: seed %1, %2 systems")
            .arg(static_cast<qulonglong>(state_.galaxySeed))
            .arg(static_cast<qulonglong>(state_.stars.size())));
}

} // namespace suns
