#include "main_window.hpp"
#include "suns/wormholes.hpp"

#include <QAction>
#include <QDockWidget>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace suns {
namespace {
QString riskText(WormholeStability stability)
{
    switch (stability) {
    case WormholeStability::Unknown: return "Unclassified anomaly; risk unknown";
    case WormholeStability::Unstable: return "Unstable WH; elevated loss risk";
    case WormholeStability::Variable: return "Variable WH; moderate loss risk";
    case WormholeStability::Stable: return "Stable WH; small, real loss risk";
    }
    return {};
}

QString contactText(WormholeTransitStatus status)
{
    switch (status) {
    case WormholeTransitStatus::AwaitingEntryReport: return {}; // Entry itself is still unknown.
    case WormholeTransitStatus::AwaitingEmergence: return "Awaiting emergence report";
    case WormholeTransitStatus::Overdue: return "OVERDUE / NO CONTACT";
    case WormholeTransitStatus::PresumedLost: return "PRESUMED LOST";
    case WormholeTransitStatus::EmergenceConfirmed: return "Emergence confirmed";
    }
    return {};
}
} // namespace

void MainWindow::installWormholes()
{
    if (findChild<QDockWidget*>("wormholesDock")) return;
    auto* dock = new QDockWidget("Spatial anomalies & wormholes", this);
    dock->setObjectName("wormholesDock");
    auto* body = new QWidget(dock);
    auto* layout = new QVBoxLayout(body);
    auto* hint = new QLabel("Select a fleet and an anomaly. Approach to classify it, then explicitly enter. "
        "Both mouths drift; last observed coordinates may be stale. Lifetime is uncertain. "
        "Weak signatures need an Anomaly Detector (Electronics 6).", body);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* tree = new QTreeWidget(body);
    tree->setObjectName("wormholeContacts");
    tree->setHeaderLabels({"Contact", "Last observation", "Classification / status", "Known exit"});
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree->setRootIsDecorated(false);
    layout->addWidget(tree);
    auto* buttons = new QHBoxLayout;
    auto* approach = new QPushButton("Fly to anomaly", body);
    approach->setObjectName("approachWormholeButton");
    auto* enter = new QPushButton("Enter wormhole", body);
    enter->setObjectName("enterWormholeButton");
    enter->setToolTip("Deliberately risk this entire fleet; the exit may be unknown and the fleet may never emerge.");
    buttons->addWidget(approach);
    buttons->addWidget(enter);
    layout->addLayout(buttons);
    auto* status = new QLabel(body);
    status->setObjectName("wormholeFleetStatus");
    status->setWordWrap(true);
    layout->addWidget(status);
    dock->setWidget(body);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    dock->hide();
    auto* viewMenu = menuBar()->findChild<QMenu*>("sunsViewMenu");
    if (viewMenu) viewMenu->addAction(dock->toggleViewAction());
    else menuBar()->addAction(dock->toggleViewAction());
    connect(approach, &QPushButton::clicked, this, [this, tree] {
        if (const auto* item = tree->currentItem()) queueWormholeApproach(item->data(0, Qt::UserRole).toUInt(), false);
    });
    connect(enter, &QPushButton::clicked, this, [this, tree] {
        if (const auto* item = tree->currentItem()) queueWormholeApproach(item->data(0, Qt::UserRole).toUInt(), true);
    });
    const auto updateButtons = [this, tree, approach, enter] {
        const auto* item = tree->currentItem();
        const auto* knowledge = item ? known_wormhole(state_, pendingOrders_.player, item->data(0, Qt::UserRole).toUInt()) : nullptr;
        const bool possible = selectedFleet() && knowledge && !knowledge->collapsed;
        approach->setEnabled(possible);
        enter->setEnabled(possible && knowledge->stability != WormholeStability::Unknown);
    };
    connect(tree, &QTreeWidget::itemSelectionChanged, this, updateButtons);
    connect(this, &MainWindow::routeProgramContextChanged, this, [this, updateButtons] {
        refreshWormholes();
        updateButtons();
    });
    refreshWormholes();
    updateButtons();
}

void MainWindow::refreshWormholes()
{
    auto* tree = findChild<QTreeWidget*>("wormholeContacts");
    if (!tree) return;
    const auto selected = tree->currentItem() ? tree->currentItem()->data(0, Qt::UserRole).toUInt() : 0;
    const QSignalBlocker blocker(tree);
    tree->clear();
    const auto* player = find_player(state_, pendingOrders_.player);
    if (player) for (const auto& knowledge : player->wormholeKnowledge) {
        auto* row = new QTreeWidgetItem(tree);
        row->setText(0, QString("%1 %2").arg(knowledge.stability == WormholeStability::Unknown ? "Anomaly" : "WH").arg(knowledge.endpoint));
        row->setData(0, Qt::UserRole, knowledge.endpoint);
        row->setText(1, QString("Turn %1 (%2, %3)").arg(static_cast<qulonglong>(knowledge.observedTurn))
            .arg(knowledge.lastPosition.x, 0, 'f', 1).arg(knowledge.lastPosition.y, 0, 'f', 1));
        row->setText(2, knowledge.collapsed ? "Collapse observed" : riskText(knowledge.stability));
        row->setText(3, knowledge.linkedEndpoint ? QString("WH %1; last known link").arg(knowledge.linkedEndpoint) : "Unknown");
        if (knowledge.endpoint == selected) tree->setCurrentItem(row);
    }
    QStringList status;
    for (const auto& transit : state_.wormholeTransits) if (transit.lastContact.owner == pendingOrders_.player) {
        const auto text = contactText(transit.status);
        if (!text.isEmpty()) status << QString("%1: %2").arg(QString::fromStdString(transit.lastContact.name), text);
    }
    if (auto* label = findChild<QLabel*>("wormholeFleetStatus")) label->setText(status.join("\n"));
}

bool MainWindow::queueWormholeApproach(WormholeEndpointId endpoint, bool enter)
{
    const auto* fleet = selectedFleet();
    const auto* knowledge = known_wormhole(state_, pendingOrders_.player, endpoint);
    if (!fleet || !knowledge || knowledge->collapsed || (enter && knowledge->stability == WormholeStability::Unknown)) return false;
    FleetArrivalAction action;
    if (enter) { action.kind = FleetArrivalActionKind::EnterWormhole; action.wormholeEndpoint = endpoint; }
    replacePendingFleetMove(fleet->id, knowledge->lastPosition, selectedFleetSuggestedWarpForRouteProgram(), action,
        QString("%1 %2 at last observed coordinates%3").arg(enter ? "Enter WH" : "Approach anomaly").arg(endpoint)
            .arg(enter ? "; uncertain exit, entire fleet at risk" : ""));
    rebuildScene();
    return true;
}

void MainWindow::renderKnownWormholes()
{
    const auto* player = find_player(state_, pendingOrders_.player);
    if (!player) return;
    for (const auto& k : player->wormholeKnowledge) {
        const auto color = k.collapsed ? QColor("#718096") : QColor("#bf85ff");
        QPen pen(color, 1.5, k.collapsed ? Qt::DashLine : Qt::SolidLine);
        auto* marker = scene_->addEllipse(k.lastPosition.x-7, k.lastPosition.y-7, 14, 14, pen);
        marker->setZValue(8);
        const auto description = QString("%1 %2\n%3\nLast observed turn %4; mouths drift independently\nLifetime unknown; %5")
            .arg(k.stability == WormholeStability::Unknown ? "Anomaly" : "WH").arg(k.endpoint)
            .arg(k.collapsed ? "Collapse observed" : riskText(k.stability)).arg(static_cast<qulonglong>(k.observedTurn))
            .arg(k.linkedEndpoint ? QString("last known link WH %1").arg(k.linkedEndpoint) : "exit unknown");
        marker->setToolTip(description);
        auto* label = scene_->addText(QString("%1 %2").arg(k.stability == WormholeStability::Unknown ? "?" : "WH").arg(k.endpoint));
        label->setPos(k.lastPosition.x+9, k.lastPosition.y-13);
        label->setDefaultTextColor(color);
        label->setToolTip(description);
        label->setScale(0.8);
        label->setZValue(8);
    }
}

} // namespace suns
