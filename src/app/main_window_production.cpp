#include "main_window.hpp"

#include <QDockWidget>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QShortcut>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace suns {
namespace {

QString mineralAmounts(const MineralCargo& minerals)
{
    return QString("I %1 / B %2 / G %3 kt")
        .arg(minerals.ironium, 0, 'f', 1).arg(minerals.boranium, 0, 'f', 1).arg(minerals.germanium, 0, 'f', 1);
}

QString productionItemName(const GameState& state, const ProductionItem& item)
{
    switch (item.kind) {
    case ProductionKind::Factory: return "Factory";
    case ProductionKind::Mine: return "Mine";
    case ProductionKind::Research: return "Legacy Research item";
    case ProductionKind::OrbitalStation: return "Orbital Dock";
    case ProductionKind::ColonyShip:
        if (const auto* design = find_ship_design(
                state, item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId)) {
            return QString::fromStdString(design->name);
        }
        return "Ship";
    }
    return "Production";
}

std::vector<ProductionItem> plannedQueue(
    const GameState& state, const Planet& planet, const PlayerOrders& pending)
{
    auto queue = planet.productionQueue;
    std::erase_if(queue, [](const ProductionItem& item) {
        return item.kind == ProductionKind::Research;
    });
    const auto addDefaultShip = [&] {
        const auto design = std::find_if(state.shipDesigns.begin(), state.shipDesigns.end(), [&](const ShipDesign& candidate) {
            return candidate.owner == pending.player && ship_design_can_colonize(candidate);
        });
        if (design != state.shipDesigns.end()) {
            queue.push_back({ProductionKind::ColonyShip, ship_design_cost(*design), design->id});
        }
    };

    for (const auto& order : pending.orders) {
        std::visit([&](const auto& concrete) {
            using T = std::decay_t<decltype(concrete)>;
            if constexpr (std::is_same_v<T, QueueProductionOrder>) {
                if (concrete.colony != planet.id) return;
                if (concrete.kind == ProductionKind::Factory) {
                    queue.push_back({ProductionKind::Factory, kFactoryCost, 0});
                } else if (concrete.kind == ProductionKind::Mine) {
                    queue.push_back({ProductionKind::Mine, kMineCost, 0});
                } else if (concrete.kind == ProductionKind::OrbitalStation) {
                    if (!find_orbital_station_at_planet(state, planet.id)
                        && std::none_of(queue.begin(), queue.end(), [](const ProductionItem& item) {
                            return item.kind == ProductionKind::OrbitalStation;
                        })) queue.push_back({ProductionKind::OrbitalStation, kOrbitalDockCost, 0});
                } else if (concrete.kind == ProductionKind::ColonyShip) {
                    addDefaultShip();
                }
            } else if constexpr (std::is_same_v<T, QueueShipDesignOrder>) {
                if (concrete.colony != planet.id) return;
                if (const auto* design = resolve_ship_design_order(state, pending.player, concrete);
                    design && design->owner == pending.player && ship_design_valid(*design)
                    && colony_has_orbital_service(state, planet.id, pending.player,
                        OrbitalStationModule::Shipyard)) {
                    queue.push_back({ProductionKind::ColonyShip, ship_design_cost(*design), design->id});
                }
            } else if constexpr (std::is_same_v<T, CancelProductionOrder>) {
                if (concrete.colony == planet.id && concrete.index < queue.size()) {
                    queue.erase(queue.begin() + concrete.index);
                }
            } else if constexpr (std::is_same_v<T, ReorderProductionQueueOrder>) {
                if (concrete.colony != planet.id
                    || concrete.fromIndex >= queue.size()
                    || concrete.toIndex >= queue.size()
                    || concrete.fromIndex == concrete.toIndex) {
                    return;
                }
                auto item = std::move(queue[concrete.fromIndex]);
                queue.erase(queue.begin() + concrete.fromIndex);
                queue.insert(queue.begin() + concrete.toIndex, std::move(item));
            }
        }, order);
    }
    return queue;
}

} // namespace

std::vector<ProductionItem> MainWindow::plannedProductionQueue(const Planet& planet) const
{
    return plannedQueue(planned_ship_design_state(state_, pendingOrders_), planet, pendingOrders_);
}

void MainWindow::installProductionQueue()
{
    auto* dock = findChild<QDockWidget*>("productionDock");
    auto* panel = dock ? dock->widget() : nullptr;
    auto* panelLayout = panel ? qobject_cast<QVBoxLayout*>(panel->layout()) : nullptr;
    if (!panelLayout) return;

    auto* group = new QGroupBox("Build queue", panel);
    group->setObjectName("productionQueueGroup");
    auto* layout = new QVBoxLayout(group);

    productionQueueSummary_ = new QLabel(group);
    productionQueueSummary_->setWordWrap(true);
    layout->addWidget(productionQueueSummary_);

    productionQueueTree_ = new QTreeWidget(group);
    productionQueueTree_->setObjectName("productionQueueTree");
    productionQueueTree_->setColumnCount(4);
    productionQueueTree_->setHeaderLabels({"#", "Item", "Remaining", "Completion"});
    productionQueueTree_->setRootIsDecorated(false);
    productionQueueTree_->setAlternatingRowColors(true);
    productionQueueTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    productionQueueTree_->setMinimumHeight(72);
    productionQueueTree_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    productionQueueTree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    productionQueueTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    productionQueueTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    productionQueueTree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(productionQueueTree_, 1);
    productionMineralDetails_ = new QLabel(group);
    productionMineralDetails_->setObjectName("productionMineralDetails");
    productionMineralDetails_->setTextFormat(Qt::PlainText);
    productionMineralDetails_->setWordWrap(true);
    layout->addWidget(productionMineralDetails_);

    auto* moveRow = new QHBoxLayout;
    productionMoveUpButton_ = new QPushButton("Move up", group);
    productionMoveDownButton_ = new QPushButton("Move down", group);
    productionRemoveButton_ = new QPushButton("Remove", group);
    productionRemoveButton_->setObjectName("productionRemoveButton");
    productionRemoveButton_->setToolTip("Remove the selected build (Delete). Production already spent is lost; minerals are charged only on completion.");
    productionMoveUpButton_->setToolTip("Move the selected item one position earlier");
    productionMoveDownButton_->setToolTip("Move the selected item one position later");
    moveRow->addWidget(productionMoveUpButton_);
    moveRow->addWidget(productionMoveDownButton_);
    moveRow->addWidget(productionRemoveButton_);
    layout->addLayout(moveRow);
    panelLayout->addWidget(group);

    connect(productionMoveUpButton_, &QPushButton::clicked, this,
        [this] { moveSelectedProductionItem(-1); });
    connect(productionMoveDownButton_, &QPushButton::clicked, this,
        [this] { moveSelectedProductionItem(1); });
    connect(productionRemoveButton_, &QPushButton::clicked, this, &MainWindow::removeSelectedProductionItem);
    auto* removeShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), productionQueueTree_);
    removeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(removeShortcut, &QShortcut::activated, this, &MainWindow::removeSelectedProductionItem);
    connect(productionQueueTree_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto row = productionQueueTree_->indexOfTopLevelItem(productionQueueTree_->currentItem());
        productionMineralDetails_->setText(row >= 0
            ? productionQueueTree_->currentItem()->data(0, Qt::UserRole + 1).toString() : QString{});
        productionMoveUpButton_->setEnabled(row > 0);
        productionRemoveButton_->setEnabled(row >= 0);
        productionMoveDownButton_->setEnabled(
            row >= 0 && row + 1 < productionQueueTree_->topLevelItemCount());
    });

    auto* timer = new QTimer(this);
    timer->setInterval(150);
    connect(timer, &QTimer::timeout, this, &MainWindow::refreshProductionQueue);
    timer->start();
    refreshProductionQueue();
}

void MainWindow::refreshProductionQueue()
{
    if (shuttingDown_ || !productionQueueTree_ || !productionQueueSummary_) return;
    const auto previousRow = productionQueueTree_->indexOfTopLevelItem(productionQueueTree_->currentItem());
    productionQueueTree_->clear();
    productionMineralDetails_->clear();
    productionQueuePlanet_.reset();

    const auto* planet = selectedPlanet();
    if (!planet || planet->owner != pendingOrders_.player) {
        productionQueueSummary_->setText("Select a friendly colony to inspect its production plan.");
        productionMoveUpButton_->setEnabled(false);
        productionMoveDownButton_->setEnabled(false);
        productionRemoveButton_->setEnabled(false);
        return;
    }

    productionQueuePlanet_ = planet->id;
    const auto queue = plannedProductionQueue(*planet);
    auto forecastState = planned_ship_design_state(state_, pendingOrders_);
    const auto forecastPlayerIt = std::find_if(
        forecastState.players.begin(), forecastState.players.end(), [this](const Player& player) {
            return player.id == pendingOrders_.player;
        });
    auto* forecastPlayer = forecastPlayerIt == forecastState.players.end() ? nullptr : &*forecastPlayerIt;
    if (forecastPlayer) {
        for (const auto& order : pendingOrders_.orders) {
            if (const auto* allocation = std::get_if<SetResearchAllocationOrder>(&order)) {
                forecastPlayer->technology.researchAllocationPercent = allocation->percent;
            } else if (const auto* plan = std::get_if<SetResearchPlanOrder>(&order)) {
                forecastPlayer->technology.researchActive = plan->active;
            }
        }
    }
    const auto forecast = forecast_production_queue(forecastState, *planet, queue);
    const auto allocationPercent = forecastPlayer && forecastPlayer->technology.researchActive
        ? forecastPlayer->technology.researchAllocationPercent
        : 0;
    const auto output = colony_output(*planet);
    const auto guaranteedResearch = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(output) * allocationPercent / 100U);
    productionQueueSummary_->setText(
        QString("<b>%1</b> • output %2/turn • guaranteed research %3 • production %4 • %5 item%6")
            .arg(QString::fromStdString(planet->name))
            .arg(output)
            .arg(guaranteedResearch)
            .arg(output - guaranteedResearch)
            .arg(static_cast<qulonglong>(queue.size()))
            .arg(queue.size() == 1 ? "" : "s"));

    MineralCargo total;
    for (const auto& item : queue) {
        const auto cost = production_item_mineral_cost(forecastState, item);
        total.ironium += cost.ironium; total.boranium += cost.boranium; total.germanium += cost.germanium;
    }
    productionQueueSummary_->setText(productionQueueSummary_->text()
        + QString("<br>Stock: %1<br>Mining/turn: %2<br>Queue minerals: %3<br>Minerals are spent on completion.")
            .arg(mineralAmounts(planet->minerals), mineralAmounts(projected_mineral_mining(state_, *planet)), mineralAmounts(total)));

    bool shipyardAvailable = colony_has_orbital_service(
        state_, planet->id, planet->owner, OrbitalStationModule::Shipyard);
    for (std::size_t index = 0; index < queue.size(); ++index) {
        const auto& item = queue[index];
        QString remaining = QString::number(item.remainingCost);
        const bool waitingForShipyard = item.kind == ProductionKind::ColonyShip
            && !shipyardAvailable;
        if (waitingForShipyard) {
            remaining = "shipyard";
        } else if (item.remainingCost == 0
            && !mineral_cargo_sufficient(planet->minerals, production_item_mineral_cost(forecastState, item))) {
            remaining = "minerals";
        }

        QString completion;
        if (forecast[index].completionTurn) {
            completion = QString("Turn %1 (+%2)")
                .arg(static_cast<qulonglong>(*forecast[index].completionTurn))
                .arg(static_cast<qulonglong>(*forecast[index].completionTurn - state_.turn));
        } else completion = waitingForShipyard ? "waiting for dock" : "beyond forecast";

        auto* row = new QTreeWidgetItem(productionQueueTree_);
        row->setText(0, QString::number(index + 1));
        row->setText(1, productionItemName(forecastState, item));
        row->setText(2, remaining);
        row->setText(3, completion);
        row->setData(0, Qt::UserRole, static_cast<qulonglong>(index));
        const auto cost = production_item_mineral_cost(forecastState, item);
        const MineralCargo missing{std::max(0.0, cost.ironium - planet->minerals.ironium),
            std::max(0.0, cost.boranium - planet->minerals.boranium),
            std::max(0.0, cost.germanium - planet->minerals.germanium)};
        const auto details = QString("%1 — minerals charged on completion: %2\n"
                                     "Shortfall against current stock before other builds: %3")
            .arg(productionItemName(forecastState, item), mineralAmounts(cost), mineralAmounts(missing));
        row->setData(0, Qt::UserRole + 1, details);
        for (int column = 0; column < 4; ++column) row->setToolTip(column, details);
        if (item.kind == ProductionKind::OrbitalStation) shipyardAvailable = true;
    }

    if (!queue.empty()) {
        const auto selectedRow = std::clamp(previousRow, 0, static_cast<int>(queue.size() - 1));
        productionQueueTree_->setCurrentItem(productionQueueTree_->topLevelItem(selectedRow));
    }
    const auto row = productionQueueTree_->indexOfTopLevelItem(productionQueueTree_->currentItem());
    productionMoveUpButton_->setEnabled(row > 0);
    productionMoveDownButton_->setEnabled(row >= 0 && row + 1 < static_cast<int>(queue.size()));
    productionRemoveButton_->setEnabled(row >= 0);
}

void MainWindow::removeSelectedProductionItem()
{
    const auto* planet = selectedPlanet();
    if (!planet || planet->owner != pendingOrders_.player || !productionQueueTree_
        || productionQueuePlanet_ != planet->id) return;
    const auto row = productionQueueTree_->indexOfTopLevelItem(productionQueueTree_->currentItem());
    if (row < 0) return;
    const auto queue = plannedProductionQueue(*planet);
    if (static_cast<std::size_t>(row) >= queue.size()) return;
    appendPendingOrder(
        CancelProductionOrder{planet->id, static_cast<std::uint32_t>(row)},
        QString("Cancel %1 at %2")
            .arg(productionItemName(planned_ship_design_state(state_, pendingOrders_), queue[row]))
            .arg(QString::fromStdString(planet->name)));
    refreshProductionQueue();
}

void MainWindow::moveSelectedProductionItem(int direction)
{
    const auto* planet = selectedPlanet();
    if (!planet || planet->owner != pendingOrders_.player || !productionQueueTree_
        || productionQueuePlanet_ != planet->id) return;
    const auto from = productionQueueTree_->indexOfTopLevelItem(productionQueueTree_->currentItem());
    const auto to = from + direction;
    if (from < 0 || to < 0 || to >= productionQueueTree_->topLevelItemCount()) return;

    const auto itemName = productionQueueTree_->topLevelItem(from)->text(1);
    appendPendingOrder(
        ReorderProductionQueueOrder{
            planet->id,
            static_cast<std::uint32_t>(from),
            static_cast<std::uint32_t>(to),
        },
        QString("Move %1 to production position %2 at %3")
            .arg(itemName)
            .arg(to + 1)
            .arg(QString::fromStdString(planet->name)));
    refreshProductionQueue();
    productionQueueTree_->setCurrentItem(productionQueueTree_->topLevelItem(to));
}

} // namespace suns
