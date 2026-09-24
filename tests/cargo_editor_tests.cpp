#include "main_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <cassert>

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
        fleet.colonists = 10'000;
        fleet.minerals = {1, 0, 0};
        assert(fleet_cargo_capacity(w.state_, fleet) == 5);
        auto other = fleet;
        other.id = w.state_.nextFleetId++;
        other.name = "Second transport";
        other.colonists = 0;
        other.minerals = {};
        w.state_.fleets.push_back(other);
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star;
        w.selection_.fleet = 1;
        w.rebuildScene();
    }
    static void installEnemyColony(MainWindow& w)
    {
        install(w);
        w.state_.players.push_back({2, "Enemy"});
        w.state_.planets.front().owner = 2;
        w.state_.planets.front().population = 8'000;
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.rebuildScene();
    }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
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
        // A selected fleet that already carries cargo opens in unloading
        // direction. The sliders still span the full hold; source stock only
        // limits the handle, not the scale.
        assert(source->currentData().toInt() == 1);
        assert(destination->currentData().toInt() == 0);
        assert(people->maximum() == 50'000);
        assert(iron->maximum() == 500);
        people->setValue(people->maximum());
        assert(people->value() == 10'000);
        iron->setValue(iron->maximum());
        assert(iron->value() == 100);

        source->setCurrentIndex(source->findData(0));
        destination->setCurrentIndex(destination->findData(1));
        distinct();
        assert(people->maximum() == 30'000); // 5 kt hold, 2 kt already aboard
        assert(iron->maximum() == 300 && boron->maximum() == 300);
        boron->setValue(boron->maximum());
        assert(boron->value() == 25 && boron->maximum() == 300); // stock limits handle, not scale
        boron->setValue(0);
        iron->setValue(iron->maximum());
        assert(exactIron->value() == 3 && people->maximum() == 0 && boron->maximum() == 0);
        exactIron->setValue(1);
        assert(people->maximum() == 20'000);
        people->setValue(people->maximum());
        assert(exactPeople->value() == 20'000 && boron->maximum() == 0);
        exactPeople->setValue(10'000);
        assert(iron->maximum() == 200 && boron->maximum() == 100);

        // Selecting the old destination as source rebuilds the destination list.
        source->setCurrentIndex(source->findData(1));
        distinct();
        destination->setCurrentIndex(destination->findData(0)); // unload to planet
        assert(people->maximum() == 50'000 && iron->maximum() == 500);
        people->setValue(people->maximum());
        assert(people->value() == 10'000 && people->maximum() == 50'000);
        destination->setCurrentIndex(destination->findData(2)); // fleet-to-fleet
        distinct();
        assert(people->maximum() == 50'000 && iron->maximum() == 500);
        iron->setValue(iron->maximum());
        assert(iron->value() == 100 && iron->maximum() == 500);
        source->setCurrentIndex(source->findData(2));
        distinct();
        assert(people->maximum() == 50'000 && iron->maximum() == 500);
        people->setValue(people->maximum());
        iron->setValue(iron->maximum());
        assert(people->value() == 0 && iron->value() == 0);
        assert(!buttons->button(QDialogButtonBox::Ok)->isEnabled());

        source->setCurrentIndex(source->findData(0));
        destination->setCurrentIndex(destination->findData(1));
        distinct();
        exactIron->setValue(1);
        exactPeople->setValue(10'000);
        assert(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    window.openCargoManifestDialog();
    const auto& orders = suns::MainWindowTestAccess::orders(window).orders;
    assert(orders.size() == 1);
    const auto& transfer = std::get<suns::TransferCargoOrder>(orders.front());
    assert(transfer.source.planet == 1 && transfer.destination.fleet == 1);
    assert(transfer.colonists == 10'000 && transfer.minerals.ironium == 1);

    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* people = dialog->findChild<QSlider*>("cargoColonistSlider");
        auto* iron = dialog->findChild<QSlider*>("cargoMineralSlider0");
        auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
        auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
        assert(source->currentData().toInt() == 1 && destination->currentData().toInt() == 0);
        assert(people->maximum() == 50'000 && iron->maximum() == 500);
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
        assert(people->value() == 10'000);
        assert(buttons->button(QDialogButtonBox::Ok)->text() == "Queue invasion");
        assert(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    window.openCargoManifestDialog();
    const auto& invasionOrders = suns::MainWindowTestAccess::orders(window).orders;
    assert(invasionOrders.size() == 1);
    const auto& invasion = std::get<suns::TransferCargoOrder>(invasionOrders.front());
    assert(invasion.source.fleet == 1 && invasion.destination.planet == 1);
    assert(invasion.colonists == 10'000);
    assert(suns::mineral_cargo_mass(invasion.minerals) == 0.0);
}
