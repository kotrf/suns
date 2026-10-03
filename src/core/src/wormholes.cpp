#include "suns/wormholes.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace suns {
namespace {

std::uint64_t mix(std::uint64_t n)
{
    n += 0x9e3779b97f4a7c15ULL;
    n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ULL;
    n = (n ^ (n >> 27)) * 0x94d049bb133111ebULL;
    return n ^ (n >> 31);
}

double unit(std::uint64_t n) { return static_cast<double>(mix(n) >> 11) / 9007199254740992.0; }
std::uint64_t seed(const GameState& state, std::uint64_t id, std::uint64_t turn, std::uint64_t channel)
{
    return mix(state.galaxySeed) ^ mix(id) ^ mix(turn + channel * 0x9e3779b97f4a7c15ULL);
}

Position random_position(const WormholeRules& rules, std::uint64_t n)
{
    return {(unit(n) - 0.5) * rules.width, (unit(n ^ 0x9861ULL) - 0.5) * rules.height};
}

double segment_distance(Position p, Position a, Position b)
{
    const auto dx = b.x - a.x, dy = b.y - a.y;
    const auto length = dx * dx + dy * dy;
    const auto t = length > 1e-12 ? std::clamp(((p.x-a.x)*dx + (p.y-a.y)*dy) / length, 0.0, 1.0) : 0.0;
    return distance_between(p, {a.x + t * dx, a.y + t * dy});
}

WormholeStability stability_band(double stability)
{
    return stability < 0.35 ? WormholeStability::Unstable
        : stability < 0.75 ? WormholeStability::Variable : WormholeStability::Stable;
}

double detection_range(const WormholeEndpoint& mouth, double range, bool detector)
{
    if (mouth.signature == WormholeSignature::Weak) return detector ? range * 0.35 : 0.0;
    return range * (mouth.signature == WormholeSignature::Faint ? 0.45 : 1.0);
}

Player* player_for(GameState& state, PlayerId id)
{
    const auto it = std::find_if(state.players.begin(), state.players.end(), [=](const auto& p) { return p.id == id; });
    return it == state.players.end() ? nullptr : &*it;
}

void queue(GameState& state, PlayerId owner, WormholeReportKind kind, WormholeKnowledge knowledge,
    FleetId fleet, Position source, std::uint64_t turn)
{
    auto* p = player_for(state, owner);
    if (!p) return;
    const auto due = turn + communication_delay_turns(state, owner, source);
    if (kind == WormholeReportKind::Observation) {
        const auto duplicate = std::find_if(p->pendingWormholeReports.begin(), p->pendingWormholeReports.end(),
            [&](const auto& r) { return r.kind == kind && r.knowledge.endpoint == knowledge.endpoint
                && r.observedTurn == turn && r.deliveryTurn <= due && r.knowledge.stability >= knowledge.stability; });
        if (duplicate != p->pendingWormholeReports.end()) return;
    }
    p->pendingWormholeReports.push_back({kind, turn, due, fleet, knowledge});
}

void observe(GameState& state, PlayerId owner, FleetId fleet, Position start, Position end,
    double range, bool detector, std::uint64_t turn, bool collapsed = false)
{
    for (const auto& hole : state.wormholes) for (const auto& mouth : hole.endpoints) {
        const auto reach = detection_range(mouth, range, detector);
        const auto distance = segment_distance(mouth.position, start, end);
        if (reach <= 0.0 || distance > reach) continue;
        const auto* player = find_player(state, owner);
        const bool inFlight = player && std::any_of(player->pendingWormholeReports.begin(), player->pendingWormholeReports.end(),
            [&](const auto& report) { return report.knowledge.endpoint == mouth.id; });
        if (collapsed && !known_wormhole(state, owner, mouth.id) && !inFlight) continue;
        WormholeKnowledge knowledge{mouth.id, mouth.position, turn};
        if (detector || distance <= reach * 0.35) knowledge.stability = stability_band(hole.stability);
        knowledge.collapsed = collapsed;
        queue(state, owner, collapsed ? WormholeReportKind::Collapsed : WormholeReportKind::Observation,
            knowledge, fleet, end, turn);
    }
}

GameEvent event_for(PlayerId owner, GameEventKind kind, const WormholeKnowledge& knowledge,
    FleetId fleet, std::uint64_t deliveredTurn)
{
    GameEvent event;
    event.id = mix(mix(owner) ^ mix(static_cast<std::uint64_t>(kind)) ^ mix(knowledge.endpoint)
        ^ mix(fleet) ^ mix(knowledge.observedTurn + 0x837621ULL));
    event.recipient = owner;
    event.kind = kind;
    event.turn = deliveredTurn;
    event.observedTurn = knowledge.observedTurn;
    event.position = knowledge.lastPosition;
    event.fleet = fleet;
    event.wormholeEndpoint = knowledge.endpoint;
    if (kind == GameEventKind::WormholeOverdue || kind == GameEventKind::WormholeEntryMissed
        || kind == GameEventKind::WormholeCollapsed) event.severity = GameEventSeverity::Warning;
    if (kind == GameEventKind::WormholePresumedLost) event.severity = GameEventSeverity::Critical;
    return event;
}

void merge_knowledge(Player& player, const WormholeKnowledge& incoming)
{
    auto it = std::find_if(player.wormholeKnowledge.begin(), player.wormholeKnowledge.end(),
        [&](const auto& k) { return k.endpoint == incoming.endpoint; });
    if (it == player.wormholeKnowledge.end()) { player.wormholeKnowledge.push_back(incoming); return; }
    if (incoming.observedTurn >= it->observedTurn) {
        it->lastPosition = incoming.lastPosition;
        it->observedTurn = incoming.observedTurn;
        it->collapsed = incoming.collapsed || it->collapsed;
    }
    if (incoming.stability != WormholeStability::Unknown) it->stability = incoming.stability;
    if (incoming.linkedEndpoint) it->linkedEndpoint = incoming.linkedEndpoint;
}

} // namespace

const WormholeKnowledge* known_wormhole(const GameState& state, PlayerId owner, WormholeEndpointId id)
{
    const auto* player = find_player(state, owner);
    if (!player) return nullptr;
    const auto it = std::find_if(player->wormholeKnowledge.begin(), player->wormholeKnowledge.end(),
        [=](const auto& k) { return k.endpoint == id; });
    return it == player->wormholeKnowledge.end() ? nullptr : &*it;
}

bool fleet_has_anomaly_detector(const GameState& state, const Fleet& fleet)
{
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        if (design && stack.count > 0 && std::find(design->components.begin(), design->components.end(),
                ShipComponentType::AnomalyDetector) != design->components.end()) return true;
    }
    return false;
}

void observe_wormhole_sweep(GameState& state, const Fleet& fleet, Position start, Position end, std::uint64_t turn)
{
    observe(state, fleet.owner, fleet.id, start, end, fleet_sensor_range(state, fleet),
        fleet_has_anomaly_detector(state, fleet), turn);
}

void observe_current_wormholes(GameState& state, std::uint64_t turn)
{
    for (const auto& fleet : state.fleets) observe_wormhole_sweep(state, fleet, fleet.position, fleet.position, turn);
    for (const auto& planet : state.planets) if (planet.owner && planet.population) {
        if (const auto* star = find_star(state, planet.star))
            observe(state, planet.owner, 0, star->position, star->position, kColonySensorRange, false, turn);
    }
}

void advance_wormholes(GameState& state)
{
    const auto turn = state.turn + 1;
    const auto& rules = state.wormholeRules;
    // Observe a collapse only where an existing contact is actually in sensor coverage.
    for (const auto& hole : state.wormholes) if (hole.collapseTurn <= turn) {
        GameState observation = state;
        observation.wormholes = {hole};
        for (const auto& fleet : state.fleets)
            observe(observation, fleet.owner, fleet.id, fleet.position, fleet.position,
                fleet_sensor_range(state, fleet), fleet_has_anomaly_detector(state, fleet), turn, true);
        for (const auto& planet : state.planets) if (planet.owner && planet.population) {
            if (const auto* star = find_star(state, planet.star))
                observe(observation, planet.owner, 0, star->position, star->position, kColonySensorRange, false, turn, true);
        }
        for (std::size_t i = 0; i < state.players.size(); ++i)
            state.players[i].pendingWormholeReports = std::move(observation.players[i].pendingWormholeReports);
    }
    std::erase_if(state.wormholes, [=](const auto& h) { return h.collapseTurn <= turn; });
    for (auto& hole : state.wormholes) for (auto& mouth : hole.endpoints) {
        const auto n = seed(state, mouth.id, turn, 1);
        if (unit(n) < rules.relocationChance * (1.0 - hole.stability)) mouth.position = random_position(rules, n);
        else {
            if (mouth.driftDirection.x == 0 && mouth.driftDirection.y == 0) {
                const auto angle = unit(seed(state, mouth.id, 0, 2)) * 6.283185307179586;
                mouth.driftDirection = {std::cos(angle), std::sin(angle)};
            }
            const auto drift = rules.driftPerTurn * (1.25 - hole.stability);
            mouth.position.x += drift * mouth.driftDirection.x;
            mouth.position.y += drift * mouth.driftDirection.y;
            // Reflect the trajectory instead of pinning a mouth at a corner.
            if (std::abs(mouth.position.x) > rules.width/2) mouth.driftDirection.x *= -1;
            if (std::abs(mouth.position.y) > rules.height/2) mouth.driftDirection.y *= -1;
            mouth.position.x = std::clamp(mouth.position.x, -rules.width/2, rules.width/2);
            mouth.position.y = std::clamp(mouth.position.y, -rules.height/2, rules.height/2);
        }
    }
    if (state.galaxySeed == 0 || turn < 5 || state.wormholes.size() >= rules.maximumPairs
        || state.nextWormholeEndpointId > std::numeric_limits<WormholeEndpointId>::max() - 2
        || unit(seed(state, 0, turn, 3)) >= rules.spawnChancePerTurn) return;
    Wormhole hole;
    hole.createdTurn = turn;
    hole.stability = unit(seed(state, state.nextWormholeEndpointId, turn, 4));
    const auto span = rules.maximumLifetime - rules.minimumLifetime;
    hole.collapseTurn = turn + rules.minimumLifetime + static_cast<std::uint32_t>(hole.stability * span);
    for (auto& mouth : hole.endpoints) {
        mouth.id = state.nextWormholeEndpointId++;
        const auto n = seed(state, mouth.id, turn, 5);
        mouth.position = random_position(rules, n);
        mouth.signature = static_cast<WormholeSignature>(mix(n) % 3);
    }
    // Natural shortcuts should connect widely separated parts of the map.
    if (distance_between(hole.endpoints[0].position, hole.endpoints[1].position) < std::hypot(rules.width, rules.height)*0.3) {
        auto& b = hole.endpoints[1].position;
        const auto a = hole.endpoints[0].position;
        b = {a.x > 0 ? -rules.width*0.45 : rules.width*0.45,
             a.y > 0 ? -rules.height*0.45 : rules.height*0.45};
    }
    state.wormholes.push_back(hole);
}

void resolve_wormhole_approaches(GameState& state)
{
    for (auto& fleet : state.fleets) {
        if (!fleet.destination || !fleet.arrivalAction || fleet.arrivalAction->kind != FleetArrivalActionKind::EnterWormhole) continue;
        for (const auto& hole : state.wormholes) for (const auto& mouth : hole.endpoints) {
            if (mouth.id != fleet.arrivalAction->wormholeEndpoint) continue;
            const auto reach = detection_range(mouth, fleet_sensor_range(state, fleet), fleet_has_anomaly_detector(state, fleet));
            if (reach > 0 && distance_between(fleet.position, mouth.position) <= reach) fleet.destination = mouth.position;
        }
    }
}

bool enter_wormhole(GameState& state, Fleet& fleet, WormholeEndpointId endpoint)
{
    const auto turn = state.turn + 1;
    const Wormhole* selected{};
    std::size_t side{};
    for (const auto& hole : state.wormholes) for (std::size_t i = 0; i < 2; ++i)
        if (hole.endpoints[i].id == endpoint && same_position(fleet.position, hole.endpoints[i].position)) { selected = &hole; side = i; }
    fleet.destination.reset();
    fleet.arrivalAction.reset();
    fleet.waypointQueue.clear();
    fleet.repeatOrders = false;
    fleet.routeTemplate.clear();
    fleet.targetFleet = 0;
    fleet.task = FleetTask::None;
    WormholeKnowledge entry{endpoint, fleet.position, turn};
    if (!selected) {
        queue(state, fleet.owner, WormholeReportKind::EntryMissed, entry, fleet.id, fleet.position, turn);
        return false;
    }
    entry.stability = stability_band(selected->stability);
    const auto delay = communication_delay_turns(state, fleet.owner, fleet.position);
    // The timeout uses the whole-map signal bound, never the secret outcome or exit.
    const auto wait = static_cast<std::uint64_t>(std::ceil(std::hypot(state.wormholeRules.width,
        state.wormholeRules.height) / kCommunicationSignalSpeed));
    const auto overdue = turn + delay + wait + state.wormholeRules.overdueGraceTurns;
    std::erase_if(state.wormholeTransits, [&](const auto& t) { return t.lastContact.id == fleet.id; });
    auto contact = fleet_player_view(state, fleet);
    contact.fuelStalled = false;
    if (contact.telemetry.ships.empty()) contact.ships = {{contact.design, 1}};
    state.wormholeTransits.push_back({contact, endpoint, turn, overdue,
        overdue + state.wormholeRules.presumedLostGraceTurns});
    queue(state, fleet.owner, WormholeReportKind::Entered, entry, fleet.id, fleet.position, turn);
    const auto& rules = state.wormholeRules;
    const auto risk = std::clamp((rules.minimumLossChance + rules.instabilityLossChance * (1.0-selected->stability))
        * (fleet_has_anomaly_detector(state, fleet) ? rules.detectorRiskMultiplier : 1.0), rules.minimumLossChance, 1.0);
    if (unit(seed(state, fleet.id, turn, 6) ^ mix(endpoint)) < risk) return true;
    const auto& exit = selected->endpoints[1-side];
    fleet.position = exit.position;
    WormholeKnowledge emergence{exit.id, exit.position, turn, stability_band(selected->stability), endpoint};
    queue(state, fleet.owner, WormholeReportKind::Emerged, emergence, fleet.id, fleet.position, turn);
    observe_wormhole_sweep(state, fleet, fleet.position, fleet.position, turn);
    return false;
}

std::vector<GameEvent> deliver_wormhole_reports(GameState& state)
{
    std::vector<GameEvent> events;
    for (auto& player : state.players) {
        auto& reports = player.pendingWormholeReports;
        std::stable_sort(reports.begin(), reports.end(), [](const auto& a, const auto& b) {
            return std::tie(a.deliveryTurn, a.observedTurn, a.kind, a.knowledge.endpoint, a.fleet)
                < std::tie(b.deliveryTurn, b.observedTurn, b.kind, b.knowledge.endpoint, b.fleet);
        });
        for (const auto& report : reports) {
            if (report.deliveryTurn > state.turn) continue;
            const auto* old = known_wormhole(state, player.id, report.knowledge.endpoint);
            const bool discovered = !old;
            const bool classified = report.knowledge.stability != WormholeStability::Unknown
                && (!old || old->stability == WormholeStability::Unknown);
            const bool collapseKnown = old && old->collapsed;
            auto knowledge = report.knowledge;
            if (report.kind != WormholeReportKind::EntryMissed) merge_knowledge(player, knowledge);
            if (report.kind == WormholeReportKind::Entered) {
                for (const auto& exit : player.wormholeKnowledge) if (exit.linkedEndpoint == knowledge.endpoint) {
                    knowledge.linkedEndpoint = exit.endpoint;
                    merge_knowledge(player, knowledge);
                    break;
                }
            }
            std::optional<GameEventKind> kind;
            switch (report.kind) {
            case WormholeReportKind::Observation:
                if (discovered) kind = GameEventKind::AnomalyDetected;
                else if (classified) kind = GameEventKind::WormholeClassified;
                break;
            case WormholeReportKind::Entered: kind = GameEventKind::WormholeEntered; break;
            case WormholeReportKind::Emerged: kind = GameEventKind::WormholeEmerged; break;
            case WormholeReportKind::EntryMissed: kind = GameEventKind::WormholeEntryMissed; break;
            case WormholeReportKind::Collapsed: if (!collapseKnown) kind = GameEventKind::WormholeCollapsed; break;
            }
            if (kind) events.push_back(event_for(player.id, *kind, knowledge, report.fleet, state.turn));
            for (auto& transit : state.wormholeTransits) {
                if (transit.lastContact.owner != player.id || transit.lastContact.id != report.fleet
                    || transit.enteredTurn != report.observedTurn) continue;
                if (report.kind == WormholeReportKind::Emerged) {
                    transit.status = WormholeTransitStatus::EmergenceConfirmed;
                    if (const auto* knownEntry = known_wormhole(state, player.id, transit.endpoint)) {
                        auto entry = *knownEntry;
                        entry.linkedEndpoint = knowledge.endpoint;
                        merge_knowledge(player, entry);
                    }
                } else if (report.kind == WormholeReportKind::Entered
                    && transit.status == WormholeTransitStatus::AwaitingEntryReport) {
                    transit.status = WormholeTransitStatus::AwaitingEmergence;
                    auto& contact = transit.lastContact;
                    contact.position = knowledge.lastPosition;
                    contact.destination.reset();
                    contact.arrivalAction.reset();
                    contact.waypointQueue.clear();
                    contact.repeatOrders = false;
                    contact.routeTemplate.clear();
                    contact.telemetry.waypointQueue.clear();
                    contact.telemetry.repeatOrders = false;
                    contact.telemetry.routeTemplate.clear();
                    contact.telemetry.position = contact.position;
                    contact.telemetry.observedTurn = report.observedTurn;
                    contact.telemetry.destination.reset();
                    contact.telemetry.arrivalAction.reset();
                }
            }
        }
        std::erase_if(reports, [&](const auto& r) { return r.deliveryTurn <= state.turn; });
    }
    for (auto& transit : state.wormholeTransits) {
        WormholeKnowledge knowledge{transit.endpoint, transit.lastContact.position, state.turn};
        if (transit.status == WormholeTransitStatus::AwaitingEmergence && state.turn >= transit.overdueTurn) {
            transit.status = WormholeTransitStatus::Overdue;
            events.push_back(event_for(transit.lastContact.owner, GameEventKind::WormholeOverdue, knowledge, transit.lastContact.id, state.turn));
        }
        if (transit.status == WormholeTransitStatus::Overdue && state.turn >= transit.presumedLostTurn) {
            transit.status = WormholeTransitStatus::PresumedLost;
            events.push_back(event_for(transit.lastContact.owner, GameEventKind::WormholePresumedLost, knowledge, transit.lastContact.id, state.turn));
        }
    }
    return events;
}

std::vector<Fleet> wormhole_missing_contacts(const GameState& state, PlayerId player)
{
    std::vector<Fleet> contacts;
    for (const auto& transit : state.wormholeTransits) {
        if (transit.lastContact.owner != player || transit.status == WormholeTransitStatus::PresumedLost
            || transit.status == WormholeTransitStatus::EmergenceConfirmed) continue;
        if (std::none_of(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == transit.lastContact.id; }))
            contacts.push_back(transit.lastContact);
    }
    return contacts;
}

} // namespace suns
