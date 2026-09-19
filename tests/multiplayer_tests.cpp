#include "main_window.hpp"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>

#include <cassert>
#include <limits>

namespace suns {
struct MainWindowTestAccess {
    static bool load(MainWindow& w, const QString& path) { return w.loadGameFromPath(path); }
    static void plan(MainWindow& w) { w.queueResearchAllocation(75); }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static const Fleet* fleet(const MainWindow& w) { return w.selectedFleet(); }
    static void endTurn(MainWindow& w) { w.endTurn(); }
    static std::uint64_t turn(const MainWindow& w) { return w.state_.turn; }
    static void receive(MainWindow& w, PlayerOrders orders) { w.inbox_.push_back(std::move(orders)); }
    static SessionMode mode(const MainWindow& w) { return w.sessionMode_; }
    static QString empire(const MainWindow& w) { return w.empireLabel_->text(); }
    static bool save(MainWindow& w, const QString& path) { return w.saveGameToPath(path); }
};
}

using namespace suns;

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    assert(directory.isValid());
    SaveGameData host;
    host.mode = SessionMode::Host;
    host.campaignId = 12345;
    host.turnToken = 111;
    host.playerTokens = {{1, 111}, {2, 222}};
    host.state = generate_campaign(host.galaxyConfig,
        {{"Local", RacePreset::Terran}, {"Remote", RacePreset::Cryophile}});
    host.pendingOrders = {1, {}};
    QString error;
    const auto hostPath = directory.filePath("host.suns");
    const auto turnPath = directory.filePath("remote.sunsturn");
    assert(write_save_game_file(hostPath, host, error));
    const auto packet = make_player_turn(host, 2);
    assert(packet.playerTokens.empty() && packet.inbox.empty());
    assert(packet.turnToken == 222 && packet.mode == SessionMode::PlayerTurn);
    assert(write_save_game_file(turnPath, packet, error));
    // Hidden changes cannot change the bytes delivered to the remote player.
    auto altered = host;
    altered.state.players[0].technology.levels.fill(9);
    altered.state.planets[0].population = 999999;
    altered.state.planets[0].productionQueue.push_back({ProductionKind::Factory, 1, 0});
    altered.state.planets[0].precursorArtifacts = {true, false, 0, 999};
    altered.state.fleets[0].colonists = 54321;
    altered.state.fleets[0].destination = Position{123, 456};
    const auto alteredPath = directory.filePath("altered.sunsturn");
    assert(write_save_game_file(alteredPath, make_player_turn(altered, 2), error));
    QFile originalBytes(turnPath), changedBytes(alteredPath);
    assert(originalBytes.open(QIODevice::ReadOnly) && changedBytes.open(QIODevice::ReadOnly));
    assert(originalBytes.readAll() == changedBytes.readAll());
    SaveGameData loaded;
    assert(read_save_game_file(turnPath, loaded, error));
    assert(loaded.state.players.size() == 1 && loaded.pendingOrders.player == 2);
    assert(loaded.state.players.front().race.environmentBased);
    assert(loaded.state.planets.front().observedHabitability == 100);
    assert(loaded.state.planets.front().observedConcentration);

    MainWindow remote;
    app.processEvents();
    assert(MainWindowTestAccess::load(remote, turnPath));
    assert(MainWindowTestAccess::mode(remote) == SessionMode::PlayerTurn);
    assert(MainWindowTestAccess::fleet(remote)->owner == 2);
    assert(MainWindowTestAccess::empire(remote).contains("Remote"));
    MainWindowTestAccess::plan(remote);
    const auto& planned = MainWindowTestAccess::orders(remote);
    assert(planned.player == 2 && planned.orders.size() == 1);
    assert(std::get<SetResearchAllocationOrder>(planned.orders.front()).percent == 75);
    assert(MainWindowTestAccess::save(remote, directory.filePath("draft.suns")));
    assert(read_save_game_file(directory.filePath("draft.suns"), loaded, error));
    assert(loaded.mode == SessionMode::PlayerTurn && loaded.pendingOrders.player == 2);

    TurnOrderFileData submission{host.campaignId, host.state.turn, 222, planned, {"Allocate 75%"}};
    assert(validate_turn_submission(host, submission).isEmpty());
    const auto ordersPath = directory.filePath("remote.sunsorders");
    assert(write_turn_order_file(ordersPath, submission, error));
    TurnOrderFileData accepted;
    assert(read_turn_order_file(ordersPath, accepted, error));
    assert(validate_turn_submission(host, accepted).isEmpty());
    auto bad = accepted;
    ++bad.turn;
    assert(!validate_turn_submission(host, bad).isEmpty());
    bad = accepted; ++bad.campaignId;
    assert(!validate_turn_submission(host, bad).isEmpty());
    bad = accepted; bad.orders.player = 3;
    assert(!validate_turn_submission(host, bad).isEmpty());
    bad = accepted; bad.turnToken = 111;
    assert(!validate_turn_submission(host, bad).isEmpty());
    bad = accepted; bad.orders.orders = {MoveFleetOrder{2, {std::numeric_limits<double>::quiet_NaN(), 0}, 5}};
    assert(write_turn_order_file(ordersPath, bad, error));
    assert(!read_turn_order_file(ordersPath, bad, error));

    host.inbox = {accepted.orders};
    assert(write_save_game_file(hostPath, host, error));
    assert(read_save_game_file(hostPath, loaded, error));
    assert(loaded.inbox.size() == 1 && loaded.playerTokens.at(2) == 222);
    const auto result = resolve_campaign_turn(loaded.state, {loaded.pendingOrders, loaded.inbox.front()});
    assert(find_player(result.state, 2)->technology.researchAllocationPercent == 75);
    assert(find_player(result.state, 1)->technology.researchAllocationPercent == 0);

    host.inbox.clear();
    assert(write_save_game_file(hostPath, host, error));
    MainWindow local;
    app.processEvents();
    assert(MainWindowTestAccess::load(local, hostPath));
    MainWindowTestAccess::endTurn(local);
    assert(MainWindowTestAccess::turn(local) == 1); // Waits; never silently skips remote player.
    MainWindowTestAccess::receive(local, accepted.orders);
    MainWindowTestAccess::endTurn(local);
    assert(MainWindowTestAccess::turn(local) == 2);
    assert(read_save_game_file(hostPath, loaded, error));
    assert(loaded.state.turn == 2 && loaded.inbox.empty());
    assert(loaded.playerTokens.at(2) != 222);
    assert(!validate_turn_submission(loaded, accepted).isEmpty());
    return 0;
}
