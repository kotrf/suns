#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "ship_designer_dialog.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEventLoop>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>

#include <algorithm>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static ShipDesignerDialog* designer(const MainWindow& w) { return w.shipDesigner_; }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static void addDesign(MainWindow& w, const ShipDesignDraft& draft)
    {
        w.pendingOrders_.orders.push_back(CreateShipDesignOrder{draft.name, draft.hull, draft.components, draft.placements});
    }
    static void nextTurn(MainWindow& w) { w.endTurn(); }
    static void deliverReports(MainWindow& w, const std::vector<GameEvent>& reports)
    {
        w.appendTurnMessages(reports);
    }
    static std::uint64_t latestHistoryTurn(const MainWindow& w)
    {
        return w.state_.players.front().history.back().turn;
    }
    static PlanetId firstColony(const MainWindow& w)
    {
        const auto colony = std::find_if(w.state_.planets.begin(), w.state_.planets.end(),
            [&](const auto& planet) { return planet.owner == w.pendingOrders_.player; });
        assert(colony != w.state_.planets.end());
        return colony->id;
    }
};
} // namespace suns

static void settle()
{
    QEventLoop loop;
    QTimer::singleShot(50, &loop, &QEventLoop::quit);
    loop.exec();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

static void installWorkspace(suns::MainWindow& window)
{
    suns::attachRouteProgramDock(window);
    window.installUiPolish();
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
    window.installPanelLayoutFixes();
}

int main(int argc, char** argv)
{
    using namespace suns;
    QApplication app(argc, argv);
    QTemporaryDir settingsDir;
    assert(settingsDir.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
    {
        // Reproduce a saved layout with History floating and a hidden colony
        // panel. The new workspace must not restore either old arrangement.
        QMainWindow oldWindow;
        auto* oldHistory = new QDockWidget("Empire History", &oldWindow);
        oldHistory->setObjectName("empireHistoryDock");
        oldWindow.addDockWidget(Qt::BottomDockWidgetArea, oldHistory);
        oldHistory->setFloating(true);
        auto* oldResearch = new QDockWidget("Empire Research", &oldWindow);
        oldResearch->setObjectName("researchDock");
        oldWindow.addDockWidget(Qt::BottomDockWidgetArea, oldResearch);
        oldWindow.tabifyDockWidget(oldHistory, oldResearch);
        auto* production = new QDockWidget("Production", &oldWindow);
        production->setObjectName("productionDock");
        oldWindow.addDockWidget(Qt::LeftDockWidgetArea, production);
        production->hide();
        QSettings settings("SunsProject", "Suns");
        settings.setValue("workspace/docks", oldWindow.saveState(5));
        settings.setValue("workspace/customDocks", oldWindow.saveState(5));
        settings.setValue("workspace/customVersion", 5);
    }

    MainWindow window;
    installWorkspace(window);
    window.show();
    settle();
    assert(!window.findChild<QDockWidget*>("empireHistoryDock"));
    assert(!window.findChild<QDockWidget*>("researchDock"));
    assert(window.findChild<QDockWidget*>("productionDock")->isVisible());
    assert(!window.findChild<QAction*>("restoreCustomWorkspaceAction")->isEnabled());
    assert(!window.findChild<QAction*>("designWorkspaceAction"));
    const auto dockState = window.saveState(6);

    auto* designerTool = window.findChild<QAction*>("openShipDesignerToolAction");
    auto* designerMenu = window.findChild<QAction*>("openShipDesignerAction");
    auto* historyTool = window.findChild<QAction*>("openEmpireHistoryToolAction");
    auto* historyMenu = window.findChild<QAction*>("openEmpireHistoryAction");
    auto* researchTool = window.findChild<QAction*>("openResearchToolAction");
    auto* researchMenu = window.findChild<QAction*>("openResearchAction");
    auto* toolbar = window.findChild<QToolBar*>("dialogToolbar");
    auto* viewMenu = window.menuBar()->findChild<QMenu*>("sunsViewMenu");
    assert(designerTool && designerMenu && historyTool && historyMenu);
    assert(toolbar->actions().contains(historyTool) && viewMenu->actions().contains(historyMenu));
    assert(!historyMenu->isCheckable());
    assert(researchTool && researchMenu && !researchMenu->isCheckable());
    assert(toolbar->actions().contains(researchTool) && viewMenu->actions().contains(researchMenu));

    designerTool->trigger();
    settle();
    QPointer<ShipDesignerDialog> first = MainWindowTestAccess::designer(window);
    assert(first && first->isWindow() && first->isVisible() && !first->isModal());
    first->findChild<QLineEdit*>("shipDesignName")->setText("Keep this draft");
    designerMenu->trigger();
    assert(MainWindowTestAccess::designer(window) == first);
    assert(first->findChild<QLineEdit*>("shipDesignName")->text() == "Keep this draft");
    first->showMinimized();
    designerTool->trigger();
    assert(first->isVisible() && !first->isMinimized());
    assert(window.saveState(6) == dockState);

    // Reopen before Qt delivers deferred deletion of the old dialog. Its
    // destruction must not erase the new live draft or open extra windows.
    first->reject();
    designerTool->trigger();
    auto* second = MainWindowTestAccess::designer(window);
    assert(second && second != first && second->isVisible());
    settle();
    assert(!first);
    designerMenu->trigger();
    assert(MainWindowTestAccess::designer(window) == second);
    assert(window.findChildren<QDialog*>("shipDesignerDialog").size() == 1);
    auto* save = second->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save);
    // AUD-006: name collisions never accept/delete the live nonmodal draft.
    auto* name = second->findChild<QLineEdit*>("shipDesignName");
    const auto beforeOrders = MainWindowTestAccess::orders(window).orders.size();
    name->setText("Scout");
    assert(!save->isEnabled());
    second->accept();
    settle();
    assert(MainWindowTestAccess::designer(window) == second && second->isVisible());
    assert(MainWindowTestAccess::orders(window).orders.size() == beforeOrders);
    // Also validate live pending orders that appeared after opening the window.
    name->setText("Late collision");
    const auto draft = second->draft();
    const auto originalHull = draft.hull;
    const auto originalComponents = draft.components;
    MainWindowTestAccess::addDesign(window, draft);
    save->click();
    settle();
    assert(MainWindowTestAccess::designer(window) == second && second->isVisible());
    assert(second->draft().hull == originalHull && second->draft().components == originalComponents);
    name->setText("Separate window scout");
    assert(save->isEnabled());
    if (app.arguments().contains("--capture")) second->grab().save("/tmp/suns-designer-window.png");
    save->click();
    assert(!MainWindowTestAccess::designer(window));
    const auto& orders = MainWindowTestAccess::orders(window).orders;
    assert(std::any_of(orders.begin(), orders.end(), [](const auto& order) {
        const auto* design = std::get_if<CreateShipDesignOrder>(&order);
        return design && design->name == "Separate window scout";
    }));
    settle();

    researchTool->trigger();
    settle();
    auto* research = window.findChild<QDialog*>("researchDialog");
    assert(research && research->isWindow() && research->isVisible() && !research->isModal());
    assert(!QApplication::activeModalWidget());
    auto* allocation = research->findChild<QSpinBox*>("researchAllocationSpin");
    auto* plan = research->findChild<QTreeWidget*>("researchPlanTree");
    allocation->setValue(allocation->value() == 25 ? 35 : 25);
    const auto allocationPercent = allocation->value();
    const auto initialPlanRows = plan->topLevelItemCount();
    research->findChild<QPushButton*>("researchAddButton")->click();
    const auto plannedRows = initialPlanRows + 1;
    assert(plan->topLevelItemCount() == plannedRows);
    const auto pendingResearchOrders = MainWindowTestAccess::orders(window).orders.size();
    assert(std::any_of(orders.begin(), orders.end(), [](const auto& order) {
        return std::holds_alternative<SetResearchAllocationOrder>(order);
    }));
    assert(std::any_of(orders.begin(), orders.end(), [](const auto& order) {
        return std::holds_alternative<SetResearchPlanOrder>(order);
    }));
    research->showMinimized();
    researchMenu->trigger();
    assert(research->isVisible() && !research->isMinimized());
    assert(window.findChildren<QDialog*>("researchDialog").size() == 1);
    assert(window.saveState(6) == dockState);
    if (app.arguments().contains("--capture")) research->grab().save("/tmp/suns-research-window.png");
    research->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close)->click();
    researchMenu->trigger(); // Immediately reuse the same window and pending plan.
    assert(window.findChild<QDialog*>("researchDialog") == research);
    assert(allocation->value() == allocationPercent && plan->topLevelItemCount() == plannedRows);
    assert(MainWindowTestAccess::orders(window).orders.size() == pendingResearchOrders);
    const auto beforeResearch = research->findChild<QProgressBar*>("researchProgress")->format();
    research->close();

    historyTool->trigger();
    settle();
    auto* history = window.findChild<QDialog*>("empireHistoryDialog");
    assert(history && history->isWindow() && history->isVisible() && !history->isModal());
    assert(!QApplication::activeModalWidget());
    auto* metric = history->findChild<QComboBox*>("historyMetric");
    auto* scope = history->findChild<QComboBox*>("historyScope");
    auto* compare = history->findChild<QComboBox*>("historyCompare");
    assert(metric->itemText(22) == "Received combat activity (reports/year)");
    metric->setCurrentIndex(22);
    auto* historyChart = history->findChild<QWidget*>("empireHistoryChart");
    assert(historyChart);
    const auto chartDescription = historyChart->accessibleDescription();
    for (const auto& name : {"Space-battle reports", "Ground reports",
                             "Bombardment reports", "Mine-strike reports"})
        assert(chartDescription.contains(name));
    assert(history->findChild<QLabel*>("historySummary")->text().contains(
        "undetected opponent actions remain unknown"));
    metric->setCurrentIndex(3);
    const auto colonyIndex = scope->findData(static_cast<quint32>(MainWindowTestAccess::firstColony(window)));
    assert(colonyIndex >= 0);
    scope->setCurrentIndex(colonyIndex);
    compare->setCurrentIndex(compare->findData(0));
    const auto scopeId = scope->currentData();
    const auto comparisonId = compare->currentData();
    historyMenu->trigger();
    assert(window.findChildren<QDialog*>("empireHistoryDialog").size() == 1);
    history->showMinimized();
    historyMenu->trigger();
    assert(history->isVisible() && !history->isMinimized());
    history->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close)->click();
    assert(history->isHidden());
    const auto previousYear = history->findChild<QSpinBox*>("historyLastTurn")->value();
    // Advance while the window is closed, then reopen current observations
    // with the same explicit colony and comparison filters.
    MainWindowTestAccess::nextTurn(window);
    MainWindowTestAccess::nextTurn(window);
    researchTool->trigger();
    assert(allocation->value() == allocationPercent);
    assert(research->findChild<QProgressBar*>("researchProgress")->format() != beforeResearch);
    const auto latest = MainWindowTestAccess::latestHistoryTurn(window);
    assert(latest > static_cast<std::uint64_t>(previousYear));
    historyTool->trigger();
    settle();
    assert(window.findChild<QDialog*>("empireHistoryDialog") == history);
    assert(metric->currentIndex() == 3 && scope->currentData() == scopeId);
    assert(compare->currentData() == comparisonId);
    assert(history->findChild<QLabel*>("historySummary")->text().contains(
        QString("latest: %1").arg(static_cast<qulonglong>(latest))));
    assert(history->findChild<QSpinBox*>("historyLastTurn")->value() == static_cast<int>(latest));
    assert(window.saveState(6) == dockState);
    if (app.arguments().contains("--capture")) history->grab().save("/tmp/suns-history-window.png");

    // A hostile-activity briefing is made only from recipient-delivered reports.
    // Another empire's event must not become visible through the grouped filter.
    GameEvent contact; contact.id = 900001; contact.turn = latest; contact.observedTurn = latest;
    contact.recipient = 1; contact.kind = GameEventKind::EnemyFleetDetected; contact.fleet = 123;
    GameEvent battle; battle.id = 900002; battle.turn = latest; battle.observedTurn = latest;
    battle.recipient = 1; battle.kind = GameEventKind::SpaceBattle;
    GameEvent science; science.id = 900003; science.turn = latest; science.observedTurn = latest;
    science.recipient = 1; science.kind = GameEventKind::ResearchLevelCompleted;
    science.researchField = ResearchField::Biology; science.technologyLevel = 1;
    GameEvent hidden = contact; hidden.id = 900004; hidden.recipient = 2;
    MainWindowTestAccess::deliverReports(window, {contact, battle, science, hidden});
    auto* reportFilter = window.findChild<QComboBox*>("turnMessageTypeFilter");
    auto* reportList = window.findChild<QListWidget*>("turnMessagesList");
    assert(reportFilter && reportList);
    const auto allReports = reportList->count();
    assert(allReports >= 3);
    const int activityIndex = reportFilter->findText("Observed hostile activity");
    assert(activityIndex >= 0);
    reportFilter->setCurrentIndex(activityIndex);
    assert(reportList->count() == 2);
    reportFilter->setCurrentIndex(reportFilter->findText("Enemy contacts"));
    assert(reportList->count() == 1);
    reportFilter->setCurrentIndex(reportFilter->findText("Combat"));
    assert(reportList->count() == 1);
    reportFilter->setCurrentIndex(0);
    assert(reportList->count() == allReports);

    // Window visibility is independent of panel presets, resets and the
    // temporary fullscreen map. The tools never become dock tabs.
    window.resetPanelLayout();
    window.findChild<QAction*>("empireWorkspaceAction")->trigger();
    assert(history->isVisible());
    assert(research->isVisible());
    window.setMapExpanded(true);
    settle();
    assert(history->isVisible());
    assert(research->isVisible());
    window.setMapExpanded(false);
    settle();
    assert(history->isVisible() && !window.findChild<QDockWidget*>("empireHistoryDock"));
    assert(research->isVisible() && !window.findChild<QDockWidget*>("researchDock"));
    window.findChild<QAction*>("saveCustomWorkspaceAction")->trigger();
    history->close();
    research->close();
    window.findChild<QAction*>("restoreCustomWorkspaceAction")->trigger();
    assert(history->isHidden());
    assert(research->isHidden());
    window.findChild<QAction*>("empireWorkspaceAction")->trigger();
    assert(history->isHidden());
    assert(research->isHidden());
    window.close();

    MainWindow restored;
    installWorkspace(restored);
    restored.show();
    settle();
    assert(!restored.findChild<QDockWidget*>("empireHistoryDock"));
    assert(!restored.findChild<QDockWidget*>("researchDock"));
    assert(restored.findChild<QDialog*>("researchDialog")->isHidden());
    assert(restored.findChild<QDialog*>("empireHistoryDialog")->isHidden());
    assert(restored.findChild<QAction*>("restoreCustomWorkspaceAction")->isEnabled());
    restored.findChild<QAction*>("openEmpireHistoryAction")->trigger();
    assert(restored.findChild<QDialog*>("empireHistoryDialog")->isVisible());
    restored.findChild<QAction*>("openResearchAction")->trigger();
    assert(restored.findChild<QDialog*>("researchDialog")->isVisible());
    restored.close();
}
