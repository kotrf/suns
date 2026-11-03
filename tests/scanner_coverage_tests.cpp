#include "main_window.hpp"
#include "suns/communications.hpp"
#include <QAction>
#include <QApplication>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QStyleOptionGraphicsItem>
#include <QTemporaryDir>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w, const GameState& s) {
        w.state_ = s; w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.selection_.star = 1; w.rebuildScene();
    }
};
}
namespace {
using namespace suns;
QGraphicsPathItem* layer(MainWindow& w, const char* kind) {
    QGraphicsPathItem* result = nullptr;
    for (auto* item : w.routeProgramScene()->items()) if (item->data(0).toString() == kind) {
        assert(!result); result = dynamic_cast<QGraphicsPathItem*>(item); assert(result);
        assert(result->pen().style() == Qt::NoPen && result->acceptedMouseButtons() == Qt::NoButton);
        assert(result->path().fillRule() == Qt::WindingFill);
    }
    return result;
}
QImage paintCoverage(MainWindow& w) {
    QImage image(1200, 600, QImage::Format_ARGB32_Premultiplied); image.fill(QColor(8, 12, 18));
    QPainter painter(&image); painter.translate(700, 300);
    QStyleOptionGraphicsItem option;
    for (const auto* kind : {"scannerCoverage", "penetratingScannerCoverage"})
        if (auto* item = layer(w, kind)) item->paint(&painter, &option, nullptr);
    return image;
}
GameState fixture() {
    auto s = make_demo_game();
    s.stars[0].position = {-500, 0}; s.planets[0].radar = PlanetaryRadarLevel::Full;
    s.shipDesigns.push_back({90, 1, "Penetrating scout", ShipHullType::Scout,
        {ShipComponentType::QuickJump5, ShipComponentType::DolphinScanner}});
    s.fleets.resize(1); auto& f = s.fleets.front();
    f.design = 90; f.ships = {{90, 1}}; f.position = {0, 0};
    f.electronics = {}; f.telemetry = {}; f.telemetry.observedTurn = s.turn;
    f.telemetry.position = f.position; f.telemetry.ships = f.ships;
    f.telemetry.electronics = f.electronics;
    auto second = f; second.id = 2; second.position = {120, 0}; second.telemetry.position = second.position;
    auto duplicate = f; duplicate.id = 3;
    s.fleets.push_back(second); s.fleets.push_back(duplicate); s.nextFleetId = 4;
    s.players[0].sensorStatus.clear();
    return s;
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv); QTemporaryDir dir; assert(dir.isValid());
    QCoreApplication::setOrganizationName("SunsScannerCoverageTests");
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir.path());
    MainWindow w; w.installUiPolish(); auto s = fixture(); MainWindowTestAccess::install(w, s);
    auto* ordinary = layer(w, "scannerCoverage"); auto* penetrating = layer(w, "penetratingScannerCoverage");
    assert(ordinary && penetrating);
    assert(ordinary->path().contains({-500, 0})); // Planetary radar shares ordinary coverage.
    assert(ordinary->path().contains({-219, 0}) && !ordinary->path().contains({-221, 0}));
    assert(penetrating->path().contains({-99, 0}) && !penetrating->path().contains({-101, 0}));
    assert(penetrating->path().contains({219, 0}) && !penetrating->path().contains({221, 0}));
    assert(ordinary->path().contains({60, 150}) && penetrating->path().contains({60, 0}));
    const auto image = paintCoverage(w);
    const auto singleOrdinary = image.pixelColor(700 - 180, 300);
    assert(singleOrdinary == image.pixelColor(700 + 60, 300 + 150)); // Two normal discs, no stronger tint.
    const auto singlePenetrating = image.pixelColor(700 - 50, 300);
    assert(singlePenetrating == image.pixelColor(700 + 60, 300)); // Two penetrating discs, same tint.
    assert(singlePenetrating.blue() > singleOrdinary.blue());

    // Hidden enemy platforms and unreported own emissions changes must not
    // alter received coverage. No host interference queries are used here.
    auto hidden = s; hidden.players.push_back({2, "Hidden"});
    auto enemy = hidden.fleets[0]; enemy.id = 99; enemy.owner = 2; enemy.position = {3000, 0};
    enemy.telemetry.position = enemy.position; hidden.fleets.push_back(enemy);
    hidden.fleets[0].electronics.mode = EmissionMode::Passive;
    MainWindowTestAccess::install(w, hidden); assert(paintCoverage(w) == image);

    // Received interference contracts both radii equally. It is not read from
    // current hidden jammer components. A scanner without penetration adds none.
    auto jammed = s; jammed.fleets.resize(1);
    SensorStatus status; status.fleet = 1; status.interferencePercent = 50;
    status.nominalRange = 220; status.activeRange = 110;
    jammed.players[0].sensorStatus = {status};
    MainWindowTestAccess::install(w, jammed);
    assert(layer(w, "scannerCoverage")->path().contains({109, 0}));
    assert(!layer(w, "scannerCoverage")->path().contains({111, 0}));
    assert(layer(w, "penetratingScannerCoverage")->path().contains({49, 0}));
    assert(!layer(w, "penetratingScannerCoverage")->path().contains({51, 0}));
    jammed.shipDesigns.back().components.back() = ShipComponentType::MoleScanner;
    MainWindowTestAccess::install(w, jammed); assert(!layer(w, "penetratingScannerCoverage"));

    for (auto& f : s.fleets) { f.electronics.mode = EmissionMode::Passive; f.telemetry.electronics = f.electronics; }
    MainWindowTestAccess::install(w, s);
    assert(!layer(w, "penetratingScannerCoverage"));
    assert(!layer(w, "scannerCoverage")->path().contains({0, 0})); // Only distant colony radar remains.
    auto* sensors = w.findChild<QAction*>("showSensorRangesAction"); assert(sensors && sensors->isChecked());
    sensors->trigger(); assert(!layer(w, "scannerCoverage") && !layer(w, "penetratingScannerCoverage"));
    sensors->trigger(); assert(layer(w, "scannerCoverage"));
}
