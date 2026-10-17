#include "suns/campaign.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/turn_processor.hpp"
#include "suns/stations.hpp"

#include <algorithm>
#include <cassert>
#include <string>

using namespace suns;
using C = ShipComponentType;

namespace {
const Fleet& fleet(const GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(),
        [&](const auto& f) { return f.id == id; });
    assert(it != state.fleets.end());
    return *it;
}

GameState survey_state()
{
    auto state = make_demo_game();
    state.turn = 10;
    state.wormholeRules.spawnChancePerTurn = 0;
    state.stars.resize(2); state.planets.resize(2); state.fleets.clear();
    state.stars[1].position = {1000, 0};
    state.stars[1].variability = {};
    state.players.push_back({2, "Foreign"});
    for (auto& player : state.players) {
        player.race = race_preset(RacePreset::Terran);
        player.technology.levels.fill(26);
        player.technology.researchActive = false;
        player.surveyKnowledge.clear(); player.surveyedStars.clear();
        player.pendingSurveyReports.clear();
    }
    auto& planet = state.planets[1];
    planet.owner = 2; planet.population = 100000; planet.industry = 100;
    planet.minerals = {1000, 1000, 1000};
    planet.environment = {24, 50, 20}; planet.naturalEnvironment.reset();
    planet.productionQueue.clear();
    return state;
}

void aud001_frozen_environment()
{
    auto state = survey_state();
    set_survey_level(state, 1, 2, SurveyLevel::GeologicalSurvey, 4);
    const auto before = make_player_view(state, 1);
    const auto hab = known_planet_habitability(state, 1, 2);
    assert(hab);
    // Ordinary paid production changes physical axes without any new observer.
    const auto result = TurnProcessor{}.process_with_events(state,
        {{2, {QueueProductionBatchOrder{2, ProductionKind::Terraforming, 1}}}});
    assert(result.state.planets[1].environment.temperature != state.planets[1].environment.temperature);
    const auto after = make_player_view(result.state, 1);
    assert(after.state.planets.back().environment.temperature == before.state.planets.back().environment.temperature);
    assert(bool(after.state.planets.back().naturalEnvironment) == bool(before.state.planets.back().naturalEnvironment));
    assert(known_planet_habitability(result.state, 1, 2) == hab);
    assert(system_intel_age(result.state, 1, 2) == 7);
    assert(result.state.players[0].pendingSurveyReports.empty());

    // In-flight packets also freeze the observed axes, not the delivery state.
    auto delayed = state;
    delayed.fleets = {{1, 1, "Observer", FleetRole::Scout, kScoutDesignId, {1000, 0}}};
    delayed.fleets[0].fuel = 200;
    observe_current_sensor_coverage(delayed, delayed.turn);
    delayed.fleets.clear();
    delayed.planets[1].environment.temperature = 80;
    delayed.turn = 100;
    (void)deliver_due_survey_reports(delayed);
    assert(make_player_view(delayed, 1).state.planets.back().environment.temperature == 24);

    // Basic scans keep only a rough estimate; they cannot disclose raw axes.
    auto basic = state;
    basic.players[0].surveyKnowledge.clear();
    set_survey_level(basic, 1, 2, SurveyLevel::BasicScan, 10);
    const auto rough = known_planet_habitability(basic, 1, 2);
    basic.planets[1].environment.temperature = 90;
    assert(known_planet_habitability(basic, 1, 2) == rough);
}

void aud002_freshness_and_depth()
{
    auto state = survey_state(); state.turn = 7;
    auto& player = state.players[0];
    player.pendingSurveyReports = {
        {2, 1, 4, 7, SurveyLevel::DeepSurvey, PlayerId{0}},
        {2, 9, 6, 7, SurveyLevel::OrbitalSurvey, PlayerId{2}}};
    auto reversed = state;
    std::reverse(reversed.players[0].pendingSurveyReports.begin(), reversed.players[0].pendingSurveyReports.end());
    (void)deliver_due_survey_reports(state);
    (void)deliver_due_survey_reports(reversed);
    assert(survey_level(state, 1, 2) == SurveyLevel::DeepSurvey);
    assert(known_planet_owner(state, 1, 2) == 2);
    assert(system_intel_age(state, 1, 2) == 1);
    assert(known_planet_owner(reversed, 1, 2) == 2);
    // An older high-quality packet received later cannot roll back ownership.
    player.pendingSurveyReports = {{2, 1, 3, 7, SurveyLevel::DeepSurvey, PlayerId{0}}};
    (void)deliver_due_survey_reports(state);
    assert(known_planet_owner(state, 1, 2) == 2);

    // Exercise queue coalescing through the actual observation API.
    auto queued = survey_state(); queued.turn = 6;
    queued.fleets = {{1, 1, "Observer", FleetRole::Scout, kScoutDesignId, {1000, 0}}};
    queued.players[0].pendingSurveyReports = {{2, 1, 4, 7, SurveyLevel::DeepSurvey, PlayerId{0}}};
    observe_current_sensor_coverage(queued, 6);
    assert(std::any_of(queued.players[0].pendingSurveyReports.begin(), queued.players[0].pendingSurveyReports.end(),
        [](const auto& report) { return report.star == 2 && report.observedTurn == 6 && report.observedOwner == 2; }));
}

GameState gate_network()
{
    auto state = make_demo_game(); state.turn = 10;
    state.wormholeRules.spawnChancePerTurn = 0;
    state.stars.resize(3); state.planets.resize(3); state.orbitalStations.clear();
    state.players[0].technology.levels.fill(26);
    state.players[0].technology.researchActive = false;
    for (unsigned i = 0; i < 3; ++i) {
        state.stars[i].id = i + 1; state.stars[i].position = {100.0 * i, 0};
        auto& planet = state.planets[i]; planet.id = i + 1; planet.star = i + 1;
        planet.owner = 1; planet.population = 1000; planet.productionQueue.clear();
        state.orbitalStations.push_back({i + 1, 1, i + 1, "Gate", OrbitalStationHullType::Starbase,
            {}, {C::JumpGate}});
    }
    state.nextOrbitalStationId = 4;
    state.fleets = {{1, 1, "First", FleetRole::Scout, kScoutDesignId, {0, 0}},
        {2, 1, "Second", FleetRole::Scout, kScoutDesignId, {100, 0}}};
    state.nextFleetId = 3;
    return state;
}

void aud003_gate_reorganization()
{
    auto state = gate_network();
    const auto merged = TurnProcessor{}.process(state, {{1, {GateFleetOrder{1, 1, 2},
        MergeFleetsOrder{2, 1}, GateFleetOrder{2, 2, 3}}}});
    assert(merged.fleets.size() == 1 && fleet_ship_count(fleet(merged, 2)) == 2);
    assert(same_position(fleet(merged, 2).position, {100, 0}));

    state.fleets.resize(1); state.fleets[0].ships = {{kScoutDesignId, 2}};
    const auto split = TurnProcessor{}.process(state, {{1, {GateFleetOrder{1, 1, 2},
        SplitFleetOrder{1, {{kScoutDesignId, 1}}}, GateFleetOrder{3, 2, 3}, GateFleetOrder{1, 2, 3}}}});
    assert(split.fleets.size() == 2);
    for (const auto& f : split.fleets) assert(same_position(f.position, {100, 0}));
    // The restriction expires at the next annual planning boundary.
    const auto next = TurnProcessor{}.process(split, {{1, {GateFleetOrder{3, 2, 3}}}});
    assert(same_position(fleet(next, 3).position, {200, 0}));
}

GameState radiation_rendezvous(bool unsafePursuer, std::uint64_t targetColonists)
{
    auto state = make_demo_game(); state.wormholeRules.spawnChancePerTurn = 0;
    const auto safe = unsafePursuer ? C::RadiatingRamScoopDrive : C::FusionDrive;
    state.shipDesigns = {{10, 1, "Pursuer", ShipHullType::MediumTransport, {safe, safe}},
        {11, 1, "Target", ShipHullType::MediumTransport, {C::RadiatingRamScoopDrive, C::RadiatingRamScoopDrive}}};
    for (const auto& design : state.shipDesigns) assert(ship_design_valid(design));
    state.nextShipDesignId = 12;
    state.fleets = {{1, 1, "Pursuer", FleetRole::ColonyShip, 10, {-100, 0}, Position{-80, 0}, 8, 500, 1000},
        {2, 1, "Target", FleetRole::ColonyShip, 11, {-80, 0}, Position{-20, 0}, 4, 500, targetColonists}};
    state.fleets[0].targetFleet = 2;
    state.fleets[0].arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
    state.nextFleetId = 3;
    return state;
}

void aud004_radiation_after_merge()
{
    for (bool unsafe : {false, true}) for (std::uint64_t passengers : {0, 1000}) {
        auto state = radiation_rendezvous(unsafe, passengers);
        const auto result = TurnProcessor{}.process(state, {});
        assert(result.fleets.size() == 1);
        assert(fleet(result, 2).position.x > -73); // Travels further after contact.
        assert(fleet(result, 2).colonists == 900 + passengers * 9 / 10);
        std::reverse(state.fleets.begin(), state.fleets.end());
        const auto reversed = TurnProcessor{}.process(state, {});
        assert(fleet(reversed, 2).colonists == fleet(result, 2).colonists);
    }
}
} // namespace

int main(int argc, char** argv)
{
    const std::string selected = argc > 1 ? argv[1] : "all";
    if (selected == "all" || selected == "AUD-001") aud001_frozen_environment();
    if (selected == "all" || selected == "AUD-002") aud002_freshness_and_depth();
    if (selected == "all" || selected == "AUD-003") aud003_gate_reorganization();
    if (selected == "all" || selected == "AUD-004") aud004_radiation_after_merge();
}
