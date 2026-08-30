#include "suns/game_state.hpp"
#include "star_name_pool.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <string_view>

namespace suns {

namespace {

std::uint64_t bounded(std::mt19937_64& rng, std::uint64_t upperExclusive)
{
    return upperExclusive == 0 ? 0 : rng() % upperExclusive;
}

StarClass generated_star_class(std::mt19937_64& rng)
{
    const auto roll = bounded(rng, 100);
    if (roll < 4) return StarClass::BlueWhite;
    if (roll < 10) return StarClass::White;
    if (roll < 20) return StarClass::YellowWhite;
    if (roll < 35) return StarClass::Yellow;
    if (roll < 62) return StarClass::Orange;
    return StarClass::Red;
}

std::vector<std::string_view> generated_star_name_deck(std::mt19937_64& rng)
{
    std::vector<std::string_view> deck;
    deck.reserve(kCuratedStarNameCount);
    for (const auto name : kCuratedStarNames) deck.push_back(name);

    for (std::size_t remaining = deck.size(); remaining > 1; --remaining) {
        const auto other = static_cast<std::size_t>(bounded(rng, remaining));
        std::swap(deck[remaining - 1], deck[other]);
    }
    return deck;
}

std::string generated_planet_name(std::mt19937_64& rng, const std::string& starName)
{
    static constexpr std::array<const char*, 4> numerals = {"II", "III", "IV", "V"};
    return starName + " " + numerals[bounded(rng, numerals.size())];
}

std::uint32_t generated_habitability(std::mt19937_64& rng)
{
    const auto a = bounded(rng, 86);
    const auto b = bounded(rng, 86);
    return static_cast<std::uint32_t>(15 + (a + b) / 2);
}

bool far_enough(Position candidate, const std::vector<StarSystem>& stars, double minimumSeparation)
{
    const double minimumSquared = minimumSeparation * minimumSeparation;
    return std::all_of(stars.begin(), stars.end(), [&](const StarSystem& star) {
        const double dx = candidate.x - star.position.x;
        const double dy = candidate.y - star.position.y;
        return dx * dx + dy * dy >= minimumSquared;
    });
}

Position generated_position(
    std::mt19937_64& rng,
    const GalaxyConfig& config,
    const std::vector<StarSystem>& stars)
{
    const int halfWidth = std::max(250, static_cast<int>(config.width / 2.0));
    const int halfHeight = std::max(200, static_cast<int>(config.height / 2.0));
    const double requestedSeparation = std::clamp(config.minimumSeparation, 24.0, 100.0);

    for (int relaxation = 0; relaxation < 6; ++relaxation) {
        const double separation = requestedSeparation * std::pow(0.90, relaxation);
        for (int attempt = 0; attempt < 12000; ++attempt) {
            const auto x = static_cast<int>(bounded(rng, static_cast<std::uint64_t>(halfWidth * 2 + 1))) - halfWidth;
            const auto y = static_cast<int>(bounded(rng, static_cast<std::uint64_t>(halfHeight * 2 + 1))) - halfHeight;
            const Position candidate{static_cast<double>(x), static_cast<double>(y)};
            if (far_enough(candidate, stars, separation)) return candidate;
        }
    }

    throw std::runtime_error("Unable to place generated star systems with the requested galaxy density");
}

std::vector<ShipDesign> default_ship_designs(PlayerId owner)
{
    return {
        {kScoutDesignId, owner, "Scout", ShipHullType::Scout,
         {ShipComponentType::FusionDrive, ShipComponentType::LongRangeScanner}},
        {kColonyShipDesignId, owner, "Colony Ship", ShipHullType::LightTransport,
         {ShipComponentType::FusionDrive, ShipComponentType::ColonyModule}},
    };
}

const ShipComponentSpec* primary_engine(const ShipDesign& design, ShipComponentSpec& storage)
{
    for (const auto component : design.components) {
        auto spec = component_spec(component);
        if (spec.kind == ShipComponentKind::Engine) {
            storage = std::move(spec);
            return &storage;
        }
    }
    return nullptr;
}

} // namespace

const StarSystem* find_star(const GameState& state, StarId id)
{
    const auto it = std::find_if(state.stars.begin(), state.stars.end(), [id](const StarSystem& star) {
        return star.id == id;
    });
    return it == state.stars.end() ? nullptr : &*it;
}

const Planet* find_planet_at_star(const GameState& state, StarId star)
{
    const auto it = std::find_if(state.planets.begin(), state.planets.end(), [star](const Planet& planet) {
        return planet.star == star;
    });
    return it == state.planets.end() ? nullptr : &*it;
}

const Player* find_player(const GameState& state, PlayerId id)
{
    const auto it = std::find_if(state.players.begin(), state.players.end(), [id](const Player& player) {
        return player.id == id;
    });
    return it == state.players.end() ? nullptr : &*it;
}

const ShipDesign* find_ship_design(const GameState& state, ShipDesignId id)
{
    const auto it = std::find_if(state.shipDesigns.begin(), state.shipDesigns.end(), [id](const ShipDesign& design) {
        return design.id == id;
    });
    return it == state.shipDesigns.end() ? nullptr : &*it;
}

const ShipDesign* fleet_design(const GameState& state, const Fleet& fleet)
{
    const auto stacks = fleet_ship_stacks(fleet);
    return stacks.empty() ? nullptr : find_ship_design(state, stacks.front().design);
}

std::vector<FleetShipStack> fleet_ship_stacks(const Fleet& fleet)
{
    if (fleet.ships.empty()) return {{fleet.design, 1}};

    std::vector<FleetShipStack> result;
    for (const auto& stack : fleet.ships) {
        if (stack.design == 0 || stack.count == 0) continue;
        const auto existing = std::find_if(result.begin(), result.end(), [&](const FleetShipStack& candidate) {
            return candidate.design == stack.design;
        });
        if (existing == result.end()) result.push_back(stack);
        else existing->count += stack.count;
    }
    return result;
}

std::uint32_t fleet_ship_count(const Fleet& fleet)
{
    std::uint32_t total = 0;
    for (const auto& stack : fleet_ship_stacks(fleet)) total += stack.count;
    return total;
}

std::uint32_t fleet_ship_count(const Fleet& fleet, ShipDesignId design)
{
    const auto stacks = fleet_ship_stacks(fleet);
    const auto stack = std::find_if(stacks.begin(), stacks.end(), [&](const FleetShipStack& candidate) {
        return candidate.design == design;
    });
    return stack == stacks.end() ? 0 : stack->count;
}

void normalize_fleet_composition(Fleet& fleet)
{
    fleet.ships = fleet_ship_stacks(fleet);
    if (!fleet.ships.empty()) fleet.design = fleet.ships.front().design;
}

ShipHullSpec hull_spec(ShipHullType type)
{
    switch (type) {
    case ShipHullType::Scout:
        return {type, "Scout Hull", 34.5, 2, 300.0, 0.0, 1, 2, 0};
    case ShipHullType::LightTransport:
        return {type, "Light Transport", 45.0, 2, 400.0, 5.0, 1, 3, 0};
    case ShipHullType::MediumTransport:
        return {type, "Medium Transport", 70.0, 5, 500.0, 50.0, 1, 5, 0};
    case ShipHullType::RemoteMiner:
        return {type, "Remote Miner", 120.0, 8, 500.0, 0.0, 1, 1, 2};
    }
    return {type, "Unknown Hull", 0.0, 0, 0.0, 0.0, 0, 0, 0};
}

ShipComponentSpec component_spec(ShipComponentType type)
{
    ShipComponentSpec spec;
    spec.type = type;

    switch (type) {
    case ShipComponentType::FusionDrive:
        spec.name = "Fusion Drive";
        spec.kind = ShipComponentKind::Engine;
        spec.mass = 15.0;
        spec.buildCost = 3;
        spec.engineThrust = 595.0;
        spec.maxWarp = 8;
        spec.fuelPer100MassLy = {0.0, 0.05, 0.07, 0.10, 0.15, 0.23, 0.36, 0.60, 1.00, 0.0, 0.0};
        break;
    case ShipComponentType::RamScoopDrive:
        spec.name = "Ram Scoop Drive";
        spec.kind = ShipComponentKind::Engine;
        spec.mass = 20.0;
        spec.buildCost = 5;
        spec.engineThrust = 520.0;
        spec.maxWarp = 9;
        spec.fuelPer100MassLy = {0.0, -0.08, -0.08, -0.06, -0.03, 0.0, 0.05, 0.13, 0.30, 0.68, 0.0};
        break;
    case ShipComponentType::RadiatingRamScoopDrive:
        spec.name = "Radiating Ram Scoop";
        spec.kind = ShipComponentKind::Engine;
        spec.mass = 18.0;
        spec.buildCost = 4;
        spec.engineThrust = 560.0;
        spec.maxWarp = 9;
        spec.fuelPer100MassLy = {0.0, -0.12, -0.12, -0.10, -0.07, -0.03, 0.0, 0.07, 0.18, 0.42, 0.0};
        spec.radiationHazard = 1.0;
        break;
    case ShipComponentType::LongRangeScanner:
        spec.name = "Long Range Scanner";
        spec.kind = ShipComponentKind::Scanner;
        spec.mass = 10.0;
        spec.buildCost = 3;
        spec.sensorRange = 90.0;
        break;
    case ShipComponentType::PenetratingScanner:
        spec.name = "Penetrating Scanner";
        spec.kind = ShipComponentKind::Scanner;
        spec.mass = 14.0;
        spec.buildCost = 6;
        spec.sensorRange = 70.0;
        spec.penetratesPlanets = true;
        break;
    case ShipComponentType::CompactLongRangeScanner:
        spec.name = "Compact Long Range Scanner";
        spec.kind = ShipComponentKind::Scanner;
        spec.mass = 5.0;
        spec.buildCost = 2;
        spec.sensorRange = 55.0;
        break;
    case ShipComponentType::RemoteMiningModule:
        spec.name = "Remote Mining Module";
        spec.kind = ShipComponentKind::Mining;
        spec.mass = 80.0;
        spec.buildCost = 6;
        spec.remoteMiningUnits = 1.0;
        break;
    case ShipComponentType::ColonyModule:
        spec.name = "Colony Module";
        spec.kind = ShipComponentKind::Special;
        spec.mass = 25.0;
        spec.buildCost = 7;
        spec.enablesColonization = true;
        break;
    case ShipComponentType::FuelTank:
        spec.name = "Fuel Tank";
        spec.kind = ShipComponentKind::Fuel;
        spec.mass = 8.0;
        spec.buildCost = 2;
        spec.fuelCapacity = 300.0;
        break;
    case ShipComponentType::CargoPod:
        spec.name = "Cargo Pod";
        spec.kind = ShipComponentKind::Cargo;
        spec.mass = 12.0;
        spec.buildCost = 3;
        spec.cargoCapacity = 100.0;
        break;
    case ShipComponentType::AntimatterGenerator:
        spec.name = "Antimatter Generator";
        spec.kind = ShipComponentKind::Fuel;
        spec.mass = 10.0;
        spec.buildCost = 6;
        spec.fuelCapacity = 200.0;
        spec.fuelGenerationPerTurn = 50.0;
        break;
    }

    return spec;
}

std::size_t ship_design_engine_slots_used(const ShipDesign& design)
{
    return static_cast<std::size_t>(std::count_if(
        design.components.begin(), design.components.end(), [](ShipComponentType component) {
            return component_spec(component).kind == ShipComponentKind::Engine;
        }));
}

std::size_t ship_design_general_slots_used(const ShipDesign& design)
{
    return static_cast<std::size_t>(std::count_if(
        design.components.begin(), design.components.end(), [](ShipComponentType component) {
            const auto kind = component_spec(component).kind;
            return kind != ShipComponentKind::Engine && kind != ShipComponentKind::Mining;
        }));
}

std::size_t ship_design_mining_slots_used(const ShipDesign& design)
{
    return static_cast<std::size_t>(std::count_if(
        design.components.begin(), design.components.end(), [](ShipComponentType component) {
            return component_spec(component).kind == ShipComponentKind::Mining;
        }));
}

bool ship_design_valid(const ShipDesign& design)
{
    if (design.owner == 0 || design.name.empty() || design.name.size() > 48) return false;
    const auto hull = hull_spec(design.hull);
    const auto engines = ship_design_engine_slots_used(design);
    const auto general = ship_design_general_slots_used(design);
    const auto mining = ship_design_mining_slots_used(design);
    return engines == 1
        && engines <= hull.engineSlots
        && general <= hull.generalSlots
        && mining <= hull.miningSlots;
}

std::string research_field_name(ResearchField field)
{
    switch (field) {
    case ResearchField::Energy: return "Energy";
    case ResearchField::Propulsion: return "Propulsion";
    case ResearchField::Construction: return "Construction";
    case ResearchField::Electronics: return "Electronics";
    case ResearchField::Biology: return "Biology";
    case ResearchField::Weapons: return "Weapons";
    }
    return "Unknown";
}

namespace {

constexpr std::size_t research_index(ResearchField field)
{
    return static_cast<std::size_t>(field);
}

} // namespace

std::uint8_t technology_level(const GameState& state, PlayerId player, ResearchField field)
{
    const auto* owner = find_player(state, player);
    const auto index = research_index(field);
    return owner && index < kResearchFieldCount ? owner->technology.levels[index] : 0;
}

std::uint32_t research_level_cost(ResearchField, std::uint8_t level)
{
    if (level == 0) return 0;
    std::uint32_t cost = kFirstResearchLevelCost;
    for (std::uint8_t current = 1; current < level; ++current) {
        if (cost > std::numeric_limits<std::uint32_t>::max() / 2U) {
            return std::numeric_limits<std::uint32_t>::max();
        }
        cost *= 2U;
    }
    return cost;
}

bool component_available_to_player(
    const GameState& state, PlayerId player, ShipComponentType component)
{
    switch (component) {
    case ShipComponentType::CompactLongRangeScanner:
        return technology_level(state, player, ResearchField::Electronics) >= 1;
    case ShipComponentType::PenetratingScanner:
        return technology_level(state, player, ResearchField::Electronics) >= 3;
    case ShipComponentType::RemoteMiningModule:
        return technology_level(state, player, ResearchField::Construction) >= 1;
    default:
        return true;
    }
}

bool ship_design_available_to_player(
    const GameState& state, PlayerId player, const ShipDesign& design)
{
    const bool hullAvailable = design.hull != ShipHullType::RemoteMiner
        || technology_level(state, player, ResearchField::Construction) >= 1;
    return design.owner == player
        && hullAvailable
        && std::all_of(design.components.begin(), design.components.end(), [&](ShipComponentType component) {
            return component_available_to_player(state, player, component);
        });
}

double ship_design_mass(const ShipDesign& design)
{
    double mass = hull_spec(design.hull).mass;
    for (const auto component : design.components) mass += component_spec(component).mass;
    return mass;
}

std::uint32_t ship_design_cost(const ShipDesign& design)
{
    std::uint32_t cost = hull_spec(design.hull).buildCost;
    for (const auto component : design.components) cost += component_spec(component).buildCost;
    return cost;
}

double ship_design_speed(const ShipDesign& design)
{
    double thrust = 0.0;
    for (const auto component : design.components) thrust += component_spec(component).engineThrust;
    const auto mass = ship_design_mass(design);
    return mass > 0.0 && thrust > 0.0 ? thrust * 10.0 / mass : 0.0;
}

double ship_design_sensor_range(const ShipDesign& design)
{
    double range = 0.0;
    for (const auto component : design.components) range += component_spec(component).sensorRange;
    return range;
}

double ship_design_ordinary_sensor_range(const ShipDesign& design)
{
    double range = 0.0;
    for (const auto component : design.components) {
        const auto spec = component_spec(component);
        if (spec.kind == ShipComponentKind::Scanner && !spec.penetratesPlanets) {
            range += spec.sensorRange;
        }
    }
    return range;
}

double ship_design_penetrating_sensor_range(const ShipDesign& design)
{
    double range = 0.0;
    for (const auto component : design.components) {
        const auto spec = component_spec(component);
        if (spec.penetratesPlanets) range += spec.sensorRange;
    }
    return range;
}

bool ship_design_can_colonize(const ShipDesign& design)
{
    return std::any_of(design.components.begin(), design.components.end(), [](ShipComponentType component) {
        return component_spec(component).enablesColonization;
    });
}

bool ship_design_can_remote_mine(const ShipDesign& design)
{
    return design.hull == ShipHullType::RemoteMiner
        && std::any_of(design.components.begin(), design.components.end(), [](ShipComponentType component) {
            return component_spec(component).remoteMiningUnits > 0.0;
        });
}

std::uint8_t ship_design_max_warp(const ShipDesign& design)
{
    std::uint8_t maxWarp = 0;
    for (const auto component : design.components) maxWarp = std::max(maxWarp, component_spec(component).maxWarp);
    return maxWarp;
}

double ship_design_fuel_rate(const ShipDesign& design, std::uint8_t warp)
{
    if (warp == 0 || warp > kMaxWarp) return 0.0;
    ShipComponentSpec engine;
    const auto* spec = primary_engine(design, engine);
    if (!spec || warp > spec->maxWarp) return 0.0;
    return spec->fuelPer100MassLy[warp];
}

double ship_design_fuel_capacity(const ShipDesign& design)
{
    double capacity = hull_spec(design.hull).baseFuelCapacity;
    for (const auto component : design.components) capacity += component_spec(component).fuelCapacity;
    return capacity;
}

double ship_design_cargo_capacity(const ShipDesign& design)
{
    double capacity = hull_spec(design.hull).baseCargoCapacity;
    for (const auto component : design.components) capacity += component_spec(component).cargoCapacity;
    return capacity;
}

double ship_design_fuel_generation(const ShipDesign& design)
{
    double generation = 0.0;
    for (const auto component : design.components) generation += component_spec(component).fuelGenerationPerTurn;
    return generation;
}

double ship_design_radiation_hazard(const ShipDesign& design)
{
    double hazard = 0.0;
    for (const auto component : design.components) hazard += component_spec(component).radiationHazard;
    return hazard;
}

bool same_position(Position a, Position b)
{
    constexpr double epsilon = 0.000001;
    return std::abs(a.x - b.x) < epsilon && std::abs(a.y - b.y) < epsilon;
}

double distance_between(Position a, Position b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

FleetEncounterGeometry analyze_fleet_encounter(
    Position pursuerStart,
    Position pursuerEnd,
    Position targetStart,
    Position targetEnd,
    double encounterRadius)
{
    const Position relativeStart{
        pursuerStart.x - targetStart.x,
        pursuerStart.y - targetStart.y,
    };
    const Position relativeVelocity{
        (pursuerEnd.x - pursuerStart.x) - (targetEnd.x - targetStart.x),
        (pursuerEnd.y - pursuerStart.y) - (targetEnd.y - targetStart.y),
    };
    const auto dot = [](Position left, Position right) {
        return left.x * right.x + left.y * right.y;
    };
    const auto interpolate = [](Position start, Position end, double time) {
        return Position{
            start.x + (end.x - start.x) * time,
            start.y + (end.y - start.y) * time,
        };
    };

    const auto velocitySquared = dot(relativeVelocity, relativeVelocity);
    const auto closestTime = velocitySquared <= 0.000000000001
        ? 0.0
        : std::clamp(-dot(relativeStart, relativeVelocity) / velocitySquared, 0.0, 1.0);
    const Position closestRelative{
        relativeStart.x + relativeVelocity.x * closestTime,
        relativeStart.y + relativeVelocity.y * closestTime,
    };

    FleetEncounterGeometry result;
    result.closestTimeFraction = closestTime;
    result.closestDistance = std::hypot(closestRelative.x, closestRelative.y);
    const auto encounterMidpoint = [&](double time) {
        const auto pursuer = interpolate(pursuerStart, pursuerEnd, time);
        const auto target = interpolate(targetStart, targetEnd, time);
        return Position{(pursuer.x + target.x) * 0.5, (pursuer.y + target.y) * 0.5};
    };
    result.encounterPosition = encounterMidpoint(closestTime);

    const auto radius = std::max(0.0, encounterRadius);
    const auto startDistanceSquared = dot(relativeStart, relativeStart);
    if (startDistanceSquared <= radius * radius + 0.000000000001) {
        result.encounterTimeFraction = 0.0;
        result.encounterPosition = encounterMidpoint(0.0);
        return result;
    }
    if (velocitySquared <= 0.000000000001 || result.closestDistance > radius + 0.000000001) {
        return result;
    }

    const auto linear = 2.0 * dot(relativeStart, relativeVelocity);
    const auto constant = startDistanceSquared - radius * radius;
    const auto discriminant = linear * linear - 4.0 * velocitySquared * constant;
    if (discriminant < -0.000000001) return result;

    const auto entry = (-linear - std::sqrt(std::max(0.0, discriminant)))
        / (2.0 * velocitySquared);
    if (entry < -0.000000001 || entry > 1.0 + 0.000000001) return result;

    result.encounterTimeFraction = std::clamp(entry, 0.0, 1.0);
    result.encounterPosition = encounterMidpoint(*result.encounterTimeFraction);
    return result;
}

double warp_distance(std::uint8_t warp)
{
    if (warp == 0 || warp > kMaxWarp) return 0.0;
    const auto value = static_cast<double>(warp);
    return value * value;
}

double colonist_cargo_mass(std::uint64_t colonists)
{
    return static_cast<double>(colonists) / kColonistsPerCargoUnit;
}

double mineral_cargo_mass(const MineralCargo& minerals)
{
    return std::max(0.0, minerals.ironium)
        + std::max(0.0, minerals.boranium)
        + std::max(0.0, minerals.germanium);
}

double fleet_speed(FleetRole role)
{
    return role == FleetRole::Scout ? kScoutTravelSpeed : kColonyShipTravelSpeed;
}

double fleet_sensor_range(FleetRole role)
{
    return role == FleetRole::Scout ? kScoutSensorRange : 0.0;
}

std::uint32_t fleet_eta(const Fleet& fleet)
{
    return fleet.destination
        ? travel_turns(fleet.position, *fleet.destination, warp_distance(fleet.warp))
        : 0;
}

double fleet_speed(const GameState& state, const Fleet& fleet)
{
    double speed = std::numeric_limits<double>::infinity();
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        if (!design) return 0.0;
        speed = std::min(speed, ship_design_speed(*design));
    }
    return std::isfinite(speed) ? speed : 0.0;
}

double fleet_sensor_range(const GameState& state, const Fleet& fleet)
{
    return std::max(
        fleet_ordinary_sensor_range(state, fleet),
        fleet_penetrating_sensor_range(state, fleet));
}

double fleet_ordinary_sensor_range(const GameState& state, const Fleet& fleet)
{
    double range = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            range = std::max(range, ship_design_ordinary_sensor_range(*design));
        }
    }
    return range;
}

double fleet_penetrating_sensor_range(const GameState& state, const Fleet& fleet)
{
    double range = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            range = std::max(range, ship_design_penetrating_sensor_range(*design));
        }
    }
    return range;
}

bool fleet_can_colonize(const GameState& state, const Fleet& fleet)
{
    const auto stacks = fleet_ship_stacks(fleet);
    return std::any_of(stacks.begin(), stacks.end(), [&](const FleetShipStack& stack) {
        const auto* design = find_ship_design(state, stack.design);
        return design && ship_design_can_colonize(*design);
    });
}

bool fleet_can_remote_mine(const GameState& state, const Fleet& fleet)
{
    const auto stacks = fleet_ship_stacks(fleet);
    return std::any_of(stacks.begin(), stacks.end(), [&](const FleetShipStack& stack) {
        const auto* design = find_ship_design(state, stack.design);
        return design
            && ship_design_available_to_player(state, fleet.owner, *design)
            && ship_design_can_remote_mine(*design);
    });
}

std::uint8_t fleet_max_warp(const GameState& state, const Fleet& fleet)
{
    std::uint8_t result = kMaxWarp;
    bool found = false;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        if (!design) return 0;
        result = std::min(result, ship_design_max_warp(*design));
        found = true;
    }
    return found ? result : 0;
}

double fleet_fuel_generation(const GameState& state, const Fleet& fleet)
{
    double result = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            result += ship_design_fuel_generation(*design) * stack.count;
        }
    }
    return result;
}

double fleet_radiation_hazard(const GameState& state, const Fleet& fleet)
{
    double result = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            result = std::max(result, ship_design_radiation_hazard(*design));
        }
    }
    return result;
}

std::uint32_t fleet_eta(const GameState&, const Fleet& fleet)
{
    return fleet_eta(fleet);
}

double fleet_fuel_capacity(const GameState& state, const Fleet& fleet)
{
    double capacity = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            capacity += ship_design_fuel_capacity(*design) * stack.count;
        }
    }
    return capacity;
}

double fleet_cargo_capacity(const GameState& state, const Fleet& fleet)
{
    double capacity = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            capacity += ship_design_cargo_capacity(*design) * stack.count;
        }
    }
    return capacity;
}

double fleet_cargo_used(const GameState&, const Fleet& fleet)
{
    return colonist_cargo_mass(fleet.colonists) + mineral_cargo_mass(fleet.minerals);
}

double fleet_gross_mass(const GameState& state, const Fleet& fleet)
{
    double mass = fleet_cargo_used(state, fleet);
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        if (const auto* design = find_ship_design(state, stack.design)) {
            mass += ship_design_mass(*design) * stack.count;
        }
    }
    return mass;
}

double fleet_fuel_rate(const GameState& state, const Fleet& fleet)
{
    double weightedRate = 0.0;
    double dryMass = 0.0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        if (!design) continue;
        const auto mass = ship_design_mass(*design) * stack.count;
        weightedRate += ship_design_fuel_rate(*design, fleet.warp) * mass;
        dryMass += mass;
    }
    return dryMass > 0.0 ? weightedRate / dryMass : 0.0;
}

double fleet_fuel_change_for_distance(const GameState& state, const Fleet& fleet, double distance)
{
    if (distance <= 0.0) return 0.0;
    return fleet_fuel_rate(state, fleet) * (fleet_gross_mass(state, fleet) / 100.0) * distance;
}

bool fleet_warp_valid(const GameState& state, const Fleet& fleet, std::uint8_t warp)
{
    return warp >= 1 && warp <= fleet_max_warp(state, fleet);
}

bool within_range(Position source, Position target, double range)
{
    return range >= 0.0 && distance_between(source, target) <= range + 0.000001;
}

std::uint32_t travel_turns(Position from, Position to, double speed)
{
    if (speed <= 0.0 || same_position(from, to)) return 0;
    return static_cast<std::uint32_t>(std::ceil(distance_between(from, to) / speed));
}

SurveyLevel survey_level(const GameState& state, PlayerId player, StarId star)
{
    const auto* knownPlayer = find_player(state, player);
    if (!knownPlayer) return SurveyLevel::Detected;
    const auto knowledge = std::find_if(knownPlayer->surveyKnowledge.begin(), knownPlayer->surveyKnowledge.end(),
        [star](const SystemSurveyKnowledge& entry) { return entry.star == star; });
    if (knowledge != knownPlayer->surveyKnowledge.end()) return knowledge->level;

    // Compatibility for fixtures and saves created before staged surveying:
    // an old surveyed-star entry represented perfect knowledge.
    return std::find(knownPlayer->surveyedStars.begin(), knownPlayer->surveyedStars.end(), star)
            != knownPlayer->surveyedStars.end()
        ? SurveyLevel::GeologicalSurvey
        : SurveyLevel::Detected;
}

bool is_surveyed(const GameState& state, PlayerId player, StarId star)
{
    return survey_level(state, player, star) >= SurveyLevel::BasicScan;
}

std::optional<std::uint32_t> known_planet_habitability(
    const GameState& state, PlayerId player, PlanetId planetId)
{
    const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [planetId](const Planet& candidate) {
        return candidate.id == planetId;
    });
    if (planet == state.planets.end()) return std::nullopt;

    const auto level = survey_level(state, player, planet->star);
    if (level < SurveyLevel::BasicScan && planet->owner != player) return std::nullopt;
    if (level >= SurveyLevel::OrbitalSurvey || planet->owner == player) return planet->habitability;

    std::uint64_t mixed = state.galaxySeed
        ^ (static_cast<std::uint64_t>(player) << 32U)
        ^ (static_cast<std::uint64_t>(planet->id) * 0x9E3779B97F4A7C15ULL);
    mixed ^= mixed >> 30U;
    mixed *= 0xBF58476D1CE4E5B9ULL;
    mixed ^= mixed >> 27U;
    const auto error = static_cast<int>((mixed % 7ULL) * 5ULL) - 15;
    const auto rough = std::clamp(static_cast<int>(planet->habitability) + error, 0, 100);
    return static_cast<std::uint32_t>((rough + 5) / 10 * 10);
}

bool planet_geology_known(const GameState& state, PlayerId player, PlanetId planetId)
{
    const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [planetId](const Planet& candidate) {
        return candidate.id == planetId;
    });
    return planet != state.planets.end()
        && (planet->owner == player
            || survey_level(state, player, planet->star) >= SurveyLevel::GeologicalSurvey);
}

void set_survey_level(
    GameState& state,
    PlayerId player,
    StarId star,
    SurveyLevel level,
    std::uint64_t observedTurn)
{
    const auto it = std::find_if(state.players.begin(), state.players.end(), [player](const Player& candidate) {
        return candidate.id == player;
    });
    if (it == state.players.end()) return;

    const auto knowledge = std::find_if(it->surveyKnowledge.begin(), it->surveyKnowledge.end(),
        [star](const SystemSurveyKnowledge& entry) { return entry.star == star; });
    if (knowledge == it->surveyKnowledge.end()) {
        it->surveyKnowledge.push_back({star, level, observedTurn});
    } else if (level > knowledge->level) {
        knowledge->level = level;
        knowledge->observedTurn = observedTurn;
    } else if (level == knowledge->level) {
        knowledge->observedTurn = std::max(knowledge->observedTurn, observedTurn);
    }

    if (level >= SurveyLevel::BasicScan
        && std::find(it->surveyedStars.begin(), it->surveyedStars.end(), star) == it->surveyedStars.end()) {
        it->surveyedStars.push_back(star);
    }
}

void mark_surveyed(GameState& state, PlayerId player, StarId star)
{
    set_survey_level(state, player, star, SurveyLevel::GeologicalSurvey, state.turn);
}

void refresh_sensor_intel(GameState& state)
{
    for (const auto& star : state.stars) {
        for (const auto& planet : state.planets) {
            if (planet.owner == 0) continue;
            const auto* sourceStar = find_star(state, planet.star);
            if (sourceStar && within_range(sourceStar->position, star.position, kColonySensorRange)) {
                set_survey_level(state, planet.owner, star.id,
                    star.id == planet.star ? SurveyLevel::GeologicalSurvey : SurveyLevel::SystemScan,
                    state.turn);
            }
        }
        for (const auto& fleet : state.fleets) {
            const auto range = fleet_sensor_range(state, fleet);
            if (range > 0.0 && within_range(fleet.position, star.position, range)) {
                const auto penetratingRange = fleet_penetrating_sensor_range(state, fleet);
                set_survey_level(state, fleet.owner, star.id,
                    same_position(fleet.position, star.position)
                        ? SurveyLevel::OrbitalSurvey
                        : within_range(fleet.position, star.position, penetratingRange)
                            ? SurveyLevel::BasicScan
                            : SurveyLevel::SystemScan,
                    state.turn);
            }
        }
    }
}

std::uint64_t population_capacity(const Planet& planet)
{
    return static_cast<std::uint64_t>(planet.habitability) * 25;
}

std::uint64_t projected_population_growth(const Planet& planet)
{
    if (planet.owner == 0 || planet.population == 0 || planet.habitability == 0) return 0;
    const auto capacity = population_capacity(planet);
    if (capacity == 0 || planet.population >= capacity) return 0;
    const auto rawGrowth = std::max<std::uint64_t>(1, planet.population * planet.habitability / 1000);
    const auto headroom = capacity - planet.population;
    auto growth = rawGrowth * headroom / capacity;
    if (growth == 0) growth = 1;
    return std::min(growth, headroom);
}

std::uint32_t colony_output(const Planet& planet)
{
    if (planet.owner == 0) return 0;
    return planet.industry + static_cast<std::uint32_t>(planet.population / 500);
}

namespace {

void initialize_initial_fleet_telemetry(Fleet& fleet, std::uint64_t turn)
{
    normalize_fleet_composition(fleet);
    fleet.telemetry.observedTurn = turn;
    fleet.telemetry.position = fleet.position;
    fleet.telemetry.destination = fleet.destination;
    fleet.telemetry.warp = fleet.warp;
    fleet.telemetry.fuel = fleet.fuel;
    fleet.telemetry.colonists = fleet.colonists;
    fleet.telemetry.arrivalAction = fleet.arrivalAction;
    fleet.telemetry.waypointQueue = fleet.waypointQueue;
    fleet.telemetry.minerals = fleet.minerals;
    fleet.telemetry.ships = fleet.ships;
}

} // namespace

GameState generate_game(const GalaxyConfig& config)
{
    const auto starCount = std::clamp<std::size_t>(config.starCount, 2, 64);

    std::mt19937_64 physicalRng(config.seed);
    std::mt19937_64 namingRng(config.seed ^ 0x9E3779B97F4A7C15ULL);
    const auto nameDeck = generated_star_name_deck(namingRng);

    GameState state;
    state.galaxySeed = config.seed;
    state.players.push_back({1, "Terrans", {1}});
    state.shipDesigns = default_ship_designs(1);
    state.stars.reserve(starCount);
    state.planets.reserve(starCount);

    state.stars.push_back({1, "Sol", {0.0, 0.0}, StarClass::Yellow});
    state.planets.push_back({1, 1, "Earth", 100, 1, 1000, 4, 0, {}});
    state.planets.front().minerals = {100.0, 100.0, 100.0};

    for (std::size_t index = 2; index <= starCount; ++index) {
        const auto id = static_cast<StarId>(index);
        const auto name = std::string(nameDeck[index - 2]);
        const auto position = generated_position(physicalRng, config, state.stars);
        const auto stellarClass = generated_star_class(physicalRng);
        state.stars.push_back({id, name, position, stellarClass});
        state.planets.push_back({static_cast<PlanetId>(index), id, generated_planet_name(namingRng, name),
            generated_habitability(physicalRng), 0, 0, 1, 0, {}});
    }

    const auto* scout = find_ship_design(state, kScoutDesignId);
    const auto scoutFuel = scout ? ship_design_fuel_capacity(*scout) : 0.0;
    state.fleets.push_back({
        1, 1, "Scout 1", FleetRole::Scout, kScoutDesignId,
        {0.0, 0.0}, std::nullopt, kScoutCruiseWarp, scoutFuel, 0,
    });
    initialize_initial_fleet_telemetry(state.fleets.back(), state.turn);
    state.nextFleetId = 2;
    refresh_sensor_intel(state);
    return state;
}

GameState make_demo_game()
{
    GameState state;
    state.galaxySeed = 0;
    state.players.push_back({1, "Terrans", {1}});
    state.shipDesigns = default_ship_designs(1);
    state.stars = {
        {1, "Sol", {0.0, 0.0}, StarClass::Yellow},
        {2, "Alpha Centauri", {86.0, -42.0}, StarClass::YellowWhite},
        {3, "Sirius", {-118.0, 76.0}, StarClass::BlueWhite},
        {4, "Vega", {154.0, 96.0}, StarClass::BlueWhite},
        {5, "Altair", {42.0, 142.0}, StarClass::White},
        {6, "Tau Ceti", {-72.0, -126.0}, StarClass::Yellow},
        {7, "Epsilon Eridani", {142.0, -132.0}, StarClass::Orange},
        {8, "Procyon", {-168.0, -34.0}, StarClass::YellowWhite},
    };
    state.planets = {
        {1, 1, "Earth", 100, 1, 1000, 4, 0, {}},
        {2, 2, "Centauri II", 82, 0, 0, 1, 0, {}},
        {3, 3, "Sirius III", 48, 0, 0, 1, 0, {}},
        {4, 4, "Vega II", 71, 0, 0, 1, 0, {}},
        {5, 5, "Altair IV", 63, 0, 0, 1, 0, {}},
        {6, 6, "Tau Ceti III", 91, 0, 0, 1, 0, {}},
        {7, 7, "Eridani II", 56, 0, 0, 1, 0, {}},
        {8, 8, "Procyon II", 76, 0, 0, 1, 0, {}},
    };
    state.planets.front().minerals = {100.0, 100.0, 100.0};
    const auto* scout = find_ship_design(state, kScoutDesignId);
    const auto scoutFuel = scout ? ship_design_fuel_capacity(*scout) : 0.0;
    state.fleets.push_back({
        1, 1, "Scout 1", FleetRole::Scout, kScoutDesignId,
        {0.0, 0.0}, std::nullopt, kScoutCruiseWarp, scoutFuel, 0,
    });
    initialize_initial_fleet_telemetry(state.fleets.back(), state.turn);
    state.nextFleetId = 2;
    refresh_sensor_intel(state);
    return state;
}

} // namespace suns
