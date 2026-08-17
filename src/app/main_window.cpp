#include "main_window.hpp"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
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

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , state_(make_demo_game())
{
    setWindowTitle("Suns!");
    resize(1200, 780);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);

    scene_ = new QGraphicsScene(this);
    scene_->setSceneRect(-300.0, -240.0, 600.0, 480.0);

    auto* view = new QGraphicsView(scene_, central);
    view->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(view, 1);

    auto* sidePanel = new QWidget(central);
    sidePanel->setFixedWidth(350);
    auto* sideLayout = new QVBoxLayout(sidePanel);

    sideLayout->addWidget(new QLabel("<h2>Suns!</h2>", sidePanel));

    auto* help = new QLabel(
        "Star positions are known, but planetary data is not. Send Scout 1 to an unknown system, "
        "end the turn, then inspect the newly surveyed world before deciding where to expand.",
        sidePanel);
    help->setWordWrap(true);
    sideLayout->addWidget(help);

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
    statusBar()->showMessage("Turn 1 — choose the first system to survey");
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
    scene_->clear();
    selectedStarId_.reset();

    for (const auto& star : state_.stars) {
        constexpr double radius = 7.0;
        auto* marker = scene_->addEllipse(
            star.position.x - radius,
            star.position.y - radius,
            radius * 2.0,
            radius * 2.0);
        marker->setFlag(QGraphicsItem::ItemIsSelectable);
        marker->setData(0, static_cast<unsigned int>(star.id));

        const bool surveyed = is_surveyed(state_, 1, star.id);
        const auto* planet = find_planet_at_star(state_, star.id);
        QString tooltip = QString::fromStdString(star.name);
        QString mapLabel = QString::fromStdString(star.name);

        if (!surveyed) {
            tooltip += "\nUnsurveyed system — planetary data unknown";
            mapLabel += "  [?]";
        } else if (planet) {
            tooltip += QString("\n%1 — habitability %2%")
                           .arg(QString::fromStdString(planet->name))
                           .arg(planet->habitability);
            if (planet->owner == 1) {
                mapLabel += "  [COLONY]";
                tooltip += QString("\nIndustry %1 — %2")
                               .arg(planet->industry)
                               .arg(productionSummary(*planet));
            }
        }
        marker->setToolTip(tooltip);

        auto* label = scene_->addText(mapLabel);
        label->setPos(star.position.x + 10.0, star.position.y - 14.0);
    }

    int fleetOffset = 0;
    for (const auto& fleet : state_.fleets) {
        constexpr double size = 8.0;
        const double y = fleet.position.y + 12.0 + fleetOffset * 11.0;
        auto* marker = scene_->addRect(
            fleet.position.x - size / 2.0,
            y,
            size,
            size);
        marker->setToolTip(QString::fromStdString(fleet.name));

        auto* label = scene_->addText(QString::fromStdString(fleet.name));
        label->setPos(fleet.position.x + 7.0, y - 5.0);
        ++fleetOffset;
    }

    updateControls();
}

void MainWindow::updateControls()
{
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const bool surveyed = star && is_surveyed(state_, 1, star->id);

    std::size_t colonies = 0;
    std::uint64_t population = 0;
    std::uint32_t stockpile = 0;
    std::uint32_t industry = 0;
    for (const auto& candidate : state_.planets) {
        if (candidate.owner == 1) {
            ++colonies;
            population += candidate.population;
            stockpile += candidate.stockpile;
            industry += candidate.industry;
        }
    }

    const auto colonyShips = static_cast<std::size_t>(std::count_if(
        state_.fleets.begin(), state_.fleets.end(), [](const Fleet& fleet) {
            return fleet.owner == 1 && fleet.role == FleetRole::ColonyShip;
        }));

    const auto* player = find_player(state_, 1);
    const auto surveyedCount = player ? player->surveyedStars.size() : 0;

    empireLabel_->setText(
        QString("<b>Terrans</b><br>Surveyed: %1 / %2 &nbsp; Colonies: %3<br>"
                "Population: %4 &nbsp; Industry: %5 / turn<br>Stored: %6 &nbsp; Colony ships: %7")
            .arg(static_cast<qulonglong>(surveyedCount))
            .arg(static_cast<qulonglong>(state_.stars.size()))
            .arg(static_cast<qulonglong>(colonies))
            .arg(static_cast<qulonglong>(population))
            .arg(industry)
            .arg(stockpile)
            .arg(static_cast<qulonglong>(colonyShips)));

    if (star && !surveyed) {
        selectionLabel_->setText(
            QString("<hr><b>%1</b><br><b>UNSURVEYED</b><br>Planetary data unknown.<br>"
                    "Send Scout 1 here and end the turn to survey this system.")
                .arg(QString::fromStdString(star->name)));
    } else if (star && planet) {
        const QString owner = planet->owner == 1 ? "Terran colony" : "Uncolonized";
        selectionLabel_->setText(
            QString("<hr><b>%1</b><br>%2<br>Habitability: %3%<br>Status: %4<br>"
                    "Population: %5<br>Industry: %6 / turn<br>Stored production: %7<br>"
                    "Production: <b>%8</b>")
                .arg(QString::fromStdString(star->name))
                .arg(QString::fromStdString(planet->name))
                .arg(planet->habitability)
                .arg(owner)
                .arg(static_cast<qulonglong>(planet->population))
                .arg(planet->industry)
                .arg(planet->stockpile)
                .arg(productionSummary(*planet)));
    } else {
        selectionLabel_->setText("<hr>Select a star system.");
    }

    scoutMoveButton_->setEnabled(star != nullptr && playerScout() != nullptr);
    colonyMoveButton_->setEnabled(star != nullptr && playerColonyShip() != nullptr);

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
    updateControls();
    statusBar()->showMessage(description);
}

void MainWindow::queueScoutMove()
{
    const auto* star = selectedStar();
    const auto* scout = playerScout();
    if (!star || !scout) {
        return;
    }

    appendPendingOrder(
        MoveFleetOrder{scout->id, star->position},
        QString("Move Scout 1 to %1 and survey")
            .arg(QString::fromStdString(star->name)));
}

void MainWindow::queueColonyShipMove()
{
    const auto* star = selectedStar();
    const auto* ship = playerColonyShip();
    if (!star || !ship) {
        return;
    }

    appendPendingOrder(
        MoveFleetOrder{ship->id, star->position},
        QString("Move %1 to %2")
            .arg(QString::fromStdString(ship->name))
            .arg(QString::fromStdString(star->name)));
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
    rebuildScene();
    statusBar()->showMessage(
        QString("Turn %1 — orders resolved, scouting intel and production updated")
            .arg(static_cast<qulonglong>(state_.turn)));
}

} // namespace suns
