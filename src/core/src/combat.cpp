#include "suns/combat.hpp"

#include "suns/communications.hpp"
#include "suns/equipment.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <set>

namespace suns {
namespace {

constexpr double epsilon = 1e-8;

struct Weapon {
    ShipComponentSpec spec;
    std::uint32_t copies{};
    int initiative{};
};

struct Unit {
    FleetId fleet{};
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
    Position position;
    std::vector<Weapon> weapons;
};

bool is_weapon(const ShipComponentSpec& spec)
{
    return spec.weaponPower > 0 && (spec.kind == ShipComponentKind::BeamWeapon
        || spec.kind == ShipComponentKind::Torpedo);
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

std::optional<std::size_t> target_for(const std::vector<Unit>& units, std::size_t source,
    const ShipComponentSpec* weapon = nullptr)
{
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (!units[i].count || units[i].owner == units[source].owner) continue;
        if (weapon && (distance_between(units[source].position, units[i].position)
                > weapon->weaponRange + epsilon
            || (weapon->shieldOnly && units[i].shields <= epsilon))) continue;
        if (!best || threat(units[i]) > threat(units[*best]) + epsilon
            || (std::abs(threat(units[i]) - threat(units[*best])) <= epsilon
                && distance_between(units[source].position, units[i].position)
                    < distance_between(units[source].position, units[*best].position) - epsilon)) best = i;
    }
    return best;
}

SpaceBattleReport fight(const GameState& state, const std::vector<FleetId>& fleets,
    Position position, std::uint64_t turn)
{
    SpaceBattleReport report;
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
            std::map<ShipComponentType, std::uint32_t> components;
            for (const auto component : design->components) ++components[component];
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
            units.push_back(unit);
            report.units.push_back({unit.owner, id, stack.design, found->name, design->name,
                stack.count, stack.count, unit.armor, unit.shields});
        }
    }
    std::vector<PlayerId> ownerList(owners.begin(), owners.end());
    for (auto& unit : units) {
        const auto owner = std::find(ownerList.begin(), ownerList.end(), unit.owner) - ownerList.begin();
        const double angle = 2 * std::numbers::pi * owner / ownerList.size();
        unit.position = {4 * std::cos(angle), 4 * std::sin(angle)};
        unit.movement = std::clamp(unit.movement - dampener, 0.25, 2.5);
    }
    struct Action { std::size_t unit; std::size_t weapon; int initiative; };
    std::vector<Action> actions;
    for (std::size_t i = 0; i < units.size(); ++i)
        for (std::size_t w = 0; w < units[i].weapons.size(); ++w)
            actions.push_back({i, w, units[i].weapons[w].initiative});
    std::stable_sort(actions.begin(), actions.end(), [](const auto& a, const auto& b) {
        return a.initiative > b.initiative;
    });
    for (std::uint8_t round = 1; round <= kSpaceBattleRoundLimit; ++round) {
        std::set<PlayerId> living;
        for (const auto& u : units) if (u.count) living.insert(u.owner);
        if (living.size() < 2) break;
        report.rounds = round;
        // Movement is simultaneous, using a single round-start snapshot.
        auto moved = units;
        for (std::size_t i = 0; i < units.size(); ++i) {
            if (!units[i].count || units[i].weapons.empty()) continue;
            const auto target = target_for(units, i);
            if (!target) continue;
            double range = 0;
            for (const auto& w : units[i].weapons) range = std::max(range, double(w.spec.weaponRange));
            const auto distance = distance_between(units[i].position, units[*target].position);
            if (distance <= range + epsilon) continue;
            const auto step = std::min(units[i].movement, distance - range);
            moved[i].position.x += (units[*target].position.x - units[i].position.x) * step / distance;
            moved[i].position.y += (units[*target].position.y - units[i].position.y) * step / distance;
        }
        for (std::size_t i = 0; i < units.size(); ++i) units[i].position = moved[i].position;
        for (std::size_t begin = 0; begin < actions.size();) {
            std::size_t end = begin + 1;
            while (end < actions.size() && actions[end].initiative == actions[begin].initiative) ++end;
            std::vector<SpaceBattleShot> volleys;
            // Equal initiative fires simultaneously: ships killed in this tier
            // still fire, ships killed in an earlier tier cannot fire.
            for (std::size_t a = begin; a < end; ++a) {
                const auto& action = actions[a];
                const auto& source = units[action.unit];
                if (!source.count) continue;
                const auto& weapon = source.weapons[action.weapon];
                std::vector<std::size_t> targets;
                if (weapon.spec.gatling) {
                    for (std::size_t i = 0; i < units.size(); ++i)
                        if (units[i].count && units[i].owner != source.owner
                            && distance_between(source.position, units[i].position) <= weapon.spec.weaponRange + epsilon)
                            targets.push_back(i);
                } else if (const auto target = target_for(units, action.unit, &weapon.spec)) targets.push_back(*target);
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
    }
    std::set<PlayerId> living;
    for (std::size_t i = 0; i < units.size(); ++i) {
        report.units[i].survivingShips = units[i].count;
        report.units[i].remainingArmor = units[i].armor;
        report.units[i].remainingShields = units[i].shields;
        if (units[i].count) living.insert(units[i].owner);
    }
    report.stalemate = living.size() > 1;
    return report;
}

void apply_battle(GameState& state, const SpaceBattleReport& report)
{
    std::set<PlayerId> recipients;
    std::set<FleetId> fleetIds;
    for (const auto& u : report.units) { recipients.insert(u.owner); fleetIds.insert(u.fleet); }
    std::map<PlayerId, std::uint64_t> deliveries;
    // Broadcast result packets while the pre-casualty relay network still
    // exists. This does not require a destroyed ship to transmit later.
    for (const auto recipient : recipients) {
        const auto delivery = report.observedTurn + communication_delay_turns(state, recipient, report.position);
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
        for (const auto& u : report.units) if (u.owner == recipient) { packet.fleet = u.fleet; break; }
        for (const auto& star : state.stars) if (distance_between(star.position, report.position) <= kFleetEncounterRadius) {
            packet.star = star.id;
            if (const auto* planet = find_planet_at_star(state, star.id)) packet.planet = planet->id;
            break;
        }
        player->pendingPlayerReports.push_back(std::move(packet));
    }
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
                contact.telemetryInTransit.clear();
                state.pendingFleetLossContacts.push_back({std::move(contact), deliveries[fleet.owner]});
            }
            continue;
        }
        fleet.ships = std::move(survivors);
        normalize_fleet_composition(fleet);
        const double cargoAfter = fleet_cargo_capacity(state, fleet);
        const double cargoShare = cargoBefore > epsilon ? std::min(1.0, cargoAfter / cargoBefore) : 1;
        fleet.colonists = std::uint64_t(std::floor(fleet.colonists * cargoShare));
        fleet.minerals.ironium *= cargoShare;
        fleet.minerals.boranium *= cargoShare;
        fleet.minerals.germanium *= cargoShare;
        const double fuelAfter = fleet_fuel_capacity(state, fleet);
        fleet.fuel = std::min(fuelAfter, fleet.fuel * (fuelBefore > epsilon ? fuelAfter / fuelBefore : 0));
        fleet.damagePercent = std::clamp(100 * (1 - remainingArmor / fullArmor), 0.0, 99.999999);
        // An encounter interrupts the current onboard programme; new commands
        // already in flight retain their ordinary delivery semantics.
        fleet.destination.reset();
        fleet.arrivalAction.reset();
        fleet.waypointQueue.clear();
        fleet.targetFleet = 0;
        fleet.repeatOrders = false;
        fleet.routeTemplate.clear();
        fleet.task = FleetTask::None;
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

void resolve_space_battles(GameState& state, std::uint64_t observationTurn)
{
    struct Contact { FleetId id; PlayerId owner; Position position; bool armed; };
    std::vector<Contact> contacts;
    for (const auto& f : state.fleets) contacts.push_back({f.id, f.owner, f.position, fleet_has_space_weapons(state, f)});
    std::sort(contacts.begin(), contacts.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::set<FleetId> resolved;
    for (const auto& anchor : contacts) {
        if (resolved.contains(anchor.id)) continue;
        const auto position = anchor.position;
        std::vector<FleetId> local;
        std::set<PlayerId> owners;
        bool armed = false;
        for (const auto& other : contacts) {
            if (resolved.contains(other.id) || distance_between(position, other.position) > kFleetEncounterRadius + 1e-6) continue;
            local.push_back(other.id);
            owners.insert(other.owner);
            armed |= other.armed;
        }
        if (owners.size() < 2 || !armed) continue;
        const auto report = fight(state, local, position, observationTurn);
        if (report.rounds == 0) continue;
        apply_battle(state, report);
        resolved.insert(local.begin(), local.end());
    }
}

} // namespace suns
