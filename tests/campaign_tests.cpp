#include "suns/campaign.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace suns;

int main()
{
    const std::vector<EmpireSetup> setup{{"Terrans", RacePreset::Terran},
        {"Ice", RacePreset::Cryophile}, {"Embers", RacePreset::Radiotroph}};
    auto state = generate_campaign(GalaxyConfig{}, setup);
    assert(state.players.size() == 3 && state.fleets.size() == 3);
    assert(state.shipDesigns.size() == 6 && state.nextShipDesignId == 7);
    for (const auto& player : state.players) {
        const auto colony = std::find_if(state.planets.begin(), state.planets.end(),
            [&](const Planet& p) { return p.owner == player.id; });
        assert(colony != state.planets.end());
        assert(current_planet_habitability(state, *colony, state.turn) == 100);
        assert(population_capacity(state, *colony, state.turn) == 2'500'000);
        assert(colony_has_orbital_service(state, colony->id, player.id, OrbitalStationModule::Shipyard));
        assert(state.fleets[player.id - 1].owner == player.id);
        assert(fleet_design(state, state.fleets[player.id - 1])->owner == player.id);
    }
    const auto terran = race_preset(RacePreset::Terran);
    const auto ice = race_preset(RacePreset::Cryophile);
    const auto hot = race_preset(RacePreset::Radiotroph);
    assert(race_habitability(terran, {20, 50, 20}, 0) == 0);
    assert(race_habitability(terran, {20, 50, 20}, 1) > 0);
    assert(race_habitability(ice, {20, 40, 20}, 0) == 100);
    assert(race_habitability(hot, {70, 65, 100}, 0) == 100);
    assert(race_habitability(hot, {50, 50, 20}, 0) == 0);

    // The host enforces racial colonization constraints; habitat research
    // changes which worlds can actually be settled, not just their UI color.
    auto colonyTrial = state;
    auto& target = *std::find_if(colonyTrial.planets.begin(), colonyTrial.planets.end(),
        [](const Planet& planet) { return planet.owner == 0; });
    const auto targetId = target.id;
    target.environment = {20, 50, 20};
    auto& trialStar = *std::find_if(colonyTrial.stars.begin(), colonyTrial.stars.end(),
        [&](const StarSystem& star) { return star.id == target.star; });
    trialStar.variability = {};
    colonyTrial.players[0].surveyKnowledge.push_back({target.star, SurveyLevel::OrbitalSurvey, 1});
    colonyTrial.players[0].surveyedStars.push_back(target.star);
    colonyTrial.fleets[0].design = 2;
    colonyTrial.fleets[0].position = trialStar.position;
    colonyTrial.fleets[0].colonists = 100;
    const auto rejectedColony = resolve_campaign_turn(colonyTrial,
        {{1, {ColonizePlanetOrder{1, targetId}}}, {2, {}}, {3, {}}}).state;
    assert(find_planet_at_star(rejectedColony, target.star)->owner == 0);
    colonyTrial.players[0].technology.levels[static_cast<std::size_t>(ResearchField::Biology)] = 1;
    const auto founded = resolve_campaign_turn(colonyTrial,
        {{1, {ColonizePlanetOrder{1, targetId}}}, {2, {}}, {3, {}}}).state;
    assert(find_planet_at_star(founded, target.star)->owner == 1);

    auto legacy = make_demo_game();
    legacy.planets[0].habitability = 0;
    legacy.stars[0].variability = {4, 20, 1};
    assert(current_planet_habitability(legacy, legacy.planets[0], 1) == 20);

    bool rejected = false;
    try { (void)resolve_campaign_turn(state, {{1, {}}, {2, {}}}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { (void)resolve_campaign_turn(state, {{1, {}}, {2, {}}, {2, {}}}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    std::vector<PlayerOrders> orders{
        {3, {SetResearchPlanOrder{ResearchField::Biology}, SetResearchAllocationOrder{100}}},
        {2, {SetResearchPlanOrder{ResearchField::Propulsion}, SetResearchAllocationOrder{100},
            // Cannot redirect another player's fleet or spend its colony output.
            MoveFleetOrder{1, {999, 999}, 5}, QueueShipDesignOrder{1, 3}}},
        {1, {SetResearchPlanOrder{ResearchField::Energy}, SetResearchAllocationOrder{100}}},
    };
    auto next = resolve_campaign_turn(state, orders).state;
    std::reverse(orders.begin(), orders.end());
    const auto replay = resolve_campaign_turn(state, orders).state;
    assert(next.turn == 2 && same_position(next.fleets[0].position, state.fleets[0].position));
    assert(next.planets[0].productionQueue.empty());
    for (int i = 0; i < 3; ++i) assert(next.players[i].technology.progress == replay.players[i].technology.progress);
    next = resolve_campaign_turn(next, {{1, {}}, {2, {}}, {3, {}}}).state;
    next = resolve_campaign_turn(next, {{1, {}}, {2, {}}, {3, {}}}).state;
    assert(technology_level(next, 1, ResearchField::Energy) == 1);
    assert(technology_level(next, 2, ResearchField::Propulsion) == 1);
    assert(technology_level(next, 3, ResearchField::Biology) == 1);
    assert(component_available_to_player(next, 2, ShipComponentType::AdvancedFusionDrive));
    assert(!component_available_to_player(next, 1, ShipComponentType::AdvancedFusionDrive));

    auto& observer = state.players[1];
    observer.pendingSurveyReports.push_back({1, 2, 1, 99, SurveyLevel::DeepSurvey});
    observer.pendingPlayerReports.push_back({});
    auto view = make_player_view(state, 2);
    assert(view.state.galaxySeed == 0 && view.state.players.size() == 1);
    assert(view.state.players.front().id == 2);
    assert(view.state.players.front().pendingSurveyReports.empty());
    assert(view.state.players.front().pendingPlayerReports.empty());
    assert(!find_planet_at_star(view.state, 1));
    for (const auto& design : view.state.shipDesigns) assert(design.owner == 2);
    for (const auto& fleet : view.state.fleets) {
        assert(fleet.owner == 2);
        assert(fleet.pendingCommands.empty() && fleet.telemetryInTransit.empty());
    }
    const auto& home = view.state.planets.front();
    assert(home.observedHabitability == 100);
    const auto* physical = find_planet_at_star(state, home.star);
    assert(planet_mineral_concentration(view.state, home).ironium
        == planet_mineral_concentration(state, *physical).ironium);

    auto& surveyedPlanet = *std::find_if(state.planets.begin(), state.planets.end(),
        [](const Planet& planet) { return planet.owner == 0; });
    surveyedPlanet.precursorArtifacts = {true, false, 0, 123};
    set_survey_level(state, 2, surveyedPlanet.star, SurveyLevel::DeepSurvey, state.turn);
    auto surveyedView = make_player_view(state, 2);
    assert(known_precursor_artifact_hint(surveyedView.state, 2, surveyedPlanet.id).value_or(false));
    assert(find_planet_at_star(surveyedView.state, surveyedPlanet.star)->precursorArtifacts.researchPoints == 0);
    auto& variableStar = *std::find_if(state.stars.begin(), state.stars.end(),
        [&](const StarSystem& star) { return star.id == surveyedPlanet.star; });
    variableStar.variability = {12, 17, 5};
    for (auto& knowledge : state.players[1].surveyKnowledge)
        if (knowledge.star == variableStar.id) knowledge.level = SurveyLevel::OrbitalSurvey;
    surveyedView = make_player_view(state, 2);
    const auto variability = known_stellar_variability(surveyedView.state, 2, variableStar.id);
    assert(variability && variability->variable && !variability->characterized);
    assert(variability->periodTurns == 0 && variability->amplitudePercent == 0);

    // A detected enemy exposes only its contact position, never cargo or route.
    state.fleets[0].position = state.fleets[1].position;
    state.fleets[0].colonists = 999;
    state.fleets[0].destination = Position{456, 789};
    view = make_player_view(state, 2);
    const auto contact = std::find_if(view.state.fleets.begin(), view.state.fleets.end(),
        [](const Fleet& fleet) { return fleet.owner == 1; });
    assert(contact != view.state.fleets.end());
    assert(contact->colonists == 0 && contact->fuel == 0 && !contact->destination);
    assert(contact->design == 0 && contact->ships.empty());
    assert(!find_player(view.state, 1));
    std::cout << "Campaign, racial habitats, independent research, turn collection and player projection passed\n";
}
