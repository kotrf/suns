#include "suns/campaign.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/planetary_recovery.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/stations.hpp"
#include "suns/communications.hpp"
#include "suns/wormholes.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/hulls.hpp"
#include "suns/scanners.hpp"
#include "suns/mining.hpp"
#include "suns/equipment.hpp"

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
    static const std::vector<ResearchUnlock> unlocks = [] {
        std::vector<ResearchUnlock> entries{
            {ResearchField::Energy, 1, "Antimatter Generator", "Generate fuel onboard; adds 200 reserve capacity.", ShipComponentType::AntimatterGenerator},
            {ResearchField::Propulsion, 1, "Advanced Fusion Drive", "Light engine with safe Warp 9; consumes fuel.", ShipComponentType::AdvancedFusionDrive},
            {ResearchField::Propulsion, 2, "High Warp Drive", "Safe Warp 10; heavier and fuel-hungry at top speed.", ShipComponentType::HighWarpDrive},
            {ResearchField::Propulsion, 3, "Efficient Ram Scoop", "Safe Warp 9; collects fuel through Warp 7 without a radiation hazard, but is heavy and costly.", ShipComponentType::EfficientRamScoopDrive},
            {ResearchField::Construction, 1, "Remote Mining", "Remote miner hull and module: extract uncolonized surface deposits.", ShipComponentType::RemoteMiningModule},
            {ResearchField::Construction, 2, "Heavy Transport", "High-capacity three-engine freight hull for large mineral convoys.", {}},
            {ResearchField::Construction, 3, "Field Repair Bay", "Repair 8 hull-damage points per year away from a dock; mixed fleets repair in proportion to equipped ships.", ShipComponentType::FieldRepairBay},
            {ResearchField::Electronics, 1, "Compact Scanner", "55 ly sensor in a lighter, cheaper package.", ShipComponentType::CompactLongRangeScanner},
            {ResearchField::Electronics, 2, "Extended Scanner", "Heavy 160 ly sensor for distant system contacts.", ShipComponentType::ExtendedRangeScanner},
            {ResearchField::Electronics, 3, "Penetrating Scanner", "Estimate planetary habitability without entering orbit.", ShipComponentType::PenetratingScanner},
            {ResearchField::Electronics, 4, "Deep Penetrating Scanner", "Estimate habitability up to 145 ly away; substantially heavier than a standard scanner.", ShipComponentType::DeepPenetratingScanner},
            {ResearchField::Electronics, 5, "Relay Array", "Extend the live communications mesh by 180 ly without revealing ships or planets.", ShipComponentType::RelayArray},
            {ResearchField::Electronics, 6, "Anomaly Detector", "Detect weak spatial anomalies, classify wormholes and reduce transit risk; natural wormholes always remain dangerous.", ShipComponentType::AnomalyDetector},
        };
        for (const auto component : {ShipComponentType::FuelDepotKit, ShipComponentType::DeepSpaceRelayKit}) {
            const bool depot = component == ShipComponentType::FuelDepotKit;
            ResearchUnlock unlock{ResearchField::Construction, 4, depot ? "Fuel Depot Kit" : "Deep-space Relay Kit",
                depot ? "Paid constructor fitting: fly to deep space, then permanently deploy a finite 2500 mg reservoir. Upkeep 1 mg/year; tankers replenish it."
                      : "Paid constructor fitting: permanently deploy a 300 ly communications relay. Upkeep 1 mg/year; no extra relay range while mobile.", component};
            unlock.extraLevels[std::size_t(depot ? ResearchField::Energy : ResearchField::Electronics)] = depot ? 3 : 5;
            entries.push_back(std::move(unlock));
        }
        for(const auto name:{"Orbital mirrors","Orbital shades"}) {
            ResearchUnlock unlock{ResearchField::Construction,14,name,
                "Long climate project: 2400 production, I/B/G 240/120/240 kt, at most 80 work/year. Needs a friendly Starbase or Fortress with a shipyard; changes effective temperature by up to 10 points while supported.", {}};
            unlock.extraLevels[std::size_t(ResearchField::Energy)]=14; unlock.planetaryEngineering=true; entries.push_back(std::move(unlock));
        }
        for(const bool adaptive:{false,true}) {
            ResearchUnlock unlock{ResearchField::Construction,std::uint8_t(adaptive?18:16),adaptive?"Adaptive magnetosphere":"Artificial magnetosphere",
                adaptive?"Requires delivered completed Compact remnant field research. Radiation reduction 0–20 while supported; 4800 work (80/year), I/B/G 480/240/480 kt. Needs a populated colony and friendly Starbase/Fortress shipyard; competes with climate engineering."
                    :"Radiation reduction 0–10 while supported; 2400 work (80/year), I/B/G 240/120/240 kt. Needs a populated colony and friendly Starbase/Fortress shipyard; competes with climate engineering.", {}};
            unlock.extraLevels[std::size_t(ResearchField::Energy)]=adaptive?18:16;
            unlock.planetaryEngineering=true; unlock.requiresCompactStudy=adaptive; entries.push_back(std::move(unlock));
        }
        {
            ResearchUnlock mining{ResearchField::Construction, 8, "Xenocrystal extraction",
                "Remote mining robots extract whole 1 kt crystals from a received DeepSurvey deposit: one per mining ship/year, up to four per planet/year. Transport the surface cache physically.", {}};
            mining.extraLevels[std::size_t(ResearchField::Electronics)] = 8; entries.push_back(std::move(mining));
            ResearchUnlock lab{ResearchField::Energy, 16, "Resonance laboratory",
                "Optional colony laboratory: 32 native-field RP/year per retained, studied artifact outside the empire focus. Deliver eight xenocrystals; 600 work (60/year) and I/B/G 120/60/120. Friendly Starbase/Fortress with shipyard required.", {}};
            lab.extraLevels[std::size_t(ResearchField::Electronics)] = 16;
            lab.extraLevels[std::size_t(ResearchField::Construction)] = 14; entries.push_back(std::move(lab));
            ResearchUnlock pulse{ResearchField::Energy, 18, "Resonant gate pulse",
                "Optional transit beyond ordinary gate range: consume eight physically delivered source xenocrystals on success to double both finite endpoint ranges. Existing gate construction, mass and racial cargo rules still apply.", {}};
            pulse.extraLevels[std::size_t(ResearchField::Electronics)] = 18;
            pulse.extraLevels[std::size_t(ResearchField::Construction)] = 16; entries.push_back(std::move(pulse));
        }
        {
            ResearchUnlock habitat{ResearchField::Construction,18,"Orbital habitat",
                "One engineering slot; 2 million orbital residents. 4800 work, 80/year, I/B/G 480/240/480; friendly Starbase/Fortress shipyard. Biology 14 and Energy 16. Occupied upkeep: 2 work and I/B/G 1/1/1 yearly; three paid emergency years, then 10% yearly losses. Physical colonist transport required.",{}};
            habitat.planetaryEngineering=true;
            habitat.extraLevels[std::size_t(ResearchField::Energy)]=16;
            habitat.extraLevels[std::size_t(ResearchField::Biology)]=14;
            entries.push_back(std::move(habitat));
        }
        for (const auto& gate : stargate_technologies()) {
            ResearchUnlock unlock{ResearchField::Energy, gate.energy, gate.name,
                std::string("Paid orbital fitting; requires two owned, surviving gates and a live command link. ")
                    + (gate.rangeLy == 0 ? "Unlimited range and loaded fleet mass; both endpoints must be Infinity gates. "
                        : "Each endpoint limits range and total loaded fleet mass. ")
                    + "Stargate Specialists may transport minerals and colonists. Engines remain necessary beyond the network.", {}};
            unlock.extraLevels[std::size_t(ResearchField::Electronics)] = gate.electronics;
            unlock.extraLevels[std::size_t(ResearchField::Construction)] = gate.construction;
            if (gate.specialistOnly) unlock.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
            entries.push_back(std::move(unlock));
        }
        constexpr std::array<std::uint8_t, 4> limits{3, 7, 11, 15};
        constexpr std::array<std::uint8_t, 4> extra{1, 5, 10, 16};
        for (std::size_t tier = 0; tier < limits.size(); ++tier) {
            for (const auto field : {ResearchField::Energy, ResearchField::Propulsion, ResearchField::Weapons}) {
                const auto axis = field == ResearchField::Energy ? "Temperature"
                    : field == ResearchField::Propulsion ? "Gravity" : "Radiation";
                ResearchUnlock entry{ResearchField::Biology, static_cast<std::uint8_t>(tier + 1),
                    std::string(axis) + " Terraforming " + std::to_string(limits[tier]),
                    "Shift " + std::string(axis) + " up to " + std::to_string(limits[tier])
                        + " points from its natural value. Queue each unit change: 12 production, no minerals.", {}};
                entry.extraLevels[static_cast<std::size_t>(field)] = extra[tier];
                entry.terraforming = true;
                entries.push_back(std::move(entry));
            }
        }
        for (auto& entry : entries) {
            entry.legacyScanner = entry.component && legacy_scanner_component(*entry.component);
            entry.legacyPropulsion = entry.component && legacy_propulsion_component(*entry.component);
            entry.legacyMining = entry.component == ShipComponentType::RemoteMiningModule;
            entry.legacyEquipment = entry.component && legacy_equipment_component(*entry.component);
            if (entry.name == "Heavy Transport") { entry.hull = ShipHullType::HeavyTransport; entry.legacyHull = true; }
        }
        for (const auto& engine : propulsion_technologies()) {
            const auto description = std::string("Optimal Warp ") + std::to_string(engine.optimalWarp)
                + "; free through Warp " + std::to_string(engine.freeWarp)
                + "; safe through Warp " + std::to_string(engine.safeWarp)
                + (engine.radiating ? "; hazardous to unprotected colonists." : ".")
                + (engine.access == EngineAccess::Settler ? " Fits Mini-Colony Ship only." : "");
            ResearchUnlock entry{ResearchField::Propulsion, engine.propulsion,
                engine.name, description, engine.component};
            entry.extraLevels[static_cast<std::size_t>(ResearchField::Energy)] = engine.energy;
            entry.engineAccess = engine.access;
            entry.excludesNoRamScoops = engine.ramScoop && engine.access != EngineAccess::Settler
                && engine.component != ShipComponentType::FuelMizer;
            entries.push_back(std::move(entry));
        }
        for (const auto& scanner : scanner_technologies()) {
            const auto field = scanner.electronics ? ResearchField::Electronics
                : scanner.biology ? ResearchField::Biology : ResearchField::Electronics;
            ResearchUnlock entry{field, scanner.electronics ? scanner.electronics : scanner.biology,
                scanner.name, scanner.ordinaryRange == 0 ? "Orbital survey only; no remote radar coverage."
                    : std::to_string(int(scanner.ordinaryRange)) + " ly ordinary; "
                        + std::to_string(int(scanner.penetratingRange)) + " ly penetrating.", scanner.component};
            entry.extraLevels[static_cast<std::size_t>(ResearchField::Energy)] = scanner.energy;
            if (field != ResearchField::Biology)
                entry.extraLevels[static_cast<std::size_t>(ResearchField::Biology)] = scanner.biology;
            entry.extraLevels[static_cast<std::size_t>(ResearchField::Propulsion)] = scanner.propulsion;
            entry.requiresSuperStealth = scanner.superStealth;
            if (scanner.component == ShipComponentType::PickPocketScanner
                || scanner.component == ShipComponentType::RobberBaronScanner)
                entry.description += " Cargo theft is not available yet.";
            if (scanner.component == ShipComponentType::ChameleonScanner)
                entry.description += " Cloaking is not available yet.";
            entries.push_back(std::move(entry));
        }
        for (const auto& miner : mining_technologies()) {
            ResearchUnlock entry{ResearchField::Construction, miner.construction, miner.name,
                std::to_string(int(miner.rate)) + " kt of each mineral/year at concentration 100; "
                    + std::to_string(int(miner.mass)) + " kt mass. Deposits output on an unowned planet's surface.",
                miner.component};
            entry.extraLevels[static_cast<std::size_t>(ResearchField::Electronics)] = miner.electronics;
            entries.push_back(std::move(entry));
        }
        for (const auto& equipment : equipment_technologies()) {
            std::size_t primary = 0;
            while (primary + 1 < kResearchFieldCount && equipment.levels[primary] == 0) ++primary;
            if (equipment.levels[primary] == 0) {
                primary = static_cast<std::size_t>(equipment.kind == ShipComponentKind::Shield ? ResearchField::Energy
                    : equipment.kind == ShipComponentKind::BeamWeapon || equipment.kind == ShipComponentKind::Torpedo
                        || equipment.kind == ShipComponentKind::Bomb ? ResearchField::Weapons : ResearchField::Construction);
            }
            ResearchUnlock entry{static_cast<ResearchField>(primary), equipment.levels[primary],
                equipment.name, equipment_effect_description(equipment), equipment.component};
            entry.extraLevels = equipment.levels;
            entry.extraLevels[primary] = 0;
            if (entry.description.empty()) entry.description = "Ship fitting equipment: mass, costs and capabilities shown in the designer.";
            entries.push_back(std::move(entry));
        }
        ResearchUnlock mini{ResearchField::Construction, 0, "Mini-Colony Ship",
            "Small 10 kt colony transport with one engine and one general cell.", {}};
        mini.engineAccess = EngineAccess::Settler;
        mini.hull = ShipHullType::MiniColonyShip;
        mini.legacyHull = true;
        entries.push_back(std::move(mini));
        for (const auto& hull : reference_hulls()) {
            ResearchUnlock entry{ResearchField::Construction, hull.constructionLevel, hull.name,
                std::to_string(hull.requiredEngines) + " engines; " + std::to_string(int(hull.baseCargoCapacity))
                    + " kt cargo; " + std::to_string(int(hull.baseFuelCapacity)) + " mg fuel; "
                    + std::to_string(hull.armor) + " base armor.", {}};
            entry.hull = hull.type;
            if (hull.type == ShipHullType::SurveyProbe)
                entry.extraLevels[static_cast<std::size_t>(ResearchField::Electronics)] = 18;
            entries.push_back(std::move(entry));
        }
        entries.push_back({ResearchField::Energy,1,"Colony defenses",
            "Paid surface defenses reduce bombardment losses. Build in Production; limit 100.",{}});
        entries.push_back({ResearchField::Construction,9,"Starbase",
            "Orbital station: 6 weapon, 6 armor/shield and 4 electronics slots; 1600 base armor.",{}});
        entries.push_back({ResearchField::Construction,16,"Fortress",
            "Orbital station: 12 weapon, 12 armor/shield and 6 electronics slots; 6000 base armor.",{}});
        return entries;
    }();
    return unlocks;
}

bool research_unlock_applicable(const GameState& state, PlayerId player, const ResearchUnlock& unlock)
{
    const auto* owner = find_player(state, player);
    const auto* miner = unlock.component ? mining_technology(*unlock.component) : nullptr;
    const auto* equipment = unlock.component ? equipment_technology(*unlock.component) : nullptr;
    return owner && (!unlock.primaryTrait || owner->race.primaryTrait == *unlock.primaryTrait)
        && (!unlock.planetaryEngineering || owner->race.environmentBased)
        && (!unlock.terraforming || (owner->race.environmentBased
            && !(owner->race.radiationImmune && unlock.extraLevels[static_cast<std::size_t>(ResearchField::Weapons)] != 0)))
        && engine_access_available(owner->race, unlock.engineAccess)
        && (!miner || ((!miner->advancedRemoteMining || owner->race.advancedRemoteMining)
            && (!miner->excludesBasicRemoteMining || !owner->race.basicRemoteMining)))
        && (!unlock.excludesNoRamScoops || !owner->race.noRamScoopEngines)
        && (!unlock.requiresSuperStealth || owner->race.hullAccess == HullAccess::SuperStealth)
        && (!equipment || equipment_access_applicable(state, player, *equipment))
        && (!unlock.hull || ship_hull_access_applicable(state, player, *unlock.hull));
}

bool research_unlock_available(const GameState& state, PlayerId player, const ResearchUnlock& unlock)
{
    if (!research_unlock_applicable(state, player, unlock)
        || technology_level(state, player, unlock.field) < unlock.level) return false;
    for (std::size_t field = 0; field < kResearchFieldCount; ++field)
        if (technology_level(state, player, static_cast<ResearchField>(field)) < unlock.extraLevels[field])
            return false;
    return !unlock.requiresCompactStudy || compact_study_received(state,player);
}

std::string research_unlock_requirement(const ResearchUnlock& unlock)
{
    std::string text = research_field_name(unlock.field) + " " + std::to_string(unlock.level);
    if(unlock.requiresCompactStudy) text += "; delivered completed Compact remnant study";
    for (std::size_t field = 0; field < kResearchFieldCount; ++field)
        if (unlock.extraLevels[field]) text += " + " + research_field_name(static_cast<ResearchField>(field))
            + " " + std::to_string(unlock.extraLevels[field]);
    switch (unlock.engineAccess) {
    case EngineAccess::Any: break;
    case EngineAccess::ImprovedFuelEfficiency: text += "; Improved Fuel Efficiency"; break;
    case EngineAccess::NoRamScoops: text += "; No Ram Scoop Engines"; break;
    case EngineAccess::Settler: text += "; Settler engine access"; break;
    }
    if (unlock.requiresSuperStealth) text += "; Super Stealth";
    if (unlock.primaryTrait == PrimaryRaceTrait::StargateSpecialist) text += "; Stargate Specialist";
    if (unlock.component) if (const auto* equipment = equipment_technology(*unlock.component)) {
        const auto access = equipment_access_requirement(*equipment);
        if (!access.empty()) text += "; " + access;
    }
    if (unlock.component) if (const auto* miner = mining_technology(*unlock.component)) {
        if (miner->advancedRemoteMining) text += "; Advanced Remote Mining";
        if (miner->excludesBasicRemoteMining) text += "; unavailable with Basic Remote Mining";
    }
    if (unlock.excludesNoRamScoops) text += "; requires ram scoops enabled";
    if (unlock.hull) if (const auto* hull = reference_hull(*unlock.hull)) {
        if (hull->access != HullAccess::Standard) text += "; " + hull_access_name(hull->access) + " hull access";
        if (hull->requiresAdvancedRemoteMining) text += "; Advanced Remote Mining";
        if (hull->excludesBasicRemoteMining) text += "; unavailable with Basic Remote Mining";
        if (hull->mysteryTrader) text += "; Mystery Trader acquisition";
    }
    return text;
}

bool ship_hull_access_applicable(const GameState& state, PlayerId player, ShipHullType type)
{
    const auto* owner = find_player(state, player);
    if (!owner) return false;
    if (const auto* hull = reference_hull(type)) {
        if (hull->access != HullAccess::Standard && hull->access != owner->race.hullAccess
            && !(type == ShipHullType::StarsMiniColonyShip && owner->race.settlerEngineAccess)) return false;
        if (hull->requiresAdvancedRemoteMining && !owner->race.advancedRemoteMining) return false;
        if (hull->excludesBasicRemoteMining && owner->race.basicRemoteMining) return false;
        if (hull->mysteryTrader) return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [&](const auto& design) {
            return design.owner == player && design.hull == type;
        });
        return true;
    }
    if (!player_uses_legacy_hulls(state, player)) return false;
    return type != ShipHullType::MiniColonyShip || owner->race.settlerEngineAccess;
}

bool ship_hull_available_to_player(const GameState& state, PlayerId player, ShipHullType hull)
{
    if (!ship_hull_access_applicable(state, player, hull)) return false;
    if (hull == ShipHullType::SurveyProbe && technology_level(state, player, ResearchField::Electronics) < 18) return false;
    if (const auto* reference = reference_hull(hull))
        return technology_level(state, player, ResearchField::Construction) >= reference->constructionLevel;
    if (hull == ShipHullType::RemoteMiner)
        return technology_level(state, player, ResearchField::Construction) >= 1;
    if (hull == ShipHullType::HeavyTransport)
        return technology_level(state, player, ResearchField::Construction) >= 2;
    if (hull == ShipHullType::MiniColonyShip) return find_player(state, player)->race.settlerEngineAccess;
    return hull_spec(hull).requiredEngines > 0;
}

std::int32_t race_habitability(
    const RaceProfile& race, PlanetEnvironment environment, std::uint8_t biology)
{
    const int adaptation = race.legacyBiologyAdaptation ? 5 * std::min<int>(biology, 3) : 0;
    auto suitability = [adaptation](int value, RaceEnvironmentRange range) {
        const int lo = std::max(0, int(range.minimum) - adaptation);
        const int hi = std::min(100, int(range.maximum) + adaptation);
        if (value < lo) return -std::clamp((lo - value) * 5, 1, 100);
        if (value > hi) return -std::clamp((value - hi) * 5, 1, 100);
        const double center = (int(range.minimum) + int(range.maximum)) / 2.0;
        const double halfWidth = std::max(1.0, (hi - lo) / 2.0);
        return std::clamp(int(std::lround(100 - 50 * std::abs(value - center) / halfWidth)), 1, 100);
    };
    const int temperature = suitability(environment.temperature, race.habitableTemperature);
    const int gravity = suitability(environment.gravity, race.habitableGravity);
    const int radiation = race.radiationImmune ? 100 : suitability(environment.radiation, race.habitableRadiation);
    if (temperature < 0 || gravity < 0 || radiation < 0)
        return std::min({temperature, gravity, radiation});
    return (temperature + gravity + radiation) / 3;
}

std::int32_t player_planet_habitability(
    const GameState& state, PlayerId player, const Planet& planet, std::uint64_t turn)
{
    if (planet.observedHabitability) return *planet.observedHabitability;
    const auto* empire = find_player(state, player);
    const auto baseline = empire && empire->race.environmentBased
        ? race_habitability(empire->race, engineered_environment(planet),
            technology_level(state, player, ResearchField::Biology))
        : planet.habitability;
    const auto* star = find_star(state, planet.star);
    const int shift = star ? int(std::lround((stellar_luminosity(*star, turn) - 1.0) * 100.0)) : 0;
    return static_cast<std::int32_t>(std::clamp(int(baseline) + shift
        - contamination_habitability_penalty(state,player,planet), -100, 100));
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
        planet.radar = PlanetaryRadarLevel::None;
        planet.homeworld = false;
    }
    std::vector<StarId> homes;
    for (std::size_t i = 0; i < empires.size(); ++i) {
        if (empires[i].name.empty() || empires[i].name.size() > 80)
            throw std::invalid_argument("Empire names must contain 1–80 bytes");
        if (empires[i].hullAccess > HullAccess::HyperExpansion || (empires[i].advancedRemoteMining && empires[i].basicRemoteMining))
            throw std::invalid_argument("Invalid hull access or mutually exclusive remote-mining settings");
        if (empires[i].controller > PlayerController::Computer)
            throw std::invalid_argument("Invalid empire controller");
        if (empires[i].primaryTrait != PrimaryRaceTrait::Generalist
            && empires[i].primaryTrait != PrimaryRaceTrait::StargateSpecialist)
            throw std::invalid_argument("This primary race trait is not playable yet");
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
        player.controller = empires[i].controller;
        player.race = race_preset(empires[i].race);
        player.race.primaryTrait = empires[i].primaryTrait;
        player.race.improvedFuelEfficiency = empires[i].improvedFuelEfficiency;
        player.race.noRamScoopEngines = empires[i].noRamScoopEngines;
        player.race.settlerEngineAccess = empires[i].settlerEngineAccess || empires[i].hullAccess == HullAccess::HyperExpansion;
        player.race.hullAccess = empires[i].hullAccess;
        player.race.advancedRemoteMining = empires[i].advancedRemoteMining;
        player.race.basicRemoteMining = empires[i].basicRemoteMining;
        player.technology.levels[static_cast<std::size_t>(ResearchField::Electronics)] = 1;
        state.players.push_back(player);
        auto& planet = *std::find_if(state.planets.begin(), state.planets.end(),
            [&](const Planet& p) { return p.star == home->id; });
        planet.owner = id;
        planet.population = kInitialHomePopulation;
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
        const auto* scoutDesign = find_ship_design(state, scoutId);
        scout.warp = ship_design_cruise_warp(*scoutDesign);
        scout.fuel = ship_design_fuel_capacity(*scoutDesign);
        scout.telemetry.observedTurn = state.turn;
        scout.telemetry.position = scout.position;
        scout.telemetry.fuel = scout.fuel;
        scout.telemetry.warp = scout.warp;
        state.fleets.push_back(scout);
        planet.radar = PlanetaryRadarLevel::Full;
        planet.homeworld = true;
        state.orbitalStations.push_back({state.nextOrbitalStationId++, id, planet.id,
            empires[i].name + " Orbital Dock", OrbitalStationHullType::OrbitalDock,
            {OrbitalStationModule::Shipyard, OrbitalStationModule::RefuelingDepot}});
    }
    generate_xenocrystal_deposits(state);
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
    state.interstellarMediumEnabled=host.interstellarMediumEnabled;
    state.mediumObserver=playerId;
    // Do not export the generation seed or global allocation counters.
    Player own = *player;
    own.pendingSurveyReports.clear();
    own.pendingPlayerReports.clear();
    own.pendingVisitorReports.clear();
    own.observedEnemyFleets.clear();
    own.enemyContacts=known_enemy_contacts(host,playerId);
    own.pendingEnemyContacts.clear();
    own.pendingFriendlyFleetContacts.clear();
    own.sensorStatus=known_sensor_status(host,playerId);
    own.pendingSensorStatus.clear(); own.observedSensorStatus.clear();
    own.pendingWormholeReports.clear();
    own.pendingStrategicIntel.clear();
    std::erase_if(own.fieldScience, [&](const auto& r) { return r.deliveryTurn > host.turn; });
    state.players.push_back(own);
    for (const auto& star : host.stars) {
        StarSystem publicStar{star.id, star.name, star.position, star.stellarClass};
        const auto intel = known_stellar_variability(host, playerId, star.id);
        if (intel && intel->characterized) publicStar.variability = star.variability;
        else if (intel && intel->variable) {
            // Detection-only sentinel. Orbital knowledge exposes the variable
            // flag, while known_stellar_variability withholds all cycle values.
            publicStar.variability = {1, 1, 0};
        }
        state.stars.push_back(publicStar);
    }
    for (const auto& planet : host.planets) {
        const bool owned = planet.owner == playerId;
        const auto level = survey_level(host, playerId, planet.star);
        if (!owned && level < SurveyLevel::BasicScan) continue;
        Planet known;
        if (owned) known = planet;
        else {
            known.aftermathKnownLocally=false;
            known.id = planet.id;
            known.star = planet.star;
            known.name = level >= SurveyLevel::OrbitalSurvey ? planet.name : "Unsurveyed planet";
            known.industry = 0;
            if (level >= SurveyLevel::OrbitalSurvey) {
                known.owner = known_planet_owner(host, playerId, planet.id).value_or(0);
                const auto intel = std::find_if(player->surveyKnowledge.begin(), player->surveyKnowledge.end(),
                    [&](const auto& entry) { return entry.star == planet.star; });
                if (intel != player->surveyKnowledge.end() && intel->planetSnapshot) {
                    known.environment = intel->planetSnapshot->environment.value_or(PlanetEnvironment{});
                    known.naturalEnvironment = intel->planetSnapshot->naturalEnvironment;
                    known.engineering = intel->planetSnapshot->engineering;
                }
            } else known.environment = {};
            // Surface stocks change through other empires' mining/cargo orders.
            // Only an immediately connected fleet on site knows today's stock.
            if (level >= SurveyLevel::GeologicalSurvey && planet.owner == 0) {
                const auto* star = find_star(host, planet.star);
                const bool onSite = star && std::any_of(host.fleets.begin(), host.fleets.end(),
                    [&](const Fleet& fleet) {
                        return fleet.owner == playerId && same_position(fleet.position, star->position)
                            && fleet_has_instant_link(host, fleet);
                    });
                if (onSite) known.minerals = planet.minerals;
            }
        }
        known.observedHabitability = known_planet_habitability(host, playerId, planet.id);
        known.habitability = known.observedHabitability.value_or(0);
        if (owned || level >= SurveyLevel::GeologicalSurvey)
            known.observedConcentration = planet_mineral_concentration(host, planet);
        else known.observedConcentration = MineralCargo{};
        if (!owned) {
            known.precursorArtifacts = {};
            known.precursorArtifacts.researchPoints = 0;
            known.precursorArtifacts.present = known_precursor_artifact_hint(
                host, playerId, planet.id).value_or(false);
        }
        // Ownership does not deliver remote construction telemetry. Both the
        // installation and its remaining queue work come from the same packet.
        known.habitat = {};
        // Owned remote construction is known only through delivered reports.
        // Older packets without receiver evidence confirm no receiver tier.
        known.radar = PlanetaryRadarLevel::None;
        known.resonanceLab = {};
        std::erase_if(known.productionQueue, [](const auto& item) { return item.kind == ProductionKind::ResonanceLaboratory; });
        if (known.owner == playerId) {
            known.scienceSamples.clear();
            known.engineering = {};
            std::erase_if(known.productionQueue,[](const auto& item) {return engineering_kind(item.kind);});
            const auto intel = std::find_if(player->surveyKnowledge.begin(), player->surveyKnowledge.end(),
                [&](const auto& entry) { return entry.star == planet.star; });
            if (intel != player->surveyKnowledge.end() && intel->planetSnapshot) {
                const auto& snapshot = *intel->planetSnapshot;
                if(snapshot.scienceSamples && intel->observedOwner==playerId) known.scienceSamples=*snapshot.scienceSamples;
                known.engineering=snapshot.engineering;
                known.engineering.invested={snapshot.engineering.work*0.1,snapshot.engineering.work*0.05,snapshot.engineering.work*0.1};
                if(snapshot.environment) known.environment=*snapshot.environment;
                known.naturalEnvironment=snapshot.naturalEnvironment;
                known.observedHabitability.reset();
                known.observedHabitability=player_planet_habitability(state,playerId,known,state.turn);
                known.habitability=*known.observedHabitability;
                if(snapshot.engineeringQueueIndex) {
                    const auto priorLab=snapshot.resonanceQueueIndex && *snapshot.resonanceQueueIndex<*snapshot.engineeringQueueIndex;
                    const auto index=std::min<std::size_t>(*snapshot.engineeringQueueIndex-std::uint32_t(priorLab),known.productionQueue.size());
                    known.productionQueue.insert(known.productionQueue.begin()+index,
                        {engineering_production_kind(known.engineering.kind),engineering_work(known.engineering.kind)-known.engineering.work});
                }
                known.resonanceLab = snapshot.resonanceLab;
                if (snapshot.resonanceQueueIndex) {
                    const auto index = std::min<std::size_t>(*snapshot.resonanceQueueIndex, known.productionQueue.size());
                    known.productionQueue.insert(known.productionQueue.begin() + index,
                        {ProductionKind::ResonanceLaboratory, kResonanceWork - known.resonanceLab.work});
                }
            }
        }
        const auto exotic = known_xenocrystal(host, playerId, planet.id);
        known.xenocrystal = exotic.value_or(XenocrystalObservation{}).site;
        if (exotic && exotic->colony && exotic->colony->owner == playerId && known.owner == playerId) {
            known.population = exotic->colony->population;
            known.mines = exotic->colony->mines;
        }
        if(known.owner==playerId) {
            const auto evidence=std::find_if(player->surveyKnowledge.begin(),player->surveyKnowledge.end(),[&](const auto& k){return k.star==planet.star;});
            if(evidence!=player->surveyKnowledge.end() && evidence->observedOwner==playerId && evidence->planetSnapshot && evidence->planetSnapshot->habitat) {
                const auto& h=*evidence->planetSnapshot->habitat;
                known.habitat=h.habitat; known.population=h.surfacePopulation; known.industry=h.industry; known.minerals=h.minerals;
                known.mines=h.mines; known.productionQueue=h.queue;
                known.radar=h.radar;
                if(known.engineering.kind==PlanetaryEngineeringKind::OrbitalHabitat && !h.stationSupported)known.engineering.operational=false;
            }
        }
        state.planets.push_back(known);
    }
    for (const auto& design : host.shipDesigns) if (design.owner == playerId) {
        state.shipDesigns.push_back(design);
        state.nextShipDesignId = std::max(state.nextShipDesignId, design.id + 1);
    }
    for (const auto& fleet : host.fleets) if (fleet.owner == playerId && !unreported_orbital_ship(host,fleet)) {
        auto known = fleet_player_view(host, fleet);
        // fleet_player_view supports legacy UI fixtures; do not let its fallback
        // to current composition disclose undelivered changes to a remote fleet.
        if (known.telemetry.ships.empty()) known.ships = {{known.design, 1}};
        known.fuelStalled = false;
        known.pendingCommands.clear();
        known.telemetryInTransit.clear();
        state.fleets.push_back(known);
        state.nextFleetId = std::max(state.nextFleetId, fleet.id + 1);
    }
    for (const auto& contact : missing_fleet_contacts(host, playerId)) {
        state.fleets.push_back(contact);
        state.nextFleetId = std::max(state.nextFleetId, contact.id + 1);
    }
    // Export only delivered transit status; allocation/deadlines and physical WHs stay with the host.
    for (const auto& transit : host.wormholeTransits) if (transit.lastContact.owner == playerId
        && transit.status != WormholeTransitStatus::AwaitingEntryReport) {
        auto known = transit;
        known.lastContact = fleet_player_view(host, transit.lastContact);
        known.overdueTurn = 0;
        known.presumedLostTurn = 0;
        state.wormholeTransits.push_back(known);
    }
    std::sort(state.fleets.begin(), state.fleets.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    state.wormholeRules.spawnChancePerTurn = 0.0;
    // Anonymous hull evidence is built only from received/connected snapshots.
    for(const auto& observed:state.players.front().enemyContacts) if(!observed.lost && observed.quality!=ContactQuality::Probable) {
        auto contact=recon_contact_fleet(observed,host.turn);
        state.fleets.push_back(contact); state.nextFleetId=std::max(state.nextFleetId,contact.id+1);
    }
    for(const auto& planet:host.planets) {
        const OrbitalStation* station=nullptr;bool frozen=false;
        for(const auto& record:player->surveyKnowledge) if(record.star==planet.star && record.observedOwner==playerId
            && record.planetSnapshot && record.planetSnapshot->habitat) {
            const auto& observation=*record.planetSnapshot->habitat;frozen=true;
            if(observation.station)station=&*observation.station;
            break;
        }
        if(!frozen)station=known_orbital_station(host,playerId,planet.id);
        if(station) {
            state.orbitalStations.push_back(*station);
            state.nextOrbitalStationId=std::max(state.nextOrbitalStationId,station->id+1);
        }
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
