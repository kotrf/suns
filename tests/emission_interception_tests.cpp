#include "suns/communications.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/campaign.hpp"
#include "suns/turn_processor.hpp"
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

void expendable_probe_broadcasts_at_the_final_waypoint()
{
    auto state = fixture();
    state.stars[1].position = {680, 0};
    state.shipDesigns[1] = {2, 2, "Decoy Probe", ShipHullType::SurveyProbe, {C::FuelMizer, C::DecoyTransmitter}};
    assert(ship_design_validation_error(state.shipDesigns[1]).empty());
    state.fleets[1].electronics = {};
    const Position waypoint{560, 0};
    const FleetArrivalAction action{FleetArrivalActionKind::BroadcastDecoy};
    auto wrongHull = fixture();
    assert(!submit_fleet_route_command(wrongHull, 2, 2, waypoint, 5, action, {}));
    auto stacked = state;
    stacked.fleets[1].ships.front().count = 2;
    assert(!submit_fleet_route_command(stacked, 2, 2, waypoint, 5, action, {}));
    assert(!fleet_route_tasks_valid({waypoint, 5, action, {{{550, 0}, 5, {}}}}));
    const auto decoy = [](const EmissionPulse& pulse) { return pulse.decoyOnly; };
    const TurnProcessor processor;
    state = processor.process(state, {{2, {MoveFleetOrder{2, waypoint, 5, action}}}});
    assert(state.fleets.size() == 2 && std::none_of(state.emissions.begin(), state.emissions.end(), decoy));
    auto tachyon = state;
    tachyon.shipDesigns[0].components.insert(tachyon.shipDesigns[0].components.end(), 4, C::TachyonDetector);
    auto waiting = state;
    waiting.fleets[1].electronics.mode = EmissionMode::Passive;
    waiting = processor.process(waiting, {});
    assert(waiting.fleets.size() == 2 && same_position(waiting.fleets[1].position, waypoint));
    assert(std::none_of(waiting.emissions.begin(), waiting.emissions.end(), decoy));

    auto launch = processor.process_with_events(state, {});
    assert(launch.state.fleets.size() == 1);
    const auto pulse = std::find_if(launch.state.emissions.begin(), launch.state.emissions.end(), decoy);
    assert(pulse != launch.state.emissions.end() && pulse->fleet == 2 && same_position(pulse->position, waypoint));
    assert(std::count_if(launch.state.emissions.begin(), launch.state.emissions.end(), decoy) == 1);
    assert(std::any_of(launch.events.begin(), launch.events.end(), [](const auto& event) {
        return event.recipient == 2 && event.kind == GameEventKind::DecoyBroadcastStatus && event.quantity == 2;
    }));
    const auto echo = [year = launch.state.turn](const StrategicIntel& evidence) {
        return evidence.emission && evidence.emission->emittedTurn == year
            && same_position(evidence.position, {610, 30});
    };
    const auto& pending = launch.state.players[0].pendingStrategicIntel;
    const auto evidence = std::find_if(pending.begin(), pending.end(), echo);
    assert(evidence != pending.end() && !evidence->id && !evidence->owner && evidence->deliveryTurn > launch.state.turn);
    const auto due = evidence->deliveryTurn;
    auto view = make_player_view(launch.state, 1);
    assert(view.state.emissions.empty() && view.state.players[0].pendingStrategicIntel.empty());
    tachyon = processor.process(tachyon, {});
    assert(std::none_of(tachyon.players[0].pendingStrategicIntel.begin(), tachyon.players[0].pendingStrategicIntel.end(), echo));
    while (launch.state.turn < due) launch.state = processor.process(launch.state, {});
    view = make_player_view(launch.state, 1);
    assert(view.state.emissions.empty());
    assert(std::count_if(view.state.players[0].strategicIntel.begin(), view.state.players[0].strategicIntel.end(), echo) == 1);
    state = processor.process(launch.state, {});
    assert(std::none_of(state.players[0].strategicIntel.begin(), state.players[0].strategicIntel.end(), echo));
}

void only_strong_signals_reveal_anonymous_activity()
{
    auto state = fixture();
    state.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 8;
    state.fleets[0].position = {510, 0};
    auto weak = state;
    weak.fleets[0].position = {440, 0};
    auto earlyTechnology = state;
    earlyTechnology.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 7;
    const auto detect = [](GameState& s, std::uint64_t observedTurn) {
        record_scan_transmission(s, s.fleets[1], s.turn);
        publish_fleet_telemetry(s, s.turn);
        observe_emission_pulses(s, observedTurn);
    };
    const auto activity = [](const GameState& s, SignalChannel channel) {
        const auto& evidence = s.players[0].pendingStrategicIntel;
        const auto observation = std::find_if(evidence.begin(), evidence.end(), [channel](const auto& e) {
            return e.emission && e.emission->channel == channel && same_position(e.position, {610, 10});
        });
        assert(observation != evidence.end() && !observation->owner && !observation->id);
        return observation->emission->activity;
    };
    detect(state, 11);
    assert(activity(state, SignalChannel::Slow) == SignalActivity::RadioTraffic);
    assert(activity(state, SignalChannel::Fast) == SignalActivity::ActiveElectronics);
    detect(weak, 12);
    assert(activity(weak, SignalChannel::Slow) == SignalActivity::Unclassified);
    detect(earlyTechnology, 11);
    assert(activity(earlyTechnology, SignalChannel::Slow) == SignalActivity::Unclassified);

    auto mixed = fixture();
    mixed.stars[1].position = {700, 0};
    mixed.fleets[0].position = {510, 0};
    mixed.fleets[1].electronics = {};
    mixed.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 8;
    publish_fleet_telemetry(mixed, mixed.turn);
    observe_emission_pulses(mixed, mixed.turn + 1);
    assert(activity(mixed, SignalChannel::Fast) == SignalActivity::Mixed);
}

void late_classification_survives_the_first_alert()
{
    for (const bool silent : {false, true}) {
        auto state = fixture();
        const Position area{210, 10};
        state.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 8;
        state.fleets[0].position = {285, 0};
        state.fleets[1].position = {200, 0};
        record_scan_transmission(state, state.fleets[1], 10);
        state.fleets.pop_back(); // Isolate receipt ordering from simultaneous hull detection.
        if (silent) {
            state.fleets[0].electronics.mode = EmissionMode::Passive;
            auto secondReceiver = state.fleets[0];
            secondReceiver.id = state.nextFleetId++;
            state.fleets.push_back(secondReceiver);
        }
        observe_emission_pulses(state, 10);
        auto& player = state.players.front();
        assert(player.pendingStrategicIntel.size() == (silent ? 3 : 2));
        assert(player.pendingPlayerReports.size() == player.pendingStrategicIntel.size());
        deliver_strategic_intel(state);
        const auto alert = deliver_due_player_reports(state);
        assert(alert.size() == 1 && alert[0].emission->activity == SignalActivity::Unclassified);
        assert(player.strategicIntel.size() == 1 && player.strategicIntel[0].emission->activity == SignalActivity::Unclassified);
        const auto reported = make_player_view(state, 1);
        assert(reported.state.players.front().pendingStrategicIntel.empty());
        assert(reported.state.players.front().pendingPlayerReports.empty());
        state.turn = 11;
        deliver_strategic_intel(state);
        assert(deliver_due_player_reports(state).empty() && player.strategicIntel.empty());
        if (silent) {
            // The second listener reaches a friendly colony first. The original
            // listener still has its copy aboard and stays in Passive.
            state.turn = 13;
            state.fleets.back().position = {0, 0};
            dispatch_fleet_report_outbox(state);
        } else {
            state.turn = 12;
        }
        deliver_strategic_intel(state);
        const auto refinement = deliver_due_player_reports(state);
        assert(refinement.size() == 1 && refinement[0].emission->activity == SignalActivity::ActiveElectronics);
        assert(refinement[0].observedTurn == 10 && refinement[0].emission->emittedTurn == 10);
        assert(refinement[0].turn == state.turn && same_position(refinement[0].position, area));
        assert(player.strategicIntel.size() == 1 && player.strategicIntel[0].emission->activity == SignalActivity::ActiveElectronics);
        assert(!player.strategicIntel[0].owner && !player.strategicIntel[0].id);
        assert(player.pendingStrategicIntel.empty() && player.pendingPlayerReports.empty());
    }
}

void a_hull_contact_does_not_hide_a_classified_signal()
{
    auto state = fixture();
    state.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 8;
    state.fleets[1].position = {485, 0};
    assert(detector_detects_fleet(state, state.fleets[0], state.fleets[1]));
    record_scan_transmission(state, state.fleets[1], 10);
    auto earlyTechnology = state;
    earlyTechnology.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 7;
    observe_emission_pulses(earlyTechnology, 10);
    assert(earlyTechnology.players[0].pendingStrategicIntel.empty());
    observe_emission_pulses(state, 10);
    const auto& pending = state.players[0].pendingStrategicIntel;
    assert(pending.size() == 1 && pending.front().emission->activity == SignalActivity::ActiveElectronics);
    assert(pending.front().deliveryTurn == 13 && same_position(pending.front().position, {490, 10}));
    state.turn = 13;
    deliver_strategic_intel(state);
    const auto reports = deliver_due_player_reports(state);
    assert(reports.size() == 1 && !reports[0].fleet && !reports[0].contactOwner);
    assert(reports[0].emission->activity == SignalActivity::ActiveElectronics);
    assert(state.players[0].strategicIntel.front().emission->activity == SignalActivity::ActiveElectronics);
}

int main()
{
    two_physical_delays_and_immutable_source(); passive_reception_waits_for_real_dispatch();
    colony_forwards_silent_fleet_without_relocating_emission(); actual_fast_transmission_and_command_source();
    expendable_probe_broadcasts_at_the_final_waypoint();
    only_strong_signals_reveal_anonymous_activity();
    late_classification_survives_the_first_alert();
    a_hull_contact_does_not_hide_a_classified_signal();
}
