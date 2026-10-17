#include "suns/player_knowledge.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/planetary_recovery.hpp"
#include "suns/unknown_objects.hpp"
#include "suns/interstellar_medium.hpp"

#include "suns/communications.hpp"
#include "suns/wormholes.hpp"
#include "suns/strategic_operations.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <tuple>

namespace suns {

namespace {

double distance_to_segment(Position point, Position start, Position end)
{
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.000000000001) return distance_between(point, start);

    const double projection = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared;
    const double t = std::clamp(projection, 0.0, 1.0);
    const Position closest{start.x + t * dx, start.y + t * dy};
    return distance_between(point, closest);
}

Player* mutable_player(GameState& state, PlayerId id)
{
    const auto it = std::find_if(state.players.begin(), state.players.end(), [id](const Player& player) {
        return player.id == id;
    });
    return it == state.players.end() ? nullptr : &*it;
}

std::uint64_t stable_event_id(const PendingSurveyReport& report, PlayerId recipient)
{
    // FNV-1a over the stable event payload. Replays therefore reproduce the
    // same IDs without persisting UI unread state in the simulation.
    std::uint64_t hash = 1469598103934665603ULL;
    auto mix = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (value >> (byte * 8)) & 0xffULL;
            hash *= 1099511628211ULL;
        }
    };
    mix(static_cast<std::uint64_t>(GameEventKind::SystemSurveyed));
    mix(recipient);
    mix(report.star);
    mix(report.sourceFleet);
    mix(report.observedTurn);
    mix(report.deliveryTurn);
    mix(static_cast<std::uint64_t>(report.level));
    return hash;
}

GameEventKind event_kind(PlayerReportKind kind)
{
    switch (kind) {
    case PlayerReportKind::FleetArrived: return GameEventKind::FleetArrived;
    case PlayerReportKind::RouteCompleted: return GameEventKind::RouteCompleted;
    case PlayerReportKind::FleetStalledForFuel: return GameEventKind::FleetStalledForFuel;
    case PlayerReportKind::ProductionCompleted: return GameEventKind::ProductionCompleted;
    case PlayerReportKind::ColonyFounded: return GameEventKind::ColonyFounded;
    case PlayerReportKind::ProductionWaitingForMinerals: return GameEventKind::ProductionWaitingForMinerals;
    case PlayerReportKind::FleetTargetLost: return GameEventKind::FleetTargetLost;
    case PlayerReportKind::FleetsMerged: return GameEventKind::FleetsMerged;
    case PlayerReportKind::ProductionWaitingForShipyard:
        return GameEventKind::ProductionWaitingForShipyard;
    case PlayerReportKind::PrecursorArtifactsDiscovered:
        return GameEventKind::PrecursorArtifactsDiscovered;
    case PlayerReportKind::ResearchLevelCompleted:
        return GameEventKind::ResearchLevelCompleted;
    case PlayerReportKind::GroundInvasionWon: return GameEventKind::GroundInvasionWon;
    case PlayerReportKind::GroundInvasionLost: return GameEventKind::GroundInvasionLost;
    case PlayerReportKind::GroundDefenseWon: return GameEventKind::GroundDefenseWon;
    case PlayerReportKind::ColonyLost: return GameEventKind::ColonyLost;
    case PlayerReportKind::FreightDelivered: return GameEventKind::FreightDelivered;
    case PlayerReportKind::EnemyFleetDetected: return GameEventKind::EnemyFleetDetected;
    case PlayerReportKind::EnemyFleetLost: return GameEventKind::EnemyFleetLost;
    case PlayerReportKind::FleetMobilityRestored: return GameEventKind::FleetMobilityRestored;
    case PlayerReportKind::SpaceBattle: return GameEventKind::SpaceBattle;
    case PlayerReportKind::Bombardment: return GameEventKind::Bombardment;
    case PlayerReportKind::MineStrike: return GameEventKind::MineStrike;
    case PlayerReportKind::ScientificData: return GameEventKind::ScientificData;
    case PlayerReportKind::EmissionDetected: return GameEventKind::EmissionDetected;
    case PlayerReportKind::EnemyFleetReclassified: return GameEventKind::EnemyFleetReclassified;
    case PlayerReportKind::SensorInterferenceChanged: return GameEventKind::SensorInterferenceChanged;
    case PlayerReportKind::PlanetaryAftermath: return GameEventKind::PlanetaryAftermath;
    case PlayerReportKind::UnknownObjectObserved: return GameEventKind::UnknownObjectObserved;
    case PlayerReportKind::InterstellarMediumObserved: return GameEventKind::InterstellarMediumObserved;
    case PlayerReportKind::StargateTransit: return GameEventKind::StargateTransit;
    case PlayerReportKind::StargateTransitFailed: return GameEventKind::StargateTransitFailed;
    }
    return GameEventKind::FleetArrived;
}

GameEventSeverity event_severity(PlayerReportKind kind)
{
    if (kind == PlayerReportKind::SpaceBattle || kind == PlayerReportKind::Bombardment
        || kind == PlayerReportKind::MineStrike || kind == PlayerReportKind::EmissionDetected) return GameEventSeverity::Warning;
    if (kind == PlayerReportKind::ColonyLost) return GameEventSeverity::Critical;
    return kind == PlayerReportKind::StargateTransitFailed || kind == PlayerReportKind::EnemyFleetDetected
            || kind == PlayerReportKind::FleetStalledForFuel
            || kind == PlayerReportKind::FleetTargetLost
            || kind == PlayerReportKind::ProductionWaitingForMinerals
            || kind == PlayerReportKind::ProductionWaitingForShipyard
            || kind == PlayerReportKind::GroundInvasionLost
        ? GameEventSeverity::Warning
        : GameEventSeverity::Information;
}

std::uint64_t stable_event_id(const PendingPlayerReport& report, PlayerId recipient)
{
    std::uint64_t hash = 1469598103934665603ULL;
    auto mix = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (value >> (byte * 8)) & 0xffULL;
            hash *= 1099511628211ULL;
        }
    };
    mix(static_cast<std::uint64_t>(event_kind(report.kind)));
    mix(recipient);
    mix(report.observedTurn);
    mix(report.deliveryTurn);
    mix(report.star);
    mix(report.planet);
    mix(report.fleet);
    mix(report.shipDesign);
    mix(static_cast<std::uint64_t>(report.productionKind));
    mix(report.quantity);
    mix(static_cast<std::uint64_t>(report.researchField));
    mix(report.technologyLevel);
    if (report.kind >= PlayerReportKind::Bombardment) {
        mix(std::bit_cast<std::uint64_t>(report.contactPosition.x));
        mix(std::bit_cast<std::uint64_t>(report.contactPosition.y));
        mix(report.deliveredColonists);
    }
    if(report.emission) { mix(report.emission->emittedTurn); mix(std::uint64_t(report.emission->channel)); }
    if(report.objectObservation) {
        const auto& x=*report.objectObservation; mix(x.id); mix(std::uint64_t(x.level));
        mix(x.kind ? std::uint64_t(*x.kind)+1 : 0);
        mix(std::bit_cast<std::uint64_t>(x.position.x)); mix(std::bit_cast<std::uint64_t>(x.position.y));
    }
    if (report.mediumObservation) {
        const auto& x=*report.mediumObservation; mix(x.id); mix(std::uint64_t(x.level));
        for (double value:{x.center.x,x.center.y,x.radiusX,x.radiusY,x.densityMinimum,x.densityMaximum})
            mix(std::bit_cast<std::uint64_t>(value));
    }
    if(report.aftermath) {
        const auto& a=*report.aftermath;
        mix(a.planet); mix(a.factoriesMinimum); mix(a.factoriesMaximum); mix(a.minesMinimum); mix(a.minesMaximum);
        mix(a.contaminationMinimum); mix(a.contaminationMaximum); mix(a.exact);
    }
    if(report.sensorStatus) { mix(report.sensorStatus->fleet); mix(report.sensorStatus->colony); mix(report.sensorStatus->interferencePercent); }
    if(report.contact) { mix(std::uint64_t(report.contact->quality)); mix(report.contact->sourceFleet); mix(report.contact->sourceColony); mix(report.contact->jammed); }
    if (report.battle) {
        mix(std::bit_cast<std::uint64_t>(report.battle->position.x));
        mix(std::bit_cast<std::uint64_t>(report.battle->position.y));
        for (const auto& unit : report.battle->units) { mix(unit.fleet); mix(unit.design); if(unit.station) mix(unit.station); }
    }
    if (report.losses) {
        mix(static_cast<std::uint64_t>(report.losses->kind));
        mix(report.losses->ownColonistsLost);
        mix(report.losses->opposingColonistsLost);
        mix(report.losses->ownInstallationsLost);
        mix(report.losses->opposingInstallationsLost);
        mix(report.losses->ownShipsLost);
    }
    // A receiving colony gets one aggregated freight report per year.
    // The manifest is part of the stable identity even when replaying a turn.
    if (report.kind == PlayerReportKind::FreightDelivered || report.kind == PlayerReportKind::StargateTransit) {
        mix(std::bit_cast<std::uint64_t>(report.deliveredMinerals.ironium));
        mix(std::bit_cast<std::uint64_t>(report.deliveredMinerals.boranium));
        mix(std::bit_cast<std::uint64_t>(report.deliveredMinerals.germanium));
        mix(report.deliveredColonists);
    }
    if (report.kind == PlayerReportKind::EnemyFleetDetected
        || report.kind == PlayerReportKind::EnemyFleetLost) {
        mix(report.contactOwner);
        mix(std::bit_cast<std::uint64_t>(report.contactPosition.x));
        mix(std::bit_cast<std::uint64_t>(report.contactPosition.y));
    }
    return hash;
}

void queue_survey_report(
    GameState& state,
    PlayerId playerId,
    StarId star,
    FleetId sourceFleet,
    Position sourcePosition,
    std::uint64_t observationTurn,
    SurveyLevel level)
{
    if (survey_level(state, playerId, star) > level && level < SurveyLevel::OrbitalSurvey) return;
    auto* player = mutable_player(state, playerId);
    if (!player) return;

    const auto deliveryTurn = fleet_report_delivery_turn(state, playerId, sourcePosition,observationTurn,sourceFleet);
    std::optional<PlayerId> observedOwner;
    if (level >= SurveyLevel::OrbitalSurvey) {
        if (const auto* planet = find_planet_at_star(state, star)) observedOwner = planet->owner;
    }
    const PendingSurveyReport candidate{
        star, sourceFleet, observationTurn, deliveryTurn, level, observedOwner,
        held_report_source(state,playerId,sourcePosition,observationTurn,sourceFleet)};
    const auto dominated = std::any_of(player->pendingSurveyReports.begin(), player->pendingSurveyReports.end(),
        [&](const PendingSurveyReport& report) {
            return report.star == star && report.level >= level && report.deliveryTurn <= deliveryTurn;
        });
    if (dominated) return;

    std::erase_if(player->pendingSurveyReports, [&](const PendingSurveyReport& report) {
        return report.star == star && report.level <= level && report.deliveryTurn >= deliveryTurn;
    });
    player->pendingSurveyReports.push_back(candidate);
    if(deliveryTurn!=kAwaitingTransmission) record_report_transmission(state,playerId,sourcePosition,observationTurn,sourceFleet);
}

SurveyLevel best_observed_level(const GameState& state, PlayerId playerId, StarId star)
{
    auto level = survey_level(state, playerId, star);
    const auto* player = find_player(state, playerId);
    if (!player) return level;
    for (const auto& report : player->pendingSurveyReports) {
        if (report.star == star) level = std::max(level, report.level);
    }
    return level;
}

} // namespace

void queue_player_report(
    GameState& state,
    PlayerId recipient,
    PlayerReportKind kind,
    Position sourcePosition,
    std::uint64_t observationTurn,
    StarId star,
    PlanetId planet,
    FleetId fleet,
    ShipDesignId shipDesign,
    ProductionKind productionKind,
    std::uint32_t quantity,
    ResearchField researchField,
    std::uint8_t technologyLevel,
    MineralCargo deliveredMinerals,
    std::uint64_t deliveredColonists,
    Position contactPosition,
    PlayerId contactOwner,
    std::optional<OperationalLossReport> losses,FleetId observer)
{
    auto* player = mutable_player(state, recipient);
    if (!player) return;
    // The new colony reports its founding itself; a dismantled silent colony
    // ship is not a transmitter that can wake up and send a report later.
    const auto source = observer ? observer : kind==PlayerReportKind::ColonyFounded ? FleetId{0} : fleet;
    const auto deliveryTurn = dispatch_report_delivery_turn(state,recipient,sourcePosition,observationTurn,source);
    player->pendingPlayerReports.push_back({
        kind,
        observationTurn,
        deliveryTurn,
        star,
        planet,
        fleet,
        shipDesign,
        productionKind,
        sourcePosition,
        quantity,
        researchField,
        technologyLevel,
        deliveredMinerals,
        deliveredColonists,
        contactPosition,
        contactOwner,
        {},
        losses,
    });
    player->pendingPlayerReports.back().transmissionFleet =
        held_report_source(state,recipient,sourcePosition,observationTurn,source);
}

void observe_fleet_sensor_sweep(
    GameState& state,
    const Fleet& fleet,
    Position start,
    Position end,
    std::uint64_t observationTurn)
{
    observe_wormhole_sweep(state, fleet, start, end, observationTurn);
    const auto range = strategic_sensor_range(state, fleet,observationTurn);
    if (!fleet_has_scanner(state, fleet) || !fleet_active_sensors(fleet,observationTurn)) return;
    const auto penetratingRange = fleet_penetrating_sensor_range(state, fleet);

    for (const auto& star : state.stars) {
        const auto closest = distance_to_segment(star.position, start, end);
        const auto dx=end.x-start.x,dy=end.y-start.y,length=dx*dx+dy*dy;
        const auto t=length>0?std::clamp(((star.position.x-start.x)*dx+(star.position.y-start.y)*dy)/length,0.0,1.0):0;
        const Position source{start.x+dx*t,start.y+dy*t};
        const auto medium=medium_active_path_factor(state,source,star.position);
        if (closest > range*medium + 0.000001) continue;
        const auto level = penetratingRange > 0.0 && closest <= penetratingRange*medium_active_sensor_factor(state,source)*medium + 0.000001
            ? SurveyLevel::BasicScan
            : SurveyLevel::SystemScan;
        queue_survey_report(state, fleet.owner, star.id, fleet.id, end, observationTurn, level);
    }
}

void observe_current_sensor_coverage(GameState& state, std::uint64_t observationTurn)
{
    for (const auto& star : state.stars) {
        for (const auto& planet : state.planets) {
            if (planet.owner == 0) continue;
            const auto* sourceStar = find_star(state, planet.star);
            const auto radar = planetary_radar_range(planet.radar);
            if (sourceStar && radar > 0
                && within_range(sourceStar->position, star.position, radar*medium_active_sensor_factor(state,sourceStar->position)
                    *medium_active_path_factor(state,sourceStar->position,star.position))) {
                queue_survey_report(
                    state,
                    planet.owner,
                    star.id,
                    0,
                    sourceStar->position,
                    observationTurn,
                    star.id == planet.star ? SurveyLevel::GeologicalSurvey : SurveyLevel::SystemScan);
            }
        }

        for (const auto& fleet : state.fleets) {
            const auto range = strategic_sensor_range(state, fleet,observationTurn);
            const auto medium=medium_active_path_factor(state,fleet.position,star.position);
            if (fleet_active_sensors(fleet,observationTurn) && fleet_has_scanner(state, fleet) && within_range(fleet.position, star.position, range*medium)) {
                auto level = SurveyLevel::SystemScan;
                if (same_position(fleet.position, star.position)) {
                    const auto known = best_observed_level(state, fleet.owner, star.id);
                    if (known < SurveyLevel::OrbitalSurvey) level = SurveyLevel::OrbitalSurvey;
                    else if (known < SurveyLevel::GeologicalSurvey) level = SurveyLevel::GeologicalSurvey;
                    else level = SurveyLevel::DeepSurvey;
                } else if (within_range(
                               fleet.position,
                               star.position,
                               fleet_penetrating_sensor_range(state, fleet)*medium_active_sensor_factor(state,fleet.position)*medium)) {
                    level = SurveyLevel::BasicScan;
                }
                queue_survey_report(
                    state, fleet.owner, star.id, fleet.id, fleet.position, observationTurn, level);
            }
        }
    }
}

void observe_enemy_fleet_contacts(GameState& state,std::uint64_t turn)
{ observe_reconnaissance(state,turn); }

std::vector<GameEvent> deliver_due_survey_reports(GameState& state)
{
    std::vector<GameEvent> events;
    for (auto& player : state.players) {
        std::stable_sort(player.pendingSurveyReports.begin(), player.pendingSurveyReports.end(),
            [](const PendingSurveyReport& lhs, const PendingSurveyReport& rhs) {
                if (lhs.deliveryTurn != rhs.deliveryTurn) return lhs.deliveryTurn < rhs.deliveryTurn;
                if (lhs.observedTurn != rhs.observedTurn) return lhs.observedTurn < rhs.observedTurn;
                if (lhs.star != rhs.star) return lhs.star < rhs.star;
                return lhs.level > rhs.level;
            });

        std::vector<PendingSurveyReport> due;
        for (const auto& report : player.pendingSurveyReports) {
            if (report.deliveryTurn > state.turn) break;
            const auto best = std::find_if(due.begin(), due.end(), [&](const PendingSurveyReport& candidate) {
                return candidate.star == report.star;
            });
            if (best == due.end()) due.push_back(report);
            else if (report.level > best->level
                || (report.level == best->level && report.sourceFleet < best->sourceFleet)) {
                *best = report;
            }
        }

        std::stable_sort(due.begin(), due.end(), [](const PendingSurveyReport& lhs, const PendingSurveyReport& rhs) {
            return lhs.star < rhs.star;
        });
        for (const auto& report : due) {
            const auto previousLevel = survey_level(state, player.id, report.star);
            if (previousLevel > report.level
                && report.level < SurveyLevel::OrbitalSurvey) continue;
            const auto existing = std::find_if(
                player.surveyKnowledge.begin(), player.surveyKnowledge.end(),
                [&](const SystemSurveyKnowledge& entry) { return entry.star == report.star; });
            if (existing != player.surveyKnowledge.end()
                && existing->level == report.level
                && existing->observedTurn >= report.observedTurn) continue;
            set_survey_level(state, player.id, report.star, report.level, report.observedTurn);
            if (report.level >= SurveyLevel::OrbitalSurvey) {
                const auto knowledge = std::find_if(
                    player.surveyKnowledge.begin(), player.surveyKnowledge.end(),
                    [&](const SystemSurveyKnowledge& entry) { return entry.star == report.star; });
                if (knowledge != player.surveyKnowledge.end()
                    && report.observedTurn >= knowledge->observedTurn)
                    knowledge->observedOwner = report.observedOwner;
            }
            if (report.level <= previousLevel) continue;

            const auto* planet = find_planet_at_star(state, report.star);
            const auto* star = find_star(state, report.star);
            const auto knownHabitability = planet
                ? known_planet_habitability(state, player.id, planet->id).value_or(0)
                : 0;
            const auto artifactHint = planet && report.level >= SurveyLevel::DeepSurvey
                ? known_precursor_artifact_hint(state, player.id, planet->id).value_or(false)
                : false;
            events.push_back({
                stable_event_id(report, player.id),
                state.turn,
                report.observedTurn,
                player.id,
                GameEventKind::SystemSurveyed,
                GameEventSeverity::Information,
                report.star,
                planet ? planet->id : 0,
                report.sourceFleet,
                0,
                ProductionKind::ColonyShip,
                star ? star->position : Position{},
                knownHabitability,
                report.level,
                ResearchField::Electronics,
                0,
                artifactHint,
            });
        }

        std::erase_if(player.pendingSurveyReports, [&](const PendingSurveyReport& report) {
            return report.deliveryTurn <= state.turn
                || (survey_level(state, player.id, report.star) > report.level
                    && report.level < SurveyLevel::OrbitalSurvey);
        });
    }
    return events;
}

std::vector<GameEvent> deliver_due_player_reports(GameState& state)
{
    deliver_reconnaissance(state);
    std::vector<GameEvent> events;
    for (auto& player : state.players) {
        std::stable_sort(player.pendingPlayerReports.begin(), player.pendingPlayerReports.end(),
            [](const PendingPlayerReport& lhs, const PendingPlayerReport& rhs) {
                if (lhs.deliveryTurn != rhs.deliveryTurn) return lhs.deliveryTurn < rhs.deliveryTurn;
                if (lhs.observedTurn != rhs.observedTurn) return lhs.observedTurn < rhs.observedTurn;
                if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
                if (lhs.fleet != rhs.fleet) return lhs.fleet < rhs.fleet;
                return lhs.planet < rhs.planet;
            });

        for (const auto& report : player.pendingPlayerReports) {
            if (report.deliveryTurn > state.turn) break;
            auto medium=report.mediumObservation;
            if (medium) {
                medium->receivedTurn=state.turn;
                auto current=std::find_if(player.mediumKnowledge.begin(),player.mediumKnowledge.end(),
                    [&](const auto& x) { return x.id==medium->id; });
                const bool upgraded=current==player.mediumKnowledge.end() || medium->level>current->level;
                if (upgraded || (medium->level==current->level && medium->observedTurn>current->observedTurn)) {
                    if (current==player.mediumKnowledge.end()) player.mediumKnowledge.push_back(*medium);
                    else *current=*medium;
                }
                if (!upgraded) continue; // Refresh routine measurements without annual briefings.
            }
            auto object=report.objectObservation;
            if(object) {
                object->receivedTurn=state.turn;
                auto current=std::find_if(player.objectKnowledge.begin(),player.objectKnowledge.end(),
                    [&](const auto& x){return x.id==object->id;});
                // A study is a finite discovery per empire, including when an
                // already delivered packet is replayed or copied in an outbox.
                if(report.kind==PlayerReportKind::ScientificData && current!=player.objectKnowledge.end()
                    && current->level==ObjectKnowledgeLevel::Studied) continue;
                const bool upgraded=current==player.objectKnowledge.end() || object->level>current->level;
                if(upgraded || (object->level==current->level && object->observedTurn>current->observedTurn)) {
                    if(current==player.objectKnowledge.end()) player.objectKnowledge.push_back(*object);
                    else *current=*object;
                }
                if(report.kind==PlayerReportKind::UnknownObjectObserved && !upgraded) continue;
            }
            auto aftermath=report.aftermath;
            if(aftermath) {
                aftermath->receivedTurn=state.turn;
                auto current=std::find_if(player.planetaryAftermath.begin(),player.planetaryAftermath.end(),
                    [&](const auto& a){return a.planet==report.planet;});
                const bool newer=current==player.planetaryAftermath.end() || aftermath->observedTurn>current->observedTurn
                    || (aftermath->observedTurn==current->observedTurn && aftermath->exact && !current->exact);
                const bool changed=current==player.planetaryAftermath.end() || !same_aftermath_estimate(*current,*aftermath);
                if(newer) {
                    if(current==player.planetaryAftermath.end()) player.planetaryAftermath.push_back(*aftermath);
                    else *current=*aftermath;
                }
                // Routine unchanged measurements refresh the date without a
                // new briefing every year. Old packets never roll knowledge back.
                if(report.kind==PlayerReportKind::PlanetaryAftermath && (!newer || !changed)) continue;
            }
            if(report.sensorStatus) {
                const auto current=std::find_if(player.sensorStatus.begin(),player.sensorStatus.end(),[&](const auto& x){return x.fleet==report.fleet && x.colony==report.planet;});
                if(current!=player.sensorStatus.end() && current->observedTurn>report.observedTurn) continue;
            }
            if(report.contact) {
                const auto current=std::find_if(player.enemyContacts.begin(),player.enemyContacts.end(),[&](const auto& c){return c.fleet==report.fleet;});
                if(current!=player.enemyContacts.end() && current->observedTurn>report.observedTurn) continue;
            }
            if(report.contact || report.sensorStatus) {
                const auto id=stable_event_id(report,player.id);
                if(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.id==id;})
                    || std::any_of(player.history.begin(),player.history.end(),[&](const auto& h){return std::any_of(h.milestones.begin(),h.milestones.end(),[&](const auto& m){return m.eventId==id;});})) continue;
            }
            if (report.kind == PlayerReportKind::ScientificData) apply_scientific_data(state, player, report, events);
            auto severity = event_severity(report.kind);
            if(report.sensorStatus) severity=report.quantity?GameEventSeverity::Warning:GameEventSeverity::Information;
            if (report.battle) {
                bool survivors = false, losses = false;
                for (const auto& unit : report.battle->units) if (unit.owner == player.id) {
                    survivors |= unit.survivingShips > 0;
                    losses |= unit.survivingShips < unit.initialShips;
                }
                severity = !survivors ? GameEventSeverity::Critical
                    : losses || report.battle->stalemate ? GameEventSeverity::Warning : GameEventSeverity::Information;
            }
            events.push_back({
                stable_event_id(report, player.id),
                state.turn,
                report.observedTurn,
                player.id,
                event_kind(report.kind),
                severity,
                report.star,
                report.planet,
                report.fleet,
                report.shipDesign,
                report.productionKind,
                medium ? medium->center : object ? object->position : report.kind == PlayerReportKind::EnemyFleetDetected
                        || report.kind == PlayerReportKind::EnemyFleetLost || report.kind == PlayerReportKind::EnemyFleetReclassified
                        || report.kind == PlayerReportKind::ScientificData || report.kind == PlayerReportKind::EmissionDetected
                    ? report.contactPosition : report.position,
                static_cast<std::int32_t>(report.quantity),
                SurveyLevel::Detected,
                report.researchField,
                report.technologyLevel,
                false,
                report.deliveredMinerals,
                report.deliveredColonists,
                report.contactOwner,
                0,
                report.battle,
                report.losses,
                report.contact,
                report.sensorStatus,
                report.emission,
                aftermath,
                object,
                medium,
            });
        }

        std::erase_if(player.pendingPlayerReports, [&](const PendingPlayerReport& report) {
            return report.deliveryTurn <= state.turn;
        });
    }
    std::erase_if(state.pendingFleetLossContacts, [&](const auto& contact) {
        return contact.deliveryTurn <= state.turn;
    });
    return events;
}

} // namespace suns
