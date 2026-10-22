#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "suns/communications.hpp"
#include "suns/reconnaissance.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <cassert>
#include <cmath>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.state_.planets.front().minerals = {100, 0.25, 100};
        auto& fleet = w.state_.fleets.front();
        fleet.design = kColonyShipDesignId;
        fleet.ships = {{kColonyShipDesignId, 1}};
        fleet.role = FleetRole::ColonyShip;
        fleet.colonists = 100;
        fleet.minerals = {1, 0, 0};
        assert(fleet_cargo_capacity(w.state_, fleet) == 5);
        auto other = fleet;
        other.id = w.state_.nextFleetId++;
        other.name = "Second transport";
        other.colonists = 0;
        other.minerals = {};
        w.state_.fleets.push_back(other);
        set_survey_level(w.state_,1,w.state_.planets.front().star,SurveyLevel::DeepSurvey,w.state_.turn);
        // These local fixture edits represent already received manifests.
        for (auto& received : w.state_.fleets) {
            received.telemetry = {};
            received.telemetry = confirmed_fleet_telemetry(w.state_, received);
        }
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star;
        w.selection_.fleet = 1;
        w.rebuildScene();
    }
    static void installFullTransport(MainWindow& w)
    {
        install(w);
        w.state_.shipDesigns.push_back({3, 1, "Five light transports",
            ShipHullType::LightTransport, {ShipComponentType::FusionDrive}});
        auto& fleet = w.state_.fleets.front();
        fleet.design = 3;
        fleet.ships = {{3, 5}};
        fleet.minerals = {};
        fleet.colonists = 0;
        w.state_.fleets.resize(1);
        assert(fleet_cargo_capacity(w.state_, fleet) == 25);
        fleet.telemetry = {};
        fleet.telemetry = confirmed_fleet_telemetry(w.state_, fleet);
        w.rebuildScene();
    }
    static void installEnemyColony(MainWindow& w)
    {
        install(w);
        w.state_.players.push_back({2, "Enemy"});
        w.state_.planets.front().owner = 2;
        w.state_.planets.front().population = 8'000;
        set_survey_level(w.state_, 1, 1, SurveyLevel::OrbitalSurvey, w.state_.turn);
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.rebuildScene();
    }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static const GameState& state(const MainWindow& w) { return w.state_; }
    static QString systemInfo(const MainWindow& w) { return w.selectionLabel_->text(); }
    static void endTurn(MainWindow& w) { w.endTurn(); }
    static void selectStar(MainWindow& w, std::optional<StarId> star)
    {
        w.selection_.star = star;
        w.rebuildScene();
    }
    static std::optional<StarId> selectedStar(const MainWindow& w) { return w.selection_.star; }
    static void installRadarPosition(MainWindow& w)
    {
        installOldSolReport(w, 100);
        auto& fleet=w.state_.fleets.front();
        fleet.electronics={EmissionMode::RadioSilence,9};
        fleet.telemetry.electronics=fleet.electronics;
        observe_reconnaissance(w.state_,w.state_.turn);
        deliver_reconnaissance(w.state_);
        w.rebuildScene();
    }
    static void installSilentOrbit(MainWindow& w, EmissionMode mode)
    {
        install(w); w.state_.fleets.resize(1);
        auto& fleet=w.state_.fleets.front();
        fleet.electronics={mode,mode==EmissionMode::RadioSilence?20u:0u};
        fleet.telemetry.colonists=0;
        publish_fleet_telemetry(w.state_,w.state_.turn);
        w.selection_.star.reset();
        w.rebuildScene();
    }
    static void installOldSolReport(MainWindow& w, double actualX)
    {
        install(w);
        w.state_.fleets.resize(1);
        w.state_.turn = 5;
        auto& fleet = w.state_.fleets.front();
        fleet.position = {actualX, 0};
        fleet.destination = Position{actualX + 50, 0};
        fleet.telemetry.observedTurn = 1;
        fleet.telemetry.position = w.state_.stars.front().position;
        fleet.telemetry.destination.reset();
        w.selection_.star = w.state_.stars.front().id;
        w.rebuildScene();
    }
};
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    suns::MainWindowTestAccess::install(window);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->windowTitle().startsWith("Transfer cargo —"));
        auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
        auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
        auto* people = dialog->findChild<QSlider*>("cargoColonistSlider");
        auto* exactPeople = dialog->findChild<QSpinBox*>("cargoColonistSpin");
        auto* iron = dialog->findChild<QSlider*>("cargoMineralSlider0");
        auto* boron = dialog->findChild<QSlider*>("cargoMineralSlider1");
        auto* exactIron = dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin0");
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        assert(source && destination && people && exactPeople && iron && boron && exactIron && buttons);
        const auto distinct = [&] {
            assert(destination->findData(source->currentData()) == -1);
            assert(destination->currentData() != source->currentData());
            assert(destination->count() == source->count() - 1);
        };
        distinct();
        // The dialog starts with planet on the left and selected ship on
        // the right regardless of existing cargo; it must never silently
        // reverse into unload mode when an earlier order filled the hold.
        assert(source->currentData().toInt() == 0);
        assert(destination->currentData().toInt() == 1);
        assert(dialog->findChild<QGroupBox*>("cargoSourceContainer"));
        assert(dialog->findChild<QGroupBox*>("cargoDestinationContainer"));
        const auto* destinationPeople = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        assert(destinationPeople && destinationPeople->value() == 100);
        assert(people->maximum() == 300); // 5 kt hold, 2 kt already aboard
        assert(iron->maximum() == 300 && boron->maximum() == 300);
        boron->setValue(boron->maximum());
        assert(boron->value() == 25 && boron->maximum() == 300); // stock limits handle, not scale
        boron->setValue(0);
        iron->setValue(iron->maximum());
        assert(exactIron->value() == 3 && people->maximum() == 0 && boron->maximum() == 0);
        exactIron->setValue(1);
        assert(people->maximum() == 200);
        people->setValue(people->maximum());
        assert(exactPeople->value() == 200 && boron->maximum() == 0);
        exactPeople->setValue(100);
        assert(iron->maximum() == 200 && boron->maximum() == 100);

        // Selecting the old destination as source rebuilds the destination list.
        source->setCurrentIndex(source->findData(1));
        distinct();
        destination->setCurrentIndex(destination->findData(0)); // unload to planet
        assert(people->maximum() == 500 && iron->maximum() == 500);
        people->setValue(people->maximum());
        assert(people->value() == 100 && people->maximum() == 500);
        destination->setCurrentIndex(destination->findData(2)); // fleet-to-fleet
        distinct();
        assert(people->maximum() == 500 && iron->maximum() == 500);
        iron->setValue(iron->maximum());
        assert(iron->value() == 100 && iron->maximum() == 500);
        source->setCurrentIndex(source->findData(2));
        distinct();
        assert(people->maximum() == 500 && iron->maximum() == 500);
        people->setValue(people->maximum());
        iron->setValue(iron->maximum());
        assert(people->value() == 0 && iron->value() == 0);
        assert(!buttons->button(QDialogButtonBox::Ok)->isEnabled());

        source->setCurrentIndex(source->findData(0));
        destination->setCurrentIndex(destination->findData(1));
        distinct();
        exactIron->setValue(1);
        exactPeople->setValue(100);
        assert(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    window.openCargoManifestDialog();
    const auto& orders = suns::MainWindowTestAccess::orders(window).orders;
    assert(orders.size() == 1);
    const auto& transfer = std::get<suns::TransferCargoOrder>(orders.front());
    assert(transfer.source.planet == 1 && transfer.destination.fleet == 1);
    assert(transfer.colonists == 100 && transfer.minerals.ironium == 1);
    assert(suns::MainWindowTestAccess::systemInfo(window).contains("Queued cargo: -100 / +0"));
    assert(suns::MainWindowTestAccess::systemInfo(window).contains("population after validated orders: 999900"));
    // The growth display is an explicit one-rate estimate, not a simulated
    // End Turn. Its percentage is anchored to the *confirmed* population and
    // its next-year estimate to the *after-cargo* population.
    {
        const auto& baseline = suns::MainWindowTestAccess::state(window);
        const auto& planet = baseline.planets.front();
        const auto growth = suns::projected_population_growth(baseline, planet, baseline.turn);
        const double rate = static_cast<double>(growth) / planet.population;
        const auto estimated = static_cast<std::int64_t>(std::llround(999900.0 * rate));
        const auto nextPopulation = static_cast<std::uint64_t>(999900 + estimated);
        const auto info = suns::MainWindowTestAccess::systemInfo(window);
        assert(info.contains(QString("Current estimated annual growth: %1%2%")
            .arg(rate >= 0.0 ? "+" : "").arg(rate * 100.0, 0, 'f', 2)));
        assert(info.contains(QString("Simple next-year estimate: ~%1")
            .arg(static_cast<qulonglong>(nextPopulation))));
    }

    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* people = dialog->findChild<QSlider*>("cargoColonistSlider");
        auto* iron = dialog->findChild<QSlider*>("cargoMineralSlider0");
        auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
        auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
        assert(source->currentData().toInt() == 0 && destination->currentData().toInt() == 1);
        assert(people->maximum() == 100 && iron->maximum() == 100);
        const auto* destinationPeople = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        const auto* surfaceAfter = dialog->findChild<QLabel*>("cargoSourceAfterLabel");
        const auto* manifestAfter = dialog->findChild<QLabel*>("cargoDestinationAfterLabel");
        assert(destinationPeople && destinationPeople->value() == 200);
        assert(surfaceAfter && surfaceAfter->text().contains("999900"));
        assert(manifestAfter && manifestAfter->text().contains("colonists 200"));
        dialog->reject();
    });
    window.openCargoManifestDialog();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);

    // A queued load changes the surface at resolution, *before* annual growth.
    // The final value can exceed the old population despite the exact debit.
    const auto& before = suns::MainWindowTestAccess::state(window);
    auto afterCargo = before.planets.front();
    afterCargo.population -= 100;
    const auto expected = afterCargo.population +
        suns::projected_population_growth(before, afterCargo, before.turn);
    suns::MainWindowTestAccess::endTurn(window);
    const auto& resolved = suns::MainWindowTestAccess::state(window);
    assert(resolved.planets.front().population == expected);
    assert(resolved.fleets.front().colonists == 200);

    // Full-hold scenario: exactly 2500 colonists must remain visibly aboard
    // when the transfer dialog opens for the second time, without growing
    // planet population or queuing an accidental duplicate.
    suns::MainWindowTestAccess::installFullTransport(window);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* transfer = dialog->findChild<QSlider*>("cargoColonistSlider");
        auto* destination = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        assert(transfer && destination && buttons);
        assert(transfer->maximum() == 2500 && destination->value() == 0);
        transfer->setValue(2500);
        assert(destination->value() == 2500 && destination->maximum() == 2500);
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    window.openCargoManifestDialog();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);
    assert(suns::MainWindowTestAccess::systemInfo(window).contains("Queued cargo: -2500 / +0"));
    assert(suns::MainWindowTestAccess::systemInfo(window).contains("population after validated orders: 997500"));
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        const auto* destination = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        const auto* sourceAfter = dialog->findChild<QLabel*>("cargoSourceAfterLabel");
        const auto* destinationAfter = dialog->findChild<QLabel*>("cargoDestinationAfterLabel");
        assert(destination && destination->value() == 2500 && destination->maximum() == 2500);
        assert(sourceAfter && sourceAfter->text().contains("997500"));
        assert(destinationAfter && destinationAfter->text().contains("colonists 2500"));
        dialog->reject();
    });
    window.openCargoManifestDialog();
    assert(suns::MainWindowTestAccess::orders(window).orders.size() == 1);

    suns::MainWindowTestAccess::installEnemyColony(window);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
        auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
        auto* people = dialog->findChild<QSlider*>("cargoColonistSlider");
        auto* iron = dialog->findChild<QSlider*>("cargoMineralSlider0");
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        assert(source && destination && people && iron && buttons);
        assert(source->findData(0) == -1); // Enemy surface is never a cargo source.
        assert(destination->findData(0) >= 0);
        destination->setCurrentIndex(destination->findData(0));
        assert(iron->maximum() == 0);
        people->setValue(people->maximum());
        assert(people->value() == 100);
        assert(buttons->button(QDialogButtonBox::Ok)->text() == "Queue invasion");
        const auto* warning = dialog->findChild<QLabel*>("cargoDestinationAfterLabel");
        assert(warning && warning->text().contains("Any hostile station blocks landing"));
        assert(warning->text().contains("colonists remain aboard"));
        assert(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    window.openCargoManifestDialog();
    const auto& invasionOrders = suns::MainWindowTestAccess::orders(window).orders;
    assert(invasionOrders.size() == 1);
    const auto& invasion = std::get<suns::TransferCargoOrder>(invasionOrders.front());
    assert(invasion.source.fleet == 1 && invasion.destination.planet == 1);
    assert(invasion.colonists == 100);
    assert(suns::mineral_cargo_mass(invasion.minerals) == 0.0);

    // Select only a fleet, then use the real dock button/menu/toolbar. Cargo
    // always belongs to its orbit, regardless of the independent map selection.
    suns::MainWindow dockWindow;
    suns::attachRouteProgramDock(dockWindow);
    dockWindow.installUiPolish();
    dockWindow.installCommunicationStatus();
    suns::MainWindowTestAccess::install(dockWindow);
    suns::MainWindowTestAccess::selectStar(dockWindow, std::nullopt);
    auto* transferButton = dockWindow.findChild<QPushButton*>("fleetTransferCargoButton");
    auto* transferAction = dockWindow.findChild<QAction*>("fleetTransferCargoAction");
    auto* transferTool = dockWindow.findChild<QAction*>("fleetTransferCargoToolAction");
    auto* comms = dockWindow.findChild<QLabel*>("fleetCommunicationSummary");
    assert(transferButton && transferAction && transferTool && comms);
    assert(transferButton->isEnabled() && transferAction->isEnabled() && transferTool->isEnabled());
    assert(comms->text().contains("LIVE"));
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "cargoTransferDialog");
        assert(dialog->windowTitle() == "Transfer cargo — Sol");
        dialog->findChild<QSpinBox*>("cargoColonistSpin")->setValue(100);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    transferButton->click();
    const auto& fleetOnlyLoad = std::get<suns::TransferCargoOrder>(
        suns::MainWindowTestAccess::orders(dockWindow).orders.front());
    assert(fleetOnlyLoad.source.planet == 1 && fleetOnlyLoad.destination.fleet == 1);
    for (int year = 0; year < 4; ++year) {
        suns::MainWindowTestAccess::endTurn(dockWindow);
        assert(comms->text().contains("LIVE"));
        assert(transferButton->isEnabled());
    }
    assert(suns::MainWindowTestAccess::state(dockWindow).fleets.front().colonists == 200);

    suns::MainWindowTestAccess::selectStar(dockWindow, suns::StarId{2});
    dockWindow.selectFleetForRouteProgram(2);
    const auto checkInferredSol = [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->windowTitle() == "Transfer cargo — Sol");
        const auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
        assert(destination && destination->currentText().contains("Second transport"));
        dialog->reject();
    };
    QTimer::singleShot(0, checkInferredSol);
    transferAction->trigger();
    QTimer::singleShot(0, checkInferredSol);
    transferTool->trigger();
    assert(suns::MainWindowTestAccess::selectedStar(dockWindow) == suns::StarId{2});

    for(auto mode:{suns::EmissionMode::Passive,suns::EmissionMode::RadioSilence}) {
        suns::MainWindowTestAccess::installSilentOrbit(dockWindow,mode);
        suns::MainWindowTestAccess::endTurn(dockWindow);
        assert(comms->text().contains("LIVE") && transferButton->isEnabled());
        assert(comms->toolTip().contains("share full status with the colony"));
        const auto& fleet=suns::MainWindowTestAccess::state(dockWindow).fleets.front();
        assert(fleet.electronics.mode==mode && fleet.telemetry.colonists==100);
        QTimer::singleShot(0, [&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            assert(dialog && dialog->windowTitle()=="Transfer cargo — Sol");
            assert(dialog->findChild<QSlider*>("cargoDestinationColonistSlider")->value()==100);
            dialog->reject();
        });
        transferButton->click();
    }

    suns::MainWindowTestAccess::installRadarPosition(dockWindow);
    assert(comms->text().contains("DELAYED") && comms->text().contains("telemetry 4 turns old"));
    assert(comms->text().contains("Radar position: current") && !comms->text().contains("Position unconfirmed"));
    assert(dockWindow.fleetSourceNameForRouteProgram(1).contains("Radar:")
        && !dockWindow.fleetSourceNameForRouteProgram(1).contains("Orbit: Sol"));
    assert(!transferButton->isEnabled() && transferButton->toolTip().contains("Radar locates this fleet"));

    // Reproduce a four-year-old stationary Sol report while the real fleet is
    // elsewhere. Do not label that orbit confirmed or leak remote cargo data.
    suns::MainWindowTestAccess::installOldSolReport(dockWindow, 900);
    assert(comms->text().contains("DELAYED") && comms->text().contains("Position unconfirmed"));
    assert(dockWindow.fleetSourceNameForRouteProgram(1).contains("Estimated:"));
    assert(dockWindow.fleetSourceNameForRouteProgram(1).contains("report 4 turn(s) old"));
    assert(!transferButton->isEnabled() && !transferAction->isEnabled() && !transferTool->isEnabled());
    const auto explanation = transferButton->toolTip();
    assert(explanation.contains("4 turn(s) old"));
    dockWindow.openCargoManifestDialog();
    assert(!QApplication::activeModalWidget());
    assert(dockWindow.statusBar()->currentMessage() == explanation);
    suns::MainWindowTestAccess::installOldSolReport(dockWindow, 1500);
    assert(transferButton->toolTip() == explanation);
}
