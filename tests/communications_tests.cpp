#include "suns/communications.hpp"
#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/campaign.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace suns;

Fleet& scout(GameState& state)
{
    assert(!state.fleets.empty());
    return state.fleets.front();
}

void colony_radar_bootstraps_detection_and_links()
{
    auto state = make_demo_game();
    state.fleets.clear();
    auto& outpost = state.planets[1];
    outpost.owner = 1;
    outpost.population = 100;
    outpost.industry = 1;
    outpost.radar = PlanetaryRadarLevel::None;
    outpost.homeworld = false;
    // A nearby colony needs no local scanner to receive instant orders from
    // the homeworld scanner, but it must not extend that scanner's range.
    outpost.productionQueue.clear();
    state.stars[1].position = {100, 0};
    assert(communication_delay_turns(state, 1, state.stars[1].position) == 0);
    assert(outpost.productionQueue.empty());
    state.stars[1].position = {175, 0};
    assert(communication_delay_turns(state, 1, state.stars[1].position) > 0);
    // An explicit build programme can still make an isolated colony connect.
    outpost.productionQueue = {{ProductionKind::LocalRadar, production_cost(ProductionKind::LocalRadar)}};
    state.players.front().technology.researchActive = true;
    state.players.front().technology.researchAllocationPercent = 100;
    assert(planetary_radar_range(outpost.radar) == 0);
    state.stars[3].position = {200, 0}; // Vega was unscanned at start; now in outpost R30.
    assert(survey_level(state, 1, state.stars[3].id) < SurveyLevel::SystemScan);
    assert(communication_delay_turns(state, 1, state.stars[1].position) > 0);
    auto first = TurnProcessor{}.process(state, {});
    assert(first.planets[1].radar == PlanetaryRadarLevel::None);
    const auto built = TurnProcessor{}.process(first, {});
    assert(built.planets[1].radar == PlanetaryRadarLevel::Local);
    assert(built.planets[1].productionQueue.empty());
    assert(communication_delay_turns(built, 1, {175, 0}) == 0);
    assert(planetary_radar_range(built.planets[1].radar) == 30.0);
    auto scanned = TurnProcessor{}.process(built, {});
    assert(survey_level(scanned, 1, scanned.stars[3].id) >= SurveyLevel::SystemScan);

    auto isolated = built;
    isolated.stars[1].position = {220, 0};
    assert(communication_delay_turns(isolated, 1, {220, 0}) > 0);
    isolated.planets[1].industry = 50;
    isolated.planets[1].minerals = {20, 20, 20};
    isolated.players.front().technology.researchAllocationPercent = 0; // Upgrade is ordinary paid production.
    const auto upgraded = TurnProcessor{}.process(isolated,
        {{1, {QueueProductionBatchOrder{2, ProductionKind::PlanetaryRadar, 1, ProductionAutomation::None}}}});
    assert(upgraded.planets[1].radar == PlanetaryRadarLevel::Full);
    assert(planetary_radar_range(upgraded.planets[1].radar) == 150.0);
    assert(communication_delay_turns(upgraded, 1, {220, 0}) == 0);

    auto noHome = built;
    noHome.planets.front().radar = PlanetaryRadarLevel::None;
    assert(communication_delay_turns(noHome, 1, {175, 0}) > 0);
}

void local_commands_remain_immediate()
{
    auto state = generate_game(GalaxyConfig{});
    auto& fleet = scout(state);
    assert(communication_delay_turns(state, 1, fleet.position) == 0);

    MoveFleetOrder move;
    move.fleet = fleet.id;
    move.destination = {25.0, 0.0};
    move.warp = 5; // Within the original Scout's fuel budget for one year.

    TurnProcessor processor;
    const auto next = processor.process(state, {{1, {move}}});
    const auto& moved = next.fleets.front();

    assert(same_position(moved.position, move.destination));
    assert(moved.pendingCommands.empty());
}

void homeworld_scanner_defines_the_initial_network_field()
{
    auto state = generate_game(GalaxyConfig{});
    const auto& home = state.stars.front();
    state.fleets.clear();

    assert(communication_delay_turns(state, 1, home.position) == 0);
    assert(communication_delay_turns(state, 1, {home.position.x + kColonySensorRange, home.position.y}) == 0);
    assert(communication_delay_turns(state, 1, {home.position.x + kColonySensorRange + 0.001, home.position.y}) == 2);
    assert(communication_delay_turns(
        state, 1, {home.position.x + 2.0 * kCommunicationSignalSpeed, home.position.y}) == 2);
    assert(communication_delay_turns(
        state, 1, {home.position.x + 2.0 * kCommunicationSignalSpeed + 0.001, home.position.y}) == 3);
}

void ordinary_scanners_automatically_form_a_relay_chain()
{
    auto state = generate_game(GalaxyConfig{});
    auto& firstRelay = state.fleets.front();
    firstRelay.position = {200.0, 0.0};

    auto secondRelay = firstRelay;
    secondRelay.id = 99;
    secondRelay.position = {300.0, 0.0};
    state.fleets.push_back(secondRelay);

    // Homeworld R150 overlaps Scout A R50; the two scout fields then overlap.
    assert(communication_delay_turns(state, 1, {340.0, 0.0}) == 0);

    // Breaking the first overlap detaches the entire mobile branch. Its local
    // scanner island forwards instantly to Scout A, then the slow signal must
    // cover the physical 201 ly from Scout A to Homeworld (two turns).
    state.fleets.front().position = {201.0, 0.0};
    assert(communication_delay_turns(state, 1, {340.0, 0.0}) == 2);
}

void penetrating_only_scanner_does_not_extend_the_network()
{
    auto state = generate_game(GalaxyConfig{});
    state.shipDesigns.push_back({
        99,
        1,
        "Penetrating-only",
        ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::PenetratingScanner},
    });
    state.fleets.front().design = 99;
    state.fleets.front().ships = {{99, 1}};
    state.fleets.front().position = {120.0, 0.0};

    // The ship itself is a physical receiver inside colony coverage, so the
    // remote point can reach it in one turn. Its penetrating field still does
    // not turn that point into part of the instantaneous mesh.
    assert(communication_delay_turns(state, 1, {180.0, 0.0}) == 1);
}

void relay_array_extends_comms_without_detecting_contacts()
{
    auto state = generate_game(GalaxyConfig{});
    state.shipDesigns.push_back({99, 1, "Relay ship", ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::RelayArray}});
    auto& relay = state.fleets.front();
    relay.design = 99;
    relay.ships = {{99, 1}};
    relay.position = {200.0, 0.0};

    assert(!component_available_to_player(state, 1, ShipComponentType::RelayArray));
    assert(!ship_design_available_to_player(state, 1, state.shipDesigns.back()));
    assert(fleet_sensor_range(state, relay) == 0.0);
    assert(fleet_penetrating_sensor_range(state, relay) == 0.0);
    assert(fleet_communication_range(state, relay) == 180.0);
    assert(communication_delay_turns(state, 1, {370.0, 0.0}) == 0);

    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Electronics)] = 5;
    assert(component_available_to_player(state, 1, ShipComponentType::RelayArray));
    assert(ship_design_available_to_player(state, 1, state.shipDesigns.back()));
    assert(fleet_sensor_range(state, relay) == 0.0);

    relay.design = kScoutDesignId;
    relay.ships = {{kScoutDesignId, 1}};
    assert(communication_delay_turns(state, 1, {370.0, 0.0}) > 0);
}

void slow_signal_reaches_a_physical_receiver_not_a_field_boundary()
{
    auto state = generate_game(GalaxyConfig{});
    state.fleets.clear();

    // The Homeworld scanner reaches 150 ly, but there is no receiver at x=150.
    // From x=200 the real radio path to Homeworld is 200 ly: two turns, not one.
    assert(communication_delay_turns(state, 1, {200.0, 0.0}) == 2);
}

void nearest_colony_scanner_field_is_used()
{
    auto state = generate_game(GalaxyConfig{});

    // A local receiver is not a second artificial capital. Its field must
    // overlap the actual homeworld network to provide an instant connection.
    auto& relayPlanet = state.planets[1];
    assert(relayPlanet.star == state.stars[1].id);
    relayPlanet.owner = 1;
    relayPlanet.population = 1000;
    relayPlanet.radar = PlanetaryRadarLevel::Local;
    relayPlanet.homeworld = false;
    state.stars[1].position = {175, 0};
    assert(communication_delay_turns(state, 1, {185, 0}) == 0);
    state.stars[1].position = {200, 0};
    assert(communication_delay_turns(state, 1, {210, 0}) > 0);
}

void remote_command_arrives_after_signal_delay()
{
    auto state = generate_game(GalaxyConfig{});
    state.turn = 10;
    auto& fleet = scout(state);
    fleet.position = {420.0, 0.0};
    fleet.destination = Position{600.0, 0.0};
    fleet.warp = 8;
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    fleet.telemetry = FleetTelemetry{
        10,
        fleet.position,
        fleet.destination,
        fleet.warp,
        fleet.fuel,
        0,
        std::nullopt,
        {},
        {},
    };

    assert(communication_delay_turns(state, 1, fleet.position) == 3);

    MoveFleetOrder returnHome;
    returnHome.fleet = fleet.id;
    returnHome.destination = {0.0, 0.0};
    returnHome.warp = 8;

    TurnProcessor processor;
    auto turn11 = processor.process(state, {{1, {returnHome}}});
    assert(turn11.turn == 11);
    assert(turn11.fleets.front().pendingCommands.size() == 1);
    assert(turn11.fleets.front().pendingCommands.front().deliveryTurn == 13);
    assert(turn11.fleets.front().destination.has_value());
    assert(same_position(*turn11.fleets.front().destination, Position{600.0, 0.0}));
    assert(turn11.fleets.front().position.x > 420.0);

    auto turn12 = processor.process(turn11, {});
    assert(turn12.fleets.front().pendingCommands.size() == 1);

    auto turn13 = processor.process(turn12, {});
    assert(turn13.turn == 13);
    assert(turn13.fleets.front().pendingCommands.empty());
    assert(turn13.fleets.front().destination.has_value());
    assert(same_position(*turn13.fleets.front().destination, Position{0.0, 0.0}));

    const auto positionAtDelivery = turn13.fleets.front().position.x;
    auto turn14 = processor.process(turn13, {});
    assert(turn14.fleets.front().position.x < positionAtDelivery);
}

void remote_clear_route_stops_when_command_arrives()
{
    auto state = generate_game(GalaxyConfig{});
    state.turn = 10;
    auto& fleet = scout(state);
    fleet.position = {420.0, 0.0};
    fleet.destination = Position{700.0, 0.0};
    fleet.warp = 8;
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    fleet.telemetry = {10, fleet.position, fleet.destination, fleet.warp, fleet.fuel, 0, std::nullopt, {}, {}};

    const auto visibleAtTransmission = projected_fleet_position(state, fleet);
    MoveFleetOrder stop;
    stop.fleet = fleet.id;
    stop.destination = visibleAtTransmission;
    stop.warp = fleet.warp;

    TurnProcessor processor;
    auto turn11 = processor.process(state, {{1, {stop}}});
    assert(turn11.fleets.front().pendingCommands.size() == 1);
    assert(turn11.fleets.front().pendingCommands.front().program.clearRoute);
    assert(turn11.fleets.front().destination.has_value());

    auto turn12 = processor.process(turn11, {});
    assert(turn12.fleets.front().pendingCommands.size() == 1);

    auto turn13 = processor.process(turn12, {});
    assert(turn13.fleets.front().pendingCommands.empty());
    assert(!turn13.fleets.front().destination.has_value());
    const auto stoppedAt = turn13.fleets.front().position;

    auto turn14 = processor.process(turn13, {});
    assert(same_position(turn14.fleets.front().position, stoppedAt));
}

void stale_telemetry_predicts_without_revealing_route_change()
{
    auto state = generate_game(GalaxyConfig{});
    state.turn = 10;
    auto& fleet = scout(state);
    fleet.position = {420.0, 0.0};
    fleet.destination = Position{600.0, 0.0};
    fleet.warp = 8;
    fleet.fuel = fleet_fuel_capacity(state, fleet);
    fleet.telemetry = FleetTelemetry{
        10,
        fleet.position,
        fleet.destination,
        fleet.warp,
        fleet.fuel,
        0,
        std::nullopt,
        {},
        {},
    };

    MoveFleetOrder returnHome;
    returnHome.fleet = fleet.id;
    returnHome.destination = {0.0, 0.0};
    returnHome.warp = 8;

    TurnProcessor processor;
    auto next = processor.process(state, {{1, {returnHome}}});
    next = processor.process(next, {});
    next = processor.process(next, {}); // command arrives at turn 13
    next = processor.process(next, {}); // actual fleet now starts home

    const auto& actual = next.fleets.front();
    assert(next.turn == 14);
    assert(actual.destination.has_value());
    assert(same_position(*actual.destination, Position{0.0, 0.0}));
    assert(fleet_telemetry_age(next, actual) >= 1);

    const auto playerView = fleet_player_view(next, actual);
    assert(playerView.destination.has_value());
    assert(same_position(*playerView.destination, Position{600.0, 0.0}));
    assert(!same_position(playerView.position, actual.position));
}

void passive_and_silent_receivers_accept_orders_without_acknowledging()
{
    for(auto mode:{EmissionMode::Passive,EmissionMode::RadioSilence}) {
        auto s=make_demo_game(); s.turn=2;
        auto& f=s.fleets.front(); f.electronics={mode,mode==EmissionMode::RadioSilence?20u:0u};
        const auto old=f.telemetry.observedTurn;
        assert(submit_fleet_route_command(s,1,f.id,{25,0},5,{},{}));
        assert(f.pendingCommands.empty() && f.destination && !fleet_transmits(f,s.turn));
        assert(f.telemetry.observedTurn==old); // Receiving sends no acknowledgement.
        assert(submit_fleet_task_command(s,1,f.id,FleetTask::None));
        assert(f.electronics.mode==mode);
        assert(submit_electronics_command(s,1,f.id,{EmissionMode::Standard,0}));
        assert(f.pendingCommands.empty() && f.electronics.mode==EmissionMode::Standard);
    }
    Fleet passive; passive.electronics.mode=EmissionMode::Passive;
    for(std::uint64_t turn=1;turn<=12;++turn) assert(!fleet_transmits(passive,turn));
    assert(fleet_available_turn(passive,3)==kAwaitingTransmission);
}

void silent_slow_reception_preserves_propagation_and_holds_outgoing_reports()
{
    for(auto mode:{EmissionMode::Passive,EmissionMode::RadioSilence}) {
        auto s=make_demo_game(); s.turn=10; s.wormholeRules.spawnChancePerTurn=0;
        auto& f=s.fleets.front(); f.position={420,0}; f.destination.reset();
        f.telemetry.observedTurn=10; f.telemetry.position=f.position; f.telemetry.destination.reset();
        f.electronics={mode,mode==EmissionMode::RadioSilence?30u:0u};
        f.fuel=fleet_fuel_capacity(s,f);
        assert(communication_delay_turns(s,1,f.position)==3);
        assert(submit_fleet_route_command(s,1,f.id,{600,0},5,{},{}));
        assert(f.pendingCommands.front().deliveryTurn==13);
        queue_player_report(s,1,PlayerReportKind::FleetArrived,f.position,10,0,0,f.id);
        assert(s.players[0].pendingPlayerReports.back().deliveryTurn==kAwaitingTransmission);
        assert(s.players[0].pendingPlayerReports.back().transmissionFleet==f.id);
        for(int year=0;year<3;++year) {
            const auto result=TurnProcessor{}.process_with_events(s,{}); s=result.state;
            assert(s.fleets.front().telemetry.observedTurn==10 && s.fleets.front().telemetryInTransit.empty());
            assert(std::none_of(result.events.begin(),result.events.end(),[](const auto& e){return e.kind==GameEventKind::FleetArrived;}));
        }
        assert(s.turn==13 && s.fleets.front().pendingCommands.empty() && s.fleets.front().destination);
        assert(!fleet_transmits(s.fleets.front(),s.turn));
        const auto view=make_player_view(s,1).state;
        assert(view.players[0].pendingPlayerReports.empty() && view.players[0].pendingSensorStatus.empty());
        apply_electronics_program(s.fleets.front(),{});
        const auto sent=s.turn;
        const auto due=sent+communication_delay_turns(s,1,s.fleets.front().position);
        dispatch_fleet_report_outbox(s);
        const auto held=std::find_if(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::FleetArrived;});
        assert(held!=s.players[0].pendingPlayerReports.end() && held->deliveryTurn==due && !held->transmissionFleet);
        assert(deliver_due_player_reports(s).empty());
        // Already transmitted data continues in flight after switching off TX.
        apply_electronics_program(s.fleets.front(),{EmissionMode::Passive,0}); s.turn=due;
        const auto events=deliver_due_player_reports(s);
        assert(std::any_of(events.begin(),events.end(),[](const auto& e){return e.kind==GameEventKind::FleetArrived && e.observedTurn==10;}));
        assert(s.fleets.front().telemetry.observedTurn==10);
    }
}

void friendly_orbit_shares_full_status_over_the_colony_link()
{
    for(auto mode:{EmissionMode::Passive,EmissionMode::RadioSilence}) for(int site=0;site<4;++site) {
        auto s=make_demo_game(); s.wormholeRules.spawnChancePerTurn=0;
        // Homeworld; a nearby radar-free colony; isolated colonies with and
        // without radar. Local exchange never requires a planetary scanner.
        const auto target=site==0?Position{0,0}:site==1?Position{100,0}:Position{420,0};
        if(site) {
            s.stars[1].position=target;
            auto& colony=s.planets[1]; colony.owner=1; colony.population=1000;
            colony.radar=site==2?PlanetaryRadarLevel::Local:PlanetaryRadarLevel::None;
        }
        auto& f=s.fleets.front(); f.position={target.x+25,0}; f.destination=target; f.warp=5;
        f.fuel=fleet_fuel_capacity(s,f); f.electronics={mode,mode==EmissionMode::RadioSilence?30u:0u};
        f.telemetry.position=f.position; f.telemetry.destination.reset(); f.telemetry.colonists=100; f.colonists=200;
        f.telemetry.damagePercent=50; f.damagePercent=12;
        queue_player_report(s,1,PlayerReportKind::FleetArrived,f.position,1,0,0,f.id);
        assert(s.players[0].pendingPlayerReports.back().transmissionFleet==f.id);
        const auto result=TurnProcessor{}.process_with_events(s,{}); s=result.state;
        assert(same_position(s.fleets.front().position,target));
        assert(!fleet_transmits(s.fleets.front(),s.turn) && s.fleets.front().electronics.mode==mode);
        const auto delay=communication_delay_turns(s,1,target);
        assert(fleet_has_instant_link(s,s.fleets.front())==(site<2));
        if(delay) {
            const auto& fleet=s.fleets.front();
            assert(fleet.telemetry.observedTurn==1 && fleet.telemetry.colonists==100);
            assert(fleet.telemetryInTransit.size()==1);
            const auto& packet=fleet.telemetryInTransit.front();
            assert(packet.deliveryTurn==2+delay && packet.telemetry.colonists==200);
            assert(packet.telemetry.damagePercent==fleet.damagePercent && packet.telemetry.fuel==fleet.fuel);
            const auto route=std::find_if(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r){return r.kind==PlayerReportKind::RouteCompleted;});
            assert(route!=s.players[0].pendingPlayerReports.end() && route->deliveryTurn==2+delay && !route->transmissionFleet);
            const auto exported=make_player_view(s,1).state;
            assert(exported.fleets.front().colonists==100 && exported.fleets.front().telemetryInTransit.empty());
            assert(exported.players[0].pendingPlayerReports.empty());
            while(s.turn<2+delay) {
                s=TurnProcessor{}.process(s,{});
                if(s.turn<2+delay) assert(s.fleets.front().telemetry.observedTurn==1);
            }
        } else {
            assert(std::count_if(result.events.begin(),result.events.end(),[](const auto& e){return e.kind==GameEventKind::RouteCompleted && e.observedTurn==2;})==1);
            assert(std::none_of(result.events.begin(),result.events.end(),[](const auto& e){return e.kind==GameEventKind::FleetArrived && e.observedTurn==2;}));
        }
        const auto known=confirmed_fleet_telemetry(s,s.fleets.front());
        assert(known.observedTurn==2 && same_position(known.position,target));
        assert(known.colonists==200 && !known.destination && known.electronics.mode==mode);
        assert(fleet_player_view(s,s.fleets.front()).colonists==200);
        const auto report=std::find_if(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r){return r.observedTurn==1 && r.kind==PlayerReportKind::FleetArrived;});
        assert(report==s.players[0].pendingPlayerReports.end()); // The local upload also emptied the old outbox.
    }
}

void orbital_snapshot_survives_silent_departure_without_following_hidden_changes()
{
    auto s=make_demo_game(); s.turn=10;
    s.stars[1].position={420,0};
    auto& colony=s.planets[1]; colony.owner=1; colony.population=1000; colony.radar=PlanetaryRadarLevel::None;
    auto& f=s.fleets.front(); f.position={420,0}; f.electronics={EmissionMode::RadioSilence,30};
    f.destination=Position{600,0}; f.waypointQueue={{{750,0},5,{}}};
    f.fuel=147; f.damagePercent=17; f.colonists=200; f.minerals={1,2,3}; f.ships={{f.design,2}};
    f.telemetry.observedTurn=8; f.telemetry.colonists=100;
    publish_fleet_telemetry(s,10);
    assert(f.telemetryInTransit.size()==1 && f.telemetryInTransit.front().deliveryTurn==13);
    assert(report_transmission_turn(s,1,f.position,10,f.id)==10);
    f.position={600,0}; f.fuel=5; f.damagePercent=55; f.colonists=99; f.minerals={}; f.ships={{f.design,3}};
    f.destination=Position{900,0}; f.waypointQueue.clear();
    s.turn=12; publish_fleet_telemetry(s,12); deliver_due_fleet_telemetry(s);
    assert(f.telemetry.observedTurn==8 && f.telemetryInTransit.size()==1);
    s.turn=13; deliver_due_fleet_telemetry(s);
    const auto view=make_player_view(s,1).state.fleets.front();
    assert(view.telemetry.observedTurn==10 && same_position(view.telemetry.position,Position{420,0}));
    assert(view.fuel==147 && view.damagePercent==17 && view.colonists==200 && view.minerals.boranium==2);
    assert(view.ships.size()==1 && view.ships.front().count==2 && view.electronics.mode==EmissionMode::RadioSilence);
    assert(view.destination && same_position(*view.destination,Position{600,0}) && view.waypointQueue.size()==1);
    assert(f.telemetryInTransit.empty() && !fleet_transmits(f,s.turn));
}

void local_status_exchange_requires_an_owned_populated_colony()
{
    for(int site=0;site<3;++site) {
        auto s=make_demo_game(); s.turn=2;
        auto& f=s.fleets.front(); f.position=s.stars[1].position; f.electronics={EmissionMode::Passive,0};
        auto& colony=s.planets[1]; colony.owner=site==0?2:site==1?0:1; colony.population=site==2?0:1000;
        f.telemetry.observedTurn=1; f.telemetry.colonists=100; f.colonists=200;
        publish_fleet_telemetry(s,s.turn);
        assert(f.telemetry.observedTurn==1 && f.telemetryInTransit.empty() && !fleet_has_instant_link(s,f));
        queue_player_report(s,1,PlayerReportKind::FleetArrived,f.position,s.turn,0,0,f.id);
        assert(s.players[0].pendingPlayerReports.back().transmissionFleet==f.id);
    }
}

void merged_fleet_keeps_unsent_observations()
{
    auto s=make_demo_game(); s.wormholeRules.spawnChancePerTurn=0;
    s.fleets.front().position={25,0}; // Away from local ship/surface exchange.
    auto second=s.fleets.front(); second.id=2; second.electronics={EmissionMode::Passive,0};
    s.fleets.push_back(second); s.nextFleetId=3;
    queue_player_report(s,1,PlayerReportKind::FleetArrived,second.position,s.turn,1,0,2);
    assert(s.players[0].pendingPlayerReports.back().transmissionFleet==2);
    const auto result=TurnProcessor{}.process_with_events(s,{{1,{MergeFleetsOrder{1,2}}}});
    assert(result.state.fleets.size()==1 && result.state.fleets.front().id==1);
    assert(std::any_of(result.events.begin(),result.events.end(),[](const auto& e){return e.kind==GameEventKind::FleetArrived && e.observedTurn==1;}));
}

} // namespace

int main()
{
    colony_radar_bootstraps_detection_and_links();
    local_commands_remain_immediate();
    homeworld_scanner_defines_the_initial_network_field();
    ordinary_scanners_automatically_form_a_relay_chain();
    penetrating_only_scanner_does_not_extend_the_network();
    relay_array_extends_comms_without_detecting_contacts();
    slow_signal_reaches_a_physical_receiver_not_a_field_boundary();
    nearest_colony_scanner_field_is_used();
    remote_command_arrives_after_signal_delay();
    remote_clear_route_stops_when_command_arrives();
    stale_telemetry_predicts_without_revealing_route_change();
    passive_and_silent_receivers_accept_orders_without_acknowledging();
    silent_slow_reception_preserves_propagation_and_holds_outgoing_reports();
    friendly_orbit_shares_full_status_over_the_colony_link();
    orbital_snapshot_survives_silent_departure_without_following_hidden_changes();
    local_status_exchange_requires_an_owned_populated_colony();
    merged_fleet_keeps_unsent_observations();
    std::cout << "communications tests passed\n";
    return 0;
}
