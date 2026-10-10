#include "main_window.hpp"
#include "suns/communications.hpp"

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
    Freight,
    EnemyContacts,
    Combat,
    Research,
    ProductionDelays,
    Warnings,
};

QString production_item_name(const GameState& state, const GameEvent& event)
{
    if (event.productionKind == ProductionKind::Terraforming) return "Terraforming";
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
    case GameEventKind::Bombardment: subject = QString("Bombardment of %1: %2 colonists, %3 installations lost").arg(planetName).arg(qulonglong(event.deliveredColonists)).arg(event.quantity); break;
    case GameEventKind::MineStrike: subject = QString("%1 hit a minefield — damage %2%, movement stopped").arg(fleetName).arg(qulonglong(event.deliveredColonists)); break;
    case GameEventKind::ScientificData: subject = QString("Field science: +%1 %2 research points").arg(event.quantity).arg(QString::fromStdString(research_field_name(event.researchField))); break;
    case GameEventKind::EmissionDetected: subject = QString("Unidentified emission — uncertainty %1 ly").arg(event.quantity); break;
    case GameEventKind::AnomalyDetected: subject = QString("Spatial anomaly %1 detected").arg(event.wormholeEndpoint); break;
    case GameEventKind::WormholeClassified: subject = QString("WH %1 classified").arg(event.wormholeEndpoint); break;
    case GameEventKind::WormholeEntered: subject = QString("%1 entered WH %2").arg(fleetName).arg(event.wormholeEndpoint); break;
    case GameEventKind::WormholeEmerged: subject = QString("%1 emerged at WH %2").arg(fleetName).arg(event.wormholeEndpoint); break;
    case GameEventKind::WormholeEntryMissed: subject = QString("%1: WH entry not found at rendezvous").arg(fleetName); break;
    case GameEventKind::WormholeOverdue: subject = QString("%1: OVERDUE / NO CONTACT").arg(fleetName); break;
    case GameEventKind::WormholePresumedLost: subject = QString("%1: PRESUMED LOST after WH transit").arg(fleetName); break;
    case GameEventKind::WormholeCollapsed: subject = QString("WH %1: collapse observed").arg(event.wormholeEndpoint); break;
    case GameEventKind::SpaceBattle: {
        std::uint64_t lost = 0;
        if (event.battle) for (const auto& unit : event.battle->units)
            if (unit.owner == event.recipient) lost += unit.initialShips - unit.survivingShips;
        subject = QString("Space battle at %1 — %2 own ships lost").arg(starName).arg(qulonglong(lost));
        break;
    }
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
    case GameEventKind::GroundInvasionWon:
        subject = QString("Ground invasion captured %1").arg(planetName);
        break;
    case GameEventKind::GroundInvasionLost:
        subject = QString("Ground invasion repelled on %1").arg(planetName);
        break;
    case GameEventKind::GroundDefenseWon:
        subject = QString("Ground defense held on %1").arg(planetName);
        break;
    case GameEventKind::ColonyLost:
        subject = QString("Colony lost on %1").arg(planetName);
        break;
    case GameEventKind::FreightDelivered:
        subject = QString("Cargo delivered to %1").arg(planetName);
        break;
    case GameEventKind::EnemyFleetDetected:
        subject = QString("Enemy fleet %1 detected").arg(event.fleet);
        break;
    case GameEventKind::EnemyFleetLost:
        subject = QString("Contact lost: enemy fleet %1").arg(event.fleet);
        break;
    case GameEventKind::FleetMobilityRestored:
        subject = QString("%1 can fly again after repairs").arg(fleetName);
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
            || event.kind == GameEventKind::PrecursorArtifactsDiscovered
            || event.kind == GameEventKind::AnomalyDetected
            || event.kind == GameEventKind::WormholeClassified
            || event.kind == GameEventKind::WormholeCollapsed;
    case MessageTypeFilter::Archaeology:
        return event.kind == GameEventKind::PrecursorArtifactsDiscovered;
    case MessageTypeFilter::FleetMovement:
        return event.kind == GameEventKind::FleetArrived
            || event.kind == GameEventKind::RouteCompleted
            || event.kind == GameEventKind::FleetTargetLost
            || event.kind == GameEventKind::FleetsMerged
            || event.kind == GameEventKind::FleetMobilityRestored
            || event.kind == GameEventKind::WormholeEntered
            || event.kind == GameEventKind::WormholeEmerged
            || event.kind == GameEventKind::WormholeEntryMissed
            || event.kind == GameEventKind::WormholeOverdue
            || event.kind == GameEventKind::WormholePresumedLost;
    case MessageTypeFilter::ShipConstruction:
        return event.kind == GameEventKind::ProductionCompleted
            && event.productionKind == ProductionKind::ColonyShip;
    case MessageTypeFilter::Infrastructure:
        return event.kind == GameEventKind::ProductionCompleted
            && event.productionKind != ProductionKind::ColonyShip;
    case MessageTypeFilter::Colonization: return event.kind == GameEventKind::ColonyFounded;
    case MessageTypeFilter::Freight: return event.kind == GameEventKind::FreightDelivered;
    case MessageTypeFilter::EnemyContacts:
        return event.kind == GameEventKind::EnemyFleetDetected
            || event.kind == GameEventKind::EnemyFleetLost || event.kind == GameEventKind::EmissionDetected;
    case MessageTypeFilter::Combat:
        return event.kind == GameEventKind::Bombardment || event.kind == GameEventKind::MineStrike
            || event.kind == GameEventKind::SpaceBattle || event.kind == GameEventKind::GroundInvasionWon
            || event.kind == GameEventKind::GroundInvasionLost
            || event.kind == GameEventKind::GroundDefenseWon
            || event.kind == GameEventKind::ColonyLost;
    case MessageTypeFilter::Research:
        return event.kind == GameEventKind::ScientificData || event.kind == GameEventKind::ResearchLevelCompleted
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

QString space_battle_text(const GameEvent& event)
{
    if (!event.battle) return "Battle report unavailable.";
    const auto& battle = *event.battle;
    std::uint64_t ownBefore = 0, ownAfter = 0, enemyAfter = 0;
    for (const auto& unit : battle.units) {
        if (unit.owner == event.recipient) { ownBefore += unit.initialShips; ownAfter += unit.survivingShips; }
        else enemyAfter += unit.survivingShips;
    }
    const QString outcome = battle.stalemate ? "Unresolved after round limit"
        : ownAfter ? "Victory" : enemyAfter ? "Defeat" : "Mutual destruction";
    QString text = QString("<b>Space battle — %1</b><br>Observation: turn %2. Report received: turn %3.<br>"
        "Position: %4, %5. Rounds: %6. Own ships lost: %7 of %8.<br><br>")
        .arg(outcome).arg(qulonglong(event.observedTurn)).arg(qulonglong(event.turn))
        .arg(battle.position.x, 0, 'f', 1).arg(battle.position.y, 0, 'f', 1).arg(battle.rounds)
        .arg(qulonglong(ownBefore - ownAfter)).arg(qulonglong(ownBefore));
    text += "<table cellspacing='4'><tr><th>Empire / Fleet / Design</th><th>Before</th><th>Lost</th><th>After</th><th>Armor left</th></tr>";
    for (const auto& unit : battle.units) text += QString("<tr><td>%1%2 / %3 / %4</td><td>%5</td><td>%6</td><td>%7</td><td>%8</td></tr>")
        .arg(unit.owner == event.recipient ? "Own " : "Empire ").arg(unit.owner)
        .arg(QString::fromStdString(unit.fleetName).toHtmlEscaped(), QString::fromStdString(unit.designName).toHtmlEscaped())
        .arg(unit.initialShips).arg(unit.initialShips - unit.survivingShips).arg(unit.survivingShips)
        .arg(unit.remainingArmor, 0, 'f', 1);
    text += "</table><br><b>Volley log</b><br>";
    text += "<table cellspacing='4'><tr><th>Round</th><th>Shot</th><th>Range</th><th>Hits</th><th>Shield / Armor damage</th><th>Lost</th></tr>";
    const auto shown = std::min(std::size_t(256), battle.shots.size());
    for (std::size_t i = 0; i < shown; ++i) {
        const auto& shot = battle.shots[i];
        if (shot.attacker >= battle.units.size() || shot.target >= battle.units.size()) continue;
        const auto& source = battle.units[shot.attacker];
        const auto& target = battle.units[shot.target];
        text += QString("<tr><td>%1</td><td>%2 (#%3) → %4 (#%5)<br>%6</td><td>%7</td><td>%8 / %9</td><td>%10 / %11</td><td>%12</td></tr>")
            .arg(shot.round).arg(QString::fromStdString(source.designName).toHtmlEscaped()).arg(source.fleet)
            .arg(QString::fromStdString(target.designName).toHtmlEscaped()).arg(target.fleet)
            .arg(QString::fromStdString(component_spec(shot.weapon).name).toHtmlEscaped())
            .arg(shot.range, 0, 'f', 1).arg(qulonglong(shot.hits)).arg(qulonglong(shot.fired))
            .arg(shot.shieldDamage, 0, 'f', 1).arg(shot.armorDamage, 0, 'f', 1).arg(shot.shipsDestroyed);
    }
    text += "</table>";
    if (shown < battle.shots.size() || battle.logTruncated)
        text += "<br>Volley display is limited; the fleet totals above include the entire battle.";
    text += "<br><br>Survivors interrupt their route. Armor damage persists and can be repaired; shields recharge for the next battle. "
        "All different empires are hostile in this first combat model. Orbital station combat and battle doctrines are pending.";
    return text;
}

QString event_text(const GameState& state, const GameEvent& event)
{
    if (event.kind == GameEventKind::SpaceBattle) return space_battle_text(event);
    const auto* star = find_star(state, event.star);
    const auto* planet = event.planet != 0 ? find_planet_at_star(state, event.star) : nullptr;
    const auto battlePlanetName = planet
        ? QString::fromStdString(planet->name)
        : QString("Planet %1").arg(event.planet);
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
    if (event.kind >= GameEventKind::Bombardment && event.kind <= GameEventKind::EmissionDetected) {
        text = event_subject(state,event) + QString("\nObserved turn %1; received turn %2.\nPosition: %3, %4.")
            .arg(qulonglong(event.observedTurn)).arg(qulonglong(event.turn))
            .arg(event.position.x,0,'f',1).arg(event.position.y,0,'f',1);
        if (event.kind == GameEventKind::ScientificData)
            text += "\nThese research points have been delivered to your empire. Each field objective is finite; wreckage must contain unfamiliar technology.";
        else if (event.kind == GameEventKind::EmissionDetected)
            text += "\nThis is an uncertain signal area. The source, owner and fleet composition are unverified; it may be a decoy. The marker expires after this planning year.";
        else if (event.kind == GameEventKind::MineStrike)
            text += "\nThe route was interrupted. A safe Warp or beam-equipped sweeping fleet can help with known fields.";
        else text += "\nReported population and installation losses are from this bombardment. Current colony conditions may have changed since the observation.";
        return text.toHtmlEscaped().replace("\n","<br>");
    }
    if (event.kind >= GameEventKind::AnomalyDetected && event.kind <= GameEventKind::WormholeCollapsed) {
        text = event_subject(state, event) + QString("\nObservation: turn %1. Report received: turn %2.\nPosition: %3, %4.\n")
            .arg(static_cast<qulonglong>(event.observedTurn)).arg(static_cast<qulonglong>(event.turn))
            .arg(event.position.x, 0, 'f', 1).arg(event.position.y, 0, 'f', 1);
        text += event.kind == GameEventKind::WormholePresumedLost || event.kind == GameEventKind::WormholeOverdue
            ? "No emergence report has arrived. This is an assessment from missing contact, not immediate proof of destruction."
            : "Natural wormholes drift and may collapse. Exit knowledge is partial; transit can destroy the entire fleet.";
        return text.toHtmlEscaped().replace("\n", "<br>");
    }
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
    } else if (event.kind == GameEventKind::FleetMobilityRestored) {
        text = QString("Turn %1  •  %2 is operational after hull repairs at %3. "
                       "The previous route was cleared at critical damage; issue a new order to depart.")
                   .arg(static_cast<qulonglong>(event.observedTurn))
                   .arg(fleetName, starName);
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
        if (event.productionKind == ProductionKind::Terraforming) itemName = "Terraforming";
        else if (event.productionKind == ProductionKind::Factory) itemName = "Factory";
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
    } else if (event.kind == GameEventKind::FreightDelivered) {
        text = QString("Turn %1  •  Cargo delivered to %2\n"
                       "Minerals: I %3 / B %4 / G %5 kt; colonists: %6")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(event_planet_name(state, event))
                   .arg(event.deliveredMinerals.ironium, 0, 'f', 2)
                   .arg(event.deliveredMinerals.boranium, 0, 'f', 2)
                   .arg(event.deliveredMinerals.germanium, 0, 'f', 2)
                   .arg(static_cast<qulonglong>(event.deliveredColonists));
    } else if (event.kind == GameEventKind::EnemyFleetDetected
        || event.kind == GameEventKind::EnemyFleetLost) {
        const auto location = event.star != 0 ? starName
            : QString("(%1, %2)").arg(event.position.x, 0, 'f', 0).arg(event.position.y, 0, 'f', 0);
        text = QString("Turn %1  •  %2: enemy fleet %3 (Empire %4)\n"
                       "Last observed at %5 in year %6. Its current position and composition are unknown.")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(event.kind == GameEventKind::EnemyFleetDetected ? "Contact detected" : "Contact lost")
                   .arg(event.fleet)
                   .arg(event.contactOwner)
                   .arg(location)
                   .arg(static_cast<qulonglong>(event.observedTurn));
    } else if (event.kind == GameEventKind::GroundInvasionWon) {
        text = QString("Turn %1  •  Ground invasion succeeded on %2\n"
                       "The colony was captured; %3 attacking colonists survived.")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(battlePlanetName)
                   .arg(event.quantity);
    } else if (event.kind == GameEventKind::GroundInvasionLost) {
        text = QString("Turn %1  •  Warning: ground invasion was repelled on %2\n"
                       "%3 defending colonists remain.")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(battlePlanetName)
                   .arg(event.quantity);
    } else if (event.kind == GameEventKind::GroundDefenseWon) {
        text = QString("Turn %1  •  Ground defenses held on %2\n"
                       "%3 defending colonists remain.")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(battlePlanetName)
                   .arg(event.quantity);
    } else if (event.kind == GameEventKind::ColonyLost) {
        text = QString("Turn %1  •  Critical: %2 is no longer your colony\n"
                       "%3 enemy colonists survived; zero may indicate bombardment emptied the colony.")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(battlePlanetName)
                   .arg(event.quantity);
    } else if (event.kind == GameEventKind::ResearchLevelCompleted) {
        text = QString("Turn %1  •  Research completed: %2 %3")
                   .arg(static_cast<qulonglong>(event.turn))
                   .arg(QString::fromStdString(research_field_name(event.researchField)))
                   .arg(event.technologyLevel);
        QStringList technologies;
        for (const auto& unlock : research_unlocks()) {
            if (unlock.legacyPropulsion && !player_uses_legacy_propulsion(state, event.recipient)) continue;
            const bool threshold = (unlock.field == event.researchField && unlock.level == event.technologyLevel)
                || unlock.extraLevels[static_cast<std::size_t>(event.researchField)] == event.technologyLevel;
            if (threshold && research_unlock_available(state, event.recipient, unlock))
                technologies << QString::fromStdString(unlock.name);
        }
        if (!technologies.isEmpty()) text += "\nAvailable technologies: " + technologies.join(", ");
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
        if (event.productionKind == ProductionKind::Terraforming) itemName = "Terraforming";
        else if (event.productionKind == ProductionKind::Factory) itemName = "Factory";
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
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(4);

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
    turnMessageTypeFilter_->addItem("Cargo deliveries", static_cast<int>(MessageTypeFilter::Freight));
    turnMessageTypeFilter_->addItem("Enemy contacts", static_cast<int>(MessageTypeFilter::EnemyContacts));
    turnMessageTypeFilter_->addItem("Combat", static_cast<int>(MessageTypeFilter::Combat));
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
    auto* previousUnread = new QPushButton("Previous unread", content);
    previousUnread->setObjectName("previousUnreadTurnMessage");
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
    auto* navigationRow = new QHBoxLayout;
    navigationRow->addWidget(previousUnread);
    navigationRow->addWidget(nextUnread);
    navigationRow->addStretch(1);
    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(turnMessageShowOnMapButton_);
    buttonRow->addWidget(turnMessageHideSimilarButton_);
    buttonRow->addWidget(turnMessageRestoreHiddenButton_);
    buttonRow->addStretch(1);
    buttonRow->addWidget(clearMessages);
    layout->addWidget(turnMessagesSummary_);
    layout->addLayout(filterRow);
    layout->addWidget(reader, 1);
    layout->addLayout(navigationRow);
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
        if (!event || (event->star == 0 && event->fleet == 0)) return;
        const bool battle = event->kind == GameEventKind::SpaceBattle;
        const auto position = event->position;
        const auto starId = event->star;
        const auto* star = find_star(state_, starId);
        const auto fleet = std::find_if(state_.fleets.begin(), state_.fleets.end(), [&](const Fleet& candidate) {
            return candidate.id == event->fleet;
        });
        const bool ownedFleet = fleet != state_.fleets.end() && fleet->owner == pendingOrders_.player;
        // Preserve the report's system as the star context even if its fleet
        // has travelled since the event was recorded.
        if (star && ownedFleet) selection_.star = starId;
        if (ownedFleet) {
            selectWorkspaceObject(2, fleet->id);
        } else if (star) {
            selectWorkspaceObject(1, starId);
        }
        rebuildScene();
        if (battle) {
            view_->centerOn(position.x, position.y);
        } else if (ownedFleet) {
            const auto visible = fleet_player_view(state_, *fleet);
            view_->centerOn(visible.position.x, visible.position.y);
        } else if (star) view_->centerOn(star->position.x, star->position.y);
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
        updateTurnMessagesSummary();
        if (event->kind == GameEventKind::SpaceBattle) turnMessageBody_->setHtml(event_text(state_, *event));
        else turnMessageBody_->setPlainText(event_text(state_, *event));
        turnMessageShowOnMapButton_->setEnabled(event->star != 0 || event->fleet != 0);
        turnMessageHideSimilarButton_->setEnabled(true);
    };
    connect(turnMessagesList_, &QListWidget::currentItemChanged, this,
        [read](QListWidgetItem* current, QListWidgetItem*) { read(current); });
    connect(turnMessagesList_, &QListWidget::itemClicked, this,
        [showOnMap](QListWidgetItem*) { showOnMap(); });
    connect(turnMessageShowOnMapButton_, &QPushButton::clicked, this, showOnMap);
    const auto navigateUnread = [this](int direction) {
        if (!turnMessagesList_ || turnMessagesList_->count() == 0) return;
        const int count = turnMessagesList_->count();
        int start = turnMessagesList_->currentRow();
        if (start < 0) start = direction > 0 ? -1 : 0;
        for (int offset = 1; offset <= count; ++offset) {
            const int row = (start + direction * offset + count) % count;
            auto* item = turnMessagesList_->item(row);
            if (!item->data(kUnreadRole).toBool()) continue;
            turnMessagesList_->setCurrentItem(item);
            turnMessagesList_->scrollToItem(item);
            return;
        }
        statusBar()->showMessage("No unread turn messages", 1800);
    };
    connect(nextUnread, &QPushButton::clicked, this,
        [navigateUnread] { navigateUnread(1); });
    connect(previousUnread, &QPushButton::clicked, this,
        [navigateUnread] { navigateUnread(-1); });
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
        if (event.recipient != pendingOrders_.player) continue;
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

    hiddenTurnMessagesCount_ = hidden;
    updateTurnMessagesSummary();
    if (turnMessageRestoreHiddenButton_) {
        turnMessageRestoreHiddenButton_->setEnabled(!hiddenTurnMessageClasses_.empty());
        turnMessageRestoreHiddenButton_->setText(hiddenTurnMessageClasses_.empty()
            ? "Restore hidden"
            : QString("Restore hidden (%1 types)")
                  .arg(static_cast<qulonglong>(hiddenTurnMessageClasses_.size())));
    }
}

void MainWindow::updateTurnMessagesSummary()
{
    if (!turnMessagesList_ || !turnMessagesSummary_) return;
    std::size_t unread = 0;
    for (int row = 0; row < turnMessagesList_->count(); ++row) {
        if (turnMessagesList_->item(row)->data(kUnreadRole).toBool()) ++unread;
    }
    turnMessagesSummary_->setText(QString("%1 shown of %2 reports • %3 unread%4")
        .arg(turnMessagesList_->count())
        .arg(static_cast<qulonglong>(turnMessages_.size()))
        .arg(static_cast<qulonglong>(unread))
        .arg(hiddenTurnMessagesCount_ > 0
            ? QString(" • %1 hidden by Hide").arg(static_cast<qulonglong>(hiddenTurnMessagesCount_))
            : QString{}));
}

void MainWindow::resetTurnMessages()
{
    turnMessages_.clear();
    readTurnMessageIds_.clear();
    refreshTurnMessages();
    if (turnMessagesDock_) turnMessagesDock_->hide();
}

} // namespace suns
