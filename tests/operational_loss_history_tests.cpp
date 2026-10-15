#include "suns/player_knowledge.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace suns;
using C = ShipComponentType;

const OperationalLossTurnStatistics& totals(const GameState& s, PlayerId id, OperationalLossKind kind, bool first = false)
{
    const auto* player = find_player(s, id); assert(player && !player->history.empty());
    return (first ? player->history.front() : player->history.back()).operationalLosses[std::size_t(kind)];
}

GameState arena(std::vector<C> fit = {})
{
    GameState s; s.players = {{1,"One"},{2,"Two"},{3,"Observer"}};
    s.wormholeRules.spawnChancePerTurn = 0;
    fit.insert(fit.begin(), C::FuelMizer);
    s.shipDesigns = {{10,1,"Test",ShipHullType::Destroyer,fit}}; s.nextShipDesignId = 11;
    Fleet f; f.id = 1; f.owner = 1; f.design = 10; f.ships = {{10,1}}; f.fuel = 1000;
    f.telemetry.observedTurn = 1; f.telemetry.ships = f.ships;
    s.fleets = {f}; s.nextFleetId = 2;
    return s;
}

void ground_casualties_are_exact_and_relative_to_recipient()
{
    bool won = false, lost = false;
    for (std::uint64_t year = 1; year <= 100 && (!won || !lost); ++year) {
        auto s = arena(); s.turn = year;
        // A capital receiver near the battle delivers the attacker report
        // immediately, even when the conquered world starts without a radar.
        s.stars = {{1,"Target",{0,0}}, {2,"One home",{50,0}}};
        Planet p; p.id = 1; p.star = 1; p.owner = 2; p.population = 1000;
        p.radar = PlanetaryRadarLevel::Full; p.homeworld = true;
        Planet home; home.id = 2; home.star = 2; home.owner = 1; home.population = 1000;
        home.radar = PlanetaryRadarLevel::Full; home.homeworld = true;
        s.planets = {p,home}; s.fleets[0].colonists = 1250;
        const auto r = TurnProcessor{}.process_with_events(s, {{1,{TransferCargoOrder{{0,1},{1,0},1250,{}}}}});
        const bool victory = r.state.planets[0].owner == 1;
        won |= victory; lost |= !victory;
        const auto& a = totals(r.state,1,OperationalLossKind::Ground);
        const auto& d = totals(r.state,2,OperationalLossKind::Ground);
        assert(a.recorded && d.recorded && a.reports == 1 && d.reports == 1);
        assert(a.ownColonistsLost == d.opposingColonistsLost);
        assert(a.opposingColonistsLost == d.ownColonistsLost);
        assert(victory ? (a.ownColonistsLost < 1250 && d.ownColonistsLost == 1000)
                       : (a.ownColonistsLost == 1250 && d.ownColonistsLost < 1000));
        assert(totals(r.state,3,OperationalLossKind::Ground).reports == 0);
        auto refreshed = r.state; record_empire_turn_statistics(refreshed);
        assert(totals(refreshed,1,OperationalLossKind::Ground).ownColonistsLost == a.ownColonistsLost);
        const auto repeat = TurnProcessor{}.process_with_events(s, {{1,{TransferCargoOrder{{0,1},{1,0},1250,{}}}}});
        assert(totals(repeat.state,1,OperationalLossKind::Ground).ownColonistsLost == a.ownColonistsLost);
    }
    assert(won && lost);
}

void bombardments_sum_even_for_identical_fleets()
{
    auto s = arena({C::LadyFingerBomb});
    s.stars = {{1,"Target",{0,0}}, {2,"Defender home",{50,0}}};
    Planet p; p.id = 1; p.star = 1; p.owner = 2; p.population = 100000; p.industry = 20; p.mines = 20;
    // The target is deliberately unfortified. A separate nearby capital
    // receiver delivers reports without absorbing any installation hits.
    Planet home; home.id = 2; home.star = 2; home.owner = 2; home.population = 1000;
    home.radar = PlanetaryRadarLevel::Full; home.homeworld = true;
    s.planets = {p, home}; s.fleets[0].task = FleetTask::Bombardment;
    auto second = s.fleets[0]; second.id = 2; s.fleets.push_back(second); s.nextFleetId = 3;
    const auto r = TurnProcessor{}.process_with_events(s,{});
    const auto& a = totals(r.state,1,OperationalLossKind::Bombardment);
    const auto& d = totals(r.state,2,OperationalLossKind::Bombardment);
    assert(a.reports == 2 && d.reports == 2 && a.recorded && d.recorded);
    assert(a.ownColonistsLost == 0 && a.opposingColonistsLost == 1196);
    assert(d.ownColonistsLost == 1196 && d.opposingColonistsLost == 0);
    assert(a.opposingInstallationsLost == 4 && d.ownInstallationsLost == 4);
    assert(totals(r.state,3,OperationalLossKind::Bombardment).reports == 0);
    // Colony destruction must not count its ColonyLost message as a ground battle.
    s.fleets.resize(1); s.planets[0].population = 100;
    const auto destroyed = TurnProcessor{}.process_with_events(s,{});
    assert(destroyed.state.planets[0].owner == 0);
    assert(totals(destroyed.state,2,OperationalLossKind::Ground).reports == 0);
    assert(totals(destroyed.state,2,OperationalLossKind::Bombardment).ownColonistsLost == 100);
}

void mines_only_count_destroyed_ships_and_wait_for_communication()
{
    auto s = arena(); s.minefields = {{1,2,{30,0},100,0}}; s.nextMinefieldId = 2;
    s.fleets[0].colonists = 100;
    const auto struck = TurnProcessor{}.process_with_events(s,{{1,{MoveFleetOrder{1,{100,0},6}}}});
    assert(struck.state.fleets.empty());
    const auto& losses = totals(struck.state,1,OperationalLossKind::Mines);
    assert(losses.reports == 1 && losses.ownShipsLost == 1 && losses.ownColonistsLost == 100);
    assert(totals(struck.state,2,OperationalLossKind::Mines).reports == 0);
    s.fleets[0].destination = Position{100,0}; s.fleets[0].warp = 6;
    s.fleets[0].electronics = {EmissionMode::RadioSilence,5};
    auto delayed = TurnProcessor{}.process_with_events(s,{}).state;
    assert(delayed.fleets.empty() && totals(delayed,1,OperationalLossKind::Mines).reports == 0);
    while (delayed.turn < 5) {
        delayed = TurnProcessor{}.process(delayed,{});
        assert(totals(delayed,1,OperationalLossKind::Mines).reports == (delayed.turn == 5 ? 1 : 0));
    }
    assert(totals(delayed,1,OperationalLossKind::Mines).ownShipsLost == 1);
    s = arena(); s.minefields = {{1,2,{30,0},100,2}}; s.nextMinefieldId = 2;
    const auto stopped = TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,{100,0},6}}}});
    assert(!stopped.fleets.empty() && totals(stopped,1,OperationalLossKind::Mines).reports == 1);
    assert(totals(stopped,1,OperationalLossKind::Mines).ownShipsLost == 0);
}

void overdue_duplicate_and_unmeasured_packets()
{
    auto s = arena(); s.stars = {{1,"Target",{0,0}}};
    PendingPlayerReport packet; packet.kind = PlayerReportKind::Bombardment;
    packet.observedTurn = 1; packet.deliveryTurn = 1; packet.planet = 1; packet.fleet = 1;
    packet.losses = OperationalLossReport{OperationalLossKind::Bombardment,0,5'000'000'000ULL,0,7};
    s.players[0].pendingPlayerReports = {packet,packet};
    auto r = TurnProcessor{}.process_with_events(s,{});
    assert(totals(r.state,1,OperationalLossKind::Bombardment,true).reports == 1);
    assert(totals(r.state,1,OperationalLossKind::Bombardment,true).opposingColonistsLost == 5'000'000'000ULL);
    assert(totals(r.state,1,OperationalLossKind::Bombardment).reports == 0);
    r.state.players[0].pendingPlayerReports = {packet};
    const auto again = TurnProcessor{}.process(r.state,{});
    assert(totals(again,1,OperationalLossKind::Bombardment).reports == 0);
    // An old summary-less packet makes only its category unknown, not zero.
    s.players[0].pendingPlayerReports[0].losses.reset(); s.players[0].pendingPlayerReports.resize(1);
    auto unknown = TurnProcessor{}.process(s,{});
    assert(!totals(unknown,1,OperationalLossKind::Bombardment,true).recorded);
    unknown.turn = 1; unknown.players[0].history.resize(1);
    packet.fleet = 2; unknown.players[0].pendingPlayerReports = {packet};
    record_empire_turn_statistics(unknown,{},true);
    assert(!totals(unknown,1,OperationalLossKind::Bombardment,true).recorded);
    unknown = TurnProcessor{}.process(unknown,{});
    assert(!totals(unknown,1,OperationalLossKind::Bombardment,true).recorded);
    assert(totals(unknown,1,OperationalLossKind::Bombardment,true).reports == 0);
    assert(totals(unknown,1,OperationalLossKind::Mines).recorded);
}

int main()
{
    ground_casualties_are_exact_and_relative_to_recipient();
    bombardments_sum_even_for_identical_fleets();
    mines_only_count_destroyed_ships_and_wait_for_communication();
    overdue_duplicate_and_unmeasured_packets();
    std::cout << "operational loss history tests passed\n";
}
