#include "suns/communications.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/campaign.hpp"
#include <cassert>
#include <algorithm>
using namespace suns;
using C=ShipComponentType;
GameState fixture()
{
    GameState s; s.turn=10; s.wormholeRules.spawnChancePerTurn=0;
    s.players={{1,"Observer"},{2,"Sender"}};
    s.stars={{1,"HQ1",{0,0}},{2,"HQ2",{1000,0}}};
    for(auto id:{1U,2U}) {
        Planet p; p.id=id; p.star=id; p.owner=id; p.population=10000; p.homeworld=true;
        p.radar=PlanetaryRadarLevel::Full; s.planets.push_back(p);
    }
    s.shipDesigns={{1,1,"Listener",ShipHullType::Destroyer,{C::FuelMizer,C::MoleScanner}},
        {2,2,"Sender",ShipHullType::Destroyer,{C::FuelMizer,C::RhinoScanner}}};
    for(auto id:{1U,2U}) {
        Fleet f; f.id=id; f.owner=id; f.design=id; f.ships={{id,1}};
        f.position={id==1?400.0:600.0,0}; f.fuel=1000;
        s.fleets.push_back(f);
    }
    // Deliberate data-only window: no contemporaneous active-scan signature.
    s.fleets[1].electronics={EmissionMode::RadioSilence,100,false,10,10,false,false};
    s.nextFleetId=3; s.nextShipDesignId=3;
    return s;
}
void two_physical_delays_and_immutable_source()
{
    auto s=fixture();
    assert(communication_delay_turns(s,2,s.fleets[1].position)==3);
    assert(s.emissions.empty()); // UI/knowledge queries never transmit.
    publish_fleet_telemetry(s,10);
    assert(s.emissions.size()==3);
    assert(s.emissions.back().channel==SignalChannel::Slow && s.emissions.back().fleet==2);
    s.fleets.pop_back(); // A sent pulse survives destruction, with no live source lookup.
    observe_emission_pulses(s,10); observe_emission_pulses(s,11);
    assert(s.players[0].pendingStrategicIntel.empty());
    observe_emission_pulses(s,12);
    assert(s.players[0].pendingStrategicIntel.size()==1);
    const auto evidence=s.players[0].pendingStrategicIntel[0];
    assert(evidence.emission->emittedTurn==10 && evidence.observedTurn==12 && evidence.deliveryTurn==15);
    assert(evidence.emission->channel==SignalChannel::Slow && evidence.owner==0);
    assert(same_position(evidence.position,{610,10}));
    s.turn=14; deliver_strategic_intel(s); assert(s.players[0].strategicIntel.empty());
    auto report=deliver_due_player_reports(s); assert(report.empty());
    s.turn=15; deliver_strategic_intel(s); report=deliver_due_player_reports(s);
    assert(s.players[0].strategicIntel.size()==1 && report.size()==1);
    assert(report[0].emission->emittedTurn==10 && report[0].observedTurn==12 && report[0].turn==15);
    assert(!report[0].fleet && !report[0].contactOwner && !report[0].contact);
    const auto exported=make_player_view(s,1).state;
    assert(exported.emissions.empty() && exported.nextEmissionId==1);
    assert(exported.players.front().strategicIntel.front().emission->emittedTurn==10);
    observe_emission_pulses(s,13); observe_emission_pulses(s,14);
    assert(s.players[0].pendingStrategicIntel.empty()); // No repeated reception inside a filled disk.
    s.turn=16; deliver_strategic_intel(s); assert(s.players[0].strategicIntel.empty());
}
void passive_reception_waits_for_real_dispatch()
{
    auto s=fixture(); s.fleets[0].electronics.mode=EmissionMode::Passive;
    publish_fleet_telemetry(s,10); assert(s.emissions.size()==1);
    observe_emission_pulses(s,12);
    assert(s.players[0].pendingStrategicIntel.front().deliveryTurn==kAwaitingTransmission);
    assert(s.emissions.size()==1); // Listening never emits.
    s.turn=15; s.fleets[0].position={450,0}; s.fleets[0].electronics={};
    dispatch_fleet_report_outbox(s);
    const auto& evidence=s.players[0].pendingStrategicIntel.front();
    assert(evidence.deliveryTurn==18 && evidence.observedTurn==12 && evidence.emission->emittedTurn==10);
    assert(!evidence.transmissionFleet && same_position(evidence.position,{610,10}));
    assert(s.emissions.back().emittedTurn==15 && same_position(s.emissions.back().position,{450,0}));
}
void colony_forwards_silent_fleet_without_relocating_emission()
{
    auto s=fixture(); s.stars.push_back({3,"Outpost",{600,0}});
    Planet p; p.id=3; p.star=3; p.owner=2; p.population=1000; s.planets.push_back(p);
    s.fleets[1].electronics.mode=EmissionMode::Passive;
    publish_fleet_telemetry(s,10);
    assert(s.emissions.back().colony==3 && !s.emissions.back().fleet);
    assert(s.fleets[1].electronics.mode==EmissionMode::Passive);
    s.fleets[1].position={900,0}; observe_emission_pulses(s,12);
    assert(same_position(s.players[0].pendingStrategicIntel.front().position,{610,10}));
    assert(!s.players[0].pendingStrategicIntel.front().owner);
}
void actual_fast_transmission_and_command_source()
{
    auto s=fixture(); s.fleets[1].position={900,0}; s.fleets[0].position={700,0};
    assert(communication_route(s,2,{900,0}).channel==SignalChannel::Fast);
    publish_fleet_telemetry(s,10); observe_emission_pulses(s,10);
    assert(s.players[0].pendingStrategicIntel.front().emission->channel==SignalChannel::Fast);
    assert(s.players[0].pendingStrategicIntel.front().observedTurn==10);
    auto before=s.emissions.size(); observe_emission_pulses(s,10); assert(s.emissions.size()==before);
    auto quiet=fixture(); quiet.fleets[1].electronics.mode=EmissionMode::Passive;
    assert(submit_fleet_route_command(quiet,2,2,{650,0},4,{},{}));
    assert(quiet.emissions.size()==1 && same_position(quiet.emissions.front().position,{1000,0}));
    assert(quiet.fleets[1].electronics.mode==EmissionMode::Passive);
    assert(quiet.fleets[1].telemetryInTransit.empty());
    auto noScanner=fixture(); noScanner.shipDesigns[0].components={C::FuelMizer};
    publish_fleet_telemetry(noScanner,10); observe_emission_pulses(noScanner,12);
    assert(noScanner.players[0].pendingStrategicIntel.empty());
}
int main()
{
    two_physical_delays_and_immutable_source(); passive_reception_waits_for_real_dispatch();
    colony_forwards_silent_fleet_without_relocating_emission(); actual_fast_transmission_and_command_source();
}
