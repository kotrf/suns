#include "main_window.hpp"

#include <QGraphicsScene>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace suns {
namespace {

QString miningDeltaText(const MineralCargo& current, const MineralCargo& next)
{
    return QString("+I %1 / +B %2 / +G %3 per turn")
        .arg(next.ironium - current.ironium, 0, 'f', 2)
        .arg(next.boranium - current.boranium, 0, 'f', 2)
        .arg(next.germanium - current.germanium, 0, 'f', 2);
}

} // namespace

void MainWindow::installMiningInfrastructure()
{
    auto* productionGroup = findChild<QGroupBox*>("productionGroup");
    auto* productionLayout = productionGroup
        ? qobject_cast<QVBoxLayout*>(productionGroup->layout())
        : nullptr;
    if (!productionGroup || !productionLayout) return;

    auto* miningSummary = new QLabel(productionGroup);
    miningSummary->setObjectName("miningInfrastructureSummary");
    miningSummary->setWordWrap(true);
    miningSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* queueMineButton = new QPushButton(
        QString("Queue Mine (%1 production)").arg(kMineCost),
        productionGroup);
    queueMineButton->setObjectName("queueMineButton");
    queueMineButton->setToolTip(
        "Build permanent extraction infrastructure. One Mine costs I 1 / B 2 / G 1 and adds 0.75 extraction units every turn.");

    const int factoryIndex = productionLayout->indexOf(buildFactoryButton_);
    const int insertAt = factoryIndex >= 0 ? factoryIndex + 1 : productionLayout->count();
    productionLayout->insertWidget(insertAt, queueMineButton);
    productionLayout->insertWidget(insertAt + 1, miningSummary);

    auto* remoteMiningSummary = new QLabel(productionGroup);
    remoteMiningSummary->setObjectName("remoteMiningSummary");
    remoteMiningSummary->setWordWrap(true);
    remoteMiningSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* remoteMiningButton = new QPushButton("Start Remote Mining", productionGroup);
    remoteMiningButton->setObjectName("remoteMiningButton");
    remoteMiningButton->setToolTip(
        "Assign or remove the selected fleet's stationary mining task. Plotting a course cancels the task when the command arrives.");
    productionLayout->insertWidget(insertAt + 2, remoteMiningSummary);
    productionLayout->insertWidget(insertAt + 3, remoteMiningButton);

    connect(queueMineButton, &QPushButton::clicked, this, [this] {
        const auto* star = selectedStar();
        const auto* planet = selectedPlanet();
        if (!star || !is_surveyed(state_, 1, star->id) || !planet || planet->owner != 1) return;
        appendPendingOrder(
            QueueProductionOrder{planet->id, ProductionKind::Mine},
            QString("Queue Mine at %1").arg(QString::fromStdString(planet->name)));
    });

    connect(remoteMiningButton, &QPushButton::clicked, this, [this] {
        const auto* fleet = selectedFleet();
        if (!fleet) return;

        bool enabled = fleet->task == FleetTask::RemoteMining;
        for (const auto& command : fleet->pendingCommands) {
            if (command.task) enabled = *command.task == FleetTask::RemoteMining;
        }
        for (const auto& order : pendingOrders_.orders) {
            if (const auto* mining = std::get_if<SetRemoteMiningOrder>(&order); mining && mining->fleet == fleet->id) {
                enabled = mining->enabled;
            }
        }
        const bool requested = !enabled;
        const auto description = QString("%1 remote mining for %2")
                                     .arg(requested ? "Start" : "Stop")
                                     .arg(QString::fromStdString(fleet->name));

        for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
            if (const auto* mining = std::get_if<SetRemoteMiningOrder>(&pendingOrders_.orders[index]);
                mining && mining->fleet == fleet->id) {
                pendingOrders_.orders[index] = SetRemoteMiningOrder{fleet->id, requested};
                pendingDescriptions_[static_cast<int>(index)] = description;
                rebuildScene();
                statusBar()->showMessage(description);
                return;
            }
        }
        appendPendingOrder(SetRemoteMiningOrder{fleet->id, requested}, description);
    });

    const auto refresh = [this, queueMineButton, miningSummary, remoteMiningButton, remoteMiningSummary] {
        if (shuttingDown_) return;

        const auto* star = selectedStar();
        const auto* planet = selectedPlanet();
        const auto* fleet = selectedFleet();
        bool miningEnabled = fleet && fleet->task == FleetTask::RemoteMining;
        if (fleet) {
            for (const auto& command : fleet->pendingCommands) {
                if (command.task) miningEnabled = *command.task == FleetTask::RemoteMining;
            }
            for (const auto& order : pendingOrders_.orders) {
                if (const auto* mining = std::get_if<SetRemoteMiningOrder>(&order); mining && mining->fleet == fleet->id) {
                    miningEnabled = mining->enabled;
                }
            }
        }
        const auto* design = fleet ? fleet_design(state_, *fleet) : nullptr;
        const bool hasMiningModule = design && std::any_of(
            design->components.begin(), design->components.end(), [](ShipComponentType component) {
                return component_spec(component).remoteMiningUnits > 0.0;
            });
        const bool atUncolonizedPlanet = fleet && star && planet && planet->owner == 0
            && same_position(fleet->position, star->position);
        const bool stationary = fleet && !fleet->destination && fleet->waypointQueue.empty();
        const bool canStart = hasMiningModule && atUncolonizedPlanet && stationary;

        remoteMiningButton->setText(miningEnabled ? "Stop Remote Mining" : "Start Remote Mining");
        remoteMiningButton->setEnabled(fleet && (miningEnabled || canStart));
        if (miningEnabled) {
            remoteMiningSummary->setText(
                "<b>Fleet task: Remote Mining</b><br><span style='color:#a9bdd0'>Minerals remain on the planetary surface.</span>");
        } else if (canStart) {
            remoteMiningSummary->setText(
                "Remote miner ready in orbit. Assign the task to begin extraction at End Turn.");
        } else if (hasMiningModule) {
            remoteMiningSummary->setText(
                "<span style='color:#8090a2'>A remote miner must be stationary at an uncolonized planet.</span>");
        } else {
            remoteMiningSummary->setText(
                "<span style='color:#8090a2'>Select a fleet fitted with a Remote Mining Module.</span>");
        }

        const bool friendlyColony = star
            && is_surveyed(state_, 1, star->id)
            && planet
            && planet->owner == 1;

        queueMineButton->setEnabled(friendlyColony);
        if (!friendlyColony) {
            miningSummary->setText(
                "<span style='color:#8090a2'>Select a friendly colony to evaluate mining infrastructure.</span>");
            return;
        }

        const auto currentMining = projected_mineral_mining(state_, *planet);
        Planet withAnotherMine = *planet;
        ++withAnotherMine.mines;
        const auto nextMining = projected_mineral_mining(state_, withAnotherMine);

        miningSummary->setText(
            QString("Mines <b>%1</b> • current extraction I %2 / B %3 / G %4<br>"
                    "<span style='color:#a9bdd0'>Next Mine: %5</span>")
                .arg(planet->mines)
                .arg(currentMining.ironium, 0, 'f', 2)
                .arg(currentMining.boranium, 0, 'f', 2)
                .arg(currentMining.germanium, 0, 'f', 2)
                .arg(miningDeltaText(currentMining, nextMining)));
    };

    auto* refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    refreshTimer->setInterval(0);
    connect(refreshTimer, &QTimer::timeout, this, refresh);
    if (scene_) {
        connect(scene_, &QGraphicsScene::changed, this, [this, refreshTimer](const QList<QRectF>&) {
            if (!shuttingDown_) refreshTimer->start();
        });
        connect(scene_, &QGraphicsScene::selectionChanged, this, [this, refreshTimer] {
            if (!shuttingDown_) refreshTimer->start();
        });
    }
    refresh();
}

} // namespace suns
