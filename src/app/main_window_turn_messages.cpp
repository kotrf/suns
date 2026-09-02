#include "main_window.hpp"

#include <QAction>
#include <QColor>
#include <QDockWidget>
#include <QFont>
#include <QGraphicsView>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTextBrowser>
#include <QVariant>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <set>

namespace suns {

namespace {

constexpr int kEventIdRole = Qt::UserRole + 1;
constexpr int kStarIdRole = Qt::UserRole + 2;
constexpr int kUnreadRole = Qt::UserRole + 3;
constexpr int kFleetIdRole = Qt::UserRole + 4;
constexpr int kPositionXRole = Qt::UserRole + 5;
constexpr int kPositionYRole = Qt::UserRole + 6;
constexpr int kMessageClassRole = Qt::UserRole + 7;

enum class MessageTypeFilter {
    All,
    Exploration,
    Archaeology,
    FleetMovement,
    ShipConstruction,
    Infrastructure,
    Colonization,
    Research,
    ProductionDelays,
    Warnings,
};

QString production_item_name(const GameState& state, const GameEvent& event)
{
    if (event.productionKind == ProductionKind::Factory) return "Factory";
    if (event.productionKind == ProductionKind::Mine) return "Mine";
    if (event.productionKind == ProductionKind::OrbitalStation) return "Orbital Dock";
    if (const auto* design = find_ship_design(state, event.shipDesign)) {
        return QString::fromStdString(design->name);
    }
    return "Ship";
}

QString event_planet_name(const GameState& state, const GameEvent& event)
{
    const auto planet = event.planet != 0
        ? std::find_if(state.planets.begin(), state.planets.end(), [&](const Planet& candidate) {
              return candidate.id == event.planet;
          })
        : state.planets.end();
    if (planet != state.planets.end()) return QString::fromStdString(planet->name);
    if (const auto* atStar = find_planet_at_star(state, event.star)) {
        return QString::fromStdString(atStar->name);
    }
    return event.planet != 0 ? QString("Planet %1").arg(event.planet) : QString{};
}

PlanetId event_planet_id(const GameState& state, const GameEvent& event)
{
    if (event.planet != 0) return event.planet;
    if (const auto* planet = find_planet_at_star(state, event.star)) return planet->id;
    return 0;
}

QString event_subject(const GameState& state, const GameEvent& event)
{
    const auto* star = find_star(state, event.star);
    const auto starName = star ? QString::fromStdString(star->name) : QString("deep space");
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& candidate) {
        return candidate.id == event.fleet;
    });
    const auto fleetName = fleet != state.fleets.end()
        ? QString::fromStdString(fleet->name)
        : QString("Fleet %1").arg(event.fleet);
    const auto planetName = event_planet_name(state, event);

    QString subject;
    switch (event.kind) {
    case GameEventKind::SystemSurveyed: subject = QString("Survey report: %1").arg(starName); break;
    case GameEventKind::FleetArrived: subject = QString("%1 arrived at %2").arg(fleetName, starName); break;
    case GameEventKind::RouteCompleted: subject = QString("%1 completed its route").arg(fleetName); break;
    case GameEventKind::FleetStalledForFuel: subject = QString("%1 stalled: insufficient fuel").arg(fleetName); break;
    case GameEventKind::ProductionCompleted:
        subject = QString("%1 completed on %2").arg(production_item_name(state, event), planetName);
        break;
    case GameEventKind::ColonyFounded: subject = QString("New colony on %1").arg(planetName); break;
    case GameEventKind::ProductionWaitingForMinerals:
        subject = QString("%1 waits for minerals on %2").arg(production_item_name(state, event), planetName);
        break;
    case GameEventKind::ResearchLevelCompleted:
        subject = QString("Research: %1 %2")
                      .arg(QString::fromStdString(research_field_name(event.researchField)))
                      .arg(static_cast<int>(event.technologyLevel));
        break;
    case GameEventKind::FleetTargetLost: subject = QString("%1 lost its fleet target").arg(fleetName); break;
    case GameEventKind::FleetsMerged: subject = QString("Fleets merged into %1").arg(fleetName); break;
    case GameEventKind::ProductionWaitingForShipyard:
        subject = QString("%1 waits for a shipyard on %2").arg(production_item_name(state, event), planetName);
        break;
    case GameEventKind::PrecursorArtifactsDiscovered:
        subject = QString("Precursor artifacts found on %1").arg(planetName);
        break;
    }
    return QString("T%1  %2").arg(static_cast<qulonglong>(event.turn)).arg(subject);
}

QString message_class(const GameEvent& event)
{
    auto key = QString::number(static_cast<int>(event.kind));
    if (event.kind == GameEventKind::SystemSurveyed) {
        key += QString(":survey-%1").arg(static_cast<int>(event.surveyLevel));
    } else if (event.kind == GameEventKind::ProductionCompleted
        || event.kind == GameEventKind::ProductionWaitingForMinerals
        || event.kind == GameEventKind::ProductionWaitingForShipyard) {
        const bool ship = event.productionKind == ProductionKind::ColonyShip;
        key += ship ? ":ship" : QString(":infrastructure-%1").arg(static_cast<int>(event.productionKind));
    }
    return key;
}

bool matches_type(const GameEvent& event, MessageTypeFilter filter)
{
    switch (filter) {
    case MessageTypeFilter::All: return true;
    case MessageTypeFilter::Exploration:
        return event.kind == GameEventKind::SystemSurveyed
            || event.kind == GameEventKind::PrecursorArtifactsDiscovered;
    case MessageTypeFilter::Archaeology:
        return event.kind == GameEventKind::PrecursorArtifactsDiscovered;
    case MessageTypeFilter::FleetMovement:
        return event.kind == GameEventKind::FleetArrived
            || event.kind == GameEventKind::RouteCompleted
            || event.kind == GameEventKind::FleetTargetLost
            || event.kind == GameEventKind::FleetsMerged;
    case MessageTypeFilter::ShipConstruction:
        return event.kind == GameEventKind::ProductionCompleted
            && event.productionKind == ProductionKind::ColonyShip;
    case MessageTypeFilter::Infrastructure:
        return event.kind == GameEventKind::ProductionCompleted
            && event.productionKind != ProductionKind::ColonyShip;
    case MessageTypeFilter::Colonization: return event.kind == GameEventKind::ColonyFounded;
    case MessageTypeFilter::Research:
        return event.kind == GameEventKind::ResearchLevelCompleted
            || event.kind == GameEventKind::PrecursorArtifactsDiscovered;
    case MessageTypeFilter::ProductionDelays:
        return event.kind == GameEventKind::ProductionWaitingForMinerals
            || event.kind == GameEventKind::ProductionWaitingForShipyard;
    case MessageTypeFilter::Warnings: return event.severity != GameEventSeverity::Information;
    }
    return true;
}

const GameEvent* find_event(const std::vector<GameEvent>& events, std::uint64_t id)
{
    const auto event = std::find_if(events.begin(), events.end(), [id](const GameEvent& candidate) {
        return candidate.id == id;
    });
    return event == events.end() ? nullptr : &*event;
}

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
            if (event.surveyLevel >= SurveyLevel::DeepSurvey) {
                detail += event.precursorArtifactHint
                    ? "\nPossible artificial structures detected — exact nature unknown"
                    : "\nDeep survey complete — no unexamined unusual sites detected";
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
        else if (event.surveyLevel == SurveyLevel::GeologicalSurvey) surveyName = "Geological survey";
        else if (event.surveyLevel >= SurveyLevel::DeepSurvey) surveyName = "Deep survey";
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
    } else if (event.kind == GameEventKind::PrecursorArtifactsDiscovered) {
        const auto planetName = planet
            ? QString::fromStdString(planet->name)
            : QString("Planet %1").arg(event.planet);
        text = QString("Turn %1  •  Archaeological discovery on %2\n"
                       "A precursor site yielded %3 RP to the current %4 research plan. "
                       "The site is now exhausted and remains in the planet's history.")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(planetName)
                   .arg(event.quantity)
                   .arg(QString::fromStdString(research_field_name(event.researchField)));
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

    turnMessagesSummary_ = new QLabel("No strategic reports.", content);
    turnMessagesSummary_->setObjectName("turnMessagesSummary");

    turnMessageAgeFilter_ = new QComboBox(content);
    turnMessageAgeFilter_->setObjectName("turnMessageAgeFilter");
    turnMessageAgeFilter_->addItem("All turns", 0);
    turnMessageAgeFilter_->addItem("This turn", 1);
    turnMessageAgeFilter_->addItem("Last 5 turns", 5);
    turnMessageAgeFilter_->addItem("Last 10 turns", 10);
    turnMessageAgeFilter_->addItem("Last 25 turns", 25);
    turnMessageAgeFilter_->setCurrentIndex(turnMessageAgeFilter_->findData(10));
    turnMessageAgeFilter_->setToolTip("Limit reports by their game turn");

    turnMessageTypeFilter_ = new QComboBox(content);
    turnMessageTypeFilter_->setObjectName("turnMessageTypeFilter");
    turnMessageTypeFilter_->addItem("All subjects", static_cast<int>(MessageTypeFilter::All));
    turnMessageTypeFilter_->addItem("Exploration reports", static_cast<int>(MessageTypeFilter::Exploration));
    turnMessageTypeFilter_->addItem("Archaeological discoveries", static_cast<int>(MessageTypeFilter::Archaeology));
    turnMessageTypeFilter_->addItem("Fleet movement", static_cast<int>(MessageTypeFilter::FleetMovement));
    turnMessageTypeFilter_->addItem("Ships completed", static_cast<int>(MessageTypeFilter::ShipConstruction));
    turnMessageTypeFilter_->addItem("Infrastructure completed", static_cast<int>(MessageTypeFilter::Infrastructure));
    turnMessageTypeFilter_->addItem("New colonies", static_cast<int>(MessageTypeFilter::Colonization));
    turnMessageTypeFilter_->addItem("Research completed", static_cast<int>(MessageTypeFilter::Research));
    turnMessageTypeFilter_->addItem("Production delays", static_cast<int>(MessageTypeFilter::ProductionDelays));
    turnMessageTypeFilter_->addItem("Warnings", static_cast<int>(MessageTypeFilter::Warnings));
    turnMessageTypeFilter_->setToolTip("Show only one semantic subject category");

    turnMessagePlanetFilter_ = new QComboBox(content);
    turnMessagePlanetFilter_->setObjectName("turnMessagePlanetFilter");
    turnMessagePlanetFilter_->addItem("All planets", static_cast<quint32>(0));
    turnMessagePlanetFilter_->setToolTip("Show reports associated with one planet");

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(turnMessageAgeFilter_);
    filterRow->addWidget(turnMessageTypeFilter_);
    filterRow->addWidget(turnMessagePlanetFilter_);

    turnMessagesList_ = new QListWidget(content);
    turnMessagesList_->setObjectName("turnMessagesList");
    turnMessagesList_->setWordWrap(false);
    turnMessagesList_->setAlternatingRowColors(true);
    turnMessagesList_->setMinimumWidth(290);

    turnMessageBody_ = new QTextBrowser(content);
    turnMessageBody_->setObjectName("turnMessageBody");
    turnMessageBody_->setPlaceholderText("Select a report to read it.");

    auto* reader = new QSplitter(Qt::Horizontal, content);
    reader->setObjectName("turnMessageReader");
    reader->addWidget(turnMessagesList_);
    reader->addWidget(turnMessageBody_);
    reader->setStretchFactor(0, 0);
    reader->setStretchFactor(1, 1);
    reader->setSizes({330, 680});

    auto* nextUnread = new QPushButton("Next unread", content);
    nextUnread->setObjectName("nextUnreadTurnMessage");
    turnMessageShowOnMapButton_ = new QPushButton("Show on map", content);
    turnMessageShowOnMapButton_->setObjectName("showTurnMessageOnMap");
    turnMessageHideSimilarButton_ = new QPushButton("Hide similar", content);
    turnMessageHideSimilarButton_->setObjectName("hideSimilarTurnMessages");
    turnMessageHideSimilarButton_->setToolTip(
        "Hide this semantic event type, while leaving other subjects visible");
    turnMessageRestoreHiddenButton_ = new QPushButton("Restore hidden", content);
    turnMessageRestoreHiddenButton_->setObjectName("restoreHiddenTurnMessages");
    auto* clearMessages = new QPushButton("Clear messages", content);
    clearMessages->setObjectName("clearTurnMessages");
    clearMessages->setToolTip("Remove all reports currently shown in this panel");
    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(nextUnread);
    buttonRow->addWidget(turnMessageShowOnMapButton_);
    buttonRow->addWidget(turnMessageHideSimilarButton_);
    buttonRow->addWidget(turnMessageRestoreHiddenButton_);
    buttonRow->addStretch(1);
    buttonRow->addWidget(clearMessages);
    layout->addWidget(turnMessagesSummary_);
    layout->addLayout(filterRow);
    layout->addWidget(reader, 1);
    layout->addLayout(buttonRow);
    turnMessagesDock_->setWidget(content);
    addDockWidget(Qt::BottomDockWidgetArea, turnMessagesDock_);
    turnMessagesDock_->hide();

    QMenu* view = menuBar()->findChild<QMenu*>("sunsViewMenu");
    if (!view) {
        view = menuBar()->addMenu("&View");
        view->setObjectName("sunsViewMenu");
    }
    view->addAction(turnMessagesDock_->toggleViewAction());

    QSettings settings("SunsProject", "Suns");
    for (const auto& key : settings.value("messages/hiddenClasses").toStringList()) {
        hiddenTurnMessageClasses_.insert(key);
    }

    const auto selectedEvent = [this]() -> const GameEvent* {
        const auto* item = turnMessagesList_ ? turnMessagesList_->currentItem() : nullptr;
        return item ? find_event(turnMessages_, item->data(kEventIdRole).toULongLong()) : nullptr;
    };

    const auto showOnMap = [this, selectedEvent] {
        const auto* event = selectedEvent();
        if (!event) return;
        const auto starId = event->star;
        const auto* star = find_star(state_, starId);
        const auto fleet = std::find_if(state_.fleets.begin(), state_.fleets.end(), [&](const Fleet& candidate) {
            return candidate.id == event->fleet;
        });
        if (star) selectedStarId_ = starId;
        if (fleet != state_.fleets.end()) selectedFleetId_ = fleet->id;
        rebuildScene();
        if (star) view_->centerOn(star->position.x, star->position.y);
        else view_->centerOn(event->position.x, event->position.y);
    };

    const auto read = [this](QListWidgetItem* item) {
        if (!item) return;
        const auto id = item->data(kEventIdRole).toULongLong();
        const auto* event = find_event(turnMessages_, id);
        if (!event) return;
        readTurnMessageIds_.insert(id);
        item->setData(kUnreadRole, false);
        auto font = item->font();
        font.setBold(false);
        item->setFont(font);
        turnMessageBody_->setPlainText(event_text(state_, *event));
        turnMessageShowOnMapButton_->setEnabled(event->star != 0 || event->fleet != 0);
        turnMessageHideSimilarButton_->setEnabled(true);
    };
    connect(turnMessagesList_, &QListWidget::currentItemChanged, this,
        [read](QListWidgetItem* current, QListWidgetItem*) { read(current); });
    connect(turnMessagesList_, &QListWidget::itemDoubleClicked, this,
        [showOnMap](QListWidgetItem*) { showOnMap(); });
    connect(turnMessageShowOnMapButton_, &QPushButton::clicked, this, showOnMap);
    connect(nextUnread, &QPushButton::clicked, this, [this, read] {
        if (!turnMessagesList_) return;
        for (int row = 0; row < turnMessagesList_->count(); ++row) {
            auto* item = turnMessagesList_->item(row);
            if (!item->data(kUnreadRole).toBool()) continue;
            turnMessagesList_->setCurrentItem(item);
            turnMessagesList_->scrollToItem(item);
            read(item);
            return;
        }
        statusBar()->showMessage("No unread turn messages", 1800);
    });
    connect(turnMessageHideSimilarButton_, &QPushButton::clicked, this, [this, selectedEvent] {
        const auto* event = selectedEvent();
        if (!event) return;
        hiddenTurnMessageClasses_.insert(message_class(*event));
        QStringList stored;
        for (const auto& key : hiddenTurnMessageClasses_) stored.push_back(key);
        QSettings("SunsProject", "Suns").setValue("messages/hiddenClasses", stored);
        refreshTurnMessages();
    });
    connect(turnMessageRestoreHiddenButton_, &QPushButton::clicked, this, [this] {
        hiddenTurnMessageClasses_.clear();
        QSettings("SunsProject", "Suns").remove("messages/hiddenClasses");
        refreshTurnMessages();
    });
    connect(turnMessageAgeFilter_, &QComboBox::currentIndexChanged, this,
        [this](int) { refreshTurnMessages(); });
    connect(turnMessageTypeFilter_, &QComboBox::currentIndexChanged, this,
        [this](int) { refreshTurnMessages(); });
    connect(turnMessagePlanetFilter_, &QComboBox::currentIndexChanged, this,
        [this](int) { refreshTurnMessages(); });
    connect(clearMessages, &QPushButton::clicked, this, [this] {
        turnMessages_.clear();
        readTurnMessageIds_.clear();
        refreshTurnMessages();
        statusBar()->showMessage("Turn messages cleared", 1800);
    });

    refreshTurnMessages();
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
        ++added;
    }

    if (added == 0) return;
    refreshTurnMessages();
    turnMessagesDock_->show();
    turnMessagesDock_->raise();
}

void MainWindow::refreshTurnMessages()
{
    if (!turnMessagesList_ || !turnMessagesSummary_) return;
    const auto selectedId = turnMessagesList_->currentItem()
        ? turnMessagesList_->currentItem()->data(kEventIdRole).toULongLong()
        : 0;

    const auto selectedPlanet = turnMessagePlanetFilter_
        ? static_cast<PlanetId>(turnMessagePlanetFilter_->currentData().toUInt())
        : 0;
    if (turnMessagePlanetFilter_) {
        std::set<PlanetId> planets;
        for (const auto& event : turnMessages_) {
            if (const auto planet = event_planet_id(state_, event); planet != 0) planets.insert(planet);
        }
        const QSignalBlocker blocker(turnMessagePlanetFilter_);
        turnMessagePlanetFilter_->clear();
        turnMessagePlanetFilter_->addItem("All planets", static_cast<quint32>(0));
        for (const auto planetId : planets) {
            const auto planet = std::find_if(state_.planets.begin(), state_.planets.end(), [&](const Planet& candidate) {
                return candidate.id == planetId;
            });
            const auto name = planet != state_.planets.end()
                ? QString::fromStdString(planet->name)
                : QString("Planet %1").arg(planetId);
            turnMessagePlanetFilter_->addItem(name, static_cast<quint32>(planetId));
        }
        const auto index = turnMessagePlanetFilter_->findData(static_cast<quint32>(selectedPlanet));
        turnMessagePlanetFilter_->setCurrentIndex(index >= 0 ? index : 0);
    }

    const auto age = turnMessageAgeFilter_ ? turnMessageAgeFilter_->currentData().toUInt() : 0;
    const auto type = turnMessageTypeFilter_
        ? static_cast<MessageTypeFilter>(turnMessageTypeFilter_->currentData().toInt())
        : MessageTypeFilter::All;
    const auto planetFilter = turnMessagePlanetFilter_
        ? static_cast<PlanetId>(turnMessagePlanetFilter_->currentData().toUInt())
        : 0;

    turnMessagesList_->clear();
    std::size_t hidden = 0;
    for (auto event = turnMessages_.rbegin(); event != turnMessages_.rend(); ++event) {
        if (age > 0 && state_.turn >= event->turn && state_.turn - event->turn >= age) continue;
        if (!matches_type(*event, type)) continue;
        if (planetFilter != 0 && event_planet_id(state_, *event) != planetFilter) continue;
        const auto eventClass = message_class(*event);
        if (hiddenTurnMessageClasses_.contains(eventClass)) {
            ++hidden;
            continue;
        }

        auto* item = new QListWidgetItem(event_subject(state_, *event), turnMessagesList_);
        item->setData(kEventIdRole, QVariant::fromValue<qulonglong>(event->id));
        item->setData(kStarIdRole, static_cast<quint32>(event->star));
        item->setData(kFleetIdRole, static_cast<quint32>(event->fleet));
        item->setData(kPositionXRole, event->position.x);
        item->setData(kPositionYRole, event->position.y);
        item->setData(kMessageClassRole, eventClass);
        const bool unread = !readTurnMessageIds_.contains(event->id);
        item->setData(kUnreadRole, unread);
        if (event->severity == GameEventSeverity::Warning) item->setForeground(QColor(210, 135, 35));
        else if (event->severity == GameEventSeverity::Critical) item->setForeground(QColor(210, 65, 65));
        auto font = item->font();
        font.setBold(unread);
        item->setFont(font);
    }

    auto* restore = static_cast<QListWidgetItem*>(nullptr);
    for (int row = 0; row < turnMessagesList_->count(); ++row) {
        auto* item = turnMessagesList_->item(row);
        if (item->data(kEventIdRole).toULongLong() == selectedId) restore = item;
    }
    if (!restore && turnMessagesList_->count() > 0) restore = turnMessagesList_->item(0);
    if (restore) turnMessagesList_->setCurrentItem(restore);
    else {
        if (turnMessageBody_) turnMessageBody_->clear();
        if (turnMessageShowOnMapButton_) turnMessageShowOnMapButton_->setEnabled(false);
        if (turnMessageHideSimilarButton_) turnMessageHideSimilarButton_->setEnabled(false);
    }

    std::size_t unread = 0;
    for (int row = 0; row < turnMessagesList_->count(); ++row) {
        if (turnMessagesList_->item(row)->data(kUnreadRole).toBool()) ++unread;
    }
    turnMessagesSummary_->setText(QString("%1 shown of %2 reports • %3 unread%4")
        .arg(turnMessagesList_->count())
        .arg(static_cast<qulonglong>(turnMessages_.size()))
        .arg(static_cast<qulonglong>(unread))
        .arg(hidden > 0 ? QString(" • %1 hidden by Hide").arg(static_cast<qulonglong>(hidden)) : QString{}));
    if (turnMessageRestoreHiddenButton_) {
        turnMessageRestoreHiddenButton_->setEnabled(!hiddenTurnMessageClasses_.empty());
        turnMessageRestoreHiddenButton_->setText(hiddenTurnMessageClasses_.empty()
            ? "Restore hidden"
            : QString("Restore hidden (%1 types)")
                  .arg(static_cast<qulonglong>(hiddenTurnMessageClasses_.size())));
    }
}

void MainWindow::resetTurnMessages()
{
    turnMessages_.clear();
    readTurnMessageIds_.clear();
    refreshTurnMessages();
    if (turnMessagesDock_) turnMessagesDock_->hide();
}

} // namespace suns
