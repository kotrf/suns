#include "suns/survey_probes.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/computer_player.hpp"
#include "suns/player_knowledge.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
using namespace suns;
namespace {
GameState world()
{
    auto s = make_demo_game(); s.turn = 10; s.wormholeRules.spawnChancePerTurn = 0;
    s.players.resize(1); auto& player = s.players[0]; player.technology.levels.fill(26); player.technology.researchActive = false;
    player.surveyKnowledge.clear(); player.pendingSurveyReports.clear(); player.surveyedStars.clear();
    auto home = s.planets[0]; home.productionQueue.clear(); home.mines = 0; home.industry = 1000; home.minerals = {10000, 10000, 10000};
    auto star = s.stars[0]; star.position = {0, 0}; star.variability = {}; s.stars = {star}; s.planets = {home};
    for (StarId id = 2; id <= 5; ++id) {
        star.id = id; star.name = "Field " + std::to_string(id); star.position = {900.0 + 20.0 * (id - 2), 0}; s.stars.push_back(star);
        Planet p; p.id = id; p.star = id; p.name = star.name; s.planets.push_back(p);
    }
    s.fleets.clear(); s.shipDesigns.push_back(basic_survey_probe_design(1, 200)); s.nextShipDesignId = 201;
    Fleet f; f.id = 1; f.owner = 1; f.name = "Survey probe"; f.design = 200; f.ships = {{200, 1}};
    f.position = {900, 0}; f.fuel = fleet_fuel_capacity(s, f);
    f.telemetry.observedTurn = s.turn; f.telemetry.position = f.position; f.telemetry.ships = f.ships; f.telemetry.fuel = f.fuel;
    s.fleets.push_back(f); s.nextFleetId = 2; set_survey_level(s, 1, 1, SurveyLevel::DeepSurvey, s.turn);
    return s;
}
ProbeProgram programme()
{ ProbeProgram p; p.center = {930, 0}; p.radius = 100; p.maxSystems = 4; return p; }
GameState next(const GameState& s, std::vector<Order> orders = {}) { return TurnProcessor{}.process(s, {{1, std::move(orders)}}); }
void configure(GameState& s) { apply_probe_command(s, s.fleets[0], {ProbeAction::Configure, programme(), {{1, SurveyLevel::DeepSurvey, s.turn}}}); }
void paid_design_and_technology()
{
    auto s = world(); auto d = basic_survey_probe_design(1, 200);
    assert(ship_design_valid(d) && ship_design_available_to_player(s, 1, d));
    assert(hull_spec(d.hull).mass == 12 && ship_design_mass(d) > 15 && ship_design_cost(d) >= 160);
    s.players[0].technology.levels[unsigned(ResearchField::Electronics)] = 17;
    assert(!ship_hull_available_to_player(s, 1, d.hull) && !component_available_to_player(s, 1, ShipComponentType::SurveyAutopilot));
    s.players[0].technology.levels.fill(26); d.hull = ShipHullType::Frigate; assert(!ship_design_valid(d));
    auto& p = s.planets[0]; p.productionQueue.clear(); const auto minerals = p.minerals;
    const auto built = next(s, {QueueShipDesignOrder{p.id, 200}});
    assert(built.fleets.size() == 2 && built.planets[0].minerals.ironium < minerals.ironium);
    assert(built.fleets.back().design == 200 && !built.fleets.back().probe.configured);
}
void autonomous_expedition_and_knowledge_delay()
{
    auto s = world(); const auto delay = communication_delay_turns(s, 1, s.fleets[0].position); assert(delay > 0);
    const auto start = s.turn; s = next(s, {SetProbeProgramOrder{1, ProbeAction::Configure, programme()}});
    assert(!s.fleets[0].probe.configured && s.fleets[0].pendingCommands.size() == 1);
    std::map<StarId, std::uint64_t> orbital;
    bool aheadOfHQ = false, usedLocalKnowledge = false; const auto initialFuel = s.fleets[0].fuel;
    for (int year = 0; year < 45; ++year) {
        const auto& f = s.fleets[0];
        usedLocalKnowledge |= f.probe.configured && f.probe.target >= 3
            && probe_survey_level(f, 2) == SurveyLevel::DeepSurvey && survey_level(s, 1, 2) < SurveyLevel::DeepSurvey;
        if (s.turn < start + delay) assert(!f.probe.configured);
        for (const auto& k : f.probe.surveys) if (k.star != 1 && k.level >= SurveyLevel::OrbitalSurvey) {
            if (!orbital.contains(k.star)) orbital[k.star] = k.observedTurn;
            if (k.level == SurveyLevel::DeepSurvey) {
                assert(k.observedTurn >= orbital[k.star] + 2);
                aheadOfHQ |= survey_level(s, 1, k.star) < k.level;
            }
        }
        if (f.probe.configured && !f.probe.enabled) break;
        s = next(s);
    }
    assert(aheadOfHQ && usedLocalKnowledge && s.fleets[0].probe.completed.size() >= 3);
    for (StarId id = 2; id <= 5; ++id) assert(probe_survey_level(s.fleets[0], id) == SurveyLevel::DeepSurvey);
    assert(s.fleets[0].probe.status == ProbeStatus::Complete && same_position(s.fleets[0].position, Position{900, 0}));
    assert(s.fleets[0].fuel < initialFuel && s.fleets[0].fuel >= 0);
}
void hidden_world_and_other_notebooks_do_not_plan_for_this_probe()
{
    auto s = world(); configure(s); record_probe_survey(s.fleets[0], 2, SurveyLevel::DeepSurvey, s.turn);
    auto changed = s;
    changed.planets[2].xenocrystal = {48, 1, 47, 4}; changed.planets[2].minerals = {99999, 99999, 99999}; changed.planets[2].owner = 2;
    changed.unknownObjects = {{7, {930, 0}, UnknownObjectKind::AncientProbe, true}}; changed.nextUnknownObjectId = 8;
    changed.players.push_back({2, "Other"}); set_survey_level(changed, 1, 3, SurveyLevel::DeepSurvey, s.turn);
    auto other = changed.fleets[0]; other.id = 2; other.probe.enabled = false;
    record_probe_survey(other, 3, SurveyLevel::DeepSurvey, s.turn); changed.fleets.push_back(other);
    advance_survey_probes(s); advance_survey_probes(changed);
    assert(s.fleets[0].probe.target == 3 && changed.fleets[0].probe.target == 3);
    assert(same_position(*s.fleets[0].destination, *changed.fleets[0].destination));
    auto reordered = world(); configure(reordered); record_probe_survey(reordered.fleets[0], 2, SurveyLevel::DeepSurvey, reordered.turn);
    reordered.stars[2].position = {920, 0}; reordered.stars[3].position = {900, 20};
    std::reverse(reordered.stars.begin(), reordered.stars.end()); advance_survey_probes(reordered);
    assert(reordered.fleets[0].probe.target == 3);
    auto host = world(); configure(host);
    host.fleets[0].telemetry.probe = host.fleets[0].probe;
    host.fleets[0].telemetry.probe.surveys.clear();
    auto forecast = make_player_view(host, 1).state; advance_survey_probes(forecast);
    assert(!forecast.fleets[0].destination && forecast.fleets[0].probe.target == 0);
}
void delayed_manual_control_stop_and_knowledge_upload()
{
    auto s = world(); configure(s); auto& f = s.fleets[0];
    const auto delay = communication_delay_turns(s, 1, f.position); const auto start = s.turn;
    s = next(s, {MoveFleetOrder{1, {1100, 50}, 5}});
    while (s.turn < start + delay) { assert(s.fleets[0].probe.enabled); s = next(s); }
    assert(!s.fleets[0].probe.enabled && s.fleets[0].probe.status == ProbeStatus::ManualPause);
    s = next(s); assert(!s.fleets[0].probe.enabled);
    // The upload carries issue-time evidence, not whatever HQ knows at receipt.
    auto upload = world(); configure(upload); set_survey_level(upload, 1, 3, SurveyLevel::OrbitalSurvey, upload.turn);
    assert(submit_probe_command(upload, 1, {1, ProbeAction::UpdateKnowledge}));
    set_survey_level(upload, 1, 3, SurveyLevel::DeepSurvey, upload.turn + 1);
    upload.turn += communication_delay_turns(upload, 1, upload.fleets[0].position);
    deliver_due_fleet_commands(upload);
    assert(probe_survey_level(upload.fleets[0], 3) == SurveyLevel::OrbitalSurvey);
    auto stop = world(); configure(stop); assert(submit_probe_command(stop, 1, {1, ProbeAction::Stop}));
    assert(stop.fleets[0].probe.enabled);
    stop.turn = stop.fleets[0].pendingCommands[0].deliveryTurn; deliver_due_fleet_commands(stop);
    assert(!stop.fleets[0].probe.enabled && stop.fleets[0].probe.status == ProbeStatus::Stopped);
    apply_probe_command(stop, stop.fleets[0], {ProbeAction::Resume}); advance_survey_probes(stop); assert(stop.fleets[0].probe.enabled);
    record_probe_survey(stop.fleets[0], 4, SurveyLevel::DeepSurvey, stop.turn);
    apply_probe_command(stop, stop.fleets[0], {ProbeAction::Configure, programme()});
    assert(probe_survey_level(stop.fleets[0], 4) == SurveyLevel::DeepSurvey && stop.fleets[0].probe.completed.empty());
    pause_probe(stop.fleets[0]); stop.fleets[0].destination = Position{1100, 0};
    apply_probe_command(stop, stop.fleets[0], {ProbeAction::Stop});
    assert(!stop.fleets[0].destination && stop.fleets[0].probe.status == ProbeStatus::Stopped);
}
void fuel_damage_equipment_and_reorganization()
{
    auto s = world(); configure(s); s.fleets[0].fuel = 0;
    auto dry = next(s); assert(dry.fleets[0].probe.status == ProbeStatus::AwaitingFuel && dry.fleets[0].fuel == 0);
    assert(same_position(dry.fleets[0].position, s.fleets[0].position));
    dry.fleets[0].fuel = 300; dry.fleets[0].damagePercent = 30; advance_survey_probes(dry);
    assert(dry.fleets[0].probe.status == ProbeStatus::Damaged && !dry.fleets[0].destination);
    dry.fleets[0].damagePercent = 0; advance_survey_probes(dry); assert(dry.fleets[0].probe.status == ProbeStatus::Surveying);
    std::erase(dry.shipDesigns.back().components, ShipComponentType::SurveyAutopilot); advance_survey_probes(dry);
    assert(!dry.fleets[0].probe.enabled && dry.fleets[0].probe.status == ProbeStatus::MissingEquipment);
    auto split = world(); configure(split); split.fleets[0].ships = {{200, 2}};
    split = next(split, {SplitFleetOrder{1, {{200, 1}}}});
    assert(split.fleets.size() == 2 && !split.fleets[0].probe.enabled && !split.fleets[1].probe.enabled);
    record_probe_survey(split.fleets[0], 3, SurveyLevel::GeologicalSurvey, split.turn);
    record_probe_survey(split.fleets[1], 4, SurveyLevel::DeepSurvey, split.turn);
    auto merge = next(split, {MergeFleetsOrder{1, 2}});
    assert(merge.fleets.size() == 1 && !merge.fleets[0].probe.enabled);
    assert(probe_survey_level(merge.fleets[0], 4) == SurveyLevel::DeepSurvey);
    auto arrival = split; arrival.fleets[1].targetFleet = 1; arrival.fleets[1].destination = arrival.fleets[0].position;
    arrival.fleets[1].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
    arrival = next(arrival); assert(arrival.fleets.size() == 1 && !arrival.fleets[0].probe.enabled);
    assert(probe_survey_level(arrival.fleets[0], 4) == SurveyLevel::DeepSurvey);
    auto limit = world(); configure(limit); limit.fleets[0].probe.program.maxTurns = 1;
    limit = next(next(limit)); assert(!limit.fleets[0].probe.enabled && limit.fleets[0].probe.status == ProbeStatus::Complete);
    auto done = world(); configure(done);
    for (StarId id = 2; id <= 5; ++id) record_probe_survey(done.fleets[0], id, SurveyLevel::DeepSurvey, done.turn);
    advance_survey_probes(done); assert(!done.fleets[0].probe.enabled && done.fleets[0].probe.status == ProbeStatus::Complete);
}
void computer_builds_and_launches_paid_probe()
{
    auto s = world(); s.fleets.clear(); s.shipDesigns.pop_back(); s.players[0].controller = PlayerController::Computer;
    s.players[0].computer.initialized = true; s.players[0].computer.decisions = {{9, 0, "Prior plan"}};
    auto plan = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& o) { const auto* q = std::get_if<QueueShipDesignOrder>(&o); return q && q->pendingDesignName == "Autonomous Survey Probe"; }));
    s = next(s, plan.orders.orders); assert(!s.fleets.empty());
    s.players[0].computer = plan.nextState; plan = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& o) { return std::holds_alternative<SetProbeProgramOrder>(o); }));
}
}
int main()
{
    paid_design_and_technology(); autonomous_expedition_and_knowledge_delay();
    hidden_world_and_other_notebooks_do_not_plan_for_this_probe(); delayed_manual_control_stop_and_knowledge_upload();
    fuel_damage_equipment_and_reorganization(); computer_builds_and_launches_paid_probe();
    std::cout << "Autonomous survey programmes, local knowledge and physical logistics passed\n";
}
