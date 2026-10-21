#include "suns/exotic_materials.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/computer_player.hpp"
#include "suns/combat.hpp"
#include "suns/mobile_base.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/production.hpp"
#include "suns/science_samples.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace suns;
namespace {
bool near(double a, double b) { return std::abs(a - b) < 1e-5; }
GameState fixture()
{
    auto s = make_demo_game(); s.turn = 10; s.wormholeRules.spawnChancePerTurn = 0;
    s.players[0].technology.levels.fill(16); s.players[0].technology.progress.fill(0);
    s.players[0].technology.researchActive = false;
    auto& p = s.planets[0]; p.industry = 1000; p.mines = 0; p.productionQueue.clear(); p.minerals = {10000, 10000, 10000};
    s.orbitalStations[0].hull = OrbitalStationHullType::Starbase;
    s.shipDesigns[0].hull = ShipHullType::SmallFreighter;
    s.shipDesigns[0].components = {ShipComponentType::FuelMizer, ShipComponentType::MoleScanner}; s.shipDesigns[0].placements.clear();
    s.fleets.resize(1); auto& f = s.fleets[0]; f.ships = {{f.design, 1}}; f.colonists = 0; f.minerals = {}; f.destination.reset();
    f.telemetry = {}; f.telemetryInTransit.clear(); f.fuel = fleet_fuel_capacity(s, f); s.nextFleetId = 2;
    for (auto& star : s.stars) star.variability = {};
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    return s;
}
GameState next(const GameState& s, std::vector<Order> orders = {})
{ return TurnProcessor{}.process(s, {{1, std::move(orders)}}); }
GameState resonance_network()
{
    auto s = fixture(); s.stars.resize(2); s.planets.resize(2);
    s.stars[0].position = {0, 0}; s.stars[1].position = {250, 0};
    s.players[0].technology.levels.fill(18); s.orbitalStations.clear();
    for (std::size_t i = 0; i < 2; ++i) {
        auto& p = s.planets[i]; p.owner = 1; p.population = 1000000; p.productionQueue.clear();
        p.mines = 0; p.industry = 1000;
        s.orbitalStations.push_back({p.id, 1, p.id, "Paid gate", OrbitalStationHullType::Starbase,
            {OrbitalStationModule::Shipyard}, {ShipComponentType::JumpGate}});
    }
    s.nextOrbitalStationId = 3; s.planets[0].xenocrystal.surface = 16;
    s.fleets[0].position = s.stars[0].position;
    s.fleets[0].telemetry = confirmed_fleet_telemetry(s, s.fleets[0]);
    for (std::size_t i = 0; i < 2; ++i) set_survey_level(s, 1, s.planets[i].star, SurveyLevel::DeepSurvey, s.turn);
    return s;
}
void resonant_gates_pay_once_and_preserve_normal_limits()
{
    auto s = resonance_network(); s.planets[1].xenocrystal.surface = 16;
    assert(!stargate_transfer_valid(s, 1, 1, 1, 2));
    assert(stargate_transfer_valid(s, 1, 1, 1, 2, true));
    const auto fuel = s.fleets[0].fuel;
    const auto result = TurnProcessor{}.process_with_events(s, {{1, {GateFleetOrder{1, 1, 2, true}, GateFleetOrder{1, 2, 1, true}}}});
    assert(result.state.fleets[0].position == s.stars[1].position && result.state.fleets[0].fuel == fuel);
    assert(result.state.planets[0].xenocrystal.surface == 8 && result.state.planets[1].xenocrystal.surface == 16);
    assert(known_xenocrystal(result.state, 1, 1)->site.surface == 8);
    const auto paid = std::count_if(result.state.players[0].pendingPlayerReports.begin(), result.state.players[0].pendingPlayerReports.end(),
        [](const auto& r) { return r.kind == PlayerReportKind::StargateTransit && r.quantity == 8; });
    const auto received = std::count_if(result.events.begin(), result.events.end(),
        [](const auto& e) { return e.kind == GameEventKind::StargateTransit && e.quantity == 8; });
    assert(paid + received == 1);
    for (int defect = 0; defect < 11; ++defect) {
        auto bad = s;
        if (defect < 3) bad.players[0].technology.levels[std::size_t(defect == 0 ? ResearchField::Energy
            : defect == 1 ? ResearchField::Electronics : ResearchField::Construction)] = defect == 2 ? 15 : 17;
        if (defect == 3) { bad.planets[0].xenocrystal.surface = 7; bad.fleets[0].xenocrystal = 8; bad.planets[1].xenocrystal.surface = 8; }
        if (defect == 4) bad.orbitalStations[1].damagePercent = 100;
        if (defect == 5) bad.planets[1].owner = 2;
        if (defect == 6) bad.stars[1].position = {320.001, 0};
        if (defect == 7) bad.fleets[0].ships[0].count = 1000;
        if (defect == 8) bad.fleets[0].minerals = {1, 0, 0};
        if (defect == 9) bad.fleets[0].destination = Position{20, 0};
        if (defect == 10) bad.fleets[0].task = FleetTask::RemoteMining;
        const auto stock = bad.planets[0].xenocrystal.surface;
        assert(!transfer_fleet_through_stargate(bad, 1, 1, 1, 2, true));
        assert(bad.planets[0].xenocrystal.surface == stock && bad.fleets[0].position == s.fleets[0].position);
    }
    auto ordinary = s; ordinary.stars[1].position = {160, 0}; ordinary.planets[0].xenocrystal.surface = 0;
    ordinary.players[0].technology.levels.fill(9);
    assert(transfer_fleet_through_stargate(ordinary, 1, 1, 1, 2));
    assert(!ordinary.planets[0].xenocrystal.surface);
    auto unnecessary = s; unnecessary.stars[1].position = {160, 0};
    assert(!transfer_fleet_through_stargate(unnecessary, 1, 1, 1, 2, true));
    assert(unnecessary.planets[0].xenocrystal.surface == 16);
    auto boundary = s; boundary.stars[1].position = {320, 0};
    assert(stargate_transfer_valid(boundary, 1, 1, 1, 2, true));
    // The smaller endpoint binds even against an Infinity gate; mass stays finite too.
    boundary.players[0].race.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
    boundary.orbitalStations[1].hull = OrbitalStationHullType::Fortress;
    boundary.orbitalStations[1].components = {ShipComponentType::InfinityStargate};
    assert(stargate_transfer_valid(boundary, 1, 1, 1, 2, true));
    boundary.stars[1].position = {321, 0}; assert(!stargate_transfer_valid(boundary, 1, 1, 1, 2, true));
    auto two = s; two.planets[0].xenocrystal.surface = 8;
    auto second = two.fleets[0]; second.id = two.nextFleetId++; two.fleets.push_back(second);
    const auto contested = next(two, {GateFleetOrder{1, 1, 2, true}, GateFleetOrder{second.id, 1, 2, true}});
    assert(contested.fleets[0].position == s.stars[1].position && contested.fleets[1].position == s.stars[0].position);
    assert(!contested.planets[0].xenocrystal.surface);
}
void resonant_gate_forecasts_reserve_only_received_early_deliveries()
{
    auto s = resonance_network(); s.planets[0].xenocrystal.surface = 8;
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    PlayerOrders orders{1, {GateFleetOrder{1, 1, 2, true}, SetResonanceLaboratoryOrder{1}}};
    auto preview = planned_production_state(projected_cargo_order_state(make_player_view(s, 1).state, orders), orders);
    assert(!preview.planets[0].xenocrystal.surface);
    assert(!forecast_production_queue(preview, preview.planets[0], preview.planets[0].productionQueue, 16)[0].completionTurn);
    auto after = next(s, orders.orders);
    assert(!after.planets[0].xenocrystal.surface && !after.planets[0].resonanceLab.work);
    auto stale = make_player_view(s, 1).state;
    stale.players[0].surveyKnowledge[0].planetSnapshot->xenocrystal->observedTurn = s.turn - 1;
    assert(!preview_stargate_transfer(stale, 1, {1, 1, 2, true}));
    auto hidden = s; hidden.planets[0].xenocrystal.surface = 0;
    assert(make_player_view(s, 1).state.planets[0].xenocrystal == make_player_view(hidden, 1).state.planets[0].xenocrystal);
    assert(!next(hidden, {GateFleetOrder{1, 1, 2, true}}).planets[0].xenocrystal.surface);
    // A genuinely local, current unloading order precedes the jump; material aboard is not payment.
    s.planets[0].xenocrystal.surface = 0; s.fleets[0].xenocrystal = 8;
    s.fleets[0].telemetry = {}; s.fleets[0].telemetry = confirmed_fleet_telemetry(s, s.fleets[0]);
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    PlayerOrders delivered{1, {TransferXenocrystalOrder{{0, 1}, {1, 0}, 8}, GateFleetOrder{1, 1, 2, true}}};
    auto estimate = projected_cargo_order_state(make_player_view(s, 1).state, delivered);
    assert(!estimate.planets[0].xenocrystal.surface && !estimate.fleets[0].xenocrystal);
    assert(estimate.fleets[0].position == s.stars[1].position);
    after = next(s, delivered.orders);
    assert(!after.planets[0].xenocrystal.surface && after.fleets[0].position == s.stars[1].position);
    auto reversed = next(s, {GateFleetOrder{1, 1, 2, true}, delivered.orders[0]});
    assert(reversed.planets[0].xenocrystal.surface == 8 && reversed.fleets[0].position == s.stars[0].position);
}

void computer_uses_only_received_resonance_supplies()
{
    auto s = resonance_network(); s.players[0].controller = PlayerController::Computer;
    s.players[0].race.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
    s.stars.push_back({3, "Frontier", {300, 0}}); s.players[0].surveyedStars = {1, 2};
    for (auto& station : s.orbitalStations) station.modules.push_back(OrbitalStationModule::RefuelingDepot);
    const auto plan = plan_computer_turn(make_player_view(s, 1));
    const auto pulse = std::find_if(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& order) {
        const auto* gate = std::get_if<GateFleetOrder>(&order); return gate && gate->fleet == 1 && gate->resonant;
    });
    assert(pulse != plan.orders.orders.end());
    assert(std::get<GateFleetOrder>(*pulse).destination == 2);
    const auto resolved = next(s, {*pulse});
    assert(resolved.planets[0].xenocrystal.surface == 8 && resolved.fleets[0].position == s.stars[1].position);
    auto hidden = s; hidden.planets[0].xenocrystal.surface = 0;
    const auto same = plan_computer_turn(make_player_view(hidden, 1));
    assert(plan.nextState == same.nextState);
    auto stale = make_player_view(s, 1);
    stale.state.players[0].surveyKnowledge[0].planetSnapshot->xenocrystal->observedTurn = s.turn - 1;
    const auto refused = plan_computer_turn(stale);
    assert(std::none_of(refused.orders.orders.begin(), refused.orders.orders.end(), [](const auto& order) {
        const auto* gate = std::get_if<GateFleetOrder>(&order); return gate && gate->resonant;
    }));
}

void resonant_gate_reports_wait_and_freeze_payment()
{
    auto s = resonance_network(); s.players[0].race.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
    s.stars[1].position = {900, 0}; s.planets[1].radar = PlanetaryRadarLevel::None;
    for (auto& station : s.orbitalStations) station.components = {ShipComponentType::NetworkStargate};
    const auto result = TurnProcessor{}.process_with_events(s, {{1, {GateFleetOrder{1, 1, 2, true}}}});
    assert(result.state.fleets[0].position == s.stars[1].position && result.state.planets[0].xenocrystal.surface == 8);
    assert(std::none_of(result.events.begin(), result.events.end(), [](const auto& e) { return e.kind == GameEventKind::StargateTransit; }));
    auto delayed = result.state;
    const auto packet = std::find_if(delayed.players[0].pendingPlayerReports.begin(), delayed.players[0].pendingPlayerReports.end(),
        [](const auto& r) { return r.kind == PlayerReportKind::StargateTransit; });
    assert(packet != delayed.players[0].pendingPlayerReports.end() && packet->deliveryTurn > delayed.turn);
    delayed.turn = packet->deliveryTurn; delayed.planets[0].xenocrystal.surface = 99;
    const auto events = deliver_due_player_reports(delayed);
    const auto event = std::find_if(events.begin(), events.end(), [](const auto& e) { return e.kind == GameEventKind::StargateTransit; });
    assert(event != events.end() && event->quantity == 8 && event->position == s.stars[1].position);
}

void resonant_gate_projection_replays_accepted_fleet_commands()
{
    auto s = resonance_network(); s.planets[0].xenocrystal.surface = 8;
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    const GateFleetOrder gate{1, 1, 2, true};
    const MoveFleetOrder move{1, {0, 50}, 5};
    PlayerOrders moving{1, {move, gate}};
    auto projected = projected_cargo_order_state(make_player_view(s, 1).state, moving);
    assert(projected.planets[0].xenocrystal.surface == 8);
    assert(projected.fleets[0].position == s.stars[0].position && projected.fleets[0].destination == move.destination);
    auto resolved = next(s, moving.orders);
    assert(resolved.planets[0].xenocrystal.surface == 8 && (resolved.fleets[0].position == Position{0, 25}));
    // No imaginary reservation: the laboratory or a second fleet may use the eight units.
    auto labOrders = moving; labOrders.orders.push_back(SetResonanceLaboratoryOrder{1});
    auto lab = planned_production_state(projected_cargo_order_state(make_player_view(s, 1).state, labOrders), labOrders);
    assert(forecast_production_queue(lab, lab.planets[0], lab.planets[0].productionQueue, 16)[0].completionTurn);
    assert(next(s, labOrders.orders).planets[0].resonanceLab.work == 60);
    auto other = s.fleets[0]; other.id = s.nextFleetId++; s.fleets.push_back(other);
    auto competing = moving; competing.orders.push_back(GateFleetOrder{other.id, 1, 2, true});
    projected = projected_cargo_order_state(make_player_view(s, 1).state, competing);
    assert(!projected.planets[0].xenocrystal.surface && projected.fleets[1].position == s.stars[1].position);
    resolved = next(s, competing.orders);
    assert(!resolved.planets[0].xenocrystal.surface && resolved.fleets[1].position == s.stars[1].position);
    MoveFleetOrder clear{1, s.stars[0].position, 0}; clear.clearRoute = true;
    for (const auto& commands : {std::vector<Order>{move, clear, gate},
            std::vector<Order>{MoveFleetOrder{1, {0, 50}, 255}, gate},
            std::vector<Order>{SetFleetTaskOrder{1, FleetTask::FieldResearch}, SetFleetTaskOrder{1, FleetTask::None}, gate},
            std::vector<Order>{SetFleetTaskOrder{1, FleetTask::LayMines}, gate}}) {
        projected = projected_cargo_order_state(make_player_view(s, 1).state, {1, commands});
        resolved = next(s, commands);
        assert(!projected.planets[0].xenocrystal.surface && !resolved.planets[0].xenocrystal.surface);
        assert(projected.fleets[0].position == s.stars[1].position && resolved.fleets[0].position == s.stars[1].position);
    }
    auto routed = s; routed.fleets[0].destination = move.destination; routed.fleets[0].telemetry.destination = move.destination;
    projected = projected_cargo_order_state(make_player_view(routed, 1).state, {1, {clear, gate}});
    assert(!projected.planets[0].xenocrystal.surface && next(routed, {clear, gate}).fleets[0].position == s.stars[1].position);
    const std::vector<Order> working{SetFleetTaskOrder{1, FleetTask::FieldResearch}, gate};
    projected = projected_cargo_order_state(make_player_view(s, 1).state, {1, working});
    assert(projected.planets[0].xenocrystal.surface == 8 && next(s, working).planets[0].xenocrystal.surface == 8);
    auto heavy = s; heavy.fleets.resize(1); heavy.fleets[0].ships[0].count = 20;
    heavy.fleets[0].telemetry.ships = heavy.fleets[0].ships;
    assert(!stargate_transfer_valid(heavy, 1, 1, 1, 2, true));
    const std::vector<Order> split{SplitFleetOrder{1, {{heavy.fleets[0].design, 10}}}, gate};
    projected = projected_cargo_order_state(make_player_view(heavy, 1).state, {1, split});
    assert(!projected.planets[0].xenocrystal.surface && next(heavy, split).fleets[0].position == s.stars[1].position);
    auto merging = s;
    for (auto& fleet : merging.fleets) { fleet.ships[0].count = 10; fleet.telemetry.ships = fleet.ships; }
    const std::vector<Order> merged{MergeFleetsOrder{1, other.id}, gate};
    projected = projected_cargo_order_state(make_player_view(merging, 1).state, {1, merged});
    assert(projected.planets[0].xenocrystal.surface == 8 && next(merging, merged).planets[0].xenocrystal.surface == 8);
}

void resonant_gate_requires_received_manifest_through_reorganization()
{
    const auto gate = GateFleetOrder{1, 1, 2, true};
    auto unknown = resonance_network();
    unknown.planets[0].xenocrystal.surface = 8;
    set_survey_level(unknown, 1, 1, SurveyLevel::DeepSurvey, unknown.turn);
    unknown.fleets[0].position = {900, 0}; unknown.fleets[0].telemetry = {};
    auto view = make_player_view(unknown, 1).state;
    assert(view.fleets[0].telemetry.observedTurn == 0 && view.fleets[0].position == unknown.stars[0].position);
    assert(!preview_stargate_transfer(view, 1, gate));
    auto projected = projected_cargo_order_state(make_player_view(unknown, 1).state, {1, {gate}});
    assert(projected.planets[0].xenocrystal.surface == 8);
    auto resolved = next(unknown, {gate});
    assert(resolved.planets[0].xenocrystal.surface == 8 && (resolved.fleets[0].position == Position{900, 0}));
    auto receiver = resonance_network().fleets[0]; receiver.id = unknown.nextFleetId++;
    unknown.fleets.push_back(receiver);
    const std::vector<Order> unknownMerge{MergeFleetsOrder{receiver.id, 1}, GateFleetOrder{receiver.id, 1, 2, true}};
    projected = projected_cargo_order_state(make_player_view(unknown, 1).state, {1, unknownMerge});
    assert(projected.planets[0].xenocrystal.surface == 8 && !projected.fleets[0].telemetry.observedTurn);
    // Both resulting IDs inherit the age of the source evidence, including after a later merge.
    for (const auto age : {0U, 1U}) {
        auto s = resonance_network(); s.planets[0].xenocrystal.surface = 8;
        set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
        s.fleets[0].ships[0].count = 2; s.fleets[0].telemetry.ships = s.fleets[0].ships;
        s.fleets[0].telemetry.observedTurn = s.turn - age;
        if (age) s.fleets[0].position = {900, 0};
        const auto detached = s.nextFleetId;
        for (const auto fleetId : {FleetId{1}, detached}) {
            const std::vector<Order> orders{SplitFleetOrder{1, {{s.fleets[0].design, 1}}}, GateFleetOrder{fleetId, 1, 2, true}};
            projected = projected_cargo_order_state(make_player_view(s, 1).state, {1, orders});
            resolved = next(s, orders);
            assert(projected.planets[0].xenocrystal.surface == (age ? 8 : 0));
            assert(resolved.planets[0].xenocrystal.surface == (age ? 8 : 0));
            if (age) for (const auto& fleet : projected.fleets) assert(fleet.telemetry.observedTurn == s.turn - age);
        }
        const std::vector<Order> regrouped{SplitFleetOrder{1, {{s.fleets[0].design, 1}}}, MergeFleetsOrder{1, detached}, gate};
        projected = projected_cargo_order_state(make_player_view(s, 1).state, {1, regrouped});
        assert(projected.planets[0].xenocrystal.surface == (age ? 8 : 0));
        // A current destination cannot make stale incoming ships current.
        auto other = s.fleets[0]; other.id = s.nextFleetId++; other.position = s.stars[0].position;
        other.telemetry.observedTurn = s.turn; s.fleets.push_back(other);
        const std::vector<Order> mixed{MergeFleetsOrder{other.id, 1}, GateFleetOrder{other.id, 1, 2, true}};
        projected = projected_cargo_order_state(make_player_view(s, 1).state, {1, mixed});
        assert(projected.planets[0].xenocrystal.surface == (age ? 8 : 0));
    }
}

void add_miner(GameState& s, std::uint32_t count = 1)
{
    ShipDesign design{3, 1, "Crystal miner", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RoboMiniMiner}};
    assert(ship_design_available_to_player(s, 1, design)); s.shipDesigns.push_back(design); s.nextShipDesignId = 4;
    Fleet f; f.id = s.nextFleetId++; f.owner = 1; f.design = 3; f.ships = {{3, count}};
    f.position = s.stars[1].position; f.task = FleetTask::RemoteMining; f.fuel = fleet_fuel_capacity(s, f);
    s.fleets.push_back(f);
}
void clusters_scale_without_multiplying_the_deposit_budget()
{
    for (int size = 0; size < 5; ++size) for (int density = 0; density < 4; ++density) {
        const auto config = galaxy_preset(GalaxySize(size), GalaxyDensity(density), 123);
        auto s = generate_game(config);
        const auto clusters = generate_xenocrystal_deposits(s);
        assert(clusters == std::max<std::size_t>(1, s.stars.size() / 144 + (s.stars.size() % 144 >= 72)));
        assert(std::count_if(s.planets.begin(), s.planets.end(), [](const auto& p) { return p.xenocrystal.original != 0; })
            == (s.planets.size() + 31) / 32);
        if (size == int(GalaxySize::Medium) && density == int(GalaxyDensity::Normal)) assert(clusters == 2);
    }
    auto separated = fixture(); separated.stars.clear(); separated.planets.clear();
    for (std::uint32_t i = 0; i < 288; ++i) {
        separated.stars.push_back({i + 1, "Site", {i < 144 ? -5000.0 : 5000.0, double(i % 144)}, StarClass::Yellow});
        Planet p; p.id = p.star = i + 1; separated.planets.push_back(p);
    }
    separated.galaxySeed = 333;
    assert(generate_xenocrystal_deposits(separated) == 2);
    std::size_t left = 0, right = 0;
    for (const auto& p : separated.planets) if (p.xenocrystal.original) {
        (p.id <= 144 ? left : right)++; assert(p.xenocrystal.original >= 24 && p.xenocrystal.original <= 48);
    }
    assert(left >= 4 && right >= 4 && left + right == 9);
    auto reordered = separated; std::reverse(reordered.planets.begin(), reordered.planets.end());
    std::reverse(reordered.stars.begin(), reordered.stars.end()); assert(generate_xenocrystal_deposits(reordered) == 2);
    for (const auto& p : separated.planets) assert(p.xenocrystal == find_planet(reordered, p.id)->xenocrystal);
    for (auto& star : reordered.stars) star.position = {};
    assert(generate_xenocrystal_deposits(reordered) == 1); // No invented distinct centers at coincident positions.
    assert(std::count_if(reordered.planets.begin(), reordered.planets.end(), [](const auto& p) { return p.xenocrystal.original != 0; }) == 9);
    reordered.planets.resize(2); reordered.planets[0].homeworld = true;
    assert(generate_xenocrystal_deposits(reordered) == 1 && !reordered.planets[0].xenocrystal.original);
    reordered.planets[1].owner = 1; assert(generate_xenocrystal_deposits(reordered) == 0);
    reordered.planets.clear(); assert(generate_xenocrystal_deposits(reordered) == 0);
    for (const auto systems : {71U, 72U, 215U, 216U, 287U, 288U, 359U, 360U, 1001U}) {
        GalaxyConfig custom; custom.seed = 7; custom.starCount = systems; custom.width = custom.height = 4000;
        auto s = generate_game(custom);
        assert(generate_xenocrystal_deposits(s) == std::max(1U, (systems + 72) / 144));
    }
}
GameState colony_mining_fixture()
{
    auto s = fixture(); auto& p = s.planets[0]; p.xenocrystal = {30, 30, 0, 0}; p.mines = 100;
    set_survey_level(s, 1, p.star, SurveyLevel::DeepSurvey, s.turn); return s;
}
void colonies_extract_finite_whole_units_under_existing_access_rules()
{
    auto s = colony_mining_fixture();
    for (const auto mines : {0U, 24U, 25U, 49U, 50U, 99U, 100U, 10000U}) {
        auto test = s; test.planets[0].mines = mines; mine_xenocrystals(test);
        const auto rate = std::min(4U, mines / 25);
        assert(test.planets[0].xenocrystal.surface == rate && test.planets[0].xenocrystal.remaining == 30 - rate);
    }
    for (int block = 0; block < 6; ++block) {
        auto test = s;
        if (block == 0) test.planets[0].population = 0;
        if (block == 1) test.players[0].technology.levels[std::size_t(ResearchField::Construction)] = 7;
        if (block == 2) test.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 7;
        if (block == 3) test.players[0].surveyKnowledge.clear();
        if (block == 4) { test.players[0].surveyKnowledge.clear(); set_survey_level(test, 1, 1, SurveyLevel::GeologicalSurvey, test.turn); }
        if (block == 5) test.planets[0].owner = 2;
        mine_xenocrystals(test); assert(test.planets[0].xenocrystal.remaining == 30 && !test.planets[0].xenocrystal.surface);
    }
    auto full = s; full.planets[0].xenocrystal.surface = kMaximumXenocrystal;
    mine_xenocrystals(full); assert(full.planets[0].xenocrystal.remaining == 30);
    full.planets[0].xenocrystal.surface--; ++full.turn; mine_xenocrystals(full);
    assert(full.planets[0].xenocrystal.surface == kMaximumXenocrystal && full.planets[0].xenocrystal.remaining == 29);
    s.planets[0].xenocrystal.remaining = 3;
    mine_xenocrystals(s); assert(s.planets[0].xenocrystal.extractedLastYear == 3 && s.planets[0].xenocrystal.surface == 3);
    ++s.turn; mine_xenocrystals(s); assert(!s.planets[0].xenocrystal.remaining && !s.planets[0].xenocrystal.extractedLastYear);
    auto reference = colony_mining_fixture(); reference.planets[0].xenocrystal = {};
    const auto ordinary = next(reference).planets[0].minerals;
    const auto mined = next(colony_mining_fixture()).planets[0].minerals;
    assert(near(ordinary.ironium, mined.ironium) && near(ordinary.boranium, mined.boranium) && near(ordinary.germanium, mined.germanium));
}
void colony_production_and_gate_orders_observe_the_mining_phase()
{
    auto s = colony_mining_fixture(); s.planets[0].xenocrystal.surface = 4;
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    const PlayerOrders lab{1, {SetResonanceLaboratoryOrder{1}}};
    auto preview = planned_production_state(projected_cargo_order_state(make_player_view(s, 1).state, lab), lab);
    assert(preview.planets[0].xenocrystal.surface == 4); // Forecasts never manufacture current stock.
    assert(forecast_production_queue(preview, preview.planets[0], preview.planets[0].productionQueue, 16)[0].completionTurn == s.turn + 10);
    auto built = next(s, lab.orders);
    assert(built.planets[0].resonanceLab.work == 60 && !built.planets[0].xenocrystal.surface && built.planets[0].xenocrystal.remaining == 26);
    auto gates = resonance_network(); gates.planets[0].mines = 100; gates.planets[0].xenocrystal = {30, 30, 4, 0};
    set_survey_level(gates, 1, 1, SurveyLevel::DeepSurvey, gates.turn);
    auto early = next(gates, {GateFleetOrder{1, 1, 2, true}});
    assert(early.fleets[0].position == gates.stars[0].position && early.planets[0].xenocrystal.surface == 8);
    auto later = next(early, {GateFleetOrder{1, 1, 2, true}});
    assert(later.fleets[0].position == gates.stars[1].position && later.planets[0].xenocrystal.surface == 4);
    auto load = next(s, {MoveFleetOrder{1, s.stars[0].position, 1, {FleetArrivalActionKind::LoadAllAvailable, 1, FleetCargoKind::Xenocrystal}}});
    assert(load.fleets[0].xenocrystal == 8 && !load.planets[0].xenocrystal.surface);
}
void colony_mining_reports_preserve_private_dated_inputs()
{
    auto s = colony_mining_fixture(); auto& p = s.planets[0];
    s.stars[0].position = {900, 0}; p.homeworld = false;
    // Keep a real HQ at Sol's former position, but observe the remote colony by its own sensor source.
    auto hq = s.planets[1]; hq.owner = 1; hq.population = 1000000; hq.homeworld = true; s.planets[1] = hq;
    s.stars[1].position = {0, 0};
    const auto old = *known_xenocrystal(s, 1, p.id); mine_xenocrystals(s);
    publish_exotic_material_reports(s, s);
    assert(known_xenocrystal(s, 1, p.id)->site.remaining == old.site.remaining);
    const auto packet = std::find_if(s.players[0].pendingSurveyReports.begin(), s.players[0].pendingSurveyReports.end(), [&](const auto& r) {
        return r.star == p.star && r.planetSnapshot && r.planetSnapshot->xenocrystal && r.planetSnapshot->xenocrystal->stockPhase == 1;
    });
    assert(packet != s.players[0].pendingSurveyReports.end() && packet->deliveryTurn > s.turn + 1);
    const auto due = packet->deliveryTurn;
    p.mines = 0; p.xenocrystal.remaining = 0; p.xenocrystal.surface = 0;
    auto view = make_player_view(s, 1).state;
    assert(known_colony_xenocrystal_rate(view, 1, view.planets[0]) == 4);
    s.turn = due; (void)deliver_due_survey_reports(s);
    const auto received = *known_xenocrystal(s, 1, p.id);
    assert(received.site.remaining == 26 && received.site.surface == 4 && received.colony->mines == 100);
    assert(received.observedTurn == 11);
    const auto entry = std::find_if(s.players[0].surveyKnowledge.begin(), s.players[0].surveyKnowledge.end(), [&](const auto& k) { return k.star == p.star; });
    auto stale = *entry->planetSnapshot; stale.xenocrystal->observedTurn = 10; stale.xenocrystal->depositObservedTurn = 10;
    stale.xenocrystal->colony->mines = 0;
    s.players[0].pendingSurveyReports.push_back({p.star, 0, 10, s.turn, SurveyLevel::DeepSurvey, 1, 0, stale});
    (void)deliver_due_survey_reports(s);
    assert(known_xenocrystal(s, 1, p.id)->colony->mines == 100);
    const auto foreign = capture_planet_survey(s, 2, p.star, SurveyLevel::DeepSurvey, s.turn);
    assert(foreign && foreign->xenocrystal && !foreign->xenocrystal->colony && !foreign->xenocrystal->site.surface);
}

void colonization_and_capture_do_not_double_the_annual_extraction()
{
    auto s = fixture(); auto& site = s.planets[1]; s.stars[1].position = {20, 0};
    site.xenocrystal = {30, 30, 2, 0}; site.mines = 25;
    set_survey_level(s, 1, site.star, SurveyLevel::DeepSurvey, s.turn); add_miner(s, 8);
    auto& settler = s.fleets[0]; settler.design = kColonyShipDesignId; settler.ships = {{settler.design, 1}};
    settler.position = s.stars[1].position; settler.colonists = 100; settler.fuel = fleet_fuel_capacity(s, settler);
    settler.destination = settler.position; settler.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Colonize};
    auto founded = next(s);
    assert(founded.planets[1].owner == 1 && founded.planets[1].mines == 25);
    assert(founded.planets[1].xenocrystal.remaining == 26 && founded.planets[1].xenocrystal.surface == 6);
    auto operated = next(founded);
    assert(operated.planets[1].xenocrystal.remaining == 25 && operated.planets[1].xenocrystal.surface == 7);
    auto captured = colony_mining_fixture(); captured.players.push_back({2, "Invader"});
    captured.players.back().technology.levels.fill(16); captured.planets[0].population = 1;
    captured.planets[0].mines = 25; captured.orbitalStations.clear();
    captured.shipDesigns.push_back({4, 2, "Troop transport", ShipHullType::SmallFreighter, {ShipComponentType::FuelMizer}});
    auto& troops = captured.fleets[0]; troops.owner = 2; troops.design = 4; troops.ships = {{4, 1}};
    troops.colonists = 1000; troops.destination = troops.position;
    troops.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Colonists};
    captured = TurnProcessor{}.process(captured, {});
    assert(captured.planets[0].owner == 2 && captured.planets[0].xenocrystal.remaining == 29 && captured.planets[0].xenocrystal.surface == 1);
    auto withoutStudy = TurnProcessor{}.process(captured, {});
    assert(withoutStudy.planets[0].xenocrystal.remaining == 29); // Old owner's study does not authorize the invader.
    captured.planets[0].mines = 100;
    set_survey_level(captured, 2, 1, SurveyLevel::DeepSurvey, captured.turn);
    auto informed = TurnProcessor{}.process(captured, {});
    assert(informed.planets[0].xenocrystal.remaining == 25 && informed.planets[0].xenocrystal.surface == 5);
    auto own = colony_mining_fixture(); own.planets[0].mines = 25; add_miner(own, 8);
    own.fleets.back().position = own.stars[0].position;
    mine_xenocrystals(own); assert(own.planets[0].xenocrystal.surface == 1); // Robots cannot add another four on an owned world.
}

void computer_uses_own_mines_and_exports_received_colony_caches()
{
    auto s = colony_mining_fixture(); s.players[0].controller = PlayerController::Computer;
    s.planets[0].mines = 0; s.planets[0].industry = 24; s.stars.resize(1); s.planets.resize(1);
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    auto plan = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& o) {
        const auto* batch = std::get_if<QueueProductionBatchOrder>(&o);
        const auto* single = std::get_if<QueueProductionOrder>(&o);
        return (batch && batch->kind == ProductionKind::Mine) || (single && single->kind == ProductionKind::Mine);
    }));
    s = colony_mining_fixture(); s.players[0].controller = PlayerController::Computer;
    s.stars[1].position = {20, 0}; auto& recipient = s.planets[1]; recipient.owner = 1; recipient.population = 1000000;
    recipient.productionQueue.clear(); recipient.minerals = {10000, 10000, 10000};
    // The source already paid its local laboratory; a second colony still needs material.
    s.planets[0].xenocrystal.surface = 8;
    assert(apply_resonance_order(s, s.planets[0], {1})); (void)resolve_colony_production(s, s.planets[0], true);
    s.planets[0].xenocrystal.surface = 8;
    set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    set_survey_level(s, 1, 2, SurveyLevel::DeepSurvey, s.turn);
    plan = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& o) {
        const auto* move = std::get_if<MoveFleetOrder>(&o);
        return move && move->fleet == 1 && move->arrivalAction.cargo == FleetCargoKind::Xenocrystal
            && move->arrivalAction.kind == FleetArrivalActionKind::LoadAllAvailable;
    }));
    s.players[0].computer = plan.nextState;
    s = next(s, plan.orders.orders);
    assert(s.fleets[0].xenocrystal >= 8);
    const auto returning = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(returning.orders.orders.begin(), returning.orders.orders.end(), [&](const auto& o) {
        const auto* move = std::get_if<MoveFleetOrder>(&o);
        return move && move->fleet == 1 && move->destination == s.stars[1].position
            && move->arrivalAction.kind == FleetArrivalActionKind::UnloadAll;
    }));
    auto hidden = s; hidden.planets[0].xenocrystal.remaining = 0; hidden.planets[0].xenocrystal.surface = 0;
    hidden.planets[0].mines = 0; hidden.planets[0].population = 0;
    assert(plan_computer_turn(make_player_view(hidden, 1)).nextState == returning.nextState);
}

void generated_finite_geographic_deposits()
{
    GalaxyConfig c; c.seed = 123; c.starCount = 64;
    const auto a = generate_campaign(c, {{"A"}, {"B"}}), b = generate_campaign(c, {{"A"}, {"B"}});
    std::uint32_t deposits = 0;
    for (std::size_t i = 0; i < a.planets.size(); ++i) {
        const auto& p = a.planets[i]; assert(p.xenocrystal == b.planets[i].xenocrystal);
        if (!p.xenocrystal.original) continue;
        ++deposits; assert(!p.homeworld && !p.owner);
        assert(p.xenocrystal.original >= 24 && p.xenocrystal.original <= 48);
        assert(p.xenocrystal.original == p.xenocrystal.remaining && !p.xenocrystal.surface);
        for (const auto& player : a.players) assert(!known_xenocrystal(a, player.id, p.id));
    }
    assert(deposits == 2);
    auto hostile = fixture(); hostile.planets.resize(2); hostile.planets[1].owner = 0;
    hostile.planets[1].population = 0; hostile.planets[1].habitability = -100;
    hostile.planets[1].environment = {0, 0, 100}; generate_xenocrystal_deposits(hostile);
    assert(hostile.planets[1].xenocrystal.original); // No habitability filter.
}
void discovery_is_deep_delayed_and_immutable()
{
    auto s = fixture(); auto& p = s.planets[1]; p.xenocrystal = {12, 12, 0, 0};
    s.stars[1].position = {900, 0}; auto& f = s.fleets[0]; f.position = {900, 0};
    f.telemetry = {}; f.telemetry.observedTurn = s.turn; f.telemetry.position = f.position; f.telemetry.ships = f.ships;
    queue_survey_report(s, 1, p.star, f.id, f.position, s.turn, SurveyLevel::GeologicalSurvey);
    assert(!known_xenocrystal(s, 1, p.id));
    assert(!s.players[0].pendingSurveyReports.back().planetSnapshot->xenocrystal);
    queue_survey_report(s, 1, p.star, f.id, f.position, s.turn, SurveyLevel::DeepSurvey);
    const auto due = s.players[0].pendingSurveyReports.back().deliveryTurn; assert(due > s.turn);
    assert(!known_xenocrystal(s, 1, p.id));
    p.xenocrystal = {12, 3, 9, 4}; s.turn = due;
    const auto events = deliver_due_survey_reports(s);
    const auto observation = known_xenocrystal(s, 1, p.id);
    assert(observation && observation->depositSurveyed && observation->site.remaining == 12 && !observation->site.surface);
    assert(std::any_of(events.begin(), events.end(), [](const auto& e) { return e.kind == GameEventKind::XenocrystalDiscovered && e.quantity == 12; }));
    const auto view = make_player_view(s, 1).state;
    assert(view.planets[1].xenocrystal.remaining == 12);
    p.xenocrystal.remaining = 0; p.xenocrystal.surface = 1;
    assert(make_player_view(s, 1).state.planets[1].xenocrystal == view.planets[1].xenocrystal);
    // A newer low-quality orbit must preserve the dated deep evidence.
    queue_survey_report(s, 1, p.star, f.id, f.position, s.turn + 1, SurveyLevel::OrbitalSurvey);
    s.turn = s.players[0].pendingSurveyReports.back().deliveryTurn;
    (void)deliver_due_survey_reports(s);
    assert(known_xenocrystal(s, 1, p.id)->site.remaining == 12);
    p.owner = 2; p.population = 1000; p.xenocrystal.surface = 9;
    const auto foreign = capture_planet_survey(s, 1, p.star, SurveyLevel::DeepSurvey, s.turn);
    assert(foreign->xenocrystal && foreign->xenocrystal->site.surface == 0);
}
void stock_and_deposit_reports_merge_independently()
{
    auto s = fixture(); auto& knowledge = s.players[0].surveyKnowledge[0];
    knowledge.planetSnapshot->xenocrystal = XenocrystalObservation{{0, 0, 5, 0}, 10, false, 0};
    auto snapshot = *knowledge.planetSnapshot; snapshot.xenocrystal = XenocrystalObservation{{12, 9, 3, 1}, 9, true, 9};
    PendingSurveyReport r{1, 1, 9, 10, SurveyLevel::DeepSurvey, 1, 0, snapshot};
    s.players[0].pendingSurveyReports.push_back(r); (void)deliver_due_survey_reports(s);
    const auto known = known_xenocrystal(s, 1, 1);
    assert(known && known->depositSurveyed && known->site.remaining == 9);
    assert(known->site.surface == 5 && known->observedTurn == 10 && known->depositObservedTurn == 9);
}
void finite_mining_requires_received_discovery_and_equipment()
{
    auto s = fixture(); auto& p = s.planets[1]; p.xenocrystal = {9, 9, 0, 0}; add_miner(s, 8);
    mine_xenocrystals(s); assert(p.xenocrystal.remaining == 9);
    set_survey_level(s, 1, p.star, SurveyLevel::DeepSurvey, s.turn);
    s.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 7;
    mine_xenocrystals(s); assert(p.xenocrystal.remaining == 9);
    s.players[0].technology.levels[std::size_t(ResearchField::Electronics)] = 8;
    auto other = s.fleets.back(); other.id = s.nextFleetId++; s.fleets.push_back(other);
    mine_xenocrystals(s); assert(p.xenocrystal.remaining == 5 && p.xenocrystal.surface == 4);
    assert(known_xenocrystal(s, 1, p.id)->site.remaining == 9);
    ++s.turn; mine_xenocrystals(s); ++s.turn; mine_xenocrystals(s);
    assert(!p.xenocrystal.remaining && p.xenocrystal.surface == 9 && p.xenocrystal.extractedLastYear == 1);
    ++s.turn; mine_xenocrystals(s); assert(p.xenocrystal.surface == 9 && !p.xenocrystal.extractedLastYear);
}
void integer_cargo_transfers_splits_and_both_merges()
{
    auto s = fixture(); auto& f = s.fleets[0]; f.ships = {{f.design, 2}};
    f.xenocrystal = 11; f.scienceSamples = {{7, UnknownObjectKind::AncientProbe, false}};
    s.unknownObjects = {{7, f.position, UnknownObjectKind::AncientProbe, true}}; s.nextUnknownObjectId = 8;
    f.colonists = std::uint64_t((fleet_cargo_capacity(s, f) - 16) * kColonistsPerCargoUnit);
    const auto mass = fleet_cargo_used(s, f); const auto population = f.colonists;
    s = next(s, {SplitFleetOrder{1, {{f.design, 1}}}}); assert(s.fleets.size() == 2);
    assert(s.fleets[0].xenocrystal + s.fleets[1].xenocrystal == 11);
    assert(s.fleets[0].colonists + s.fleets[1].colonists == population);
    for (const auto& part : s.fleets) assert(fleet_cargo_used(s, part) <= fleet_cargo_capacity(s, part) + 1e-6);
    assert(near(fleet_cargo_used(s, s.fleets[0]) + fleet_cargo_used(s, s.fleets[1]), mass));
    s = next(s, {MergeFleetsOrder{1, 2}}); assert(s.fleets.size() == 1 && s.fleets[0].xenocrystal == 11);
    s.fleets[0].colonists = 0; s.fleets[0].scienceSamples.clear(); s.unknownObjects.clear();
    auto source = s.fleets[0]; source.id = 3; source.xenocrystal = 7; s.fleets.push_back(source); s.nextFleetId = 4;
    s = next(s, {MoveFleetOrder{3, source.position, 1, {FleetArrivalActionKind::MergeWithFleet}, {}, false, 1}});
    assert(s.fleets.size() == 1 && s.fleets[0].xenocrystal == 18);
    auto& ship = s.fleets[0]; const auto unloaded = next(s, {MoveFleetOrder{1, ship.position, 1, {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::All}}});
    assert(!unloaded.fleets[0].xenocrystal && unloaded.planets[0].xenocrystal.surface == 18);
    auto dock = unloaded; assert(transfer_xenocrystals(dock, 1, {{1, 0}, {0, 1}, 6}));
    assert(dock.fleets[0].xenocrystal == 6 && dock.planets[0].xenocrystal.surface == 12);
    assert(!transfer_xenocrystals(dock, 2, {{1, 0}, {0, 1}, 1}));
    auto& hold = dock.fleets[0]; hold.minerals.ironium = fleet_cargo_capacity(dock, hold) - 6;
    assert(!transfer_xenocrystals(dock, 1, {{1, 0}, {0, 1}, 1}));
    const auto full = next(dock, {SetFleetMineralCargoOrder{1, 1, {hold.minerals.ironium + 1, 0, 0}}});
    assert(near(full.fleets[0].minerals.ironium, hold.minerals.ironium));
}
void stale_previews_cannot_teleport_or_spend_hidden_stocks()
{
    auto s = fixture(); auto& f = s.fleets[0]; f.xenocrystal = 8;
    f.telemetry = confirmed_fleet_telemetry(s, f); f.telemetry.observedTurn = s.turn - 1; f.position = {900, 0};
    const PlayerOrders orders{1, {TransferXenocrystalOrder{{0, 1}, {1, 0}, 8}, SetResonanceLaboratoryOrder{1}}};
    const auto preview = planned_production_state(projected_cargo_order_state(make_player_view(s, 1).state, orders), orders);
    assert(preview.fleets[0].xenocrystal == 8 && !preview.planets[0].xenocrystal.surface);
    const auto forecast = forecast_production_queue(preview, preview.planets[0], preview.planets[0].productionQueue, 16);
    assert(!forecast[0].completionTurn);
    auto hidden = s; hidden.planets[0].xenocrystal.surface = 99;
    const auto frozen = projected_cargo_order_state(hidden, {1, {}});
    assert(!frozen.planets[0].xenocrystal.surface);
}
void laboratory_pays_once_pauses_resumes_and_requires_support()
{
    auto s = fixture(); auto& p = s.planets[0];
    s.players[0].technology.levels[0] = 15; assert(!apply_resonance_order(s, p, {1})); s.players[0].technology.levels[0] = 16;
    assert(apply_resonance_order(s, p, {1})); assert(!apply_resonance_order(s, p, {1}));
    p.xenocrystal.surface = 7; (void)resolve_colony_production(s, p, true); assert(!p.resonanceLab.work && !p.resonanceLab.crystalsPaid);
    p.xenocrystal.surface = 8; auto minerals = p.minerals; p.minerals = {};
    (void)resolve_colony_production(s, p, true); assert(!p.resonanceLab.crystalsPaid && p.xenocrystal.surface == 8);
    p.minerals = minerals; (void)resolve_colony_production(s, p, true); ++s.turn;
    assert(p.resonanceLab.work == 60 && p.resonanceLab.crystalsPaid == 8 && !p.xenocrystal.surface);
    assert(apply_resonance_order(s, p, {1, ResonanceAction::Pause}));
    (void)resolve_colony_production(s, p, true); assert(p.resonanceLab.work == 60);
    assert(apply_resonance_order(s, p, {1}));
    p.productionQueue.push_back(p.productionQueue.front()); // Duplicate tokens cannot double annual assembly.
    (void)resolve_colony_production(s, p, true); ++s.turn; assert(p.resonanceLab.work == 120);
    p.productionQueue.clear(); // Ordinary CancelProductionOrder has this same physical effect.
    assert(apply_resonance_order(s, p, {1})); s.orbitalStations[0].damagePercent = 100;
    (void)resolve_colony_production(s, p, true); assert(p.resonanceLab.work == 120);
    s.orbitalStations[0].damagePercent = 0;
    for (int i = 0; i < 8; ++i) { (void)resolve_colony_production(s, p, true); ++s.turn; }
    assert(p.resonanceLab.work == 600 && p.resonanceLab.crystalsPaid == 8 && p.productionQueue.empty());
    assert(near(minerals.ironium - p.minerals.ironium, 120) && near(minerals.boranium - p.minerals.boranium, 60));
    assert(resonance_operational(s, p) && resonance_state_valid(p.resonanceLab));
    assert(apply_resonance_order(s, p, {1, ResonanceAction::Dismantle})); assert(!p.xenocrystal.surface);
    assert(apply_resonance_order(s, p, {1})); (void)resolve_colony_production(s, p, true);
    assert(!p.resonanceLab.work && !p.resonanceLab.crystalsPaid);
}
void completed_laboratory_opens_parallel_observation_gated_research()
{
    auto s = fixture(); auto& p = s.planets[0]; p.xenocrystal.surface = 8; assert(apply_resonance_order(s, p, {1}));
    for (int i = 0; i < 10; ++i) { (void)resolve_colony_production(s, p, true); ++s.turn; }
    p.scienceSamples = {{7, UnknownObjectKind::AncientProbe, true}, {8, UnknownObjectKind::CompactRemnant, true}};
    s.unknownObjects = {{7, {900, 0}, UnknownObjectKind::AncientProbe, true}, {8, {800, 0}, UnknownObjectKind::CompactRemnant, true}};
    s.nextUnknownObjectId = 9;
    s.players[0].objectKnowledge = {{7, {900, 0}, 0, ObjectKnowledgeLevel::Studied, UnknownObjectKind::AncientProbe, 9, 9},
        {8, {800, 0}, 0, ObjectKnowledgeLevel::Studied, UnknownObjectKind::CompactRemnant, 9, 9}};
    auto& technology = s.players[0].technology; technology.researchActive = true;
    technology.researchAllocationPercent = 50; technology.focus = ResearchField::Weapons;
    assert(!retained_sample_research(s, p)); const auto parallel = resonance_research(s, p);
    assert(parallel[std::size_t(ResearchField::Energy)] == 32 && parallel[std::size_t(ResearchField::Electronics)] == 32);
    const auto actual = next(s);
    assert(actual.players[0].technology.progress[std::size_t(ResearchField::Energy)] == 32);
    assert(actual.players[0].technology.progress[std::size_t(ResearchField::Electronics)] == 32);
    technology.focus = ResearchField::Energy; assert(!resonance_research(s, p)[std::size_t(ResearchField::Energy)]);
    s.players[0].objectKnowledge.clear(); assert(!resonance_research(s, p)[std::size_t(ResearchField::Electronics)]);
}
void computer_uses_received_deposits_and_ordinary_orders()
{
    auto s = fixture(); s.stars[1].position = {1, 0}; s.planets[1].xenocrystal = {24, 24, 4, 0};
    set_survey_level(s, 1, s.planets[1].star, SurveyLevel::DeepSurvey, s.turn);
    add_miner(s); s.fleets[1].position = s.fleets[0].position; s.fleets[1].task = FleetTask::None;
    for (auto& f : s.fleets) f.telemetry = confirmed_fleet_telemetry(s, f);
    s.players[0].controller = PlayerController::Computer; s.players[0].computer.initialized = true;
    s.players[0].computer.decisions = {{s.turn - 1, 0, "Prior plan"}};
    const auto planned = plan_computer_turn(make_player_view(s, 1));
    bool mining = false, hauling = false;
    for (const auto& order : planned.orders.orders) if (const auto* move = std::get_if<MoveFleetOrder>(&order)) {
        mining |= move->arrivalAction.kind == FleetArrivalActionKind::RemoteMining;
        hauling |= move->arrivalAction.kind == FleetArrivalActionKind::LoadAllAvailable && move->arrivalAction.cargo == FleetCargoKind::Xenocrystal;
    }
    assert(mining && hauling);
    auto hidden = s; hidden.planets[1].xenocrystal = {24, 0, 0, 0};
    const auto unchanged = plan_computer_turn(make_player_view(hidden, 1));
    assert(unchanged.nextState.decisions.back().reason == planned.nextState.decisions.back().reason);
    assert(unchanged.orders.orders.size() == planned.orders.orders.size());
    // Physical capture is not grounds to retire a mission before its report arrives.
    auto captured = s; captured.players.push_back({2, "Other"}); captured.players[0].computer = planned.nextState;
    ++captured.turn; captured.planets[1].owner = 2; captured.planets[1].population = 1000;
    for (auto& f : captured.fleets) f.telemetry.observedTurn = captured.turn;
    const auto unaware = plan_computer_turn(make_player_view(captured, 1));
    assert(std::any_of(unaware.nextState.fleetGoals.begin(), unaware.nextState.fleetGoals.end(),
        [](const auto& goal) { return goal.kind == ComputerFleetGoalKind::ExoticMining; }));
    set_survey_level(captured, 1, 2, SurveyLevel::DeepSurvey, captured.turn);
    const auto informed = plan_computer_turn(make_player_view(captured, 1));
    assert(std::none_of(informed.nextState.fleetGoals.begin(), informed.nextState.fleetGoals.end(),
        [](const auto& goal) { return goal.target == 2 && goal.kind >= ComputerFleetGoalKind::ExoticMining; }));
}
void actual_expedition_mining_return_and_construction()
{
    auto s = fixture(); s.stars[1].position = {1, 0}; s.planets[1].xenocrystal = {12, 12, 0, 0};
    // A normal scanner stays in orbit through the existing survey progression.
    s.fleets[0].position = s.stars[1].position; s.fleets[0].telemetry = {};
    add_miner(s, 4);
    for (int i = 0; i < 5 && !known_xenocrystal(s, 1, 2); ++i) s = next(s);
    assert(known_xenocrystal(s, 1, 2)->depositSurveyed);
    s = next(s); s = next(s);
    assert(s.planets[1].xenocrystal.surface >= 8);
    const auto before = s.planets[1].xenocrystal.surface;
    s = next(s, {MoveFleetOrder{1, s.stars[1].position, 1,
        {FleetArrivalActionKind::LoadAllAvailable, 1, FleetCargoKind::Xenocrystal}}});
    assert(s.fleets[0].xenocrystal >= 8 && s.planets[1].xenocrystal.surface < before);
    const auto cargo = s.fleets[0].xenocrystal;
    s = next(s, {MoveFleetOrder{1, s.stars[0].position, 1,
        {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Xenocrystal}}, SetResonanceLaboratoryOrder{1}});
    assert(!s.fleets[0].xenocrystal && s.planets[0].resonanceLab.work == 60);
    assert(s.planets[0].resonanceLab.crystalsPaid == 8 && s.planets[0].xenocrystal.surface == cargo - 8);
    assert(known_xenocrystal(s, 1, 1)->site.surface == cargo - 8);
    const auto work = s.planets[0].resonanceLab.work;
    s = next(s, {CancelProductionOrder{1, 0}}); assert(s.planets[0].resonanceLab.work == work);
    s = next(s, {SetResonanceLaboratoryOrder{1}}); assert(s.planets[0].resonanceLab.work == work + 60);
    assert(s.planets[0].resonanceLab.crystalsPaid == 8);
}
void gate_limits_and_fuel_include_whole_crystals()
{
    auto s = fixture(); auto& f = s.fleets[0]; f.warp = 8;
    const auto empty = fleet_fuel_consumption_for_distance(s, f, 50);
    f.xenocrystal = 60;
    assert(fleet_fuel_consumption_for_distance(s, f, 50) > empty);
    s.stars[1].position = {100, 0}; s.planets[1].owner = 1; s.planets[1].population = 1000000;
    s.orbitalStations[0].components = {ShipComponentType::JumpGate};
    s.orbitalStations.push_back({2, 1, 2, "Gate", OrbitalStationHullType::Starbase,
        {OrbitalStationModule::Shipyard}, {ShipComponentType::JumpGate}});
    assert(!stargate_transfer_valid(s, 1, f.id, 1, 2)); // Ordinary gates retain the no-cargo rule.
    s.players[0].race.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
    assert(stargate_transfer_valid(s, 1, f.id, 1, 2));
    f.ships = {{f.design, 6}}; f.xenocrystal = 0;
    f.minerals.ironium = kStargateMaximumMassKt - fleet_gross_mass(s, f);
    assert(f.minerals.ironium >= 0 && fleet_cargo_used(s, f) <= fleet_cargo_capacity(s, f));
    assert(stargate_transfer_valid(s, 1, f.id, 1, 2));
    f.xenocrystal = 1; assert(!stargate_transfer_valid(s, 1, f.id, 1, 2));
}
void conquest_keeps_surface_stock_and_paid_project()
{
    auto s = fixture(); s.players.push_back({2, "Defender"}); auto& p = s.planets[0];
    p.xenocrystal.surface = 16; assert(apply_resonance_order(s, p, {1})); (void)resolve_colony_production(s, p, true);
    p.owner = 2; p.population = 1; s.orbitalStations.clear();
    auto& f = s.fleets[0]; f.colonists = 1000; f.destination = f.position;
    f.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Colonists};
    s = next(s);
    assert(s.planets[0].owner == 1 && s.planets[0].xenocrystal.surface == 8);
    assert(s.planets[0].resonanceLab.work == 60 && s.planets[0].resonanceLab.crystalsPaid == 8);
    assert(s.planets[0].productionQueue.empty() && !resonance_operational(s, s.planets[0]));
}
void partial_combat_loss_discards_only_whole_material_units()
{
    GameState s; s.turn = 10; s.galaxySeed = 1234; s.players = {{1, "Attacker"}, {2, "Carrier"}};
    s.shipDesigns = {{10, 1, "Gunboat", ShipHullType::Destroyer, {ShipComponentType::QuickJump5, ShipComponentType::Laser}},
        {20, 2, "Carrier", ShipHullType::SmallFreighter, {ShipComponentType::QuickJump5, ShipComponentType::Tritanium}}};
    Fleet attacker; attacker.id = 1; attacker.owner = 1; attacker.design = 10; attacker.ships = {{10, 1}};
    Fleet carrier; carrier.id = 2; carrier.owner = 2; carrier.design = 20; carrier.ships = {{20, 5}};
    carrier.xenocrystal = 340; s.fleets = {attacker, carrier};
    resolve_space_battles(s, 11);
    const auto survivor = std::find_if(s.fleets.begin(), s.fleets.end(), [](const auto& f) { return f.id == 2; });
    assert(survivor != s.fleets.end() && fleet_ship_count(*survivor) < 5);
    assert(survivor->xenocrystal < 340 && fleet_cargo_used(s, *survivor) <= fleet_cargo_capacity(s, *survivor) + 1e-6);
}
void spending_after_a_scan_sends_a_new_immutable_boundary_snapshot()
{
    auto s = fixture(); auto& p = s.planets[0]; p.xenocrystal.surface = 8;
    queue_survey_report(s, 1, 1, 1, s.stars[0].position, s.turn + 1, SurveyLevel::DeepSurvey);
    assert(apply_resonance_order(s, p, {1})); (void)resolve_colony_production(s, p, true);
    publish_exotic_material_reports(s, fixture());
    bool oldPacket = false, finalPacket = false;
    for (const auto& report : s.players[0].pendingSurveyReports) if (report.star == 1 && report.planetSnapshot && report.planetSnapshot->xenocrystal) {
        const auto& snapshot = *report.planetSnapshot->xenocrystal;
        oldPacket |= !snapshot.stockPhase && snapshot.site.surface == 8;
        finalPacket |= snapshot.stockPhase && snapshot.site.surface == 0;
    }
    assert(oldPacket && finalPacket); ++s.turn; (void)deliver_due_survey_reports(s);
    const auto received = known_xenocrystal(s, 1, 1);
    assert(received && !received->site.surface && received->stockPhase == 1);
    assert(!preview_xenocrystal_transfer(s, 1, {{1, 0}, {0, 1}, 1}));
}
void remote_merge_keeps_unreceived_cargo_private()
{
    auto s = fixture(); auto& f = s.fleets[0]; f.position = {900, 0}; f.xenocrystal = 8;
    f.telemetry = {}; f.telemetry.observedTurn = s.turn; f.telemetry.position = f.position; f.telemetry.ships = f.ships;
    auto target = f; target.id = 2; target.xenocrystal = 4;
    f.targetFleet = 2; f.destination = target.position; f.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
    s.fleets.push_back(target); s.nextFleetId = 3; s = next(s);
    assert(s.fleets.size() == 1 && s.fleets[0].xenocrystal == 12);
    assert(!make_player_view(s, 1).state.fleets[0].xenocrystal);
    assert(!s.fleets[0].telemetryInTransit.empty());
    s.turn = s.fleets[0].telemetryInTransit.back().deliveryTurn; deliver_due_fleet_telemetry(s);
    assert(make_player_view(s, 1).state.fleets[0].xenocrystal == 12);
}
void laboratory_completion_creates_no_ship()
{
    auto s = fixture(); s.planets[0].xenocrystal.surface = 8;
    const auto fleetCount = s.fleets.size(); const auto nextFleet = s.nextFleetId;
    s = next(s, {SetResonanceLaboratoryOrder{1}});
    for (int year = 1; year < 10; ++year) s = next(s);
    assert(s.planets[0].resonanceLab.work == kResonanceWork);
    assert(s.fleets.size() == fleetCount && s.nextFleetId == nextFleet);
    assert(s.planets[0].productionQueue.empty());
}
void owned_remote_laboratory_waits_for_received_evidence()
{
    auto s = fixture(); auto& p = s.planets[1]; s.stars[1].position = {900, 0};
    p.owner = 1; p.population = 100000; p.industry = 1000; p.mines = 0;
    p.minerals = {10000, 10000, 10000}; p.xenocrystal.surface = 8;
    s.orbitalStations.push_back({2, 1, p.id, "Remote support", OrbitalStationHullType::Starbase,
        {OrbitalStationModule::Shipyard}});
    assert(apply_resonance_order(s, p, {p.id})); (void)resolve_colony_production(s, p, true);
    set_survey_level(s, 1, p.star, SurveyLevel::DeepSurvey, s.turn);
    assert(p.resonanceLab.work == 60 && communication_delay_turns(s, 1, {900, 0}) > 0);
    s = next(s);
    assert(s.planets[1].resonanceLab.work == 120);
    assert(make_player_view(s, 1).state.planets[1].resonanceLab.work == 60);
    assert(!s.players[0].pendingSurveyReports.empty());
    const auto due = std::find_if(s.players[0].pendingSurveyReports.begin(), s.players[0].pendingSurveyReports.end(),
        [](const auto& report) { return report.star == 2 && report.planetSnapshot
            && report.planetSnapshot->resonanceLab.work == 120; });
    assert(due != s.players[0].pendingSurveyReports.end()); s.turn = due->deliveryTurn;
    (void)deliver_due_survey_reports(s);
    assert(make_player_view(s, 1).state.planets[1].resonanceLab.work == 120);
    while (s.planets[1].resonanceLab.work < kResonanceWork) s = next(s);
    assert(make_player_view(s, 1).state.planets[1].resonanceLab.work < kResonanceWork);
    const auto completion = std::find_if(s.players[0].pendingSurveyReports.begin(), s.players[0].pendingSurveyReports.end(),
        [](const auto& report) { return report.star == 2 && report.planetSnapshot
            && report.planetSnapshot->resonanceLab.work == kResonanceWork; });
    assert(completion != s.players[0].pendingSurveyReports.end()); s.turn = completion->deliveryTurn;
    (void)deliver_due_survey_reports(s);
    assert(make_player_view(s, 1).state.planets[1].resonanceLab.operational);
    assert(!resonance_scheduled(make_player_view(s, 1).state.planets[1]));
    s.players[0].surveyKnowledge.clear();
    assert(!make_player_view(s, 1).state.planets[1].resonanceLab.started);
}
GameState mobile_base_cargo_fixture()
{
    auto s = fixture(); s.players[0].technology.levels.fill(26);
    s.shipDesigns.push_back({200, 1, "Expedition", ShipHullType::ExpeditionBase,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5, ShipComponentType::QuickJump5,
         ShipComponentType::QuickJump5, ShipComponentType::ExpeditionDock, ShipComponentType::RelayArray}});
    Fleet base; base.id = 2; base.owner = 1; base.design = 200; base.ships = {{200, 1}};
    base.position = {20, 0}; base.fuel = 5000;
    auto& client = s.fleets[0]; client.position = base.position; client.xenocrystal = 8; client.minerals = {5, 0, 0};
    client.fuel = fleet_fuel_capacity(s, client); s.fleets.push_back(base); s.nextFleetId = 3;
    for (auto& f : s.fleets) f.telemetry = confirmed_fleet_telemetry(s, f);
    return s;
}
void xenocrystal_transfers_use_mobile_base_service_phase()
{
    auto s = mobile_base_cargo_fixture();
    const TransferXenocrystalOrder crystals{{0, 1}, {0, 2}, 8};
    const TransferCargoOrder minerals{{0, 1}, {0, 2}, 0, {5, 0, 0}};
    auto preview = make_player_view(s, 1).state; assert(preview_xenocrystal_transfer(preview, 1, crystals));
    const auto stationary = next(s, {crystals, minerals});
    assert(stationary.fleets[1].xenocrystal == 8 && !stationary.fleets[0].xenocrystal);
    auto almostFull = s;
    almostFull.fleets[1].minerals = {fleet_cargo_capacity(almostFull, almostFull.fleets[1]) - 8, 0, 0};
    const auto crystalsFirst = next(almostFull, {crystals, minerals});
    const auto mineralsFirst = next(almostFull, {minerals, crystals});
    assert(crystalsFirst.fleets[1].xenocrystal == 8 && near(crystalsFirst.fleets[0].minerals.ironium, 5));
    assert(!mineralsFirst.fleets[1].xenocrystal && mineralsFirst.fleets[0].xenocrystal == 8);
    assert(near(mineralsFirst.fleets[0].minerals.ironium, 0));
    auto home = s; home.fleets[1].position = home.stars[0].position; home.fleets[1].xenocrystal = 8;
    for (auto& f : home.fleets) { f.telemetry = {}; f.telemetry = confirmed_fleet_telemetry(home, f); }
    const std::vector<Order> unloadAndBuild{TransferXenocrystalOrder{{0, 2}, {1, 0}, 8}, SetResonanceLaboratoryOrder{1}};
    const auto estimate = planned_production_state(projected_cargo_order_state(make_player_view(home, 1).state,
        {1, unloadAndBuild}), {1, unloadAndBuild});
    assert(estimate.planets[0].xenocrystal.surface == 0);
    auto delivered = next(home, unloadAndBuild);
    assert(delivered.planets[0].resonanceLab.work == 0 && delivered.planets[0].xenocrystal.surface == 8);
    delivered = next(delivered);
    assert(delivered.planets[0].resonanceLab.work == 60 && delivered.planets[0].xenocrystal.surface == 0);
    for (int reason = 0; reason < 5; ++reason) {
        auto blocked = s; std::vector<Order> orders{crystals, minerals};
        if (reason == 0) orders.push_back(MoveFleetOrder{2, {24, 0}, 3});
        if (reason == 1) std::erase(blocked.shipDesigns.back().components, ShipComponentType::ExpeditionDock);
        if (reason == 2) blocked.fleets[1].mobileBase.enabled = false;
        if (reason == 3) orders.push_back(MoveFleetOrder{1, {24, 0}, 3});
        if (reason == 4) {
            auto policy = blocked.fleets[1].mobileBase; policy.enabled = false;
            orders.push_back(SetMobileBaseOrder{2, policy});
        }
        for (auto& f : blocked.fleets) { f.telemetry = {}; f.telemetry = confirmed_fleet_telemetry(blocked, f); }
        const auto result = next(blocked, orders);
        assert(result.fleets[0].xenocrystal == 8 && !result.fleets[1].xenocrystal);
        assert(near(result.fleets[0].minerals.ironium, 5));
        const auto forecast = projected_cargo_order_state(make_player_view(blocked, 1).state, {1, orders});
        assert(forecast.fleets[0].xenocrystal == 8 && !forecast.fleets[1].xenocrystal);
        auto known = make_player_view(blocked, 1).state; const PlayerOrders pendingOrders{1, orders};
        assert(!preview_xenocrystal_transfer(known, 1, crystals, &pendingOrders));
    }
    auto disabled = s; disabled.fleets[1].mobileBase.enabled = false;
    for (auto& f : disabled.fleets) { f.telemetry = {}; f.telemetry = confirmed_fleet_telemetry(disabled, f); }
    auto enabledPolicy = disabled.fleets[1].mobileBase; enabledPolicy.enabled = true;
    const PlayerOrders enableThenTransfer{1, {SetMobileBaseOrder{2, enabledPolicy}, crystals}};
    auto enabledPreview = make_player_view(disabled, 1).state;
    assert(preview_xenocrystal_transfer(enabledPreview, 1, crystals, &enableThenTransfer));
    assert(next(disabled, enableThenTransfer.orders).fleets[1].xenocrystal == 8);
    auto remote = s; for (auto& f : remote.fleets) f.position = {900, 0};
    const auto delay = communication_delay_turns(remote, 1, {900, 0}); assert(delay > 0);
    auto pending = next(remote, {crystals});
    assert(pending.fleets[0].xenocrystal == 8 && !pending.fleets[1].xenocrystal);
    assert(pending.pendingMobileBaseTransfers.size() == 1);
    auto departed = pending; departed.fleets[0].position = {901, 0};
    while (pending.turn <= remote.turn + delay) { pending = next(pending); departed = next(departed); }
    assert(!pending.fleets[0].xenocrystal && pending.fleets[1].xenocrystal == 8);
    assert(departed.fleets[0].xenocrystal == 8 && !departed.fleets[1].xenocrystal);
    assert(pending.pendingMobileBaseTransfers.empty() && departed.pendingMobileBaseTransfers.empty());
}
void mixed_surface_and_fleet_service_transfers_keep_order()
{
    for (const bool surfaceCrystals : {false, true}) for (const bool unloadCrystals : {false, true})
        for (const bool crystalsFirst : {true, false}) {
            auto s = mobile_base_cargo_fixture(); auto& base = s.fleets[1]; auto& client = s.fleets[0];
            base.position = client.position = s.stars[0].position;
            const auto initialIronium = fleet_cargo_capacity(s, base) - 8;
            base.minerals = {initialIronium, 0, 0}; base.xenocrystal = unloadCrystals ? 8 : 0;
            client.xenocrystal = !surfaceCrystals && !unloadCrystals ? 8 : 0;
            s.planets[0].xenocrystal.surface = surfaceCrystals && !unloadCrystals ? 8 : 0;
            set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
            const CargoTransferEndpoint other = surfaceCrystals ? CargoTransferEndpoint{1, 0} : CargoTransferEndpoint{0, 1};
            const CargoTransferEndpoint baseEndpoint{0, 2};
            const TransferXenocrystalOrder crystals{unloadCrystals ? baseEndpoint : other,
                unloadCrystals ? other : baseEndpoint, 8};
            const TransferCargoOrder minerals{surfaceCrystals ? CargoTransferEndpoint{0, 1} : CargoTransferEndpoint{1, 0},
                baseEndpoint, 0, {5, 0, 0}};
            const auto initialSourceIronium = surfaceCrystals ? client.minerals.ironium
                : s.planets[0].minerals.ironium + projected_mineral_mining(s, s.planets[0]).ironium;
            const auto after = next(s, crystalsFirst ? std::vector<Order>{crystals, minerals} : std::vector<Order>{minerals, crystals});
            const bool mineralsMoved = unloadCrystals ? crystalsFirst : !crystalsFirst;
            const bool crystalsMoved = unloadCrystals || crystalsFirst;
            assert(near(after.fleets[1].minerals.ironium, initialIronium + (mineralsMoved ? 5 : 0)));
            const auto sourceIronium = surfaceCrystals ? after.fleets[0].minerals.ironium : after.planets[0].minerals.ironium;
            assert(near(sourceIronium, initialSourceIronium - (mineralsMoved ? 5 : 0)));
            assert(after.fleets[1].xenocrystal == (!unloadCrystals && crystalsMoved ? 8u : 0u));
            const auto otherStock = surfaceCrystals ? after.planets[0].xenocrystal.surface : after.fleets[0].xenocrystal;
            assert(otherStock == (unloadCrystals || !crystalsMoved ? 8u : 0u));
            assert(after.fleets[1].xenocrystal + after.fleets[0].xenocrystal + after.planets[0].xenocrystal.surface == 8);
            assert(fleet_cargo_used(after, after.fleets[1]) <= fleet_cargo_capacity(after, after.fleets[1]) + 1e-6);
            assert(after.pendingMobileBaseTransfers.empty());
        }
    auto s = mobile_base_cargo_fixture(); auto& base = s.fleets[1];
    base.position = s.fleets[0].position = s.stars[0].position;
    const auto initialIronium = fleet_cargo_capacity(s, base) - 8; base.minerals = {initialIronium, 0, 0};
    s.pendingMobileBaseTransfers.push_back({1, 2, 1, 2, s.turn - 1, s.turn, 0, {}, 0, 0, false, 8});
    s.pendingMobileBaseTransfers.push_back({1, 2, 1, 2, s.turn - 1, s.turn, 0, {5, 0, 0}, 0, 0, false, 0});
    const TransferCargoOrder later{{1, 0}, {0, 2}, 0, {5, 0, 0}};
    const auto dueFirst = next(s, {later});
    assert(dueFirst.fleets[1].xenocrystal == 8 && near(dueFirst.fleets[1].minerals.ironium, initialIronium));
    assert(dueFirst.pendingMobileBaseTransfers.empty());
    auto reversed = s; std::reverse(reversed.pendingMobileBaseTransfers.begin(), reversed.pendingMobileBaseTransfers.end());
    const auto dueMineralsFirst = next(reversed, {later});
    assert(dueMineralsFirst.fleets[1].xenocrystal == 0 && near(dueMineralsFirst.fleets[1].minerals.ironium, initialIronium + 5));
    assert(dueMineralsFirst.pendingMobileBaseTransfers.empty());
    s.pendingMobileBaseTransfers.pop_back();
    s.pendingMobileBaseTransfers[0].deliveryTurn = s.turn + 2;
    auto inTransit = next(s, {later});
    assert(inTransit.pendingMobileBaseTransfers.size() == 1 && inTransit.fleets[1].xenocrystal == 0);
    assert(near(inTransit.fleets[1].minerals.ironium, initialIronium + 5));
    inTransit = next(next(inTransit));
    assert(inTransit.pendingMobileBaseTransfers.empty() && inTransit.fleets[0].xenocrystal == 8 && inTransit.fleets[1].xenocrystal == 0);
}
void computer_completes_an_ordinary_material_supply_cycle()
{
    auto s = fixture(); s.stars.resize(2); s.planets.resize(2); s.stars[1].position = {1, 0};
    s.planets[1].environment = {0, 0, 100}; s.planets[1].naturalEnvironment = s.planets[1].environment;
    s.planets[1].xenocrystal = {28, 24, 4, 0};
    set_survey_level(s, 1, 2, SurveyLevel::DeepSurvey, s.turn);
    add_miner(s); s.fleets[1].position = s.fleets[0].position; s.fleets[1].task = FleetTask::None;
    for (auto& f : s.fleets) f.telemetry = confirmed_fleet_telemetry(s, f);
    s.players[0].controller = PlayerController::Computer; s.players[0].computer.initialized = true;
    s.players[0].computer.decisions = {{s.turn - 1, 0, "Prior plan"}};
    for (int year = 0; year < 40 && s.planets[0].resonanceLab.work < kResonanceWork; ++year) {
        auto planned = plan_computer_turn(make_player_view(s, 1)); s.players[0].computer = planned.nextState;
        s = TurnProcessor{}.process(s, {planned.orders});
    }
    assert(s.planets[0].resonanceLab.work == kResonanceWork && s.planets[0].resonanceLab.crystalsPaid == 8);
    std::uint32_t surviving = 0;
    for (const auto& p : s.planets) surviving += p.xenocrystal.remaining + p.xenocrystal.surface;
    for (const auto& f : s.fleets) surviving += f.xenocrystal;
    assert(surviving + 8 == 28);
    auto support = fixture(); support.players[0].controller = PlayerController::Computer;
    support.players[0].computer.initialized = true; support.players[0].computer.decisions = {{9, 0, "Prior plan"}};
    support.orbitalStations[0].hull = OrbitalStationHullType::OrbitalDock;
    support.planets[0].xenocrystal.surface = 8; set_survey_level(support, 1, 1, SurveyLevel::DeepSurvey, support.turn);
    const auto planned = plan_computer_turn(make_player_view(support, 1));
    assert(std::any_of(planned.orders.orders.begin(), planned.orders.orders.end(), [](const auto& order) {
        const auto* station = std::get_if<QueueStationOrder>(&order);
        return station && station->construction.loadout.hull >= OrbitalStationHullType::Starbase;
    }));
    assert(apply_resonance_order(support, support.planets[0], {1}));
    support.orbitalStations[0].hull = OrbitalStationHullType::Starbase;
    (void)resolve_colony_production(support, support.planets[0], true);
    support.orbitalStations.clear(); // Paid project, no remaining crystals, supporting station lost.
    assert(support.planets[0].resonanceLab.crystalsPaid == 8 && !support.planets[0].xenocrystal.surface);
    set_survey_level(support, 1, 1, SurveyLevel::DeepSurvey, support.turn);
    const auto restore = plan_computer_turn(make_player_view(support, 1));
    assert(std::any_of(restore.orders.orders.begin(), restore.orders.orders.end(), [](const auto& order) {
        return std::holds_alternative<QueueStationOrder>(order);
    }));
}
}
int main()
{
    resonant_gate_requires_received_manifest_through_reorganization();
    resonant_gate_projection_replays_accepted_fleet_commands();
    resonant_gate_reports_wait_and_freeze_payment();
    computer_uses_only_received_resonance_supplies();
    resonant_gates_pay_once_and_preserve_normal_limits();
    resonant_gate_forecasts_reserve_only_received_early_deliveries();
    computer_uses_own_mines_and_exports_received_colony_caches();
    clusters_scale_without_multiplying_the_deposit_budget();
    colonies_extract_finite_whole_units_under_existing_access_rules();
    colonization_and_capture_do_not_double_the_annual_extraction();
    colony_production_and_gate_orders_observe_the_mining_phase();
    colony_mining_reports_preserve_private_dated_inputs();
    generated_finite_geographic_deposits(); discovery_is_deep_delayed_and_immutable(); stock_and_deposit_reports_merge_independently();
    finite_mining_requires_received_discovery_and_equipment(); integer_cargo_transfers_splits_and_both_merges();
    stale_previews_cannot_teleport_or_spend_hidden_stocks(); laboratory_pays_once_pauses_resumes_and_requires_support();
    completed_laboratory_opens_parallel_observation_gated_research(); computer_uses_received_deposits_and_ordinary_orders();
    actual_expedition_mining_return_and_construction(); gate_limits_and_fuel_include_whole_crystals();
    conquest_keeps_surface_stock_and_paid_project(); partial_combat_loss_discards_only_whole_material_units();
    spending_after_a_scan_sends_a_new_immutable_boundary_snapshot();
    remote_merge_keeps_unreceived_cargo_private();
    laboratory_completion_creates_no_ship(); owned_remote_laboratory_waits_for_received_evidence();
    xenocrystal_transfers_use_mobile_base_service_phase();
    mixed_surface_and_fleet_service_transfers_keep_order();
    computer_completes_an_ordinary_material_supply_cycle();
    std::cout << "Exotic materials: finite discovery, logistics, laboratory and fair planning passed\n";
}
