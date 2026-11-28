#include "suns/orbital_habitats.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace suns {
namespace {
struct Receiver {
    PlayerId player{};
    Position position;
    FleetId fleet{};
    PlanetId colony{};
    double range{}, tachyon{};
    bool canClassify{};
};
std::vector<Receiver> receivers(const GameState& state,std::uint64_t turn)
{
    const auto classify = [&](PlayerId player) { return technology_level(state, player, ResearchField::Electronics) >= 8; };
    std::vector<Receiver> result;
    for(const auto& p:state.planets) if(p.owner && total_colony_population(p) && p.radar!=PlanetaryRadarLevel::None)
        if(const auto* star=find_star(state,p.star)) result.push_back({p.owner,star->position,0,p.id,
            2*planetary_radar_range(p.radar)*(1-sensor_interference(state,p.owner,star->position,turn)),0,classify(p.owner)});
    for(const auto& f:state.fleets) if(fleet_has_scanner(state,f)) result.push_back({f.owner,f.position,f.id,0,
        2*fleet_sensor_range(state,f)*(1-sensor_interference(state,f.owner,f.position,turn)),fleet_tachyon_fraction(state,f),classify(f.owner)});
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
        if(a.player!=b.player) return a.player<b.player;
        if(a.colony!=b.colony) return a.colony>b.colony;
        return a.fleet<b.fleet;
    });
    return result;
}

bool same_emission_report(const PendingPlayerReport& a, const PendingPlayerReport& b)
{
    return a.kind == PlayerReportKind::EmissionDetected && b.kind == a.kind && a.emission && b.emission
        && a.emission->emittedTurn == b.emission->emittedTurn && a.emission->channel == b.emission->channel
        && same_position(a.contactPosition, b.contactPosition);
}

bool classified(const auto& evidence)
{
    return evidence.emission && evidence.emission->activity != SignalActivity::Unclassified;
}

bool dominates(const auto& earlier, const auto& later)
{
    return earlier.deliveryTurn <= later.deliveryTurn && classified(earlier) >= classified(later);
}

bool within_pulse_lifetime(const EmissionPulse& pulse, std::uint64_t turn)
{
    const auto lifetime = pulse.channel == SignalChannel::Fast ? 1ULL
        : std::uint64_t(std::ceil(pulse.maximumRange / kCommunicationSignalSpeed));
    return turn <= pulse.emittedTurn || turn - pulse.emittedTurn <= lifetime;
}

auto propagating_emissions(const GameState& state)
{
    std::set<std::pair<std::uint64_t, SignalChannel>> emissions;
    for (const auto& pulse : state.emissions)
        if (pulse.emittedTurn <= state.turn && within_pulse_lifetime(pulse, state.turn))
            emissions.emplace(pulse.emittedTurn, pulse.channel);
    return emissions;
}

void remember_emission(Player& player, const StrategicIntel& evidence)
{
    const auto known = std::find_if(player.strategicIntel.begin(), player.strategicIntel.end(), [&](const auto& old) {
        return same_strategic_intel_subject(old, evidence);
    });
    if (known == player.strategicIntel.end()) player.strategicIntel.push_back(evidence);
    else if (classified(evidence) > classified(*known)) *known = evidence;
}

template<class Evidence, class SameSubject>
void stage(std::vector<Evidence>& queue, const Evidence& evidence, SameSubject same)
{
    // Different outboxes can send their copies in either order. Compare
    // scheduled deliveries or observations aboard the same ship.
    const auto comparable = [&](const auto& other) {
        return evidence.transmissionFleet == other.transmissionFleet && same(evidence, other);
    };
    if (std::any_of(queue.begin(), queue.end(), [&](const auto& other) {
            return comparable(other) && dominates(other, evidence);
        })) return;
    std::erase_if(queue, [&](const auto& other) {
        return comparable(other) && dominates(evidence, other);
    });
    queue.push_back(evidence);
}

template<class Evidence, class SameSubject>
void discard_redundant_arrivals(std::vector<Evidence>& queue, std::uint64_t turn, SameSubject same)
{
    std::vector<std::size_t> arrived;
    for (std::size_t i = 0; i < queue.size(); ++i)
        if (queue[i].emission && queue[i].deliveryTurn <= turn) arrived.push_back(i);
    if (arrived.empty()) return;

    // Once a copy arrives it can retire equivalent and weaker copies still
    // in flight or aboard a different receiver. Preserve later refinements.
    std::vector<bool> redundant(queue.size());
    for (std::size_t i = 0; i < queue.size(); ++i) {
        const auto& item = queue[i];
        if (!item.emission) continue;
        redundant[i] = std::any_of(arrived.begin(), arrived.end(), [&](std::size_t j) {
            const auto& copy = queue[j];
            return j != i && same(copy, item) && dominates(copy, item)
                && (copy.deliveryTurn < item.deliveryTurn || classified(copy) != classified(item) || j < i);
        });
    }
    std::size_t index{};
    std::erase_if(queue, [&](const auto&) { return redundant[index++]; });
}

void record(GameState& state, const Receiver& receiver, const EmissionPulse& pulse, Position area,
    std::uint64_t turn, SignalActivity activity)
{
    auto player=std::find_if(state.players.begin(),state.players.end(),[&](const auto& p){return p.id==receiver.player;});
    if(player==state.players.end()) return;
    const auto held=held_report_source(state,receiver.player,receiver.position,turn,receiver.fleet);
    const EmissionObservation observation{pulse.emittedTurn, pulse.channel, activity};
    StrategicIntel intel{0,StrategicObjectKind::Emission,area,15,0,turn,0,0,
        std::uint8_t(pulse.channel),held,observation};
    if (std::any_of(player->strategicIntel.begin(), player->strategicIntel.end(), [&](const auto& known) {
            return same_strategic_intel_subject(known, intel) && known.deliveryTurn <= turn
                && classified(known) >= classified(intel);
        })) return;
    const auto due = dispatch_report_delivery_turn(state, receiver.player, receiver.position, turn, receiver.fleet);
    intel.deliveryTurn = due;
    stage(player->pendingStrategicIntel, intel, same_strategic_intel_subject);

    PendingPlayerReport report; report.kind=PlayerReportKind::EmissionDetected;
    report.observedTurn=turn; report.deliveryTurn=due; report.transmissionFleet=held;
    report.position=receiver.position; report.contactPosition=area; report.quantity=15; report.emission=observation;
    stage(player->pendingPlayerReports, report, same_emission_report);
}
}

void deliver_emission_intel(GameState& state)
{
    const auto propagating = propagating_emissions(state);
    for (auto& player : state.players) {
        discard_redundant_arrivals(player.pendingStrategicIntel, state.turn, same_strategic_intel_subject);
        for (const auto& evidence : player.pendingStrategicIntel)
            if (evidence.type == StrategicObjectKind::Emission && evidence.deliveryTurn <= state.turn)
                remember_emission(player, evidence);
        // Markers are displayed in the receipt year. Keep the received
        // classification while distant observers can still sample the wave.
        std::erase_if(player.strategicIntel, [&](const auto& evidence) {
            return evidence.type == StrategicObjectKind::Emission && evidence.deliveryTurn < state.turn
                && (!evidence.emission
                    || !propagating.contains({evidence.emission->emittedTurn, evidence.emission->channel}));
        });
    }
}

void discard_redundant_emission_reports(Player& player, std::uint64_t turn)
{
    discard_redundant_arrivals(player.pendingPlayerReports, turn, same_emission_report);
    std::erase_if(player.pendingPlayerReports, [&](const PendingPlayerReport& report) {
        if (report.kind != PlayerReportKind::EmissionDetected || !report.emission) return false;
        const StrategicIntel reading{0, StrategicObjectKind::Emission, report.contactPosition, 15, 0, report.observedTurn,
            report.deliveryTurn, 0, std::uint8_t(report.emission->channel), report.transmissionFleet, report.emission};
        return std::any_of(player.strategicIntel.begin(), player.strategicIntel.end(), [&](const auto& known) {
            return known.deliveryTurn < report.deliveryTurn && known.deliveryTurn <= turn
                && same_strategic_intel_subject(known, reading) && dominates(known, reading);
        });
    });
}

void restore_emission_intel(GameState& state, std::span<const GameEvent> history)
{
    const auto propagating = propagating_emissions(state);
    if (propagating.empty()) return;
    for (const auto& event : history) {
        if (event.kind != GameEventKind::EmissionDetected || !event.emission || event.turn >= state.turn
            || !propagating.contains({event.emission->emittedTurn, event.emission->channel})) continue;
        const auto player = std::find_if(state.players.begin(), state.players.end(), [&](const auto& p) {
            return p.id == event.recipient;
        });
        if (player == state.players.end()) continue;
        remember_emission(*player, {0, StrategicObjectKind::Emission, event.position, 15, 0, event.observedTurn,
            event.turn, 0, std::uint8_t(event.emission->channel), 0, event.emission});
    }
}

void record_decoy_pulse(GameState& state, const Fleet& fleet, std::uint64_t turn)
{
    EmissionPulse pulse;
    pulse.id = state.nextEmissionId++;
    pulse.owner = fleet.owner;
    pulse.fleet = fleet.id;
    pulse.position = fleet.position;
    pulse.emittedTurn = turn;
    pulse.maximumRange = kDecoyPulseRangeLy;
    pulse.deceptionStrength = kDecoyPulseStrength;
    pulse.decoyOnly = true;
    pulse.activity = SignalActivity::RadioTraffic;
    state.emissions.push_back(std::move(pulse));
}

const char* signal_activity_name(SignalActivity activity)
{
    switch (activity) {
    case SignalActivity::RadioTraffic: return "radio traffic";
    case SignalActivity::ActiveElectronics: return "active scanners or jamming";
    case SignalActivity::Mixed: return "radio traffic and active electronics";
    case SignalActivity::Unclassified: return "unclassified";
    }
    return "unclassified";
}

void observe_emission_pulses(GameState& state,std::uint64_t turn)
{
    std::erase_if(state.emissions, [&](const auto& pulse) { return !within_pulse_lifetime(pulse, turn); });
    const auto sensors=receivers(state,turn);
    // Reports can create outgoing packet batches while this pass runs. Copy a
    // pulse before dispatch; the host vector may reallocate, and no new batch
    // can recursively create another batch at the same sender/year/channel.
    for(std::size_t index=0;index<state.emissions.size();++index) {
        const auto pulse=state.emissions[index];
        if(pulse.emittedTurn>turn) continue;
        const auto age=turn-pulse.emittedTurn;
        for(const auto& receiver:sensors) {
            if(receiver.player==pulse.owner || receiver.range<=0) continue;
            const auto distance=distance_between(pulse.position,receiver.position);
            const auto reach = std::min(receiver.range, pulse.maximumRange);
            if(distance>reach+1e-6) continue;
            const auto activity = receiver.canClassify && distance <= reach * 0.5
                ? pulse.activity : SignalActivity::Unclassified;
            if(pulse.channel==SignalChannel::Slow) {
                // A shell, not a permanently filled detection disk. Interception
                // happens before the observer's separate report propagation.
                if(distance>double(age)*kCommunicationSignalSpeed+1e-6
                    || (age && distance<=double(age-1)*kCommunicationSignalSpeed+1e-6)) continue;
            } else if(age>1) continue; // Annual planning can first sample an issue-year packet next boundary.
            auto& receipts=state.emissions[index].receipts;
            if(std::any_of(receipts.begin(),receipts.end(),[&](const auto& r) {
                return r.player==receiver.player && r.fleet==receiver.fleet && r.colony==receiver.colony;
            })) continue;
            receipts.push_back({receiver.player,receiver.fleet,receiver.colony});
            if(pulse.deceptionStrength>receiver.tachyon) {
                const Position ghost{std::floor((pulse.position.x+40)/20)*20+10,
                    std::floor((pulse.position.y+20)/20)*20+10};
                record(state,receiver,pulse,ghost,turn,activity);
            }
            if (pulse.decoyOnly) continue;
            // A hull sighting can replace an anonymous alert. A classified
            // signal also carries information about the emitter's activity.
            bool hullSeen=false;
            if(activity == SignalActivity::Unclassified && !age && pulse.fleet) {
                const auto target=std::find_if(state.fleets.begin(),state.fleets.end(),[&](const auto& f) {
                    return f.id==pulse.fleet && f.owner==pulse.owner && same_position(f.position,pulse.position);
                });
                if(target!=state.fleets.end()) {
                    if(receiver.colony) {
                        const auto colony=std::find_if(state.planets.begin(),state.planets.end(),[&](const auto& p){return p.id==receiver.colony;});
                        hullSeen=colony!=state.planets.end() && colony_detects_fleet(state,*colony,*target,turn);
                    } else {
                        const auto fleet=std::find_if(state.fleets.begin(),state.fleets.end(),[&](const auto& f){return f.id==receiver.fleet;});
                        hullSeen=fleet!=state.fleets.end() && detector_detects_fleet(state,*fleet,*target,turn);
                    }
                }
            }
            if(!hullSeen) record(state,receiver,pulse,{std::floor(pulse.position.x/20)*20+10,
                std::floor(pulse.position.y/20)*20+10},turn,activity);
        }
    }
}
}
