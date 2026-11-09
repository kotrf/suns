#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include "native_orbital_fixture.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolButton>
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
    static void surfaceTemplate(MainWindow& w, bool ordinary = false) {
        if (ordinary) w.state_ = generate_campaign({}, {{"Terrans"}});
        w.state_.players.front().productionTemplates = {{"Surface", {{ProductionKind::Factory, kFactoryCost}}}};
        w.pendingOrders_ = {1,{}}; w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star; w.rebuildScene(); w.refreshProductionQueue();
    }
    static bool factoryButtonEnabled(const MainWindow& w) { return w.buildFactoryButton_->isEnabled(); }
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
    assert(resolved.planets.front().minerals.germanium == 101);
    MainWindowTestAccess::surfaceTemplate(w, true);
    assert(MainWindowTestAccess::factoryButtonEnabled(w));
    assert(apply->isEnabled());
    for (int i=0; i<kinds->count(); ++i)
        assert(kinds->model()->flags(kinds->model()->index(i,0)) & Qt::ItemIsEnabled);
}
void loaded_native_start_cannot_restart_as_a_generalist()
{
    auto state = test::empty_native_orbital_start();
    assert(initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget()));
    SaveGameData save; save.state = state; save.mode = SessionMode::Host; save.campaignId = 285;
    save.turnToken = 17; save.playerTokens = {{1,17}}; save.pendingOrders = {1,{SetResearchAllocationOrder{17}}};
    save.pendingDescriptions = {"Research allocation 17%"};
    QTemporaryDir dir; QString error; const auto path = dir.filePath("native-host.suns");
    assert(write_save_game_file(path, save, error));
    SaveGameData decoded; assert(read_save_game_file(path, decoded, error));
    MainWindow w; assert(MainWindowTestAccess::load(w,path));
    const auto campaign = MainWindowTestAccess::campaign(w), token = MainWindowTestAccess::token(w);
    MainWindowTestAccess::restart(w);
    const auto& after = MainWindowTestAccess::state(w);
    assert(after.players.front().race.primaryTrait == PrimaryRaceTrait::HabitatCivilization);
    assert(after.planets.front().nativeOrbit.population == kInitialHomePopulation && !after.planets.front().population);
    assert(after.turn == state.turn && after.orbitalStations.size() == 1);
    assert(MainWindowTestAccess::campaign(w) == campaign && MainWindowTestAccess::token(w) == token);
    assert(MainWindowTestAccess::orders(w).orders.size() == 1);
    assert(w.statusBar()->currentMessage().contains("not playable", Qt::CaseInsensitive));
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
}
