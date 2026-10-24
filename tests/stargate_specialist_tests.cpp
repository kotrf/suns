#include "suns/stations.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace suns;
using C = ShipComponentType;

namespace {
GameState network(C component = C::NetworkStargate)
{
    auto s = make_demo_game();
    s.stars.resize(2); s.planets.resize(2);
    s.turn = 10; s.wormholeRules.spawnChancePerTurn = 0;
    s.players[0].race.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
    s.players[0].technology.levels.fill(26);
    s.players[0].technology.researchActive = false;
    s.players[0].history.clear();
    s.stars[0].position = {0, 0}; s.stars[1].position = {700, 0};
    for (auto& p : s.planets) {
        p.owner = 1; p.population = 2'000'000; p.industry = 200;
        p.radar = PlanetaryRadarLevel::Full; p.minerals = {5000, 5000, 5000};
    }
    s.orbitalStations.clear();
    for (const auto& p : s.planets) s.orbitalStations.push_back({p.id, 1, p.id, p.name + " gate",
        OrbitalStationHullType::Fortress, {OrbitalStationModule::Shipyard}, {component}});
    s.nextOrbitalStationId = 3;
    s.fleets.resize(1);
    auto& f = s.fleets[0]; f.position = {0, 0}; f.destination.reset();
    f.waypointQueue.clear(); f.pendingCommands.clear(); f.task = FleetTask::None;
    f.repeatOrders = false; f.targetFleet = 0; f.colonists = 0; f.minerals = {};
    f.telemetry.position = f.position; f.telemetry.observedTurn = s.turn;
    f.telemetry.destination.reset(); f.telemetry.waypointQueue.clear();
    for(const auto& p:s.planets)set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);
    return s;
}
void setup_and_research_are_trait_specific()
{
    EmpireSetup setup{"Travellers"}; setup.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
    auto s = generate_campaign({}, {setup});
    assert(s.players[0].race.primaryTrait == setup.primaryTrait);
    assert(!station_stargate(s.orbitalStations[0]));
    assert(!stargate_component_available(s, 1, C::JumpGate));
    s.players[0].technology.levels.fill(26);
    assert(stargate_component_available(s, 1, C::InfinityStargate));
    for (const auto field : {ResearchField::Energy, ResearchField::Electronics, ResearchField::Construction}) {
        auto low = s; low.players[0].technology.levels[std::size_t(field)] = field == ResearchField::Construction ? 15 : 23;
        assert(!stargate_component_available(low, 1, C::InfinityStargate));
    }
    for (const auto& unlock : research_unlocks()) if (unlock.name == "Infinity / Infinity Gate") {
        assert(research_unlock_available(s, 1, unlock));
        assert(research_unlock_requirement(unlock).find("Stargate Specialist") != std::string::npos);
        s.players[0].race.primaryTrait = PrimaryRaceTrait::Generalist;
        assert(!research_unlock_applicable(s, 1, unlock));
        assert(!stargate_component_available(s, 1, C::InfinityStargate));
        assert(stargate_component_available(s, 1, C::JumpGate));
    }
    for (const auto trait : {PrimaryRaceTrait::HabitatCivilization, static_cast<PrimaryRaceTrait>(255)}) {
        setup.primaryTrait = trait; bool rejected = false;
        try { (void)generate_campaign({}, {setup}); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    auto ship = s.shipDesigns.front(); ship.components.push_back(C::InfinityStargate);
    assert(!ship_design_valid(ship) && !component_available_to_player(s, 1, C::InfinityStargate));
}
void gate_fittings_are_paid_and_cannot_be_stacked()
{
    auto s = network();
    StationLoadout fit{OrbitalStationHullType::Fortress, {C::InfinityStargate}, true, true};
    assert(station_loadout_available(s, 1, fit));
    assert(station_build_cost(fit) == 1100);
    const auto cost = station_mineral_cost(fit);
    assert(cost.ironium == 350 && cost.boranium == 260 && cost.germanium == 580);
    fit.hull = OrbitalStationHullType::Starbase; assert(!station_loadout_valid(fit));
    fit.hull = OrbitalStationHullType::Fortress; fit.components.push_back(C::JumpGate);
    assert(!station_loadout_valid(fit));
    fit.components = {C::InfinityStargate};
    const auto old = s.orbitalStations[0].components;
    assert(queue_station_construction(s, 1, {1, {fit, 1}}));
    assert(s.planets[0].productionQueue[0].remainingCost == station_build_cost(fit));
    assert(s.orbitalStations[0].components == old);
    auto result = TurnProcessor{}.process(s, {});
    for (int i = 0; i < 20 && !result.planets[0].productionQueue.empty(); ++i)
        result = TurnProcessor{}.process(result, {});
    assert(result.planets[0].productionQueue.empty());
    assert(station_stargate(result.orbitalStations[0])->component == C::InfinityStargate);
    // Isolate exact mineral charging from the ordinary annual extraction.
    s = network(); s.planets[0].industry = 10000; s.planets[0].mines = 0;
    auto baseline = s;
    const auto reference = TurnProcessor{}.process(baseline, {}).planets[0].minerals;
    const auto paid = TurnProcessor{}.process(s, {{1, {QueueStationOrder{1, {fit, 1}}}}});
    assert(paid.planets[0].minerals.germanium == reference.germanium - cost.germanium);
    assert(paid.orbitalStations[0].id == 1);
}
void loaded_mixed_fleets_and_both_endpoint_limits()
{
    auto s = network(); auto& f = s.fleets[0];
    s.shipDesigns.push_back({30, 1, "Gate hauler", ShipHullType::LightTransport,
        {C::FusionDrive, C::CargoPod}});
    f.design = 30;
    f.colonists = 1000; f.minerals = {1, 2, 3};
    f.ships = {{f.design, 2}};
    const auto fuel = f.fuel; const auto manifest = f.ships;
    assert(stargate_transfer_valid(s, 1, f.id, 1, 2));
    const auto result = TurnProcessor{}.process_with_events(s, {{1, {GateFleetOrder{f.id, 1, 2}, GateFleetOrder{f.id, 2, 1}}}});
    assert(result.state.fleets[0].position == s.stars[1].position);
    assert(result.state.fleets[0].colonists == 1000 && result.state.fleets[0].minerals.boranium == 2);
    assert(std::equal(result.state.fleets[0].ships.begin(), result.state.fleets[0].ships.end(),
        manifest.begin(), manifest.end(), [](auto a, auto b) { return a.design == b.design && a.count == b.count; }));
    assert(result.state.fleets[0].fuel == fuel);
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
        auto finite = s; finite.orbitalStations[endpoint].components = {C::JumpGate};
        assert(!stargate_transfer_valid(finite, 1, f.id, 1, 2));
        finite.stars[1].position = {160, 0}; assert(stargate_transfer_valid(finite, 1, f.id, 1, 2));
        finite.stars[1].position = {160.0001, 0}; assert(!stargate_transfer_valid(finite, 1, f.id, 1, 2));
    }
    auto heavy = s; heavy.fleets[0].ships = {{f.design, 1000}};
    assert(!stargate_transfer_valid(heavy, 1, f.id, 1, 2));
    heavy.orbitalStations[0].components = heavy.orbitalStations[1].components = {C::InfinityStargate};
    heavy.stars[1].position = {1e12, 1e12};
    assert(stargate_transfer_valid(heavy, 1, f.id, 1, 2));
    heavy.orbitalStations[0].components = {C::NetworkStargate};
    assert(!stargate_transfer_valid(heavy, 1, f.id, 1, 2));
    s.players[0].race.primaryTrait = PrimaryRaceTrait::Generalist;
    assert(!stargate_transfer_valid(s, 1, f.id, 1, 2));
    s.orbitalStations[0].components = s.orbitalStations[1].components = {C::JumpGate};
    s.stars[1].position = {100, 0};
    assert(!stargate_transfer_valid(s, 1, f.id, 1, 2)); // Still carrying physical cargo.
    s.fleets[0].colonists = 0; s.fleets[0].minerals = {};
    assert(stargate_transfer_valid(s, 1, f.id, 1, 2));
}
void invalid_transits_do_not_move_and_reports_wait_for_delivery()
{
    for (int variant = 0; variant < 7; ++variant) {
        auto s = network();
        if (variant == 0) s.orbitalStations[1].damagePercent = 100;
        if (variant == 1) s.orbitalStations[1].components.clear();
        if (variant == 2) s.planets[1].owner = 2;
        if (variant == 3) s.fleets[0].task = FleetTask::FieldResearch;
        if (variant == 4) s.fleets[0].repeatOrders = true;
        if (variant == 5) s.fleets[0].destination = Position{500, 0};
        if (variant == 6) s.stars[1].position.x = std::numeric_limits<double>::infinity();
        const auto fuel = s.fleets[0].fuel;
        assert(!stargate_transfer_valid(s, 1, 1, 1, 2));
        assert(!transfer_fleet_through_stargate(s, 1, 1, 1, 2));
        assert(s.fleets[0].position == Position{} && s.fleets[0].fuel == fuel);
    }
    auto s = network();
    s.planets[1].radar = PlanetaryRadarLevel::None;
    const auto result = TurnProcessor{}.process_with_events(s, {{1, {GateFleetOrder{1, 1, 2}}}});
    assert(std::none_of(result.events.begin(), result.events.end(), [](const auto& e) { return e.kind == GameEventKind::StargateTransit; }));
    auto delivered = result.state;
    const auto& reports = delivered.players[0].pendingPlayerReports;
    const auto packet = std::find_if(reports.begin(), reports.end(), [](const auto& r) { return r.kind == PlayerReportKind::StargateTransit; });
    assert(packet != reports.end() && packet->deliveryTurn > delivered.turn);
    delivered.turn = packet->deliveryTurn; delivered.fleets[0].minerals = {99, 99, 99};
    const auto events = deliver_due_player_reports(delivered);
    const auto event = std::find_if(events.begin(), events.end(), [](const auto& e) { return e.kind == GameEventKind::StargateTransit; });
    assert(event != events.end() && event->position == s.stars[1].position && event->deliveredMinerals.ironium == 0);
    auto invalid = network(); invalid.orbitalStations[1].components.clear();
    const auto rejected = TurnProcessor{}.process_with_events(invalid, {{1, {GateFleetOrder{1, 1, 2}}}});
    assert(std::any_of(rejected.events.begin(), rejected.events.end(), [](const auto& e) { return e.kind == GameEventKind::StargateTransitFailed; }));
}
void computers_pay_for_networks_and_use_received_gates()
{
    auto s = network(); s.players[0].controller = PlayerController::Computer;
    s.players[0].technology.researchActive = true;
    s.stars.push_back({3, "Frontier", {740, 0}});
    s.players[0].surveyedStars = {1, 2};
    s.fleets[0].telemetry.observedTurn = s.turn;
    const auto dryNetwork = plan_computer_turn(make_player_view(s, 1));
    assert(std::none_of(dryNetwork.orders.orders.begin(), dryNetwork.orders.orders.end(),
        [](const auto& o) { return std::holds_alternative<GateFleetOrder>(o); }));
    for (auto& station : s.orbitalStations) station.modules.push_back(OrbitalStationModule::RefuelingDepot);
    for(const auto& p:s.planets)set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);
    const auto view = make_player_view(s, 1);
    const auto plan = plan_computer_turn(view);
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& o) { return std::holds_alternative<GateFleetOrder>(o); }));
    assert(std::none_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& o) {
        const auto* move = std::get_if<MoveFleetOrder>(&o); return move && move->fleet == 1;
    }));
    assert(plan.nextState == plan_computer_turn(view).nextState);
    const auto hopped = resolve_controlled_campaign_turn(s, {});
    assert(hopped.state.fleets[0].position == s.stars[1].position);
    auto waiting = hopped.state; bool resumed = false;
    for (int year = 0; year < 20; ++year) {
        const auto onward = plan_computer_turn(make_player_view(waiting, 1));
        resumed = std::any_of(onward.orders.orders.begin(), onward.orders.orders.end(), [](const auto& order) {
            const auto* move = std::get_if<MoveFleetOrder>(&order);
            return move && move->fleet == 1 && move->destination == Position{740, 0};
        });
        if (resumed) { assert(waiting.fleets[0].telemetry.observedTurn > s.turn); break; }
        waiting = resolve_controlled_campaign_turn(waiting, {}).state;
    }
    assert(resumed);
    auto researching = s;
    auto& p = researching.players[0];
    p.computer.initialized = true; p.computer.decisions = {{9, 0, "Prior research"}};
    p.technology.focus = ResearchField::Propulsion;
    p.technology.levels[std::size_t(ResearchField::Propulsion)] = 3;
    p.technology.levels[std::size_t(ResearchField::Construction)] = 9;
    p.technology.levels[std::size_t(ResearchField::Energy)] = 8;
    p.technology.levels[std::size_t(ResearchField::Electronics)] = 8;
    p.technology.levels[std::size_t(ResearchField::Biology)] = 1;
    p.computer.researchGoal = ComputerResearchGoal{ResearchField::Propulsion, 3};
    const auto science = plan_computer_turn(make_player_view(researching, 1));
    assert(science.nextState.researchGoal->field == ResearchField::Energy);
    s.fleets.clear(); s.players[0].technology.levels.fill(14);
    s.players[0].technology.levels[std::size_t(ResearchField::Construction)] = 9;
    for (auto& station : s.orbitalStations) { station.hull = OrbitalStationHullType::OrbitalDock; station.components = {C::Laser}; }
    for (auto& planet : s.planets) { planet.defenses = 100; planet.mines = 20; }
    for(const auto& planet:s.planets)set_survey_level(s,1,planet.star,SurveyLevel::DeepSurvey,s.turn);
    const auto build = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(build.orders.orders.begin(), build.orders.orders.end(), [](const auto& o) {
        const auto* station = std::get_if<QueueStationOrder>(&o);
        return station && std::find(station->construction.loadout.components.begin(),
            station->construction.loadout.components.end(), C::NetworkStargate) != station->construction.loadout.components.end();
    }));
    auto paid = s;
    for (int year = 0; year < 5; ++year) paid = resolve_controlled_campaign_turn(paid, {}).state;
    assert(station_stargate(paid.orbitalStations[0])->component == C::NetworkStargate);
    assert(station_stargate(paid.orbitalStations[1])->component == C::NetworkStargate);
    for (auto& planet : s.planets) planet.minerals = {};
    for(const auto& planet:s.planets)set_survey_level(s,1,planet.star,SurveyLevel::DeepSurvey,s.turn);
    const auto poor = plan_computer_turn(make_player_view(s, 1));
    assert(std::none_of(poor.orders.orders.begin(), poor.orders.orders.end(), [](const auto& o) { return std::holds_alternative<QueueStationOrder>(o); }));
}
void military_reinforcement_uses_a_gate_without_a_speculative_followup()
{
    auto s = network(); s.players[0].controller = PlayerController::Computer;
    s.shipDesigns.push_back({30, 1, "Gate guard", ShipHullType::Destroyer, {C::QuickJump5, C::Laser, C::Laser}});
    auto& f = s.fleets[0]; f.design = 30; f.ships = {{30, 4}}; f.telemetry.ships = f.ships;
    auto view = make_player_view(s, 1);
    ReconContact threat; threat.fleet = 900; threat.position = {710, 0}; threat.uncertainty = 5;
    threat.quality = ContactQuality::Probable; threat.sensor = ContactSensor::Passive;
    threat.observedTurn = threat.deliveryTurn = s.turn; threat.massMinimum = 10; threat.massMaximum = 80;
    view.state.players[0].enemyContacts.push_back(threat);
    const auto plan = plan_computer_turn(view);
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& order) {
        const auto* gate = std::get_if<GateFleetOrder>(&order); return gate && gate->fleet == 1 && gate->destination == 2;
    }));
    assert(std::any_of(plan.nextState.fleetGoals.begin(), plan.nextState.fleetGoals.end(), [](const auto& goal) {
        return goal.fleet == 1 && goal.kind == ComputerFleetGoalKind::Defend && goal.target == 2;
    }));
    assert(std::none_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& order) {
        const auto* move = std::get_if<MoveFleetOrder>(&order); return move && move->fleet == 1;
    }));
}
FleetArrivalAction gate_step(PlanetId source, PlanetId destination, bool resonant = false)
{
    FleetArrivalAction action{FleetArrivalActionKind::TransitStargate};
    action.gateSource=source; action.gateDestination=destination; action.gateResonant=resonant;
    return action;
}
void autonomous_gate_routes_use_separate_years_and_real_approach_fuel()
{
    auto s=network(); const auto fuel=s.fleets[0].fuel;
    MoveFleetOrder route{1,{0,0},5,gate_step(1,2),{{{700,0},5,gate_step(2,1)},{{0,25},5,{}}}};
    auto first=TurnProcessor{}.process(s,{{1,{route}}});
    assert((first.fleets[0].position==Position{700,0}) && first.fleets[0].fuel==fuel);
    assert(first.fleets[0].arrivalAction->gateSource==2 && first.fleets[0].waypointQueue.size()==1);
    auto second=TurnProcessor{}.process(first,{});
    assert((second.fleets[0].position==Position{0,0}) && second.fleets[0].fuel==fuel);
    auto third=TurnProcessor{}.process(second,{});
    assert((third.fleets[0].position==Position{0,25}) && third.fleets[0].fuel<fuel && !third.fleets[0].destination);
    // A conventional approach cannot also jump in its arrival year.
    s.fleets[0].position={-25,0}; s.fleets[0].telemetry={};
    s.fleets[0].telemetry=confirmed_fleet_telemetry(s,s.fleets[0]);
    auto approaching=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,{0,0},5,gate_step(1,2)}}}});
    assert((approaching.fleets[0].position==Position{0,0}) && approaching.fleets[0].arrivalAction);
    const auto afterFlight=approaching.fleets[0].fuel; assert(afterFlight<fuel);
    auto jumped=TurnProcessor{}.process(approaching,{});
    assert((jumped.fleets[0].position==Position{700,0}) && jumped.fleets[0].fuel==afterFlight);
    route.queuedWaypoints.pop_back(); route.repeatOrders=true;
    auto repeating=TurnProcessor{}.process(network(),{{1,{route}}});
    for(int i=0;i<5;++i) {
        assert(repeating.fleets[0].position==Position(i%2?Position{0,0}:Position{700,0}));
        repeating=TurnProcessor{}.process(repeating,{});
    }
}
void gate_route_waits_rechecks_and_never_charges_failed_pulses()
{
    for(int variant=0;variant<5;++variant) {
        auto s=network();
        if(variant==0) s.orbitalStations[1].components.clear();
        if(variant==1) s.orbitalStations[1].owner=s.planets[1].owner=2;
        if(variant==2) s.fleets[0].ships={{s.fleets[0].design,1000}};
        if(variant==3) { s.orbitalStations[1].components={C::JumpGate}; }
        if(variant==4) { s.players[0].race.primaryTrait=PrimaryRaceTrait::Generalist; s.fleets[0].minerals={1,0,0}; }
        const auto fuel=s.fleets[0].fuel;
        auto waiting=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,{0,0},5,gate_step(1,2),{{{700,25},5,{}}}}}}});
        assert(waiting.fleets[0].position==Position{} && waiting.fleets[0].fuel==fuel);
        assert(waiting.fleets[0].arrivalAction->kind==FleetArrivalActionKind::TransitStargate && waiting.fleets[0].waypointQueue.size()==1);
        auto again=TurnProcessor{}.process(waiting,{});
        assert(again.fleets[0].position==Position{} && again.fleets[0].arrivalAction);
        again.orbitalStations[1].components={C::NetworkStargate};
        again.orbitalStations[1].owner=again.planets[1].owner=1;
        again.fleets[0].ships={{again.fleets[0].design,1}}; again.fleets[0].minerals={};
        again.players[0].race.primaryTrait=PrimaryRaceTrait::StargateSpecialist;
        auto resumed=TurnProcessor{}.process(again,{});
        assert((resumed.fleets[0].position==Position{700,0}) && resumed.fleets[0].destination==Position(700,25));
    }
    auto paid=network(); paid.stars[1].position={900,0};
    for(auto& p:paid.planets) p.mines=0;
    paid.planets[0].xenocrystal.surface=7;
    auto waiting=TurnProcessor{}.process(paid,{{1,{MoveFleetOrder{1,{0,0},5,gate_step(1,2,true)}}}});
    assert(waiting.planets[0].xenocrystal.surface==7 && waiting.fleets[0].position==Position{});
    waiting.planets[0].xenocrystal.surface=8;
    auto completed=TurnProcessor{}.process(waiting,{});
    assert((completed.fleets[0].position==Position{900,0}) && completed.planets[0].xenocrystal.surface==0);
    auto later=TurnProcessor{}.process(completed,{});
    assert(later.planets[0].xenocrystal.surface==0 && !later.fleets[0].arrivalAction);
}
void remote_gate_commands_and_stops_wait_for_delivery()
{
    auto s=network(); s.planets[1].radar=PlanetaryRadarLevel::None;
    s.fleets[0].position={700,0}; s.fleets[0].telemetry={};
    s.fleets[0].telemetry=confirmed_fleet_telemetry(s,s.fleets[0]);
    const auto delay=communication_delay_turns(s,1,{700,0}); assert(delay>0);
    auto sent=TurnProcessor{}.process(s,{{1,{GateFleetOrder{1,2,1}}}});
    assert((sent.fleets[0].position==Position{700,0}) && !sent.fleets[0].pendingCommands.empty());
    const auto delivery=sent.fleets[0].pendingCommands[0].deliveryTurn;
    // Reception remains possible without a working headquarters mesh.
    sent.planets[0].radar=PlanetaryRadarLevel::None;
    while(sent.turn<=delivery) {
        const auto before=sent.turn;
        sent=TurnProcessor{}.process(sent,{});
        if(before<delivery) assert((sent.fleets[0].position==Position{700,0}));
    }
    assert(sent.fleets[0].position==Position{} && sent.fleets[0].pendingCommands.empty());
    // A held programme continues locally until the stop packet arrives.
    s.orbitalStations[0].components.clear();
    s.fleets[0].destination=s.fleets[0].position; s.fleets[0].arrivalAction=gate_step(2,1);
    MoveFleetOrder stop{1,{700,0},5}; stop.clearRoute=true;
    auto stopping=TurnProcessor{}.process(s,{{1,{stop}}});
    assert(stopping.fleets[0].arrivalAction && !stopping.fleets[0].pendingCommands.empty());
    const auto stopDelivery=stopping.fleets[0].pendingCommands[0].deliveryTurn;
    while(stopping.turn<=stopDelivery) stopping=TurnProcessor{}.process(stopping,{});
    assert(!stopping.fleets[0].destination && !stopping.fleets[0].arrivalAction);
    stopping.orbitalStations[0].components={C::NetworkStargate};
    assert((TurnProcessor{}.process(stopping,{}).fleets[0].position==Position{700,0}));
}
void gate_routes_preserve_annual_limits_through_split_and_fight_at_destination()
{
    auto s=network(); s.stars[1].position={100,0}; s.fleets[0].ships={{s.fleets[0].design,2}};
    const auto child=s.nextFleetId;
    const auto design=s.fleets[0].design;
    auto split=TurnProcessor{}.process(s,{{1,{GateFleetOrder{1,1,2},SplitFleetOrder{1,{{design,1}}},
        MoveFleetOrder{child,{100,0},5,gate_step(2,1)}}}});
    const auto findChild=[&](const GameState& state)->const Fleet& {
        const auto found=std::find_if(state.fleets.begin(),state.fleets.end(),[&](const auto& f){return f.id==child;});
        assert(found!=state.fleets.end()); return *found;
    };
    assert((findChild(split).position==Position{100,0}) && findChild(split).arrivalAction);
    auto returned=TurnProcessor{}.process(split,{}); assert(findChild(returned).position==Position{});
    // Gate arrivals join the normal battle phase at the receiving system.
    auto battle=network(); battle.players.push_back({2,"Defender"});
    battle.shipDesigns.push_back({99,2,"Gate ambush",ShipHullType::Scout,{C::FusionDrive,C::Laser}});
    auto enemy=battle.fleets[0]; enemy.id=battle.nextFleetId++; enemy.owner=2; enemy.design=99;
    enemy.ships={{99,100}}; enemy.position={700,0}; enemy.telemetry={}; battle.fleets.push_back(enemy);
    const auto fought=TurnProcessor{}.process_with_events(battle,{{1,{MoveFleetOrder{1,{0,0},5,gate_step(1,2)}}}});
    const auto foughtHere=[](const auto& r) { return r.battle && (r.position==Position{700,0}); };
    assert(std::any_of(fought.events.begin(),fought.events.end(),foughtHere)
        || std::any_of(fought.state.players[0].pendingPlayerReports.begin(),fought.state.players[0].pendingPlayerReports.end(),foughtHere));
}

void scheduled_gate_competition_spends_once_in_stable_fleet_order()
{
    auto s=network(); s.stars[1].position={900,0};
    for(auto& p:s.planets) p.mines=0;
    s.planets[0].xenocrystal.surface=8;
    auto other=s.fleets[0]; other.id=s.nextFleetId++; s.fleets.push_back(other);
    const PlayerOrders commands{1,{MoveFleetOrder{1,{0,0},5,gate_step(1,2,true)},
        MoveFleetOrder{other.id,{0,0},5,gate_step(1,2,true)}}};
    auto reversed=s; std::reverse(reversed.fleets.begin(),reversed.fleets.end());
    auto first=TurnProcessor{}.process(s,{commands});
    const auto replay=TurnProcessor{}.process(reversed,{commands});
    assert(first.planets[0].xenocrystal.surface==0 && replay.planets[0].xenocrystal.surface==0);
    for(const auto& fleet:first.fleets) {
        const auto twin=std::find_if(replay.fleets.begin(),replay.fleets.end(),[&](const auto& f){return f.id==fleet.id;});
        assert(twin!=replay.fleets.end() && twin->position==fleet.position);
        assert(fleet.position==(fleet.id==1?Position{900,0}:Position{}));
    }
    const auto held=TurnProcessor{}.process(first,{});
    assert(held.planets[0].xenocrystal.surface==0 && held.fleets[1].position==Position{});
    first.planets[0].xenocrystal.surface=8;
    const auto second=TurnProcessor{}.process(first,{});
    assert(second.planets[0].xenocrystal.surface==0 && (second.fleets[1].position==Position{900,0}));
}

void computer_stages_a_deferred_hop_from_received_remote_orbit()
{
    auto s=network(); s.players[0].controller=PlayerController::Computer;
    s.stars.push_back({3,"Frontier",{-40,0}}); s.players[0].surveyedStars={1,2};
    for(auto& station:s.orbitalStations) station.modules.push_back(OrbitalStationModule::RefuelingDepot);
    s.planets[1].radar=PlanetaryRadarLevel::None;
    auto& f=s.fleets[0]; f.position={700,0}; f.telemetry.position=f.position;
    f.telemetry.observedTurn=s.turn-1;
    for(const auto& p:s.planets)set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);
    assert(communication_delay_turns(s,1,f.position)>0);
    const auto plan=plan_computer_turn(make_player_view(s,1));
    assert(std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& order) {
        const auto* gate=std::get_if<GateFleetOrder>(&order); return gate && gate->fleet==1 && gate->source==2 && gate->destination==1;
    }));
    auto hidden=s; hidden.orbitalStations[0].components.clear();
    assert(plan.nextState==plan_computer_turn(make_player_view(hidden,1)).nextState);
    auto sent=TurnProcessor{}.process(s,{plan.orders});
    assert((sent.fleets[0].position==Position{700,0}) && !sent.fleets[0].pendingCommands.empty());
    const auto due=sent.fleets[0].pendingCommands[0].deliveryTurn;
    while(sent.turn<=due) sent=TurnProcessor{}.process(sent,{});
    assert(sent.fleets[0].position==Position{});
}

void one_delivered_programme_loads_gates_unloads_and_continues_flight()
{
    auto s=network();
    for(auto& p:s.planets) { p.mines=0; p.productionQueue.clear(); }
    s.shipDesigns.push_back({30,1,"Gate freighter",ShipHullType::LightTransport,{C::FusionDrive,C::CargoPod}});
    auto& fleet=s.fleets[0]; fleet.design=30; fleet.ships={{30,1}};
    fleet.fuel=fleet_fuel_capacity(s,fleet);
    const auto fuel=fleet.fuel, capacity=fleet_cargo_capacity(s,fleet);
    const auto baseline2=TurnProcessor{}.process(TurnProcessor{}.process(s,{}),{});
    const auto baseline3=TurnProcessor{}.process(baseline2,{});
    const MoveFleetOrder programme{1,{0,0},5,{FleetArrivalActionKind::LoadAllAvailable,1,FleetCargoKind::Ironium},
        {{{0,0},5,gate_step(1,2)},{{700,0},5,{FleetArrivalActionKind::UnloadAll,1,FleetCargoKind::Ironium}},{{700,25},5,{}}}};
    auto loaded=TurnProcessor{}.process(s,{{1,{programme}}});
    assert(loaded.fleets[0].minerals.ironium==capacity && loaded.fleets[0].position==Position{});
    auto hopped=TurnProcessor{}.process(loaded,{});
    assert((hopped.fleets[0].position==Position{700,0}) && hopped.fleets[0].minerals.ironium==capacity);
    assert(hopped.fleets[0].fuel==fuel && std::abs(hopped.planets[1].minerals.ironium-baseline2.planets[1].minerals.ironium)<1e-6);
    auto unloaded=TurnProcessor{}.process(hopped,{});
    assert(unloaded.fleets[0].minerals.ironium==0 && std::abs(unloaded.planets[1].minerals.ironium-baseline3.planets[1].minerals.ironium-capacity)<1e-6);
    auto onward=TurnProcessor{}.process(unloaded,{});
    assert((onward.fleets[0].position==Position{700,25}) && onward.fleets[0].fuel<fuel && !onward.fleets[0].destination);
}

void gate_route_knowledge_and_authoring_do_not_follow_hidden_fittings()
{
    auto s=network(); auto hidden=s; hidden.orbitalStations[1].components.clear();
    const auto action=gate_step(1,2);
    auto known=make_player_view(s,1).state, unchanged=make_player_view(hidden,1).state;
    assert(submit_fleet_route_command(s,1,1,{0,0},5,action,{}));
    assert(submit_fleet_route_command(hidden,1,1,{0,0},5,action,{}));
    assert(submit_fleet_route_command(known,1,1,{0,0},5,action,{}));
    assert(submit_fleet_route_command(unchanged,1,1,{0,0},5,action,{}));
    // Position extrapolation may approach a source, but never confirms its hop.
    auto telemetry=s.fleets[0].telemetry; telemetry.arrivalAction=action; telemetry.destination=Position{};
    s.fleets[0].telemetry=telemetry; s.turn+=20;
    assert(projected_fleet_position(s,s.fleets[0])==Position{});
    auto malformed=action; malformed.gateSource=malformed.gateDestination;
    assert(!submit_fleet_route_command(s,1,1,{0,0},5,malformed,{}));
    assert(!submit_fleet_route_command(s,1,1,{0,1},5,action,{}));
}

}
int main()
{
    autonomous_gate_routes_use_separate_years_and_real_approach_fuel();
    gate_route_waits_rechecks_and_never_charges_failed_pulses();
    remote_gate_commands_and_stops_wait_for_delivery();
    gate_route_knowledge_and_authoring_do_not_follow_hidden_fittings();
    gate_routes_preserve_annual_limits_through_split_and_fight_at_destination();
    scheduled_gate_competition_spends_once_in_stable_fleet_order();
    computer_stages_a_deferred_hop_from_received_remote_orbit();
    one_delivered_programme_loads_gates_unloads_and_continues_flight();
    setup_and_research_are_trait_specific();
    gate_fittings_are_paid_and_cannot_be_stacked();
    loaded_mixed_fleets_and_both_endpoint_limits();
    invalid_transits_do_not_move_and_reports_wait_for_delivery();
    computers_pay_for_networks_and_use_received_gates();
    military_reinforcement_uses_a_gate_without_a_speculative_followup();
}
