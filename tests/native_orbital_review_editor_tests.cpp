#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include "native_orbital_fixture.hpp"
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w, bool foreign = false) {
        w.state_ = test::empty_native_orbital_start();
        assert(initialize_native_orbital_start(w.state_, 1, w.state_.planets.front().id, native_orbital_start_budget()));
        if (foreign) {
            w.state_.players.push_back({2, "Other"});
            w.state_.shipDesigns.push_back({3, 2, "Foreign project", ShipHullType::StarsScout, {ShipComponentType::QuickJump5}});
            w.state_.nextShipDesignId = 4;
        }
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star;
        w.installUiPolish(); w.installProductionQueue(); w.rebuildScene(); w.show();
    }
    static ShipDesignerDialog* designer(MainWindow& w) { w.openShipDesigner(); return w.shipDesigner_; }
    static QComboBox* choices(MainWindow& w) { return w.shipDesignCombo_; }
    static QPushButton* queueButton(MainWindow& w) { return w.buildShipButton_; }
    static void queue(MainWindow& w) { w.queueShipDesign(); }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static const GameState& state(const MainWindow& w) { return w.state_; }
    static bool load(MainWindow& w, const QString& path) { return w.loadGameFromPath(path); }
    static void restart(MainWindow& w) { w.newGalaxy(); }
    static auto campaign(const MainWindow& w) { return w.campaignId_; }
    static auto token(const MainWindow& w) { return w.turnToken_; }
    static QString path(const MainWindow& w) { return w.currentSavePath_; }
    static void surfaceTemplate(MainWindow& w, bool ordinary = false) {
        if (ordinary) w.state_ = generate_campaign({}, {{"Terrans"}});
        w.state_.players.front().productionTemplates = {{"Surface", {{ProductionKind::Factory, kFactoryCost}}}};
        w.pendingOrders_ = {1,{}}; w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star; w.rebuildScene(); w.refreshProductionQueue();
    }
    static bool factoryButtonEnabled(const MainWindow& w) { return w.buildFactoryButton_->isEnabled(); }
    static void staleIndustry(MainWindow& w) {
        auto& p = w.state_.planets.front();
        p.productionQueue = {{ProductionKind::Factory, kFactoryCost, 0, ProductionAutomation::Annual, 1000},
            {ProductionKind::ColonyShip, ship_design_cost(w.state_.shipDesigns.front()), w.state_.shipDesigns.front().id}};
        w.state_.players.front().technology.researchActive = false;
        w.state_.players.front().surveyKnowledge.clear(); w.state_.players.front().surveyedStars.clear();
        set_survey_level(w.state_,1,p.star,SurveyLevel::DeepSurvey,w.state_.turn);
        w.rebuildScene(); w.refreshProductionQueue();
    }
    static QTreeWidget* production(MainWindow& w) { return w.productionQueueTree_; }
    static void workspace(MainWindow& w) { w.installColonyWorkspace(); w.rebuildScene(); }
    static void refreshHome(MainWindow& w) {
        w.selection_.star = w.state_.planets.front().star; w.rebuildScene(); w.refreshProductionQueue();
    }
    static QString productionSummary(const MainWindow& w) {
        return w.productionQueueSummary_->property("compactColonySummary").toBool()
            ? w.productionQueueSummary_->toolTip() : w.productionQueueSummary_->text();
    }
    static void hiddenShortage(MainWindow& w) {
        auto& p = w.state_.planets.front(); p.minerals = {}; p.nativeOrbit.reserveUnits = 0;
        p.nativeOrbit.service = HabitatService::Failing; w.rebuildScene();
    }
    static void moveUp(MainWindow& w) { w.moveSelectedProductionItem(-1); }
    static void remove(MainWindow& w) { w.removeSelectedProductionItem(); }
};
}
using namespace suns;
namespace {
void save_draft(MainWindow& w, const char* name, bool unfitted)
{
    auto* dialog = MainWindowTestAccess::designer(w); assert(dialog);
    auto* source = dialog->findChild<QComboBox*>("shipDesignTemplate"); assert(source);
    source->setCurrentIndex(source->findData(quint32{1}));
    if (unfitted) {
        const auto before = dialog->draft(); bool removed = false;
        for (const auto& p : before.placements) if (p.component == ShipComponentType::RoboMiniMiner) {
            dialog->findChild<QToolButton*>(QString("shipSlot_%1").arg(p.slot))->click();
            dialog->findChild<QPushButton*>("removeSelectedComponent")->click(); removed = true; break;
        }
        assert(removed && dialog->draft().components.size() == 1);
    }
    dialog->findChild<QLineEdit*>("shipDesignName")->setText(name);
    auto* save = dialog->findChild<QPushButton*>("saveShipDesign"); assert(save && save->isEnabled()); save->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
void designer_selection_queues_the_same_pending_project(bool twoDrafts, bool foreign = false)
{
    MainWindow w; MainWindowTestAccess::setup(w, foreign);
    if (twoDrafts) save_draft(w, "Unfitted Miner", true);
    save_draft(w, "Working Replacement", false);
    auto* choices = MainWindowTestAccess::choices(w);
    int selected = -1;
    for (int i=0; i<choices->count(); ++i) if (choices->itemText(i).startsWith("Working Replacement")) selected=i;
    assert(selected >= 0); choices->setCurrentIndex(selected);
    assert(MainWindowTestAccess::queueButton(w)->isEnabled());
    MainWindowTestAccess::queue(w);
    const auto& orders = MainWindowTestAccess::orders(w);
    const auto* queue = std::get_if<QueueShipDesignOrder>(&orders.orders.back());
    assert(queue && queue->design == 0 && queue->pendingDesignName == "Working Replacement");
    auto state = TurnProcessor{}.process(MainWindowTestAccess::state(w), {orders});
    for (unsigned i=0; i<3; ++i) state = TurnProcessor{}.process(state, {});
    assert(state.fleets.size() == 2 && fleet_can_remote_mine(state, state.fleets.back()));
    assert(find_ship_design(state, state.fleets.back().design)->name == "Working Replacement");
}
void native_industry_is_unavailable_in_the_actual_production_panel()
{
    MainWindow w; MainWindowTestAccess::setup(w);
    auto* kinds = w.findChild<QComboBox*>("productionBatchKind"); assert(kinds);
    for (int i=0; i<kinds->count(); ++i) {
        const auto kind = ProductionKind(kinds->itemData(i).toInt());
        if (kind == ProductionKind::Factory || kind == ProductionKind::Mine
            || kind == ProductionKind::AdvancedFactory || kind == ProductionKind::AdvancedMine
            || kind == ProductionKind::Terraforming
            || industry_upgrade_kind(kind) || reconstruction_kind(kind))
            assert(!(kinds->model()->flags(kinds->model()->index(i,0)) & Qt::ItemIsEnabled));
    }
    assert(!MainWindowTestAccess::factoryButtonEnabled(w));
    MainWindowTestAccess::surfaceTemplate(w);
    auto* templates = w.findChild<QComboBox*>("productionTemplateName");
    auto* apply = w.findChild<QPushButton*>("productionTemplateApply"); assert(templates && apply);
    templates->setCurrentIndex(templates->findText("Surface"));
    assert(!apply->isEnabled() && apply->toolTip().contains("surface factories"));
    const auto state = MainWindowTestAccess::state(w);
    QTemporaryDir dir; QString error; TurnOrderFileData packet{285, state.turn, 17,
        {1,{QueueProductionOrder{state.planets.front().id, ProductionKind::Factory},
            QueueProductionBatchOrder{state.planets.front().id, ProductionKind::Mine, 2}}}, {"Factory", "Mines"}};
    const auto path = dir.filePath("surface.sunsorders");
    assert(write_turn_order_file(path, packet, error)); TurnOrderFileData decoded;
    assert(read_turn_order_file(path, decoded, error));
    const auto resolved = TurnProcessor{}.process(state,{decoded.orders});
    assert(resolved.planets.front().productionQueue.empty() && !resolved.planets.front().industry && !resolved.planets.front().mines);
    assert(resolved.planets.front().minerals.germanium + resolved.planets.front().nativeOrbit.mineralUnitsPaid == 101);
    MainWindowTestAccess::surfaceTemplate(w, true);
    assert(MainWindowTestAccess::factoryButtonEnabled(w));
    assert(apply->isEnabled());
    for (int i=0; i<kinds->count(); ++i)
        assert(kinds->model()->flags(kinds->model()->index(i,0)) & Qt::ItemIsEnabled);
}
void life_support_is_visible_and_received()
{
    MainWindow w; MainWindowTestAccess::setup(w);
    MainWindowTestAccess::workspace(w);
    QCoreApplication::processEvents();
    auto* status = w.findChild<QLabel*>("colonyStatusSummary"); assert(status);
    const auto before = status->text();
    assert(before.contains("Life support") && before.contains("Population change"));
    MainWindowTestAccess::hiddenShortage(w); QCoreApplication::processEvents();
    assert(status->text() == before);
    const auto capture = qEnvironmentVariable("SUNS_SUPPORT_SCREENSHOT");
    if (!capture.isEmpty()) { w.resize(1440, 900); QCoreApplication::processEvents(); assert(w.grab().save(capture)); }
}
void loaded_native_start_cannot_restart_as_a_generalist(bool throughMenu = false)
{
    auto state = test::empty_native_orbital_start();
    assert(initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget()));
    SaveGameData save; save.state = state; save.mode = SessionMode::Host; save.campaignId = 285;
    save.turnToken = 17; save.playerTokens = {{1,17}}; save.pendingOrders = {1,{SetResearchAllocationOrder{17}}};
    save.pendingDescriptions = {"Research allocation 17%"};
    QTemporaryDir dir; QString error; const auto path = dir.filePath("native-host.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData decoded; assert(read_save_game_file(path, decoded, error));
    MainWindow w; w.installUiPolish(); assert(MainWindowTestAccess::load(w,path));
    QCoreApplication::processEvents();
    const auto campaign = MainWindowTestAccess::campaign(w), token = MainWindowTestAccess::token(w);
    const auto loadedPath = MainWindowTestAccess::path(w), loadedTitle = w.windowTitle();
    if (throughMenu) {
        QAction* restart = nullptr;
        for (auto* action : w.findChildren<QAction*>()) if (action->text() == "Restart current galaxy") restart = action;
        assert(restart); restart->trigger();
    } else MainWindowTestAccess::restart(w);
    const auto& after = MainWindowTestAccess::state(w);
    assert(after.players.front().race.primaryTrait == PrimaryRaceTrait::HabitatCivilization);
    assert(after.planets.front().nativeOrbit.population == kInitialHomePopulation && !after.planets.front().population);
    assert(after.turn == state.turn && after.orbitalStations.size() == 1);
    assert(MainWindowTestAccess::campaign(w) == campaign && MainWindowTestAccess::token(w) == token);
    assert(MainWindowTestAccess::orders(w).orders.size() == 1);
    assert(w.statusBar()->currentMessage().contains("not playable", Qt::CaseInsensitive));
    assert(MainWindowTestAccess::path(w) == loadedPath && w.windowTitle() == loadedTitle);
}
void a_loaded_native_host_can_create_an_explicit_generalist_campaign()
{
    MainWindow w; MainWindowTestAccess::setup(w);
    SaveGameData save; save.state = MainWindowTestAccess::state(w); save.mode = SessionMode::Host;
    save.campaignId = 285; save.turnToken = 17; save.playerTokens = {{1,17}}; save.pendingOrders = {1,{}};
    QTemporaryDir dir; QString error; const auto path = dir.filePath("replace-native.suns");
    assert(write_save_game_file(path,save,error)); assert(MainWindowTestAccess::load(w,path));
    QCoreApplication::processEvents();
    bool submitted = false;
    QTimer::singleShot(0, &w, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "newCampaignDialog");
        dialog->findChild<QSpinBox*>("campaignEmpireCount")->setValue(1);
        auto* table = dialog->findChild<QTableWidget*>("campaignEmpiresTable"); assert(table);
        table->item(0,0)->setText("Fresh Terrans");
        auto* trait = qobject_cast<QComboBox*>(table->cellWidget(0,8)); assert(trait);
        trait->setCurrentIndex(trait->findData(int(PrimaryRaceTrait::Generalist)));
        submitted = true; dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    auto* action = w.findChild<QAction*>("newCampaignAction"); assert(action); action->trigger();
    assert(submitted);
    const auto& fresh = MainWindowTestAccess::state(w);
    assert(fresh.players.size() == 1 && fresh.players.front().name == "Fresh Terrans");
    assert(fresh.players.front().race.primaryTrait == PrimaryRaceTrait::Generalist);
    assert(fresh.planets.front().population == kInitialHomePopulation && !fresh.planets.front().nativeOrbit.population);
    assert(MainWindowTestAccess::orders(w).orders.empty());
    assert(MainWindowTestAccess::campaign(w) != 285 && MainWindowTestAccess::token(w) != 17);
    assert(MainWindowTestAccess::path(w).isEmpty() && !w.windowTitle().contains("replace-native.suns"));
}
void stale_surface_rows_do_not_hide_miner_eta_or_shift_commands()
{
    MainWindow w; MainWindowTestAccess::setup(w); MainWindowTestAccess::staleIndustry(w);
    auto* tree = MainWindowTestAccess::production(w); assert(tree && tree->topLevelItemCount() == 2);
    assert(tree->topLevelItem(0)->text(3) == "Currently skipped");
    assert(tree->topLevelItem(1)->text(3).contains("(+4)"));
    assert(tree->topLevelItem(0)->data(0,Qt::UserRole).toUInt() == 0);
    assert(tree->topLevelItem(1)->data(0,Qt::UserRole).toUInt() == 1);
    auto resolved = MainWindowTestAccess::state(w);
    unsigned paidSupport = 0;
    for (int i=0; i<4; ++i) {
        resolved = TurnProcessor{}.process(resolved,{});
        paidSupport += resolved.planets.front().nativeOrbit.mineralUnitsPaid;
    }
    assert(resolved.fleets.size() == 2 && !resolved.planets.front().industry);
    assert(resolved.planets.front().minerals.germanium + paidSupport == 90);
    tree->setCurrentItem(tree->topLevelItem(1)); MainWindowTestAccess::moveUp(w);
    const auto* reorder = std::get_if<ReorderProductionQueueOrder>(&MainWindowTestAccess::orders(w).orders.back());
    assert(reorder && reorder->fromIndex == 1 && reorder->toIndex == 0);
    assert(tree->topLevelItem(0)->text(3).contains("(+4)") && tree->topLevelItem(1)->text(3) == "Currently skipped");
    MainWindowTestAccess::remove(w);
    const auto& orders = MainWindowTestAccess::orders(w);
    const auto* cancel = std::get_if<CancelProductionOrder>(&orders.orders.back()); assert(cancel && cancel->index == 0);
    resolved = TurnProcessor{}.process(MainWindowTestAccess::state(w),{orders});
    assert(resolved.fleets.size() == 1 && resolved.planets.front().productionQueue.empty());
}
void paid_miner_eta_reserves_support_minerals_after_pending_cargo()
{
    auto state = test::native_miner_waiting_for_ironium_fixture();
    const auto home = state.planets.front().id, transport = state.fleets.back().id;
    const auto boranium = state.planets.front().minerals.boranium;
    state = TurnProcessor{}.process(state, {{1, {
        TransferCargoOrder{{0, transport}, {home, 0}, 0, {16, 0, 0}},
        TransferCargoOrder{{home, 0}, {0, transport}, 0, {0, boranium, 0}}
    }}});
    assert(state.turn == 6 && state.planets.front().nativeOrbit.service == HabitatService::Reserve);
    assert(state.planets.front().minerals.ironium == 58 && state.planets.front().productionQueue.front().remainingCost == 0);
    SaveGameData host; host.state = state; host.mode = SessionMode::Host;
    host.campaignId = 288; host.turnToken = 17; host.playerTokens = {{1,17}};
    host.pendingOrders = {1, {TransferCargoOrder{{0, transport}, {home, 0}, 0, {0, 20, 0}}}};
    host.pendingDescriptions = {"Deliver boranium"};
    QTemporaryDir dir; QString error; const auto path = dir.filePath("support-budget.suns");
    MainWindow w; MainWindowTestAccess::setup(w);
    for (bool enough : {false, true}) {
        auto input = host;
        if (enough) {
            input.pendingOrders.orders.push_back(TransferCargoOrder{{0, transport}, {home, 0}, 0, {4, 0, 0}});
            input.pendingDescriptions.push_back("Deliver remaining ironium");
        }
        assert(write_save_game_file(path, input, error));
        assert(MainWindowTestAccess::load(w, path)); MainWindowTestAccess::refreshHome(w);
        auto* tree = MainWindowTestAccess::production(w); assert(tree && tree->topLevelItemCount() == 1);
        auto* row = tree->topLevelItem(0);
        assert(row->text(3).contains("(+1)") == enough);
        assert(row->text(2) == (enough ? "0" : "minerals"));
        const auto details = row->toolTip(0);
        assert(details.contains(enough
            ? "Shortfall after life support, before other builds: I 0.0 / B 0.0 / G 0.0 kt"
            : "Shortfall after life support, before other builds: I 4.0 / B 0.0 / G 0.0 kt"));
        const auto summary = MainWindowTestAccess::productionSummary(w);
        assert(summary.contains("Stock after life support"));
        assert(summary.contains(enough ? "I 58.0 / B 16.0 / G 83.0 kt" : "I 54.0 / B 16.0 / G 83.0 kt"));
        const auto actual = TurnProcessor{}.process_with_events(input.state, {input.pendingOrders});
        const auto& planet = actual.state.planets.front();
        assert(planet.nativeOrbit.service == HabitatService::Normal && planet.nativeOrbit.mineralUnitsPaid == 4);
        assert(actual.state.fleets.size() == (enough ? 3 : 2));
        assert(planet.productionQueue.empty() == enough);
        assert(planet.minerals.ironium == (enough ? 0 : 54));
        if (!enough) {
            const auto eta = row->text(3);
            MainWindowTestAccess::hiddenShortage(w); MainWindowTestAccess::refreshHome(w);
            assert(MainWindowTestAccess::productionSummary(w) == summary);
            assert(MainWindowTestAccess::production(w)->topLevelItem(0)->text(3) == eta);
            assert(MainWindowTestAccess::production(w)->topLevelItem(0)->toolTip(0) == details);
            if (qEnvironmentVariableIsSet("SUNS_BUDGET_SCREENSHOT")) {
                w.resize(1440,900); QCoreApplication::processEvents();
                w.grab().save(qEnvironmentVariable("SUNS_BUDGET_SCREENSHOT"));
            }
        }
    }
}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    const auto mode = app.arguments().value(1);
    if (mode.isEmpty() || mode=="design-one") designer_selection_queues_the_same_pending_project(false);
    if (mode.isEmpty() || mode=="design-two") designer_selection_queues_the_same_pending_project(true);
    if (mode.isEmpty() || mode=="design-foreign") designer_selection_queues_the_same_pending_project(false, true);
    if (mode.isEmpty() || mode=="industry") native_industry_is_unavailable_in_the_actual_production_panel();
    if (mode.isEmpty() || mode=="restart") loaded_native_start_cannot_restart_as_a_generalist();
    if (mode.isEmpty() || mode=="restart-menu") loaded_native_start_cannot_restart_as_a_generalist(true);
    if (mode.isEmpty() || mode=="new-campaign") a_loaded_native_host_can_create_an_explicit_generalist_campaign();
    if (mode.isEmpty() || mode=="stale-queue") stale_surface_rows_do_not_hide_miner_eta_or_shift_commands();
    if (mode.isEmpty() || mode=="support") life_support_is_visible_and_received();
    if (mode.isEmpty() || mode=="support-budget") paid_miner_eta_reserves_support_minerals_after_pending_cargo();
}
