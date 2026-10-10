#include "main_window.hpp"
#include "save_game.hpp"
#include "suns/combat.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QEventLoop>
#include <QGraphicsView>
#include <QListWidget>
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
