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
    return hash;
}

void queue_survey_report(
    GameState& state,
    PlayerId playerId,
    StarId star,
    FleetId sourceFleet,
    Position sourcePosition,
    std::uint64_t observationTurn)
{
    if (is_surveyed(state, playerId, star)) return;
    auto* player = mutable_player(state, playerId);
    if (!player) return;

    const auto deliveryTurn = observationTurn + communication_delay_turns(state, playerId, sourcePosition);
    const auto existing = std::find_if(player->pendingSurveyReports.begin(), player->pendingSurveyReports.end(),
        [star](const PendingSurveyReport& report) { return report.star == star; });

    const PendingSurveyReport candidate{star, sourceFleet, observationTurn, deliveryTurn};
    if (existing == player->pendingSurveyReports.end()) {
        player->pendingSurveyReports.push_back(candidate);
    } else if (deliveryTurn < existing->deliveryTurn
        || (deliveryTurn == existing->deliveryTurn && sourceFleet < existing->sourceFleet)) {
        *existing = candidate;
    }
}

} // namespace

void observe_fleet_sensor_sweep(
    GameState& state,
    const Fleet& fleet,
    Position start,
    Position end,
    std::uint64_t observationTurn)
{
    const auto range = fleet_sensor_range(state, fleet);
    if (range <= 0.0) return;

    for (const auto& star : state.stars) {
        if (distance_to_segment(star.position, start, end) <= range + 0.000001) {
            queue_survey_report(state, fleet.owner, star.id, fleet.id, end, observationTurn);
        }
    }
}

void observe_current_sensor_coverage(GameState& state, std::uint64_t observationTurn)
{
    for (const auto& star : state.stars) {
        for (const auto& planet : state.planets) {
            if (planet.owner == 0) continue;
            const auto* sourceStar = find_star(state, planet.star);
            if (sourceStar && within_range(sourceStar->position, star.position, kColonySensorRange)) {
                queue_survey_report(state, planet.owner, star.id, 0, sourceStar->position, observationTurn);
            }
        }

        for (const auto& fleet : state.fleets) {
            const auto range = fleet_sensor_range(state, fleet);
            if (range > 0.0 && within_range(fleet.position, star.position, range)) {
                queue_survey_report(state, fleet.owner, star.id, fleet.id, fleet.position, observationTurn);
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
                return lhs.star < rhs.star;
            });

        for (const auto& report : player.pendingSurveyReports) {
            if (report.deliveryTurn > state.turn || is_surveyed(state, player.id, report.star)) continue;
            mark_surveyed(state, player.id, report.star);

            const auto* planet = find_planet_at_star(state, report.star);
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
            });
        }

        std::erase_if(player.pendingSurveyReports, [&](const PendingSurveyReport& report) {
            return report.deliveryTurn <= state.turn || is_surveyed(state, player.id, report.star);
        });
    }
    return events;
}

} // namespace suns
