#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>

#include <cassert>
#include <cmath>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.state_.turn = 9;
        w.state_.stars.resize(3);
        w.state_.stars[0].position = {0, 0};
        w.state_.stars[1].position = {40, 0};
        w.state_.stars[1].name = "Kochab";
        w.state_.stars[2].position = {80, 0};
        w.state_.stars[2].name = "Perrin";
        w.state_.fleets.front().position = {20, 20};
        w.state_.fleets.front().telemetry = {};
        auto own = w.state_.fleets.front();
        own.id = 2;
        own.name = "Orbital scout";
        own.position = {40, 0};
        own.fuel = 17;
        w.state_.fleets.push_back(own);
        own.id = 3;
        own.name = "Orbital group";
        own.ships = {{kScoutDesignId, 10}};
        own.fuel = 200;
        w.state_.fleets.push_back(own);
        own.id = 4;
        own.owner = 2;
        own.name = "Other empire";
        w.state_.fleets.push_back(own);
        own.id = 5;
        own.owner = 1;
        own.name = "Delayed scout";
        own.position = {3000, 0};
        own.fuel = 500;
        own.ships = {{kScoutDesignId, 1}};
        own.telemetry.observedTurn = 2;
        own.telemetry.position = {80, 0};
        own.telemetry.fuel = 9;
        own.telemetry.ships = own.ships;
        w.state_.fleets.push_back(own);
        w.state_.nextFleetId = 6;
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.selection_.fleet = 1;
        w.selection_.star = 1;
        w.rebuildScene();
        emit w.routeProgramContextChanged(true);
    }
    static StarId star(const MainWindow& w) { return w.selection_.star.value_or(0); }
    static void queue(MainWindow& w)
    {
        w.pendingOrders_.orders.push_back(RenameFleetOrder{2, "Renamed scout"});
        w.pendingOrders_.orders.push_back(MoveFleetOrder{2, {80, 0}, 5});
        w.rebuildScene();
    }
    static void nextSnapshot(MainWindow& w)
    {
        ++w.state_.turn;
        w.state_.fleets.erase(w.state_.fleets.begin() + 2); // Fleet 3 disappears.
        auto fleet = w.state_.fleets.front();
        fleet.id = 6;
        fleet.name = "New fleet";
        w.state_.fleets.push_back(fleet);
        w.rebuildScene();
    }
    static void uncertainTransit(MainWindow& w)
    {
        auto contact = w.state_.fleets.front();
        contact.telemetry.observedTurn = 9;
        contact.telemetry.position = contact.position;
        contact.telemetry.fuel = contact.fuel;
        w.state_.wormholeTransits.push_back({contact, 19, 9, 12, 17});
        w.state_.fleets.front().position = {3000, 999};
        w.state_.fleets.front().fuel = 0;
        w.rebuildScene();
    }
    static void missing(MainWindow& w)
    {
        w.state_.fleets.erase(w.state_.fleets.begin());
        w.rebuildScene();
    }
    static void empty(MainWindow& w)
    {
        w.state_.fleets.clear();
        w.state_.wormholeTransits.clear();
        w.pendingOrders_ = {1, {}};
        w.rebuildScene();
    }
};
} // namespace suns

static void settle()
{
    QApplication::processEvents();
    QApplication::processEvents();
}

static void clickSystem(suns::MainWindow& window, QPointF position)
{
    auto* view = window.findChild<QGraphicsView*>();
    const auto point = view->mapFromScene(position);
    const auto global = view->viewport()->mapToGlobal(point);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(point), QPointF(global),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point), QPointF(global),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &release);
    settle();
}

static QTreeWidgetItem* row(QTreeWidget* table, suns::FleetId id)
{
    for (int i = 0; i < table->topLevelItemCount(); ++i)
        if (auto* item = table->topLevelItem(i); item->data(0, Qt::UserRole).toUInt() == id) return item;
    return nullptr;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    using namespace suns;
    MainWindow window;
    window.installDeferredMapSelectionHandler();
    attachRouteProgramDock(window);
    window.installUiPolish();
    window.installFleetReadabilityPolish();
    window.installPanelLayoutFixes();
    MainWindowTestAccess::setup(window);
    window.resize(1300, 840);
    window.show();
    settle();

    auto* source = window.findChild<QComboBox*>("routeSourceFleetCombo");
    auto* view = window.findChild<QGraphicsView*>();
    view->resetTransform();
    view->scale(2, 2);
    view->centerOn(40, 0);
    clickSystem(window, {40, 0});
    assert(MainWindowTestAccess::star(window) == 2);
    assert(window.selectedFleetForRouteProgram() == 1); // Keep the source for route editing.
    assert(source->itemData(1).toUInt() == 2 && source->itemData(2).toUInt() == 3);
    assert(!(source->model()->flags(source->model()->index(0, 0)) & Qt::ItemIsEnabled));
    assert(source->findData(quint32(4)) == -1); // Foreign fleets are not command sources.
    assert(window.findChild<QLabel*>("fleetSystemContext")->text().contains("Kochab: 2"));
    if (app.arguments().contains("--capture")) {
        source->showPopup();
        settle();
        source->view()->window()->grab().save("/tmp/suns-fleet-selector.png");
        source->hidePopup();
    }
    source->setCurrentIndex(source->findData(quint32(2)));
    settle();
    assert(window.selectedFleetForRouteProgram() == 2);
    assert(window.findChild<QProgressBar*>("fleetFuelBar")->format().contains("17.0"));
    clickSystem(window, {80, 0});
    assert(window.selectedFleetForRouteProgram() == 2);
    assert(source->itemData(1).toUInt() == 5); // Confirmed position, not hidden host position 3000.

    auto* menuAction = window.findChild<QAction*>("fleetSummaryAction");
    auto* toolAction = window.findChild<QAction*>("fleetSummaryToolAction");
    assert(menuAction && toolAction);
    assert(window.findChild<QToolBar*>("dialogToolbar")->actions().contains(toolAction));
    bool inMenu = false;
    for (auto* menu : window.findChildren<QMenu*>()) inMenu |= menu->actions().contains(menuAction);
    assert(inMenu);
    menuAction->trigger();
    settle();
    auto* dialog = window.findChild<QDialog*>("fleetSummaryDialog");
    assert(dialog && dialog->isVisible() && !dialog->isModal());
    auto* table = dialog->findChild<QTreeWidget*>("fleetSummaryTable");
    auto* search = dialog->findChild<QLineEdit*>("fleetSummarySearch");
    auto* show = dialog->findChild<QPushButton*>("fleetSummaryShowButton");
    assert(table->topLevelItemCount() == 4 && !row(table, 4));
    assert(row(table, 5)->text(1).contains("Perrin"));
    assert(row(table, 5)->text(3).startsWith("9.0 /"));
    assert(row(table, 5)->text(6).contains("7 yr old"));
    assert(row(table, 5)->text(6).contains("Delayed"));
    assert(row(table, 2)->toolTip(2).contains("Scout"));
    table->sortByColumn(2, Qt::AscendingOrder);
    assert(table->topLevelItem(3)->data(0, Qt::UserRole).toUInt() == 3); // 10 ships sort after 1.
    search->setText("kochab");
    assert(table->topLevelItemCount() == 2);
    search->clear();
    table->setCurrentItem(row(table, 3));
    assert(show->isEnabled());
    show->click();
    settle();
    assert(window.selectedFleetForRouteProgram() == 3 && MainWindowTestAccess::star(window) == 2);
    assert(source->currentData().toUInt() == 3);
    const auto center = view->mapToScene(view->viewport()->rect().center());
    assert(std::abs(center.x() - 40) < 2 && std::abs(center.y()) < 2);
    table->setCurrentItem(row(table, 2));
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(table, &enter);
    settle();
    assert(window.selectedFleetForRouteProgram() == 2);
    table->setCurrentItem(row(table, 3));
    show->click();
    toolAction->trigger();
    assert(window.findChildren<QDialog*>("fleetSummaryDialog").size() == 1);
    MainWindowTestAccess::queue(window);
    assert(row(table, 2)->text(0).contains("Renamed scout"));
    assert(row(table, 2)->text(5).contains("Queued: Perrin"));
    assert(row(table, 2)->text(5).contains("W5"));
    if (app.arguments().contains("--capture")) dialog->grab().save("/tmp/suns-fleet-summary.png");
    MainWindowTestAccess::nextSnapshot(window);
    assert(!row(table, 3) && row(table, 6));
    assert(source->findData(quint32(3)) == -1 && source->findData(quint32(6)) >= 0);
    MainWindowTestAccess::uncertainTransit(window);
    assert(row(table, 1) && row(table, 1)->text(5).contains("WH transit"));
    QStringList contact;
    for (int column = 0; column < table->columnCount(); ++column) contact << row(table, 1)->text(column);
    MainWindowTestAccess::missing(window);
    for (int column = 0; column < table->columnCount(); ++column)
        assert(row(table, 1)->text(column) == contact[column]); // No hidden exit or survival leak.
    MainWindowTestAccess::empty(window);
    assert(table->topLevelItemCount() == 0 && !show->isEnabled());
    assert(dialog->findChild<QLabel*>("fleetSummaryTotals")->text().contains("0 of 0"));
    assert(source->currentIndex() == -1);
    assert(!source->isEnabled());
    MainWindowTestAccess::setup(window); // Loading/replacing a game keeps the overview fresh.
    assert(table->topLevelItemCount() == 4 && row(table, 5));
    dialog->close();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    toolAction->trigger();
    settle();
    assert(window.findChild<QDialog*>("fleetSummaryDialog")->isVisible());
    window.close();
}
