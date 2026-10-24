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
        for(const auto& planet:w.state_.planets)set_survey_level(w.state_,1,planet.star,SurveyLevel::DeepSurvey,w.state_.turn);
        auto& f = w.state_.fleets[0]; f.design = kColonyShipDesignId; f.ships = {{kColonyShipDesignId, 1}};
        f.colonists = 1000; f.telemetry.colonists = 1000; f.telemetry.ships = f.ships;
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.selection_.star = 1; w.rebuildScene();
    }
    static const std::vector<Order>& orders(const MainWindow& w) { return w.pendingOrders_.orders; }
    static GameState snapshot(const MainWindow& w) { return w.state_; }
    static void movingRemote(MainWindow& w) {
        remote(w);
        auto& f=w.state_.fleets[0]; f.position={5060,0}; f.destination=Position{5000,0}; f.warp=1;
        f.telemetry.position=f.position; f.telemetry.destination=f.destination; f.telemetry.warp=1;
        w.state_.stars.push_back({3,"Beyond the receiving gate",{0,25}});
        ++w.planningRevision_; w.rebuildScene();
    }
    static void transit(MainWindow& w) { w.openGateTransit(); }
    static void star(MainWindow& w, StarId id) { w.selection_.star=id; }
    static void advance(MainWindow& w) { w.endTurn(); }
    static void fitting(MainWindow& w) { w.openStationDesigner(); }
    static void hideDestinationGate(MainWindow& w, bool deliver) {
        w.state_.orbitalStations[1].components.clear();
        if(deliver) set_survey_level(w.state_,1,2,SurveyLevel::DeepSurvey,w.state_.turn);
        ++w.planningRevision_; w.rebuildScene();
    }
    static void remote(MainWindow& w) {
        auto& f=w.state_.fleets[0]; f.position={5000,0}; f.telemetry.position=f.position;
        f.telemetry.observedTurn=w.state_.turn;
        w.state_.planets[1].radar=PlanetaryRadarLevel::None;
        set_survey_level(w.state_,1,2,SurveyLevel::DeepSurvey,w.state_.turn);
        w.selection_.star=2; ++w.planningRevision_; w.rebuildScene();
    }
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
    MainWindowTestAccess::transit(window);
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
    MainWindowTestAccess::fitting(window);
    const auto& refit = std::get<QueueStationOrder>(MainWindowTestAccess::orders(window).back());
    assert(refit.construction.refitStation == 1);
    assert(refit.construction.loadout.components.front() == ShipComponentType::NetworkStargate);
    MainWindowTestAccess::install(window, true);
    QTimer::singleShot(0, [&] {
        auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(d && d->objectName()=="stargateRouteDialog");
        assert(d->findChild<QComboBox*>("routeGateSource")->count()==2);
        assert(d->findChild<QComboBox*>("routeGateDestination")->currentData().toUInt()==2);
        d->accept();
    });
    window.openStargateRouteDialog();
    assert(std::holds_alternative<MoveFleetOrder>(MainWindowTestAccess::orders(window).front()));
    assert(window.appendStargateRouteStep(2,1,5));
    const auto rows=window.selectedFleetRouteProgramRows();
    assert(rows.size()==2 && rows[0].destination.contains("[gate]") && rows[0].eta=="1" && rows[1].eta=="2");
    assert(rows[0].arrivalAction.contains("wait and retry"));
    assert(window.setSelectedFleetRouteProgramLegWarp(0,4));
    assert(window.selectedFleetRouteProgramLegAction(0)->gateDestination==2);
    const auto forecast=window.selectedFleetRouteProgramForecast();
    assert(forecast.contains("projected gate hop completed"));
    MainWindowTestAccess::hideDestinationGate(window,false);
    assert(window.selectedFleetRouteProgramForecast()==forecast);
    MainWindowTestAccess::hideDestinationGate(window,true);
    assert(window.selectedFleetRouteProgramForecast().contains("Gate step waiting (forecast)"));
    assert(!window.appendStargateRouteStep(1,2,5));
    assert(window.removeSelectedFleetRouteProgramLeg(1) && window.clearSelectedFleetRouteProgram());
    // A receiving-system No action point can complete in the hop year.
    for (const bool middle:{false,true}) for (const bool repeat:{false,true}) {
        MainWindowTestAccess::install(window,true);
        assert(window.appendStargateRouteStep(1,2,5));
        MainWindowTestAccess::star(window,2);
        assert(window.appendSelectedStarWaypoint(5,{}));
        if(middle) assert(window.appendStargateRouteStep(2,1,5));
        if(repeat) assert(window.setSelectedFleetRepeatOrdersForRouteProgram(true));
        const auto forecastRows=window.selectedFleetRouteProgramRows();
        assert(forecastRows.size()==(middle?3:2) && forecastRows[0].eta=="1" && forecastRows[1].eta=="1");
        assert(forecastRows[1].fuelUsed=="0.0");
        if(middle) assert(forecastRows[2].eta=="2");
    }
    MainWindowTestAccess::install(window,true); MainWindowTestAccess::remote(window);
    QTimer::singleShot(0,[&] {
        auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(d);
        auto* targets=d->findChild<QComboBox*>("stargateDestinations");
        assert(targets->count()==1 && targets->currentData().toUInt()==1); d->accept();
    });
    MainWindowTestAccess::transit(window);
    const auto& deferred=std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).front());
    assert(deferred.arrivalAction.gateSource==2 && deferred.arrivalAction.gateDestination==1);
    const auto originalEta=window.selectedFleetRouteProgramRows()[0].eta.toInt();
    assert(originalEta>1);
    MainWindowTestAccess::advance(window);
    assert(window.selectedFleetRouteProgramRows()[0].eta.toInt()==originalEta-1);
    assert(window.selectedFleetRouteProgramForecast().contains("projected gate hop completed"));
    assert(window.appendStargateRouteStep(1,2,5));
    const auto& extended=std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).front());
    assert(extended.arrivalAction.gateSource==2 && extended.arrivalAction.gateDestination==1);
    assert(extended.queuedWaypoints.size()==1 && extended.queuedWaypoints[0].arrivalAction.gateDestination==2);
    // Sent gate programmes must outrank old active-route telemetry as well as idle telemetry.
    MainWindowTestAccess::install(window,true); MainWindowTestAccess::movingRemote(window);
    assert(window.appendStargateRouteStep(2,1,5));
    MainWindowTestAccess::advance(window);
    const auto sent=MainWindowTestAccess::snapshot(window);
    assert(sent.fleets[0].telemetry.destination && !sent.fleets[0].pendingCommands.empty());
    MainWindowTestAccess::star(window,3);
    assert(window.appendSelectedStarWaypoint(5,{}));
    const auto& revised=std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).front());
    assert(revised.queuedWaypoints.size()==2);
    assert(revised.queuedWaypoints[0].arrivalAction.gateSource==2
        && revised.queuedWaypoints[0].arrivalAction.gateDestination==1);
    const auto run=[&](GameState state, bool edited) {
        const PlayerOrders orders{1,MainWindowTestAccess::orders(window)};
        for(int year=0;year<120;++year) {
            state=TurnProcessor{}.process(state,edited && year==0?std::vector<PlayerOrders>{orders}:std::vector<PlayerOrders>{});
            if(!state.fleets[0].destination && state.fleets[0].pendingCommands.empty()) return state;
        }
        assert(false && "Gate programme must finish without further headquarters commands"); return state;
    };
    const auto control=run(sent,false), delivered=run(sent,true);
    assert(control.fleets[0].position==Position{});
    assert((delivered.fleets[0].position==Position{0,25}) && delivered.fleets[0].fuel<control.fleets[0].fuel);
    assert(!delivered.fleets[0].fuelStalled && delivered.fleets[0].fuel>0);
    // A more recent sent Stop also outranks the older sent gate programme.
    MainWindowTestAccess::install(window,true); MainWindowTestAccess::movingRemote(window);
    assert(window.appendStargateRouteStep(2,1,5)); MainWindowTestAccess::advance(window);
    assert(window.clearSelectedFleetRouteProgram()); MainWindowTestAccess::advance(window);
    MainWindowTestAccess::star(window,3); assert(window.appendSelectedStarWaypoint(5,{}));
    const auto& afterStop=std::get<MoveFleetOrder>(MainWindowTestAccess::orders(window).front());
    assert(afterStop.queuedWaypoints.empty() && afterStop.arrivalAction.kind==FleetArrivalActionKind::None
        && (afterStop.destination==Position{0,25}));
    MainWindowTestAccess::install(window, false);
    QTimer::singleShot(0, [&] {
        auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(d);
        assert(d->findChild<QComboBox*>("stargateDestinations")->count() == 0);
        assert(!d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        d->reject();
    });
    MainWindowTestAccess::transit(window);
    assert(MainWindowTestAccess::orders(window).empty());
}
