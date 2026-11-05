#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include "route_program_dock.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QMouseEvent>
#include <QAbstractButton>
#include <QCoreApplication>
#include <QEventLoop>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>
#include <memory>
#include <utility>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.state_.planets.front().productionQueue.clear();
        set_survey_level(w.state_, 1, w.state_.planets.front().star, SurveyLevel::GeologicalSurvey, w.state_.turn);
        w.selection_ = {1, 1};
        w.rebuildScene();
    }
    static void owner(const MainWindow& a, const MainWindow& b)
    {
        assert(&a.state_ == &b.state_ && &a.pendingOrders_ == &b.pendingOrders_);
        assert(&a.selection_ == &b.selection_ && &a.inbox_ == &b.inbox_);
        assert(&a.turnMessages_ == &b.turnMessages_ && &a.readTurnMessageIds_ == &b.readTurnMessageIds_);
        assert(&a.playerTokens_ == &b.playerTokens_ && &a.pendingDescriptions_ == &b.pendingDescriptions_);
        assert(&a.receivedPlayerState() == &b.receivedPlayerState());
        assert(&a.fleetPlanningState() == &b.fleetPlanningState());
    }
    static void population(MainWindow& w, std::uint64_t people)
    {
        auto& p = w.state_.planets.front(); p.population = people;
        set_survey_level(w.state_, 1, p.star, SurveyLevel::GeologicalSurvey, w.state_.turn);
        w.rebuildScene();
    }
    static QLineEdit* seed(MainWindow& w) { return w.seedEdit_; }
    static bool idle(const MainWindow& w)
    {
        return !w.sessionRefreshPending_ && !w.mapSelectionRebuildPending_ && !w.mapDisplayApplyPending_;
    }
    static QCheckBox* sensors(MainWindow& w) { return w.sensorRangesCheck_; }
    static void addFactory(MainWindow& w) { w.appendPendingOrder(QueueProductionBatchOrder{1, ProductionKind::Factory, 1}, "Shared factory"); }
    static void nextTurn(MainWindow& w) { w.endTurn(); }
    static bool save(MainWindow& w, const QString& path) { return w.saveGameToPath(path); }
    static bool load(MainWindow& w, const QString& path) { return w.loadGameFromPath(path); }
    static void designer(MainWindow& w) { w.openShipDesigner(); }
    static auto* designer(const MainWindow& w) { return w.shipDesigner_.data(); }
    static void reports(MainWindow& w, const GameEvent& event) { w.appendTurnMessages({event}); }
    static bool read(const MainWindow& w, std::uint64_t id) { return w.readTurnMessageIds_.contains(id); }
    static auto builds(const MainWindow& w) { (void)w.movementPlanningPreview(); return w.movementPreviewBuildCount_; }
    static auto campaign(const MainWindow& w) { return w.campaignId_; }
    static auto token(const MainWindow& w) { return w.turnToken_; }
    static void removeFleets(MainWindow& w) { w.state_.fleets.clear(); w.selection_.fleet = 99999; w.rebuildScene(); }
};
}
using namespace suns;

template<class Predicate>
static void waitUntil(Predicate ready, const char* description)
{
    QElapsedTimer timer; timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (ready()) return;
        QEventLoop loop; QTimer::singleShot(1, &loop, &QEventLoop::quit); loop.exec();
    } while (timer.elapsed() < 5000);
    qFatal("Timed out waiting for %s", description);
}
static void settle()
{
    // Flush opportunistically; assertions about deferred effects use waitUntil.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
static void install(MainWindow& w)
{
    w.installDeferredMapSelectionHandler(); attachRouteProgramDock(w);
    w.installUiPolish(); w.installMapDisplayModes(); w.installPlanetPolish();
    w.installProductionQueue(); w.installFleetReadabilityPolish(); w.installFleetPortraitPolish();
    w.installMiningInfrastructure(); w.installCommunicationStatus();
    w.installTurnMessages(); w.installResearch(); w.installEmpireHistory();
    w.installPanelLayoutFixes();
}
static QGraphicsItem* star(MainWindow& w, StarId id)
{
    for (auto* item : w.routeProgramScene()->items()) if (item->data(1).toInt() == 1 && item->data(0).toUInt() == id) return item;
    return nullptr;
}
static QListWidgetItem* report(MainWindow& w, std::uint64_t id)
{
    auto* list = w.findChild<QListWidget*>("turnMessagesList"); assert(list);
    for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(Qt::UserRole + 1).toULongLong() == id) return list->item(i);
    return nullptr;
}
static void shared_views_and_document_lifetime()
{
    QTemporaryDir dir;
    auto a = std::make_unique<MainWindow>(); install(*a); MainWindowTestAccess::setup(*a);
    auto session = a->session();
    const auto campaign = MainWindowTestAccess::campaign(*a), token = MainWindowTestAccess::token(*a);
    assert(campaign && token);
    const auto revision = session->revision();
    auto b = std::make_unique<MainWindow>(session);
    assert(session->revision() == revision); // Attaching a view cannot create a new planning boundary or campaign.
    assert(MainWindowTestAccess::campaign(*b) == campaign && MainWindowTestAccess::token(*b) == token);
    install(*b); a->show(); b->show();
    waitUntil([&] { return MainWindowTestAccess::idle(*a) && MainWindowTestAccess::idle(*b); }, "initial linked views");
    MainWindowTestAccess::owner(*a, *b);
    const auto* cached = &session->plannedState();
    const auto builds = MainWindowTestAccess::builds(*a);
    auto* aStar = star(*a, 1); auto* bStar = star(*b, 1); assert(aStar && bStar);
    // A real map selection uses shared stable IDs and leaves all planning caches intact.
    a->routeProgramScene()->clearSelection(); star(*a, 2)->setSelected(true);
    waitUntil([&] { return star(*b, 2)->isSelected(); }, "peer star selection");
    assert(session->selection().star == 2 && star(*b, 2)->isSelected());
    assert(session->revision() == revision && &session->plannedState() == cached);
    assert(MainWindowTestAccess::builds(*b) == builds && star(*a, 1) == aStar && star(*b, 1) == bStar);
    assert(!session->select({std::nullopt, FleetId{999999}}));
    assert(session->selection().fleet == 1);
    assert(session->select({1, 1})); // Intentionally coalesce selection + the following panel edit.
    const auto beforeIndustry = session->state().planets.front().industry;
    MainWindowTestAccess::addFactory(*a);
    waitUntil([&] { return star(*b, 1)->isSelected() && b->findChild<QTreeWidget*>("productionQueueTree")->topLevelItemCount() == 1; }, "selection and production edit");
    assert(session->pendingOrders().orders.size() == 1 && session->pendingDescriptions().size() == 1);
    assert(star(*b, 1)->isSelected() && !star(*b, 2)->isSelected());
    assert(session->state().planets.front().industry == beforeIndustry);
    assert(MainWindowTestAccess::builds(*a) == builds && MainWindowTestAccess::builds(*b) == builds);
    assert(star(*a, 1) == aStar && star(*b, 1) == bStar);
    auto* aQueue = a->findChild<QTreeWidget*>("productionQueueTree");
    auto* bQueue = b->findChild<QTreeWidget*>("productionQueueTree");
    assert(aQueue && bQueue && aQueue->topLevelItemCount() == 1 && bQueue->topLevelItemCount() == 1);
    MainWindowTestAccess::seed(*b)->setText("123456789");
    const auto path = dir.filePath("shared.suns"); assert(MainWindowTestAccess::save(*a, path));
    waitUntil([&] { return a->windowTitle() == b->windowTitle(); }, "shared save title");
    assert(MainWindowTestAccess::seed(*b)->text() == "123456789"); // Another view's Save cannot overwrite this draft.
    assert(a->windowTitle() == b->windowTitle() && b->windowTitle().contains("shared.suns"));
    bool inspectedReplacement = false;
    QObject::connect(session.get(), &GameSession::changed, a.get(), [&](GameSession::Change change, QObject*) {
        if (change != GameSession::Change::Document) return;
        // A direct observer must already see the new epoch at the notification,
        // before the authoring view's repaint or any deferred peer callbacks.
        assert(session->receivedState().planets.front().population == session->state().planets.front().population);
        inspectedReplacement = true;
    });
    // Replacing same-year orders must invalidate both views, not reuse a
    // cache keyed only by the year's number or collection sizes.
    MainWindowTestAccess::addFactory(*b); settle();
    assert(session->pendingOrders().orders.size() == 2);
    MainWindowTestAccess::population(*a, 50'000); settle();
    assert(session->receivedState().planets.front().population == 50'000);
    assert(MainWindowTestAccess::load(*a, path));
    waitUntil([&] { return aQueue->topLevelItemCount() == 1 && bQueue->topLevelItemCount() == 1 && a->windowTitle() == b->windowTitle(); }, "same-year document refresh");
    assert(inspectedReplacement && session->receivedState().planets.front().population == 25'000);
    assert(session->pendingOrders().orders.size() == 1);
    assert(aQueue->topLevelItemCount() == 1 && bQueue->topLevelItemCount() == 1);
    MainWindowTestAccess::designer(*b); settle();
    auto* liveDesigner = MainWindowTestAccess::designer(std::as_const(*b)); assert(liveDesigner);
    liveDesigner->findChild<QLineEdit*>("shipDesignName")->setText("Linked live draft");
    const auto turn = session->state().turn;
    MainWindowTestAccess::nextTurn(*a);
    waitUntil([&] { return a->windowTitle() == b->windowTitle(); }, "peer turn completion");
    assert(MainWindowTestAccess::designer(std::as_const(*b)) == liveDesigner);
    assert(liveDesigner->findChild<QLineEdit*>("shipDesignName")->text() == "Linked live draft");
    assert(session->state().turn == turn + 1 && session->pendingOrders().orders.empty());
    assert(session->state().planets.front().industry == beforeIndustry + 1); // Exactly one resolution, never one per view.
    assert(MainWindowTestAccess::token(*a) == MainWindowTestAccess::token(*b));
    assert(a->windowTitle() == b->windowTitle());
    MainWindowTestAccess::designer(*b); settle();
    auto* draft = MainWindowTestAccess::designer(std::as_const(*b)); assert(draft);
    draft->findChild<QLineEdit*>("shipDesignName")->setText("Unsubmitted old document");
    assert(MainWindowTestAccess::load(*a, path));
    waitUntil([&] { return !MainWindowTestAccess::designer(std::as_const(*b)) && a->windowTitle() == b->windowTitle(); }, "peer document/draft replacement");
    assert(session->state().turn == turn && session->pendingOrders().orders.size() == 1);
    assert(!MainWindowTestAccess::designer(std::as_const(*b))); // No draft against the replaced world's identity.
    assert(MainWindowTestAccess::token(*b) == token);
    assert(a->windowTitle() == b->windowTitle());
    MainWindowTestAccess::owner(*a, *b);
    // Reports and read state belong to the document, not the window that opened them.
    GameEvent event; event.id = 987654; event.turn = event.observedTurn = turn;
    event.recipient = 1; event.kind = GameEventKind::FleetArrived; event.star = 1; event.fleet = 1;
    MainWindowTestAccess::reports(*a, event);
    waitUntil([&] { return report(*b, event.id) && report(*a, event.id); }, "shared report rows");
    auto* peerReport = report(*b, event.id); assert(peerReport && report(*a, event.id));
    b->findChild<QListWidget*>("turnMessagesList")->setCurrentItem(peerReport);
    // Explicit read action also works for an already displayed row.
    b->findChild<QPushButton*>("nextUnreadTurnMessage")->click();
    waitUntil([&] { return MainWindowTestAccess::read(*a, event.id) && !report(*a, event.id)->font().bold(); }, "shared explicit read mark");
    assert(MainWindowTestAccess::read(*a, event.id) && MainWindowTestAccess::read(*b, event.id));
    assert(!report(*a, event.id)->font().bold());
    const auto reportRevision = session->revision();
    assert(session->select({2, 1}));
    waitUntil([&] { return star(*a, 2)->isSelected() && star(*b, 2)->isSelected(); }, "shared reference selection");
    b->findChild<QPushButton*>("showTurnMessageOnMap")->click();
    waitUntil([&] { return star(*a, 1)->isSelected() && star(*b, 1)->isSelected(); }, "report map selection");
    assert(session->selection().star == 1 && star(*a, 1)->isSelected() && star(*b, 1)->isSelected());
    assert(session->revision() == reportRevision); // A report click is selection, not a new physical boundary.

    // Closing a map leaves the canonical document and the other view alive.
    a.reset(); settle();
    assert(session->pendingOrders().orders.size() == 1);
    MainWindowTestAccess::nextTurn(*b); settle(); assert(session->state().turn == turn + 1);
    MainWindowTestAccess::removeFleets(*b); settle(); assert(!session->selection().fleet);
    auto c = std::make_unique<MainWindow>(session); settle();
    assert(c->session()->state().turn == turn + 1 && MainWindowTestAccess::campaign(*c) == campaign);
    std::weak_ptr<GameSession> lifetime = session;
    session.reset(); b.reset(); c.reset(); settle(); assert(lifetime.expired());
}
static void separate_documents_stay_separate()
{
    MainWindow a, b;
    assert(a.session() != b.session());
    MainWindowTestAccess::setup(a); MainWindowTestAccess::addFactory(a); settle();
    assert(a.session()->pendingOrders().orders.size() == 1 && b.session()->pendingOrders().orders.empty());
    assert(MainWindowTestAccess::campaign(a) != MainWindowTestAccess::campaign(b));
}
static void visitor_demo_replaces_both_contexts()
{
    MainWindow a; install(a); a.installVisitors(); MainWindowTestAccess::setup(a);
    MainWindow b(a.session()); install(b); a.show(); b.show(); settle();
    MainWindowTestAccess::designer(a); MainWindowTestAccess::designer(b);
    auto* old = MainWindowTestAccess::designer(std::as_const(b)); assert(old);
    old->findChild<QLineEdit*>("shipDesignName")->setText("Old party design");
    QTimer::singleShot(0, &a, [] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); assert(box);
        box->button(QMessageBox::Yes)->click();
    });
    a.findChild<QAction*>("visitorDemoAction")->trigger();
    assert(a.session()->state().turn == 60);
    waitUntil([&] { return !MainWindowTestAccess::designer(std::as_const(b)); }, "visitor demo closes peer draft");
    assert(!MainWindowTestAccess::designer(std::as_const(a)));
    assert(a.session()->pendingOrders().orders.empty());
}
static void every_load_signal_sees_one_document()
{
    QTemporaryDir dir; MainWindow a; install(a); MainWindowTestAccess::setup(a);
    MainWindow b(a.session()); install(b); auto session = a.session();
    MainWindowTestAccess::addFactory(a);
    const auto path = dir.filePath("atomic.suns"); assert(MainWindowTestAccess::save(a, path));
    MainWindowTestAccess::addFactory(a); MainWindowTestAccess::population(a, 50'000);
    assert(session->select({2, 1}));
    assert(session->receivedState().planets.front().population == 50'000);
    settle();
    const auto before = session->revision(); int signalCount = 0, inconsistent = 0, documents = 0;
    QObject::connect(session.get(), &GameSession::changed, &a, [&](GameSession::Change change, QObject*) {
        ++signalCount; documents += change == GameSession::Change::Document;
        if (session->state().planets.front().population != 25'000
            || session->receivedState().planets.front().population != 25'000
            || session->plannedState().planets.front().population != 25'000
            || session->pendingOrders().orders.size() != 1 || session->selection().star != 1
            || session->revision() <= before) ++inconsistent;
    });
    assert(MainWindowTestAccess::load(a, path)); settle();
    assert(signalCount && documents == 1 && inconsistent == 0);
}
static void hidden_report_refresh_does_not_read(bool navigate = false)
{
    MainWindow a; install(a); MainWindowTestAccess::setup(a);
    MainWindow b(a.session()); install(b); a.show(); b.show();
    auto* filter = b.findChild<QComboBox*>("turnMessageTypeFilter");
    filter->setCurrentIndex(filter->findText("Fleet movement"));
    GameEvent old; old.id = 1201; old.turn = old.observedTurn = a.session()->state().turn;
    old.recipient = 1; old.kind = GameEventKind::ProductionCompleted; old.productionKind = ProductionKind::Factory; old.star = 1;
    MainWindowTestAccess::reports(a, old);
    waitUntil([&] { return report(a, old.id); }, "previous report");
    a.findChild<QListWidget*>("turnMessagesList")->setCurrentItem(report(a, old.id));
    a.findChild<QPushButton*>("nextUnreadTurnMessage")->click();
    b.findChild<QDockWidget*>("turnMessagesDock")->hide(); settle();
    assert(b.findChild<QListWidget*>("turnMessagesList")->count() == 0);
    GameEvent arrived = old; arrived.id = 1202; arrived.kind = GameEventKind::FleetArrived; arrived.fleet = 1;
    MainWindowTestAccess::reports(a, arrived);
    assert(!MainWindowTestAccess::read(a, arrived.id));
    waitUntil([&] { return report(b, arrived.id); }, "hidden filtered report");
    assert(!MainWindowTestAccess::read(a, arrived.id) && !MainWindowTestAccess::read(b, arrived.id));
    assert(report(a, arrived.id)->font().bold() && report(b, arrived.id)->font().bold());
    if (navigate) b.findChild<QPushButton*>("nextUnreadTurnMessage")->click();
    else {
        b.findChild<QDockWidget*>("turnMessagesDock")->show();
        auto* list = b.findChild<QListWidget*>("turnMessagesList");
        list->scrollToItem(report(b, arrived.id)); settle();
        const auto local = QPointF(list->visualItemRect(report(b, arrived.id)).center());
        const auto global = QPointF(list->viewport()->mapToGlobal(local.toPoint()));
        QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(list->viewport(), &press); QApplication::sendEvent(list->viewport(), &release);
    }
    waitUntil([&] { return MainWindowTestAccess::read(a, arrived.id) && !report(a, arrived.id)->font().bold(); }, "explicit filtered report read");
}
static void sensor_toggle_is_local()
{
    MainWindow a; install(a); MainWindowTestAccess::setup(a);
    MainWindow b(a.session()); install(b);
    waitUntil([&] { return MainWindowTestAccess::idle(a) && MainWindowTestAccess::idle(b); }, "initial sensor views");
    const auto revision = a.session()->revision(), builds = MainWindowTestAccess::builds(a);
    const auto* cache = &a.session()->plannedState(); auto* marker = star(b, 1);
    const bool peerSensors = MainWindowTestAccess::sensors(b)->isChecked();
    MainWindowTestAccess::sensors(a)->click(); settle();
    assert(a.session()->revision() == revision && MainWindowTestAccess::builds(b) == builds);
    assert(&a.session()->plannedState() == cache && star(b, 1) == marker);
    assert(MainWindowTestAccess::sensors(b)->isChecked() == peerSensors);
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv); QTemporaryDir settings;
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    if (app.arguments().contains("--demo")) { visitor_demo_replaces_both_contexts(); return 0; }
    if (app.arguments().contains("--atomic")) { every_load_signal_sees_one_document(); return 0; }
    if (app.arguments().contains("--messages")) { hidden_report_refresh_does_not_read(); return 0; }
    if (app.arguments().contains("--sensors")) { sensor_toggle_is_local(); return 0; }
    shared_views_and_document_lifetime(); separate_documents_stay_separate();
    visitor_demo_replaces_both_contexts(); every_load_signal_sees_one_document();
    hidden_report_refresh_does_not_read(); hidden_report_refresh_does_not_read(true); sensor_toggle_is_local();
}
