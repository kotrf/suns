#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

using namespace suns;

const Fleet* find_fleet(const GameState& state, FleetId id)
{
    for (const auto& fleet : state.fleets) if (fleet.id == id) return &fleet;
    return nullptr;
}

GameState invasion_fixture(std::uint64_t turn = 1)
{
    auto state = make_demo_game();
    state.turn = turn;
    state.players.push_back({2, "Defenders"});
    auto& planet = state.planets[1];
    planet.owner = 2;
    planet.population = 1'000;
    planet.productionQueue.push_back({ProductionKind::Factory, 3, 0});

    auto& fleet = state.fleets.front();
    fleet.owner = 1;
    fleet.design = kColonyShipDesignId;
    fleet.ships = {{kColonyShipDesignId, 1}};
    fleet.position = state.stars[1].position;
    fleet.colonists = 1'250;
    fleet.minerals = {3.0, 2.0, 1.0};
    return state;
}

TurnResult invade(GameState state, std::uint64_t colonists = 1'250, MineralCargo minerals = {})
{
    return TurnProcessor{}.process_with_events(state, {{1, {TransferCargoOrder{
        {0, state.fleets.front().id},
        {state.planets[1].id, 0},
        colonists,
        minerals,
    }}}});
}

void chance_is_symmetric_and_rewards_numerical_advantage()
{
    assert(ground_invasion_success_chance(0, 100) == 0.0);
    assert(ground_invasion_success_chance(100, 0) == 1.0);
    assert(std::abs(ground_invasion_success_chance(100, 100) - 0.5) < 0.000001);
    assert(ground_invasion_success_chance(200, 100) > 0.79);
    assert(ground_invasion_success_chance(100, 200) < 0.21);
}

void outcomes_are_probabilistic_but_replay_deterministic()
{
    bool foundVictory = false;
    bool foundDefeat = false;
    for (std::uint64_t turn = 1; turn <= 256 && (!foundVictory || !foundDefeat); ++turn) {
        const auto state = invasion_fixture(turn);
        const auto first = invade(state);
        const auto replay = invade(state);
        const bool victory = first.state.planets[1].owner == 1;
        assert(replay.state.planets[1].owner == first.state.planets[1].owner);
        assert(replay.state.planets[1].population == first.state.planets[1].population);
        assert(find_fleet(replay.state, 1)->colonists == find_fleet(first.state, 1)->colonists);
        foundVictory = foundVictory || victory;
        foundDefeat = foundDefeat || !victory;
    }
    assert(foundVictory && foundDefeat);
}

void invasion_consumes_only_the_landing_force_and_reports_both_sides()
{
    auto state = invasion_fixture();
    state.fleets.front().colonists = 2'000;
    const auto result = invade(state, 1'250);
    const auto* fleet = find_fleet(result.state, 1);
    assert(fleet && fleet->colonists == 750);

    const bool victory = result.state.planets[1].owner == 1;
    assert(result.state.planets[1].population > 0);
    if (victory) {
        // No implicit radar or production programme after conquest.
        assert(result.state.planets[1].radar == PlanetaryRadarLevel::None);
        assert(result.state.planets[1].productionQueue.empty());
    }
    else assert(result.state.planets[1].owner == 2);

    const auto attackerKind = victory
        ? PlayerReportKind::GroundInvasionWon
        : PlayerReportKind::GroundInvasionLost;
    const auto defenderKind = victory
        ? PlayerReportKind::ColonyLost
        : PlayerReportKind::GroundDefenseWon;
    const auto hasPending = [&](PlayerId player, PlayerReportKind kind) {
        const auto& reports = result.state.players[player - 1].pendingPlayerReports;
        return std::any_of(reports.begin(), reports.end(),
            [&](const PendingPlayerReport& report) { return report.kind == kind; });
    };
    const auto hasDelivered = [&](PlayerId player, GameEventKind kind) {
        return std::any_of(result.events.begin(), result.events.end(), [&](const GameEvent& event) {
            return event.recipient == player && event.kind == kind;
        });
    };
    assert(hasPending(1, attackerKind) || hasDelivered(1, victory
        ? GameEventKind::GroundInvasionWon
        : GameEventKind::GroundInvasionLost));
    assert(hasPending(2, defenderKind) || hasDelivered(2, victory
        ? GameEventKind::ColonyLost
        : GameEventKind::GroundDefenseWon));
}

void minerals_cannot_be_hidden_inside_an_invasion_order()
{
    const auto state = invasion_fixture();
    const auto rejected = invade(state, 100, {1.0, 0.0, 0.0});
    assert(rejected.state.planets[1].owner == 2);
    assert(find_fleet(rejected.state, 1)->colonists == state.fleets.front().colonists);
    assert(find_fleet(rejected.state, 1)->minerals.ironium == 3.0);
}

void unload_all_colonists_uses_the_same_ground_combat_rule()
{
    auto state = invasion_fixture();
    auto& fleet = state.fleets.front();
    fleet.destination = fleet.position;
    fleet.arrivalAction = FleetArrivalAction{
        FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Colonists};

    const auto result = TurnProcessor{}.process(state, {});
    const auto* after = find_fleet(result, fleet.id);
    assert(after && after->colonists == 0);
    assert(result.planets[1].owner == 1 || result.planets[1].owner == 2);
    assert(result.planets[1].population > 0);
}

void conquest_does_not_apply_a_new_colony_default()
{
    bool captured=false;
    for (std::uint64_t turn=1; turn<=256 && !captured; ++turn) {
        auto state=invasion_fixture(turn);
        ProductionTemplate plan{"Settlement bootstrap",{
            {ProductionKind::LocalRadar,production_cost(ProductionKind::LocalRadar),0,ProductionAutomation::Annual,1},
            {ProductionKind::Factory,kFactoryCost,0,ProductionAutomation::Annual,2}},true};
        assert(production_template_valid(plan));
        state.players[0].productionTemplates={plan};
        const auto result=invade(state);
        if (result.state.planets[1].owner!=1) continue;
        captured=true;
        assert(result.state.planets[1].productionQueue.empty());
        assert(result.state.planets[1].radar==PlanetaryRadarLevel::None);
        assert(result.state.players[0].productionTemplates[0].defaultForNewColonies);
        auto& fleet=state.fleets.front(); fleet.destination=fleet.position;
        fleet.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::UnloadAll,1,FleetCargoKind::Colonists};
        const auto automated=TurnProcessor{}.process(state,{});
        assert(automated.planets[1].owner==1 && automated.planets[1].productionQueue.empty());
        assert(automated.planets[1].radar==PlanetaryRadarLevel::None);
    }
    assert(captured);
}

GameState station_blockade_fixture()
{
    auto state = invasion_fixture();
    state.wormholeRules.spawnChancePerTurn = 0;
    for (auto& player : state.players) player.technology.researchActive = false;
    state.planets[1].population = 1;
    auto station = state.orbitalStations.front();
    station.id = state.nextOrbitalStationId++;
    station.planet = state.planets[1].id;
    station.owner = 2;
    station.components.clear();
    state.orbitalStations.push_back(station);
    return state;
}

void assert_blocked_report(const TurnResult& result)
{
    const auto& pending = result.state.players[0].pendingPlayerReports;
    const auto pendingCount = std::count_if(pending.begin(), pending.end(), [](const auto& report) {
        return report.kind == PlayerReportKind::LandingBlocked && report.fleet == 1;
    });
    const auto deliveredCount = std::count_if(result.events.begin(), result.events.end(), [](const auto& event) {
        return event.kind == GameEventKind::LandingBlocked && event.fleet == 1 && event.recipient == 1;
    });
    assert(pendingCount + deliveredCount == 1);
    assert(std::none_of(result.events.begin(), result.events.end(), [](const auto& event) {
        return event.kind == GameEventKind::GroundInvasionWon || event.kind == GameEventKind::GroundInvasionLost
            || (event.kind == GameEventKind::LandingBlocked && event.recipient != 1);
    }));
    for (const auto& player : result.state.players) {
        assert(std::none_of(player.pendingPlayerReports.begin(), player.pendingPlayerReports.end(), [&](const auto& report) {
            return report.kind == PlayerReportKind::GroundInvasionWon || report.kind == PlayerReportKind::GroundInvasionLost
                || (report.kind == PlayerReportKind::LandingBlocked && player.id != 1);
        }));
    }
}

void unarmed_station_blocks_manual_and_waypoint_landings()
{
    for (const auto cargo : {FleetCargoKind::Colonists, FleetCargoKind::All}) {
        auto state = station_blockade_fixture();
        const auto initialCargo = state.fleets.front().colonists;
        auto result = invade(state);
        assert(result.state.planets[1].owner == 2);
        assert(find_fleet(result.state, 1)->colonists == initialCargo);
        assert(find_orbital_station_at_planet(result.state, state.planets[1].id)->owner == 2);
        assert_blocked_report(result);

        auto& fleet = state.fleets.front();
        fleet.destination = fleet.position;
        fleet.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::UnloadAll, 1, cargo};
        result = TurnProcessor{}.process_with_events(state, {});
        assert(result.state.planets[1].owner == 2);
        const auto* after = find_fleet(result.state, 1);
        assert(after && after->colonists == initialCargo);
        assert(after->minerals.ironium == fleet.minerals.ironium);
        assert(after->minerals.boranium == fleet.minerals.boranium);
        assert(after->minerals.germanium == fleet.minerals.germanium);
        assert_blocked_report(result);
    }
}

void blocked_landing_reports_wait_for_transmission_and_replay_stably()
{
    auto state = station_blockade_fixture();
    state.fleets[0].electronics = {EmissionMode::RadioSilence, state.turn + 20};
    const auto blocked = invade(state);
    assert_blocked_report(blocked);
    assert(std::none_of(blocked.events.begin(), blocked.events.end(), [](const auto& event) {
        return event.kind == GameEventKind::LandingBlocked;
    }));
    const auto& pending = blocked.state.players[0].pendingPlayerReports;
    const auto report = std::find_if(pending.begin(), pending.end(), [](const auto& report) {
        return report.kind == PlayerReportKind::LandingBlocked;
    });
    assert(report != pending.end() && report->deliveryTurn == kAwaitingTransmission);
    assert(report->transmissionFleet == 1 && report->observedTurn == state.turn + 1);
    auto due = blocked.state;
    // Advance the immutable packet to its receipt boundary without changing
    // the report's observation, even if the station has since disappeared.
    due.orbitalStations.clear();
    auto& packets = due.players[0].pendingPlayerReports;
    for (auto& packet : packets) packet.deliveryTurn = due.turn + 3;
    assert(deliver_due_player_reports(due).empty());
    due.turn += 3;
    auto replay = due;
    const auto delivered = deliver_due_player_reports(due);
    const auto repeated = deliver_due_player_reports(replay);
    const auto first = std::find_if(delivered.begin(), delivered.end(), [](const auto& event) {
        return event.kind == GameEventKind::LandingBlocked;
    });
    const auto second = std::find_if(repeated.begin(), repeated.end(), [](const auto& event) {
        return event.kind == GameEventKind::LandingBlocked;
    });
    assert(first != delivered.end() && second != repeated.end());
    assert(first->id == second->id && first->observedTurn == state.turn + 1);
    assert(first->turn == due.turn && first->severity == GameEventSeverity::Warning);
}

void station_must_be_destroyed_before_landing()
{
    auto state = station_blockade_fixture();
    auto& transport = state.fleets.front();
    transport.destination = transport.position;
    transport.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Colonists};
    state.shipDesigns.push_back({99, 1, "Orbital assault", ShipHullType::Destroyer,
        {ShipComponentType::FuelMizer, ShipComponentType::AntiMatterPulverizer}});
    Fleet escort;
    escort.id = state.nextFleetId++;
    escort.owner = 1;
    escort.design = 99;
    escort.ships = {{99, 100}};
    escort.position = transport.position;
    escort.fuel = fleet_fuel_capacity(state, escort);
    state.fleets.push_back(escort);
    const auto result = TurnProcessor{}.process_with_events(state, {});
    assert(!find_orbital_station_at_planet(result.state, state.planets[1].id));
    // Combat intentionally clears participant routes. The surviving transport
    // commits its landing force only after a new order against the cleared orbit.
    assert(result.state.planets[1].owner == 2);
    assert(find_fleet(result.state, 1)->colonists == 1'250);
    const auto captured = invade(result.state);
    assert(captured.state.planets[1].owner == 1);
    assert(find_fleet(captured.state, 1)->colonists == 0);
    assert(std::any_of(captured.state.players[0].pendingPlayerReports.begin(),
        captured.state.players[0].pendingPlayerReports.end(), [](const auto& report) {
            return report.kind == PlayerReportKind::GroundInvasionWon;
        }) || std::any_of(captured.events.begin(), captured.events.end(), [](const auto& event) {
            return event.kind == GameEventKind::GroundInvasionWon && event.recipient == 1;
        }));
}

void hostile_station_also_blocks_founding_on_an_empty_world()
{
    for (const bool waypoint : {false, true}) {
        auto state = station_blockade_fixture();
        state.planets[1].owner = 0;
        state.planets[1].population = 0;
        auto& fleet = state.fleets.front();
        fleet.colonists = 100;
        fleet.minerals = {};
        if (waypoint) {
            fleet.destination = fleet.position;
            fleet.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Colonize};
        }
        const auto orders = waypoint ? std::vector<PlayerOrders>{}
            : std::vector<PlayerOrders>{{1, {ColonizePlanetOrder{fleet.id, state.planets[1].id}}}};
        const auto result = TurnProcessor{}.process_with_events(state, orders);
        assert(result.state.planets[1].owner == 0);
        assert(find_fleet(result.state, 1) && find_fleet(result.state, 1)->colonists == fleet.colonists);
        assert(find_orbital_station_at_planet(result.state, state.planets[1].id)->owner == 2);
        assert_blocked_report(result);
        state.orbitalStations.pop_back();
        const auto cleared = TurnProcessor{}.process(state, orders);
        assert(cleared.planets[1].owner == 1);
    }
}

} // namespace

int main()
{
    unarmed_station_blocks_manual_and_waypoint_landings();
    station_must_be_destroyed_before_landing();
    hostile_station_also_blocks_founding_on_an_empty_world();
    blocked_landing_reports_wait_for_transmission_and_replay_stably();
    conquest_does_not_apply_a_new_colony_default();
    chance_is_symmetric_and_rewards_numerical_advantage();
    outcomes_are_probabilistic_but_replay_deterministic();
    invasion_consumes_only_the_landing_force_and_reports_both_sides();
    minerals_cannot_be_hidden_inside_an_invasion_order();
    unload_all_colonists_uses_the_same_ground_combat_rule();
}
