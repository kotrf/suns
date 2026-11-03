#include "main_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QPointer>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.state_.turn = 5;
        w.state_.players.resize(1);
        auto computer = w.state_.players.front();
        computer.id = 2;
        computer.name = "Computer";
        computer.controller = PlayerController::Computer;
        computer.computer.decisions = {{5, 1, "Initial decision"}};
        computer.computer.fleetGoals = {{1, ComputerFleetGoalKind::Survey, w.state_.stars.front().id, 5, 0}};
        w.state_.players.push_back(computer);
        ReconContact contact;
        contact.fleet = 40;
        contact.owner = 2;
        contact.observedTurn = contact.lastSeenTurn = contact.deliveryTurn = 5;
        contact.quality = ContactQuality::Confirmed;
        contact.massMinimum = 100;
        contact.massMaximum = 150;
        w.state_.players.front().enemyContacts = {contact};
        w.pendingOrders_ = {1, {}};
        w.rebuildScene();
        w.show();
    }
    static void intel(MainWindow& w) { w.openReconnaissance(); }
    static void computer(MainWindow& w) { w.openComputerDecisions(); }
    static void notify(MainWindow& w, bool reset = false)
    {
        emit w.controlsChanged();
        emit w.routeProgramContextChanged(reset);
    }
    static void mass(MainWindow& w, double value) { w.state_.players.front().enemyContacts.front().massMinimum = value; }
    static void clearContacts(MainWindow& w) { w.state_.players.front().enemyContacts.clear(); }
    static void decision(MainWindow& w, const std::string& reason) { w.state_.players.back().computer.decisions.front().reason = reason; }
    static void renameTarget(MainWindow& w) { w.state_.stars.front().name = "Replacement session target"; }
    static void playerTurn(MainWindow& w) { w.sessionMode_ = SessionMode::PlayerTurn; }
};
} // namespace suns

namespace {
void settle()
{
    QEventLoop loop;
    QTimer::singleShot(20, &loop, &QEventLoop::quit);
    loop.exec();
}

void intel_refreshes_from_events_and_on_reopen(suns::MainWindow& w)
{
    using suns::MainWindowTestAccess;
    MainWindowTestAccess::intel(w);
    auto* dialog = w.findChild<QDialog*>("intelligenceDialog");
    auto* table = dialog->findChild<QTableWidget*>("reconnaissanceTable");
    auto* timer = dialog->findChild<QTimer*>("intelligenceRefreshTimer");
    assert(table->rowCount() == 1 && timer && timer->isSingleShot() && timer->interval() == 0);
    table->setCurrentCell(0, 0);
    auto* original = table->item(0, 0);
    for (int i = 0; i < 4; ++i) MainWindowTestAccess::notify(w);
    settle();
    assert(!timer->isActive());
    assert(table->item(0, 0) == original && table->currentRow() == 0);
    MainWindowTestAccess::mass(w, 125);
    MainWindowTestAccess::notify(w);
    settle();
    assert(table->item(0, 7)->text().contains("125"));
    assert(table->item(0, 0) == original);
    dialog->hide();
    MainWindowTestAccess::clearContacts(w);
    MainWindowTestAccess::notify(w, true);
    settle();
    assert(table->rowCount() == 1); // Hidden tools do no periodic background work.
    MainWindowTestAccess::intel(w);
    settle();
    assert(table->rowCount() == 0 && !timer->isActive());
    dialog->hide();
}

void computer_refreshes_from_events_and_session_reset(suns::MainWindow& w)
{
    using suns::MainWindowTestAccess;
    MainWindowTestAccess::computer(w);
    QPointer<QDialog> dialog = w.findChild<QDialog*>("computerDecisionsDialog");
    auto* table = dialog->findChild<QTreeWidget*>("computerDecisionsTable");
    auto* goals = dialog->findChild<QTreeWidget*>("computerGoalsTable");
    auto* timer = dialog->findChild<QTimer*>("computerDecisionsRefreshTimer");
    auto* filter = dialog->findChild<QComboBox*>("computerDecisionsEmpireFilter");
    assert(table->topLevelItemCount() == 1 && timer && timer->isSingleShot() && timer->interval() == 0);
    filter->setCurrentIndex(1);
    assert(table->topLevelItemCount() == 1); // Filter changes remain synchronous.
    auto* original = table->topLevelItem(0);
    for (int i = 0; i < 4; ++i) MainWindowTestAccess::notify(w);
    settle();
    assert(table->topLevelItem(0) == original && !timer->isActive());
    MainWindowTestAccess::decision(w, "Updated decision");
    MainWindowTestAccess::notify(w);
    settle();
    assert(table->topLevelItem(0)->text(3) == "Updated decision");
    MainWindowTestAccess::renameTarget(w);
    MainWindowTestAccess::notify(w, true);
    settle();
    assert(goals->topLevelItem(0)->text(3) == "Replacement session target");
    dialog->hide();
    MainWindowTestAccess::decision(w, "Updated while hidden");
    MainWindowTestAccess::notify(w);
    settle();
    assert(table->topLevelItem(0)->text(3) == "Updated decision");
    MainWindowTestAccess::computer(w);
    settle();
    assert(table->topLevelItem(0)->text(3) == "Updated while hidden");
    MainWindowTestAccess::playerTurn(w);
    MainWindowTestAccess::notify(w, true);
    assert(!dialog || !dialog->isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    suns::MainWindow w;
    suns::MainWindowTestAccess::setup(w);
    intel_refreshes_from_events_and_on_reopen(w);
    computer_refreshes_from_events_and_session_reset(w);
}
