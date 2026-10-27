#include "suns/survey_probes.hpp"
#include "suns/communications.hpp"
#include "suns/campaign.hpp"
#include "suns/interstellar_medium.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace suns {
namespace {
void clear_route(Fleet& f)
{
    f.destination.reset(); f.targetFleet = 0; f.arrivalAction.reset();
    f.waypointQueue.clear(); f.routeTemplate.clear(); f.repeatOrders = false; f.task = FleetTask::None;
}
double leg_cost(const GameState& s, Fleet& f, Position start, Position end, std::uint8_t warp)
{
    const auto position=f.position;const auto oldWarp=f.warp;
    f.position = start; f.warp = warp;
    const auto distance = distance_between(start, end), step = warp_distance(warp);
    const auto years = std::floor(distance / step);
    const auto result = years * fleet_segment_fuel_bounds(s, f, step).consumedMaximum
        + fleet_segment_fuel_bounds(s, f, std::max(0.0, distance - years * step)).consumedMaximum;
    f.position=position;f.warp=oldWarp;return result;
}
std::optional<std::uint8_t> flight_warp(const GameState& s, const Fleet& f, Position target, bool returning)
{
    auto cap = std::min(f.probe.program.maxWarp, fleet_max_warp(s, f));
    for (const auto& stack : fleet_ship_stacks(f)) if (const auto* d = find_ship_design(s, stack.design))
        cap = std::min(cap, ship_design_cruise_warp(*d));
    std::optional<std::uint8_t> best; double bestFuel = std::numeric_limits<double>::infinity();
    auto check=f;
    for (std::uint8_t warp = 1; warp <= cap; ++warp) {
        check.warp = warp;
        if (!fleet_warp_valid(s, check, warp) || fleet_overdrive_damage_rate(s, check, warp) > 0) continue;
        const auto fuel = leg_cost(s, check, f.position, target, warp);
        const auto home = !returning && f.probe.program.returnHome ? leg_cost(s, check, target, f.probe.origin, warp) : 0.0;
        if (fuel + home + (returning ? 0.0 : f.probe.program.fuelReserve) > f.fuel + 1e-6) continue;
        if (f.probe.program.flight == ProbeFlightMode::SafeCruise || fuel < bestFuel - 1e-6) { best = warp; bestFuel = fuel; }
    }
    return best;
}
void return_or_hold(const GameState& s, Fleet& f, bool fuelShortage)
{
    auto& m = f.probe; clear_route(f); m.target = 0;
    if (m.program.returnHome && !same_position(f.position, m.origin)) {
        if (const auto warp = flight_warp(s, f, m.origin, true)) {
            f.destination = m.origin; f.warp = *warp; m.status = ProbeStatus::Returning; return;
        }
        m.status = ProbeStatus::AwaitingFuel; return;
    }
    if (fuelShortage) m.status = ProbeStatus::AwaitingFuel;
    else { m.enabled = false; m.status = ProbeStatus::Complete; }
}
}
bool probe_program_valid(const ProbeProgram& p)
{
    return std::isfinite(p.center.x) && std::isfinite(p.center.y) && std::abs(p.center.x) <= 1e9 && std::abs(p.center.y) <= 1e9
        && std::isfinite(p.radius) && p.radius > 0 && p.radius <= 1e6
        && p.depth >= SurveyLevel::OrbitalSurvey && p.depth <= SurveyLevel::DeepSurvey
        && p.flight <= ProbeFlightMode::Economical && p.maxWarp >= 1 && p.maxWarp <= kMaxWarp
        && std::isfinite(p.fuelReserve) && p.fuelReserve >= 0 && p.fuelReserve <= 1e9
        && p.maxSystems > 0 && p.maxSystems <= 1000 && p.maxTurns > 0 && p.maxTurns <= 10000;
}
bool probe_capable(const GameState& s, const Fleet& f)
{
    if (f.outpost != OutpostKind::None || !fleet_has_scanner(s, f)) return false;
    const auto stacks = fleet_ship_stacks(f); if (stacks.empty()) return false;
    for (const auto& stack : stacks) {
        const auto* d = find_ship_design(s, stack.design);
        if (!d || d->hull != ShipHullType::SurveyProbe || !ship_design_valid(*d)
            || std::find(d->components.begin(), d->components.end(), ShipComponentType::SurveyAutopilot) == d->components.end()) return false;
    }
    return true;
}
ShipDesign basic_survey_probe_design(PlayerId owner, ShipDesignId id)
{
    return {id, owner, "Autonomous Survey Probe", ShipHullType::SurveyProbe,
        {ShipComponentType::QuickJump5, ShipComponentType::MoleScanner, ShipComponentType::SurveyAutopilot}};
}
SurveyLevel probe_survey_level(const Fleet& f, StarId star)
{
    for (const auto& entry : f.probe.surveys) if (entry.star == star) return entry.level;
    return SurveyLevel::Detected;
}
void record_probe_survey(Fleet& f, StarId star, SurveyLevel level, std::uint64_t turn)
{
    auto& knowledge = f.probe.surveys;
    const auto existing = std::find_if(knowledge.begin(), knowledge.end(), [&](const auto& k) { return k.star == star; });
    if (existing == knowledge.end()) knowledge.push_back({star, level, turn});
    else if (level > existing->level || (level == existing->level && turn > existing->observedTurn)) *existing = {star, level, turn};
    if (f.probe.target == star && level >= f.probe.program.depth
        && std::find(f.probe.completed.begin(), f.probe.completed.end(), star) == f.probe.completed.end())
        f.probe.completed.push_back(star);
}
void pause_probe(Fleet& f, ProbeStatus status)
{ if (f.probe.configured) { f.probe.enabled = false; f.probe.status = status; } }
void combine_probe_knowledge(Fleet& into, const Fleet& from)
{
    // Reorganization is not a new autonomous launch. Keep the surviving
    // programme, union only knowledge physically present at the rendezvous.
    if (!into.probe.configured && from.probe.configured) into.probe = from.probe;
    for (const auto& k : from.probe.surveys) record_probe_survey(into, k.star, k.level, k.observedTurn);
    into.visitor = {};
    pause_probe(into);
}
void apply_probe_command(const GameState& s, Fleet& f, const ProbeCommand& command)
{
    if (command.action == ProbeAction::Stop) { f.visitor = {}; clear_route(f); pause_probe(f, ProbeStatus::Stopped); return; }
    if (command.action == ProbeAction::Configure) {
        if (!probe_program_valid(command.program) || !probe_capable(s, f)) return;
        auto notebook = std::move(f.probe.surveys);
        f.probe = {}; f.probe.configured = true; f.probe.program = command.program;
        f.probe.surveys = std::move(notebook);
        f.probe.origin = f.position; f.probe.startedTurn = s.turn;
    }
    if (!f.probe.configured) return;
    // Delivered updates never read the current HQ database at reception.
    for (const auto& k : command.knowledge) record_probe_survey(f, k.star, k.level, k.observedTurn);
    if (command.action == ProbeAction::Configure || command.action == ProbeAction::Resume) {
        if (!probe_capable(s, f)) { pause_probe(f, ProbeStatus::MissingEquipment); return; }
        clear_route(f); f.visitor = {}; f.probe.enabled = true; f.probe.status = ProbeStatus::Surveying;
    }
}
bool submit_probe_command(GameState& s, PlayerId owner, const SetProbeProgramOrder& order)
{
    const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const auto& f) { return f.id == order.fleet && f.owner == owner; });
    if (f == s.fleets.end() || order.action > ProbeAction::UpdateKnowledge || !probe_program_valid(order.program)) return false;
    if (order.action != ProbeAction::Stop && !probe_capable(s, *f)) return false;
    ProbeCommand command{order.action, order.program, {}};
    const auto* player = find_player(s, owner);
    for (const auto& k : player->surveyKnowledge) if (k.observedTurn <= s.turn) command.knowledge.push_back({k.star, k.level, k.observedTurn});
    record_command_transmission(s, owner, f->position, s.turn);
    const auto delivery = s.turn + communication_delay_turns(s, owner, f->position);
    if (delivery == s.turn) { apply_probe_command(s, *f, command); if (order.action == ProbeAction::Stop) f->pendingCommands.clear(); }
    else { PendingFleetCommand packet; packet.issuedTurn = s.turn; packet.deliveryTurn = delivery; packet.probe = std::move(command); f->pendingCommands.push_back(std::move(packet)); }
    return true;
}
void advance_survey_probes(GameState& s)
{
    // Player exports/navigation projections carry only a dated summary. They
    // cannot simulate future decisions without the private onboard notebook.
    if (s.mediumObserver) return;
    for (auto& f : s.fleets) {
        auto& m = f.probe; if (!m.enabled) continue;
        if (!probe_capable(s, f)) { clear_route(f); pause_probe(f, ProbeStatus::MissingEquipment); continue; }
        if (f.damagePercent >= 25) { clear_route(f); m.status = ProbeStatus::Damaged; continue; }
        if (m.status == ProbeStatus::Returning) {
            if (same_position(f.position, m.origin)) { clear_route(f); m.enabled = false; m.status = ProbeStatus::Complete; }
            else if (const auto warp = flight_warp(s, f, m.origin, true)) { f.destination = m.origin; f.warp = *warp; }
            else { clear_route(f); m.status = ProbeStatus::AwaitingFuel; }
            continue;
        }
        if (m.completed.size() >= m.program.maxSystems || s.turn - m.startedTurn >= m.program.maxTurns) { return_or_hold(s, f, false); continue; }
        if (m.target) {
            const auto* target = find_star(s, m.target);
            if (target && probe_survey_level(f, m.target) < m.program.depth) {
                if (same_position(f.position, target->position)) { clear_route(f); m.status = ProbeStatus::Surveying; continue; }
                if (const auto warp = flight_warp(s, f, target->position, false)) { f.destination = target->position; f.warp = *warp; m.status = ProbeStatus::Travelling; continue; }
                return_or_hold(s, f, true); continue;
            }
            m.target = 0;
        }
        const StarSystem* best = nullptr; double bestDistance = std::numeric_limits<double>::infinity(); std::uint8_t bestWarp{};
        bool unfinished = false;
        // Stellar coordinates are public chart data. No planet, other fleet,
        // pending survey, mineral, object or station state enters this choice.
        std::vector<const StarSystem*> candidates;
        for (const auto& star:s.stars) candidates.push_back(&star);
        std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b) {
            const auto da=distance_between(f.position,a->position),db=distance_between(f.position,b->position);
            return da<db || (da==db && a->id<b->id);
        });
        for (const auto* candidate : candidates) {
            const auto& star=*candidate;
            if (distance_between(star.position, m.program.center) > m.program.radius || probe_survey_level(f, star.id) >= m.program.depth) continue;
            unfinished = true; const auto warp = flight_warp(s, f, star.position, false); if (!warp) continue;
            const auto distance = distance_between(f.position, star.position);
            if (!best || distance < bestDistance || (distance == bestDistance && star.id < best->id)) { best = &star; bestDistance = distance; bestWarp = *warp; break; }
        }
        if (!best) { return_or_hold(s, f, unfinished); continue; }
        clear_route(f); m.target = best->id; f.warp = bestWarp;
        if (same_position(f.position, best->position)) m.status = ProbeStatus::Surveying;
        else { f.destination = best->position; m.status = ProbeStatus::Travelling; }
    }
}
std::string probe_status_name(ProbeStatus status)
{
    switch (status) {
    case ProbeStatus::Idle: return "No programme";
    case ProbeStatus::Travelling: return "Travelling";
    case ProbeStatus::Surveying: return "Surveying";
    case ProbeStatus::Returning: return "Returning to launch position";
    case ProbeStatus::Complete: return "Mission complete; explicit resume required";
    case ProbeStatus::AwaitingFuel: return "Awaiting fuel / return route unavailable";
    case ProbeStatus::Damaged: return "Holding for repair (damage >=25%)";
    case ProbeStatus::MissingEquipment: return "Autonomy stopped: equipment lost";
    case ProbeStatus::ManualPause: return "Autonomy paused by manual control/reorganization";
    case ProbeStatus::Stopped: return "Stopped by delivered command";
    }
    return "Unknown";
}
}
