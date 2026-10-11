#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/combat.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDialog>
#include <QEventLoop>
#include <QGraphicsView>
#include <QListWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QSpinBox>
#include <QToolTip>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& window, const TurnResult& result)
    {
        window.state_ = result.state;
        window.pendingOrders_ = {1, {}};
        window.selection_ = {};
        window.resetTurnMessages();
        window.appendTurnMessages(result.events);
        window.rebuildScene();
    }
};
} // namespace suns

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    assert(dir.isValid());
    QCoreApplication::setOrganizationName("SunsCombatTests");
    QCoreApplication::setApplicationName("Battle report");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    using namespace suns;
    using C = ShipComponentType;
    auto state = make_demo_game();
    state.planets.clear(); state.fleets.clear(); state.orbitalStations.clear();
    state.stars = {{1, "Arena", {80, 120}}};
    state.wormholes.clear(); state.wormholeRules.spawnChancePerTurn = 0;
    state.players.push_back({2, "Opponent"});
    state.shipDesigns.push_back({10, 1, "Gun <one>", ShipHullType::Destroyer, {C::QuickJump5, C::AntiMatterPulverizer}});
    state.shipDesigns.push_back({20, 2, "Gun two", ShipHullType::Destroyer, {C::QuickJump5, C::AntiMatterPulverizer}});
    state.nextShipDesignId = 21;
    state.nextFleetId = 3;
    for (PlayerId p : {1, 2}) {
        Fleet fleet;
        fleet.id = p; fleet.owner = p; fleet.design = p * 10; fleet.name = "Fleet " + std::to_string(p);
        fleet.position = {80, 120}; fleet.ships = {{p * 10, 1}};
        fleet.telemetry.observedTurn = 1; fleet.telemetry.position = fleet.position; fleet.telemetry.ships = fleet.ships;
        state.fleets.push_back(fleet);
    }
    const auto result = TurnProcessor{}.process_with_events(state, {});
    assert(result.state.fleets.empty());
    MainWindow window;
    window.resize(1200, 850);
    window.show();
    MainWindowTestAccess::install(window, result);
    auto* dock = window.findChild<QDockWidget*>("turnMessagesDock");
    auto* filter = window.findChild<QComboBox*>("turnMessageTypeFilter");
    auto* list = window.findChild<QListWidget*>("turnMessagesList");
    auto* body = window.findChild<QTextBrowser*>("turnMessageBody");
    assert(dock && filter && list && body);
    dock->setFloating(true);
    dock->resize(1000, 720);
    dock->show();
    filter->setCurrentIndex(filter->findText("Combat"));
    assert(list->count() == 1);
    list->setCurrentRow(0);
    app.processEvents();
    const auto text = body->toPlainText();
    if (!text.contains("Mutual destruction") || !text.contains("Gun <one>")) std::cerr << text.toStdString() << '\n';
    assert(text.contains("Mutual destruction") && text.contains("Gun <one>"));
    assert(text.contains("Volley log") && text.contains("Anti-Matter Pulverizer"));
    assert(!text.contains("Natural wormholes"));
    auto* locate = window.findChild<QPushButton*>("showTurnMessageOnMap");
    assert(locate && locate->isEnabled());
    QEventLoop settled;
    QTimer::singleShot(30, &settled, &QEventLoop::quit);
    settled.exec();
    // Use the supported maximum zoom so scene-edge clamping cannot prevent centering.
    window.findChild<QGraphicsView*>()->setTransform(QTransform::fromScale(8, 8));
    locate->click();
    app.processEvents();
    auto* map = window.findChild<QGraphicsView*>();
    const auto center = map->mapToScene(map->viewport()->rect().center());
    if (std::abs(center.x() - 80) >= 2) std::cerr << "center " << center.x() << "," << center.y()
        << " scale " << map->transform().m11() << " scene " << map->sceneRect().left() << "," << map->sceneRect().right() << '\n';
    assert(std::abs(center.x() - 80) < 2 && std::abs(center.y() - 120) < 2);
    dock->grab().save("/tmp/suns-combat-report.png");

    window.installEmpireHistory();
    auto* metric = window.findChild<QComboBox*>("historyMetric");
    auto* scope = window.findChild<QComboBox*>("historyScope");
    auto* compare = window.findChild<QComboBox*>("historyCompare");
    auto* chart = window.findChild<QWidget*>("empireHistoryChart");
    auto* summary = window.findChild<QLabel*>("historySummary");
    auto* history = window.findChild<QDialog*>("empireHistoryDialog");
    assert(metric && scope && compare && chart && summary && history);
    history->show(); app.processEvents();
    metric->setCurrentIndex(metric->findText("Military losses (ships/year)"));
    assert(!scope->isEnabled() && !compare->isEnabled());
    assert(scope->currentData().toUInt() == 0);
    assert(summary->text().contains("report receipt year"));
    auto hoverYear = [&](bool latest) {
        const QPointF point(latest ? chart->width() - 18 : 72, chart->height() / 2);
        QMouseEvent hover(QEvent::MouseMove, point, chart->mapToGlobal(point.toPoint()),
            Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(chart, &hover);
        app.processEvents();
        return QToolTip::text();
    };
    const auto losses = hoverYear(true);
    assert(losses.contains("Own ships lost: 1"));
    assert(losses.contains("Opposing ships lost (confirmed): 1"));
    assert(hoverYear(false).contains("No battle statistics record"));
    history->grab().save("/tmp/suns-battle-history.png");
    metric->setCurrentIndex(metric->findText("Battle reports (reports/year)"));
    assert(hoverYear(true).contains("Reports received: 1"));
    auto* firstYear = window.findChild<QSpinBox*>("historyFirstTurn");
    assert(firstYear);
    firstYear->setValue(2);
    assert(hoverYear(false).contains("Reports received: 1"));
    metric->setCurrentIndex(0);
    assert(scope->isEnabled() && compare->isEnabled());

    auto operationalState = result.state;
    for (const auto kind : {OperationalLossKind::Ground, OperationalLossKind::Bombardment, OperationalLossKind::Mines}) {
        PendingPlayerReport packet; packet.planet = 1; packet.fleet = 1;
        packet.kind = kind == OperationalLossKind::Ground ? PlayerReportKind::GroundInvasionLost
            : kind == OperationalLossKind::Bombardment ? PlayerReportKind::Bombardment : PlayerReportKind::MineStrike;
        packet.observedTurn = 2; packet.deliveryTurn = 3;
        packet.losses = OperationalLossReport{kind,kind == OperationalLossKind::Ground ? 9'007'199'254'740'993ULL : 100ULL,
            kind == OperationalLossKind::Mines ? 0ULL : 200ULL,
            kind == OperationalLossKind::Bombardment ? 3ULL : 0ULL,
            kind == OperationalLossKind::Bombardment ? 4ULL : 0ULL,
            kind == OperationalLossKind::Mines ? 2ULL : 0ULL};
        operationalState.players[0].pendingPlayerReports.push_back(packet);
    }
    MainWindowTestAccess::install(window, TurnProcessor{}.process_with_events(operationalState,{}));
    firstYear->setValue(1);
    for (const auto name : {"Ground losses (colonists/year)", "Bombardment losses (colonists/year)",
        "Bombardment losses (installations/year)", "Mine losses (ships/year)", "Operational reports (reports/year)"}) {
        const auto index = metric->findText(name); assert(index >= 0);
        metric->setCurrentIndex(index); app.processEvents();
        assert(!scope->isEnabled() && !compare->isEnabled());
        assert(summary->text().contains("report receipt year"));
        assert(hoverYear(false).contains("No operational loss record"));
        const auto text = hoverYear(true);
        if (index == 16) assert(text.contains("Own colonists lost: 9007199254740993"));
        if (index == 17) assert(text.contains("Opposing colonists lost (confirmed): 200"));
        if (index == 18) assert(text.contains("Own installations lost: 3") && text.contains("Opposing installations lost (confirmed): 4"));
        if (index == 19) assert(text.contains("Own ships lost to mines: 2"));
        if (index == 20) assert(text.contains("Ground reports: 1") && text.contains("Bombardment reports: 1") && text.contains("Mine-strike reports: 1"));
        history->resize(740,440); app.processEvents();
        history->grab().save(QString("/tmp/suns-operational-history-%1.png").arg(index));
    }
    metric->setCurrentIndex(0);
    assert(scope->isEnabled() && compare->isEnabled());

    SaveGameData save;
    save.campaignId = 17; save.turnToken = 18;
    save.state = result.state; save.mode = SessionMode::Host; save.playerTokens = {{1, 18}, {2, 19}};
    save.pendingOrders = {1, {}}; save.strategicMessages = result.events;
    QString error;
    const auto path = dir.filePath("battle.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData loaded;
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.strategicMessages.size() == 2 && loaded.strategicMessages.front().battle);
    assert(loaded.strategicMessages.front().battle->shots.size() == 2);
    assert(write_save_game_file(path, make_player_turn(loaded, 1), error));
    assert(read_save_game_file(path, loaded, error));
    assert(loaded.strategicMessages.size() == 1 && loaded.strategicMessages.front().recipient == 1);
    // Invalid historical shot references must be rejected, not rendered or used.
    loaded.strategicMessages.front().battle->shots.front().target = 100000;
    assert(write_save_game_file(path, loaded, error));
    assert(!read_save_game_file(path, save, error));
}
