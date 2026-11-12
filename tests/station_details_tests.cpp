#include "main_window.hpp"
#include "station_details_dialog.hpp"
#include "suns/communications.hpp"
#include "suns/stations.hpp"
#include "native_orbital_fixture.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEventLoop>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTableWidget>
#include <QToolButton>
#include <cassert>
#include <functional>

namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& window, const GameState& state, StarId star = 1, bool refit = false)
    {
        window.state_ = state;
        window.pendingOrders_ = {1, {}};
        window.pendingDescriptions_.clear();
        if (refit) window.pendingOrders_.orders.push_back(QueueStationOrder{1,
            {StationLoadout{OrbitalStationHullType::Fortress, {ShipComponentType::XRayLaser}, false, false}, 1}});
        window.selection_.star = star;
        window.rebuildScene();
    }
    static void deselect(MainWindow& window) { window.selection_.star.reset(); window.rebuildScene(); }
    static auto orderCount(const MainWindow& window) { return window.pendingOrders_.orders.size(); }
    static void designer(MainWindow& window) { window.openStationDesigner(); }
};
} // namespace suns

using namespace suns;
namespace {
void settle()
{
    QEventLoop loop;
    QTimer::singleShot(40, &loop, &QEventLoop::quit);
    loop.exec();
}

GameState fixture()
{
    auto state = make_demo_game();
    state.turn = 10;
    auto& station = state.orbitalStations.front();
    station.name = "Earth <Station>";
    station.components = {ShipComponentType::Laser, ShipComponentType::MoleSkinShield,
        ShipComponentType::BattleComputer, ShipComponentType::Laser};
    station.damagePercent = 12.5;
    station.modules = {OrbitalStationModule::Shipyard};
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    return state;
}

QString equipment(QDialog* dialog, int group, int index)
{
    auto* cell = dialog->findChild<QFrame*>(QString("stationFitCell%1_%2").arg(group).arg(index));
    assert(cell);
    return cell->findChild<QLabel*>("stationCellEquipment")->text();
}

void inspect(QToolButton* button, const std::function<void(QDialog*)>& check)
{
    assert(button->isVisible() && !button->icon().isNull());
    bool opened = false;
    QTimer::singleShot(40, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "stationDetailsDialog");
        opened = true;
        check(dialog);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close)->click();
    });
    button->click();
    assert(opened);
}

void hullsAndCells(bool capture, QWidget* parent)
{
    for (const auto& hullInfo : station_hulls()) {
        const auto hull=hullInfo.hull;
        auto station = fixture().orbitalStations.front();
        station.hull = hull;
        const auto& spec = station_hull_spec(hull);
        if (hull != OrbitalStationHullType::OrbitalDock) station.components.push_back(
            hull == OrbitalStationHullType::Fortress ? ShipComponentType::InfinityStargate : ShipComponentType::JumpGate);
        StationDetailsDialog dialog(station, 10, parent);
        dialog.show(); settle();
        assert(dialog.findChild<QLabel*>("stationDetailsName")->text() == "Earth <Station>");
        assert(dialog.findChild<QLabel*>("stationDetailsName")->textFormat() == Qt::PlainText);
        assert(dialog.findChild<QLabel*>("stationDetailsSummary")->text().contains("Damage 12.5%"));
        assert(dialog.findChild<QGroupBox*>("stationFitGroup0")->findChildren<QFrame*>(QRegularExpression("^stationFitCell")).size() == spec.weapons);
        assert(dialog.findChild<QGroupBox*>("stationFitGroup1")->findChildren<QFrame*>(QRegularExpression("^stationFitCell")).size() == spec.defenses);
        assert(dialog.findChild<QGroupBox*>("stationFitGroup2")->findChildren<QFrame*>(QRegularExpression("^stationFitCell")).size() == spec.electronics);
        assert(equipment(&dialog, 0, 0) == "Laser" && equipment(&dialog, 0, 1) == "Laser");
        assert(equipment(&dialog, 1, 0) == QString::fromStdString(component_spec(ShipComponentType::MoleSkinShield).name));
        assert(equipment(&dialog, 1, 1) == "Empty");
        assert(!dialog.findChild<QFrame*>("stationFitCell1_1")->property("occupied").toBool());
        if (hull != OrbitalStationHullType::OrbitalDock) {
            assert(equipment(&dialog, 2, 1) == (hull == OrbitalStationHullType::Fortress ? "Infinity / Infinity Gate" : "Jump Gate 160 / 450"));
            assert(dialog.findChild<QFrame*>("stationFitCell2_1")->toolTip().contains(
                hull == OrbitalStationHullType::Fortress ? "Unlimited" : "160"));
        }
        if (capture) dialog.grab().save(QString("/tmp/suns-station-fit-%1.png").arg(int(hull)));
        // A small window must keep Close reachable and allow every last cell
        // to be scrolled into view, rather than clipping the Fortress fit.
        dialog.resize(620, 490); settle();
        auto* scroll = dialog.findChild<QScrollArea*>("stationFitScroll");
        auto* last = dialog.findChild<QFrame*>(QString("stationFitCell1_%1").arg(spec.defenses - 1));
        scroll->ensureWidgetVisible(last); settle();
        assert(scroll->viewport()->rect().contains(QRect(last->mapTo(scroll->viewport(), QPoint{}), last->size())));
        auto* close = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close);
        assert(dialog.rect().contains(QRect(close->mapTo(&dialog, QPoint{}), close->size())));
    }
}
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    const bool capture = app.arguments().contains("--capture");
    MainWindow window;
    window.installUiPolish();
    window.installPlanetPolish();
    window.installProductionQueue();
    window.installFleetReadabilityPolish();
    window.installPanelLayoutFixes();
    hullsAndCells(capture, &window);
    auto state = fixture();
    MainWindowTestAccess::load(window, state, 1, true);
    window.resize(1366, 900); window.show(); settle();
    auto* button = window.findChild<QToolButton*>("systemStationButton");
    auto* details = window.findChild<QDockWidget*>("systemDetailsDock");
    assert(button && details && details->isAncestorOf(button));
    assert(button->text() == "Orbital Dock…");
    auto* header = window.findChild<QWidget*>("systemPlanetHeader");
    auto* portrait = window.findChild<QLabel*>("planetPortrait");
    assert(header && portrait && header->isAncestorOf(button) && header->isAncestorOf(portrait));
    assert(button->toolButtonStyle() == Qt::ToolButtonIconOnly && button->width() <= 30 && button->height() <= 30);
    assert(header->height() == portrait->height()); // No extra station row.
    const QRect planetRect(portrait->mapTo(header, QPoint{}), portrait->size());
    const QRect stationRect(button->mapTo(header, QPoint{}), button->size());
    assert(!planetRect.intersects(stationRect) && header->rect().contains(stationRect));
    assert(stationRect.top() <= planetRect.top() && stationRect.right() >= header->width() - 2);
    if (capture) details->grab().save("/tmp/suns-station-system-details.png");
    inspect(button, [&](QDialog* dialog) {
        assert(dialog->findChild<QLabel*>("stationDetailsSummary")->text().startsWith("Orbital Dock"));
        assert(equipment(dialog, 0, 0) == "Laser"); // Queued Fortress/X-Ray fit is not operational.
        assert(dialog->findChild<QLabel*>("stationDetailsServices")->text()
            == "Shipyard / fleet repair: Fitted • Refueling depot: Not fitted");
        assert(dialog->findChild<QLabel*>("stationDetailsKnowledge")->text().contains("turn 10"));
    });
    assert(MainWindowTestAccess::orderCount(window) == 1);
    assert(!window.findChild<QDialog*>("stationDetailsDialog")); // No stale snapshot/dialog after closing.

    // Snapshot with an earlier fit survives a hidden refit or destruction,
    // equally in a host UI and a sanitized PBEM player view.
    auto changed = state;
    changed.turn = 12;
    changed.orbitalStations.front().hull = OrbitalStationHullType::Fortress;
    changed.orbitalStations.front().components = {ShipComponentType::XRayLaser};
    for (const bool destroyed : {false, true}) {
        auto hidden = changed;
        if (destroyed) hidden.orbitalStations.clear();
        for (const bool exported : {false, true}) {
            MainWindowTestAccess::load(window, exported ? make_player_view(hidden, 1).state : hidden);
            settle();
            assert(button->text() == "Orbital Dock…");
            inspect(button, [&](QDialog* dialog) { assert(equipment(dialog, 0, 0) == "Laser"); });
        }
    }
    // Receipt of the new observation updates both the icon and the opened fit.
    set_survey_level(changed, 1, 1, SurveyLevel::DeepSurvey, changed.turn);
    MainWindowTestAccess::load(window, changed); settle();
    assert(button->text() == "Fortress…");
    inspect(button, [&](QDialog* dialog) { assert(equipment(dialog, 0, 0) == "X-Ray Laser"); });
    changed.orbitalStations.clear();
    set_survey_level(changed, 1, 1, SurveyLevel::DeepSurvey, changed.turn);
    MainWindowTestAccess::load(window, changed); settle();
    assert(!button->isVisible());

    // An explicit delivered 'no station' snapshot hides a newly built station
    // until that construction is reported.
    changed.orbitalStations = state.orbitalStations;
    MainWindowTestAccess::load(window, changed); settle();
    assert(!button->isVisible());
    // Older station-loss ledgers without a habitat snapshot use the received
    // PendingStationContact, not the absent physical object.
    auto legacy = state;
    for (auto& knowledge : legacy.players[0].surveyKnowledge) if (knowledge.planetSnapshot)
        knowledge.planetSnapshot->habitat.reset();
    legacy.pendingStationContacts = {{legacy.orbitalStations.front(), 10, 15}};
    legacy.orbitalStations.clear(); legacy.turn = 12;
    MainWindowTestAccess::load(window, legacy); settle();
    inspect(button, [&](QDialog* dialog) { assert(equipment(dialog, 0, 0) == "Laser"); });

    // Foreign stations and their fits are not in this player's received data.
    auto foreign = state;
    foreign.planets[1].owner = 2;
    foreign.orbitalStations.front().owner = 2;
    foreign.orbitalStations.front().planet = 2;
    set_survey_level(foreign, 1, 2, SurveyLevel::DeepSurvey, foreign.turn);
    MainWindowTestAccess::load(window, foreign, 2); settle();
    assert(!button->isVisible());
    MainWindowTestAccess::load(window, state, 2); settle();
    assert(!button->isVisible());
    MainWindowTestAccess::load(window, state); settle();
    assert(button->isVisible());
    MainWindowTestAccess::deselect(window); settle();
    assert(!button->isVisible());

    auto native = test::empty_native_orbital_start();
    assert(initialize_native_orbital_start(native, 1, native.planets.front().id, native_orbital_start_budget()));
    MainWindowTestAccess::load(window, native); settle();
    inspect(button, [&](QDialog* dialog) {
        assert(equipment(dialog, 1, 0) == "Bulk Radiation Screen");
        const auto* radiation = dialog->findChild<QLabel*>("stationRadiationStatus");
        assert(radiation && radiation->text().contains("background 45 - protection 20 = exposure 25"));
        assert(radiation->text().contains("0 residents / year"));
        if (capture) dialog->grab().save("/tmp/suns-native-radiation-details.png");
    });
    native.orbitalStations.front().components = {ShipComponentType::AdvancedRadiationScreen};
    native.planets.front().nativeOrbit.population = 12'000;
    ++native.turn;
    for (bool exported : {false, true}) {
        MainWindowTestAccess::load(window, exported ? make_player_view(native, 1).state : native); settle();
        inspect(button, [&](QDialog* dialog) {
            assert(equipment(dialog, 1, 0) == "Bulk Radiation Screen");
            assert(dialog->findChild<QLabel*>("stationRadiationStatus")->text().contains("protection 20"));
        });
    }
    // Preview uses only received residents/fit. It remains a cheap scalar calculation.
    QTimer::singleShot(40, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "stationDesignerDialog");
        auto* table = dialog->findChild<QTableWidget*>("stationFittingTable"); assert(table);
        auto* defense = qobject_cast<QComboBox*>(table->cellWidget(2, 1)); assert(defense);
        assert(defense->currentData().toInt() == int(ShipComponentType::BulkRadiationScreen));
        assert(defense->findData(int(ShipComponentType::AdvancedRadiationScreen)) == -1);
        defense->setCurrentIndex(defense->findData(-1));
        auto* preview = dialog->findChild<QLabel*>("stationFitPreview"); assert(preview);
        assert(preview->text().contains("exposure 45") && preview->text().contains("250 residents / year"));
        assert(preview->text().contains("Cost 24 production"));
        if (capture) dialog->grab().save("/tmp/suns-native-radiation-preview.png");
        dialog->reject();
    });
    MainWindowTestAccess::designer(window);
    set_survey_level(native, 1, native.planets.front().star, SurveyLevel::DeepSurvey, native.turn);
    MainWindowTestAccess::load(window, native); settle();
    inspect(button, [&](QDialog* dialog) {
        assert(equipment(dialog, 1, 0) == "Advanced Radiation Screen");
        assert(dialog->findChild<QLabel*>("stationRadiationStatus")->text().contains("protection 60"));
    });
}
