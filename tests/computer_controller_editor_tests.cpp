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

    QFile file(path); assert(file.open(QIODevice::ReadOnly));
    const auto original = file.readAll(); file.close();
    QByteArray marker; QDataStream signature(&marker, QIODevice::WriteOnly); signature << quint32{0x4354524Cu};
    const auto controllerOffset = original.lastIndexOf(marker);
    assert(controllerOffset > 0);
    // CTRL header, then the first human's empty record, then computer 2.
    const auto roleOffset = controllerOffset + 8 + 10 + 4;
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
    auto bad = save; bad.state.players[1].computer.decisions.back().turn = save.state.turn;
    assert(!write_save_game_file(path, bad, error));
    bad = save; bad.state.players[0].computer = bad.state.players[1].computer;
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

    MainWindowTestAccess::receive(window, {3, {}}); // An explicit human pass is still required.
    assert(MainWindowTestAccess::turnButton(window)->text().contains("1/1 human orders"));
    MainWindowTestAccess::endTurn(window);
    host = MainWindowTestAccess::snapshot(window);
    assert(host.state.turn == 2 && host.inbox.empty());
    assert(host.state.players[1].computer.initialized);
    assert(host.state.players[1].technology.researchAllocationPercent == 20);
    assert(host.state.players[0].technology.researchAllocationPercent == 0);
    assert(host.state.players[1].computer.decisions.size() == 1);
    assert(host.state.players[1].computer.decisions[0].orderCount == 1);
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
    assert(table->topLevelItem(0)->text(1) == "1" && table->topLevelItem(0)->text(2) == "1");
    capture(journal, "computer-decisions.png"); journal->close();

    // Resuming a saved host neither repeats bootstrap nor bypasses a missing human.
    assert(MainWindowTestAccess::load(window, hostPath));
    assert(MainWindowTestAccess::snapshot(window).state.players[1].computer == host.state.players[1].computer);
    MainWindowTestAccess::endTurn(window);
    assert(MainWindowTestAccess::snapshot(window).state.turn == 2);
    MainWindowTestAccess::receive(window, {3, {}});
    MainWindowTestAccess::endTurn(window);
    assert(MainWindowTestAccess::snapshot(window).state.players[1].computer.decisions.back().orderCount == 0);
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
