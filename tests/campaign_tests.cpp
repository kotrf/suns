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
        assert(population_capacity(state, *colony, state.turn) == 2500);
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
