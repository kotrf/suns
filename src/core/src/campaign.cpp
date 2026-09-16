#include "suns/campaign.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace suns {

RaceProfile race_preset(RacePreset preset)
{
    RaceProfile race;
    race.environmentBased = true;
    switch (preset) {
    case RacePreset::Terran: break;
    case RacePreset::Cryophile:
        race.habitableTemperature = {0, 40};
        race.habitableGravity = {15, 65};
        race.habitableRadiation = {0, 40};
        break;
    case RacePreset::Radiotroph:
        race.habitableTemperature = {55, 85};
        race.habitableGravity = {50, 80};
        race.habitableRadiation = {0, 100};
        race.radiationImmune = true;
        race.radiationTolerance = 1.0;
        break;
    default: throw std::invalid_argument("Unknown race preset");
    }
    return race;
}

const std::vector<ResearchUnlock>& research_unlocks()
{
    static const std::vector<ResearchUnlock> unlocks{
        {ResearchField::Energy, 1, "Antimatter Generator", "Generate fuel onboard; adds 200 reserve capacity.", ShipComponentType::AntimatterGenerator},
        {ResearchField::Propulsion, 1, "Advanced Fusion Drive", "Light engine with safe Warp 9; consumes fuel.", ShipComponentType::AdvancedFusionDrive},
        {ResearchField::Construction, 1, "Remote Mining", "Remote miner hull and module: extract uncolonized surface deposits.", ShipComponentType::RemoteMiningModule},
        {ResearchField::Electronics, 1, "Compact Scanner", "55 ly sensor in a lighter, cheaper package.", ShipComponentType::CompactLongRangeScanner},
        {ResearchField::Electronics, 2, "Extended Scanner", "Heavy 160 ly sensor for distant system contacts.", ShipComponentType::ExtendedRangeScanner},
        {ResearchField::Electronics, 3, "Penetrating Scanner", "Estimate planetary habitability without entering orbit.", ShipComponentType::PenetratingScanner},
        {ResearchField::Biology, 1, "Sealed Habitats", "New campaigns: tolerate environments 5 points outside racial ranges.", {}},
        {ResearchField::Biology, 2, "Adaptive Habitats", "New campaigns: expand environmental tolerance to 10 points.", {}},
        {ResearchField::Biology, 3, "Extreme Habitats", "New campaigns: expand environmental tolerance to 15 points.", {}},
    };
    return unlocks;
}

std::uint32_t race_habitability(
    const RaceProfile& race, PlanetEnvironment environment, std::uint8_t biology)
{
    const int adaptation = 5 * std::min<int>(biology, 3);
    auto suitability = [adaptation](int value, RaceEnvironmentRange range) {
        const int lo = std::max(0, int(range.minimum) - adaptation);
        const int hi = std::min(100, int(range.maximum) + adaptation);
        if (value < lo || value > hi) return 0;
        const double center = (int(range.minimum) + int(range.maximum)) / 2.0;
        const double halfWidth = std::max(1.0, (hi - lo) / 2.0);
        return std::clamp(int(std::lround(100 - 50 * std::abs(value - center) / halfWidth)), 1, 100);
    };
    const int temperature = suitability(environment.temperature, race.habitableTemperature);
    const int gravity = suitability(environment.gravity, race.habitableGravity);
    const int radiation = race.radiationImmune ? 100 : suitability(environment.radiation, race.habitableRadiation);
    if (!temperature || !gravity || !radiation) return 0;
    return static_cast<std::uint32_t>((temperature + gravity + radiation) / 3);
}

std::uint32_t player_planet_habitability(
    const GameState& state, PlayerId player, const Planet& planet, std::uint64_t turn)
{
    if (planet.observedHabitability) return *planet.observedHabitability;
    const auto* empire = find_player(state, player);
    const auto baseline = empire && empire->race.environmentBased
        ? race_habitability(empire->race, planet.environment,
            technology_level(state, player, ResearchField::Biology))
        : planet.habitability;
    const auto* star = find_star(state, planet.star);
    const int shift = star ? int(std::lround((stellar_luminosity(*star, turn) - 1.0) * 100.0)) : 0;
    // Stellar variation cannot make a wholly incompatible environment habitable.
    if (baseline == 0) return 0;
    return static_cast<std::uint32_t>(std::clamp(int(baseline) + shift, 0, 100));
}

GameState generate_campaign(const GalaxyConfig& config, const std::vector<EmpireSetup>& empires)
{
    if (empires.empty() || empires.size() > 8 || empires.size() > config.starCount)
        throw std::invalid_argument("A campaign needs 1–8 empires and at least one system per empire");
    auto state = generate_game(config);
    const auto initialDesigns = state.shipDesigns;
    state.players.clear();
    state.fleets.clear();
    state.shipDesigns.clear();
    state.orbitalStations.clear();
    state.nextShipDesignId = 1;
    state.nextFleetId = 1;
    state.nextOrbitalStationId = 1;
    for (auto& planet : state.planets) {
        planet.owner = 0;
        planet.population = 0;
        planet.industry = 1;
        planet.minerals = {};
    }
    std::vector<StarId> homes;
    for (std::size_t i = 0; i < empires.size(); ++i) {
        if (empires[i].name.empty() || empires[i].name.size() > 80)
            throw std::invalid_argument("Empire names must contain 1–80 bytes");
        const PlayerId id = static_cast<PlayerId>(i + 1);
        const StarSystem* home = &state.stars.front();
        double best = -1;
        if (!homes.empty()) for (const auto& candidate : state.stars) {
            if (std::find(homes.begin(), homes.end(), candidate.id) != homes.end()) continue;
            double nearest = 1e20;
            for (auto other : homes) nearest = std::min(nearest,
                distance_between(candidate.position, find_star(state, other)->position));
            if (nearest > best) { best = nearest; home = &candidate; }
        }
        homes.push_back(home->id);
        Player player{id, empires[i].name, {home->id}};
        player.race = race_preset(empires[i].race);
        state.players.push_back(player);
        auto& planet = *std::find_if(state.planets.begin(), state.planets.end(),
            [&](const Planet& p) { return p.star == home->id; });
        planet.owner = id;
        planet.population = 1000;
        planet.industry = 4;
        planet.habitability = 100;
        planet.minerals = {100, 100, 100};
        planet.precursorArtifacts = {};
        auto midpoint = [](RaceEnvironmentRange range) {
            return static_cast<std::uint8_t>((int(range.minimum) + range.maximum) / 2);
        };
        planet.environment = {midpoint(player.race.habitableTemperature),
            midpoint(player.race.habitableGravity), midpoint(player.race.habitableRadiation)};
        // Every home has the same stable stellar conditions and initial services.
        auto& homeStar = *std::find_if(state.stars.begin(), state.stars.end(),
            [&](const StarSystem& s) { return s.id == home->id; });
        homeStar.variability = {};
        homeStar.stellarClass = StarClass::Yellow;
        auto designs = initialDesigns;
        const auto scoutId = state.nextShipDesignId;
        for (auto& design : designs) {
            design.owner = id;
            design.id = state.nextShipDesignId++;
            state.shipDesigns.push_back(design);
        }
        Fleet scout;
        scout.id = state.nextFleetId++;
        scout.owner = id;
        scout.name = empires[i].name + " Scout";
        scout.design = scoutId;
        scout.position = home->position;
        scout.fuel = ship_design_fuel_capacity(*find_ship_design(state, scoutId));
        scout.telemetry.observedTurn = state.turn;
        scout.telemetry.position = scout.position;
        scout.telemetry.fuel = scout.fuel;
        scout.telemetry.warp = scout.warp;
        state.fleets.push_back(scout);
        state.orbitalStations.push_back({state.nextOrbitalStationId++, id, planet.id,
            empires[i].name + " Orbital Dock", OrbitalStationHullType::OrbitalDock,
            {OrbitalStationModule::Shipyard, OrbitalStationModule::RefuelingDepot}});
    }
    refresh_sensor_intel(state);
    record_empire_turn_statistics(state);
    return state;
}

PlayerView make_player_view(const GameState& host, PlayerId playerId)
{
    const auto* player = find_player(host, playerId);
    if (!player) throw std::invalid_argument("Unknown player");
    PlayerView view;
    view.player = playerId;
    auto& state = view.state;
    state.turn = host.turn;
    // Do not export the generation seed or global allocation counters.
    Player own = *player;
    own.pendingSurveyReports.clear();
    own.pendingPlayerReports.clear();
    state.players.push_back(own);
    for (const auto& star : host.stars) {
        StarSystem publicStar{star.id, star.name, star.position, star.stellarClass};
        const auto intel = known_stellar_variability(host, playerId, star.id);
        if (intel && intel->characterized) publicStar.variability = star.variability;
        state.stars.push_back(publicStar);
    }
    for (const auto& planet : host.planets) {
        const bool owned = planet.owner == playerId;
        const auto level = survey_level(host, playerId, planet.star);
        if (!owned && level < SurveyLevel::BasicScan) continue;
        Planet known;
        if (owned) known = planet;
        else {
            known.id = planet.id;
            known.star = planet.star;
            known.name = level >= SurveyLevel::OrbitalSurvey ? planet.name : "Unsurveyed planet";
            known.industry = 0;
            if (level >= SurveyLevel::OrbitalSurvey) {
                known.owner = planet.owner;
                known.environment = planet.environment;
            } else known.environment = {};
            if (level >= SurveyLevel::GeologicalSurvey && planet.owner == 0)
                known.minerals = planet.minerals;
        }
        known.observedHabitability = known_planet_habitability(host, playerId, planet.id);
        known.habitability = known.observedHabitability.value_or(0);
        if (owned || level >= SurveyLevel::GeologicalSurvey)
            known.observedConcentration = planet_mineral_concentration(host, planet);
        else known.observedConcentration = MineralCargo{};
        if (!owned) known.precursorArtifacts = {};
        state.planets.push_back(known);
    }
    for (const auto& design : host.shipDesigns) if (design.owner == playerId) {
        state.shipDesigns.push_back(design);
        state.nextShipDesignId = std::max(state.nextShipDesignId, design.id + 1);
    }
    for (const auto& fleet : host.fleets) if (fleet.owner == playerId) {
        auto known = fleet_player_view(host, fleet);
        known.pendingCommands.clear();
        known.telemetryInTransit.clear();
        state.fleets.push_back(known);
        state.nextFleetId = std::max(state.nextFleetId, fleet.id + 1);
    }
    // Only current contacts in the connected sensor mesh are immediately
    // reportable. Detached reconnaissance needs a future contact-report queue.
    for (const auto& enemy : host.fleets) if (enemy.owner != playerId) {
        bool visible = false;
        for (const auto& colony : host.planets) if (colony.owner == playerId && colony.population > 0) {
            const auto* star = find_star(host, colony.star);
            if (star && distance_between(star->position, enemy.position) <= kColonySensorRange) visible = true;
        }
        for (const auto& detector : host.fleets) if (detector.owner == playerId
            && fleet_has_instant_link(host, detector)
            && distance_between(detector.position, enemy.position) <= fleet_sensor_range(host, detector)) visible = true;
        if (!visible) continue;
        Fleet contact;
        contact.id = enemy.id;
        contact.owner = enemy.owner;
        contact.name = "Contact " + std::to_string(enemy.id) + " (Empire " + std::to_string(enemy.owner) + ")";
        contact.design = 0;
        contact.fuel = 0;
        contact.position = enemy.position;
        contact.telemetry.observedTurn = host.turn;
        contact.telemetry.position = contact.position;
        state.fleets.push_back(contact);
        state.nextFleetId = std::max(state.nextFleetId, contact.id + 1);
    }
    for (const auto& station : host.orbitalStations) if (station.owner == playerId) {
        state.orbitalStations.push_back(station);
        state.nextOrbitalStationId = std::max(state.nextOrbitalStationId, station.id + 1);
    }
    return view;
}

TurnResult resolve_campaign_turn(const GameState& state, std::vector<PlayerOrders> submissions)
{
    if (submissions.size() != state.players.size())
        throw std::invalid_argument("Waiting for every empire to submit orders (or an explicit pass)");
    std::sort(submissions.begin(), submissions.end(), [](const auto& a, const auto& b) { return a.player < b.player; });
    PlayerId previous = 0;
    for (const auto& submission : submissions) {
        if (submission.player == previous || !find_player(state, submission.player))
            throw std::invalid_argument("Unknown or duplicate player submission");
        previous = submission.player;
    }
    return TurnProcessor{}.process_with_events(state, submissions);
}

} // namespace suns
