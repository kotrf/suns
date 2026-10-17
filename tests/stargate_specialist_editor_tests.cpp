#include "main_window.hpp"
#include "suns/stations.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w, bool specialist) {
        w.state_ = make_demo_game(); w.state_.stars.resize(2); w.state_.planets.resize(2);
        auto& p = w.state_.players[0]; p.technology.levels.fill(26);
        p.race.primaryTrait = specialist ? PrimaryRaceTrait::StargateSpecialist : PrimaryRaceTrait::Generalist;
        w.state_.stars[1].position = {5000, 0};
        w.state_.planets[1].owner = 1; w.state_.planets[1].population = 100'000;
        w.state_.planets[1].radar = PlanetaryRadarLevel::Full;
        w.state_.orbitalStations.clear();
        for (auto id : {1U, 2U}) w.state_.orbitalStations.push_back({id, 1, id, "Infinity gate",
            OrbitalStationHullType::Fortress, {OrbitalStationModule::Shipyard}, {ShipComponentType::InfinityStargate}});
        w.state_.nextOrbitalStationId = 3;
        auto& f = w.state_.fleets[0]; f.design = kColonyShipDesignId; f.ships = {{kColonyShipDesignId, 1}};
        f.colonists = 1000; f.telemetry.colonists = 1000; f.telemetry.ships = f.ships;
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.selection_.star = 1; w.rebuildScene();
    }
    static const std::vector<Order>& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
};
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv); suns::MainWindow window;
    using namespace suns;
    MainWindowTestAccess::install(window, true);
    QTimer::singleShot(0, [&] {
        auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(d && d->objectName() == "stargateTransitDialog");
        auto* summary = d->findChild<QLabel*>("stargateTransitSummary");
        auto* destinations = d->findChild<QComboBox*>("stargateDestinations");
        assert(summary && summary->text().contains("Unlimited") && summary->text().contains("Stargate Specialist"));
        assert(destinations && destinations->count() == 1 && destinations->currentData().toUInt() == 2);
        d->accept();
    });
    window.openGateTransit();
    assert(std::holds_alternative<GateFleetOrder>(MainWindowTestAccess::orders(window).front()));
    QTimer::singleShot(0, [&] {
        auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(d && d->objectName() == "stationDesignerDialog");
        auto* table = d->findChild<QTableWidget*>("stationFittingTable"); bool replaced = false;
        for (int row = 0; row < table->rowCount(); ++row) {
            auto* c = qobject_cast<QComboBox*>(table->cellWidget(row, 1));
            if (c->currentData().toInt() != int(ShipComponentType::InfinityStargate)) continue;
            const auto index = c->findData(int(ShipComponentType::NetworkStargate));
            assert(index >= 0); c->setCurrentIndex(index); replaced = true;
        }
        assert(replaced);
        assert(d->findChild<QLabel*>("stationFitPreview")->text().contains("750 ly / 1200 kt"));
        assert(d->findChild<QPushButton*>("queueStationFit")->isEnabled());
        d->accept();
    });
    window.openStationDesigner();
    const auto& refit = std::get<QueueStationOrder>(MainWindowTestAccess::orders(window).back());
    assert(refit.construction.refitStation == 1);
    assert(refit.construction.loadout.components.front() == ShipComponentType::NetworkStargate);
    MainWindowTestAccess::install(window, false);
    QTimer::singleShot(0, [&] {
        auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(d);
        assert(d->findChild<QComboBox*>("stargateDestinations")->count() == 0);
        assert(!d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        d->reject();
    });
    window.openGateTransit();
    assert(MainWindowTestAccess::orders(window).empty());
}
