#include "native_orbital_fixture.hpp"
#include "suns/turn_processor.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include "suns/combat.hpp"
#include "suns/strategic_operations.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>
using namespace suns;
namespace {
void cargo_projection_rechecks_the_link_after_evacuation()
{
    const auto state = test::native_capital_evacuation_fixture();
    const TransferCargoOrder evacuate{{1, 0, true}, {0, 1}, 1'000, {}};
    const TransferCargoOrder unload{{0, 1}, {1, 0, true}, 0, {3, 0, 0}};
    for (const bool unloadFirst : {false, true}) {
        PlayerOrders orders{1, unloadFirst ? std::vector<Order>{unload, evacuate} : std::vector<Order>{evacuate, unload}};
        const auto preview = projected_cargo_order_state(state, orders);
        const auto actual = TurnProcessor{}.process(state, {orders});
        assert(actual.fleets.front().colonists == 1'000 && actual.planets.front().nativeOrbit.population == 0);
        assert(actual.planets.front().minerals.ironium == (unloadFirst ? 103 : 100));
        assert(preview.planets.front().minerals.ironium == actual.planets.front().minerals.ironium);
        assert(preview.fleets.front().minerals.ironium == actual.fleets.front().minerals.ironium);
        assert(communication_delay_turns(preview, 1, preview.fleets.front().position) > 0);
        assert(preview.turn == state.turn && state.planets.front().nativeOrbit.population == 1'000);
    }
    const PlayerOrders exactMinerals{1, {evacuate, SetFleetMineralCargoOrder{1, 1, {}}}};
    const auto preview = projected_cargo_order_state(state, exactMinerals);
    const auto actual = TurnProcessor{}.process(state, {exactMinerals});
    assert(preview.fleets.front().minerals.ironium == actual.fleets.front().minerals.ironium);
    assert(preview.fleets.front().minerals.ironium == 3);
}
GameState arrival(GameState state, FleetCargoKind cargo, FleetArrivalActionKind kind = FleetArrivalActionKind::UnloadAll)
{
    const auto& fleet = state.fleets.front();
    return TurnProcessor{}.process(state, {{1, {MoveFleetOrder{fleet.id, fleet.position, 5, {kind, 0, cargo}}}}});
}
void shared_unload_obeys_station_capacity_and_keeps_minerals_independent()
{
    for (auto cargo : {FleetCargoKind::Colonists, FleetCargoKind::All, FleetCargoKind::OrbitalColonists}) {
        for (std::uint64_t free : {0, 50, 500}) {
            auto state = test::native_orbital_fixture();
            state.planets.front().nativeOrbit.population = 100'000 - free;
            const auto stock = state.planets.front().minerals;
            const auto next = arrival(state, cargo);
            assert(next.planets.front().nativeOrbit.population == 100'000);
            assert(next.fleets.front().colonists == 500-free);
            assert(!next.planets.front().population && !next.planets.front().habitat.population);
            assert(total_colony_population(next.planets.front()) + next.fleets.front().colonists == 100'500-free);
            assert(next.fleets.front().minerals.ironium == (cargo == FleetCargoKind::All ? 0 : 3));
            assert(next.planets.front().minerals.ironium + next.planets.front().nativeOrbit.mineralUnitsPaid
                == stock.ironium + (cargo == FleetCargoKind::All ? 3 : 0));
        }
    }
    auto missing = test::native_orbital_fixture();
    destroy_native_orbital_population(missing, missing.orbitalStations.front().id);
    missing.orbitalStations.clear();
    const auto next = arrival(missing, FleetCargoKind::All);
    assert(!next.planets.front().owner && !next.planets.front().population && !next.planets.front().nativeOrbit.population);
    assert(next.orbitalStations.empty() && next.fleets.front().colonists == 500);
    assert(next.fleets.front().minerals.ironium == 0); // Legal mineral delivery to an unowned surface.

    auto hostile = test::native_orbital_fixture();
    hostile.players.push_back({2, "Enemy"}); hostile.planets.front().owner = 2;
    const auto blocked = arrival(hostile, FleetCargoKind::All);
    assert(blocked.planets.front().owner == 2 && blocked.fleets.front().colonists == 500);
    assert(blocked.fleets.front().minerals.ironium == 3);
}
void exact_station_transfers_and_surface_bypasses()
{
    auto state = test::native_orbital_fixture();
    const auto fleet = state.fleets.front().id;
    const TransferCargoOrder unload{{0, fleet}, {1, 0, true}, 100, {}};
    const auto preview = projected_cargo_order_state(state, {1, {unload}});
    assert(preview.planets.front().nativeOrbit.population == 10'100 && preview.fleets.front().colonists == 400);
    const auto loaded = projected_cargo_order_state(preview, {1, {TransferCargoOrder{{1, 0, true}, {0, fleet}, 100, {}}}});
    assert(loaded.planets.front().nativeOrbit == state.planets.front().nativeOrbit && loaded.fleets.front().colonists == 500);
    const auto executed = TurnProcessor{}.process(state, {{1, {unload}}});
    assert(executed.planets.front().nativeOrbit.station == preview.planets.front().nativeOrbit.station);
    assert(executed.planets.front().nativeOrbit.population - executed.planets.front().nativeOrbit.populationGrown
        + executed.planets.front().nativeOrbit.populationLost == preview.planets.front().nativeOrbit.population);
    assert(executed.fleets.front().colonists == preview.fleets.front().colonists);
    for (const Order order : {Order{SetFleetColonistsOrder{1, fleet, 0}}, Order{SetFleetColonistsOrder{1, fleet, 600}},
        Order{TransferCargoOrder{{0, fleet}, {1, 0}, 100, {1, 0, 0}}},
        Order{TransferCargoOrder{{1, 0}, {0, fleet}, 100, {}}}}) {
        const auto plan = projected_cargo_order_state(state, {1, {order}});
        const auto host = TurnProcessor{}.process(state, {{1, {order}}});
        for (const auto* checked : {&plan, &host}) {
            const auto& residents = checked->planets.front().nativeOrbit;
            assert(residents.station == state.planets.front().nativeOrbit.station);
            assert(residents.population - residents.populationGrown + residents.populationLost == state.planets.front().nativeOrbit.population);
            assert(!checked->planets.front().population && checked->fleets.front().colonists == 500);
            assert(checked->fleets.front().minerals.ironium == 3);
        }
    }
    auto full = state; full.planets.front().nativeOrbit.population = 100'000;
    assert(projected_cargo_order_state(full, {1, {unload}}).fleets.front().colonists == 500);
    auto evacuation = state; evacuation.planets.front().nativeOrbit.population = 100;
    const auto evacuated = TurnProcessor{}.process(evacuation,
        {{1, {TransferCargoOrder{{1, 0, true}, {0, fleet}, 100, {}}}}});
    assert(!evacuated.planets.front().nativeOrbit.population && evacuated.fleets.front().colonists == 600);
    assert(evacuated.planets.front().nativeOrbit.station == state.planets.front().nativeOrbit.station);
    const auto loadedAtArrival = arrival(state, FleetCargoKind::OrbitalColonists, FleetArrivalActionKind::LoadAllAvailable);
    assert(loadedAtArrival.fleets.front().colonists > 500);
    assert(loadedAtArrival.planets.front().nativeOrbit.population + loadedAtArrival.fleets.front().colonists
        - loadedAtArrival.planets.front().nativeOrbit.populationGrown + loadedAtArrival.planets.front().nativeOrbit.populationLost == 10'500);
    assert(arrival(state, FleetCargoKind::Colonists, FleetArrivalActionKind::LoadAllAvailable).fleets.front().colonists == 500);
    // Ordinary colonization cannot create a free surface/native settlement.
    auto empty = state; empty.planets.front().owner = 0; empty.planets.front().nativeOrbit = {}; empty.orbitalStations.clear();
    const auto founding = arrival(empty, FleetCargoKind::All, FleetArrivalActionKind::Colonize);
    assert(!founding.planets.front().owner && founding.orbitalStations.empty() && founding.fleets.front().colonists == 500);
}
void refits_preserve_residents_and_reject_insufficient_housing()
{
    auto state = test::native_orbital_fixture(); auto& planet = state.planets.front();
    state.players.front().technology.levels.fill(20);
    StationConstruction refit{{OrbitalStationHullType::Starbase, {}, true, true}, planet.nativeOrbit.station};
    const auto before = planet.nativeOrbit;
    assert(station_construction_valid(state, planet, refit));
    assert(complete_station_construction(state, planet, refit));
    assert(planet.nativeOrbit == before && native_orbital_station(state, planet));
    planet.nativeOrbit.population = 100'001;
    refit.loadout.hull = OrbitalStationHullType::OrbitalDock;
    assert(!station_construction_valid(state, planet, refit));
    refit.loadout.hull = OrbitalStationHullType::Fortress;
    refit.refitStation = 0;
    assert(!station_construction_valid(state, planet, refit));
}
void battle_loss_is_physical_once_and_delivered_without_resurrecting_residents()
{
    auto state = test::native_orbital_fixture();
    const auto station = state.orbitalStations.front().id;
    // The transporter was physically evacuated already and is away from battle.
    state.fleets.front().position = {-1000, -1000};
    state.players.push_back({2, "Attacker"});
    state.shipDesigns.push_back({10, 2, "Attacker", ShipHullType::Destroyer,
        {ShipComponentType::FuelMizer, ShipComponentType::AntiMatterPulverizer}});
    Fleet attacker; attacker.id = state.nextFleetId++; attacker.owner = 2;
    attacker.design = 10; attacker.ships = {{10, 100}}; attacker.position = state.stars.front().position;
    state.fleets.push_back(attacker);
    queue_survey_report(state, 1, 1, 0, state.stars.front().position, state.turn+1, SurveyLevel::DeepSurvey);
    state.players.front().pendingSurveyReports.back().deliveryTurn = state.turn+10;
    const auto late = state.players.front().pendingSurveyReports.back().deliveryTurn;
    resolve_space_battles(state, state.turn+1);
    assert(state.orbitalStations.empty() && !state.planets.front().owner);
    assert(state.planets.front().nativeOrbit == NativeOrbitalPopulation{});
    assert(state.fleets.front().colonists == 500);
    const auto beforeDelivery = make_player_view(state, 1).state;
    assert(beforeDelivery.planets.front().nativeOrbit.population == 10'000);
    const auto packet = std::find_if(state.players.front().pendingPlayerReports.begin(), state.players.front().pendingPlayerReports.end(),
        [](const auto& r) { return r.kind == PlayerReportKind::SpaceBattle; });
    assert(packet != state.players.front().pendingPlayerReports.end());
    state.turn = packet->deliveryTurn;
    (void)deliver_due_player_reports(state);
    assert(known_planet_owner(state, 1, 1) == 0);
    const auto afterDelivery = make_player_view(state, 1).state;
    assert(!afterDelivery.planets.front().nativeOrbit.population && !afterDelivery.planets.front().owner);
    state.turn = late; (void)deliver_due_survey_reports(state);
    assert(known_planet_owner(state, 1, 1) == 0);
    assert(!make_player_view(state, 1).state.planets.front().nativeOrbit.population);
    destroy_native_orbital_population(state, station);
    assert(state.fleets.front().colonists == 500 && !state.planets.front().owner);
}
void creation_requires_compatible_race_choices()
{
    EmpireSetup setup; setup.name = "Native"; setup.primaryTrait = PrimaryRaceTrait::HabitatCivilization;
    for (auto controller : {PlayerController::Human, PlayerController::Computer}) {
        setup.controller = controller;
        const auto state=generate_campaign(GalaxyConfig{}, {setup});
        assert(state.planets.front().nativeOrbit.population == kInitialHomePopulation);
        assert(!state.planets.front().population && state.fleets.size() == 1);
        for (int invalid=0;invalid<2;++invalid) {
            auto bad=setup; bad.race=invalid?RacePreset::Terran:RacePreset::Radiotroph;bad.basicRemoteMining=invalid;
            bool rejected=false;
            try { (void)generate_campaign({}, {bad}); } catch (const std::invalid_argument&) { rejected=true; }
            assert(rejected);
        }
    }
}
void abandoned_station_cleanup_cannot_leave_a_dangling_housing_link()
{
    auto state = test::native_orbital_fixture();
    state.planets.front().nativeOrbit.population = 0; // Residents already evacuated.
    state.players.push_back({2, "Bomber"});
    state.shipDesigns.push_back({10, 2, "Bomber", ShipHullType::Scout,
        {ShipComponentType::FuelMizer, ShipComponentType::LadyFingerBomb}});
    Fleet bomber; bomber.id = state.nextFleetId++; bomber.owner = 2; bomber.design = 10;
    bomber.position = state.stars.front().position; bomber.task = FleetTask::Bombardment;
    bomber.ships = {{10, 100}}; state.fleets.push_back(bomber);
    resolve_bombardments(state, state.turn+1);
    assert(!state.planets.front().owner && state.orbitalStations.empty());
    assert(state.planets.front().nativeOrbit == NativeOrbitalPopulation{});
    assert(state.fleets.front().colonists == 500);
}
}
int main()
{
    cargo_projection_rechecks_the_link_after_evacuation();
    shared_unload_obeys_station_capacity_and_keeps_minerals_independent();
    exact_station_transfers_and_surface_bypasses();
    refits_preserve_residents_and_reject_insufficient_housing();
    battle_loss_is_physical_once_and_delivered_without_resurrecting_residents();
    creation_requires_compatible_race_choices();
    abandoned_station_cleanup_cannot_leave_a_dangling_housing_link();
}
