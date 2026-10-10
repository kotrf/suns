#include "suns/combat.hpp"
#include "suns/communications.hpp"
#include "suns/equipment.hpp"
#include "suns/campaign.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

using namespace suns;
using C = ShipComponentType;

GameState arena(std::vector<C> first, std::vector<C> second, std::uint32_t count = 1)
{
    GameState state;
    state.galaxySeed = 1234;
    state.wormholeRules.spawnChancePerTurn = 0;
    state.players = {{1, "One"}, {2, "Two"}, {3, "Observer"}};
    first.insert(first.begin(), C::QuickJump5);
    second.insert(second.begin(), C::QuickJump5);
    state.shipDesigns = {{10, 1, "One gunboat", ShipHullType::Destroyer, first},
        {20, 2, "Two gunboat", ShipHullType::Destroyer, second}};
    state.nextShipDesignId = 30;
    for (PlayerId p : {1, 2}) {
        Fleet fleet;
        fleet.id = p;
        fleet.owner = p;
        fleet.name = "Fleet " + std::to_string(p);
        fleet.design = p * 10;
        fleet.ships = {{p * 10, count}};
        fleet.fuel = fleet_fuel_capacity(state, fleet);
        fleet.telemetry.observedTurn = state.turn;
        fleet.telemetry.position = fleet.position;
        fleet.telemetry.fuel = fleet.fuel;
        fleet.telemetry.ships = fleet.ships;
        state.fleets.push_back(fleet);
    }
    state.nextFleetId = 3;
    return state;
}

const GameEvent& battle_event(const TurnResult& result, PlayerId player = 1)
{
    const auto found = std::find_if(result.events.begin(), result.events.end(), [&](const auto& e) {
        return e.kind == GameEventKind::SpaceBattle && e.recipient == player;
    });
    assert(found != result.events.end() && found->battle);
    return *found;
}

const Fleet* fleet(const GameState& state, FleetId id)
{
    const auto found = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == id; });
    return found == state.fleets.end() ? nullptr : &*found;
}

bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

void simultaneous_and_initiative()
{
    auto state = arena({C::AntiMatterPulverizer}, {C::AntiMatterPulverizer});
    const auto simultaneous = TurnProcessor{}.process_with_events(state, {});
    assert(simultaneous.state.fleets.empty());
    const auto& report = *battle_event(simultaneous).battle;
    assert(!report.stalemate && report.shots.size() == 2);
    assert(report.units[0].survivingShips == 0 && report.units[1].survivingShips == 0);
    assert(simultaneous.events.size() == 2); // Only participants, never the observer.
    state.shipDesigns.front().components.push_back(C::BattleComputer);
    const auto earlier = TurnProcessor{}.process_with_events(state, {});
    assert(fleet(earlier.state, 1) && !fleet(earlier.state, 2));
    assert(battle_event(earlier).battle->shots.size() == 1);
}

void defenses_torpedoes_and_modifiers()
{
    const auto sappers = TurnProcessor{}.process_with_events(arena({C::PulsedSapper}, {C::MoleSkinShield}), {});
    const auto& sap = *battle_event(sappers).battle;
    assert(sap.stalemate && sap.units[1].remainingShields == 0);
    assert(sap.units[1].remainingArmor == 200 && sap.units[1].survivingShips == 1);
    for (const auto& shot : sap.shots) assert(shot.armorDamage == 0);
    const auto lasers = TurnProcessor{}.process_with_events(arena({C::Laser}, {C::CompletePhaseShield}), {});
    const auto& beam = *battle_event(lasers).battle;
    assert(beam.stalemate && beam.units[1].remainingArmor == 200 && beam.units[1].remainingShields < 500);
    const auto plain = TurnProcessor{}.process_with_events(arena({C::Laser}, {}), {});
    const auto enhanced = TurnProcessor{}.process_with_events(arena({C::Laser, C::EnergyCapacitor}, {}), {});
    const auto deflected = TurnProcessor{}.process_with_events(arena({C::Laser}, {C::BeamDeflector}), {});
    assert(battle_event(enhanced).battle->shots.front().armorDamage > battle_event(plain).battle->shots.front().armorDamage);
    assert(battle_event(deflected).battle->shots.front().armorDamage < battle_event(plain).battle->shots.front().armorDamage);
    const auto torps = TurnProcessor{}.process_with_events(arena({C::AlphaTorpedo}, {C::CompletePhaseShield}, 10), {});
    const auto& first = battle_event(torps).battle->shots.front();
    assert(first.hits > 0 && first.hits <= first.fired && first.shieldDamage > 0 && first.armorDamage > 0);
    auto naked = arena({C::ArmageddonMissile}, {}, 100);
    naked.fleets.front().ships.front().count = 10;
    const auto missile = TurnProcessor{}.process_with_events(naked, {});
    const auto& hit = battle_event(missile).battle->shots.front();
    assert(near(hit.armorDamage, hit.hits * component_spec(C::ArmageddonMissile).weaponPower * 2));
    const auto computers = TurnProcessor{}.process_with_events(arena({C::AlphaTorpedo, C::BattleNexus}, {}, 100), {});
    const auto noComputers = TurnProcessor{}.process_with_events(arena({C::AlphaTorpedo}, {}, 100), {});
    const auto jammers = TurnProcessor{}.process_with_events(arena({C::AlphaTorpedo}, {C::Jammer50}, 100), {});
    assert(battle_event(computers).battle->shots.front().hits > battle_event(noComputers).battle->shots.front().hits);
    assert(battle_event(jammers).battle->shots.front().hits < battle_event(noComputers).battle->shots.front().hits);
    const auto gatling = TurnProcessor{}.process_with_events(arena({C::GatlingGun}, {}, 10), {});
    assert(!battle_event(gatling).battle->shots.empty());
}

void composition_damage_and_repair()
{
    auto state = arena({C::Laser}, {C::CompletePhaseShield});
    state.shipDesigns.front().components.push_back(C::CompletePhaseShield);
    state.shipDesigns.back().components.push_back(C::AlphaTorpedo);
    auto result = TurnProcessor{}.process_with_events(state, {});
    const auto* damaged = fleet(result.state, 1);
    assert(damaged && damaged->damagePercent > 0);
    const auto beforeDamage = damaged->damagePercent;
    result.state.fleets.erase(result.state.fleets.begin() + 1);
    result.state.shipDesigns.front().components.push_back(C::FieldRepairBay);
    const auto repaired = TurnProcessor{}.process(result.state, {});
    assert(near(repaired.fleets.front().damagePercent, std::max(0.0, beforeDamage - 8)));
    auto mixed = arena({C::MegaDisruptor}, {C::Laser}, 4);
    mixed.shipDesigns.push_back({21, 2, "Freighter", ShipHullType::SmallFreighter,
        {C::QuickJump5, C::StarsCargoPod}});
    mixed.fleets.back().ships.push_back({21, 10});
    mixed.fleets.back().colonists = 10000;
    mixed.fleets.back().minerals = {1, 2, 3};
    const auto fought = TurnProcessor{}.process_with_events(mixed, {});
    const auto& report = *battle_event(fought).battle;
    assert(report.units.size() == 3);
    for (const auto& u : report.units) assert(u.survivingShips <= u.initialShips);
    for (const auto& f : fought.state.fleets) {
        assert(colonist_cargo_mass(f.colonists) + mineral_cargo_mass(f.minerals) <= fleet_cargo_capacity(fought.state, f) + 1e-6);
        assert(f.fuel <= fleet_fuel_capacity(fought.state, f) + 1e-6);
        assert(!f.destination && !f.arrivalAction && !f.repeatOrders && f.waypointQueue.empty());
        assert(fleet_ship_count(f) > 0);
    }
    auto cargo = arena({C::Laser}, {});
    cargo.shipDesigns.back().hull = ShipHullType::SmallFreighter;
    cargo.shipDesigns.back().components.push_back(C::StarsCargoPod);
    cargo.fleets.back().ships.front().count = 20;
    cargo.fleets.back().fuel = fleet_fuel_capacity(cargo, cargo.fleets.back());
    cargo.fleets.back().minerals = {20, 40, 60};
    cargo.fleets.back().colonists = 200000;
    const auto casualties = TurnProcessor{}.process(cargo, {});
    const auto* survivor = fleet(casualties, 2);
    assert(survivor && fleet_ship_count(*survivor) > 0 && fleet_ship_count(*survivor) < 20);
    const double share = fleet_ship_count(*survivor) / 20.0;
    assert(near(survivor->minerals.ironium, 20 * share));
    assert(survivor->colonists == std::uint64_t(std::floor(200000 * share)));
    assert(near(survivor->fuel, cargo.fleets.back().fuel * share));
}

void multiple_empires_and_bounded_log()
{
    auto state = arena({C::Laser, C::CompletePhaseShield}, {C::Laser, C::CompletePhaseShield});
    state.shipDesigns.push_back({30, 3, "Third", ShipHullType::Destroyer,
        {C::QuickJump5, C::Laser, C::CompletePhaseShield}});
    auto third = state.fleets.back(); third.id = 3; third.owner = 3; third.design = 30; third.ships = {{30, 1}};
    state.fleets.push_back(third);
    const auto multi = TurnProcessor{}.process_with_events(state, {});
    assert(battle_event(multi, 3).battle->units.size() == 3);
    for (const auto& shot : battle_event(multi).battle->shots)
        assert(battle_event(multi).battle->units[shot.attacker].owner != battle_event(multi).battle->units[shot.target].owner);
    state.fleets[0].position = {-10, 0}; state.fleets[0].destination = Position{15, 0};
    state.fleets[1].position = {10, 0}; state.fleets[1].destination = Position{-15, 0};
    state.fleets[2].position = {0, -10}; state.fleets[2].destination = Position{0, 15};
    for (auto& fleet : state.fleets) fleet.warp = 5;
    const auto movingMulti = TurnProcessor{}.process_with_events(state, {});
    assert(battle_event(movingMulti, 3).battle->units.size() == 3);
    auto large = arena({}, {});
    for (PlayerId p : {1, 2}) {
        large.fleets[p - 1].ships.clear();
        for (int i = 0; i < 110; ++i) {
            const ShipDesignId id = 1000 * p + i;
            large.shipDesigns.push_back({id, p, "Shielded gun", ShipHullType::Destroyer,
                {C::QuickJump5, C::Laser, C::CompletePhaseShield}});
            large.fleets[p - 1].ships.push_back({id, 1});
        }
    }
    const auto capped = TurnProcessor{}.process_with_events(large, {});
    const auto& report = *battle_event(capped).battle;
    assert(report.logTruncated && report.shots.size() == kSpaceBattleLogLimit && report.units.size() == 220);
    for (const auto& unit : report.units) assert(unit.survivingShips <= unit.initialShips);
}

void deterministic_contacts_and_movement()
{
    auto state = arena({C::AlphaTorpedo, C::Laser}, {C::Laser}, 5);
    auto reversed = state;
    std::reverse(reversed.fleets.begin(), reversed.fleets.end());
    std::reverse(reversed.shipDesigns.begin(), reversed.shipDesigns.end());
    const auto a = TurnProcessor{}.process_with_events(state, {});
    const auto b = TurnProcessor{}.process_with_events(reversed, {});
    const auto& ar = *battle_event(a).battle;
    const auto& br = *battle_event(b).battle;
    assert(battle_event(a).id == battle_event(b).id && ar.shots.size() == br.shots.size());
    for (std::size_t i = 0; i < ar.shots.size(); ++i) {
        assert(ar.shots[i].hits == br.shots[i].hits && ar.shots[i].armorDamage == br.shots[i].armorDamage);
        assert(ar.shots[i].range <= component_spec(ar.shots[i].weapon).weaponRange + 1e-6);
    }
    auto flyby = arena({C::AntiMatterPulverizer}, {});
    flyby.fleets[0].position = {0, 0};
    flyby.fleets[0].destination = Position{25, 0};
    flyby.fleets[0].warp = 5;
    flyby.fleets[1].position = {10, 0};
    const auto encounter = TurnProcessor{}.process_with_events(flyby, {});
    assert(fleet(encounter.state, 1) && !fleet(encounter.state, 2));
    assert(distance_between(fleet(encounter.state, 1)->position, {10, 0}) <= kFleetEncounterRadius);
    assert(fleet(encounter.state, 1)->fuel < flyby.fleets[0].fuel);
    auto miss = flyby;
    miss.fleets[1].position = {10, -25};
    miss.fleets[1].destination = Position{10, 0};
    miss.fleets[1].warp = 5;
    const auto missed = TurnProcessor{}.process_with_events(miss, {});
    assert(std::none_of(missed.events.begin(), missed.events.end(), [](const auto& e) { return e.kind == GameEventKind::SpaceBattle; }));
    auto friendly = state;
    friendly.fleets.back().owner = 1;
    assert(TurnProcessor{}.process(friendly, {}).fleets.size() == 2);
    assert(TurnProcessor{}.process(arena({}, {}), {}).fleets.size() == 2);
    auto enormous = arena({C::PulsedSapper}, {C::CompletePhaseShield}, std::numeric_limits<std::uint32_t>::max());
    const auto large = TurnProcessor{}.process_with_events(enormous, {});
    assert(large.state.fleets.size() == 2 && battle_event(large).battle->shots.size() <= kSpaceBattleLogLimit);
}

void delayed_reports_and_knowledge()
{
    auto state = arena({C::AntiMatterPulverizer}, {});
    state.stars = {{1, "Home One", {0, 0}}, {2, "Home Two", {0, 10}}};
    state.planets = {{1, 1, "One", 100, 1, 1000000}, {2, 2, "Two", 100, 2, 1000000}};
    for (auto& f : state.fleets) {
        f.position = {600, 0};
        f.telemetry.position = f.position;
    }
    const auto result = TurnProcessor{}.process_with_events(state, {});
    assert(!fleet(result.state, 2) && !result.state.pendingFleetLossContacts.empty());
    assert(std::none_of(result.events.begin(), result.events.end(), [](const auto& e) { return e.kind == GameEventKind::SpaceBattle; }));
    const auto contacts = missing_fleet_contacts(result.state, 2);
    assert(contacts.size() == 1 && contacts.front().id == 2 && fleet_ship_count(contacts.front()) == 1);
    assert(result.state.players[1].history.back().ships == 1);
    const auto exported = make_player_view(result.state, 2).state;
    assert(exported.fleets.size() == 1 && exported.fleets.front().id == 2);
    assert(exported.pendingFleetLossContacts.empty() && exported.players.front().pendingPlayerReports.empty());
    auto next = result;
    bool received = false;
    for (int i = 0; i < 10 && !received; ++i) {
        next = TurnProcessor{}.process_with_events(next.state, {});
        received = std::any_of(next.events.begin(), next.events.end(), [](const auto& e) {
            return e.recipient == 2 && e.kind == GameEventKind::SpaceBattle;
        });
    }
    assert(received && missing_fleet_contacts(next.state, 2).empty());
    assert(battle_event(next, 2).battle->units[1].survivingShips == 0);
}

void founding_cannot_evade_combat()
{
    auto state = arena({C::AntiMatterPulverizer}, {});
    state.shipDesigns.back().hull = ShipHullType::ColonyShip;
    state.shipDesigns.back().components.push_back(C::StarsColonizationModule);
    state.fleets.back().colonists = 10000;
    state.stars = {{1, "Neutral", {0, 0}}};
    state.planets = {{1, 1, "Neutral", 100, 0, 0}};
    const auto manual = TurnProcessor{}.process_with_events(state, {{2, {ColonizePlanetOrder{2, 1}}}});
    assert(manual.state.planets.front().owner == 0 && !fleet(manual.state, 2));
    auto arrival = state;
    arrival.fleets.back().position = {-10, 0};
    arrival.fleets.back().destination = Position{0, 0};
    arrival.fleets.back().arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Colonize};
    arrival.fleets.back().warp = 5;
    const auto automatic = TurnProcessor{}.process_with_events(arrival, {});
    assert(automatic.state.planets.front().owner == 0 && !fleet(automatic.state, 2));
}

int main()
{
    simultaneous_and_initiative();
    defenses_torpedoes_and_modifiers();
    composition_damage_and_repair();
    multiple_empires_and_bounded_log();
    deterministic_contacts_and_movement();
    delayed_reports_and_knowledge();
    founding_cannot_evade_combat();
}
