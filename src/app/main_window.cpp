#include "main_window.hpp"

#include <QGraphicsEllipseItem>
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

namespace suns {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , state_(make_demo_game())
{
    setWindowTitle("Suns!");
    resize(1100, 720);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);

    scene_ = new QGraphicsScene(this);
    scene_->setSceneRect(-300.0, -240.0, 600.0, 480.0);

    auto* view = new QGraphicsView(scene_, central);
    view->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(view, 1);

    auto* sidePanel = new QWidget(central);
    sidePanel->setFixedWidth(270);
    auto* sideLayout = new QVBoxLayout(sidePanel);

    auto* title = new QLabel("<h2>Suns!</h2>", sidePanel);
    sideLayout->addWidget(title);

    auto* help = new QLabel(
        "Select a star on the map, queue a destination for Scout 1, then end the turn.",
        sidePanel);
    help->setWordWrap(true);
    sideLayout->addWidget(help);

    selectionLabel_ = new QLabel(sidePanel);
    selectionLabel_->setWordWrap(true);
    sideLayout->addWidget(selectionLabel_);

    moveButton_ = new QPushButton("Send Scout 1 here", sidePanel);
    sideLayout->addWidget(moveButton_);

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

    connect(moveButton_, &QPushButton::clicked, this, [this] {
        queueMoveToSelectedStar();
    });
    connect(endTurnButton_, &QPushButton::clicked, this, [this] {
        endTurn();
    });

    rebuildScene();
    statusBar()->showMessage("Turn 1 — choose a destination for Scout 1");
}

const StarSystem* MainWindow::selectedStar() const
{
    if (!selectedStarId_) {
        return nullptr;
    }

    const auto it = std::find_if(state_.stars.begin(), state_.stars.end(), [this](const StarSystem& star) {
        return star.id == *selectedStarId_;
    });
    return it == state_.stars.end() ? nullptr : &*it;
}

const Fleet* MainWindow::playerFleet() const
{
    const auto it = std::find_if(state_.fleets.begin(), state_.fleets.end(), [](const Fleet& fleet) {
        return fleet.owner == 1;
    });
    return it == state_.fleets.end() ? nullptr : &*it;
}

void MainWindow::rebuildScene()
{
    scene_->clear();
    selectedStarId_.reset();

    for (const auto& star : state_.stars) {
        constexpr double radius = 6.0;
        auto* marker = scene_->addEllipse(
            star.position.x - radius,
            star.position.y - radius,
            radius * 2.0,
            radius * 2.0);
        marker->setFlag(QGraphicsItem::ItemIsSelectable);
        marker->setData(0, static_cast<unsigned int>(star.id));
        marker->setToolTip(QString::fromStdString(star.name));

        auto* label = scene_->addText(QString::fromStdString(star.name));
        label->setPos(star.position.x + 9.0, star.position.y - 13.0);
    }

    for (const auto& fleet : state_.fleets) {
        constexpr double size = 8.0;
        auto* marker = scene_->addRect(
            fleet.position.x - size / 2.0,
            fleet.position.y + 10.0,
            size,
            size);
        marker->setToolTip(QString::fromStdString(fleet.name));

        auto* label = scene_->addText(QString::fromStdString(fleet.name));
        label->setPos(fleet.position.x + 7.0, fleet.position.y + 6.0);
    }

    updateControls();
}

void MainWindow::updateControls()
{
    const auto* star = selectedStar();
    const auto* fleet = playerFleet();

    if (star) {
        selectionLabel_->setText(
            QString("Selected: <b>%1</b><br>Coordinates: %2, %3")
                .arg(QString::fromStdString(star->name))
                .arg(star->position.x, 0, 'f', 0)
                .arg(star->position.y, 0, 'f', 0));
    } else {
        selectionLabel_->setText("Selected: none");
    }

    moveButton_->setEnabled(star != nullptr && fleet != nullptr);

    if (pendingOrders_.orders.empty()) {
        ordersLabel_->setText("Orders: none");
    } else {
        ordersLabel_->setText(
            QString("Orders queued: %1").arg(static_cast<qulonglong>(pendingOrders_.orders.size())));
    }

    endTurnButton_->setText(QString("End Turn %1").arg(static_cast<qulonglong>(state_.turn)));
}

void MainWindow::queueMoveToSelectedStar()
{
    const auto* star = selectedStar();
    const auto* fleet = playerFleet();
    if (!star || !fleet) {
        return;
    }

    // The bootstrap UI controls one fleet, so a new destination replaces its previous order.
    pendingOrders_.orders.clear();
    pendingOrders_.orders.emplace_back(MoveFleetOrder{fleet->id, star->position});

    ordersLabel_->setText(
        QString("Scout 1 destination: <b>%1</b>").arg(QString::fromStdString(star->name)));
    statusBar()->showMessage(
        QString("Move order queued for %1").arg(QString::fromStdString(star->name)));
}

void MainWindow::endTurn()
{
    state_ = processor_.process(state_, {pendingOrders_});
    pendingOrders_.orders.clear();
    rebuildScene();
    statusBar()->showMessage(
        QString("Turn %1 — orders processed").arg(static_cast<qulonglong>(state_.turn)));
}

} // namespace suns
