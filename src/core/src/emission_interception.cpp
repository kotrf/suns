#include "suns/orbital_habitats.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cmath>

namespace suns {
namespace {
struct Receiver {
    PlayerId player{};
    Position position;
    FleetId fleet{};
    PlanetId colony{};
    double range{}, tachyon{};
};
std::vector<Receiver> receivers(const GameState& state,std::uint64_t turn)
{
    std::vector<Receiver> result;
    for(const auto& p:state.planets) if(p.owner && total_colony_population(p) && p.radar!=PlanetaryRadarLevel::None)
        if(const auto* star=find_star(state,p.star)) result.push_back({p.owner,star->position,0,p.id,
            2*planetary_radar_range(p.radar)*(1-sensor_interference(state,p.owner,star->position,turn)),0});
    for(const auto& f:state.fleets) if(fleet_has_scanner(state,f)) result.push_back({f.owner,f.position,f.id,0,
        2*fleet_sensor_range(state,f)*(1-sensor_interference(state,f.owner,f.position,turn)),fleet_tachyon_fraction(state,f)});
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
        if(a.player!=b.player) return a.player<b.player;
        if(a.colony!=b.colony) return a.colony>b.colony;
        return a.fleet<b.fleet;
    });
    return result;
}
void record(GameState& state,const Receiver& receiver,const EmissionPulse& pulse,Position area,
    std::uint32_t id,std::uint64_t turn)
{
    auto player=std::find_if(state.players.begin(),state.players.end(),[&](const auto& p){return p.id==receiver.player;});
    if(player==state.players.end()) return;
    const auto due=dispatch_report_delivery_turn(state,receiver.player,receiver.position,turn,receiver.fleet);
    const auto held=held_report_source(state,receiver.player,receiver.position,turn,receiver.fleet);
    const EmissionObservation observation{pulse.emittedTurn,pulse.channel};
    StrategicIntel intel{id,StrategicObjectKind::Emission,area,15,0,turn,due,0,
        std::uint8_t(pulse.channel),held,observation};
    const auto same=[&](const auto& old){return old.type==intel.type && old.id==id;};
    // An earlier received copy and a still-held duplicate are separate physical
    // reports; keep the earliest dispatch rather than replacing old evidence.
    if(std::none_of(player->strategicIntel.begin(),player->strategicIntel.end(),same)) {
        auto old=std::find_if(player->pendingStrategicIntel.begin(),player->pendingStrategicIntel.end(),same);
        if(old==player->pendingStrategicIntel.end()) player->pendingStrategicIntel.push_back(intel);
        else if(due<old->deliveryTurn) *old=intel;
    }
    PendingPlayerReport report; report.kind=PlayerReportKind::EmissionDetected;
    report.observedTurn=turn; report.deliveryTurn=due; report.transmissionFleet=held;
    report.position=receiver.position; report.contactPosition=area; report.quantity=15; report.emission=observation;
    const auto sameReport=[&](const auto& old) {
        return old.kind==report.kind && old.emission && old.emission->emittedTurn==pulse.emittedTurn
            && old.emission->channel==pulse.channel && same_position(old.contactPosition,area);
    };
    auto old=std::find_if(player->pendingPlayerReports.begin(),player->pendingPlayerReports.end(),sameReport);
    if(old==player->pendingPlayerReports.end()) player->pendingPlayerReports.push_back(report);
    else if(due<old->deliveryTurn) *old=report;
}
}
void observe_emission_pulses(GameState& state,std::uint64_t turn)
{
    std::erase_if(state.emissions,[&](const auto& p) {
        const auto lifetime=p.channel==SignalChannel::Fast?1ULL:
            std::uint64_t(std::ceil(p.maximumRange/kCommunicationSignalSpeed));
        return p.emittedTurn<turn && turn-p.emittedTurn>lifetime;
    });
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
            if(distance>std::min(receiver.range,pulse.maximumRange)+1e-6) continue;
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
                record(state,receiver,pulse,ghost,pulse.id*2+1,turn);
            }
            // Suppress only redundant contemporaneous hull evidence. An old
            // pulse never queries a departed/destroyed emitter's current truth.
            bool hullSeen=false;
            if(!age && pulse.fleet) {
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
                std::floor(pulse.position.y/20)*20+10},pulse.id*2,turn);
        }
    }
}
}
