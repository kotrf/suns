#include "main_window.hpp"
#include "suns/communications.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

#include <cassert>
#include <iostream>
#include <memory>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w, GameState state = make_demo_game())
    {
        w.stopRouteForecastWorkers();
        w.state_ = std::move(state);
        w.pendingOrders_ = {1, {MoveFleetOrder{w.state_.fleets.front().id, {48, 0}, 5}}};
        w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star;
        w.selection_.fleet = w.state_.fleets.front().id;
        w.rebuildScene();
    }
    static void route(MainWindow& w, Position destination)
    {
        w.pendingOrders_ = {1, {MoveFleetOrder{w.state_.fleets.front().id, destination, 5}}};
        w.rebuildScene();
    }
    static void launch(MainWindow& w) { w.launchAsyncRouteForecasts(); }
    static void select(MainWindow& w, FleetId fleet) { w.selection_.fleet = fleet; }
    static auto fleet(const MainWindow& w) { return w.state_.fleets.front().id; }
    static auto revision(const MainWindow& w) { return w.planningRevision_; }
    static auto readyRevision(const MainWindow& w) { return w.routeEtaRevision_; }
    static auto readyFleet(const MainWindow& w) { return w.routeEtaFleet_; }
    static auto builds(const MainWindow& w) { return w.routeForecastBuildCount_; }
    static void addSecondRoute(MainWindow& w)
    {
        auto fleet = w.state_.fleets.front();
        fleet.id = 99;
        fleet.name = "Second forecast fleet";
        w.state_.fleets.push_back(fleet);
        w.pendingOrders_.orders.push_back(MoveFleetOrder{99, {10000, 0}, 5});
        w.rebuildScene();
    }
    static void useSentCommands(MainWindow& w)
    {
        w.pendingOrders_.orders.clear();
        w.rebuildScene();
    }
    static SaveGameData save(const MainWindow& w)
    {
        SaveGameData data;
        data.campaignId = 1;
        data.turnToken = 2;
        data.state = w.state_;
        data.galaxyConfig = w.galaxyConfig_;
        data.pendingOrders = w.pendingOrders_;
        for (std::size_t i = 0; i < data.pendingOrders.orders.size(); ++i)
            data.pendingDescriptions.push_back("Forecast regression route");
        data.selectedStar = w.selection_.star;
        data.selectedFleet = w.selection_.fleet;
        return data;
    }
    static bool load(MainWindow& w, const QString& path) { return w.loadGameFromPath(path); }
};
}

namespace {
void events()
{
    QEventLoop loop;
    QTimer::singleShot(5, &loop, &QEventLoop::quit);
    loop.exec();
}

void awaitForecasts(suns::MainWindow& w)
{
    QElapsedTimer deadline;
    deadline.start();
    while (w.routeForecastsPending() && deadline.elapsed() < 20000) events();
    assert(!w.routeForecastsPending());
    events(); // Include old queued completion handlers after stop/reset.
    assert(!w.routeForecastsPending());
}

void equalRows(const std::vector<suns::RouteProgramDisplayRow>& a,
    const std::vector<suns::RouteProgramDisplayRow>& b)
{
    assert(a.size() == b.size() && !a.empty());
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(!a[i].forecastPending && !b[i].forecastPending);
        assert(a[i].destination == b[i].destination);
        assert(a[i].arrivalAction == b[i].arrivalAction);
        assert(a[i].eta == b[i].eta && a[i].fuelUsed == b[i].fuelUsed);
        assert(a[i].fuelRemaining == b[i].fuelRemaining);
        assert(a[i].fuelShortage == b[i].fuelShortage);
    }
}

void asynchronous_results_match_exact_synchronous_forecasts()
{
    using namespace suns;
    MainWindow background, reference;
    MainWindowTestAccess::setup(background);
    MainWindowTestAccess::setup(reference);
    int ready = 0;
    QObject::connect(&background, &MainWindow::routeForecastReady, &background, [&] { ++ready; });
    const auto pendingRows = background.selectedFleetRouteProgramRows(true);
    assert(!pendingRows.empty() && pendingRows.front().forecastPending);
    assert(pendingRows.front().eta == QString("Pending…"));
    assert(pendingRows.front().fuelUsed == "—" && !pendingRows.front().fuelShortage);
    assert(background.selectedFleetRouteProgramForecast(true).contains("pending"));
    bool pending = false;
    const FleetArrivalAction action{};
    assert(background.selectedFleetWaypointFuelWarning(5, 0, action, Position{80, 0}, true, &pending).contains("Calculating"));
    assert(pending);
    awaitForecasts(background);
    assert(ready == 2);
    equalRows(background.selectedFleetRouteProgramRows(true), reference.selectedFleetRouteProgramRows());
    assert(background.selectedFleetRouteProgramForecast(true) == reference.selectedFleetRouteProgramForecast());
    assert(background.selectedFleetWaypointFuelWarning(5, 0, action, Position{80, 0}, true, &pending)
        == reference.selectedFleetWaypointFuelWarning(5, 0, action, Position{80, 0}));
    assert(!pending && !background.routeForecastsPending());
    const auto builds = MainWindowTestAccess::builds(background);
    for (int i = 0; i < 4; ++i) events();
    assert(MainWindowTestAccess::builds(background) == builds); // No idle retries/polling.
}

void rapid_edits_publish_only_latest_revision()
{
    using namespace suns;
    MainWindow background, reference;
    MainWindowTestAccess::setup(background);
    MainWindowTestAccess::setup(reference);
    int ready = 0;
    QObject::connect(&background, &MainWindow::routeForecastReady, &background, [&] { ++ready; });
    MainWindowTestAccess::route(background, {10000, 0});
    (void)background.selectedFleetRouteProgramRows(true);
    MainWindowTestAccess::launch(background); // Start the obsolete worker before replacing its input.
    const auto builds = MainWindowTestAccess::builds(background);
    for (int i = 0; i < 8; ++i) {
        MainWindowTestAccess::route(background, {40.0 + i, 0});
        const auto rows = background.selectedFleetRouteProgramRows(true);
        assert(rows.front().forecastPending && rows.front().eta == QString("Pending…"));
    }
    MainWindowTestAccess::route(reference, {47, 0});
    awaitForecasts(background);
    assert(ready == 1);
    assert(MainWindowTestAccess::readyRevision(background) == MainWindowTestAccess::revision(background));
    assert(MainWindowTestAccess::builds(background) == builds + 1); // Replaceable pending request.
    equalRows(background.selectedFleetRouteProgramRows(true), reference.selectedFleetRouteProgramRows());
}

void cache_hit_cancels_obsolete_actual_and_candidate_workers()
{
    using namespace suns;
    MainWindow w;
    MainWindowTestAccess::setup(w);
    MainWindowTestAccess::addSecondRoute(w);
    const auto fleetA = MainWindowTestAccess::fleet(w);
    const auto rowsA = w.selectedFleetRouteProgramRows();
    int ready = 0;
    QObject::connect(&w, &MainWindow::routeForecastReady, &w, [&] { ++ready; });
    MainWindowTestAccess::select(w, 99);
    assert(w.selectedFleetRouteProgramRows(true).front().forecastPending);
    MainWindowTestAccess::launch(w);
    MainWindowTestAccess::select(w, fleetA);
    equalRows(w.selectedFleetRouteProgramRows(true), rowsA);
    awaitForecasts(w);
    assert(ready == 0 && MainWindowTestAccess::readyFleet(w) == fleetA);

    FleetArrivalAction actionA{FleetArrivalActionKind::SupplyOutpost};
    actionA.reserveFuel = 5;
    auto actionB = actionA;
    actionB.reserveFuel = 900; // Same compact string key; full action must still invalidate.
    const auto warningA = w.selectedFleetWaypointFuelWarning(5, 0, actionA, Position{10000, 0});
    bool pending = false;
    (void)w.selectedFleetWaypointFuelWarning(5, 0, actionB, Position{10000, 0}, true, &pending);
    assert(pending);
    MainWindowTestAccess::launch(w);
    assert(w.selectedFleetWaypointFuelWarning(5, 0, actionA, Position{10000, 0}, true, &pending) == warningA);
    assert(!pending);
    awaitForecasts(w);
    assert(ready == 0);
    assert(w.selectedFleetWaypointFuelWarning(5, 0, actionA, Position{10000, 0}, true, &pending) == warningA && !pending);
}

void sent_delayed_commands_have_exact_async_parity()
{
    using namespace suns;
    auto state = make_demo_game();
    state.turn = 8;
    auto& fleet = state.fleets.front();
    fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
    fleet.telemetry.observedTurn = 7;
    fleet.telemetry.position = {420, 0};
    fleet.telemetry.destination = Position{480, 0};
    fleet.telemetry.warp = 4;
    fleet.position = {440, 0}; // Physical position must not replace the received snapshot.
    fleet.pendingCommands.push_back({8, 11, {{520, 0}, 5, {}, {{{560, 0}, 4, {}}}}});
    MainWindow background, reference;
    MainWindowTestAccess::setup(background, state);
    MainWindowTestAccess::setup(reference, state);
    MainWindowTestAccess::useSentCommands(background);
    MainWindowTestAccess::useSentCommands(reference);
    assert(background.selectedFleetRouteProgramRows(true).size() == 2);
    awaitForecasts(background);
    equalRows(background.selectedFleetRouteProgramRows(true), reference.selectedFleetRouteProgramRows());
    assert(background.selectedFleetRouteProgramForecast(true) == reference.selectedFleetRouteProgramForecast());
}

void stop_load_close_and_destruction_reject_old_completions(const QString& path)
{
    using namespace suns;
    MainWindow w, reference;
    MainWindowTestAccess::setup(w);
    MainWindowTestAccess::setup(reference);
    MainWindowTestAccess::route(reference, {62, 0});
    QString error;
    const bool written = write_save_game_file(path, MainWindowTestAccess::save(reference), error);
    if (!written) std::cerr << error.toStdString() << '\n';
    assert(written);
    SaveGameData checked;
    const bool readable = read_save_game_file(path, checked, error);
    if (!readable) std::cerr << error.toStdString() << '\n';
    assert(readable); // Invalid fixtures must fail without opening a modal error dialog.
    int ready = 0;
    QObject::connect(&w, &MainWindow::routeForecastReady, &w, [&] { ++ready; });
    (void)w.selectedFleetRouteProgramRows(true);
    MainWindowTestAccess::launch(w);
    w.stopRouteForecastWorkers();
    assert(!w.routeForecastsPending());
    events();
    assert(!w.routeForecastsPending() && ready == 0);

    (void)w.selectedFleetRouteProgramRows(true);
    MainWindowTestAccess::launch(w);
    assert(MainWindowTestAccess::load(w, path)); // Same turn/player/fleet IDs, different route.
    events();
    assert(!w.routeForecastsPending() && ready == 0);
    assert(w.selectedFleetRouteProgramRows(true).front().forecastPending);
    awaitForecasts(w);
    assert(ready == 1);
    equalRows(w.selectedFleetRouteProgramRows(true), reference.selectedFleetRouteProgramRows());

    MainWindowTestAccess::route(w, {10000, 0});
    (void)w.selectedFleetRouteProgramRows(true);
    MainWindowTestAccess::launch(w);
    w.close();
    events();
    assert(!w.routeForecastsPending() && ready == 1);
    auto dying = std::make_unique<MainWindow>();
    MainWindowTestAccess::setup(*dying);
    (void)dying->selectedFleetRouteProgramRows(true);
    MainWindowTestAccess::launch(*dying);
    dying.reset(); // Joins before game state and QObject context are destroyed.
    events();
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    asynchronous_results_match_exact_synchronous_forecasts();
    rapid_edits_publish_only_latest_revision();
    cache_hit_cancels_obsolete_actual_and_candidate_workers();
    sent_delayed_commands_have_exact_async_parity();
    stop_load_close_and_destruction_reject_old_completions(settings.filePath("replacement.suns"));
}
