#include "main_window.hpp"
#include "orbital_base_fixture.hpp"
#include "route_program_dock.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <cassert>
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w) { w.state_=orbital_test::world(); w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.rebuildScene(); }
    static void anchored(MainWindow& w) { orbital_test::deploy(w.state_); orbital_test::acknowledge(w.state_);w.rebuildScene(); }
    static const auto& orders(MainWindow& w) { return w.pendingOrders_.orders; }
    static void clear(MainWindow& w) { w.pendingOrders_.orders.clear();w.pendingDescriptions_.clear();w.rebuildScene(); }
    static void hidden(MainWindow& w) {
        auto& f=orbital_test::base(w.state_);f.orbitalBase.assemblyYears=6;f.orbitalBase.reserveYears=0;
        f.orbitalBase.background=100;f.orbitalBase.queue.push_back({201,3});f.colonists=1;f.minerals={};
        w.rebuildScene();
    }
    static void remoteLaunch(MainWindow& w) {
        w.state_=orbital_test::world(5000);orbital_test::deploy(w.state_);
        auto& yard=orbital_test::base(w.state_);yard.orbitalBase.assemblyYears=6;yard.orbitalBase.supported=true;
        orbital_test::acknowledge(w.state_);
        apply_orbital_base_command(w.state_,yard,{OrbitalBaseAction::QueueShip,0,201,101});
        service_orbital_bases(w.state_,w.state_.turn);
        assert(w.state_.fleets.size()==2 && unreported_orbital_ship(w.state_,w.state_.fleets.back()));
        assert(make_player_view(w.state_,1).state.fleets.size()==1);
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.fleet=1;w.selection_.star=2;w.rebuildScene();
    }
    static void deliverLaunch(MainWindow& w) { orbital_test::acknowledge(w.state_);w.rebuildScene(); }
    static void colony(MainWindow& w) {
        w.state_.planets[1].owner=1;w.state_.planets[1].population=10000;
        w.rebuildScene();
    }
    static void transport(MainWindow& w) {
        auto& s=w.state_;auto f=s.fleets[0];f.id=2;f.design=202;f.ships={{202,1}};f.outpost=OutpostKind::None;f.orbitalBase={};f.name="Supply transport";
        f.telemetry={};s.fleets.push_back(f);s.nextFleetId=3;orbital_test::acknowledge(s);w.rebuildScene();
    }
};
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);using namespace suns;MainWindow w;MainWindowTestAccess::setup(w);w.installOrbitalBases();
    auto* bases=w.findChild<QComboBox*>("orbitalBaseSelection");auto* planets=w.findChild<QComboBox*>("orbitalBasePlanet");
    auto* status=w.findChild<QLabel*>("orbitalBaseStatus");auto* queue=w.findChild<QListWidget*>("orbitalBaseQueue");
    auto* deploy=w.findChild<QPushButton*>("orbitalBaseDeploy");
    assert(bases && bases->count()==1 && planets && planets->count()>0 && status && queue && deploy);
    planets->setCurrentIndex(planets->findData(2u));assert(status->text().contains("exposure 30"));
    w.findChild<QPushButton*>("orbitalBaseApproach")->click();assert(std::holds_alternative<MoveFleetOrder>(MainWindowTestAccess::orders(w).back()));
    MainWindowTestAccess::clear(w);deploy->click();
    assert(std::get<OrbitalBaseOrder>(MainWindowTestAccess::orders(w).back()).command.planet==2);
    MainWindowTestAccess::clear(w);MainWindowTestAccess::anchored(w);assert(!deploy->isEnabled());
    auto* designs=w.findChild<QComboBox*>("orbitalBaseDesign");designs->setCurrentIndex(designs->findData(201u));
    w.findChild<QPushButton*>("orbitalBaseBuild")->click();assert(queue->count()==1);
    assert(std::get<OrbitalBaseOrder>(MainWindowTestAccess::orders(w).back()).command.action==OrbitalBaseAction::QueueShip);
    queue->setCurrentRow(0);w.findChild<QPushButton*>("orbitalBaseCancel")->click();assert(queue->count()==0);
    MainWindowTestAccess::clear(w);
    const auto text=status->text();MainWindowTestAccess::hidden(w);assert(status->text()==text && queue->count()==0);
    w.findChild<QPushButton*>("orbitalBasePause")->click();assert(!std::get<OrbitalBaseOrder>(MainWindowTestAccess::orders(w).back()).command.active);
    MainWindowTestAccess::clear(w);MainWindowTestAccess::transport(w);
    auto* people=w.findChild<QSpinBox*>("orbitalBasePeople");people->setValue(1000);
    w.findChild<QPushButton*>("orbitalBaseEvacuate")->click();
    const auto cargo=std::get<TransferCargoOrder>(MainWindowTestAccess::orders(w).back());
    assert(cargo.source.fleet==1 && cargo.destination.fleet==2 && cargo.colonists==1000);
    MainWindowTestAccess::clear(w);
    auto* dismantle=w.findChild<QPushButton*>("orbitalBaseDismantle");
    auto* replace=w.findChild<QPushButton*>("orbitalBaseRebuildStation");
    auto* cancelRetirement=w.findChild<QPushButton*>("orbitalBaseCancelRetirement");
    assert(dismantle && dismantle->isEnabled() && replace && !replace->isEnabled());
    dismantle->click();
    const auto retire=std::get<OrbitalBaseOrder>(MainWindowTestAccess::orders(w).back()).command;
    assert(retire.action==OrbitalBaseAction::Decommission && retire.receiver==2 && retire.item);
    assert(cancelRetirement->isEnabled() && status->text().contains("Evacuate all residents"));
    assert(!w.findChild<QPushButton*>("orbitalBaseBuild")->isEnabled());
    cancelRetirement->click();
    const auto cancelled=std::get<OrbitalBaseOrder>(MainWindowTestAccess::orders(w).back()).command;
    assert(cancelled.action==OrbitalBaseAction::CancelRetirement && cancelled.item==retire.item);
    assert(dismantle->isEnabled());
    MainWindowTestAccess::clear(w);MainWindowTestAccess::colony(w);assert(replace->isEnabled());
    replace->click();
    const auto rebuild=std::get<OrbitalBaseOrder>(MainWindowTestAccess::orders(w).back()).command;
    assert(rebuild.action==OrbitalBaseAction::RebuildStation && rebuild.station && rebuild.receiver==2);
    MainWindow remote;MainWindowTestAccess::remoteLaunch(remote);attachRouteProgramDock(remote);
    assert(remote.availableOwnedFleetsForRouteProgram()==std::vector<FleetId>{1});
    assert(remote.availableFleetTargetsForRouteProgram().empty());
    assert(!remote.selectFleetForRouteProgram(2));
    assert(remote.fleetTargetNameForRouteProgram(2)=="Fleet 2");
    auto* sources=remote.findChild<QComboBox*>("routeSourceFleetCombo");
    auto* targets=remote.findChild<QComboBox*>("routeTargetFleetCombo");assert(sources && targets);
    assert(sources->findData(2u)<0 && targets->findData(2u)<0);
    MainWindowTestAccess::deliverLaunch(remote);
    assert(remote.availableOwnedFleetsForRouteProgram()==(std::vector<FleetId>{1,2}));
    assert(remote.availableFleetTargetsForRouteProgram()==std::vector<FleetId>{2});
    assert(sources->findData(2u)>=0 && remote.selectFleetForRouteProgram(2));
}
