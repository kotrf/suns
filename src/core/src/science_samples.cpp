#include "suns/science_samples.hpp"
#include "suns/communications.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/unknown_objects.hpp"
#include <algorithm>

namespace suns {
double science_sample_mass(std::span<const ScienceSample> samples)
{ return samples.size() * kScienceSampleMass; }
const char* science_sample_name(UnknownObjectKind kind)
{ return kind == UnknownObjectKind::AncientProbe ? "Ancient archive" : "Compact-matter specimen"; }
std::uint32_t science_sample_research_points(UnknownObjectKind kind)
{ return kind == UnknownObjectKind::AncientProbe ? 64U : 96U; }
bool sample_observation_received(const GameState& state, PlayerId owner, UnknownObjectId source)
{
    const auto* x = known_unknown_object(state, owner, source);
    return x && x->level == ObjectKnowledgeLevel::Studied && x->receivedTurn <= state.turn;
}
std::uint32_t retained_sample_research(const GameState& state, const Planet& colony)
{
    const auto* owner = find_player(state, colony.owner);
    if (!owner || !colony.population || !owner->technology.researchActive
        || owner->technology.researchAllocationPercent == 0) return 0;
    std::uint32_t result = 0;
    for (const auto& sample : colony.scienceSamples)
        if (sample.retained && sample_observation_received(state, owner->id, sample.source)
            && unknown_object_research_field(sample.kind) == owner->technology.focus) result += 4;
    return result;
}
namespace {
struct Store {
    std::vector<ScienceSample>* samples{};
    Position position;
    Fleet* fleet{};
};
std::optional<Store> store(GameState& state, PlayerId owner, CargoTransferEndpoint endpoint)
{
    if (bool(endpoint.fleet) == bool(endpoint.planet)) return {};
    if (endpoint.fleet) {
        const auto f = std::find_if(state.fleets.begin(), state.fleets.end(),
            [&](const auto& f) { return f.id == endpoint.fleet && f.owner == owner; });
        if (f == state.fleets.end() || !fleet_has_instant_link(state, *f)) return {};
        return Store{&f->scienceSamples, f->position, &*f};
    }
    const auto p = std::find_if(state.planets.begin(), state.planets.end(),
        [&](const auto& p) { return p.id == endpoint.planet && p.owner == owner && p.population; });
    if (p == state.planets.end()) return {};
    const auto* star = find_star(state, p->star);
    if (!star) return {};
    return Store{&p->scienceSamples, star->position, nullptr};
}
}
bool transfer_science_sample(GameState& state, PlayerId owner, const TransferScienceSampleOrder& order)
{
    const auto from = store(state, owner, order.source), to = store(state, owner, order.destination);
    if (!from || !to || from->samples == to->samples || !same_position(from->position, to->position)
        || to->samples->size() >= kMaximumScienceSamples) return false;
    const auto sample = std::find_if(from->samples->begin(), from->samples->end(),
        [&](const auto& s) { return s.source == order.sample; });
    if (sample == from->samples->end() || (to->fleet
        && fleet_cargo_used(state, *to->fleet) + kScienceSampleMass > fleet_cargo_capacity(state, *to->fleet) + 1e-6)) return false;
    auto moved = *sample; moved.retained = false;
    to->samples->push_back(moved); from->samples->erase(sample);
    if (order.destination.planet) {
        const auto p = std::find_if(state.planets.begin(), state.planets.end(),
            [&](const auto& p) { return p.id == order.destination.planet; });
        queue_player_report(state, owner, PlayerReportKind::SampleDelivered, to->position, state.turn + 1,
            p->star, p->id, order.source.fleet, 0, ProductionKind::Research, moved.source,
            unknown_object_research_field(moved.kind));
    }
    return true;
}
bool preview_science_sample_transfer(GameState& state, PlayerId owner, const TransferScienceSampleOrder& order)
{
    for (const auto endpoint : {order.source, order.destination}) {
        if (!endpoint.fleet) continue;
        const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(),
            [&](const auto& f) { return f.id == endpoint.fleet && f.owner == owner; });
        // Inspect the received stamp directly. Re-evaluating contact from the
        // projected position could synthesize "current" telemetry at an old orbit.
        if (fleet == state.fleets.end() || fleet->telemetry.observedTurn != state.turn) return false;
    }
    return transfer_science_sample(state, owner, order);
}
bool unload_science_samples(GameState& state, Planet& colony, Fleet& fleet, std::uint64_t turn)
{
    const auto* star = find_star(state, colony.star);
    if (colony.owner != fleet.owner || !colony.population
        || !star || !same_position(star->position, fleet.position)
        || colony.scienceSamples.size() + fleet.scienceSamples.size() > kMaximumScienceSamples) return false;
    for (auto sample : fleet.scienceSamples) {
        sample.retained = false; colony.scienceSamples.push_back(sample);
        queue_player_report(state, colony.owner, PlayerReportKind::SampleDelivered, fleet.position, turn,
            colony.star, colony.id, fleet.id, 0, ProductionKind::Research, sample.source,
            unknown_object_research_field(sample.kind));
    }
    fleet.scienceSamples.clear(); return true;
}
bool process_science_sample(GameState& state, PlayerId owner, const ProcessScienceSampleOrder& order)
{
    const auto colony = std::find_if(state.planets.begin(), state.planets.end(),
        [&](const auto& p) { return p.id == order.colony && p.owner == owner && p.population; });
    if (colony == state.planets.end()) return false;
    const auto sample = std::find_if(colony->scienceSamples.begin(), colony->scienceSamples.end(),
        [&](const auto& s) { return s.source == order.sample; });
    if (sample == colony->scienceSamples.end()) return false;
    if (order.retain) {
        if (sample->retained || !sample_observation_received(state, owner, sample->source)) return false;
        sample->retained = true;
        if (const auto* star = find_star(state, colony->star))
            queue_player_report(state, owner, PlayerReportKind::SampleRetained, star->position, state.turn + 1,
                star->id, colony->id, 0, 0, ProductionKind::Research, sample->source, unknown_object_research_field(sample->kind));
    } else {
        const auto* star = find_star(state, colony->star);
        if (!star) return false;
        queue_player_report(state, owner, PlayerReportKind::ScientificData, star->position, state.turn + 1,
            star->id, colony->id, 0, 0, ProductionKind::Research,
            science_sample_research_points(sample->kind), unknown_object_research_field(sample->kind));
        colony->scienceSamples.erase(sample);
    }
    return true;
}
void recover_science_samples(GameState& state, Fleet& fleet, std::uint64_t turn)
{
    if (fleet.task != FleetTask::RecoverSample || fleet.destination
        || !fleet_has_scanner(state, fleet) || !fleet_active_sensors(fleet, turn)) return;
    std::vector<UnknownObject*> objects;
    for (auto& object : state.unknownObjects) if (same_position(fleet.position, object.position)
        && sample_observation_received(state, fleet.owner, object.id)) objects.push_back(&object);
    std::sort(objects.begin(), objects.end(), [](auto a, auto b) { return a->id < b->id; });
    for (auto* object : objects) {
        if (object->sampleRecovered) continue;
        if (fleet.scienceSamples.size() >= kMaximumScienceSamples
            || fleet_cargo_used(state, fleet) + kScienceSampleMass > fleet_cargo_capacity(state, fleet) + 1e-6) {
            queue_player_report(state, fleet.owner, PlayerReportKind::SampleHoldFull, fleet.position,
                turn, 0, 0, fleet.id, 0, ProductionKind::Research, object->id);
            fleet.task = FleetTask::None; return;
        }
        fleet.scienceSamples.push_back({object->id, object->kind, false});
        object->sampleRecovered = true;
        queue_player_report(state, fleet.owner, PlayerReportKind::SampleRecovered, fleet.position,
            turn, 0, 0, fleet.id, 0, ProductionKind::Research, object->id,
            unknown_object_research_field(object->kind));
        fleet.task = FleetTask::None; return;
    }
    if (!objects.empty()) {
        queue_player_report(state, fleet.owner, PlayerReportKind::SampleDepleted, fleet.position,
            turn, 0, 0, fleet.id, 0, ProductionKind::Research, objects.front()->id);
        fleet.task = FleetTask::None;
    }
}
} // namespace suns
