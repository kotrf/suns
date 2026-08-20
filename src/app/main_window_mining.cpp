#include "main_window.hpp"

#include <QGraphicsScene>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

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

    connect(queueMineButton, &QPushButton::clicked, this, [this] {
        const auto* star = selectedStar();
        const auto* planet = selectedPlanet();
        if (!star || !is_surveyed(state_, 1, star->id) || !planet || planet->owner != 1) return;
        appendPendingOrder(
            QueueProductionOrder{planet->id, ProductionKind::Mine},
            QString("Queue Mine at %1").arg(QString::fromStdString(planet->name)));
    });

    const auto refresh = [this, queueMineButton, miningSummary] {
        if (shuttingDown_) return;

        const auto* star = selectedStar();
        const auto* planet = selectedPlanet();
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
