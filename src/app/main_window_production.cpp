#include "main_window.hpp"

#include <QDockWidget>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace suns {
namespace {

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
        if (const auto* design = find_ship_design(state, kColonyShipDesignId)) {
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
                    queue.push_back({ProductionKind::OrbitalStation, kOrbitalDockCost, 0});
                } else if (concrete.kind == ProductionKind::ColonyShip) {
                    addDefaultShip();
                }
            } else if constexpr (std::is_same_v<T, QueueShipDesignOrder>) {
                if (concrete.colony != planet.id) return;
                if (const auto* design = find_ship_design(state, concrete.design)) {
                    queue.push_back({ProductionKind::ColonyShip, ship_design_cost(*design), design->id});
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
    productionQueueTree_->setMinimumHeight(210);
    productionQueueTree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    productionQueueTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    productionQueueTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    productionQueueTree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(productionQueueTree_, 1);

    auto* moveRow = new QHBoxLayout;
    productionMoveUpButton_ = new QPushButton("Move up", group);
    productionMoveDownButton_ = new QPushButton("Move down", group);
    productionMoveUpButton_->setToolTip("Move the selected item one position earlier");
    productionMoveDownButton_->setToolTip("Move the selected item one position later");
    moveRow->addWidget(productionMoveUpButton_);
    moveRow->addWidget(productionMoveDownButton_);
    layout->addLayout(moveRow);
    panelLayout->addWidget(group, 1);

    connect(productionMoveUpButton_, &QPushButton::clicked, this,
        [this] { moveSelectedProductionItem(-1); });
    connect(productionMoveDownButton_, &QPushButton::clicked, this,
        [this] { moveSelectedProductionItem(1); });
    connect(productionQueueTree_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto row = productionQueueTree_->indexOfTopLevelItem(productionQueueTree_->currentItem());
        productionMoveUpButton_->setEnabled(row > 0);
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

    const auto* planet = selectedPlanet();
    if (!planet || planet->owner != pendingOrders_.player) {
        productionQueueSummary_->setText("Select a friendly colony to inspect its production plan.");
        productionMoveUpButton_->setEnabled(false);
        productionMoveDownButton_->setEnabled(false);
        return;
    }

    const auto queue = plannedQueue(state_, *planet, pendingOrders_);
    auto forecastState = state_;
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
            && !mineral_cargo_sufficient(planet->minerals, production_item_mineral_cost(state_, item))) {
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
        row->setText(1, productionItemName(state_, item));
        row->setText(2, remaining);
        row->setText(3, completion);
        row->setData(0, Qt::UserRole, static_cast<qulonglong>(index));
        if (item.kind == ProductionKind::OrbitalStation) shipyardAvailable = true;
    }

    if (!queue.empty()) {
        const auto selectedRow = std::clamp(previousRow, 0, static_cast<int>(queue.size() - 1));
        productionQueueTree_->setCurrentItem(productionQueueTree_->topLevelItem(selectedRow));
    }
    const auto row = productionQueueTree_->indexOfTopLevelItem(productionQueueTree_->currentItem());
    productionMoveUpButton_->setEnabled(row > 0);
    productionMoveDownButton_->setEnabled(row >= 0 && row + 1 < static_cast<int>(queue.size()));
}

void MainWindow::moveSelectedProductionItem(int direction)
{
    const auto* planet = selectedPlanet();
    if (!planet || planet->owner != pendingOrders_.player || !productionQueueTree_) return;
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
