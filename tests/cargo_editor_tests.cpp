#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/communications.hpp"
#include "suns/propulsion.hpp"
#include "suns/reconnaissance.hpp"
#include "native_orbital_fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace suns {
struct MainWindowTestAccess {
    static void installCapitalEvacuation(MainWindow& w)
    {
        w.state_ = test::native_capital_evacuation_fixture();
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.star = 1; w.selection_.fleet = 1; w.rebuildScene();
    }
    static void installNative(MainWindow& w, std::uint64_t residents = 10'000, std::uint64_t passengers = 0)
    {
        w.state_ = test::native_orbital_fixture();
        w.state_.planets.front().nativeOrbit.population = residents;
        w.state_.fleets.front().colonists = passengers; w.state_.fleets.front().minerals = {};
        w.state_.fleets.front().telemetry = {};
        w.state_.fleets.front().telemetry = confirmed_fleet_telemetry(w.state_, w.state_.fleets.front());
        w.state_.planets.front().minerals = {108, 104, 102};
        set_survey_level(w.state_, 1, 1, SurveyLevel::DeepSurvey, w.state_.turn);
        w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.star = 1; w.selection_.fleet = w.state_.fleets.front().id;
        w.rebuildScene();
    }
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
    static void installFullTransport(MainWindow& w, std::uint32_t shipCount = 5,
        std::string fleetName = "Heavy General Purpose Transport Alpha")
    {
        install(w);
        w.state_.shipDesigns.push_back({3, 1, "Light transport group",
            ShipHullType::LightTransport, {ShipComponentType::FusionDrive}});
        auto& fleet = w.state_.fleets.front();
        fleet.name = std::move(fleetName);
        fleet.design = 3;
        fleet.ships = {{3, shipCount}};
        fleet.minerals = {};
        fleet.colonists = 0;
        w.state_.fleets.resize(1);
        assert(fleet_cargo_capacity(w.state_, fleet) == 5.0 * shipCount);
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
    static void installScaledTransfer(MainWindow& w)
    {
        installFullTransport(w, 20);
        w.state_.planets.front().minerals = {5000, 5000, 0};
        auto& fleet = w.state_.fleets.front();
        fleet.minerals = {10, 0, 1};
        fleet.telemetry = {};
        fleet.telemetry = confirmed_fleet_telemetry(w.state_, fleet);
        set_survey_level(w.state_, 1, 1, SurveyLevel::DeepSurvey, w.state_.turn);
        w.rebuildScene();
    }
    static void installFuelPair(MainWindow& w, double receiverRoom, double donorFuel)
    {
        install(w);
        auto& receiver = w.state_.fleets[0];
        auto& donor = w.state_.fleets[1];
        donor.ships.front().count = 2;
        receiver.fuel = fleet_fuel_capacity(w.state_, receiver) - receiverRoom;
        donor.fuel = donorFuel;
        for (auto& fleet : w.state_.fleets) {
            fleet.position = w.state_.stars.front().position;
            fleet.position.x += 1.0;
            fleet.telemetry = {};
            fleet.telemetry = confirmed_fleet_telemetry(w.state_, fleet);
        }
        w.rebuildScene();
    }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static const GameState& state(const MainWindow& w) { return w.state_; }
    static void planningRefreshRegressions(MainWindow& w)
    {
        installFullTransport(w);
        const auto turn=w.state_.turn;
        assert(w.selectedFleetPlanningView()->colonists==0);
        w.colonistLoadSpin_->setValue(100);w.queueColonists();
        assert(w.selectedFleetPlanningView()->colonists==100);
        w.colonistLoadSpin_->setValue(200);w.queueColonists();
        assert(w.pendingOrders_.orders.size()==1 && w.state_.turn==turn);
        assert(w.selectedFleetPlanningView()->colonists==200); // Same-size replacement, same year.
        const auto fuel=w.selectedFleetPlanningView()->fuel;
        for (int i=0;i<20;++i) {
            w.updateControls();
            assert(w.selectedFleetPlanningView()->colonists==200);
            assert(w.selectedFleetPlanningView()->fuel==fuel);
        }
        assert(w.state_.fleets.front().colonists==0 && w.state_.turn==turn);
        w.pendingOrders_.orders.clear();w.pendingDescriptions_.clear();w.rebuildScene();
        assert(w.selectedFleetPlanningView()->colonists==0);
        // A different saved boundary can reuse both turn and fleet IDs.
        auto saved=w.campaignSnapshot();auto& fleet=saved.state.fleets.front();
        fleet.colonists=300;fleet.telemetry={};fleet.telemetry=confirmed_fleet_telemetry(saved.state,fleet);
        QTemporaryDir dir;assert(dir.isValid());QString error;
        const auto path=dir.filePath("same-year.suns");
        assert(write_save_game_file(path,saved,error));assert(w.loadGameFromPath(path));
        assert(w.state_.turn==turn && w.selectedFleetPlanningView()->colonists==300);
        installFullTransport(w); // New session/fixture, again at that same year and ID.
        assert(w.state_.turn==turn && w.selectedFleetPlanningView()->colonists==0);
    }
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

static void cargo_summary_stays_in_place(bool capture)
{
    suns::MainWindow window;
    suns::attachRouteProgramDock(window);
    window.installUiPolish();
    suns::MainWindowTestAccess::installFullTransport(window, 50);
    const auto& state = suns::MainWindowTestAccess::state(window);
    const auto emptyMass = suns::fleet_gross_mass(state, state.fleets.front());
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* slider = dialog->findChild<QSlider*>("cargoColonistSlider");
        auto* spin = dialog->findChild<QSpinBox*>("cargoColonistSpin");
        const auto* destinationSlider = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        auto* manifest = dialog->findChild<QLabel*>("cargoDestinationAfterLabel");
        const auto* manifestScroll = dialog->findChild<QScrollArea*>("cargoDestinationManifestScroll");
        const auto* summary = dialog->findChild<QLabel*>("cargoCapacityLabel");
        const auto* buttons = dialog->findChild<QDialogButtonBox*>();
        assert(slider && spin && destinationSlider && manifest && manifestScroll && summary && buttons);
        const auto openingSize = dialog->size();
        dialog->resize(920, 630);
        const auto geometry = [&](const QWidget* widget) {
            return QRect(widget->mapTo(dialog, QPoint{}), widget->size());
        };
        const auto layout = [&] {
            return std::array{dialog->rect(), geometry(slider), geometry(spin), geometry(destinationSlider),
                geometry(summary), geometry(buttons)};
        };
        const auto checkMass = [&](int people) {
            const double mass = double(people) / suns::kColonistsPerCargoUnit;
            QTextDocument document;
            document.setHtml(summary->text());
            assert(document.toPlainText() == QString("Destination hold: %1 / 250.00 kt\nGross mass: %2 kt")
                .arg(mass, 0, 'f', 2).arg(emptyMass + mass, 0, 'f', 2));
            assert(summary->text().count("&nbsp;kt") == 2);
            assert(destinationSlider->value() == people);
        };
        slider->setSliderDown(true);
        slider->setSliderPosition(18'727);
        QApplication::processEvents();
        const auto initialLayout = layout();
        checkMass(18'727);
        if (capture) assert(dialog->grab().save("/tmp/suns-cargo-layout-18727.png"));
        for (int people : {18'728, 19'090, 19'091, 19'092}) {
            slider->setSliderPosition(people);
            QApplication::processEvents();
            assert(layout() == initialLayout);
            checkMass(people);
        }
        slider->setSliderDown(false);
        spin->setValue(19'091);
        QApplication::processEvents();
        assert(layout() == initialLayout);
        assert(slider->value() == 19'091);
        checkMass(19'091);
        if (capture) assert(dialog->grab().save("/tmp/suns-cargo-layout-19091.png"));

        const QSize compactSize(920, 409);
        for (const auto size : {QSize(920, 630), QSize(920, 440), compactSize, openingSize}) {
            dialog->resize(size);
            spin->setValue(9);
            QApplication::processEvents();
            const auto shortLayout = layout();
            checkMass(9);
            spin->setValue(10);
            QApplication::processEvents();
            assert(manifest->height() >= manifest->heightForWidth(manifest->width()));
            assert(layout() == shortLayout);
            checkMass(10);
            if (capture && size == compactSize) assert(dialog->grab().save("/tmp/suns-cargo-layout-compact.png"));
            slider->setSliderDown(true);
            slider->setSliderPosition(9);
            slider->setSliderDown(false);
            QApplication::processEvents();
            assert(layout() == shortLayout);
            assert(manifest->height() >= manifest->heightForWidth(manifest->width()));
            checkMass(9);
        }

        // Make the manifest taller than its viewport with any font or DPI.
        dialog->resize(compactSize);
        QApplication::processEvents();
        const auto compactLayout = layout();
        const auto* viewport = manifestScroll->viewport();
        const int copies = viewport->height() / manifest->fontMetrics().lineSpacing() + 1;
        manifest->setText((manifest->text() + "<br>").repeated(copies));
        QApplication::processEvents(QEventLoop::AllEvents, 100);
        auto* scrollbar = manifestScroll->verticalScrollBar();
        assert(scrollbar->isVisible() && scrollbar->maximum() > 0);
        scrollbar->setValue(scrollbar->maximum());
        assert(viewport->rect().contains(manifest->mapTo(viewport, manifest->rect().bottomRight())));
        assert(layout() == compactLayout);
        dialog->reject();
    });
    window.openCargoManifestDialog();
    assert(suns::MainWindowTestAccess::orders(window).orders.empty());
}

static void cargo_manifest_overflow_can_be_scrolled()
{
    suns::MainWindow window;
    suns::attachRouteProgramDock(window);
    window.installUiPolish();
    const std::string fleetName(80, 'W');
    suns::MainWindowTestAccess::installFullTransport(window, 50, fleetName);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* spin = dialog->findChild<QSpinBox*>("cargoColonistSpin");
        auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
        assert(spin && source);
        dialog->resize(920, 630);
        spin->setValue(19'091);

        const auto checkManifest = [&](const char* paneName, const char* labelName) {
            QApplication::processEvents(QEventLoop::AllEvents, 100);
            auto* scroll = dialog->findChild<QScrollArea*>(paneName);
            auto* manifest = dialog->findChild<QLabel*>(labelName);
            assert(scroll && manifest && manifest->text().contains(QString::fromStdString(fleetName)));
            assert(manifest->text().contains("fuel "));
            auto* horizontal = scroll->horizontalScrollBar();
            assert(horizontal->maximum() > 0);
            assert(horizontal->isVisible());
            const auto* viewport = scroll->viewport();
            horizontal->setValue(horizontal->maximum());
            assert(viewport->rect().contains(manifest->mapTo(viewport, manifest->rect().bottomRight())));
        };

        checkManifest("cargoDestinationManifestScroll", "cargoDestinationAfterLabel");
        const auto fleetRow = source->findData(1);
        assert(fleetRow >= 0);
        source->setCurrentIndex(fleetRow);
        checkManifest("cargoSourceManifestScroll", "cargoSourceAfterLabel");
        dialog->reject();
    });
    window.openCargoManifestDialog();
    assert(suns::MainWindowTestAccess::orders(window).orders.empty());
}

static void cargo_tracks_show_full_hold_and_surface_stock()
{
    suns::MainWindow window;
    suns::MainWindowTestAccess::installScaledTransfer(window);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
        auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
        auto* iron = dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin0");
        auto* boron = dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin1");
        auto* germanium = dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin2");
        auto* stockIron = dialog->findChild<QSlider*>("cargoDestinationMineralSlider0");
        auto* stockBoron = dialog->findChild<QSlider*>("cargoDestinationMineralSlider1");
        auto* stockGermanium = dialog->findChild<QSlider*>("cargoDestinationMineralSlider2");
        const auto* ironValue = dialog->findChild<QLabel*>("cargoDestinationMineralStock0");
        const auto* germaniumValue = dialog->findChild<QLabel*>("cargoDestinationMineralStock2");
        const auto* transferIron = dialog->findChild<QSlider*>("cargoMineralSlider0");
        const auto* surfaceIron = dialog->findChild<QProgressBar*>("cargoSurfaceMineralStockBar0");
        const auto* surfaceGermanium = dialog->findChild<QProgressBar*>("cargoSurfaceMineralStockBar2");
        assert(source && destination && iron && boron && germanium && stockIron && stockBoron && stockGermanium);
        assert(ironValue && germaniumValue && transferIron);

        germanium->setValue(-1);
        iron->setValue(40);
        boron->setValue(50);
        assert(stockIron->maximum() == 10'000 && stockBoron->maximum() == 10'000);
        assert(stockIron->value() == 5'000 && stockBoron->value() == 5'000);
        assert(ironValue->text() == "50.00 kt" && transferIron->maximum() == 10'000);
        assert(surfaceIron && surfaceGermanium && surfaceIron->isHidden() && surfaceGermanium->isHidden());

        source->setCurrentIndex(source->findData(1));
        destination->setCurrentIndex(destination->findData(0));
        assert(stockIron->isEnabled() && stockGermanium->isEnabled());
        assert(stockIron->value() == 0 && stockIron->maximum() > 0 && stockGermanium->maximum() > 0);
        assert(ironValue->text() == "5000.00 kt" && transferIron->maximum() == 10'000);
        assert(surfaceIron->isVisible() && surfaceIron->value() == surfaceIron->maximum());
        assert(surfaceGermanium->isVisible() && surfaceGermanium->value() == 0 && germaniumValue->text() == "0.00 kt");
        stockIron->triggerAction(QSlider::SliderToMaximum);
        stockGermanium->triggerAction(QSlider::SliderToMaximum);
        assert(iron->value() == 10 && germanium->value() == 1);
        assert(surfaceIron->value() == surfaceIron->maximum() && ironValue->text() == "5010.00 kt");
        assert(surfaceGermanium->value() == surfaceGermanium->maximum() && germaniumValue->text() == "1.00 kt");
        stockIron->setValue(int(std::lround(-stockIron->maximum() / iron->maximum())));
        assert(iron->value() == -1 && ironValue->text() == "4999.00 kt");
        assert(surfaceIron->value() < surfaceIron->maximum() && transferIron->maximum() == 10'000);
        stockIron->triggerAction(QSlider::SliderToMaximum);
        auto* apply = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        assert(apply->isEnabled());
        apply->click();
    });
    window.openCargoManifestDialog();
    const auto& orders = suns::MainWindowTestAccess::orders(window);
    const auto& state = suns::MainWindowTestAccess::state(window);
    assert(orders.orders.size() == 1);
    const auto& transfer = std::get<suns::TransferCargoOrder>(orders.orders.front());
    assert(transfer.source.fleet == state.fleets.front().id && transfer.destination.planet == state.planets.front().id);
    assert(transfer.minerals.ironium == 10 && transfer.minerals.germanium == 1);
    const auto planned = suns::projected_cargo_order_state(state, orders);
    assert(planned.planets.front().minerals.ironium == 5010 && planned.planets.front().minerals.germanium == 1);
    const auto resolved = suns::TurnProcessor{}.process(state, {orders});
    assert(resolved.fleets.front().minerals.ironium == 0 && resolved.fleets.front().minerals.germanium == 0);
}

static void cargo_fuel_preserves_exact_limits()
{
    enum class Input { Exact, Amount, Stock };
    struct Case { Input input; bool forward; double room; double donor; double visibleLimit; std::optional<double> request{}; };
    const Case cases[]{{Input::Exact, false, 1.086, 20.0, 1.09}, {Input::Amount, true, 1.04, 20.0, 1.04},
        {Input::Stock, false, 2.0, 0.034, 0.04}, {Input::Stock, true, 0.034, 20.0, 0.04},
        {Input::Exact, true, 399.94, 0.14, 0.14, 0.04}, {Input::Exact, false, 2.0, 0.034, 0.04, 0.03}};
    suns::MainWindow window;
    for (const auto& test : cases) {
        suns::MainWindowTestAccess::installFuelPair(window, test.room, test.donor);
        const auto& state = suns::MainWindowTestAccess::state(window);
        const auto& receiver = state.fleets[0];
        const auto& donor = state.fleets[1];
        const double receiverTank = suns::fleet_fuel_capacity(state, receiver);
        const double donorTank = suns::fleet_fuel_capacity(state, donor);
        const double sent = test.request.value_or(std::min(test.room, test.donor));
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            assert(dialog);
            auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
            auto* exact = dialog->findChild<QDoubleSpinBox*>("cargoFuelSpin");
            auto* amount = dialog->findChild<QSlider*>("cargoFuelSlider");
            auto* stock = dialog->findChild<QSlider*>("cargoDestinationFuelSlider");
            const auto* stockValue = dialog->findChild<QLabel*>("cargoDestinationFuelStock");
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            assert(source && exact && amount && stock && stockValue && buttons);
            if (test.forward) source->setCurrentIndex(source->findData(1));
            const double sourceTank = test.forward ? donorTank : receiverTank;
            const double destinationTank = test.forward ? receiverTank : donorTank;
            assert(exact->isEnabled() && stock->isEnabled() && exact->decimals() == 2);
            assert(amount->minimum() == -std::lround(sourceTank * 100) && amount->maximum() == std::lround(destinationTank * 100));
            assert(std::abs((test.forward ? exact->maximum() : -exact->minimum()) - test.visibleLimit) < 1e-9);
            if (test.input == Input::Exact) exact->setValue((test.forward ? 1 : -1) * test.request.value_or(test.visibleLimit));
            else if (test.input == Input::Amount) amount->setValue(test.forward ? amount->maximum() : amount->minimum());
            else stock->setValue(test.forward ? stock->maximum() : stock->minimum());
            const double after = test.forward ? receiver.fuel + sent : donor.fuel - sent;
            assert(stockValue->text() == QString("%1 mg").arg(after, 0, 'f', 2));
            assert(std::abs(double(stock->value()) / stock->maximum() - after / destinationTank) < 1e-5);
            auto* apply = buttons->button(QDialogButtonBox::Ok);
            assert(apply->isEnabled());
            apply->click();
        });
        window.openCargoManifestDialog();
        const auto& orders = suns::MainWindowTestAccess::orders(window);
        assert(orders.orders.size() == 1);
        const auto& fuel = std::get<suns::TransferFleetFuelOrder>(orders.orders.front());
        assert(fuel.source == donor.id && fuel.destination == receiver.id && std::abs(fuel.amount - sent) < 1e-9);
        const auto projected = suns::projected_cargo_order_state(state, orders);
        assert(std::abs(projected.fleets[0].fuel - (receiver.fuel + sent)) < 1e-9);
        assert(std::abs(projected.fleets[1].fuel - (donor.fuel - sent)) < 1e-9);
        const auto resolved = suns::TurnProcessor{}.process(state, {orders});
        assert(std::abs(resolved.fleets[0].fuel - projected.fleets[0].fuel) < 1e-9);
        assert(std::abs(resolved.fleets[1].fuel - projected.fleets[1].fuel) < 1e-9);
        if (test.request == 0.04) {
            assert(suns::fuel_in_complete_containers(resolved.fleets[0].fuel) == 0.1);
            assert(suns::fuel_in_complete_containers(resolved.fleets[1].fuel) == 0.1);
        }
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    {
        suns::MainWindow native;
        suns::attachRouteProgramDock(native); native.installUiPolish();
        suns::MainWindowTestAccess::installNative(native);
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
            auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
            auto* iron = dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin0");
            auto* people = dialog->findChild<QSpinBox*>("cargoColonistSpin");
            const auto row = source->findText("Orbital station", Qt::MatchStartsWith); assert(row >= 0);
            source->setCurrentIndex(row);
            assert(iron->isEnabled() && people->maximum() > 0);
            assert(dialog->findChild<QLabel*>("cargoSourceBeforeLabel")->text().contains("colonists 10000"));
            assert(dialog->findChild<QLabel*>("cargoDestinationBeforeLabel")->text().contains("colonists 0"));
            people->setValue(100); iron->setValue(3);
            auto* accept = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            assert(accept->isEnabled());
            if (app.arguments().contains("--capture")) dialog->grab().save("/tmp/suns-native-cargo-review284.png");
            accept->click();
        });
        native.openCargoManifestDialog();
        const auto& orders = suns::MainWindowTestAccess::orders(native);
        assert(orders.orders.size() == 1);
        const auto& order = std::get<suns::TransferCargoOrder>(orders.orders.front());
        assert(order.source.orbital && order.source.planet == 1 && order.destination.fleet == 1);
        assert(order.colonists == 100 && order.minerals.ironium == 3);
        const auto next = suns::TurnProcessor{}.process(suns::MainWindowTestAccess::state(native), {orders});
        assert(next.planets.front().nativeOrbit.population - next.planets.front().nativeOrbit.populationGrown == 9'900
            && next.planets.front().minerals.ironium + next.planets.front().nativeOrbit.mineralUnitsPaid == 105);
        assert(next.fleets.front().colonists == 100 && next.fleets.front().minerals.ironium == 3);
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
            assert(dialog->findChild<QComboBox*>("cargoSourceCombo")->currentText().startsWith("Orbital station"));
            dialog->findChild<QSpinBox*>("cargoColonistSpin")->setValue(-100);
            dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin0")->setValue(-3);
            auto* accept = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            assert(accept->isEnabled()); accept->click();
        });
        native.openCargoManifestDialog();
        const auto& returned = suns::MainWindowTestAccess::orders(native);
        assert(returned.orders.size() == 2);
        const auto restored = suns::TurnProcessor{}.process(suns::MainWindowTestAccess::state(native), {returned});
        assert(restored.planets.front().nativeOrbit.population - restored.planets.front().nativeOrbit.populationGrown == 10'000
            && restored.planets.front().minerals.ironium + restored.planets.front().nativeOrbit.mineralUnitsPaid == 108);
        assert(restored.fleets.front().colonists == 0 && restored.fleets.front().minerals.ironium == 0);

        suns::MainWindowTestAccess::installNative(native, 99'950, 500);
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
            auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
            auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
            source->setCurrentIndex(source->findText("Fleet", Qt::MatchStartsWith));
            destination->setCurrentIndex(destination->findText("Orbital station", Qt::MatchStartsWith));
            auto* people = dialog->findChild<QSpinBox*>("cargoColonistSpin");
            assert(people->maximum() == 50);
            assert(dialog->findChild<QLabel*>("cargoCapacityLabel")->text().contains("/ 100000 residents"));
            destination->setCurrentIndex(destination->findText("Planetary surface", Qt::MatchStartsWith));
            assert(people->maximum() == 0 && people->minimum() == 0);
            dialog->reject();
        });
        native.openCargoManifestDialog();
        suns::MainWindowTestAccess::installCapitalEvacuation(native);
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(dialog);
            auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
            auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
            source->setCurrentIndex(source->findText("Fleet", Qt::MatchStartsWith));
            destination->setCurrentIndex(destination->findText("Orbital station", Qt::MatchStartsWith));
            dialog->findChild<QSpinBox*>("cargoColonistSpin")->setValue(-1'000);
            dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin0")->setValue(3);
            auto* accept = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            assert(accept->isEnabled()); accept->click();
        });
        native.openCargoManifestDialog();
        const auto evacuation = suns::MainWindowTestAccess::orders(native);
        assert(evacuation.orders.size() == 2);
        const auto& first = std::get<suns::TransferCargoOrder>(evacuation.orders.front());
        assert(first.source.fleet == 1 && first.minerals.ironium == 3);
        const auto evacuated = suns::TurnProcessor{}.process(suns::MainWindowTestAccess::state(native), {evacuation});
        assert(evacuated.planets.front().nativeOrbit.population == 0 && evacuated.planets.front().minerals.ironium == 103);
        assert(evacuated.fleets.front().colonists == 1'000 && evacuated.fleets.front().minerals.ironium == 0);
    }
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
        auto* exactBoron = dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin1");
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        assert(source && destination && people && exactPeople && iron && boron && exactIron && exactBoron && buttons);
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
        assert(destinationPeople && destinationPeople->value() == 100 && destinationPeople->maximum() == 500);
        assert(people->maximum() == 500 && exactPeople->maximum() == 300); // 2 kt already aboard
        assert(iron->maximum() == 500 && boron->maximum() == 500);
        boron->setValue(boron->maximum());
        assert(boron->value() == 25 && boron->maximum() == 500);
        boron->setValue(0);
        iron->setValue(iron->maximum());
        assert(exactIron->value() == 3 && exactPeople->maximum() == 0 && exactBoron->maximum() == 0);
        exactIron->setValue(1);
        assert(people->maximum() == 500 && exactPeople->maximum() == 200);
        people->setValue(people->maximum());
        assert(people->value() == 200 && exactPeople->value() == 200 && exactBoron->maximum() == 0);
        exactPeople->setValue(100);
        assert(iron->maximum() == 500 && boron->maximum() == 500 && exactIron->maximum() == 2);

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
    assert(suns::MainWindowTestAccess::systemInfo(window).contains("population after validated orders: 24900"));
    // One scalar estimate uses confirmed conditions and after-cargo density;
    // End Turn, future arrivals and production are not simulated.
    {
        const auto& baseline = suns::MainWindowTestAccess::state(window);
        const auto& planet = baseline.planets.front();
        const auto growth = suns::projected_population_growth(baseline, planet, baseline.turn);
        const double rate = static_cast<double>(growth) / planet.population;
        auto afterCargo=planet; afterCargo.population=24900;
        const auto estimated=suns::projected_population_growth(baseline,afterCargo,baseline.turn);
        const auto nextPopulation = static_cast<std::uint64_t>(24900 + estimated);
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
        assert(people->maximum() == 500 && iron->maximum() == 500);
        assert(dialog->findChild<QSpinBox*>("cargoColonistSpin")->maximum() == 100);
        const auto* destinationPeople = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        const auto* surfaceAfter = dialog->findChild<QLabel*>("cargoSourceAfterLabel");
        const auto* manifestAfter = dialog->findChild<QLabel*>("cargoDestinationAfterLabel");
        assert(destinationPeople && destinationPeople->value() == 200);
        assert(surfaceAfter && surfaceAfter->text().contains("24900"));
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

    cargo_summary_stays_in_place(app.arguments().contains("--capture"));
    cargo_manifest_overflow_can_be_scrolled();
    cargo_tracks_show_full_hold_and_surface_stock();
    cargo_fuel_preserves_exact_limits();

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
    assert(suns::MainWindowTestAccess::systemInfo(window).contains("population after validated orders: 22500"));
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        const auto* destination = dialog->findChild<QSlider*>("cargoDestinationColonistSlider");
        const auto* sourceAfter = dialog->findChild<QLabel*>("cargoSourceAfterLabel");
        const auto* destinationAfter = dialog->findChild<QLabel*>("cargoDestinationAfterLabel");
        assert(destination && destination->value() == 2500 && destination->maximum() == 2500);
        assert(sourceAfter && sourceAfter->text().contains("22500"));
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
    suns::MainWindowTestAccess::planningRefreshRegressions(dockWindow);
}
