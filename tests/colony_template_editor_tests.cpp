#include "main_window.hpp"
#include "colony_plan_combo.hpp"
#include "route_program_dock.hpp"
#include "suns/communications.hpp"
#include "suns/production.hpp"

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
#include <QTimer>
#include <QTreeWidget>
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
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
};
}

void settle()
{
    QEventLoop loop;
    QTimer::singleShot(220, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
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
    assert(steps->topLevelItem(0)->text(0) == "Factories");
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
}
