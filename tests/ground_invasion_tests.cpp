#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

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
    if (victory) assert(result.state.planets[1].productionQueue.empty());
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

} // namespace

int main()
{
    chance_is_symmetric_and_rewards_numerical_advantage();
    outcomes_are_probabilistic_but_replay_deterministic();
    invasion_consumes_only_the_landing_force_and_reports_both_sides();
    minerals_cannot_be_hidden_inside_an_invasion_order();
    unload_all_colonists_uses_the_same_ground_combat_rule();
}
