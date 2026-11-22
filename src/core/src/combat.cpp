#include "suns/science_samples.hpp"
#include "suns/combat.hpp"
#include "suns/stations.hpp"
#include "suns/native_orbital.hpp"

#include "suns/communications.hpp"
#include "suns/equipment.hpp"
#include "suns/campaign.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/survey_probes.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <set>

namespace suns {
namespace {

constexpr double epsilon = 1e-8;
constexpr double withdrawalRadius = 8;
constexpr double withdrawalDistance = 4 * kFleetEncounterRadius;

using BattleOpponents = std::map<PlayerId, std::set<PlayerId>>;
using ContactKey = std::pair<bool, std::uint32_t>; // Station flag and object ID.

struct Weapon {
    ShipComponentSpec spec;
    std::uint32_t copies{};
    int initiative{};
};

struct Unit {
    FleetId fleet{};
    OrbitalStationId station{};
    PlayerId owner{};
    ShipDesignId design{};
    std::uint32_t count{};
    double fullArmor{};
    double effectiveArmor{};
    double armor{};
    double shields{};
    double shieldsPerShip{};
    double computerFailureFactor{1};
    double jammerFactor{1};
    double beamFactor{1};
    double deflectorFactor{1};
    double movement{1};
    bool anchored{};
    bool cargoShip{};
    FleetBattlePlan plan;
    std::uint8_t withdrawalRound{};
    Position position;
    Position outward;
    std::vector<Weapon> weapons;
};

bool is_weapon(const ShipComponentSpec& spec)
{
    return spec.weaponPower > 0 && (spec.kind == ShipComponentKind::BeamWeapon
        || spec.kind == ShipComponentKind::Torpedo);
}

bool matches_target(BattleTarget category, bool armed, bool cargo, bool station)
{
    switch (category) {
    case BattleTarget::Any: return true;
    case BattleTarget::ArmedShips: return armed && !station;
    case BattleTarget::CargoShips: return cargo;
    case BattleTarget::Stations: return station;
    }
    return false;
}

std::uint64_t mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

std::uint64_t hit_count(std::uint64_t fired, double probability, std::uint64_t seed)
{
    // Bounded work even for enormous stacks. One shot is Bernoulli; large
    // volleys use seeded stochastic rounding of the expected hit count.
    const double expected = fired * std::clamp(probability, 0.0, 1.0);
    const auto whole = static_cast<std::uint64_t>(std::floor(expected));
    const double roll = (mix(seed) >> 11) * (1.0 / 9007199254740992.0);
    return std::min(fired, whole + (roll < expected - whole ? 1 : 0));
}

double threat(const Unit& unit)
{
    double value = 0;
    for (const auto& weapon : unit.weapons)
        value += weapon.spec.weaponPower * weapon.copies;
    return value * unit.count;
}

bool active(const Unit& unit)
{
    return unit.count && !unit.withdrawalRound;
}

bool combat_continues(const std::vector<Unit>& units, const BattleOpponents& opponents)
{
    std::set<PlayerId> fighting;
    for (const auto& unit : units) if (active(unit)) fighting.insert(unit.owner);
    for (const auto owner : fighting)
        for (const auto enemy : opponents.at(owner))
            if (fighting.contains(enemy)) return true;
    return false;
}

bool weapon_can_affect(const ShipComponentSpec& weapon, const Unit& target)
{
    return !weapon.shieldOnly || target.shields > epsilon;
}

double effective_weapon_range(const Unit& source, const Unit& target)
{
    double range = 0;
    for (const auto& weapon : source.weapons)
        if (weapon_can_affect(weapon.spec, target)) range = std::max(range, double(weapon.spec.weaponRange));
    return range;
}

std::optional<bool> target_priority(const Unit& source, const Unit& target,
    const std::set<PlayerId>& opponents, const ShipComponentSpec* weapon)
{
    if (!active(target) || !opponents.contains(target.owner)) return std::nullopt;
    if (weapon) {
        if (!weapon_can_affect(*weapon, target)
            || distance_between(source.position, target.position) > weapon->weaponRange + epsilon) return std::nullopt;
    } else if (effective_weapon_range(source, target) <= 0) {
        return std::nullopt;
    }
    const bool preferred = matches_target(source.plan.preferredTarget, !target.weapons.empty(),
        target.cargoShip, target.station != 0);
    if (preferred || source.plan.allowOtherTargets) return preferred;
    return std::nullopt;
}

std::optional<std::size_t> target_for(const std::vector<Unit>& units, std::size_t sourceIndex,
    const std::set<PlayerId>& opponents, const ShipComponentSpec* weapon = nullptr)
{
    std::optional<std::size_t> best;
    bool bestPreferred = false;
    const auto& source = units[sourceIndex];
    for (std::size_t i = 0; i < units.size(); ++i) {
        const auto preferred = target_priority(source, units[i], opponents, weapon);
        if (!preferred.has_value()) continue;
        if (!best || *preferred > bestPreferred || (*preferred == bestPreferred
            && (threat(units[i]) > threat(units[*best]) + epsilon
                || (std::abs(threat(units[i]) - threat(units[*best])) <= epsilon
                    && distance_between(source.position, units[i].position)
                        < distance_between(source.position, units[*best].position) - epsilon)))) {
            best = i;
            bestPreferred = *preferred;
        }
    }
    return best;
}

Position move_towards(Position start, Position destination, double step)
{
    const auto distance = distance_between(start, destination);
    if (distance <= epsilon) return start;
    const auto fraction = std::min(step, distance) / distance;
    return {start.x + (destination.x - start.x) * fraction,
        start.y + (destination.y - start.y) * fraction};
}

Position combat_movement(const std::vector<Unit>& units, std::size_t index,
    const std::set<PlayerId>& opponents, const std::map<FleetId, double>& withdrawalSpeeds)
{
    const auto& unit = units[index];
    if (!active(unit) || unit.movement <= 0) return unit.position;
    if (unit.plan.tactic == BattleTactic::Withdraw)
        return move_towards(unit.position, {unit.outward.x * withdrawalRadius, unit.outward.y * withdrawalRadius},
            withdrawalSpeeds.at(unit.fleet));
    if (unit.weapons.empty()) return unit.position;
    const auto target = target_for(units, index, opponents);
    if (!target) return unit.position;
    const auto& enemy = units[*target];

    if (unit.plan.tactic == BattleTactic::Escort) {
        const Unit* cargo = nullptr;
        for (std::size_t i = 0; i < units.size(); ++i) {
            const auto& candidate = units[i];
            if (i != index && active(candidate) && candidate.owner == unit.owner && candidate.cargoShip
                && (candidate.weapons.empty() || candidate.plan.tactic != BattleTactic::Escort)
                && (!cargo || distance_between(unit.position, candidate.position)
                    < distance_between(unit.position, cargo->position))) cargo = &candidate;
        }
        if (cargo) {
            const auto screen = move_towards(cargo->position, enemy.position, 1);
            return move_towards(unit.position, screen, unit.movement);
        }
    }

    const auto range = effective_weapon_range(unit, enemy);
    const auto distance = distance_between(unit.position, enemy.position);
    if (distance > range + epsilon)
        return move_towards(unit.position, enemy.position, std::min(unit.movement, distance - range));
    if (unit.plan.tactic != BattleTactic::KeepDistance || range - distance <= epsilon) return unit.position;
    const auto step = std::min(unit.movement, range - distance);
    const auto direction = distance <= epsilon ? unit.outward
        : Position{(unit.position.x - enemy.position.x) / distance, (unit.position.y - enemy.position.y) / distance};
    return {unit.position.x + direction.x * step, unit.position.y + direction.y * step};
}

void configure_unit(Unit& unit, const std::vector<ShipComponentType>& componentsToFit, int initiative, double& dampener)
{
    std::map<ShipComponentType, std::uint32_t> components;
    for (const auto component : componentsToFit) ++components[component];
    for (const auto& [component, copies] : components) {
        const auto spec = component_spec(component);
        initiative += spec.initiativeBonus * copies;
        unit.computerFailureFactor *= std::pow(1 - spec.accuracyBonusPercent / 100, copies);
        unit.jammerFactor *= std::pow(1 - spec.jammingPercent / 100, copies);
        unit.beamFactor *= std::pow(1 + spec.beamBonusPercent / 100, copies);
        unit.deflectorFactor *= std::pow(1 - spec.beamDeflectionPercent / 100, copies);
        unit.movement += spec.battleMovementBonus * copies;
        dampener = std::max(dampener, spec.battleMovementPenalty);
        if (is_weapon(spec)) unit.weapons.push_back({spec, copies, 0});
    }
    unit.beamFactor = std::min(2.5, unit.beamFactor);
    unit.deflectorFactor = std::max(0.1, unit.deflectorFactor);
    for (auto& weapon : unit.weapons) weapon.initiative = initiative + weapon.spec.weaponInitiative;
}

struct BattleResult {
    SpaceBattleReport report;
    std::map<FleetId, Position> withdrawalExits;
};

BattleResult fight(const GameState& state, const std::vector<FleetId>& fleets,
    const std::vector<OrbitalStationId>& stations, const BattleOpponents& opponents,
    const std::set<ContactKey>& initiators, Position position, std::uint64_t turn)
{
    BattleResult result;
    auto& report = result.report;
    report.observedTurn = turn;
    report.position = position;
    std::vector<Unit> units;
    std::set<PlayerId> owners;
    double dampener = 0;
    std::uint64_t seed = mix(state.galaxySeed ^ mix(turn));
    for (const auto id : fleets) {
        seed = mix(seed ^ id);
        const auto found = std::find_if(state.fleets.begin(), state.fleets.end(),
            [&](const auto& f) { return f.id == id; });
        if (found == state.fleets.end()) continue;
        owners.insert(found->owner);
        auto stacks = fleet_ship_stacks(*found);
        std::sort(stacks.begin(), stacks.end(), [](const auto& a, const auto& b) { return a.design < b.design; });
        for (const auto& stack : stacks) {
            const auto* design = find_ship_design(state, stack.design);
            if (!design) continue;
            Unit unit;
            unit.fleet = id;
            unit.owner = found->owner;
            unit.design = stack.design;
            unit.count = stack.count;
            unit.fullArmor = std::max(1.0, ship_design_armor(*design));
            // Warp damage at 100% disables engines without deleting the hull.
            unit.effectiveArmor = std::max(0.001, unit.fullArmor * (1 - found->damagePercent / 100));
            unit.armor = unit.count * unit.effectiveArmor;
            unit.shieldsPerShip = ship_design_shields(*design);
            unit.shields = unit.count * unit.shieldsPerShip;
            int initiative = hull_spec(design->hull).initiative;
            configure_unit(unit,design->components,initiative,dampener);
            unit.plan = found->battlePlan;
            unit.cargoShip = ship_design_cargo_capacity(*design) > 0;
            unit.anchored = found->outpost != OutpostKind::None
                || (unit.plan.tactic == BattleTactic::Withdraw && found->damagePercent >= 100);
            units.push_back(unit);
            report.units.push_back({unit.owner, id, stack.design, found->name, design->name,
                stack.count, stack.count, unit.armor, unit.shields});
            report.units.back().initiated = initiators.contains({false, id});
            report.units.back().attemptedWithdrawal = unit.plan.tactic == BattleTactic::Withdraw;
        }
    }
    for (const auto id : stations) {
        const auto* station=find_orbital_station(state,id); if (!station) continue;
        owners.insert(station->owner); seed=mix(seed ^ (std::uint64_t(id)<<32) ^ 0x53544154494f4eULL);
        Unit unit; unit.station=id; unit.owner=station->owner; unit.count=1;
        unit.fullArmor=station_armor(*station);
        unit.effectiveArmor=std::max(0.001,unit.fullArmor*(1-station->damagePercent/100));
        unit.armor=unit.effectiveArmor; unit.shieldsPerShip=station_shields(*station); unit.shields=unit.shieldsPerShip;
        configure_unit(unit,station->components,0,dampener); unit.movement=0;
        units.push_back(unit);
        report.units.push_back({unit.owner,0,0,station->name,station_hull_spec(station->hull).name,1,1,unit.armor,unit.shields,id});
        report.units.back().initiated = initiators.contains({true, id});
    }
    std::vector<PlayerId> ownerList(owners.begin(), owners.end());
    for (auto& unit : units) {
        const auto owner = std::find(ownerList.begin(), ownerList.end(), unit.owner) - ownerList.begin();
        const double angle = 2 * std::numbers::pi * owner / ownerList.size();
        const double radius=stations.empty() ? 4 : 1;
        unit.outward = {std::cos(angle), std::sin(angle)};
        unit.position = unit.station ? Position{} : Position{radius * unit.outward.x, radius * unit.outward.y};
        unit.movement = unit.station || unit.anchored ? 0 : std::clamp(unit.movement-dampener,0.25,2.5);
    }
    struct Action { std::size_t unit; std::size_t weapon; int initiative; };
    std::vector<Action> actions;
    for (std::size_t i = 0; i < units.size(); ++i)
        for (std::size_t w = 0; w < units[i].weapons.size(); ++w)
            actions.push_back({i, w, units[i].weapons[w].initiative});
    std::stable_sort(actions.begin(), actions.end(), [](const auto& a, const auto& b) {
        return a.initiative > b.initiative;
    });
    std::vector<Position> moved(units.size());
    for (std::uint8_t round = 1; round <= kSpaceBattleRoundLimit; ++round) {
        if (!combat_continues(units, opponents)) break;
        report.rounds = round;
        // Movement is simultaneous, using a single round-start snapshot.
        std::map<FleetId, double> withdrawalSpeeds;
        for (const auto& unit : units) if (active(unit) && unit.plan.tactic == BattleTactic::Withdraw) {
            const auto [it, inserted] = withdrawalSpeeds.try_emplace(unit.fleet, unit.movement);
            if (!inserted) it->second = std::min(it->second, unit.movement);
        }
        for (std::size_t i = 0; i < units.size(); ++i)
            moved[i] = combat_movement(units, i, opponents.at(units[i].owner), withdrawalSpeeds);
        for (std::size_t i = 0; i < units.size(); ++i) units[i].position = moved[i];
        for (std::size_t begin = 0; begin < actions.size();) {
            std::size_t end = begin + 1;
            while (end < actions.size() && actions[end].initiative == actions[begin].initiative) ++end;
            std::vector<SpaceBattleShot> volleys;
            // Equal initiative fires simultaneously: ships killed in this tier
            // still fire, ships killed in an earlier tier cannot fire.
            for (std::size_t a = begin; a < end; ++a) {
                const auto& action = actions[a];
                const auto& source = units[action.unit];
                if (!active(source)) continue;
                const auto& weapon = source.weapons[action.weapon];
                std::vector<std::size_t> targets;
                const auto& enemies = opponents.at(source.owner);
                if (const auto target = target_for(units, action.unit, enemies, &weapon.spec)) {
                    if (weapon.spec.gatling) {
                        const auto priority = target_priority(source, units[*target], enemies, &weapon.spec);
                        for (std::size_t i = 0; i < units.size(); ++i)
                            if (target_priority(source, units[i], enemies, &weapon.spec) == priority) targets.push_back(i);
                    } else {
                        targets.push_back(*target);
                    }
                }
                for (const auto target : targets) {
                    const std::uint64_t fired = std::uint64_t(source.count) * weapon.copies;
                    const double accuracy = weapon.spec.kind == ShipComponentKind::Torpedo
                        ? (1 - (1 - weapon.spec.weaponAccuracy / 100) * source.computerFailureFactor)
                            * units[target].jammerFactor : 1;
                    const auto hits = hit_count(fired, accuracy, seed ^ mix(round)
                        ^ mix(action.unit + 1000 * action.weapon) ^ mix(100000 + target));
                    volleys.push_back({round, std::uint32_t(action.unit), std::uint32_t(target),
                        weapon.spec.type, fired, hits, distance_between(source.position, units[target].position)});
                }
            }
            for (auto& shot : volleys) {
                auto& target = units[shot.target];
                const auto& source = units[shot.attacker];
                const auto weapon = component_spec(shot.weapon);
                const auto beforeCount = target.count;
                double damage = shot.hits * weapon.weaponPower;
                double directArmor = 0;
                if (weapon.kind == ShipComponentKind::BeamWeapon) {
                    damage *= source.beamFactor * target.deflectorFactor * std::max(0.1, 1 - 0.1 * shot.range);
                } else {
                    if (weapon.missile && target.shields <= epsilon) damage *= 2;
                    directArmor = damage / 2;
                    damage -= directArmor;
                }
                shot.shieldDamage = std::min(target.shields, damage);
                target.shields -= shot.shieldDamage;
                shot.armorDamage = weapon.shieldOnly ? 0
                    : std::min(target.armor, directArmor + damage - shot.shieldDamage);
                target.armor = std::max(0.0, target.armor - shot.armorDamage);
                target.count = target.armor <= epsilon ? 0 : std::uint32_t(std::clamp(
                    std::ceil(target.armor / target.effectiveArmor - epsilon), 1.0, double(beforeCount)));
                if (!target.count) target.armor = 0;
                target.shields = std::min(target.shields, target.count * target.shieldsPerShip);
                shot.shipsDestroyed = beforeCount - target.count;
                if (report.shots.size() < kSpaceBattleLogLimit) report.shots.push_back(shot);
                else report.logTruncated = true;
            }
            begin = end;
        }
        for (auto& unit : units) {
            if (!active(unit) || unit.plan.tactic != BattleTactic::Withdraw
                || std::hypot(unit.position.x, unit.position.y) < withdrawalRadius - epsilon) continue;
            unit.withdrawalRound = round;
            result.withdrawalExits.try_emplace(unit.fleet,
                Position{position.x + unit.outward.x * withdrawalDistance, position.y + unit.outward.y * withdrawalDistance});
        }
    }
    for (std::size_t i = 0; i < units.size(); ++i) {
        report.units[i].survivingShips = units[i].count;
        report.units[i].remainingArmor = units[i].armor;
        report.units[i].remainingShields = units[i].shields;
        report.units[i].withdrawalRound = units[i].withdrawalRound;
    }
    report.stalemate = combat_continues(units, opponents);
    return result;
}

void apply_battle(GameState& state, const BattleResult& result)
{
    const auto& report = result.report;
    std::set<PlayerId> recipients;
    std::set<FleetId> fleetIds;
    for (const auto& u : report.units) { recipients.insert(u.owner); if (u.fleet) fleetIds.insert(u.fleet); }
    std::map<PlayerId, std::uint64_t> deliveries;
    // Broadcast result packets while the pre-casualty relay network still
    // exists. This does not require a destroyed ship to transmit later.
    for (const auto recipient : recipients) {
        const auto delivery = dispatch_report_delivery_turn(state,recipient,report.position,report.observedTurn);
        deliveries[recipient] = delivery;
        auto player = std::find_if(state.players.begin(), state.players.end(),
            [&](const auto& p) { return p.id == recipient; });
        if (player == state.players.end()) continue;
        PendingPlayerReport packet;
        packet.kind = PlayerReportKind::SpaceBattle;
        packet.observedTurn = report.observedTurn;
        packet.deliveryTurn = delivery;
        packet.position = report.position;
        packet.battle = report;
        packet.transmissionFleet = held_report_source(state,recipient,report.position,report.observedTurn);
        for (const auto& u : report.units) if (u.owner == recipient) { packet.fleet = u.fleet; break; }
        for (const auto& star : state.stars) if (distance_between(star.position, report.position) <= kFleetEncounterRadius) {
            packet.star = star.id;
            if (const auto* planet = find_planet_at_star(state, star.id)) packet.planet = planet->id;
            break;
        }
        player->pendingPlayerReports.push_back(std::move(packet));
    }
    SalvageWreck wreck;
    wreck.position = report.position; wreck.expiresTurn = report.observedTurn + 20;
    bool debris = false;
    for (const auto& u : report.units) if (u.survivingShips < u.initialShips) {
        debris = true;
        if (const auto* d = find_ship_design(state,u.design)) for (const auto& unlock : research_unlocks()) {
            if ((!unlock.component || std::find(d->components.begin(),d->components.end(),*unlock.component) == d->components.end())
                && (!unlock.hull || *unlock.hull != d->hull)) continue;
            const auto primary = std::size_t(unlock.field);
            wreck.technology[primary] = std::max(wreck.technology[primary],unlock.level);
            for (std::size_t i = 0; i < kResearchFieldCount; ++i)
                wreck.technology[i] = std::max(wreck.technology[i],unlock.extraLevels[i]);
        }
    }
    if (debris) { wreck.id = state.nextWreckId++; state.wrecks.push_back(wreck); }
    std::set<OrbitalStationId> destroyedStations;
    for(auto& station:state.orbitalStations) {
        const auto unit=std::find_if(report.units.begin(),report.units.end(),[&](const auto& u) { return u.station==station.id; });
        if(unit==report.units.end()) continue;
        if(deliveries[station.owner]>report.observedTurn)
            state.pendingStationContacts.push_back({station,report.observedTurn,deliveries[station.owner]});
        if(!unit->survivingShips) destroyedStations.insert(station.id);
        else station.damagePercent=std::clamp(100*(1-unit->remainingArmor/station_armor(station)),0.0,99.999999);
    }
    for (const auto station : destroyedStations) destroy_native_orbital_population(state, station);
    std::erase_if(state.orbitalStations,[&](const auto& s) { return destroyedStations.contains(s.id); });
    std::set<FleetId> destroyed;
    for (auto& fleet : state.fleets) {
        if (!fleetIds.contains(fleet.id)) continue;
        const double cargoBefore = fleet_cargo_capacity(state, fleet);
        const double fuelBefore = fleet_fuel_capacity(state, fleet);
        std::vector<FleetShipStack> survivors;
        double remainingArmor = 0, fullArmor = 0;
        for (const auto& u : report.units) if (u.fleet == fleet.id && u.survivingShips) {
            survivors.push_back({u.design, u.survivingShips});
            remainingArmor += u.remainingArmor;
            if (const auto* design = find_ship_design(state, u.design))
                fullArmor += std::max(1.0, ship_design_armor(*design)) * u.survivingShips;
        }
        if (survivors.empty()) {
            destroyed.insert(fleet.id);
            if (deliveries[fleet.owner] > report.observedTurn) {
                auto contact = fleet_player_view(state, fleet);
                contact.pendingCommands.clear();
                contact.telemetryInTransit = fleet.telemetryInTransit;
                state.pendingFleetLossContacts.push_back({std::move(contact), deliveries[fleet.owner]});
            }
            continue;
        }
        fleet.ships = std::move(survivors);
        normalize_fleet_composition(fleet);
        const double cargoAfter = fleet_cargo_capacity(state, fleet);
        const double samplesBefore = science_sample_mass(fleet.scienceSamples);
        fleet.scienceSamples.resize(std::min(fleet.scienceSamples.size(), std::size_t(
            std::max(0.0, std::floor((cargoAfter + epsilon) / kScienceSampleMass)))));
        const auto crystalsBefore = fleet.xenocrystal;
        fleet.xenocrystal = std::min(fleet.xenocrystal, std::uint32_t(std::max(0.0,
            std::min(double(fleet.xenocrystal), std::floor(cargoAfter - science_sample_mass(fleet.scienceSamples) + epsilon)))));
        const double ordinaryBefore = cargoBefore - samplesBefore - crystalsBefore;
        const double ordinaryAfter = cargoAfter - science_sample_mass(fleet.scienceSamples) - fleet.xenocrystal;
        const double cargoShare = ordinaryBefore > epsilon
            ? std::clamp(ordinaryAfter / ordinaryBefore, 0.0, 1.0) : 0;
        fleet.colonists = std::uint64_t(std::floor(fleet.colonists * cargoShare));
        fleet.minerals.ironium *= cargoShare;
        fleet.minerals.boranium *= cargoShare;
        fleet.minerals.germanium *= cargoShare;
        const double fuelAfter = fleet_fuel_capacity(state, fleet);
        fleet.fuel = std::min(fuelAfter, fleet.fuel * (fuelBefore > epsilon ? fuelAfter / fuelBefore : 0));
        fleet.damagePercent = std::clamp(100 * (1 - remainingArmor / fullArmor), 0.0, 99.999999);
        const auto exit = result.withdrawalExits.find(fleet.id);
        if (exit != result.withdrawalExits.end()) {
            fleet.position = exit->second;
            fleet.lastMovedTurn = report.observedTurn;
            fleet.visitor = {};
            if (fleet.probe.enabled) pause_probe(fleet);
        }
        // An encounter interrupts the current onboard programme; new commands
        // already in flight retain their ordinary delivery semantics.
        fleet.destination.reset();
        fleet.arrivalAction.reset();
        fleet.waypointQueue.clear();
        fleet.targetFleet = 0;
        fleet.repeatOrders = false;
        fleet.routeTemplate.clear();
        // A surviving bomber may continue its orbital mission once defenses are cleared.
        if (fleet.task != FleetTask::Bombardment || exit != result.withdrawalExits.end()) fleet.task = FleetTask::None;
        fleet.fuelStalled = false;
    }
    std::erase_if(state.fleets, [&](const auto& f) { return destroyed.contains(f.id); });
}

} // namespace

bool fleet_has_space_weapons(const GameState& state, const Fleet& fleet)
{
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(state, stack.design))
            for (const auto component : design->components)
                if (is_weapon(component_spec(component))) return true;
    return false;
}

bool fleet_battle_plan_valid(const FleetBattlePlan& plan)
{
    return plan.preferredTarget <= BattleTarget::Stations && plan.tactic <= BattleTactic::Withdraw;
}

BattleContact battle_contact(const GameState& state, const Fleet& fleet)
{
    BattleContact contact{fleet.owner, false, false, false, fleet.battlePlan};
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        if (!design) continue;
        contact.cargoShips |= ship_design_cargo_capacity(*design) > 0;
        if (!contact.armed)
            for (const auto component : design->components) {
                if (!is_weapon(component_spec(component))) continue;
                contact.armed = true;
                break;
            }
        if (contact.cargoShips && contact.armed) break;
    }
    return contact;
}

BattleContact battle_contact(const OrbitalStation& station)
{
    return {station.owner, station_has_weapons(station), false, true, {}};
}

bool can_start_space_battle(const BattleContact& attacker, const BattleContact& target)
{
    return attacker.owner != target.owner && attacker.armed && attacker.plan.attackOnContact
        && (attacker.plan.allowOtherTargets || matches_target(attacker.plan.preferredTarget,
            target.armed, target.cargoShips, target.station));
}

bool space_battle_between(const BattleContact& a, const BattleContact& b)
{
    return can_start_space_battle(a, b) || can_start_space_battle(b, a);
}

void resolve_space_battles(GameState& state, std::uint64_t observationTurn,
    std::span<const SpaceBattleEncounter> encounters)
{
    struct Contact {
        std::uint32_t id;
        Position position;
        BattleContact profile;
        ContactKey key() const { return {profile.station, id}; }
    };
    std::vector<Contact> contacts;
    for (const auto& fleet : state.fleets)
        contacts.push_back({fleet.id, fleet.position, battle_contact(state, fleet)});
    for (const auto& station : state.orbitalStations) {
        const auto* planet = find_planet(state, station.planet);
        const auto* star = planet ? find_star(state, planet->star) : nullptr;
        if (star) contacts.push_back({station.id, star->position, battle_contact(station)});
    }
    std::sort(contacts.begin(), contacts.end(), [](const auto& a, const auto& b) { return a.key() < b.key(); });

    const auto contact_by_key = [&](ContactKey key) -> Contact* {
        const auto it = std::lower_bound(contacts.begin(), contacts.end(), key,
            [](const Contact& contact, ContactKey key) { return contact.key() < key; });
        return it == contacts.end() || it->key() != key ? nullptr : &*it;
    };
    std::map<ContactKey, std::set<ContactKey>> attacks;
    std::set<ContactKey> intercepted;
    const auto join = [&](Contact& contact, Position encounter) {
        intercepted.insert(contact.key());
        // A tactical participant shares the battle location even when its
        // strategic position, such as an anchored outpost, cannot change.
        contact.position = encounter;
    };
    for (const auto& encounter : encounters) {
        bool hasSurvivingPair = false;
        for (const auto& engagement : encounter.engagements) {
            auto* first = contact_by_key({false, engagement.fleet});
            auto* second = engagement.station ? contact_by_key({true, engagement.station})
                : contact_by_key({false, engagement.otherFleet});
            // A mine can destroy a ship before it reaches its selected contact.
            if (!first || !second) continue;
            hasSurvivingPair = true;
            join(*first, encounter.position);
            join(*second, encounter.position);
            if (engagement.fleetInitiated) attacks[first->key()].insert(second->key());
            if (engagement.otherInitiated) attacks[second->key()].insert(first->key());
        }
        if (!hasSurvivingPair) continue;
        for (const auto id : encounter.escorts)
            if (auto* escort = contact_by_key({false, id})) join(*escort, encounter.position);
    }
    const auto starts_battle = [&](const Contact& attacker, const Contact& target) {
        // For an intercepted fleet, only an admitted contact on its original
        // trajectory can initiate combat. Endpoints can make escorts look
        // co-located with an enemy they had not yet encountered.
        if (intercepted.contains(attacker.key()) || intercepted.contains(target.key())) {
            const auto found = attacks.find(attacker.key());
            return found != attacks.end() && found->second.contains(target.key());
        }
        return can_start_space_battle(attacker.profile, target.profile)
            && distance_between(attacker.position, target.position) <= kFleetEncounterRadius + 1e-6;
    };
    std::set<ContactKey> resolved;
    for (const auto& anchor : contacts) {
        if (resolved.contains(anchor.key()) || !anchor.profile.armed || !anchor.profile.plan.attackOnContact) continue;
        std::vector<const Contact*> local;
        bool beginsBattle = false;
        for (const auto& other : contacts) {
            if (resolved.contains(other.key())
                || distance_between(anchor.position, other.position) > kFleetEncounterRadius + 1e-6) continue;
            local.push_back(&other);
            beginsBattle |= starts_battle(anchor, other);
        }
        if (!beginsBattle) continue;

        BattleOpponents opponents;
        std::set<ContactKey> initiators;
        for (const auto* attacker : local) for (const auto* target : local) {
            if (!starts_battle(*attacker, *target)) continue;
            initiators.insert(attacker->key());
            opponents[attacker->profile.owner].insert(target->profile.owner);
            opponents[target->profile.owner].insert(attacker->profile.owner);
        }
        // A contact activates only its two empires. Local allies can support
        // them, while unrelated encounters keep separate reports.
        std::set<PlayerId> assigned;
        for (const auto& edge : opponents) {
            const auto owner = edge.first;
            if (!assigned.insert(owner).second) continue;
            std::vector<PlayerId> sideOwners{owner};
            for (std::size_t i = 0; i < sideOwners.size(); ++i)
                for (const auto enemy : opponents.at(sideOwners[i]))
                    if (assigned.insert(enemy).second) sideOwners.push_back(enemy);
            std::vector<const Contact*> participants;
            std::vector<FleetId> fleets;
            std::vector<OrbitalStationId> stations;
            for (const auto* contact : local) {
                if (std::find(sideOwners.begin(), sideOwners.end(), contact->profile.owner) == sideOwners.end()) continue;
                participants.push_back(contact);
                if (contact->profile.station) stations.push_back(contact->id);
                else fleets.push_back(contact->id);
            }
            const auto result = fight(state, fleets, stations, opponents, initiators,
                participants.front()->position, observationTurn);
            if (!result.report.rounds) continue;
            apply_battle(state, result);
            for (const auto* contact : participants) resolved.insert(contact->key());
        }
    }
}

} // namespace suns
