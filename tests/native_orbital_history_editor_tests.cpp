#include "main_window.hpp"
#include "native_orbital_fixture.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QImage>
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
    static const GameState& state(const MainWindow& w) { return w.state_; }
};
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory; QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    {
        suns::SaveGameData legacy; QString error;
        assert(suns::read_save_game_file(QString::fromUtf8(SUNS_SOURCE_DIR "/tests/fixtures/native-mining-v117.suns"), legacy, error));
        suns::MainWindow archived;
        suns::MainWindowTestAccess::installHistory(archived, legacy.state);
        auto* metric = archived.findChild<QComboBox*>("historyMetric");
        auto* scope = archived.findChild<QComboBox*>("historyScope");
        auto* comparison = archived.findChild<QComboBox*>("historyCompare");
        assert(metric && scope && comparison); metric->setCurrentIndex(10);
        assert(scope->findData(quint32{2}) == -1 && comparison->findData(quint32{2}) == -1);
        const auto& raw = suns::MainWindowTestAccess::state(archived).players.front().history.back();
        assert(!raw.remoteExtractionRecorded && raw.remoteExtraction.ironium == 8 && raw.remoteMineHistory.size() == 1);
        suns::MainWindowTestAccess::installHistory(archived, suns::make_player_view(legacy.state, 1).state);
        assert(scope->findData(quint32{2}) == -1 && comparison->findData(quint32{2}) == -1);
        const auto received = suns::TurnProcessor{}.process(legacy.state, {});
        suns::MainWindowTestAccess::installHistory(archived, received);
        assert(scope->findData(quint32{2}) >= 0 && comparison->findData(quint32{2}) >= 0);
    }
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
    assert(row.population == state.planets.front().nativeOrbit.population + 10'000);
    assert(row.minerals.ironium == state.planets.front().minerals.ironium + 100);
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
        assert(!QToolTip::text().contains(QString("Population: %1").arg(row.population)));
        QToolTip::hideText();
    }
    scope->setCurrentIndex(scope->findData(quint32(2))); app.processEvents();
    assert(!summary->text().contains("Reports from different years"));
    if (app.arguments().contains("--capture")) {
        scope->setCurrentIndex(scope->findData(quint32(0))); app.processEvents();
        dialog->grab().save("/tmp/suns-native-asynchronous-history.png");
    }
    // An ordinary empire can have a confirmed event without any current
    // fleet-damage measurement. Rendering that event and the year labels
    // must remain independent of the availability of numeric series.
    auto ordinary = suns::make_demo_game();
    ordinary.fleets.resize(1);
    ordinary.fleets.front().position = {420, 0};
    ordinary.fleets.front().destination.reset();
    ordinary.players.front().history.clear();
    suns::record_empire_turn_statistics(ordinary);
    assert(ordinary.players.front().history.back().fleetHistory.empty());
    const auto renderDamage = [&] {
        suns::MainWindowTestAccess::installHistory(window, ordinary);
        metric->setCurrentIndex(13);
        scope->setCurrentIndex(scope->findData(quint32(0)));
        app.processEvents();
        return chart->grab().toImage();
    };
    const auto withoutEvent = renderDamage();
    ordinary.players.front().history.back().milestones.push_back({901, ordinary.turn,
        suns::HistoryMilestoneKind::ResearchCompleted, 0, suns::ResearchField::Energy, 2});
    const auto withEvent = renderDamage();
    assert(withEvent != withoutEvent);
    if (app.arguments().contains("--capture")) dialog->grab().save("/tmp/suns-history-empty-with-event.png");
    ordinary.players.front().history.back().milestones.clear();
    ordinary.players.front().history.back().turn = ++ordinary.turn;
    const auto nextYear = renderDamage();
    assert(nextYear != withoutEvent);
}
