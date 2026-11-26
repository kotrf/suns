#include "suns/combat.hpp"
#include "suns/communications.hpp"
#include "suns/deep_space.hpp"
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
    for (const auto& player : simultaneous.state.players) {
        const auto& history = player.history.back();
        assert(history.battleStatisticsRecorded);
        assert(history.battles.reports == (player.id == 3 ? 0 : 1));
        assert(history.battles.ownShipsLost == (player.id == 3 ? 0 : 1));
        assert(history.battles.opposingShipsLost == (player.id == 3 ? 0 : 1));
    }
    state.shipDesigns.front().components.push_back(C::BattleComputer);
    const auto earlier = TurnProcessor{}.process_with_events(state, {});
    assert(fleet(earlier.state, 1) && !fleet(earlier.state, 2));
    assert(battle_event(earlier).battle->shots.size() == 1);
}

void defenses_torpedoes_and_modifiers()
{
    // AUD-005: a long-range sapper cannot hold the armor weapon outside range
    // when the selected target has no shields, including after stripping them.
    for (const auto shields : {std::vector<C>{}, std::vector<C>{C::MoleSkinShield}}) {
        const auto result = TurnProcessor{}.process_with_events(arena({C::PulsedSapper, C::Laser}, shields), {});
        const auto& battle = *battle_event(result).battle;
        assert(std::any_of(battle.shots.begin(), battle.shots.end(), [](const auto& shot) {
            return shot.weapon == C::Laser && shot.armorDamage > 0;
        }));
        assert(battle.units[1].remainingArmor < 200);
        if (!shields.empty()) assert(std::any_of(battle.shots.begin(), battle.shots.end(), [](const auto& shot) {
            return shot.weapon == C::PulsedSapper && shot.shieldDamage > 0;
        }));
    }
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
    const auto jammed = TurnProcessor{}.process_with_events(arena({C::AlphaTorpedo}, {C::Jammer50}, 100), {});
    assert(std::none_of(jammed.events.begin(), jammed.events.end(), [](const auto& e) {
        return e.recipient == 1 && e.kind == GameEventKind::SpaceBattle;
    })); // Strategic interference delays the original combat report by one year.
    const auto jammers = TurnProcessor{}.process_with_events(jammed.state, {});
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
    auto alreadySent=state.fleets[1].telemetry;
    alreadySent.fuel=123; alreadySent.damagePercent=7;
    state.fleets[1].telemetryInTransit.push_back({2,alreadySent});
    const auto result = TurnProcessor{}.process_with_events(state, {});
    assert(!fleet(result.state, 2) && !result.state.pendingFleetLossContacts.empty());
    assert(std::none_of(result.events.begin(), result.events.end(), [](const auto& e) { return e.kind == GameEventKind::SpaceBattle; }));
    const auto contacts = missing_fleet_contacts(result.state, 2);
    assert(contacts.size() == 1 && contacts.front().id == 2 && fleet_ship_count(contacts.front()) == 1);
    assert(contacts.front().fuel==123 && contacts.front().damagePercent==7);
    assert(result.state.players[1].history.back().ships == 1);
    for (const auto& player : result.state.players) {
        assert(player.history.back().battleStatisticsRecorded);
        assert(player.history.back().battles.reports == 0);
        assert(player.history.back().battles.ownShipsLost == 0);
        assert(player.history.back().battles.opposingShipsLost == 0);
    }
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
    const auto& receipt = next.state.players[1].history.back();
    assert(receipt.turn > battle_event(next, 2).observedTurn);
    assert(receipt.battles.reports == 1 && receipt.battles.ownShipsLost == 1
        && receipt.battles.opposingShipsLost == 0);
    const auto packet = make_player_view(next.state, 2).state;
    assert(packet.players.size() == 1 && packet.players.front().history.back().battles.ownShipsLost == 1);
    auto refreshed = next.state;
    record_empire_turn_statistics(refreshed);
    record_empire_turn_statistics(refreshed, {}, true);
    assert(refreshed.players[1].history.size() == next.state.players[1].history.size());
    assert(refreshed.players[1].history.back().battles.reports == 1);
    assert(refreshed.players[1].history.back().battles.ownShipsLost == 1);
    const auto following = TurnProcessor{}.process(refreshed, {});
    assert(following.players[1].history.back().battleStatisticsRecorded);
    assert(following.players[1].history.back().battles.reports == 0);
    assert(following.players[1].history.back().battles.ownShipsLost == 0);
}

void yearly_battle_totals()
{
    auto state = arena({C::AntiMatterPulverizer}, {C::AntiMatterPulverizer}, 3);
    for (int i = 0; i < 2; ++i) {
        auto another = state.fleets[i];
        another.id += 2;
        another.position = {100, 0};
        another.telemetry.position = another.position;
        state.fleets.push_back(another);
    }
    state.nextFleetId = 5;
    record_empire_turn_statistics(state);
    assert(!state.players.front().history.front().battleStatisticsRecorded);
    const auto result = TurnProcessor{}.process_with_events(state, {});
    const auto replay = TurnProcessor{}.process_with_events(state, {});
    for (const auto& player : result.state.players) {
        const auto& totals = player.history.back().battles;
        const auto& again = find_player(replay.state, player.id)->history.back().battles;
        assert(totals.reports == (player.id == 3 ? 0 : 2));
        std::uint64_t own = 0, opposing = 0;
        for (const auto& event : result.events) {
            if (event.recipient != player.id || !event.battle) continue;
            for (const auto& unit : event.battle->units) {
                const auto lost = std::uint64_t(unit.initialShips) - unit.survivingShips;
                if (unit.owner == player.id) own += lost;
                else opposing += lost;
            }
        }
        assert(totals.ownShipsLost == own && totals.opposingShipsLost == opposing);
        assert(totals.reports == again.reports && totals.ownShipsLost == again.ownShipsLost
            && totals.opposingShipsLost == again.opposingShipsLost);
    }
    // Multi-owner battles count each opposing stack once, even without kill attribution.
    auto three = arena({C::AntiMatterPulverizer}, {C::AntiMatterPulverizer});
    auto third = three.fleets.back();
    third.id = 3; third.owner = 3; third.design = 30; third.ships = {{30, 1}};
    three.shipDesigns.push_back({30, 3, "Third gunboat", ShipHullType::Destroyer,
        {C::QuickJump5, C::AntiMatterPulverizer}});
    three.nextShipDesignId = 31;
    three.fleets.push_back(third);
    three.nextFleetId = 4;
    const auto multi = TurnProcessor{}.process_with_events(three, {});
    for (const auto& player : multi.state.players) {
        assert(player.history.back().battles.reports == 1);
        std::uint64_t ownSurvivors = 0, opposingSurvivors = 0;
        for (const auto& survivor : multi.state.fleets) {
            if (survivor.owner == player.id) ownSurvivors += fleet_ship_count(survivor);
            else opposingSurvivors += fleet_ship_count(survivor);
        }
        assert(player.history.back().battles.ownShipsLost == 1 - ownSurvivors);
        assert(player.history.back().battles.opposingShipsLost == 2 - opposingSurvivors);
    }
}

void overdue_reports_keep_receipt_year()
{
    auto state = arena({}, {});
    record_empire_turn_statistics(state);
    // The live ships/designs need not exist when a historical packet arrives.
    state.fleets.clear();
    PendingPlayerReport packet;
    packet.kind = PlayerReportKind::SpaceBattle;
    packet.observedTurn = 1; packet.deliveryTurn = 1; packet.fleet = 42;
    SpaceBattleReport report;
    report.observedTurn = 1; report.rounds = 1; report.logTruncated = true;
    const auto many = std::numeric_limits<std::uint32_t>::max();
    report.units = {{1, 42, 10, "Lost", "One", many, 0},
        {1, 42, 11, "Lost", "Two", many, 0}, {2, 43, 20, "Other", "Three", 5, 2}};
    packet.battle = report;
    state.players.front().pendingPlayerReports = {packet, packet}; // Same observation ID.
    const auto result = TurnProcessor{}.process_with_events(state, {});
    const auto& first = result.state.players.front().history.front();
    assert(first.turn == battle_event(result).turn && first.turn == 1);
    assert(first.battleStatisticsRecorded && first.battles.reports == 1);
    assert(first.battles.ownShipsLost == std::uint64_t(many) * 2);
    assert(first.battles.opposingShipsLost == 3);
    assert(result.state.players.front().history.back().battles.reports == 0);
    assert(result.state.players.back().history.front().battles.reports == 0);
    auto repeated = result.state;
    repeated.players.front().pendingPlayerReports = {packet};
    const auto again = TurnProcessor{}.process(repeated, {});
    assert(again.players.front().history[0].battles.reports == 1);
    assert(again.players.front().history[1].battles.reports == 0);
    assert(again.players.front().history[2].battles.reports == 0);

    auto unmeasured = state;
    unmeasured.players.front().history.front().milestones.push_back(
        {123, 1, HistoryMilestoneKind::SpaceBattle, 0, ResearchField::Electronics, 0, 42});
    record_empire_turn_statistics(unmeasured, {}, true);
    assert(!unmeasured.players.front().history.front().battleStatisticsRecorded);
    const auto unknown = TurnProcessor{}.process(unmeasured, {});
    assert(unknown.players.front().history.front().milestones.size() == 2);
    assert(!unknown.players.front().history.front().battleStatisticsRecorded);
    assert(unknown.players.front().history.front().battles.reports == 0);
    assert(unknown.players.front().history.back().battleStatisticsRecorded);
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

void attack_policy_controls_route_contact()
{
    auto state = arena({C::Laser, C::CompletePhaseShield}, {C::Laser, C::CompletePhaseShield});
    state.fleets[0].position = {0, 0};
    state.fleets[0].destination = Position{25, 0};
    state.fleets[0].warp = 5;
    state.fleets[1].position = {10, 0};
    for (auto& fleet : state.fleets) fleet.battlePlan.attackOnContact = false;
    const auto passing = TurnProcessor{}.process_with_events(state, {});
    assert(passing.state.fleets.size() == 2 && same_position(fleet(passing.state, 1)->position, {25, 0}));
    assert(std::none_of(passing.events.begin(), passing.events.end(), [](const auto& e) { return e.battle.has_value(); }));

    // An immediate plan order takes effect before movement. Return-fire
    // fleets defend themselves without contributing to battle initiation.
    const auto attacked = TurnProcessor{}.process_with_events(state, {{1, {SetFleetBattlePlanOrder{1, {}}}}});
    const auto& battle = *battle_event(attacked).battle;
    assert(same_position(battle.position, {10, 0}));
    assert(battle.units[0].initiated && !battle.units[1].initiated);
    assert(std::any_of(battle.shots.begin(), battle.shots.end(), [](const auto& shot) { return shot.attacker == 1; }));

    state.shipDesigns[1].components = {C::QuickJump5};
    auto armedOnly = FleetBattlePlan{true, BattleTarget::ArmedShips, false};
    const auto filtered = TurnProcessor{}.process_with_events(state, {{1, {SetFleetBattlePlanOrder{1, armedOnly}}}});
    assert(same_position(fleet(filtered.state, 1)->position, {25, 0}));
    assert(std::none_of(filtered.events.begin(), filtered.events.end(), [](const auto& e) { return e.battle.has_value(); }));
    const auto againstUnarmed = TurnProcessor{}.process_with_events(state, {{1, {SetFleetBattlePlanOrder{1, {}}}}});
    assert(battle_event(againstUnarmed).battle->units[0].initiated);
}

void battle_precedes_simultaneous_merge()
{
    auto state = arena({C::AntiMatterPulverizer}, {});
    state.shipDesigns[1].hull = ShipHullType::SmallFreighter;
    state.shipDesigns.push_back({21, 2, "Scout", ShipHullType::Scout, {C::QuickJump5}});
    auto& raider = state.fleets[0];
    raider.warp = 5;
    raider.battlePlan = {true, BattleTarget::CargoShips, false};
    auto& transport = state.fleets[1];
    transport.warp = 5;
    transport.targetFleet = 3;
    transport.destination = Position{10, 0};
    transport.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::MergeWithFleet};
    auto scout = transport;
    scout.id = 3;
    scout.design = 21;
    scout.ships = {{21, 1}};
    scout.position = {10, 0};
    scout.targetFleet = 0;
    scout.destination.reset();
    scout.arrivalAction.reset();
    state.fleets.push_back(scout);
    state.nextFleetId = 4;

    const auto rendezvous = analyze_fleet_encounter({0, 0}, {10, 0}, {10, 0}, {10, 0});
    assert(rendezvous.encounterTimeFraction);
    for (const double offset : {1e-9, -1e-9}) {
        auto& attacker = state.fleets[0];
        attacker.position = {34.975 + offset, 0};
        attacker.destination = Position{9.975 + offset, 0};
        const auto attack = analyze_fleet_encounter(attacker.position, *attacker.destination, {0, 0}, {10, 0});
        assert(attack.encounterTimeFraction);
        const auto difference = *attack.encounterTimeFraction - *rendezvous.encounterTimeFraction;
        assert(std::abs(difference) < 1e-9 && (difference > 0) == (offset > 0));

        const auto result = TurnProcessor{}.process_with_events(state, {});
        const auto& report = *battle_event(result, 2).battle;
        const auto loss = std::find_if(report.units.begin(), report.units.end(),
            [](const auto& unit) { return unit.fleet == 2; });
        assert(loss != report.units.end() && loss->survivingShips == 0);
        assert(!fleet(result.state, 2));
        const auto* survivor = fleet(result.state, 3);
        assert(survivor && fleet_ship_count(*survivor) == 1);
        assert(std::none_of(result.events.begin(), result.events.end(),
            [](const auto& event) { return event.kind == GameEventKind::FleetsMerged; }));
    }

    // A separately timed encounter cannot use the transport's old path
    // after the earlier rendezvous has interrupted it.
    state.fleets[0].position = {34.980, 0};
    state.fleets[0].destination = Position{9.980, 0};
    const auto later = analyze_fleet_encounter({34.980, 0}, {9.980, 0}, {0, 0}, {10, 0});
    assert(later.encounterTimeFraction && *later.encounterTimeFraction - *rendezvous.encounterTimeFraction > 1e-9);
    const auto earlierMerge = TurnProcessor{}.process_with_events(state, {});
    assert(std::none_of(earlierMerge.events.begin(), earlierMerge.events.end(),
        [](const auto& event) { return event.kind == GameEventKind::SpaceBattle; }));
    assert(std::any_of(earlierMerge.events.begin(), earlierMerge.events.end(),
        [](const auto& event) { return event.kind == GameEventKind::FleetsMerged; }));
    const auto* joined = fleet(earlierMerge.state, 3);
    assert(joined && fleet_ship_count(*joined) == 2);
}

void preferred_and_strict_targets()
{
    auto state = arena({C::GatlingGun, C::CompletePhaseShield}, {C::Laser, C::CompletePhaseShield});
    state.shipDesigns.push_back({21, 2, "Cargo ship", ShipHullType::SmallFreighter, {C::QuickJump5}});
    state.fleets[1].ships.push_back({21, 1});
    // Keep both of the defender's designs together so the gatling first
    // reaches its preferred and fallback categories in the same round.
    state.fleets[1].battlePlan = {false, BattleTarget::Stations, false};
    state.fleets[0].battlePlan = {true, BattleTarget::CargoShips, false};
    auto observer = state.fleets[1];
    observer.id = 3;
    observer.owner = 3;
    observer.design = 30;
    observer.ships = {{30, 1}};
    observer.destination = Position{25, 0};
    observer.warp = 5;
    state.shipDesigns.push_back({30, 3, "Observer", ShipHullType::Destroyer, {C::QuickJump5, C::Laser}});
    state.fleets.push_back(observer);
    state.nextFleetId = 4;

    const auto strict = TurnProcessor{}.process_with_events(state, {});
    const auto& filtered = *battle_event(strict).battle;
    assert(filtered.units.size() == 3 && same_position(fleet(strict.state, 3)->position, {25, 0}));
    assert(std::none_of(strict.events.begin(), strict.events.end(), [](const auto& e) { return e.recipient == 3 && e.battle; }));
    assert(std::any_of(filtered.shots.begin(), filtered.shots.end(), [](const auto& shot) { return shot.attacker == 0; }));
    for (const auto& shot : filtered.shots)
        if (shot.attacker == 0) assert(filtered.units[shot.target].design == 21);

    state.fleets.pop_back();
    state.fleets[0].battlePlan.allowOtherTargets = true;
    const auto fallback = TurnProcessor{}.process_with_events(state, {});
    const auto& preferred = *battle_event(fallback).battle;
    const auto firstVolley = std::find_if(preferred.shots.begin(), preferred.shots.end(),
        [](const auto& shot) { return shot.attacker == 0; });
    assert(firstVolley != preferred.shots.end() && preferred.units[firstVolley->target].design == 21);
    assert(std::any_of(firstVolley, preferred.shots.end(), [&](const auto& shot) {
        return shot.attacker == 0 && preferred.units[shot.target].design == 20;
    }));
}

void ranged_and_escort_tactics()
{
    auto state = arena({C::MarkIVBlaster, C::CompletePhaseShield}, {C::Laser, C::CompletePhaseShield});
    const auto approach = TurnProcessor{}.process_with_events(state, {});
    state.fleets[0].battlePlan.tactic = BattleTactic::KeepDistance;
    const auto ranged = TurnProcessor{}.process_with_events(state, {});
    const auto returnShots = [](const auto& result) {
        const auto& shots = battle_event(result).battle->shots;
        return std::count_if(shots.begin(), shots.end(), [](const auto& shot) { return shot.attacker == 1; });
    };
    assert(returnShots(ranged) < returnShots(approach));
    const auto& shots = battle_event(ranged).battle->shots;
    assert(std::any_of(shots.begin(), shots.end(), [](const auto& shot) { return shot.attacker == 0 && shot.range > 1; }));

    auto convoy = arena({C::Laser}, {});
    convoy.shipDesigns.push_back({11, 1, "Cargo ship", ShipHullType::SmallFreighter, {C::QuickJump5}});
    convoy.fleets[0].ships.push_back({11, 1});
    convoy.fleets[0].battlePlan.tactic = BattleTactic::Escort;
    const auto escort = TurnProcessor{}.process_with_events(convoy, {});
    assert(battle_event(escort).battle->shots.empty());
    convoy.fleets[0].ships.pop_back();
    const auto withoutCargo = TurnProcessor{}.process_with_events(convoy, {});
    assert(!battle_event(withoutCargo).battle->shots.empty());
}

void sapper_closes_on_an_affectable_target()
{
    auto state = arena({C::PulsedSapper}, {C::Laser, C::MoleSkinShield});
    state.shipDesigns.push_back({21, 2, "Unshielded cargo ship", ShipHullType::SmallFreighter, {C::QuickJump5}});
    state.fleets[1].ships.push_back({21, 1});
    state.fleets[1].battlePlan = {false, BattleTarget::Stations, false}; // Hold both targets in place.
    state.fleets[0].battlePlan = {true, BattleTarget::CargoShips, true};
    const auto fallback = TurnProcessor{}.process_with_events(state, {});
    const auto& report = *battle_event(fallback).battle;
    assert(std::any_of(report.shots.begin(), report.shots.end(), [&](const auto& shot) {
        return report.units[shot.target].design == 20 && shot.shieldDamage > 0;
    }));
    state.fleets[0].battlePlan.allowOtherTargets = false;
    const auto strict = TurnProcessor{}.process_with_events(state, {});
    assert(battle_event(strict).battle->shots.empty());
}

void separate_escort_joins_moving_cargo()
{
    auto state = arena({C::Laser}, {});
    state.fleets[0].position = {10, 0};
    state.fleets[0].battlePlan = {true, BattleTarget::CargoShips, false};
    state.fleets[1].destination = Position{25, 0};
    state.fleets[1].warp = 5;
    state.shipDesigns[1].hull = ShipHullType::SmallFreighter;
    state.shipDesigns.push_back({21, 2, "Escort", ShipHullType::Destroyer, {C::QuickJump5, C::Laser}});
    auto escort = state.fleets[1];
    escort.id = 3;
    escort.design = 21;
    escort.ships = {{21, 1}};
    escort.position = {0, 0.008};
    escort.destination = Position{25, 0.008};
    escort.battlePlan = {false, BattleTarget::ArmedShips, false, BattleTactic::Escort};
    state.fleets.push_back(escort);
    state.nextFleetId = 4;
    const auto verify = [](const GameState& start, bool escortInitiated) {
        const auto result = TurnProcessor{}.process_with_events(start, {});
        const auto& report = *battle_event(result, 2).battle;
        const auto guard = std::find_if(report.units.begin(), report.units.end(), [](const auto& u) { return u.fleet == 3; });
        assert(guard != report.units.end() && guard->initiated == escortInitiated);
        assert(std::any_of(report.shots.begin(), report.shots.end(), [&](const auto& shot) {
            return report.units[shot.attacker].fleet == 3;
        }));
    };
    for (const auto attackOnContact : {false, true}) {
        state.fleets.back().battlePlan.attackOnContact = attackOnContact;
        // Parallel tracks 0.008 ly apart: the raider meets the cargo fleet
        // first under either ID ordering. The escort is close to its cargo
        // at that moment, although its own opposing contact comes later.
        verify(state, false);
        std::swap(state.fleets[0].id, state.fleets[1].id);
        verify(state, false);
        std::swap(state.fleets[0].id, state.fleets[1].id);
    }

    // A genuine simultaneous attack by an escort still counts as initiation.
    state.fleets.back().position = state.fleets[1].position;
    state.fleets.back().destination = state.fleets[1].destination;
    verify(state, true);
}

void anchored_escort_stays_in_place()
{
    auto state = arena({C::Laser}, {});
    const Position anchor{kOutpostStellarClearanceLy, 0};
    state.stars = {{1, "Clearance boundary", {0, 0}}};
    state.fleets[0].position = {anchor.x - 0.014, 0};
    state.fleets[0].battlePlan = {true, BattleTarget::CargoShips, false};
    state.shipDesigns[1].hull = ShipHullType::SmallFreighter;
    state.fleets[1].position = {anchor.x - 0.005, 0};
    state.fleets[1].destination = Position{state.fleets[1].position.x, 25};
    state.fleets[1].warp = 5;
    state.shipDesigns.push_back({21, 2, "Armed depot", ShipHullType::Scout,
        {C::QuickJump5, C::FuelDepotKit, C::Laser}});
    auto depot = state.fleets[1];
    depot.id = 3;
    depot.design = 21;
    depot.ships = {{21, 1}};
    depot.position = anchor;
    depot.destination.reset();
    depot.fuel = 100;
    depot.outpost = OutpostKind::FuelDepot;
    depot.battlePlan = {false, BattleTarget::Any, true, BattleTactic::Escort};
    state.fleets.push_back(depot);
    state.nextFleetId = 4;
    assert(outpost_state_valid(state, depot));
    for (int order = 0; order < 2; ++order) {
        const auto result = TurnProcessor{}.process_with_events(state, {});
        const auto& report = *battle_event(result, 2).battle;
        assert(std::any_of(report.units.begin(), report.units.end(), [](const auto& u) {
            return u.fleet == 3 && !u.initiated;
        }));
        const auto* anchored = fleet(result.state, 3);
        assert(anchored && same_position(anchored->position, anchor));
        assert(outpost_state_valid(result.state, *anchored) && anchored->lastMovedTurn == depot.lastMovedTurn);
        std::swap(state.fleets[0].id, state.fleets[1].id);
    }
}

void withdrawals_near_stations()
{
    auto state = arena({}, {});
    state.fleets.pop_back();
    state.shipDesigns[0].hull = ShipHullType::SmallFreighter;
    state.shipDesigns.push_back({11, 1, "Escort", ShipHullType::Destroyer, {C::QuickJump5, C::ManeuveringJet}});
    state.stars = {{1, "Station", {0, 0}}};
    state.planets = {{1, 1, "Station", 100, 0, 0}};
    state.orbitalStations = {{1, 2, 1, "Enemy dock", OrbitalStationHullType::OrbitalDock, {}, {C::Laser}}};
    state.nextOrbitalStationId = 2;
    auto& freighter = state.fleets.front();
    freighter.ships.push_back({11, 1});
    freighter.colonists = 500;
    freighter.minerals = {2, 3, 4};
    freighter.fuel = 100.5;
    freighter.warp = 5;
    freighter.destination = Position{25, 0};
    freighter.waypointQueue = {{{30, 0}, 5}};
    freighter.battlePlan = {false, BattleTarget::Any, true, BattleTactic::Withdraw};
    const auto escaped = TurnProcessor{}.process_with_events(state, {});
    const auto& report = *battle_event(escaped).battle;
    assert(report.units.size() == 3 && report.units.back().station == 1 && report.units.back().initiated);
    assert(!report.stalemate && !report.units[0].initiated && report.units[0].withdrawalRound > 0);
    assert(report.units[0].withdrawalRound == report.units[1].withdrawalRound);
    const auto* survivor = fleet(escaped.state, 1);
    assert(survivor && distance_between(survivor->position, {0, 0}) > kFleetEncounterRadius);
    assert(!survivor->destination && survivor->waypointQueue.empty());
    assert(survivor->colonists == freighter.colonists && near(survivor->fuel, freighter.fuel)
        && near(survivor->minerals.ironium, freighter.minerals.ironium));
    const auto following = TurnProcessor{}.process_with_events(escaped.state, {});
    assert(std::none_of(following.events.begin(), following.events.end(), [](const auto& e) { return e.battle.has_value(); }));

    freighter.damagePercent = 100;
    const auto caught = TurnProcessor{}.process_with_events(state, {});
    const auto& loss = *battle_event(caught).battle;
    assert(!fleet(caught.state, 1) && loss.units[0].attemptedWithdrawal && !loss.units[0].withdrawalRound);

    // Station targeting and initiation are independent of landing rights.
    freighter.damagePercent = 0;
    state.orbitalStations.front().components.clear();
    state.shipDesigns.back().components.push_back(C::Laser);
    freighter.battlePlan = {true, BattleTarget::ArmedShips, false};
    const auto passed = TurnProcessor{}.process_with_events(state, {});
    assert(same_position(fleet(passed.state, 1)->position, {25, 0}));
    freighter.battlePlan.preferredTarget = BattleTarget::Stations;
    const auto struck = TurnProcessor{}.process_with_events(state, {});
    const auto& assault = *battle_event(struck).battle;
    assert(assault.units[1].initiated && !assault.units.back().initiated);
    assert(std::any_of(assault.shots.begin(), assault.shots.end(),
        [&](const auto& shot) { return assault.units[shot.target].station == 1; }));
}

void newer_battle_plan_survives_overtaken_packet()
{
    auto state = arena({C::Laser}, {});
    state.stars = {{1, "Home", {0, 0}}};
    state.planets = {{1, 1, "Home", 100, 1, 1000000}};
    state.planets.front().homeworld = true;
    state.planets.front().radar = PlanetaryRadarLevel::Full;
    auto& guard = state.fleets.front();
    guard.position = guard.telemetry.position = {600, 0};
    const FleetBattlePlan withdrawal{false, BattleTarget::Any, true, BattleTactic::Withdraw};
    const FleetBattlePlan escort{true, BattleTarget::ArmedShips, true, BattleTactic::Escort};
    assert(!submit_battle_plan_command(state, 2, guard.id, withdrawal));
    assert(submit_battle_plan_command(state, 1, guard.id, withdrawal));
    assert(guard.battlePlan == FleetBattlePlan{} && guard.battlePlansInTransit.size() == 1);
    const auto delivery = guard.battlePlansInTransit.front().deliveryTurn;
    auto ordered = state;
    ordered.turn = delivery;
    deliver_due_fleet_commands(ordered);
    assert(ordered.fleets.front().battlePlan == withdrawal);
    assert(fleet_player_view(ordered, ordered.fleets.front()).battlePlan == FleetBattlePlan{});

    guard.position = {0, 0}; // The fleet re-enters its local communication mesh.
    assert(submit_fleet_route_command(state, 1, guard.id, guard.position, 5, {}, {}, false, 0, true));
    assert(guard.battlePlansInTransit.size() == 1);
    assert(submit_battle_plan_command(state, 1, guard.id, escort));
    state.turn = delivery;
    deliver_due_fleet_commands(state);
    assert(guard.battlePlan == escort && guard.battlePlansInTransit.empty());
}

void only_a_surviving_battle_participant_can_wake_its_radio()
{
    auto state = arena({C::Laser}, {C::CompletePhaseShield});
    auto& defender = state.fleets[1];
    defender.electronics = {EmissionMode::RadioSilence, state.turn + 20};
    defender.electronics.wakeOnContactOrCombat = true;
    const auto program = defender.electronics;
    const auto id = defender.id;
    const auto result = TurnProcessor{}.process_with_events(state, {});
    assert(fleet(result.state, id)->electronics.mode == EmissionMode::Standard);
    assert(std::any_of(result.events.begin(), result.events.end(), [id](const auto& event) {
        return event.kind == GameEventKind::RadioSilenceEnded && event.recipient == 2 && event.fleet == id && event.quantity == 1;
    }));
    state = arena({C::ArmageddonMissile}, {});
    state.fleets[1].electronics = program;
    const auto loss = TurnProcessor{}.process_with_events(state, {});
    assert(!fleet(loss.state, id));
    assert(std::none_of(loss.events.begin(), loss.events.end(), [](const auto& event) {
        return event.kind == GameEventKind::RadioSilenceEnded;
    }));
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
    yearly_battle_totals();
    overdue_reports_keep_receipt_year();
    attack_policy_controls_route_contact();
    battle_precedes_simultaneous_merge();
    preferred_and_strict_targets();
    ranged_and_escort_tactics();
    sapper_closes_on_an_affectable_target();
    separate_escort_joins_moving_cargo();
    anchored_escort_stays_in_place();
    withdrawals_near_stations();
    newer_battle_plan_survives_overtaken_packet();
    only_a_surviving_battle_participant_can_wake_its_radio();
}
