#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "star_item.hpp"
#include "map_marker_items.hpp"

#include <QAction>
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsLineItem>
#include <QMouseEvent>
#include <QMenu>
#include <QPainterPath>
#include <QSettings>
#include <QTemporaryDir>

#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& window)
    {
        window.state_ = make_demo_game();
        window.state_.stars.resize(3);
        window.state_.planets.resize(3);
        window.state_.stars[1].name = "Kochab";
        window.state_.stars[1].position = {-80, -55};
        window.state_.stars[2].name = "Perrin";
        window.state_.stars[2].position = {-65, -55};
        window.selection_.star = 1;
        window.selection_.fleet = 1;
        window.pendingOrders_ = {1, {}};
        window.pendingDescriptions_.clear();
        window.rebuildScene();
        emit window.routeProgramContextChanged(true);
    }
    static void orbits(MainWindow& window, bool deepSpace = false)
    {
        setup(window);
        auto own = window.state_.fleets.front();
        own.id = 2;
        own.name = "Second scout";
        own.position = deepSpace ? Position{35, 30} : Position{-80, -55};
        own.telemetry.position = own.position;
        window.state_.fleets.push_back(own);
        own.id = 3;
        own.owner = 2;
        own.name = "Other empire";
        window.state_.fleets.push_back(own);
        if (!deepSpace) {
            own.id = 4;
            own.position = {-65, -55};
            own.telemetry.position = own.position;
            window.state_.fleets.push_back(own);
        }
        window.rebuildScene();
        emit window.routeProgramContextChanged(true);
    }
    static void deepFleet(MainWindow& window)
    {
        setup(window);
        window.state_.fleets.front().position = {20, 25};
        window.state_.fleets.front().telemetry.position = {20, 25};
        window.rebuildScene();
    }
    static void labels(MainWindow& window) { window.refreshMapLabels(); }
    static void dense(MainWindow& window)
    {
        setup(window);
        for (int i = 0; i < 64; ++i)
            window.state_.stars.push_back({static_cast<StarId>(i + 4), "Dense system " + std::to_string(i),
                {40.0 + (i % 8) * 15.0, -60.0 + (i / 8) * 15.0}, StarClass::White});
        window.rebuildScene();
    }
    static StarId selectedStar(MainWindow& window) { return window.selection_.star.value_or(0); }
    static QGraphicsView* view(MainWindow& window) { return window.view_; }
    static const PlayerOrders& orders(MainWindow& window) { return window.pendingOrders_; }
    static int selectionKind(MainWindow& window) { return window.currentDistanceSelectionKind_; }
};
} // namespace suns

static void click(suns::MainWindow& window, QPointF scenePosition, Qt::MouseButton button,
    QEvent::Type pressType = QEvent::MouseButtonPress)
{
    auto* view = suns::MainWindowTestAccess::view(window);
    const auto position = view->mapFromScene(scenePosition);
    assert(view->viewport()->rect().contains(position));
    const auto global = view->viewport()->mapToGlobal(position);
    QMouseEvent press(pressType, QPointF(position), QPointF(global),
        button, button, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(position), QPointF(global),
        button, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &release);
    QApplication::processEvents();
    QApplication::processEvents();
}

static void enlargeMarkers(suns::MainWindow& window)
{
    for (auto* item : window.routeProgramScene()->items()) {
        if (auto* star = dynamic_cast<suns::StarItem*>(item)) star->setVisualStyle(Qt::yellow, 2);
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settingsDirectory;
    assert(settingsDirectory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDirectory.path());
    using namespace suns;
    StarItem geometry(42, Qt::yellow, false, false);
    assert(geometry.shape().contains({0, 0}));
    assert(!geometry.shape().contains({30, 30}));
    geometry.setVisualStyle(Qt::yellow, 2);
    assert(geometry.shape().contains({6, 0}));
    assert(!geometry.shape().contains({0, 20}));
    MainWindow window;
    window.installDeferredMapSelectionHandler();
    attachRouteProgramDock(window);
    window.installUiPolish();
    MainWindowTestAccess::setup(window);
    window.resize(1300, 840);
    window.show();
    QApplication::processEvents();
    auto* view = MainWindowTestAccess::view(window);

    for (const auto zoom : {0.5, 1.0, 2.5}) {
        MainWindowTestAccess::setup(window);
        view->resetTransform();
        view->scale(zoom, zoom);
        view->centerOn(-72.5, -55);
        // The later-painted Perrin used to steal clicks on Kochab through its
        // invisible 76x76 bounding rectangle. Its label also overlaps Kochab's.
        for (int attempt = 0; attempt < 3; ++attempt) {
            click(window, {-80, -55}, Qt::LeftButton);
            assert(MainWindowTestAccess::selectedStar(window) == 2);
            click(window, {-65, -55}, Qt::LeftButton);
            assert(MainWindowTestAccess::selectedStar(window) == 3);
        }
        // Decorative glows, labels and scanner circles are not selectable.
        click(window, {-80, -40}, Qt::LeftButton);
        assert(MainWindowTestAccess::selectedStar(window) == 3);
        // Screen-sized discs overlap at low zoom. At every zoom, a point three
        // screen pixels toward the neighbor must still select the nearer star.
        enlargeMarkers(window);
        click(window, {-80 + 3 / zoom, -55}, Qt::LeftButton);
        assert(MainWindowTestAccess::selectedStar(window) == 2);
        enlargeMarkers(window);
        click(window, {-65 - 3 / zoom, -55}, Qt::LeftButton);
        assert(MainWindowTestAccess::selectedStar(window) == 3);
        enlargeMarkers(window);
        click(window, {-80 + 3 / zoom, -55}, Qt::LeftButton, QEvent::MouseButtonDblClick);
        assert(MainWindowTestAccess::selectedStar(window) == 2);
        if (zoom == 2.5 && app.arguments().contains("--capture"))
            view->viewport()->grab().save("/tmp/suns-selection-kochab.png");

        // Target picking must use the same geometry and retain the source fleet.
        assert(window.beginRouteProgramMapTargetPick());
        click(window, {-80, -55}, Qt::LeftButton);
        assert(!window.routeProgramMapTargetPickActive());
        assert(window.selectedFleetForRouteProgram() == 1);
        assert(MainWindowTestAccess::selectedStar(window) == 2);
        assert(window.beginRouteProgramMapTargetPick());
        click(window, {-80, -55}, Qt::LeftButton); // already-selected destination
        assert(!window.routeProgramMapTargetPickActive());

        MainWindowTestAccess::setup(window);
        click(window, {-80, -55}, Qt::RightButton);
        assert(MainWindowTestAccess::orders(window).orders.size() == 1);
        const auto& route = std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front());
        assert(same_position(route.destination, {-80, -55}));
        enlargeMarkers(window);
        click(window, {-65 - 3 / zoom, -55}, Qt::RightButton);
        const auto& appended = std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front());
        assert(appended.queuedWaypoints.size() == 1);
        assert(same_position(appended.queuedWaypoints.back().destination, {-65, -55}));
    }
    // Fleet selection still resolves its own visible marker, not the nearby
    // colony, and survives the deferred redraw through the normal state API.
    MainWindowTestAccess::deepFleet(window);
    view->resetTransform();
    view->centerOn(0, 0);
    QPointF fleetCenter;
    bool foundFleet = false;
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() != 2 || item->data(0).toUInt() != 1) continue;
        fleetCenter = item->sceneBoundingRect().center();
        foundFleet = true;
        break;
    }
    assert(foundFleet);
    assert(fleetCenter == QPointF(20, 25));
    click(window, fleetCenter, Qt::LeftButton);
    assert(window.selectedFleetForRouteProgram() == 1);
    assert(MainWindowTestAccess::selectionKind(window) == 2);
    // Occupied systems have one ring, regardless of fleet count. Clicking the
    // core keeps system selection; clicking the annulus opens a fleet picker.
    for (const auto zoom : {0.5, 1.0, 2.5, 8.0}) {
        MainWindowTestAccess::orbits(window);
        view->resetTransform();
        view->scale(zoom, zoom);
        view->centerOn(-80, -55);
        int rings = 0;
        int separateFleets = 0;
        for (auto* item : window.routeProgramScene()->items()) {
            if (item->data(1).toInt() == kMapItemOrbit) ++rings;
            if (item->data(1).toInt() == 2) ++separateFleets;
        }
        assert(rings == 3 && separateFleets == 0);
        click(window, {-80, -55}, Qt::LeftButton);
        assert(MainWindowTestAccess::selectedStar(window) == 2);
        assert(!window.findChild<QMenu*>("mapFleetPicker"));
        click(window, {-80, -55 - 10 / zoom}, Qt::LeftButton);
        auto* menu = window.findChild<QMenu*>("mapFleetPicker");
        assert(menu && menu->isVisible());
        QAction* second = nullptr;
        bool listedEnemy = false;
        for (auto* action : menu->actions()) {
            if (action->data().toUInt() == 2) second = action;
            if (action->data().toUInt() == 3) listedEnemy = true;
        }
        assert(second && listedEnemy);
        second->trigger();
        QApplication::processEvents();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        assert(window.selectedFleetForRouteProgram() == 2);
        assert(MainWindowTestAccess::selectionKind(window) == 2);

        MainWindowTestAccess::orbits(window);
        assert(window.beginRouteProgramMapTargetPick());
        click(window, {-80, -55 - 10 / zoom}, Qt::LeftButton);
        menu = window.findChild<QMenu*>("mapFleetPicker");
        assert(menu && menu->isVisible());
        for (auto* action : menu->actions()) if (action->data().toUInt() == 2) action->trigger();
        QApplication::processEvents();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        assert(!window.routeProgramMapTargetPickActive());
        assert(window.selectedFleetForRouteProgram() == 1);

        MainWindowTestAccess::orbits(window);
        click(window, {-80, -55 - 10 / zoom}, Qt::RightButton);
        assert(MainWindowTestAccess::orders(window).orders.size() == 1);
        const auto& move = std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front());
        assert(same_position(move.destination, {-80, -55}));
        assert(window.selectedFleetForRouteProgram() == 1);
        bool actualRouteOrigin = false;
        for (auto* item : window.routeProgramScene()->items()) {
            if (item->zValue() != -18) continue;
            if (auto* line = dynamic_cast<QGraphicsLineItem*>(item)) {
                assert(line->line().p1() == QPointF(0, 0));
                actualRouteOrigin = true;
            }
        }
        assert(actualRouteOrigin);
    }

    // Co-located deep-space fleets remain individually selectable and usable
    // as quick-route targets, with the original source fleet preserved.
    MainWindowTestAccess::orbits(window, true);
    view->resetTransform();
    view->centerOn(35, 30);
    click(window, {35, 30}, Qt::RightButton);
    auto* menu = window.findChild<QMenu*>("mapFleetPicker");
    assert(menu && menu->isVisible());
    for (auto* action : menu->actions()) if (action->data().toUInt() == 2) action->trigger();
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    assert(window.selectedFleetForRouteProgram() == 1);
    assert(std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front()).targetFleet == 2);

    QGraphicsSimpleTextItem* hoverLabel = nullptr;
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() != 2) continue;
        for (auto* child : item->childItems())
            if (auto* label = dynamic_cast<QGraphicsSimpleTextItem*>(child)) hoverLabel = label;
    }
    assert(hoverLabel && !hoverLabel->isVisible());
    const auto movePointer = [&](const QPoint& position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position),
            QPointF(view->viewport()->mapToGlobal(position)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &move);
        QApplication::processEvents();
    };
    const auto hoverPosition = view->mapFromScene(35, 30);
    movePointer(hoverPosition);
    assert(hoverLabel->isVisible());
    movePointer(hoverPosition + QPoint(40, 40));
    assert(!hoverLabel->isVisible());

    // Dense-map labels are placed without covering each other, and both glyphs
    // and text keep their screen dimensions when zoom changes.
    MainWindowTestAccess::dense(window);
    view->resetTransform();
    view->scale(0.5, 0.5);
    view->centerOn(0, 0);
    MainWindowTestAccess::labels(window);
    std::vector<QRectF> visibleLabels;
    int allLabels = 0;
    QGraphicsSimpleTextItem* selectedLabel = nullptr;
    StarItem* selectedMarker = nullptr;
    for (auto* item : window.routeProgramScene()->items()) {
        if (item->data(1).toInt() != kMapLabelStar) continue;
        ++allLabels;
        if (item->data(0).toUInt() == 1) {
            selectedLabel = dynamic_cast<QGraphicsSimpleTextItem*>(item);
            selectedMarker = dynamic_cast<StarItem*>(item->parentItem());
        }
        if (!item->isVisible()) continue;
        const auto area = item->deviceTransform(view->viewportTransform()).mapRect(item->boundingRect());
        for (const auto& previous : visibleLabels) assert(!area.intersects(previous));
        visibleLabels.push_back(area);
    }
    assert(selectedLabel && selectedLabel->isVisible() && selectedMarker);
    assert(!visibleLabels.empty() && visibleLabels.size() < static_cast<std::size_t>(allLabels));
    const auto labelSize = selectedLabel->deviceTransform(view->viewportTransform()).mapRect(selectedLabel->boundingRect()).size();
    const auto markerSize = selectedMarker->deviceTransform(view->viewportTransform()).mapRect(selectedMarker->boundingRect()).size();
    view->scale(8, 8);
    assert(selectedLabel->deviceTransform(view->viewportTransform()).mapRect(selectedLabel->boundingRect()).size() == labelSize);
    assert(selectedMarker->deviceTransform(view->viewportTransform()).mapRect(selectedMarker->boundingRect()).size() == markerSize);
    if (app.arguments().contains("--capture")) {
        window.installMapDisplayModes();
        window.installPlanetPolish();
        window.installProductionQueue();
        window.installFleetReadabilityPolish();
        window.installFleetPortraitPolish();
        window.installMiningInfrastructure();
        window.installCommunicationStatus();
        window.installTurnMessages();
        window.installResearch();
        window.installEmpireHistory();
        window.installWormholes();
        window.installPanelLayoutFixes();
        MainWindowTestAccess::orbits(window);
        window.resetPanelLayout();
        window.resize(1300, 840);
        for (int i = 0; i < 8; ++i) QApplication::processEvents();
        view->resetTransform();
        view->scale(2.5, 2.5);
        view->centerOn(-30, -20);
        for (int i = 0; i < 8; ++i) QApplication::processEvents();
        MainWindowTestAccess::labels(window);
        window.grab().save("/tmp/suns-compact-workspace.png");
        click(window, {-80, -59}, Qt::LeftButton);
        menu = window.findChild<QMenu*>("mapFleetPicker");
        assert(menu && menu->isVisible());
        menu->grab().save("/tmp/suns-compact-picker.png");
        menu->close();
        QApplication::processEvents();
        window.resize(1100, 720);
        window.resetPanelLayout();
        for (int i = 0; i < 8; ++i) QApplication::processEvents();
        view->resetTransform();
        view->scale(1.5, 1.5);
        view->centerOn(-30, -20);
        for (int i = 0; i < 8; ++i) QApplication::processEvents();
        window.grab().save("/tmp/suns-compact-workspace-small.png");
    }
    window.close();
}
