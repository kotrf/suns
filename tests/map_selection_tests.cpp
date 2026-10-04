#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "star_item.hpp"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMouseEvent>
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
    assert(geometry.shape().contains({10, 0}));
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
        // Real discs can still overlap at large population-mode marker sizes.
        // Choose the nearest center rather than the scene stacking order.
        enlargeMarkers(window);
        click(window, {-75, -55}, Qt::LeftButton);
        assert(MainWindowTestAccess::selectedStar(window) == 2);
        enlargeMarkers(window);
        click(window, {-70, -55}, Qt::LeftButton);
        assert(MainWindowTestAccess::selectedStar(window) == 3);
        enlargeMarkers(window);
        click(window, {-75, -55}, Qt::LeftButton, QEvent::MouseButtonDblClick);
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
        click(window, {-70, -55}, Qt::RightButton);
        const auto& appended = std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).orders.front());
        assert(appended.queuedWaypoints.size() == 1);
        assert(same_position(appended.queuedWaypoints.back().destination, {-65, -55}));
    }
    // Fleet selection still resolves its own visible marker, not the nearby
    // colony, and survives the deferred redraw through the normal state API.
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
    click(window, fleetCenter, Qt::LeftButton);
    assert(window.selectedFleetForRouteProgram() == 1);
    assert(MainWindowTestAccess::selectionKind(window) == 2);
    window.close();
}
