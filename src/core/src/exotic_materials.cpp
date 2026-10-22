#include "suns/exotic_materials.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/mobile_base.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/science_samples.hpp"
#include "suns/unknown_objects.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace suns {
bool resonant_gate_technology_available(const GameState& s, PlayerId owner)
{
    return technology_level(s, owner, ResearchField::Energy) >= 18
        && technology_level(s, owner, ResearchField::Electronics) >= 18
        && technology_level(s, owner, ResearchField::Construction) >= 16;
}
namespace {
std::uint64_t mix(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31);
}
struct Store { std::uint32_t* amount; Position position; Fleet* fleet; Planet* planet; };
std::optional<Store> store(GameState& s, PlayerId owner, CargoTransferEndpoint endpoint, bool commandDelivered)
{
    if(endpoint.orbital) return std::nullopt;
    if (bool(endpoint.planet) == bool(endpoint.fleet)) return {};
    if (endpoint.fleet) {
        auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const auto& f) { return f.id == endpoint.fleet && f.owner == owner; });
        if (f == s.fleets.end() || (!commandDelivered && !fleet_has_instant_link(s, *f))) return {};
        if (mobile_base_hulls(s, *f) && !mobile_base_service_error(s, *f, s.turn + 1).empty()) return {};
        return Store{&f->xenocrystal, f->position, &*f, nullptr};
    }
    auto p = std::find_if(s.planets.begin(), s.planets.end(), [&](const auto& p) { return p.id == endpoint.planet; });
    if (p == s.planets.end() || (p->owner && p->owner != owner) || !known_xenocrystal(s, owner, p->id)) return {};
    const auto* star = find_star(s, p->star); if (!star) return {};
    return Store{&p->xenocrystal.surface, star->position, nullptr, &*p};
}
}
std::size_t generate_xenocrystal_deposits(GameState& s)
{
    for (auto& p : s.planets) p.xenocrystal = {};
    std::vector<Planet*> candidates;
    for (auto& p : s.planets) {
        const auto* star = find_star(s, p.star);
        if (!p.homeworld && !p.owner && star && std::isfinite(star->position.x) && std::isfinite(star->position.y))
            candidates.push_back(&p);
    }
    if (candidates.empty()) return 0;
    std::sort(candidates.begin(), candidates.end(), [](auto a, auto b) { return a->id < b->id; });
    const auto seed = mix(s.galaxySeed ^ 0x58454e4f43525953ULL);
    const auto budget = std::min(candidates.size(), s.planets.size() / 32 + (s.planets.size() % 32 != 0));
    // round_half_up(2 * systems / 288), without multiplying a size_t.
    const auto target = std::min(budget, std::max<std::size_t>(1,
        s.stars.size() / 144 + (s.stars.size() % 144 >= 72)));
    const auto separation = [&](const Planet* a, const Planet* b) {
        return distance_between(find_star(s, a->star)->position, find_star(s, b->star)->position);
    };
    std::vector<Planet*> centers{candidates[seed % candidates.size()]};
    while (centers.size() < target) {
        Planet* best = nullptr; double farthest = 0;
        for (auto* p : candidates) {
            double nearest = std::numeric_limits<double>::infinity();
            for (const auto* center : centers) nearest = std::min(nearest, separation(p, center));
            if (nearest > farthest) { best = p; farthest = nearest; }
        }
        if (!best) break; // Coincident worlds cannot form distinct geographic centers.
        centers.push_back(best);
    }
    std::vector<std::vector<Planet*>> regions(centers.size());
    for (auto* p : candidates) {
        std::size_t closest = 0;
        for (std::size_t i = 1; i < centers.size(); ++i)
            if (separation(p, centers[i]) < separation(p, centers[closest])) closest = i;
        regions[closest].push_back(p);
    }
    std::vector<std::size_t> allocation(centers.size()); std::size_t assigned = 0;
    for (std::size_t i = 0; i < centers.size(); ++i) {
        auto& region = regions[i];
        std::sort(region.begin(), region.end(), [&](auto a, auto b) {
            const auto da = separation(a, centers[i]), db = separation(b, centers[i]);
            return da != db ? da < db : a->id < b->id;
        });
        allocation[i] = std::min(region.size(), budget / centers.size() + (i < budget % centers.size()));
        assigned += allocation[i];
    }
    // Undersized regions' quotas move to regions with room. Every pass makes
    // progress because budget never exceeds the total candidate count.
    while (assigned < budget) for (std::size_t i = 0; i < centers.size() && assigned < budget; ++i)
        if (allocation[i] < regions[i].size()) { ++allocation[i]; ++assigned; }
    for (std::size_t i = 0; i < centers.size(); ++i) for (std::size_t j = 0; j < allocation[i]; ++j) {
        auto* p = regions[i][j]; const auto amount = std::uint32_t(24 + mix(seed ^ p->id) % 25);
        p->xenocrystal = {amount, amount, 0, 0};
    }
    return centers.size();
}
std::optional<XenocrystalObservation> known_xenocrystal(const GameState& s, PlayerId owner, PlanetId id)
{
    const auto* p = find_planet(s, id); const auto* player = find_player(s, owner);
    if (!p || !player) return {};
    for (const auto& k : player->surveyKnowledge) if (k.star == p->star && k.planetSnapshot)
        return k.planetSnapshot->xenocrystal;
    return {};
}
bool exotic_mining_available(const GameState& s, PlayerId owner)
{ return technology_level(s, owner, ResearchField::Electronics) >= 8 && technology_level(s, owner, ResearchField::Construction) >= 8; }
std::uint32_t xenocrystal_mining_rate(const GameState& s, const Fleet& f)
{
    if (!exotic_mining_available(s, f.owner)) return 0;
    std::uint32_t result = 0;
    for (const auto& stack : fleet_ship_stacks(f)) {
        const auto* design = find_ship_design(s, stack.design);
        if (design && ship_design_available_to_player(s, f.owner, *design) && ship_design_can_remote_mine(*design))
            result += std::min(stack.count, kXenocrystalPerPlanetYear - result);
    }
    return result;
}
void observe_xenocrystal_surface(GameState& s, PlayerId owner, PlanetId id, FleetId source, std::uint64_t turn, bool finalSurfaceObservation)
{
    const auto* p = find_planet(s, id); if (!p) return;
    const auto known = known_xenocrystal(s, owner, id);
    if (!known && p->owner != owner) return;
    const auto* star = find_star(s, p->star); if (!star) return;
    const bool deep = known && known->depositSurveyed;
    // Surface-only observations need a payload too; final marking belongs only
    // to the post-production phase, never to a transfer earlier in the year.
    queue_survey_report(s, owner, p->star, source, star->position, turn, deep ? SurveyLevel::DeepSurvey : SurveyLevel::OrbitalSurvey, finalSurfaceObservation);
}
std::uint32_t colony_xenocrystal_mining_rate(const GameState& s, const Planet& p)
{
    const auto known = known_xenocrystal(s, p.owner, p.id);
    if (!p.owner || !p.population || !exotic_mining_available(s, p.owner)
        || !known || !known->depositSurveyed || !known->site.original) return 0;
    return std::min(kXenocrystalPerPlanetYear, p.mines / 25);
}
std::string known_colony_xenocrystal_block(const GameState& s, PlayerId owner, const Planet& p)
{
    if (p.owner != owner) return "Own colony required";
    const auto known = known_xenocrystal(s, owner, p.id);
    if (!known || !known->depositSurveyed) return "Awaiting delivered DeepSurvey";
    if (!known->site.original) return "No surveyed deposit";
    if (!known->colony || known->colony->owner != owner) return "Awaiting private colony mining report";
    if (!known->colony->population) return "No received population";
    if (!exotic_mining_available(s, owner)) return "Requires Construction 8 and Electronics 8";
    if (known->colony->mines < 25) return "Requires 25 operating mines per unit/year";
    if (!known->site.remaining) return "Known deposit exhausted";
    if (p.xenocrystal.surface >= kMaximumXenocrystal) return "Received surface cache full";
    return {};
}
std::uint32_t known_colony_xenocrystal_rate(const GameState& s, PlayerId owner, const Planet& p)
{
    if (!known_colony_xenocrystal_block(s, owner, p).empty()) return 0;
    const auto known = *known_xenocrystal(s, owner, p.id);
    return std::min({kXenocrystalPerPlanetYear, known.colony->mines / 25,
        known.site.remaining, kMaximumXenocrystal - p.xenocrystal.surface});
}
void mine_xenocrystals(GameState& s)
{
    // One annual phase before movement/production. Later ownership changes
    // cannot re-enter either branch or reset the shared allowance.
    for (auto& p : s.planets) {
        p.xenocrystal.extractedLastYear = 0;
        if (!p.xenocrystal.remaining) continue;
        const auto* star = find_star(s, p.star); if (!star) continue;
        const auto extract = [&](PlayerId owner, FleetId source, std::uint32_t rate) {
            const auto mined = std::min({rate, p.xenocrystal.remaining,
                kXenocrystalPerPlanetYear - p.xenocrystal.extractedLastYear, kMaximumXenocrystal - p.xenocrystal.surface});
            p.xenocrystal.remaining -= mined; p.xenocrystal.surface += mined; p.xenocrystal.extractedLastYear += mined;
            if (mined && !p.xenocrystal.remaining)
                queue_player_report(s, owner, PlayerReportKind::XenocrystalDepleted, star->position, s.turn + 1,
                    p.star, p.id, source, 0, ProductionKind::Research, p.xenocrystal.surface);
        };
        if (p.owner) { extract(p.owner, 0, colony_xenocrystal_mining_rate(s, p)); continue; }
        for (const auto& f : s.fleets) {
            if (f.task != FleetTask::RemoteMining || f.destination || !same_position(f.position, star->position)) continue;
            const auto known = known_xenocrystal(s, f.owner, p.id);
            if (!known || !known->depositSurveyed || !known->site.original) continue;
            extract(f.owner, f.id, xenocrystal_mining_rate(s, f));
        }
    }
}
bool move_xenocrystals(std::uint32_t& from, std::uint32_t& to, std::uint32_t amount)
{
    if (!amount || &from == &to || amount > from || to > kMaximumXenocrystal || amount > kMaximumXenocrystal - to) return false;
    from -= amount; to += amount; return true;
}
bool transfer_xenocrystals(GameState& s, PlayerId owner, const TransferXenocrystalOrder& order, bool commandDelivered)
{
    auto from = store(s, owner, order.source, commandDelivered), to = store(s, owner, order.destination, commandDelivered);
    if (!from || !to || !same_position(from->position, to->position)) return false;
    if (to->fleet && fleet_cargo_used(s, *to->fleet) + order.amount > fleet_cargo_capacity(s, *to->fleet) + 1e-6) return false;
    if (!move_xenocrystals(*from->amount, *to->amount, order.amount)) return false;
    const auto sourceFleet = from->fleet ? from->fleet->id : to->fleet ? to->fleet->id : 0;
    queue_player_report(s, owner, PlayerReportKind::XenocrystalAcquired, to->position, s.turn + 1,
        to->planet ? to->planet->star : 0, to->planet ? to->planet->id : 0, sourceFleet, 0, ProductionKind::Research, order.amount);
    return true;
}
bool preview_xenocrystal_transfer(GameState& s, PlayerId owner, const TransferXenocrystalOrder& order, const PlayerOrders* pending)
{
    const auto isBase = [&](FleetId id) {
        const auto* f = find_service_fleet(s, id);
        return f && mobile_base_hulls(s, *f) > 0;
    };
    if (pending && (isBase(order.source.fleet) || isBase(order.destination.fleet))) {
        // A dockside estimate cannot spend cargo before the later service
        // phase. Either participant's departure invalidates that estimate.
        for (const auto& command : pending->orders) {
            if (const auto* move = std::get_if<MoveFleetOrder>(&command); move && !move->clearRoute
                && (move->fleet == order.source.fleet || move->fleet == order.destination.fleet)) return false;
            if (const auto* gate = std::get_if<GateFleetOrder>(&command); gate
                && (gate->fleet == order.source.fleet || gate->fleet == order.destination.fleet)) return false;
        }
        auto projected = s;
        for (const auto endpoint : {order.source, order.destination}) if (isBase(endpoint.fleet)) {
            auto& base = *std::find_if(projected.fleets.begin(), projected.fleets.end(),
                [&](const auto& fleet) { return fleet.id == endpoint.fleet; });
            for (const auto& command : pending->orders)
                if (const auto* control = std::get_if<SetMobileBaseOrder>(&command); control && control->fleet == base.id
                    && mobile_base_program_valid(control->program))
                    base.mobileBase = control->program;
        }
        if (!preview_xenocrystal_transfer(projected, owner, order)) return false;
        s = std::move(projected); return true;
    }
    for (const auto endpoint : {order.source, order.destination}) {
        if (endpoint.fleet) {
            const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const auto& f) { return f.id == endpoint.fleet && f.owner == owner; });
            if (f == s.fleets.end() || f->telemetry.observedTurn != s.turn) return false;
        } else if (endpoint.planet) {
            const auto known = known_xenocrystal(s, owner, endpoint.planet);
            if (!known || known->observedTurn != s.turn) return false;
            const auto* p = find_planet(s, endpoint.planet);
            if (p && p->owner == owner && !known->stockPhase) return false;
        }
    }
    return transfer_xenocrystals(s, owner, order);
}
bool resonance_technology_available(const GameState& s, PlayerId owner)
{
    return technology_level(s, owner, ResearchField::Energy) >= 16
        && technology_level(s, owner, ResearchField::Electronics) >= 16
        && technology_level(s, owner, ResearchField::Construction) >= 14;
}
bool resonance_scheduled(const Planet& p)
{ return std::any_of(p.productionQueue.begin(), p.productionQueue.end(), [](const auto& i) { return i.kind == ProductionKind::ResonanceLaboratory; }); }
bool resonance_operational(const GameState& s, const Planet& p)
{ return p.resonanceLab.started && p.resonanceLab.work == kResonanceWork && p.resonanceLab.active
    && p.owner && p.population && resonance_technology_available(s, p.owner) && engineering_station_available(s, p); }
std::string resonance_block(const GameState& s, const Planet& p)
{
    const auto& lab = p.resonanceLab;
    if (!lab.started) return "No project";
    if (!p.owner || !p.population) return "Populated colony required";
    if (!lab.active || (lab.work < kResonanceWork && !resonance_scheduled(p))) return "Paused";
    if (!resonance_technology_available(s, p.owner)) return "Requires Energy 16, Electronics 16 and Construction 14";
    if (!engineering_station_available(s, p)) return "Requires operational friendly Starbase/Fortress with shipyard";
    if (lab.work == kResonanceWork) return "Operational: artifact research in parallel fields";
    if (!lab.crystalsPaid && p.xenocrystal.surface < kResonanceCrystals) return "Awaiting 8 physically delivered xenocrystals";
    if (p.minerals.ironium + 1e-9 < 0.2 || p.minerals.boranium + 1e-9 < 0.1 || p.minerals.germanium + 1e-9 < 0.2) return "Awaiting ordinary construction minerals";
    return "Building; up to 60 work/year from the production queue";
}
bool apply_resonance_order(const GameState& s, Planet& p, const SetResonanceLaboratoryOrder& order)
{
    if (p.id != order.colony || !p.owner || !p.population || order.action > ResonanceAction::SetActive) return false;
    auto& lab = p.resonanceLab;
    const auto pause = [&] { std::erase_if(p.productionQueue, [](const auto& i) { return i.kind == ProductionKind::ResonanceLaboratory; }); };
    if (order.action == ResonanceAction::Dismantle) { if (!lab.started) return false; pause(); lab = {}; return true; }
    if (order.action == ResonanceAction::Pause) { if (!lab.started) return false; pause(); lab.active = false; lab.operational = false; return true; }
    if (order.action == ResonanceAction::SetActive) { if (!lab.started) return false; lab.active = order.active; lab.operational = resonance_operational(s, p); return true; }
    if (!resonance_technology_available(s, p.owner) || lab.work == kResonanceWork || resonance_scheduled(p)) return false;
    if (!lab.started) { lab.started = true; lab.builder = p.owner; lab.startedTurn = s.turn; }
    lab.active = true;
    p.productionQueue.push_back({ProductionKind::ResonanceLaboratory, kResonanceWork - lab.work}); return true;
}
std::uint32_t advance_resonance_laboratory(const GameState& s, Planet& p, std::uint32_t available, bool supported, std::uint32_t annualRemaining)
{
    auto& lab = p.resonanceLab;
    if (!lab.started || !lab.active || lab.work >= kResonanceWork || !p.owner || !p.population
        || !supported || !resonance_technology_available(s, p.owner)
        || (!lab.crystalsPaid && p.xenocrystal.surface < kResonanceCrystals)) return 0;
    const auto affordable = [](double stock, double rate) { return std::uint32_t(std::clamp(std::floor(std::max(0.0, stock) / rate + 1e-8), 0.0, double(kResonanceWorkPerYear))); };
    const auto work = std::min({available, annualRemaining, kResonanceWork - lab.work,
        affordable(p.minerals.ironium, 0.2), affordable(p.minerals.boranium, 0.1), affordable(p.minerals.germanium, 0.2)});
    if (!work) return 0;
    if (!lab.crystalsPaid) { p.xenocrystal.surface -= kResonanceCrystals; lab.crystalsPaid = kResonanceCrystals; }
    const auto pay = [&](double& stock, double& invested, double rate) { const auto cost = work * rate; stock = std::max(0.0, stock - cost); invested += cost; };
    pay(p.minerals.ironium, lab.invested.ironium, 0.2); pay(p.minerals.boranium, lab.invested.boranium, 0.1); pay(p.minerals.germanium, lab.invested.germanium, 0.2);
    lab.work += work; if (lab.work == kResonanceWork) lab.completedTurn = s.turn + 1;
    return work;
}
std::array<std::uint32_t, kResearchFieldCount> resonance_research(const GameState& s, const Planet& p)
{
    std::array<std::uint32_t, kResearchFieldCount> result{};
    const auto* owner = find_player(s, p.owner);
    if (!owner || !resonance_operational(s, p) || !owner->technology.researchActive || !owner->technology.researchAllocationPercent) return result;
    for (const auto& sample : p.scienceSamples) {
        const auto field = unknown_object_research_field(sample.kind);
        if (sample.retained && field != owner->technology.focus && sample_observation_received(s, p.owner, sample.source))
            result[std::size_t(field)] += kResonanceResearchPerArtifact;
    }
    return result;
}
bool resonance_state_valid(const ResonanceLaboratory& lab)
{
    if (lab.work > kResonanceWork || (lab.crystalsPaid != 0 && lab.crystalsPaid != kResonanceCrystals)) return false;
    const auto validMineral = [&](double amount, double rate) { return std::isfinite(amount) && std::abs(amount - lab.work * rate) < 1e-5; };
    if (!validMineral(lab.invested.ironium, 0.2) || !validMineral(lab.invested.boranium, 0.1) || !validMineral(lab.invested.germanium, 0.2)) return false;
    if (!lab.started) return !lab.work && !lab.crystalsPaid && !lab.operational && !lab.builder && !lab.startedTurn && !lab.completedTurn;
    return lab.builder && lab.startedTurn && (bool(lab.work) == bool(lab.crystalsPaid))
        && (lab.work == kResonanceWork ? lab.completedTurn >= lab.startedTurn : !lab.completedTurn)
        && (!lab.operational || (lab.work == kResonanceWork && lab.active));
}
void publish_exotic_material_reports(GameState& s, const GameState& previous)
{
    for (auto& p : s.planets) {
        p.resonanceLab.operational = resonance_operational(s, p);
        const auto* star = find_star(s, p.star); if (!star) continue;
        if (p.owner) observe_xenocrystal_surface(s, p.owner, p.id, 0, s.turn + 1, true);
        for (const auto& f : s.fleets) if (same_position(f.position, star->position))
            observe_xenocrystal_surface(s, f.owner, p.id, f.id, s.turn + 1, true);
        const auto* old = find_planet(previous, p.id);
        if (p.owner && old && (p.resonanceLab.work != old->resonanceLab.work
            || resonance_block(s, p) != resonance_block(previous, *old) || p.owner != old->owner)) {
            // Work is 0..600; 601..608 encode an immutable crystal shortage.
            const auto quantity = resonance_block(s, p) == "Awaiting 8 physically delivered xenocrystals"
                ? kResonanceWork + kResonanceCrystals - p.xenocrystal.surface : p.resonanceLab.work;
            queue_player_report(s, p.owner, PlayerReportKind::ResonanceProjectStatus, star->position, s.turn + 1,
                p.star, p.id, 0, 0, ProductionKind::ResonanceLaboratory, quantity);
        }
    }
}
} // namespace suns
