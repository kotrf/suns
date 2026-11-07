#include "main_window.hpp"
#include "native_orbital_fixture.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolTip>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void installHistory(MainWindow& w, GameState state) {
        w.state_ = std::move(state); w.pendingOrders_ = {1, {}};
        w.openEmpireHistory();
    }
};
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory; QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    auto state = suns::test::asynchronous_native_unload_fixture();
    state = suns::TurnProcessor{}.process(state, {});
    state = suns::TurnProcessor{}.process(state, {});
    assert(state.turn == 3 && state.fleets.front().position.x == 145);
    suns::SaveGameData host; host.state = state; host.mode = suns::SessionMode::Host;
    host.campaignId = 117; host.turnToken = 17; host.playerTokens = {{1, 17}}; host.pendingOrders = {1, {}};
    const auto path = directory.filePath("asynchronous.sunsturn"); QString error;
    assert(suns::write_save_game_file(path, suns::make_player_turn(host, 1), error));
    suns::SaveGameData loaded; assert(suns::read_save_game_file(path, loaded, error));
    const auto& row = loaded.state.players.front().history.back();
    assert(!row.assetTotalsSynchronized && row.oldestAssetObservation == 1 && row.newestAssetObservation == 3);
    assert(row.population == 20'000 && row.minerals.ironium == 200);
    suns::MainWindow window;
    suns::MainWindowTestAccess::installHistory(window, loaded.state);
    auto* dialog = window.findChild<QDialog*>("empireHistoryDialog"); assert(dialog);
    auto* metric = dialog->findChild<QComboBox*>("historyMetric");
    auto* scope = dialog->findChild<QComboBox*>("historyScope");
    auto* summary = dialog->findChild<QLabel*>("historySummary");
    auto* chart = dialog->findChild<QWidget*>("empireHistoryChart");
    scope->setCurrentIndex(scope->findData(quint32(0)));
    dialog->findChild<QSpinBox*>("historyFirstTurn")->setValue(3);
    dialog->findChild<QSpinBox*>("historyLastTurn")->setValue(3);
    for (const auto kind : {0, 3}) {
        metric->setCurrentIndex(kind); app.processEvents();
        assert(summary->text().contains("Reports from different years"));
        const QPointF point(chart->width()-17, chart->height()/2);
        QMouseEvent hover(QEvent::MouseMove, point, chart->mapToGlobal(point.toPoint()),
            Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(chart, &hover);
        assert(QToolTip::text().contains("No synchronized total"));
        assert(!QToolTip::text().contains("Population: 20000"));
        QToolTip::hideText();
    }
    scope->setCurrentIndex(scope->findData(quint32(2))); app.processEvents();
    assert(!summary->text().contains("Reports from different years"));
    if (app.arguments().contains("--capture")) {
        scope->setCurrentIndex(scope->findData(quint32(0))); app.processEvents();
        dialog->grab().save("/tmp/suns-native-asynchronous-history.png");
    }
}
