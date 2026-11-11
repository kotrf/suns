#include "main_window.hpp"
#include "colony_plan_combo.hpp"
#include "route_program_dock.hpp"
#include "suns/communications.hpp"
#include "suns/production.hpp"
#include "native_orbital_fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <algorithm>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w) {
        w.state_ = make_demo_game();
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        auto& ship = w.state_.fleets.front();
        ship.design = kColonyShipDesignId; ship.ships = {{kColonyShipDesignId, 1}};
        ship.role = FleetRole::ColonyShip; ship.colonists = 100;
        ship.destination = w.state_.stars[1].position;
        ship.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Colonize};
        w.selection_.fleet = ship.id; w.selection_.star = w.state_.stars[1].id;
        publish_fleet_telemetry(w.state_, w.state_.turn);
        w.rebuildScene();
        emit w.routeProgramContextChanged(true);
    }
    static const MoveFleetOrder* move(const MainWindow& w) {
        for (const auto& order : w.pendingOrders_.orders) if (const auto* move = std::get_if<MoveFleetOrder>(&order)) return move;
        return nullptr;
    }
    static void manualFixture(MainWindow& w) {
        w.state_ = make_demo_game();
        w.state_.players.front().productionTemplates = {colony_development_presets().front()};
        auto& ship = w.state_.fleets.front();
        ship.design = kColonyShipDesignId; ship.ships = {{kColonyShipDesignId, 1}};
        ship.role = FleetRole::ColonyShip; ship.colonists = 100;
        ship.position = w.state_.stars[1].position;
        w.selection_.star = w.state_.stars[1].id; w.selection_.fleet = ship.id;
        set_survey_level(w.state_, 1, w.state_.stars[1].id, SurveyLevel::GeologicalSurvey, w.state_.turn);
        publish_fleet_telemetry(w.state_, w.state_.turn);
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear(); w.rebuildScene();
    }
    static void colonize(MainWindow& w) { w.queueColonize(); }
    static void nativeFixture(MainWindow& w) {
        w.state_ = test::native_orbital_founding_fixture();
        w.state_.planets[1].environment = {0, 100, 20};
        w.selection_.fleet = w.state_.fleets.back().id; w.selection_.star = w.state_.planets[1].star;
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear(); w.rebuildScene();
    }
    static bool overcrowded(MainWindow& w) {
        auto fleet = w.state_.fleets.back(); fleet.ships.front().count = 50; fleet.colonists = 100'001;
        return w.confirmFleetColonization(fleet, w.state_.planets[1], false);
    }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static const GameState& game(const MainWindow& w) { return w.state_; }
    static void select(MainWindow& w, FleetId fleet, StarId star) {
        w.selection_.fleet = fleet; w.selection_.star = star; w.rebuildScene();
    }
    static void replace(MainWindow& w, GameState state) {
        w.state_ = std::move(state); w.pendingOrders_ = {1,{}}; w.pendingDescriptions_.clear();
        select(w,w.state_.fleets.back().id,w.state_.planets[1].star);
    }
    static bool load(MainWindow& w, const QString& path) { return w.loadGameFromPath(path); }
    static void nativePlans(MainWindow& w) {
        nativeFixture(w);
        w.state_.players.front().productionTemplates = {colony_development_presets().front(),
            {"Receiver plan", {{ProductionKind::LocalRadar,production_cost(ProductionKind::LocalRadar)}}}};
    }
    static void legacyTemplate(MainWindow& w) {
        nativeFixture(w);
        w.state_.players.front().productionTemplates = {{"Legacy Terraform", {
            {ProductionKind::Terraforming, kTerraformingCost, 0, ProductionAutomation::MinTerraform, 1},
            {ProductionKind::LocalRadar, production_cost(ProductionKind::LocalRadar)}}}};
        w.pendingOrders_.orders.push_back(SetProductionTemplateOrder{{"Rejected pending edit", {
            {ProductionKind::Terraforming, kTerraformingCost, 0, ProductionAutomation::MinTerraform, 1}}}});
    }
};
}

void settle()
{
    QEventLoop loop;
    QTimer::singleShot(220, &loop, &QEventLoop::quit);
    loop.exec();
}

namespace {
using namespace suns;
void native_template_editor_blocks_bad_steps_and_repairs_legacy_programs()
{
    {
        MainWindow w; MainWindowTestAccess::nativeFixture(w);
        w.openColonyTemplates(); w.show(); settle();
        auto* dialog = w.findChild<QDialog*>("colonyTemplatesDialog"); assert(dialog);
        auto* name = dialog->findChild<QLineEdit*>("colonyTemplateName");
        auto* kind = dialog->findChild<QComboBox*>("colonyTemplateStepKind");
        auto* mode = dialog->findChild<QComboBox*>("colonyTemplateStepMode");
        auto* add = dialog->findChild<QPushButton*>("colonyTemplateStepAdd");
        auto* save = dialog->findChild<QPushButton*>("colonyTemplateSave");
        name->setText("Unsuitable program");
        const auto index = kind->findData(int(ProductionKind::Terraforming)); assert(index >= 0);
        kind->setCurrentIndex(index);
        mode->setCurrentIndex(mode->findData(int(ProductionAutomation::MinTerraform)));
        add->click();
        assert(!save->isEnabled()); // The old editor accepted a new Min Terraform-only program here.
        assert(!add->isEnabled());
        assert(!(kind->model()->flags(kind->model()->index(index, 0)) & Qt::ItemIsEnabled));
        save->click();
        assert(MainWindowTestAccess::orders(w).orders.empty() && w.colonyTemplatesForOrders().empty());
    }
    for (bool replace : {false, true}) {
        MainWindow w; MainWindowTestAccess::legacyTemplate(w);
        assert(w.colonyTemplatesForOrders().size() == 1); // Pending legacy-incompatible edits use the real race.
        w.openColonyTemplates(); w.show(); settle();
        auto* dialog = w.findChild<QDialog*>("colonyTemplatesDialog"); assert(dialog);
        auto* list = dialog->findChild<QListWidget*>("colonyTemplateList");
        auto* steps = dialog->findChild<QTreeWidget*>("colonyTemplateSteps");
        auto* kind = dialog->findChild<QComboBox*>("colonyTemplateStepKind");
        auto* mode = dialog->findChild<QComboBox*>("colonyTemplateStepMode");
        auto* save = dialog->findChild<QPushButton*>("colonyTemplateSave");
        list->setCurrentRow(0);
        assert(steps->topLevelItemCount() == 2 && !save->isEnabled());
        assert(dialog->findChild<QLabel*>("colonyTemplateStatus")->text().contains("unavailable", Qt::CaseInsensitive));
        steps->setCurrentItem(steps->topLevelItem(0));
        assert(steps->topLevelItem(0)->text(0).contains("unavailable", Qt::CaseInsensitive));
        auto* remove = dialog->findChild<QPushButton*>("colonyTemplateStepRemove");
        auto* update = dialog->findChild<QPushButton*>("colonyTemplateStepUpdate");
        assert(remove->isEnabled() && !update->isEnabled());
        if (replace) {
            kind->setCurrentIndex(kind->findData(int(ProductionKind::PlanetaryRadar)));
            mode->setCurrentIndex(mode->findData(int(ProductionAutomation::None)));
            assert(update->isEnabled()); update->click();
        } else remove->click();
        assert(save->isEnabled()); save->click();
        const auto repaired = w.colonyTemplatesForOrders();
        assert(repaired.size() == 1 && repaired.front().name == "Legacy Terraform");
        assert(repaired.front().items.size() == (replace ? 2 : 1));
        assert(production_template_applicable(PrimaryRaceTrait::HabitatCivilization, repaired.front()));
        const auto actual = TurnProcessor{}.process(MainWindowTestAccess::game(w), {MainWindowTestAccess::orders(w)});
        assert(actual.players.front().productionTemplates == repaired);
    }
}

void native_plan_choices_are_applicable(bool route)
{
    MainWindow w; MainWindowTestAccess::nativePlans(w);
    if (route) {
        attachRouteProgramDock(w); w.show(); settle();
        auto* combo = static_cast<ColonyPlanCombo*>(w.findChild<QComboBox*>("routeColonyPlanCombo"));
        assert(combo && combo->findText("Fast Link") < 0 && combo->findText("Receiver plan") > 0);
        ProductionTemplate legacy{"Legacy surface plan", {{ProductionKind::Factory,kFactoryCost}}};
        combo->setPlan(legacy);
        assert(combo->plan() == legacy && combo->currentText().contains("unavailable",Qt::CaseInsensitive));
        assert(!(combo->model()->flags(combo->model()->index(combo->currentIndex(),0)) & Qt::ItemIsEnabled));
        combo->setCurrentIndex(0); assert(!combo->plan());
        return;
    }
    bool sawPlan = false;
    QTimer answer;
    QObject::connect(&answer,&QTimer::timeout,&w,[&] {
        if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            warning->button(QMessageBox::Yes)->click();
        else if (auto* choice = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            choice && choice->objectName() == "colonizePlanDialog") {
            sawPlan = true;
            auto* plans = choice->findChild<QComboBox*>("colonizePlanCombo");
            assert(plans && plans->findText("Fast Link") < 0 && plans->findText("Receiver plan") > 0);
            plans->setCurrentIndex(plans->findText("Receiver plan")); choice->accept();
        }
    });
    answer.start(1); MainWindowTestAccess::colonize(w); answer.stop();
    assert(sawPlan);
    const auto& order = std::get<ColonizePlanetOrder>(MainWindowTestAccess::orders(w).orders.back());
    assert(order.colonyPlan && order.colonyPlan->name == "Receiver plan");
}
void colonize_waypoints_use_received_ownership()
{
    auto state = test::native_orbital_founding_fixture();
    const auto source = state.fleets.back().id, target = state.planets[1].id;
    auto second = state.fleets.back(); second.id = state.nextFleetId++;
    second.position = state.stars.front().position; second.telemetry = {};
    state.fleets.push_back(second);
    state.fleets.back().telemetry = confirmed_fleet_telemetry(state,state.fleets.back());
    const Position remote{400,0}; state.stars[1].position = remote;
    const PlayerOrders travel{1,{MoveFleetOrder{source,remote,5,{FleetArrivalActionKind::Colonize}}}};
    for (unsigned year=0;year<40 && !state.planets[1].nativeOrbit.station;++year)
        state = TurnProcessor{}.process(state,year==0?std::vector<PlayerOrders>{travel}:std::vector<PlayerOrders>{});
    assert(state.planets[1].owner == 1 && known_planet_owner(state,1,target) == 0);
    const auto report = std::find_if(state.players.front().pendingPlayerReports.begin(),
        state.players.front().pendingPlayerReports.end(),[](const auto& r){return r.kind == PlayerReportKind::ColonyFounded;});
    assert(report != state.players.front().pendingPlayerReports.end() && report->deliveryTurn > state.turn);
    const auto due = report->deliveryTurn;
    QTemporaryDir dir; QString error;
    for (const bool delivered : {false,true}) {
        if (delivered) while (state.turn<=due+1) state=TurnProcessor{}.process(state,{});
        for (const bool pbem : {true,false}) {
            SaveGameData save; save.state=state; save.mode=SessionMode::Host;
            save.pendingOrders={1,{}}; save.campaignId=286; save.turnToken=17; save.playerTokens={{1,17}};
            const auto path=dir.filePath(pbem?"player.suns":"host.suns");
            assert(write_save_game_file(path,pbem?make_player_turn(save,1):save,error));
            MainWindow w; assert(MainWindowTestAccess::load(w,path));
            MainWindowTestAccess::select(w,second.id,state.planets[1].star);
            assert(known_planet_owner(MainWindowTestAccess::game(w),1,target) == (delivered?1:0));
            bool confirmed=false;
            QTimer answer;
            QObject::connect(&answer,&QTimer::timeout,&w,[&] {
                auto* warning=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); assert(warning);
                confirmed=true; warning->button(QMessageBox::Yes)->click();
            });
            answer.start(1);
            const bool accepted=w.appendSelectedStarWaypoint(5,{FleetArrivalActionKind::Colonize},false);
            answer.stop();
            assert(accepted == !delivered && confirmed == !delivered);
            if (delivered) assert(w.statusBar()->currentMessage().contains("unowned"));
            else assert(MainWindowTestAccess::move(w)
                && MainWindowTestAccess::move(w)->arrivalAction.kind == FleetArrivalActionKind::Colonize);
        }
    }
}
void an_unloading_route_can_plan_before_the_housing_limit_is_met()
{
    auto state=test::native_orbital_founding_fixture();
    state.planets.front().nativeOrbit.population=90'000;
    auto& fleet=state.fleets.back(); const auto id=fleet.id;
    fleet.ships.front().count=50; fleet.colonists=100'001;
    fleet.position=state.stars.front().position; fleet.fuel=fleet_fuel_capacity(state,fleet);
    fleet.telemetry={}; fleet.telemetry=confirmed_fleet_telemetry(state,fleet);
    assert(fleet_cargo_used(state,fleet)<=fleet_cargo_capacity(state,fleet));
    MainWindow w; MainWindowTestAccess::replace(w,state);
    MainWindowTestAccess::select(w,id,state.planets.front().star);
    assert(w.appendSelectedStarWaypoint(5,{FleetArrivalActionKind::UnloadAll,0,FleetCargoKind::Colonists},false));
    MainWindowTestAccess::select(w,id,state.planets[1].star);
    bool confirmed=false;
    QTimer answer;
    QObject::connect(&answer,&QTimer::timeout,&w,[&] {
        auto* warning=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); assert(warning);
        assert(warning->informativeText().contains("100001 / 100000"));
        assert(warning->informativeText().contains("before arrival",Qt::CaseInsensitive));
        confirmed=true; warning->button(QMessageBox::Yes)->click();
    });
    answer.start(1); const bool accepted=w.appendSelectedStarWaypoint(5,{FleetArrivalActionKind::Colonize},false); answer.stop();
    assert(accepted && confirmed);
    const auto* move=MainWindowTestAccess::move(w);
    assert(move && move->arrivalAction.kind == FleetArrivalActionKind::UnloadAll
        && move->queuedWaypoints.size()==1 && move->queuedWaypoints.back().arrivalAction.kind==FleetArrivalActionKind::Colonize);
    auto result=TurnProcessor{}.process(state,{MainWindowTestAccess::orders(w)});
    assert(result.planets.front().nativeOrbit.population==100'000);
    for(unsigned year=0;year<4 && !result.planets[1].nativeOrbit.station;++year)
        result=TurnProcessor{}.process(result,{});
    assert(result.planets[1].nativeOrbit.population==90'001);
    assert(std::none_of(result.fleets.begin(),result.fleets.end(),[&](const auto& f){return f.id==id;}));
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (app.arguments().contains("native-template")) { native_template_editor_blocks_bad_steps_and_repairs_legacy_programs(); return 0; }
    if (app.arguments().contains("plans-manual")) { native_plan_choices_are_applicable(false); return 0; }
    if (app.arguments().contains("plans-route")) { native_plan_choices_are_applicable(true); return 0; }
    if (app.arguments().contains("ownership")) { colonize_waypoints_use_received_ownership(); return 0; }
    if (app.arguments().contains("unloading")) { an_unloading_route_can_plan_before_the_housing_limit_is_met(); return 0; }
    suns::MainWindow window;
    window.installDeferredMapSelectionHandler();
    suns::attachRouteProgramDock(window);
    window.installUiPolish();
    window.installProductionQueue();
    suns::MainWindowTestAccess::install(window);
    window.show(); settle();
    assert(window.colonyTemplatesForOrders().empty());
    auto* open = window.findChild<QAction*>("openColonyTemplatesToolAction");
    assert(open); open->trigger();
    auto* dialog = window.findChild<QDialog*>("colonyTemplatesDialog");
    assert(dialog && !dialog->isModal() && dialog->isWindow());
    auto* list = dialog->findChild<QListWidget*>("colonyTemplateList");
    auto* preset = dialog->findChild<QComboBox*>("colonyTemplatePreset");
    auto* use = dialog->findChild<QPushButton*>("colonyTemplateUsePreset");
    auto* name = dialog->findChild<QLineEdit*>("colonyTemplateName");
    auto* steps = dialog->findChild<QTreeWidget*>("colonyTemplateSteps");
    auto* qty = dialog->findChild<QSpinBox*>("colonyTemplateStepQuantity");
    auto* update = dialog->findChild<QPushButton*>("colonyTemplateStepUpdate");
    auto* save = dialog->findChild<QPushButton*>("colonyTemplateSave");
    assert(list && preset && use && name && steps && qty && update && save);
    assert(list->count() == 0 && preset->currentIndex() == 0 && !use->isEnabled() && !save->isEnabled());
    preset->setCurrentIndex(preset->findText("Industry First")); use->click();
    assert(steps->topLevelItemCount() == 5 && name->text() == "Industry First");
    steps->setCurrentItem(steps->topLevelItem(0)); qty->setValue(4); update->click();
    assert(steps->topLevelItem(0)->text(1) == "4");
    dialog->findChild<QPushButton*>("colonyTemplateStepDown")->click();
    assert(steps->topLevelItem(0)->text(0).contains("Radar"));
    dialog->findChild<QPushButton*>("colonyTemplateStepUp")->click();
    assert(steps->topLevelItem(0)->text(0) == "Basic factories");
    assert(save->isEnabled()); save->click();
    const auto snapshot = window.colonyTemplatesForOrders().front();
    assert(snapshot.items.front().quantity == 4 && !snapshot.defaultForNewColonies);
    auto* route = window.findChild<QTreeWidget*>("routeProgramQueue");
    assert(route && route->topLevelItemCount() == 1);
    route->setCurrentItem(route->topLevelItem(0));
    auto* combo = static_cast<suns::ColonyPlanCombo*>(window.findChild<QComboBox*>("routeColonyPlanCombo"));
    assert(combo && !combo->plan());
    combo->setCurrentIndex(combo->findText("Industry First"));
    const auto* move = suns::MainWindowTestAccess::move(window);
    assert(move && move->arrivalAction.colonyPlan && *move->arrivalAction.colonyPlan == snapshot);
    // Renaming and changing the source must preserve the copy already ordered.
    name->setText("Factory-first remote colony");
    steps->setCurrentItem(steps->topLevelItem(0)); qty->setValue(6); update->click(); save->click();
    assert(window.colonyTemplatesForOrders().size() == 1);
    assert(window.colonyTemplatesForOrders().front().name == "Factory-first remote colony");
    move = suns::MainWindowTestAccess::move(window);
    assert(move && *move->arrivalAction.colonyPlan == snapshot);
    assert(combo->plan() && *combo->plan() == snapshot && combo->currentText().contains("order copy"));
    dialog->findChild<QPushButton*>("colonyTemplateDuplicate")->click(); save->click();
    assert(window.colonyTemplatesForOrders().size() == 2);
    dialog->findChild<QPushButton*>("colonyTemplateDelete")->click();
    list->setCurrentRow(0);
    dialog->findChild<QPushButton*>("colonyTemplateDelete")->click();
    assert(window.colonyTemplatesForOrders().empty());
    assert(combo->plan() && *combo->plan() == snapshot);
    combo->setCurrentIndex(0);
    assert(!suns::MainWindowTestAccess::move(window)->arrivalAction.colonyPlan);
    if (const auto path = qEnvironmentVariable("SUNS_COLONY_TEMPLATES_SCREENSHOT"); !path.isEmpty()) {
        preset->setCurrentIndex(preset->findText("Industry First")); use->click();
        settle(); assert(dialog->grab().save(path));
    }
    dialog->close();
    suns::MainWindowTestAccess::manualFixture(window);
    QTimer answer;
    QObject::connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            warning->button(QMessageBox::Yes)->click();
        } else if (auto* choice = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            choice && choice->objectName() == "colonizePlanDialog") {
            auto* plans = choice->findChild<QComboBox*>("colonizePlanCombo");
            assert(plans && plans->currentIndex() == 0);
            plans->setCurrentIndex(plans->findText("Fast Link"));
            choice->accept();
        }
    });
    answer.start(1);
    suns::MainWindowTestAccess::colonize(window);
    answer.stop();
    const auto& direct = std::get<suns::ColonizePlanetOrder>(suns::MainWindowTestAccess::orders(window).orders.back());
    assert(direct.colonyPlan == suns::colony_development_presets().front());
    suns::MainWindowTestAccess::nativeFixture(window);
    bool sawStation = false, sawPlan = false;
    QTimer nativeAnswer;
    QObject::connect(&nativeAnswer, &QTimer::timeout, &window, [&] {
        if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            sawStation = true;
            const auto text = warning->informativeText();
            assert(warning->windowTitle() == "Confirm orbital colony founding");
            assert(text.contains("Residents transferred to station: 1000 / 100000"));
            assert(text.contains("Prepaid seed stock: I 10 / B 10 / G 10"));
            assert(text.contains("excluding prepaid deployment: I 11 / B 5 / G 10"));
            assert(text.contains("including cargo: I 24 / B 17 / G 21"));
            assert(!text.contains("Population will decline") && !text.contains("Colonists landed"));
            if (app.arguments().contains("--capture")) assert(warning->grab().save("/tmp/suns-native-founding-confirmation.png"));
            warning->button(QMessageBox::Yes)->click();
        } else if (auto* choice = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            choice && choice->objectName() == "colonizePlanDialog") {
            sawPlan = true;
            bool explainsKit = false;
            for (auto* label : choice->findChildren<QLabel*>())
                explainsKit |= label->text().contains("paid construction module includes");
            assert(explainsKit);
            choice->accept();
        }
    });
    nativeAnswer.start(10);
    suns::MainWindowTestAccess::colonize(window);
    nativeAnswer.stop();
    assert(sawStation && sawPlan);
    const auto& nativeOrder = std::get<suns::ColonizePlanetOrder>(suns::MainWindowTestAccess::orders(window).orders.back());
    assert(nativeOrder.fleet == 2 && nativeOrder.planet == 2 && !nativeOrder.colonyPlan);
    assert(!suns::MainWindowTestAccess::overcrowded(window));
    native_plan_choices_are_applicable(false);
    native_plan_choices_are_applicable(true);
    colonize_waypoints_use_received_ownership();
    an_unloading_route_can_plan_before_the_housing_limit_is_met();
    native_template_editor_blocks_bad_steps_and_repairs_legacy_programs();
}
