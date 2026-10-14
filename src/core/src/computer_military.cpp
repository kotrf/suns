#include "computer_policy.hpp"
#include "suns/combat.hpp"
#include "suns/equipment.hpp"
#include "suns/hulls.hpp"
#include "suns/stations.hpp"
#include "suns/strategic_operations.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace suns::computer_detail {
namespace {
enum class Role { Guard, Bomber, Assault };

bool bombs(const ShipDesign& design)
{
    return std::any_of(design.components.begin(), design.components.end(), [](auto c) {
        const auto s = component_spec(c);
        return s.kind == ShipComponentKind::Bomb && !s.unterraformingBomb;
    });
}
double weapons(const ShipDesign& design)
{
    double power = 0;
    for (auto c : design.components) {
        const auto s = component_spec(c);
        if ((s.kind == ShipComponentKind::BeamWeapon || s.kind == ShipComponentKind::Torpedo) && !s.shieldOnly)
            power += s.weaponPower * (s.kind == ShipComponentKind::Torpedo ? s.weaponAccuracy / 100.0 : 1.0);
    }
    return power;
}
bool assault(const ShipDesign& design)
{
    return !ship_design_can_colonize(design) && !ship_design_can_remote_mine(design)
        && !ship_design_has_scanner(design) && !bombs(design) && weapons(design) == 0
        && ship_design_cargo_capacity(design) >= 10;
}
bool fleet_bombs(const GameState& state, const Fleet& fleet)
{
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* d = find_ship_design(state, stack.design); d && bombs(*d)) return true;
    return false;
}
double strength(const GameState& state, const Fleet& fleet)
{
    double result = 0;
    for (const auto& stack : fleet_ship_stacks(fleet)) if (const auto* d = find_ship_design(state, stack.design))
        if (weapons(*d) > 0) result += stack.count * (ship_design_armor(*d) + ship_design_shields(*d) + 20 * weapons(*d));
    return result * std::max(0.0, 1 - fleet.damagePercent / 100);
}

double contact_threat(const PlayerView& view, Position position, double radius)
{
    double result = 0;
    std::vector<const ReconContact*> contacts;
    for (const auto& contact : view.state.players.front().enemyContacts) contacts.push_back(&contact);
    std::sort(contacts.begin(), contacts.end(), [](auto a, auto b) { return a->fleet < b->fleet; });
    for (const auto* observed : contacts) {
        const auto& contact = *observed;
        if (contact.lost || contact.owner == view.player || contact.observedTurn > view.state.turn
            || view.state.turn - contact.observedTurn > 6
            || distance_between(contact.position, position) > radius + contact.uncertainty) continue;
        if (contact.quality == ContactQuality::Identified && !contact.hulls.empty()
            && std::all_of(contact.hulls.begin(), contact.hulls.end(), [](auto h) {
                return h == ShipHullType::StarsScout;
            })) continue; // This public hull has neither weapons cells nor cargo.
        // Mass is an observation interval, not a weapons measurement. Age and
        // anonymous contacts increase the allowance; no foreign design is read.
        result += std::max(250.0, contact.massMaximum * 4)
            * (1 + 0.15 * (view.state.turn - contact.observedTurn))
            * (contact.quality == ContactQuality::Probable ? 1.5 : 1.0);
    }
    return result;
}

std::optional<ShipComponentType> best_component(const GameState& state, PlayerId owner, ShipComponentKind kind)
{
    std::optional<ShipComponentType> best;
    double value = 0;
    for (const auto& equipment : equipment_technologies()) {
        if (equipment.kind != kind || !component_available_to_player(state, owner, equipment.component)) continue;
        const auto s = component_spec(equipment.component);
        const auto score = kind == ShipComponentKind::BeamWeapon ? (s.shieldOnly ? 0 : s.weaponPower * (1 + 0.2 * s.weaponRange))
            : kind == ShipComponentKind::Bomb ? (s.unterraformingBomb ? 0 : s.bombPopulationPercent + s.bombMinimumKills / 1000.0)
            : s.shields;
        if (score > value) { value = score; best = equipment.component; }
    }
    return best;
}

std::optional<ShipDesign> military_design(const GameState& state, PlayerId owner, Role role)
{
    const auto weapon = best_component(state, owner, ShipComponentKind::BeamWeapon);
    const auto bomb = best_component(state, owner, ShipComponentKind::Bomb);
    const auto shield = best_component(state, owner, ShipComponentKind::Shield);
    const auto* player = find_player(state, owner);
    std::optional<ShipComponentType> engine;
    double engineScore = -1;
    for (const auto& technology : propulsion_technologies()) {
        if (!component_available_to_player(state, owner, technology.component)
            || (technology.radiating && player->race.radiationTolerance < kRadiatingDriveSafeTolerance)) continue;
        const auto score = 2.0 * technology.freeWarp + technology.optimalWarp - technology.cost / 30.0;
        if (score > engineScore) { engine = technology.component; engineScore = score; }
    }
    if (!engine || (role == Role::Guard && !weapon) || (role == Role::Bomber && !bomb)) return std::nullopt;
    const std::vector<ShipHullType> hulls = role == Role::Guard
        ? std::vector{ShipHullType::Destroyer, ShipHullType::Frigate, ShipHullType::Cruiser}
        : role == Role::Bomber ? std::vector{ShipHullType::MiniBomber}
        : std::vector{ShipHullType::SmallFreighter, ShipHullType::MediumFreighter, ShipHullType::LargeFreighter};
    std::optional<ShipDesign> best;
    double bestScore = -1;
    for (const auto hull : hulls) {
        if (!ship_hull_available_to_player(state, owner, hull)) continue;
        ShipDesign design{0, owner, "Computer design", hull, {}};
        for (unsigned i = 0; i < hull_spec(hull).requiredEngines; ++i) design.components.push_back(*engine);
        normalize_ship_design_placement(design);
        const auto fit = [&](ShipComponentType c) {
            auto candidate = design; candidate.components.push_back(c); candidate.placements.clear();
            normalize_ship_design_placement(candidate);
            if (!ship_design_valid(candidate) || !ship_design_available_to_player(state, owner, candidate)) return false;
            design = std::move(candidate); return true;
        };
        if (role == Role::Guard && component_available_to_player(state, owner, ShipComponentType::RhinoScanner))
            (void)fit(ShipComponentType::RhinoScanner);
        if (role == Role::Guard || role == Role::Bomber) {
            const auto c = role == Role::Guard ? *weapon : *bomb;
            for (int i = 0; i < 4 && fit(c); ++i) {}
        }
        if (shield) (void)fit(*shield);
        if (!ship_design_valid(design) || !ship_design_available_to_player(state, owner, design)) continue;
        if ((role == Role::Guard && weapons(design) == 0) || (role == Role::Bomber && !bombs(design))
            || (role == Role::Assault && ship_design_cargo_capacity(design) < 50)) continue;
        const auto score = (role == Role::Guard ? ship_design_armor(design) + ship_design_shields(design) + 20 * weapons(design)
            : role == Role::Bomber ? 100.0 : ship_design_cargo_capacity(design)) / ship_design_cost(design);
        if (score > bestScore) { bestScore = score; best = design; }
    }
    if (!best) return std::nullopt;
    const ShipDesign* match = nullptr;
    for (const auto& design : state.shipDesigns) if (design.owner == owner && design.hull == best->hull
        && design.components == best->components && ship_design_valid(design)
        && (!match || design.id < match->id)) match = &design;
    if (match) return *match;
    auto name = std::string(role == Role::Guard ? "Guard: " : role == Role::Bomber ? "Bombard: " : "Assault: ")
        + hull_spec(best->hull).name + " / " + component_spec(*engine).name;
    if (role != Role::Assault) name += " / " + component_spec(role == Role::Guard ? *weapon : *bomb).name;
    name.resize(std::min<std::size_t>(40, name.size()));
    best->name = name;
    for (unsigned suffix = 2; std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(),
        [&](const auto& d) { return d.owner == owner && d.name == best->name; }); ++suffix)
        best->name = name + " " + std::to_string(suffix);
    return best;
}
} // namespace

bool military_fleet(const GameState& state, const Fleet& fleet)
{
    for (const auto& stack : fleet_ship_stacks(fleet)) if (const auto* d = find_ship_design(state, stack.design))
        if (weapons(*d) > 0 || bombs(*d) || assault(*d)) return true;
    return false;
}

bool hostile_intel(const PlayerView& view)
{
    for (const auto& planet : view.state.planets) {
        const auto owner = known_planet_owner(view.state, view.player, planet.id);
        if (owner && *owner != 0 && *owner != view.player) return true;
    }
    for (const auto& contact : view.state.players.front().enemyContacts)
        if (!contact.lost && contact.observedTurn <= view.state.turn
            && view.state.turn - contact.observedTurn <= 6) return true;
    return false;
}

void plan_military(const PlayerView& view, ComputerTurnPlan& plan, std::string& reason)
{
    const auto& state = view.state;
    auto& goals = plan.nextState.fleetGoals;
    std::vector<const Planet*> colonies, enemies;
    std::vector<const Fleet*> fleets;
    for (const auto& planet : state.planets) {
        if (planet.owner == view.player) colonies.push_back(&planet);
        const auto owner = known_planet_owner(state, view.player, planet.id);
        if (owner && *owner != 0 && *owner != view.player
            && system_intel_age(state, view.player, planet.star).value_or(1000) <= 30) enemies.push_back(&planet);
    }
    for (const auto& f : state.fleets) if (f.owner == view.player && military_fleet(state, f)) fleets.push_back(&f);
    const auto byId = [](auto a, auto b) { return a->id < b->id; };
    std::sort(colonies.begin(), colonies.end(), byId); std::sort(enemies.begin(), enemies.end(), byId);
    std::sort(fleets.begin(), fleets.end(), byId);
    std::erase_if(goals, [&](const auto& g) {
        if (g.kind <= ComputerFleetGoalKind::Resupply) return false;
        return std::none_of(fleets.begin(), fleets.end(), [&](auto f) { return f->id == g.fleet; });
    });
    if (colonies.empty()) return;

    const Planet* threatened = nullptr;
    double danger = 0;
    for (const auto* colony : colonies) if (const auto* star = find_star(state, colony->star)) {
        const auto estimate = contact_threat(view, star->position, 100);
        if (estimate > danger) { danger = estimate; threatened = colony; }
    }
    const Planet* target = nullptr;
    const Planet* rally = colonies.front();
    double bestDistance = std::numeric_limits<double>::infinity();
    for (const auto* enemy : enemies) for (const auto* colony : colonies) {
        if (!colony_has_orbital_service(state, colony->id, view.player, OrbitalStationModule::RefuelingDepot)) continue;
        const auto* a = find_star(state, enemy->star); const auto* b = find_star(state, colony->star);
        if (!a || !b) continue;
        const auto distance = distance_between(a->position, b->position);
        if (distance < bestDistance) { bestDistance = distance; target = enemy; rally = colony; }
    }
    const auto* rallyStar = find_star(state, rally->star);
    if (!rallyStar) return;
    const auto emit = [&](Fleet fleet, StarId starId, ComputerFleetGoalKind kind, FleetArrivalAction action = {}) {
        const auto* star = find_star(state, starId); if (!star) return false;
        auto route = std::vector<Position>{star->position};
        if (kind == ComputerFleetGoalKind::Raid) route.push_back(rallyStar->position);
        const auto warp = route_warp(state, fleet, route);
        if (!warp || goals.size() >= kMaximumComputerFleetGoals) return false;
        std::erase_if(goals, [&](const auto& g) { return g.fleet == fleet.id; });
        plan.orders.orders.push_back(MoveFleetOrder{fleet.id, star->position, *warp, action});
        goals.push_back({fleet.id, kind, starId, state.turn, review_turn(state, fleet)});
        explain(reason, std::string(kind == ComputerFleetGoalKind::Raid ? "Raid " : kind == ComputerFleetGoalKind::Defend ? "Defend "
            : kind == ComputerFleetGoalKind::Invade ? "Invade " : kind == ComputerFleetGoalKind::Resupply ? "Resupply at " : "Gather at ")
            + std::to_string(starId) + " by fleet " + std::to_string(fleet.id));
        return true;
    };

    std::vector<Fleet> ready;
    double defending = 0;
    if (threatened) for (const auto& goal : goals) if (goal.kind == ComputerFleetGoalKind::Defend && goal.target == threatened->star)
        for (const auto* fleet : fleets) if (fleet->id == goal.fleet) defending += strength(state, *fleet);
    for (const auto* original : fleets) {
        auto fleet = *original;
        auto goal = std::find_if(goals.begin(), goals.end(), [&](const auto& g) { return g.fleet == fleet.id; });
        if (goal != goals.end()) {
            if (fleet.telemetry.observedTurn <= goal->issuedTurn) continue;
            const auto* star = find_star(state, goal->target);
            const bool arrived = star && same_position(fleet.telemetry.position, star->position)
                && !fleet.telemetry.destination && fleet.telemetry.waypointQueue.empty();
            if (!arrived) continue; // Includes projected arrivals and in-flight replacements.
            if (goal->kind == ComputerFleetGoalKind::Bombard) {
                const auto* planet = find_planet_at_star(state, goal->target);
                const auto owner = planet ? known_planet_owner(state, view.player, planet->id) : std::nullopt;
                if (owner && *owner != 0 && *owner != view.player) {
                    if (fleet.telemetry.task != FleetTask::Bombardment) {
                        if (state.turn < goal->reviewTurn) continue;
                    } else if (state.turn - goal->issuedTurn < 8 || fleet.colonists < 1'000) continue;
                    else {
                        (void)emit(fleet, goal->target, ComputerFleetGoalKind::Invade,
                            {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Colonists});
                        continue;
                    }
                }
            }
            if (goal->kind == ComputerFleetGoalKind::Raid && fleet_bombs(state, fleet)) {
                const auto* planet = find_planet_at_star(state, goal->target);
                const auto owner = planet ? known_planet_owner(state, view.player, planet->id) : std::nullopt;
                if (owner && *owner != 0 && *owner != view.player && strategic_task_available(state, fleet, FleetTask::Bombardment)) {
                    plan.orders.orders.push_back(SetFleetTaskOrder{fleet.id, FleetTask::Bombardment});
                    goal->kind = ComputerFleetGoalKind::Bombard; goal->issuedTurn = state.turn; goal->reviewTurn = review_turn(state, fleet);
                    explain(reason, "Bombard system " + std::to_string(goal->target)); continue;
                }
            }
            if (goal->kind == ComputerFleetGoalKind::Defend && threatened && threatened->star == goal->target) continue;
            const auto completed = goal->kind;
            goals.erase(goal);
            if (completed == ComputerFleetGoalKind::Raid || completed == ComputerFleetGoalKind::Invade
                || completed == ComputerFleetGoalKind::Bombard) {
                if (emit(fleet, rally->star, ComputerFleetGoalKind::Resupply, {FleetArrivalActionKind::Refuel})) continue;
            }
        }
        if (!confirmed_idle(fleet, 0) || !idle(fleet) || fleet.damagePercent >= 100) continue;
        fleet.position = fleet.telemetry.position;
        const auto* local = colony_at(state, view.player, fleet.position);
        if (local && colony_has_orbital_service(state, local->id, view.player, OrbitalStationModule::RefuelingDepot))
            fleet.fuel = fleet_fuel_capacity(state, fleet);
        if (fleet.damagePercent > 40) {
            if (!local) (void)emit(fleet, rally->star, ComputerFleetGoalKind::Resupply, {FleetArrivalActionKind::Refuel});
            continue;
        }
        if (threatened && defending < 1.3 * danger && strength(state, fleet) > 0) {
            if (emit(fleet, threatened->star, ComputerFleetGoalKind::Defend)) defending += strength(state, fleet);
            continue;
        }
        if (!target) continue;
        if (!same_position(fleet.position, rallyStar->position)) {
            (void)emit(fleet, rally->star, ComputerFleetGoalKind::Gather); continue;
        }
        ready.push_back(fleet);
    }
    if (!ready.empty()) {
        auto& group = ready.front();
        if (ready.size() > 1) {
            // Merge only confirmed stationary contacts inside the local mesh.
            // Wait a boundary for the resulting manifest before planning travel.
            for (std::size_t i = 1; i < ready.size(); ++i)
                plan.orders.orders.push_back(MergeFleetsOrder{group.id, ready[i].id});
            explain(reason, "Assemble task group at " + std::to_string(rally->star));
        } else {
            if (fleet_cargo_capacity(state, group) >= 10 && fleet_radiation_safe(state, group)
                && group.colonists < 1'000 && rally->population > 500'000) {
                const auto available = std::min(rally->population / 4, rally->population - 500'000);
                const auto capacity = std::uint64_t(std::max(0.0, fleet_cargo_capacity(state, group) - mineral_cargo_mass(group.minerals)) * kColonistsPerCargoUnit);
                const auto load = std::min({capacity, std::uint64_t{1'000}, group.colonists + available});
                if (load > group.colonists) {
                    plan.orders.orders.push_back(SetFleetColonistsOrder{rally->id, group.id, load});
                    group.colonists = load; explain(reason, "Load assault troops into fleet " + std::to_string(group.id));
                }
            }
            const auto* targetStar = find_star(state, target->star);
            const auto required = 800 + 1.3 * contact_threat(view, targetStar->position, 60);
            const auto bomber = military_design(state, view.player, Role::Bomber);
            if (strength(state, group) >= required && (!bomber || (fleet_bombs(state, group) && group.colonists >= 1'000))) {
                explain(reason, "Attack rating " + std::to_string(std::uint64_t(strength(state, group)))
                    + "; assumed threat up to " + std::to_string(std::uint64_t(std::ceil(required))));
                (void)emit(group, target->star, ComputerFleetGoalKind::Raid);
            }
        }
    }

    std::uint32_t guards = 0, bombers = 0, transports = 0;
    const auto count = [&](const ShipDesign& design, std::uint32_t quantity) {
        if (weapons(design) > 0) guards += quantity;
        if (bombs(design)) bombers += quantity;
        if (assault(design)) transports += quantity;
    };
    for (const auto* fleet : fleets) for (const auto& stack : fleet_ship_stacks(*fleet))
        if (const auto* design = find_ship_design(state, stack.design)) count(*design, stack.count);
    for (const auto* colony : colonies) for (const auto& item : colony->productionQueue)
        if (item.kind == ProductionKind::ColonyShip) if (const auto* d = find_ship_design(state, item.shipDesign)) count(*d, item.quantity);
    const bool alert = hostile_intel(view);
    const auto guardDesign = military_design(state, view.player, Role::Guard);
    const auto bombDesign = military_design(state, view.player, Role::Bomber);
    const auto assaultDesign = military_design(state, view.player, Role::Assault);
    const auto opposition = target ? contact_threat(view, find_star(state, target->star)->position, 60) : 0;
    const auto wantedGuards = std::uint32_t(std::clamp(std::ceil(std::max(danger, opposition) / 500) + 2, 2.0, 8.0));
    std::vector<std::string> created;
    for (const auto* colony : colonies) {
        if (!colony->productionQueue.empty() || colony->mines < 2 || (!alert && colony->industry < 8)) continue;
        const auto* star = find_star(state, colony->star); if (!star) continue;
        const auto near = contact_threat(view, star->position, 100);
        if (colony_defenses_available(state, view.player) && colony->defenses < (near > 0 ? 20 : 5)
            && mineral_cargo_sufficient(colony->minerals, {10, 5, 5})) {
            plan.orders.orders.push_back(QueueProductionBatchOrder{colony->id, ProductionKind::ColonyDefense, 5});
            explain(reason, "Build defenses at " + std::to_string(colony->id)); continue;
        }
        const auto* station = find_orbital_station_at_planet(state, colony->id);
        const auto weapon = best_component(state, view.player, ShipComponentKind::BeamWeapon);
        if (station && station->owner == view.player && weapon && !station_has_weapons(*station)) {
            auto loadout = station_loadout(*station);
            loadout.components.push_back(*weapon); loadout.components.push_back(*weapon);
            if (const auto shield = best_component(state, view.player, ShipComponentKind::Shield)) loadout.components.push_back(*shield);
            StationConstruction work{loadout, station->id};
            if (station_construction_valid(state, *colony, work)
                && mineral_cargo_sufficient(colony->minerals, station_mineral_cost(loadout))) {
                plan.orders.orders.push_back(QueueStationOrder{colony->id, work});
                explain(reason, "Arm station at " + std::to_string(colony->id)); continue;
            }
        }
        if (!alert || !colony_has_orbital_service(state, colony->id, view.player, OrbitalStationModule::Shipyard)) continue;
        const ShipDesign* design = guards < wantedGuards && guardDesign ? &*guardDesign
            : target && bombers < 1 && bombDesign ? &*bombDesign
            : target && transports < 2 && assaultDesign ? &*assaultDesign : nullptr;
        if (!design || !mineral_cargo_sufficient(colony->minerals, ship_design_mineral_cost(*design))) continue;
        if (design->id == 0 && std::find(created.begin(), created.end(), design->name) == created.end()) {
            plan.orders.orders.push_back(CreateShipDesignOrder{design->name, design->hull, design->components, design->placements});
            created.push_back(design->name);
        }
        plan.orders.orders.push_back(QueueShipDesignOrder{colony->id, design->id, design->id == 0 ? design->name : std::string{}});
        count(*design, 1); explain(reason, "Build " + design->name + " at " + std::to_string(colony->id));
    }
    std::sort(goals.begin(), goals.end(), [](const auto& a, const auto& b) { return a.fleet < b.fleet; });
}
} // namespace suns::computer_detail
