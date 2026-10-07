#include "suns/scanners.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/wormholes.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

using namespace suns;

namespace {

ShipDesign design(std::initializer_list<ShipComponentType> scanners)
{
    ShipDesign result{99, 1, "Scanner test", ShipHullType::StarsScout, {ShipComponentType::QuickJump5}};
    result.components.insert(result.components.end(), scanners);
    return result;
}

void catalog_and_prerequisites()
{
    assert(scanner_technologies().size() == 16);
    const auto* rhino = scanner_technology(ShipComponentType::RhinoScanner);
    assert(rhino && rhino->ordinaryRange == 50 && rhino->penetratingRange == 0);
    assert(rhino->mass == 5 && rhino->electronics == 1 && rhino->cost == 3);
    const auto* elephant = scanner_technology(ShipComponentType::ElephantScanner);
    assert(elephant && elephant->ordinaryRange == 300 && elephant->penetratingRange == 200);
    const auto minerals = component_mineral_cost(ShipComponentType::ElephantScanner);
    assert(minerals.ironium == 8 && minerals.boranium == 5 && minerals.germanium == 14);
    auto state = generate_game(GalaxyConfig{});
    assert(fleet_sensor_range(state, state.fleets.front()) == 50);
    assert(ship_design_valid(state.shipDesigns.front()));
    assert(ship_design_available_to_player(state, 1, state.shipDesigns.front()));
    assert(component_available_to_player(state, 1, ShipComponentType::BatScanner));
    assert(!component_available_to_player(state, 1, ShipComponentType::LongRangeScanner));
    auto& tech = state.players.front().technology.levels;
    tech[static_cast<std::size_t>(ResearchField::Electronics)] = 7;
    tech[static_cast<std::size_t>(ResearchField::Energy)] = 3;
    assert(!component_available_to_player(state, 1, ShipComponentType::FerretScanner));
    tech[static_cast<std::size_t>(ResearchField::Biology)] = 2;
    assert(component_available_to_player(state, 1, ShipComponentType::FerretScanner));
    assert(!component_available_to_player(state, 1, ShipComponentType::ChameleonScanner));
    state.players.front().race.hullAccess = HullAccess::SuperStealth;
    assert(component_available_to_player(state, 1, ShipComponentType::ChameleonScanner));
    tech[static_cast<std::size_t>(ResearchField::Biology)] = 6;
    assert(!component_available_to_player(state, 1, ShipComponentType::DnaScanner));
    tech[static_cast<std::size_t>(ResearchField::Propulsion)] = 3;
    assert(component_available_to_player(state, 1, ShipComponentType::DnaScanner));
    tech[static_cast<std::size_t>(ResearchField::Electronics)] = 24;
    tech[static_cast<std::size_t>(ResearchField::Energy)] = 6;
    assert(!component_available_to_player(state, 1, ShipComponentType::PeerlessScanner));
    tech[static_cast<std::size_t>(ResearchField::Energy)] = 7;
    assert(component_available_to_player(state, 1, ShipComponentType::PeerlessScanner));
    const auto campaign = generate_campaign(GalaxyConfig{}, {{"A"}, {"B"}});
    for (const auto& fleet : campaign.fleets) {
        assert(fleet_sensor_range(campaign, fleet) == 50);
        assert(ship_design_available_to_player(campaign, fleet.owner, *find_ship_design(campaign, fleet.design)));
    }
    const auto legacy = make_demo_game();
    assert(component_available_to_player(legacy, 1, ShipComponentType::LongRangeScanner));
    assert(fleet_sensor_range(legacy, legacy.fleets.front()) == 50);
}

void host_enforces_scanner_access()
{
    auto state = generate_campaign({}, {{"Host"}});
    auto& levels = state.players.front().technology.levels;
    levels[static_cast<std::size_t>(ResearchField::Electronics)] = 7;
    levels[static_cast<std::size_t>(ResearchField::Energy)] = 3;
    const auto ferret = design({ShipComponentType::FerretScanner});
    const PlayerOrders orders{1, {CreateShipDesignOrder{ferret.name, ferret.hull, ferret.components}}};
    auto rejected = TurnProcessor{}.process(state, {orders});
    assert(rejected.shipDesigns.size() == state.shipDesigns.size());
    levels[static_cast<std::size_t>(ResearchField::Biology)] = 2;
    auto accepted = TurnProcessor{}.process(state, {orders});
    assert(accepted.shipDesigns.size() == state.shipDesigns.size() + 1);
    const auto chameleon = design({ShipComponentType::ChameleonScanner});
    const PlayerOrders restricted{1, {CreateShipDesignOrder{chameleon.name, chameleon.hull, chameleon.components}}};
    rejected = TurnProcessor{}.process(state, {restricted});
    assert(rejected.shipDesigns.size() == state.shipDesigns.size());
    state.players.front().race.hullAccess = HullAccess::SuperStealth;
    accepted = TurnProcessor{}.process(state, {restricted});
    assert(accepted.shipDesigns.size() == state.shipDesigns.size() + 1);
}

void stacking_and_channels()
{
    const auto two = design({ShipComponentType::RhinoScanner, ShipComponentType::RhinoScanner});
    assert(std::abs(ship_design_sensor_range(two) - 59.46035575) < 0.000001);
    const auto mixed = design({ShipComponentType::FerretScanner, ShipComponentType::DolphinScanner});
    assert(std::abs(ship_design_ordinary_sensor_range(mixed) - 243.47124571) < 0.00001);
    assert(std::abs(ship_design_penetrating_sensor_range(mixed) - 101.52715924) < 0.00001);
    assert(ship_design_sensor_range(mixed) == ship_design_ordinary_sensor_range(mixed));
    assert(ship_design_communication_range(mixed) == ship_design_ordinary_sensor_range(mixed));
    auto state = generate_game(GalaxyConfig{});
    state.shipDesigns.push_back(mixed);
    auto& fleet = state.fleets.front();
    fleet.design = mixed.id;
    fleet.ships = {{mixed.id, 1}};
    const auto range = fleet_sensor_range(state, fleet);
    fleet.ships.front().count = 1000;
    assert(fleet_sensor_range(state, fleet) == range);
    fleet.ships.push_back({kScoutDesignId, 1000});
    assert(fleet_sensor_range(state, fleet) == range);
    const auto pureLegacy = design({ShipComponentType::PenetratingScanner});
    assert(ship_design_ordinary_sensor_range(pureLegacy) == 0);
    assert(ship_design_penetrating_sensor_range(pureLegacy) == 70);
    assert(ship_design_communication_range(pureLegacy) == 0);
}

GameState coverage_fixture(ShipComponentType scanner)
{
    auto state = generate_game(GalaxyConfig{});
    state.stars = {{1, "Home", {}, StarClass::Yellow}, {2, "Near", {1000, 0}, StarClass::Yellow},
        {3, "Far", {1051, 0}, StarClass::Yellow}};
    state.planets.resize(1);
    state.players.front().surveyedStars = {1};
    state.players.front().surveyKnowledge.clear();
    state.players.front().pendingSurveyReports.clear();
    state.shipDesigns.push_back(design({scanner}));
    state.fleets.front().design = 99;
    state.fleets.front().ships = {{99, 1}};
    state.fleets.front().position = {1000, 0};
    return state;
}

void coverage_and_special_detector()
{
    auto bat = coverage_fixture(ShipComponentType::BatScanner);
    assert(fleet_has_scanner(bat, bat.fleets.front()));
    assert(fleet_sensor_range(bat, bat.fleets.front()) == 0);
    refresh_sensor_intel(bat);
    assert(survey_level(bat, 1, 2) == SurveyLevel::OrbitalSurvey);
    assert(survey_level(bat, 1, 3) == SurveyLevel::Detected);
    bat = coverage_fixture(ShipComponentType::BatScanner);
    observe_current_sensor_coverage(bat, 1);
    assert(std::any_of(bat.players.front().pendingSurveyReports.begin(), bat.players.front().pendingSurveyReports.end(),
        [](const auto& report) { return report.star == 2 && report.level == SurveyLevel::OrbitalSurvey; }));
    auto rhino = coverage_fixture(ShipComponentType::RhinoScanner);
    rhino.stars[1].position = {1050, 0};
    refresh_sensor_intel(rhino);
    assert(survey_level(rhino, 1, 2) == SurveyLevel::SystemScan);
    assert(survey_level(rhino, 1, 3) == SurveyLevel::Detected);
    auto detector = coverage_fixture(ShipComponentType::AnomalyDetector);
    assert(fleet_sensor_range(detector, detector.fleets.front()) == 0);
    assert(fleet_communication_range(detector, detector.fleets.front()) == 0);
    detector.wormholes.push_back({{{{10, {1050, 0}, WormholeSignature::Weak},
        {11, {1071, 0}, WormholeSignature::Weak}}}, 1, 500, 0.8});
    observe_current_wormholes(detector, 1);
    assert(std::any_of(detector.players.front().pendingWormholeReports.begin(), detector.players.front().pendingWormholeReports.end(),
        [](const auto& report) { return report.knowledge.endpoint == 10; }));
    assert(std::none_of(detector.players.front().pendingWormholeReports.begin(), detector.players.front().pendingWormholeReports.end(),
        [](const auto& report) { return report.knowledge.endpoint == 11; }));
}

} // namespace

int main()
{
    catalog_and_prerequisites();
    host_enforces_scanner_access();
    stacking_and_channels();
    coverage_and_special_detector();
}
