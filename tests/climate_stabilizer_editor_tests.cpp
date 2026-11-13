#include "main_window.hpp"
#include "climate_stabilizer_fixture.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <cassert>
using namespace suns;using E=PlanetaryEngineeringKind;
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w,GameState s) {
        set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
        w.state_=std::move(s);w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.star=1;w.rebuildScene();
    }
    static const PlayerOrders& orders(const MainWindow& w){return w.pendingOrders_;}
    static const GameState& state(const MainWindow& w){return w.state_;}
    static bool canTerraform(MainWindow& w){w.refreshProductionQueue();return w.productionControls_.canTerraform;}
    static void hidden(MainWindow& w) {
        w.state_.planets.front().engineering.targetTemperature=77;w.state_.planets.front().minerals={};
        w.state_.orbitalStations.clear();refresh_planetary_engineering(w.state_);w.rebuildScene();
    }
    static void receive(MainWindow& w){++w.state_.turn;set_survey_level(w.state_,1,1,SurveyLevel::DeepSurvey,w.state_.turn);w.rebuildScene();}
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);MainWindow w;w.installUiPolish();w.installProductionQueue();w.installPlanetaryEngineering();MainWindowTestAccess::load(w,test::climate_fixture());
    auto* kind=w.findChild<QComboBox*>("engineeringKind");auto* target=w.findChild<QSpinBox*>("engineeringTargetTemperature");
    auto* output=w.findChild<QSpinBox*>("engineeringOutput");auto* start=w.findChild<QPushButton*>("engineeringStart");
    auto* effect=w.findChild<QLabel*>("engineeringEffect");auto* status=w.findChild<QLabel*>("engineeringStatus");
    assert(kind && target && output && start && effect && target->isHidden());
    kind->setCurrentIndex(kind->findData(int(E::ClimateStabilizer)));assert(!target->isHidden() && start->isEnabled());
    target->setValue(46);output->setValue(6);assert(effect->text().contains("Target temperature: 46") && effect->text().contains("4800 work"));
    assert(!effect->text().contains("Estimated completion"));start->click();
    const auto& initial=std::get<SetPlanetaryEngineeringOrder>(MainWindowTestAccess::orders(w).orders.front());
    assert(initial.kind==E::ClimateStabilizer && initial.targetTemperature==46 && initial.strength==6);
    assert(MainWindowTestAccess::state(w).planets.front().engineering.kind==E::None);
    target->setValue(58);output->setValue(3);w.findChild<QPushButton*>("engineeringApply")->click();
    const auto applied=TurnProcessor{}.process(MainWindowTestAccess::state(w),{MainWindowTestAccess::orders(w)});
    assert(applied.planets.front().engineering.work==80 && applied.planets.front().engineering.targetTemperature==58);
    assert(applied.planets.front().engineering.strength==3);
    auto complete=test::climate_fixture();test::finish_climate(complete,48);MainWindowTestAccess::load(w,complete);
    assert(target->value()==48);const auto text=status->text(),before=effect->text();
    const auto capture=qEnvironmentVariable("SUNS_CLIMATE_STABILIZER_SCREENSHOT");
    if(!capture.isEmpty()) {
        auto* dock=w.findChild<QDockWidget*>("planetaryEngineeringDock");dock->setFloating(true);dock->resize(960,1000);dock->show();
        app.processEvents();assert(dock->grab().save(capture));
    }
    MainWindowTestAccess::hidden(w);assert(target->value()==48 && status->text()==text && effect->text()==before);
    MainWindowTestAccess::receive(w);assert(target->value()==77 && status->text().contains("Starbase"));
    auto foreign=complete;auto other=foreign.players.front();other.id=2;other.name="Other";
    foreign.players.push_back(other);foreign.planets.front().owner=2;foreign.orbitalStations.front().owner=2;
    MainWindowTestAccess::load(w,foreign);assert(target->value()==48 && !target->isEnabled() && !start->isEnabled());
    const auto foreignEffect=effect->text();MainWindowTestAccess::hidden(w);assert(target->value()==48 && effect->text()==foreignEffect);
    auto locked=test::climate_fixture();locked.players.front().technology.levels[std::size_t(ResearchField::Biology)]=17;
    MainWindowTestAccess::load(w,locked);kind->setCurrentIndex(kind->findData(int(E::ClimateStabilizer)));
    assert(!start->isEnabled() && status->text().contains("Biology 18"));
    auto plateau=test::climate_fixture();test::finish_climate(plateau,46);
    auto& p=plateau.planets.front();
    assert(apply_planetary_engineering_order(plateau,p,{1,E::ClimateStabilizer,EngineeringAction::SetOutput,3,46}));
    p.environment.temperature=45;p.naturalEnvironment=p.environment;p.observedHabitability.reset();
    MainWindowTestAccess::load(w,plateau);
    assert(MainWindowTestAccess::canTerraform(w));
    assert(MainWindowTestAccess::state(w).planets.front().environment.temperature==45);
    assert(apply_planetary_engineering_order(plateau,p,{1,E::ClimateStabilizer,EngineeringAction::SetOutput,10,55}));
    plateau.players.front().race.habitableTemperature={60,80};
    MainWindowTestAccess::load(w,plateau);assert(!MainWindowTestAccess::canTerraform(w));
    plateau.players.front().race.habitableTemperature={30,71};
    p.environment.temperature=55;p.naturalEnvironment=p.environment;
    assert(apply_planetary_engineering_order(plateau,p,{1,E::ClimateStabilizer,EngineeringAction::SetOutput,10,51}));
    MainWindowTestAccess::load(w,plateau);assert(!MainWindowTestAccess::canTerraform(w));
    assert(apply_planetary_engineering_order(plateau,p,{1,E::ClimateStabilizer,EngineeringAction::SetOutput,10,52}));
    MainWindowTestAccess::load(w,plateau);assert(MainWindowTestAccess::canTerraform(w));
}
