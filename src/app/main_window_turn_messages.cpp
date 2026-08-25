#include "main_window.hpp"

#include <QAction>
#include <QDockWidget>
#include <QFont>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace suns {

namespace {

constexpr int kEventIdRole = Qt::UserRole + 1;
constexpr int kStarIdRole = Qt::UserRole + 2;
constexpr int kUnreadRole = Qt::UserRole + 3;

QString event_text(const GameState& state, const GameEvent& event)
{
    const auto* star = find_star(state, event.star);
    const auto* planet = event.planet != 0 ? find_planet_at_star(state, event.star) : nullptr;
    const auto starName = star ? QString::fromStdString(star->name) : QString("System %1").arg(event.star);

    QString detail;
    if (planet) {
        const auto concentrations = planet_mineral_concentration(state, *planet);
        detail = QString("%1 — habitability %2%, deposits I %3 / B %4 / G %5")
                     .arg(QString::fromStdString(planet->name))
                     .arg(planet->habitability)
                     .arg(concentrations.ironium, 0, 'f', 1)
                     .arg(concentrations.boranium, 0, 'f', 1)
                     .arg(concentrations.germanium, 0, 'f', 1);
    }

    const auto delay = event.turn > event.observedTurn ? event.turn - event.observedTurn : 0;
    QString text = QString("Turn %1  •  Survey report: %2")
                       .arg(static_cast<qulonglong>(event.turn))
                       .arg(starName);
    if (!detail.isEmpty()) text += QString("\n%1").arg(detail);
    if (delay > 0) {
        text += QString("\nObserved Turn %1 • received after %2 turn%3")
                    .arg(static_cast<qulonglong>(event.observedTurn))
                    .arg(static_cast<qulonglong>(delay))
                    .arg(delay == 1 ? "" : "s");
    }
    return text;
}

void mark_read(QListWidgetItem* item)
{
    if (!item || !item->data(kUnreadRole).toBool()) return;
    item->setData(kUnreadRole, false);
    auto font = item->font();
    font.setBold(false);
    item->setFont(font);
}

} // namespace

void MainWindow::installTurnMessages()
{
    if (turnMessagesDock_) return;

    turnMessagesDock_ = new QDockWidget("Turn Messages", this);
    turnMessagesDock_->setObjectName("turnMessagesDock");
    turnMessagesDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);

    auto* content = new QWidget(turnMessagesDock_);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    turnMessagesSummary_ = new QLabel("No new strategic reports.", content);
    turnMessagesSummary_->setObjectName("turnMessagesSummary");
    turnMessagesList_ = new QListWidget(content);
    turnMessagesList_->setObjectName("turnMessagesList");
    turnMessagesList_->setWordWrap(true);
    turnMessagesList_->setAlternatingRowColors(true);

    auto* nextUnread = new QPushButton("Next unread", content);
    nextUnread->setObjectName("nextUnreadTurnMessage");
    layout->addWidget(turnMessagesSummary_);
    layout->addWidget(turnMessagesList_, 1);
    layout->addWidget(nextUnread);
    turnMessagesDock_->setWidget(content);
    addDockWidget(Qt::BottomDockWidgetArea, turnMessagesDock_);
    turnMessagesDock_->hide();

    QMenu* view = menuBar()->findChild<QMenu*>("sunsViewMenu");
    if (!view) {
        view = menuBar()->addMenu("&View");
        view->setObjectName("sunsViewMenu");
    }
    view->addAction(turnMessagesDock_->toggleViewAction());

    const auto focus = [this](QListWidgetItem* item) {
        if (!item) return;
        mark_read(item);
        const auto starId = static_cast<StarId>(item->data(kStarIdRole).toUInt());
        const auto* star = find_star(state_, starId);
        if (!star) return;
        selectedStarId_ = starId;
        selectedFleetId_.reset();
        rebuildScene();
        view_->centerOn(star->position.x, star->position.y);
    };
    connect(turnMessagesList_, &QListWidget::itemActivated, this, focus);
    connect(turnMessagesList_, &QListWidget::itemClicked, this, focus);
    connect(nextUnread, &QPushButton::clicked, this, [this, focus] {
        if (!turnMessagesList_) return;
        for (int row = 0; row < turnMessagesList_->count(); ++row) {
            auto* item = turnMessagesList_->item(row);
            if (!item->data(kUnreadRole).toBool()) continue;
            turnMessagesList_->setCurrentItem(item);
            turnMessagesList_->scrollToItem(item);
            focus(item);
            return;
        }
        statusBar()->showMessage("No unread turn messages", 1800);
    });
}

void MainWindow::appendTurnMessages(const std::vector<GameEvent>& events)
{
    if (events.empty()) return;
    if (!turnMessagesDock_) installTurnMessages();

    std::size_t added = 0;
    for (const auto& event : events) {
        if (event.recipient != 1) continue;
        const auto duplicate = std::any_of(turnMessages_.begin(), turnMessages_.end(), [&](const GameEvent& existing) {
            return existing.id == event.id;
        });
        if (duplicate) continue;

        turnMessages_.push_back(event);
        auto* item = new QListWidgetItem(event_text(state_, event), turnMessagesList_);
        item->setData(kEventIdRole, QVariant::fromValue<qulonglong>(event.id));
        item->setData(kStarIdRole, static_cast<quint32>(event.star));
        item->setData(kUnreadRole, true);
        auto font = item->font();
        font.setBold(true);
        item->setFont(font);
        ++added;
    }

    if (added == 0) return;
    turnMessagesSummary_->setText(QString("%1 new strategic report%2 on Turn %3")
        .arg(static_cast<qulonglong>(added))
        .arg(added == 1 ? "" : "s")
        .arg(static_cast<qulonglong>(state_.turn)));
    turnMessagesDock_->show();
    turnMessagesDock_->raise();
}

void MainWindow::resetTurnMessages()
{
    turnMessages_.clear();
    if (turnMessagesList_) turnMessagesList_->clear();
    if (turnMessagesSummary_) turnMessagesSummary_->setText("No new strategic reports.");
    if (turnMessagesDock_) turnMessagesDock_->hide();
}

} // namespace suns
