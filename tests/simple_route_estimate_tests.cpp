#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "suns/communications.hpp"

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <cassert>
#include <cmath>
#include <utility>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& window, GameState state, PlayerOrders orders)
    {
        window.state_ = std::move(state);
        window.pendingOrders_ = std::move(orders);
        window.pendingDescriptions_.clear();
        window.selection_.fleet = window.state_.fleets.front().id;
        window.selection_.star = window.state_.stars.front().id;
        window.rebuildScene();
    }
    static const GameState& planned(const MainWindow& window) { return window.fleetPlanningState(); }
};
}

namespace {
using namespace suns;

GameState world()
{
    auto state = make_demo_game();
    state.stars.front().position = {0, 0};
    state.fleets.front().position = {0, 0};
    state.fleets.front().destination.reset();
    state.fleets.front().telemetry = {};
    state.fleets.front().telemetry = confirmed_fleet_telemetry(state, state.fleets.front());
    return state;
}

MoveFleetOrder route(FleetId fleet)
{
    return {fleet, {25, 0}, 5, {}, {{{75, 0}, 5, {}}}};
}

void nominal_time_is_cumulative_and_has_no_simulation_horizon(MainWindow& window)
{
    auto state = world();
    const auto fleet = state.fleets.front().id;
    MainWindowTestAccess::install(window, state, {1, {route(fleet)}});
    auto rows = window.selectedFleetRouteProgramRows();
    assert(rows.size() == 2 && rows[0].eta == "≈1" && rows[1].eta == "≈3");
    assert(rows[0].warp == 5 && rows[0].active && !rows[1].active);
    assert(rows[0].fuel.startsWith("≈") && rows[1].fuel.startsWith("≈"));
    assert(!rows[0].etaTooltip.isEmpty());
    const auto summary = window.selectedFleetRouteProgramEstimate();
    assert(!summary.isEmpty());

    MainWindowTestAccess::install(window, state, {1, {MoveFleetOrder{fleet, {25000, 0}, 5}}});
    rows = window.selectedFleetRouteProgramRows();
    assert(rows.size() == 1 && rows[0].eta == "≈1000");
    assert(rows[0].fuel.startsWith("≈"));
    assert(!window.selectedFleetRouteProgramEstimate().isEmpty());
}

void fuel_economy_and_medium_do_not_change_nominal_flight_time(MainWindow& window)
{
    auto state = world();
    const auto fleet = state.fleets.front().id;
    MainWindowTestAccess::install(window, state, {1, {route(fleet)}});
    const auto before = window.selectedFleetRouteProgramRows();

    state.fleets.front().fuel = 0;
    state.fleets.front().telemetry.fuel = 0;
    state.planets.front().industry = 10000;
    state.planets.front().population = 1;
    state.interstellarMediumEnabled = true;
    const PlayerOrders orders{1, {route(fleet),
        QueueProductionOrder{state.planets.front().id, ProductionKind::Factory},
        SetResearchAllocationOrder{95}}};
    MainWindowTestAccess::install(window, state, orders);
    const auto after = window.selectedFleetRouteProgramRows();
    assert(after.size() == before.size());
    assert(after[0].eta == before[0].eta && after[1].eta == before[1].eta);
    assert(after[0].fuel == before[0].fuel && after[1].fuel == before[1].fuel);
}

void conditional_arrival_does_not_promise_later_completion(MainWindow& window)
{
    auto state = world();
    const auto fleet = state.fleets.front().id;
    for (const auto kind : {FleetArrivalActionKind::LoadAllAvailable,
            FleetArrivalActionKind::Refuel, FleetArrivalActionKind::TransitStargate,
            FleetArrivalActionKind::EnterWormhole, FleetArrivalActionKind::Colonize}) {
        auto program = route(fleet);
        program.arrivalAction.kind = kind;
        MainWindowTestAccess::install(window, state, {1, {program}});
        const auto rows = window.selectedFleetRouteProgramRows();
        assert(rows.size() == 2 && rows[0].eta == "≈1" && rows[1].eta == "—");
        assert(rows[0].fuel.startsWith("≈"));
        assert(kind == FleetArrivalActionKind::Refuel ? rows[1].fuel.startsWith("≈") : rows[1].fuel == "—");
        assert(!rows[1].etaTooltip.isEmpty());
    }
}

void moving_target_uses_received_position_and_stops_later_estimates(MainWindow& window)
{
    auto state = world();
    state.turn = 5;
    auto target = state.fleets.front();
    target.id = state.nextFleetId++;
    target.name = "Received moving target";
    target.position = {1000, 0};
    target.telemetry.observedTurn = 4;
    target.telemetry.position = {25, 0};
    target.telemetry.destination = Position{400, 0};
    target.telemetry.warp = 9;
    state.fleets.push_back(target);
    auto program = route(state.fleets.front().id);
    program.targetFleet = target.id;
    program.destination = {900, 0}; // The received target, not a physical/fallback position.
    // One elapsed year advances the received W9 programme by 81 ly. This
    // established position estimate uses no hidden physical coordinates.
    assert(same_position(projected_fleet_position(state, target), Position{106, 0}));
    MainWindowTestAccess::install(window, state, {1, {program}});
    auto rows = window.selectedFleetRouteProgramRows();
    assert(rows.size() == 2 && rows[0].eta == "≈5" && rows[1].eta == "—");

    state.fleets.back().position = {2000, 0};
    MainWindowTestAccess::install(window, state, {1, {program}});
    const auto hiddenChange = window.selectedFleetRouteProgramRows();
    assert(hiddenChange[0].eta == rows[0].eta && hiddenChange[1].eta == rows[1].eta);
    assert(rows[0].fuel.startsWith("≈") && rows[1].fuel == "—");
    assert(hiddenChange[0].fuel == rows[0].fuel && hiddenChange[1].fuel == rows[1].fuel);
    state.fleets.back().telemetry.position = {60, 0};
    assert(same_position(projected_fleet_position(state, state.fleets.back()), Position{141, 0}));
    MainWindowTestAccess::install(window, state, {1, {program}});
    rows = window.selectedFleetRouteProgramRows();
    assert(rows[0].eta == "≈6" && rows[1].eta == "—");
}

void sent_navigation_outranks_old_telemetry_without_simulating_delivery(MainWindow& window)
{
    auto state = world();
    state.turn = 10;
    auto& fleet = state.fleets.front();
    fleet.position = {500, 0};
    fleet.telemetry.observedTurn = 9;
    fleet.telemetry.position = {25, 0};
    fleet.telemetry.destination = Position{50, 0};
    fleet.telemetry.warp = 1;
    fleet.pendingCommands.push_back({10, 30, {{75, 0}, 5, {}, {{{125, 0}, 5, {}}}}});
    MainWindowTestAccess::install(window, state, {1, {}});
    auto rows = window.selectedFleetRouteProgramRows();
    assert(rows.size() == 2 && rows[0].warp == 5);
    assert(rows[0].eta == "≈2" && rows[1].eta == "≈4");

    const auto fleetId = fleet.id;
    MainWindowTestAccess::install(window, state, {1, {MoveFleetOrder{fleetId, {50, 0}, 5}}});
    rows = window.selectedFleetRouteProgramRows();
    assert(rows.size() == 1 && rows[0].eta == "≈1"); // New pending edit still has precedence.
}

double nominalFuel(const RouteProgramDisplayRow& row)
{
    assert(row.fuel.startsWith("≈"));
    bool ok = false; const auto fuel = row.fuel.mid(1).toDouble(&ok);
    assert(ok); return fuel;
}

void fuel_uses_leg_distance_warp_and_rounding_without_year_simulation(MainWindow& window)
{
    auto state = generate_game(GalaxyConfig{});
    state.stars.front().position = {0, 0}; state.fleets.front().position = {0, 0};
    state.fleets.front().fuel = 50;
    state.fleets.front().telemetry = {};
    state.fleets.front().telemetry = confirmed_fleet_telemetry(state, state.fleets.front());
    const auto id = state.fleets.front().id; const PlayerOrders orders{1, {route(id)}};
    MainWindowTestAccess::install(window, state, orders);
    const auto rows = window.selectedFleetRouteProgramRows();
    assert(nominalFuel(rows[0]) == 3 && nominalFuel(rows[1]) > nominalFuel(rows[0]));
    assert(rows[0].fuelTooltip.contains("mg") && rows[0].fuelTooltip.contains("per-year rounding"));
    const auto first = TurnProcessor{}.process(state, {orders});
    assert(50 - first.fleets.front().fuel == nominalFuel(rows[0]));
    const auto third = TurnProcessor{}.process(TurnProcessor{}.process(first, {}), {});
    const auto realSecondLeg = first.fleets.front().fuel - third.fleets.front().fuel;
    assert(std::abs(realSecondLeg - nominalFuel(rows[1])) <= 2); // Annual billing can round separately.

    QApplication::processEvents(); QApplication::processEvents();
    auto* tree = window.findChild<QTreeWidget*>("routeProgramQueue"); assert(tree && tree->columnCount() == 5);
    assert(tree->topLevelItem(0)->text(4) == rows[0].fuel);
    auto fast = route(id); fast.warp = 8;
    MainWindowTestAccess::install(window, state, {1, {fast}});
    const auto faster = window.selectedFleetRouteProgramRows();
    assert(faster[0].eta == rows[0].eta && nominalFuel(faster[0]) > nominalFuel(rows[0]));
    QApplication::processEvents(); QApplication::processEvents();
    assert(tree->topLevelItem(0)->text(4) == faster[0].fuel);
}

void fuel_updates_for_accepted_cargo_but_not_hidden_manifests(MainWindow& window)
{
    auto state = world();
    auto& design = state.shipDesigns.front(); design.hull = ShipHullType::MediumTransport;
    design.components = {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}; design.placements.clear();
    state.fleets.front().fuel = fleet_fuel_capacity(state, state.fleets.front());
    state.fleets.front().telemetry = {};
    state.fleets.front().telemetry = confirmed_fleet_telemetry(state, state.fleets.front());
    const auto id = state.fleets.front().id;
    MainWindowTestAccess::install(window, state, {1, {route(id)}});
    const auto empty = window.selectedFleetRouteProgramRows();
    const TransferCargoOrder load{{state.planets.front().id, 0}, {0, id}, 0, {40, 0, 0}};
    MainWindowTestAccess::install(window, state, {1, {load, route(id)}});
    const auto loaded = window.selectedFleetRouteProgramRows();
    const auto& planned = MainWindowTestAccess::planned(window);
    assert(planned.fleets.front().minerals.ironium == 40);
    assert(loaded[0].eta == empty[0].eta && nominalFuel(loaded[0]) > nominalFuel(empty[0]));
    QApplication::processEvents(); QApplication::processEvents();
    auto* tree = window.findChild<QTreeWidget*>("routeProgramQueue");
    assert(tree->topLevelItem(0)->text(4) == loaded[0].fuel);
    assert(window.selectedFleetRouteProgramEstimate().contains(loaded[0].fuel));

    state.turn = 5; state.fleets.front().position = {1000, 1000};
    state.fleets.front().telemetry.observedTurn = 4;
    MainWindowTestAccess::install(window, state, {1, {route(id)}});
    const auto received = window.selectedFleetRouteProgramRows();
    state.fleets.front().position = {2000, 2000}; state.fleets.front().minerals = {200, 0, 0};
    state.fleets.front().fuel = 0; state.fleets.front().colonists = 100000;
    MainWindowTestAccess::install(window, state, {1, {route(id)}});
    const auto hidden = window.selectedFleetRouteProgramRows();
    assert(hidden[0].fuel == received[0].fuel && hidden[1].fuel == received[1].fuel);
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    suns::MainWindow window;
    suns::attachRouteProgramDock(window);
    nominal_time_is_cumulative_and_has_no_simulation_horizon(window);
    fuel_economy_and_medium_do_not_change_nominal_flight_time(window);
    conditional_arrival_does_not_promise_later_completion(window);
    moving_target_uses_received_position_and_stops_later_estimates(window);
    sent_navigation_outranks_old_telemetry_without_simulating_delivery(window);
    fuel_uses_leg_distance_warp_and_rounding_without_year_simulation(window);
    fuel_updates_for_accepted_cargo_but_not_hidden_manifests(window);
}
