#include "native_orbital_fixture.hpp"
#include "suns/mining.hpp"
#include "suns/combat.hpp"
#include "suns/strategic_operations.hpp"
#include <algorithm>
#include <cassert>

using namespace suns;
namespace {
void owned_output_and_stock_access()
{
    auto state = test::native_orbital_miner_fixture();
    const auto idle = TurnProcessor{}.process(state, {});
    assert(idle.planets.front().minerals.ironium == 100);
    const auto next = TurnProcessor{}.process(state, {{1, {SetRemoteMiningOrder{state.fleets.front().id, true}}}});
    assert(next.fleets.front().task == FleetTask::RemoteMining);
    assert(next.planets.front().minerals.ironium == 108);
    assert(next.planets.front().minerals.boranium == 104);
    assert(next.planets.front().minerals.germanium == 102);
    assert(mineral_cargo_mass(next.fleets.front().minerals) == 0);
    assert(next.players.front().history.back().extraction.ironium == 0);
    assert(next.players.front().history.back().remoteExtraction.ironium == 8);
    assert(next.players.front().history.back().remoteMineHistory.size() == 1);
    auto again = TurnProcessor{}.process(next, {});
    assert(again.planets.front().minerals.ironium == 116);
    assert(again.players.front().history.back().remoteExtraction.ironium == 8);
    const auto stopped = TurnProcessor{}.process(again, {{1, {SetRemoteMiningOrder{1, false}}}});
    assert(stopped.planets.front().minerals.ironium == 116);
    assert(stopped.players.front().history.back().remoteExtraction.ironium == 0);

    // The mined stock is usable through the existing orbital cargo endpoint,
    // without putting ore in the miner or counting collection as extraction.
    auto collected = next;
    auto hauler = test::native_orbital_fixture().fleets.front();
    hauler.id = collected.nextFleetId++; hauler.colonists = 0; hauler.minerals = {};
    hauler.telemetry = {}; hauler.telemetry = confirmed_fleet_telemetry(collected, hauler);
    collected.fleets.push_back(hauler);
    collected = TurnProcessor{}.process(collected, {{1, {SetRemoteMiningOrder{1, false},
        TransferCargoOrder{{1, 0, true}, {0, hauler.id}, 0, {3, 0, 0}}}}});
    assert(collected.planets.front().minerals.ironium == 105 && collected.fleets.back().minerals.ironium == 3);
    assert(collected.players.front().history.back().remoteExtraction.ironium == 0);
}
void eligibility_and_arrival()
{
    auto state = test::native_orbital_miner_fixture();
    const auto* design = find_ship_design(state, 90); assert(design);
    assert(projected_remote_mining(state, state.planets.front(), *design).ironium == 4);
    for (const auto native : {false, true}) for (const auto owner : {0U, 1U, 2U}) {
        auto s = state; s.players.front().race.primaryTrait = native ? PrimaryRaceTrait::HabitatCivilization : PrimaryRaceTrait::Generalist;
        s.planets.front().owner = owner;
        assert(remote_mining_site_available(s, 1, s.planets.front()) == (owner == 0 || (native && owner == 1)));
    }
    auto noStation = state; noStation.orbitalStations.clear();
    assert(!remote_mining_site_available(noStation, 1, noStation.planets.front()));
    auto innate = state; innate.planets.front().population = 25'000; innate.planets.front().mines = 100;
    assert(mineral_cargo_mass(projected_mineral_mining(innate, innate.planets.front())) == 0);
    auto missingEquipment = state; missingEquipment.players.front().technology.levels[std::size_t(ResearchField::Construction)] = 0;
    missingEquipment.fleets.front().task = FleetTask::RemoteMining;
    assert(TurnProcessor{}.process(missingEquipment, {}).planets.front().minerals.ironium == 100);

    state.fleets.front().position = {-25, 0}; state.fleets.front().telemetry.position = {-25, 0};
    const auto arrived = TurnProcessor{}.process(state, {{1, {MoveFleetOrder{1, {0, 0}, 5, {FleetArrivalActionKind::RemoteMining}}}}});
    assert(arrived.fleets.front().task == FleetTask::RemoteMining);
    assert(arrived.planets.front().minerals.ironium == 100); // Travel is not a full mining year.
    assert(TurnProcessor{}.process(arrived, {}).planets.front().minerals.ironium == 108);
}
GameState remote_site()
{
    auto state = test::native_orbital_miner_fixture();
    auto p = state.planets.front(); p.id = state.planets[1].id; p.star = state.stars[1].id;
    p.homeworld = false; p.radar = PlanetaryRadarLevel::None;
    auto station = state.orbitalStations.front(); station.id = state.nextOrbitalStationId++; station.planet = p.id;
    p.nativeOrbit.station = station.id; state.orbitalStations.push_back(station);
    state.planets[1] = p; state.stars[1].position = {420, 0};
    auto& fleet = state.fleets.front(); fleet.position = {420, 0}; fleet.telemetry.position = fleet.position;
    fleet.task = fleet.telemetry.task = FleetTask::RemoteMining;
    set_survey_level(state, 1, p.star, SurveyLevel::DeepSurvey, state.turn);
    state.players.front().history.clear(); record_empire_turn_statistics(state);
    return state;
}
void received_manifests_only()
{
    auto state = remote_site();
    auto resolved = TurnProcessor{}.process_with_events(state, {}); state = resolved.state;
    assert(state.planets[1].minerals.ironium == 108);
    assert(state.players.front().history.back().remoteExtraction.ironium == 0);
    assert(std::none_of(resolved.events.begin(), resolved.events.end(), [](const auto& e) { return e.kind == GameEventKind::RemoteMiningOutput; }));
    const auto report = std::find_if(state.players.front().pendingPlayerReports.begin(), state.players.front().pendingPlayerReports.end(),
        [](const auto& r) { return r.kind == PlayerReportKind::RemoteMiningOutput; });
    assert(report != state.players.front().pendingPlayerReports.end() && report->deliveredMinerals.ironium == 8);
    const auto due = report->deliveryTurn; assert(due > state.turn);
    while (state.turn < due) {
        resolved = TurnProcessor{}.process_with_events(state, {}); state = resolved.state;
        if (state.turn < due) assert(state.players.front().history.back().remoteExtraction.ironium == 0);
    }
    assert(std::count_if(resolved.events.begin(), resolved.events.end(), [](const auto& e) { return e.kind == GameEventKind::RemoteMiningOutput; }) == 1);
    assert(state.players.front().history.back().remoteExtraction.ironium == 8);
    record_empire_turn_statistics(state);
    assert(state.players.front().history.back().remoteExtraction.ironium == 8);
    assert(state.players.front().history[1].remoteExtraction.ironium == 0);

    // Ownership is checked when a delayed task packet actually reaches the
    // miner, rather than when HQ authored it using the older owned snapshot.
    auto delayed = remote_site(); delayed.fleets.front().task = delayed.fleets.front().telemetry.task = FleetTask::None;
    delayed = TurnProcessor{}.process(delayed, {{1, {SetRemoteMiningOrder{1, true}}}});
    assert(!delayed.fleets.front().pendingCommands.empty());
    const auto arrival = delayed.fleets.front().pendingCommands.front().deliveryTurn;
    delayed.players.push_back({2, "Other"});
    delayed.planets[1].owner = 2; delayed.planets[1].nativeOrbit = {}; delayed.planets[1].population = 25'000;
    std::erase_if(delayed.orbitalStations, [](const auto& s) { return s.planet == 2; });
    while (delayed.turn <= arrival) delayed = TurnProcessor{}.process(delayed, {});
    assert(delayed.fleets.front().task == FleetTask::None);
    assert(delayed.players.front().history.back().remoteExtraction.ironium == 0);
}
GameState neutral_miner()
{
    auto state = test::native_orbital_miner_fixture();
    auto& site = state.planets[1]; site.minerals = {};
    site.observedConcentration = MineralCargo{100, 50, 25};
    state.stars[1].position = {10, 0};
    auto& miner = state.fleets.front(); miner.position = {10, 0};
    miner.task = FleetTask::RemoteMining;
    miner.telemetry = {};
    miner.telemetry = confirmed_fleet_telemetry(state, miner);
    set_survey_level(state, 1, site.star, SurveyLevel::DeepSurvey, state.turn);
    state.players.front().history.clear(); record_empire_turn_statistics(state);
    assert(communication_delay_turns(state, 1, miner.position) == 0);
    return state;
}
void mining_reports_keep_their_physical_source()
{
    for (const auto mode : {EmissionMode::Passive, EmissionMode::RadioSilence}) {
        for (const bool bystander : {false, true}) {
            auto state = neutral_miner();
            state.fleets.front().electronics = {mode, mode == EmissionMode::RadioSilence ? 20u : 0u};
            if (bystander) {
                auto other = test::native_orbital_fixture().fleets.front();
                other.id = state.nextFleetId++; other.position = {10, 0};
                other.telemetry = {};
                other.telemetry = confirmed_fleet_telemetry(state, other);
                state.fleets.push_back(other);
            }
            const auto result = TurnProcessor{}.process_with_events(state, {});
            state = result.state;
            assert(state.planets[1].minerals.ironium == 8);
            assert(state.players.front().history.back().remoteExtraction.ironium == 0);
            assert(std::none_of(result.events.begin(), result.events.end(), [](const auto& e) { return e.kind == GameEventKind::RemoteMiningOutput; }));
            const auto pending = std::find_if(state.players.front().pendingPlayerReports.begin(), state.players.front().pendingPlayerReports.end(),
                [](const auto& r) { return r.kind == PlayerReportKind::RemoteMiningOutput; });
            assert(pending != state.players.front().pendingPlayerReports.end());
            assert(pending->transmissionFleet == 1 && pending->deliveryTurn == kAwaitingTransmission);
            assert(make_player_view(state, 1).state.players.front().history.back().remoteExtraction.ironium == 0);

            auto lost = state;
            std::erase_if(lost.fleets, [](const auto& f) { return f.id == 1; });
            const auto afterLoss = TurnProcessor{}.process_with_events(lost, {});
            assert(afterLoss.state.players.front().history.back().remoteExtraction.ironium == 0);
            assert(std::none_of(afterLoss.events.begin(), afterLoss.events.end(), [](const auto& e) { return e.kind == GameEventKind::RemoteMiningOutput; }));

            // Waking the actual source releases its already mined output in
            // the receipt year, without mining again or rewriting year one.
            state.fleets.front().task = FleetTask::None;
            apply_electronics_program(state.fleets.front(), {});
            dispatch_fleet_report_outbox(state);
            const auto received = TurnProcessor{}.process_with_events(state, {});
            assert(received.state.players.front().history[1].remoteExtraction.ironium == 8);
            assert(received.state.players.front().history.back().remoteExtraction.ironium == 0);
            assert(received.state.players.front().history.front().remoteExtraction.ironium == 0);
        }
        auto owned = test::native_orbital_miner_fixture();
        owned.fleets.front().task = FleetTask::RemoteMining;
        owned.fleets.front().electronics = {mode, mode == EmissionMode::RadioSilence ? 20u : 0u};
        assert(TurnProcessor{}.process(owned, {}).players.front().history.back().remoteExtraction.ironium == 8);
    }
    for (const bool silent : {false, true}) {
        auto state = neutral_miner();
        if (silent) state.fleets.front().electronics = {EmissionMode::RadioSilence, 20};
        auto second = state.fleets.front(); second.id = state.nextFleetId++;
        second.electronics = {}; second.telemetry = confirmed_fleet_telemetry(state, second);
        state.fleets.push_back(second);
        const auto result = TurnProcessor{}.process_with_events(state, {});
        assert(result.state.planets[1].minerals.ironium == 16);
        assert(result.state.players.front().history.back().remoteExtraction.ironium == (silent ? 8 : 16));
        std::vector<GameEvent> reports;
        for (const auto& e : result.events) if (e.kind == GameEventKind::RemoteMiningOutput) reports.push_back(e);
        assert(reports.size() == (silent ? 1 : 2));
        for (const auto& e : reports) assert(e.deliveredMinerals.ironium == 8);
        if (silent) assert(reports.front().fleet == second.id);
        else assert(reports[0].id != reports[1].id && reports[0].fleet != reports[1].fleet);
    }
}
void destroyed_physical_miner_stops()
{
    auto state = test::native_orbital_miner_fixture();
    state.fleets.front().task = state.fleets.front().telemetry.task = FleetTask::RemoteMining;
    state.players.push_back({2, "Attacker"});
    state.shipDesigns.push_back({91, 2, "Attacker", ShipHullType::Destroyer,
        {ShipComponentType::FuelMizer, ShipComponentType::AntiMatterPulverizer, ShipComponentType::Jammer50}});
    Fleet attacker; attacker.id = state.nextFleetId++; attacker.owner = 2; attacker.design = 91;
    attacker.ships = {{91, 100}}; attacker.position = {0, 0}; attacker.fuel = fleet_fuel_capacity(state, attacker);
    state.fleets.push_back(attacker);
    state = TurnProcessor{}.process(state, {});
    assert(std::none_of(state.fleets.begin(), state.fleets.end(), [](const auto& f) { return f.owner == 1; }));
    assert(state.planets.front().minerals.ironium == 108); // Mining preceded this year's battle.
    const auto next = TurnProcessor{}.process(state, {});
    assert(next.planets.front().minerals.ironium == 108);
}
}
int main()
{
    mining_reports_keep_their_physical_source();
    owned_output_and_stock_access(); eligibility_and_arrival(); received_manifests_only(); destroyed_physical_miner_stops();
}
