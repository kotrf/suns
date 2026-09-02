#include "main_window.hpp"

#include <QAction>
#include <QColor>
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
constexpr int kFleetIdRole = Qt::UserRole + 4;
constexpr int kPositionXRole = Qt::UserRole + 5;
constexpr int kPositionYRole = Qt::UserRole + 6;

QString event_text(const GameState& state, const GameEvent& event)
{
    const auto* star = find_star(state, event.star);
    const auto* planet = event.planet != 0 ? find_planet_at_star(state, event.star) : nullptr;
    const auto starName = star
        ? QString::fromStdString(star->name)
        : QString("deep space (%1, %2)").arg(event.position.x, 0, 'f', 0).arg(event.position.y, 0, 'f', 0);

    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == event.fleet;
    });
    const auto fleetName = fleet != state.fleets.end()
        ? QString::fromStdString(fleet->name)
        : QString("Fleet %1").arg(event.fleet);

    QString text;
    if (event.kind == GameEventKind::SystemSurveyed) {
        QString detail;
        if (event.surveyLevel == SurveyLevel::SystemScan) {
            detail = "Ordinary scanner contact — planetary parameters require orbit or a penetrating scanner";
        } else if (planet) {
            detail = QString("%1 — habitability %2%3")
                         .arg(QString::fromStdString(planet->name))
                         .arg(event.quantity)
                         .arg(event.surveyLevel == SurveyLevel::BasicScan ? " estimated" : " confirmed");
            if (event.surveyLevel >= SurveyLevel::GeologicalSurvey) {
                const auto concentrations = planet_mineral_concentration(state, *planet);
                detail += QString(", deposits I %1 / B %2 / G %3")
                              .arg(concentrations.ironium, 0, 'f', 1)
                              .arg(concentrations.boranium, 0, 'f', 1)
                              .arg(concentrations.germanium, 0, 'f', 1);
            }
        }
        if (star && star_is_variable(*star) && event.surveyLevel >= SurveyLevel::OrbitalSurvey) {
            detail += event.surveyLevel >= SurveyLevel::GeologicalSurvey
                ? QString("\nVariable star characterized — period %1 years, luminosity amplitude ±%2%")
                      .arg(star->variability.periodTurns)
                      .arg(star->variability.amplitudePercent)
                : QString("\nVariable star detected — remain in orbit to characterize its cycle");
        }
        QString surveyName = event.surveyLevel == SurveyLevel::SystemScan
            ? "Long-range system scan"
            : "Penetrating scan";
        if (event.surveyLevel == SurveyLevel::OrbitalSurvey) surveyName = "Orbital survey";
        else if (event.surveyLevel >= SurveyLevel::GeologicalSurvey) surveyName = "Geological survey";
        text = QString("Turn %1  •  %2: %3")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(surveyName, starName);
        if (!detail.isEmpty()) text += QString("\n%1").arg(detail);
    } else if (event.kind == GameEventKind::FleetArrived) {
        text = QString("Turn %1  •  %2 arrived at %3 and continues its route")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(fleetName, starName);
    } else if (event.kind == GameEventKind::RouteCompleted) {
        text = QString("Turn %1  •  %2 completed its route at %3")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(fleetName, starName);
    } else if (event.kind == GameEventKind::FleetStalledForFuel) {
        text = QString("Turn %1  •  Warning: %2 cannot continue — insufficient fuel")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(fleetName);
    } else if (event.kind == GameEventKind::FleetTargetLost) {
        text = QString("Turn %1  •  Warning: %2 lost moving target Fleet %3; route cleared")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(fleetName)
                   .arg(event.quantity);
    } else if (event.kind == GameEventKind::FleetsMerged) {
        text = QString("Turn %1  •  Fleet %2 rendezvoused with %3 and merged into it")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(event.quantity)
                   .arg(fleetName);
    } else if (event.kind == GameEventKind::ProductionCompleted) {
        const auto planetName = planet
            ? QString::fromStdString(planet->name)
            : QString("Colony %1").arg(event.planet);
        QString itemName;
        if (event.productionKind == ProductionKind::Factory) itemName = "Factory";
        else if (event.productionKind == ProductionKind::Mine) itemName = "Mine";
        else if (event.productionKind == ProductionKind::OrbitalStation) itemName = "Orbital Dock";
        else if (const auto* design = find_ship_design(state, event.shipDesign)) {
            itemName = QString::fromStdString(design->name);
        } else {
            itemName = "Ship";
        }
        text = QString("Turn %1  •  %2 completed on %3")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(itemName, planetName);
    } else if (event.kind == GameEventKind::ColonyFounded) {
        const auto planetName = planet
            ? QString::fromStdString(planet->name)
            : QString("Planet %1").arg(event.planet);
        text = QString("Turn %1  •  New colony founded on %2")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(planetName);
    } else if (event.kind == GameEventKind::ResearchLevelCompleted) {
        text = QString("Turn %1  •  Research completed: %2 %3")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(QString::fromStdString(research_field_name(event.researchField)))
                   .arg(event.technologyLevel);
        if (event.researchField == ResearchField::Energy && event.technologyLevel == 1) {
            text += "\nUnlocked: Antimatter Generator";
        } else if (event.researchField == ResearchField::Propulsion && event.technologyLevel == 1) {
            text += "\nUnlocked: Advanced Fusion Drive";
        } else if (event.researchField == ResearchField::Electronics && event.technologyLevel == 1) {
            text += "\nUnlocked: Compact Long Range Scanner";
        } else if (event.researchField == ResearchField::Electronics && event.technologyLevel == 2) {
            text += "\nUnlocked: Extended Range Scanner";
        } else if (event.researchField == ResearchField::Electronics && event.technologyLevel == 3) {
            text += "\nUnlocked: Penetrating Scanner";
        } else if (event.researchField == ResearchField::Construction && event.technologyLevel == 1) {
            text += "\nUnlocked: Remote Mining Module";
        }
    } else {
        const auto planetName = planet
            ? QString::fromStdString(planet->name)
            : QString("Colony %1").arg(event.planet);
        QString itemName;
        if (event.productionKind == ProductionKind::Factory) itemName = "Factory";
        else if (event.productionKind == ProductionKind::Mine) itemName = "Mine";
        else if (event.productionKind == ProductionKind::OrbitalStation) itemName = "Orbital Dock";
        else if (const auto* design = find_ship_design(state, event.shipDesign)) {
            itemName = QString::fromStdString(design->name);
        } else {
            itemName = "Ship";
        }
        const auto reason = event.kind == GameEventKind::ProductionWaitingForShipyard
            ? "requires an orbital shipyard"
            : "is waiting for minerals";
        text = QString("Turn %1  •  Warning: %2 on %3 %4")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(itemName, planetName, reason);
    }

    const auto delay = event.turn > event.observedTurn ? event.turn - event.observedTurn : 0;
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
        const auto fleetId = static_cast<FleetId>(item->data(kFleetIdRole).toUInt());
        const auto fleet = std::find_if(state_.fleets.begin(), state_.fleets.end(), [&](const Fleet& candidate) {
            return candidate.id == fleetId;
        });
        if (star) selectedStarId_ = starId;
        if (fleet != state_.fleets.end()) selectedFleetId_ = fleetId;
        else selectedFleetId_.reset();
        rebuildScene();
        if (star) view_->centerOn(star->position.x, star->position.y);
        else view_->centerOn(item->data(kPositionXRole).toDouble(), item->data(kPositionYRole).toDouble());
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
        item->setData(kFleetIdRole, static_cast<quint32>(event.fleet));
        item->setData(kPositionXRole, event.position.x);
        item->setData(kPositionYRole, event.position.y);
        item->setData(kUnreadRole, true);
        if (event.severity == GameEventSeverity::Warning) item->setForeground(QColor(210, 135, 35));
        else if (event.severity == GameEventSeverity::Critical) item->setForeground(QColor(210, 65, 65));
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
