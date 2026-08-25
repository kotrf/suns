#include "suns/player_knowledge.hpp"

#include "suns/communications.hpp"

#include <algorithm>
#include <cstdint>

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
    }
    return GameEventKind::FleetArrived;
}

GameEventSeverity event_severity(PlayerReportKind kind)
{
    return kind == PlayerReportKind::FleetStalledForFuel
            || kind == PlayerReportKind::ProductionWaitingForMinerals
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
    if (survey_level(state, playerId, star) >= level) return;
    auto* player = mutable_player(state, playerId);
    if (!player) return;

    const auto deliveryTurn = observationTurn + communication_delay_turns(state, playerId, sourcePosition);
    const PendingSurveyReport candidate{star, sourceFleet, observationTurn, deliveryTurn, level};
    const auto dominated = std::any_of(player->pendingSurveyReports.begin(), player->pendingSurveyReports.end(),
        [&](const PendingSurveyReport& report) {
            return report.star == star && report.level >= level && report.deliveryTurn <= deliveryTurn;
        });
    if (dominated) return;

    std::erase_if(player->pendingSurveyReports, [&](const PendingSurveyReport& report) {
        return report.star == star && report.level <= level && report.deliveryTurn >= deliveryTurn;
    });
    player->pendingSurveyReports.push_back(candidate);
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
    std::uint32_t quantity)
{
    auto* player = mutable_player(state, recipient);
    if (!player) return;
    const auto deliveryTurn = observationTurn
        + communication_delay_turns(state, recipient, sourcePosition);
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
    });
}

void observe_fleet_sensor_sweep(
    GameState& state,
    const Fleet& fleet,
    Position start,
    Position end,
    std::uint64_t observationTurn)
{
    const auto range = fleet_sensor_range(state, fleet);
    if (range <= 0.0) return;
    const auto penetratingRange = fleet_penetrating_sensor_range(state, fleet);

    for (const auto& star : state.stars) {
        const auto closest = distance_to_segment(star.position, start, end);
        if (closest > range + 0.000001) continue;
        const auto level = penetratingRange > 0.0 && closest <= penetratingRange + 0.000001
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
            if (sourceStar && within_range(sourceStar->position, star.position, kColonySensorRange)) {
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
            const auto range = fleet_sensor_range(state, fleet);
            if (range > 0.0 && within_range(fleet.position, star.position, range)) {
                auto level = SurveyLevel::SystemScan;
                if (same_position(fleet.position, star.position)) {
                    level = best_observed_level(state, fleet.owner, star.id) >= SurveyLevel::OrbitalSurvey
                        ? SurveyLevel::GeologicalSurvey
                        : SurveyLevel::OrbitalSurvey;
                } else if (within_range(
                               fleet.position,
                               star.position,
                               fleet_penetrating_sensor_range(state, fleet))) {
                    level = SurveyLevel::BasicScan;
                }
                queue_survey_report(
                    state, fleet.owner, star.id, fleet.id, fleet.position, observationTurn, level);
            }
        }
    }
}

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
            if (survey_level(state, player.id, report.star) >= report.level) continue;
            set_survey_level(state, player.id, report.star, report.level, report.observedTurn);

            const auto* planet = find_planet_at_star(state, report.star);
            const auto* star = find_star(state, report.star);
            const auto knownHabitability = planet
                ? known_planet_habitability(state, player.id, planet->id).value_or(0)
                : 0;
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
            });
        }

        std::erase_if(player.pendingSurveyReports, [&](const PendingSurveyReport& report) {
            return report.deliveryTurn <= state.turn
                || survey_level(state, player.id, report.star) >= report.level;
        });
    }
    return events;
}

std::vector<GameEvent> deliver_due_player_reports(GameState& state)
{
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
            events.push_back({
                stable_event_id(report, player.id),
                state.turn,
                report.observedTurn,
                player.id,
                event_kind(report.kind),
                event_severity(report.kind),
                report.star,
                report.planet,
                report.fleet,
                report.shipDesign,
                report.productionKind,
                report.position,
                report.quantity,
            });
        }

        std::erase_if(player.pendingPlayerReports, [&](const PendingPlayerReport& report) {
            return report.deliveryTurn <= state.turn;
        });
    }
    return events;
}

} // namespace suns
