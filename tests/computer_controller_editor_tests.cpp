#include "main_window.hpp"
#include "suns/computer_player.hpp"

#include <QApplication>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include <cassert>
#include <stdexcept>

namespace suns {
struct MainWindowTestAccess {
    static void campaign(MainWindow& w) { w.newCampaign(); }
    static void restart(MainWindow& w) { w.newGalaxy(); }
    static SaveGameData snapshot(const MainWindow& w) { return w.campaignSnapshot(); }
    static void receive(MainWindow& w, PlayerOrders orders) { w.inbox_.push_back(std::move(orders)); w.updateControls(); }
    static void endTurn(MainWindow& w) { w.endTurn(); }
    static void exportTurns(MainWindow& w) { w.exportPlayerTurns(); }
    static void journal(MainWindow& w) { w.openComputerDecisions(); }
    static bool save(MainWindow& w, const QString& path) { return w.saveGameToPath(path); }
    static bool load(MainWindow& w, const QString& path) { return w.loadGameFromPath(path); }
    static QAbstractButton* turnButton(MainWindow& w) { return w.endTurnButton_; }
};
}

using namespace suns;

namespace {

void capture(QWidget* widget, const QString& name)
{
    const auto directory = qEnvironmentVariable("SUNS_TEST_CAPTURE_DIR");
    if (!directory.isEmpty()) {
        QDir().mkpath(directory);
        widget->grab().save(QDir(directory).filePath(name));
    }
}

void create_campaign(MainWindow& window, bool remoteHuman)
{
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "newCampaignDialog");
        auto* count = dialog->findChild<QSpinBox*>("campaignEmpireCount");
        auto* table = dialog->findChild<QTableWidget*>("campaignEmpiresTable");
        assert(count && table && table->columnCount() == 8);
        count->setValue(remoteHuman ? 3 : 2);
        auto* local = qobject_cast<QComboBox*>(table->cellWidget(0, 7));
        auto* computer = qobject_cast<QComboBox*>(table->cellWidget(1, 7));
        assert(local->currentText() == "Human" && !local->isEnabled());
        assert(computer->currentText() == "Computer" && computer->isEnabled());
        table->item(0, 0)->setText("Local");
        table->item(1, 0)->setText("Computer");
        if (remoteHuman) {
            qobject_cast<QComboBox*>(table->cellWidget(2, 7))->setCurrentIndex(0);
            table->item(2, 0)->setText("Remote");
        }
        capture(dialog, "campaign-controllers.png");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    MainWindowTestAccess::campaign(window);
}

void persist_and_reject_corrupt_controllers(const SaveGameData& save, const QTemporaryDir& directory)
{
    QString error;
    const auto path = directory.filePath("controllers.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.state.players[1].controller == PlayerController::Computer);
    assert(loaded.state.players[1].computer == save.state.players[1].computer);
    assert(loaded.playerTokens.size() == 2 && !loaded.playerTokens.contains(2));
    for (const auto kind : {ComputerFleetGoalKind::Gather, ComputerFleetGoalKind::Defend,
        ComputerFleetGoalKind::Raid, ComputerFleetGoalKind::Bombard, ComputerFleetGoalKind::Invade}) {
        auto military = save;
        military.state.players[1].computer.fleetGoals.front().kind = kind;
        assert(write_save_game_file(directory.filePath("military-goals.suns"), military, error));
        SaveGameData restored;
        assert(read_save_game_file(directory.filePath("military-goals.suns"), restored, error));
        assert(restored.state.players[1].computer == military.state.players[1].computer);
    }

    QFile file(path); assert(file.open(QIODevice::ReadOnly));
    const auto original = file.readAll(); file.close();
    QByteArray marker; QDataStream signature(&marker, QIODevice::WriteOnly); signature << quint32{0x4354524Cu};
    const auto controllerOffset = original.lastIndexOf(marker);
    assert(controllerOffset > 0);
    // CTRL header, then the first human's empty record, then computer 2.
    const auto roleOffset = controllerOffset + 8 + 15 + 4;
    const auto corrupt = [&](auto edit) {
        auto bytes = original;
        QDataStream stream(&bytes, QIODevice::ReadWrite);
        edit(stream, roleOffset);
        assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        assert(file.write(bytes) == bytes.size()); file.close();
        SaveGameData rejected;
        assert(!read_save_game_file(path, rejected, error));
    };
    corrupt([](QDataStream& out, qint64 offset) { out.device()->seek(offset); out << quint8{2}; });
    corrupt([](QDataStream& out, qint64 offset) { out.device()->seek(offset + 1); out << quint8{2}; });
    corrupt([](QDataStream& out, qint64 offset) { out.device()->seek(offset + 2); out << quint32{33}; });
    corrupt([&](QDataStream& out, qint64 offset) { out.device()->seek(offset + 6); out << quint64(save.state.turn); });
    QDataStream fields(original);
    fields.device()->seek(roleOffset + 6);
    for (std::size_t i = 0; i < save.state.players[1].computer.decisions.size(); ++i) {
        quint64 turn{}; quint32 orders{}; QString reason;
        fields >> turn >> orders >> reason;
    }
    const auto goalsOffset = fields.device()->pos();
    assert(!save.state.players[1].computer.fleetGoals.empty());
    corrupt([&](QDataStream& out, qint64) { out.device()->seek(goalsOffset); out << quint32{129}; });
    corrupt([&](QDataStream& out, qint64) { out.device()->seek(goalsOffset + 4 + 4); out << quint8{8}; });
    corrupt([&](QDataStream& out, qint64) { out.device()->seek(goalsOffset + 4 + 5); out << quint32{0}; });
    corrupt([&](QDataStream& out, qint64) { out.device()->seek(goalsOffset + 4 + 9); out << quint64(save.state.turn); });
    const auto researchOffset = goalsOffset + 4 + 25 * save.state.players[1].computer.fleetGoals.size();
    assert(save.state.players[1].computer.researchGoal);
    corrupt([&](QDataStream& out, qint64) { out.device()->seek(researchOffset); out << quint8{2}; });
    corrupt([&](QDataStream& out, qint64) { out.device()->seek(researchOffset + 1); out << quint8{6}; });
    corrupt([&](QDataStream& out, qint64) { out.device()->seek(researchOffset + 2); out << quint8{0}; });
    auto bad = save; bad.state.players[1].computer.decisions.back().turn = save.state.turn;
    assert(!write_save_game_file(path, bad, error));
    bad = save; bad.state.players[0].computer = bad.state.players[1].computer;
    assert(!write_save_game_file(path, bad, error));
    bad = save; bad.state.players[1].computer.fleetGoals.push_back(bad.state.players[1].computer.fleetGoals.front());
    assert(!write_save_game_file(path, bad, error));

    bad = save; bad.playerTokens[2] = 222;
    assert(write_save_game_file(path, bad, error));
    assert(!read_save_game_file(path, loaded, error));
    bad = save; bad.inbox = {{2, {}}};
    assert(write_save_game_file(path, bad, error));
    assert(!read_save_game_file(path, loaded, error));
    bad = save; bad.pendingOrders = {2, {}};
    assert(write_save_game_file(path, bad, error));
    assert(!read_save_game_file(path, loaded, error));

    // Resuming ongoing exploration must make the same decisions and physical
    // progress as an uninterrupted campaign, including later settlement goals.
    assert(write_save_game_file(path, save, error));
    assert(read_save_game_file(path, loaded, error));
    auto uninterrupted = save.state;
    auto resumed = loaded.state;
    for (int year = 0; year < 25; ++year) {
        uninterrupted = resolve_controlled_campaign_turn(uninterrupted, {{1, {}}, {3, {}}}).state;
        resumed = resolve_controlled_campaign_turn(resumed, {{1, {}}, {3, {}}}).state;
        assert(find_player(uninterrupted, 2)->computer == find_player(resumed, 2)->computer);
        assert(uninterrupted.fleets.size() == resumed.fleets.size());
        for (std::size_t i = 0; i < resumed.fleets.size(); ++i) {
            assert(same_position(uninterrupted.fleets[i].position, resumed.fleets[i].position));
            assert(uninterrupted.fleets[i].fuel == resumed.fleets[i].fuel);
        }
        for (std::size_t i = 0; i < resumed.planets.size(); ++i)
            assert(uninterrupted.planets[i].owner == resumed.planets[i].owner
                && uninterrupted.planets[i].population == resumed.planets[i].population);
    }
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QTemporaryDir directory;
    assert(directory.isValid());
    MainWindow window;
    window.show(); app.processEvents();
    create_campaign(window, true);
    auto host = MainWindowTestAccess::snapshot(window);
    assert(host.state.players.size() == 3 && host.mode == SessionMode::Host);
    assert(host.state.players[0].controller == PlayerController::Human);
    assert(host.state.players[1].controller == PlayerController::Computer);
    assert(host.state.players[2].controller == PlayerController::Human);
    assert(host.playerTokens.size() == 2 && host.playerTokens.contains(3) && !host.playerTokens.contains(2));
    assert(MainWindowTestAccess::turnButton(window)->text().contains("0/1 human orders"));
    assert(MainWindowTestAccess::turnButton(window)->toolTip().contains("Computer: computer"));
    MainWindowTestAccess::endTurn(window);
    MainWindowTestAccess::endTurn(window);
    host = MainWindowTestAccess::snapshot(window);
    assert(host.state.turn == 1 && !host.state.players[1].computer.initialized);
    assert(window.statusBar()->currentMessage().contains("remote human"));

    const auto hostPath = directory.filePath("host.suns");
    assert(MainWindowTestAccess::save(window, hostPath));
    const auto exports = directory.filePath("turns"); QDir().mkpath(exports);
    QTimer::singleShot(0, [&] {
        auto* picker = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        assert(picker);
        picker->setDirectory(exports); picker->selectFile(exports);
        QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection);
    });
    MainWindowTestAccess::exportTurns(window);
    const auto files = QDir(exports).entryList({"*.sunsturn"}, QDir::Files);
    assert(files.size() == 1 && files.front().contains("player-3"));
    QString error;
    SaveGameData remote;
    assert(read_save_game_file(QDir(exports).filePath(files.front()), remote, error));
    assert(remote.state.players.size() == 1 && remote.state.players[0].id == 3);
    assert(remote.state.players[0].controller == PlayerController::Human);
    bool refusedComputerExport = false;
    try { (void)make_player_turn(host, 2); } catch (const std::invalid_argument&) { refusedComputerExport = true; }
    assert(refusedComputerExport);
    assert(validate_turn_submission(host, {host.campaignId, 1, 222, {2, {}}, {}}).contains("Computer"));

    const auto firstPlan = plan_computer_turn(make_player_view(host.state, 2));
    MainWindowTestAccess::receive(window, {3, {}}); // An explicit human pass is still required.
    assert(MainWindowTestAccess::turnButton(window)->text().contains("1/1 human orders"));
    MainWindowTestAccess::endTurn(window);
    host = MainWindowTestAccess::snapshot(window);
    assert(host.state.turn == 2 && host.inbox.empty());
    assert(host.state.players[1].computer.initialized);
    assert(host.state.players[1].technology.researchAllocationPercent == 20);
    assert(host.state.players[0].technology.researchAllocationPercent == 0);
    assert(host.state.players[1].computer.decisions.size() == 1);
    assert(host.state.players[1].computer == firstPlan.nextState);
    persist_and_reject_corrupt_controllers(host, directory);

    // Other empires' controller memory cannot alter a human's PBEM bytes.
    const auto remotePath = directory.filePath("remote.sunsturn");
    const auto changedPath = directory.filePath("changed.sunsturn");
    assert(write_save_game_file(remotePath, make_player_turn(host, 3), error));
    auto hidden = host; hidden.state.players[1].computer.decisions[0].reason = "A different private decision.";
    assert(write_save_game_file(changedPath, make_player_turn(hidden, 3), error));
    QFile original(remotePath), changed(changedPath);
    assert(original.open(QIODevice::ReadOnly) && changed.open(QIODevice::ReadOnly));
    assert(original.readAll() == changed.readAll());

    MainWindowTestAccess::journal(window); app.processEvents();
    auto* journal = window.findChild<QDialog*>("computerDecisionsDialog");
    auto* table = journal->findChild<QTreeWidget*>("computerDecisionsTable");
    assert(table->topLevelItemCount() == 1 && table->topLevelItem(0)->text(0) == "Computer");
    assert(table->topLevelItem(0)->text(1) == "1"
        && table->topLevelItem(0)->text(2).toUInt() == firstPlan.orders.orders.size());
    auto* goals = journal->findChild<QTreeWidget*>("computerGoalsTable");
    assert(goals && goals->topLevelItemCount() == int(firstPlan.nextState.fleetGoals.size()));
    assert(goals->topLevelItem(0)->text(2) == "Survey and return");
    capture(journal, "computer-decisions.png"); journal->close();
    auto military = host;
    military.state.players[1].computer.fleetGoals.front().kind = ComputerFleetGoalKind::Invade;
    const auto militaryPath = directory.filePath("military-ui.suns");
    assert(write_save_game_file(militaryPath, military, error));
    MainWindow militaryWindow;
    assert(MainWindowTestAccess::load(militaryWindow, militaryPath));
    MainWindowTestAccess::journal(militaryWindow); app.processEvents();
    auto* militaryDialog = militaryWindow.findChild<QDialog*>("computerDecisionsDialog");
    auto* militaryGoals = militaryDialog->findChild<QTreeWidget*>("computerGoalsTable");
    assert(militaryGoals->topLevelItem(0)->text(2) == "Land assault troops");
    capture(militaryDialog, "computer-military-goals.png");

    // Resuming a saved host neither repeats bootstrap nor bypasses a missing human.
    assert(MainWindowTestAccess::load(window, hostPath));
    assert(MainWindowTestAccess::snapshot(window).state.players[1].computer == host.state.players[1].computer);
    MainWindowTestAccess::endTurn(window);
    assert(MainWindowTestAccess::snapshot(window).state.turn == 2);
    MainWindowTestAccess::receive(window, {3, {}});
    MainWindowTestAccess::endTurn(window);
    assert(MainWindowTestAccess::snapshot(window).state.players[1].computer.fleetGoals == firstPlan.nextState.fleetGoals);
    MainWindowTestAccess::restart(window);
    host = MainWindowTestAccess::snapshot(window);
    assert(host.state.turn == 1 && host.state.players[1].controller == PlayerController::Computer);
    assert(host.state.players[2].controller == PlayerController::Human && !host.state.players[1].computer.initialized);

    create_campaign(window, false);
    assert(MainWindowTestAccess::turnButton(window)->text().contains("computer empires"));
    MainWindowTestAccess::exportTurns(window); // No folder dialog and no AI turn files.
    assert(window.statusBar()->currentMessage().contains("No remote human"));
    MainWindowTestAccess::endTurn(window);
    assert(MainWindowTestAccess::snapshot(window).state.turn == 2);

    MainWindow recipient;
    assert(MainWindowTestAccess::load(recipient, remotePath));
    MainWindowTestAccess::journal(recipient);
    assert(!recipient.findChild<QDialog*>("computerDecisionsDialog"));
    return 0;
}
