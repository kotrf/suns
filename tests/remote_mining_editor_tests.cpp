#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/communications.hpp"

#include <QApplication>
#include <QComboBox>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w, const GameState& s) {
        w.state_ = s; w.pendingOrders_ = {1, {}}; w.pendingDescriptions_.clear();
        w.selection_.fleet = 1; w.selection_.star = 2;
        w.rebuildScene(); emit w.routeProgramContextChanged(true);
    }
    static PlayerOrders orders(const MainWindow& w) { return w.pendingOrders_; }
    static QStringList descriptions(const MainWindow& w) { return w.pendingDescriptions_; }
};
}

namespace {
using namespace suns;
void settle() { QApplication::processEvents(); QApplication::processEvents(); }
GameState fixture(double distance = 10, bool inOrbit = true) {
    auto s = make_demo_game();
    s.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 2;
    s.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Electronics)] = 1;
    s.shipDesigns.push_back({90, 1, "Test miner", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RoboMiniMiner}});
    auto& p = s.planets[1]; p.owner = 0; p.population = 0; p.minerals = {};
    p.observedConcentration = MineralCargo{100, 50, 25};
    s.stars[1].position = {distance, 0};
    auto& f = s.fleets.front(); f.design = 90; f.ships = {{90, 1}};
    f.position = inOrbit ? s.stars[1].position : s.stars[0].position;
    f.destination.reset(); f.waypointQueue.clear(); f.pendingCommands.clear();
    f.task = FleetTask::None; f.fuel = fleet_fuel_capacity(s, f);
    f.electronics = {EmissionMode::Passive, 0};
    f.telemetry = {}; f.telemetry.observedTurn = s.turn; f.telemetry.position = f.position;
    f.telemetry.ships = f.ships; f.telemetry.fuel = f.fuel; f.telemetry.warp = f.warp;
    f.telemetry.electronics = f.electronics;
    set_survey_level(s, 1, 2, SurveyLevel::OrbitalSurvey, s.turn);
    return s;
}
const Fleet& miner(const GameState& s) {
    return *std::find_if(s.fleets.begin(), s.fleets.end(), [](const auto& f) { return f.id == 1; });
}
void chooseMining(MainWindow& w) {
    auto* combo = w.findChild<QComboBox*>("routeArrivalActionCombo"); assert(combo);
    combo->setCurrentIndex(combo->findData(int(FleetArrivalActionKind::RemoteMining))); settle();
}

void stopThenMineCancelsSentNavigation(MainWindow& w) {
    // An old six-year packet remains in flight when a scheduled relay resumes
    // transmission and suddenly makes new commands instantaneous.
    auto s = fixture(800);
    s.fleets[0].electronics = {}; s.fleets[0].telemetry.electronics = {};
    s.players[0].technology.levels[static_cast<std::size_t>(ResearchField::Electronics)] = 24;
    s.shipDesigns.push_back({91, 1, "Resume relay", ShipHullType::Scout,
        {ShipComponentType::QuickJump5, ShipComponentType::PeerlessScanner}});
    auto relay = s.fleets.front(); relay.id = 2; relay.design = 91; relay.ships = {{91, 1}};
    relay.position = {400, 0}; relay.electronics = {EmissionMode::RadioSilence, s.turn + 1};
    relay.fuel = fleet_fuel_capacity(s, relay); relay.telemetry.position = relay.position;
    relay.telemetry.ships = relay.ships; relay.telemetry.fuel = relay.fuel;
    relay.telemetry.electronics = relay.electronics;
    s.fleets.push_back(relay); s.nextFleetId = 3;
    const auto delay = communication_delay_turns(s, 1, miner(s).position);
    assert(delay == 6);
    const TurnProcessor processor;
    auto boundary = processor.process(s, {{1, {MoveFleetOrder{1, {1000, 0}, 5}}}});
    assert(boundary.turn == s.turn + 1 && communication_delay_turns(boundary, 1, miner(boundary).position) == 0);
    assert(!miner(boundary).destination && miner(boundary).pendingCommands.size() == 1);
    const auto oldDelivery = miner(boundary).pendingCommands.front().deliveryTurn;
    assert(oldDelivery == s.turn + delay);

    assert(same_position(fleet_player_view(boundary, miner(boundary)).position, s.stars[1].position));
    assert(known_planet_owner(boundary, 1, boundary.planets[1].id) == std::optional<PlayerId>{0});
    MainWindowTestAccess::install(w, boundary); settle();
    auto* stop = w.findChild<QPushButton*>("routeStopButton"); assert(stop);
    auto* here = w.findChild<QPushButton*>("routeWorkHereButton"); assert(here);
    stop->click(); settle(); chooseMining(w); here->click(); settle();
    const auto orders = MainWindowTestAccess::orders(w);
    assert(orders.orders.size() == 2);
    assert(w.selectedFleetRouteProgramPolyline().size() < 2);
    auto started = processor.process(boundary, {orders});
    assert(miner(started).task == FleetTask::RemoteMining && !miner(started).destination);
    assert(miner(started).pendingCommands.empty());
    assert(w.selectedFleetRouteProgramSummary().contains("Queued work"));
    assert(!w.selectedFleetRouteProgramSummary().contains("set No Task when"));
    for (; started.turn <= oldDelivery + 2; started = processor.process(started, {})) {
        assert(same_position(miner(started).position, s.stars[1].position));
        assert(miner(started).task == FleetTask::RemoteMining && !miner(started).destination);
        assert(miner(started).electronics.mode == EmissionMode::Standard);
        assert(started.planets[1].minerals.ironium > 0);
    }

    // Control: dropping Stop reproduces the original failure beyond the old
    // delivery boundary, despite successful mining in the intervening years.
    auto droppedStop = processor.process(boundary, {{1, {SetRemoteMiningOrder{1, true}}}});
    while (droppedStop.turn <= oldDelivery) droppedStop = processor.process(droppedStop, {});
    assert(!same_position(miner(droppedStop).position, s.stars[1].position));
    assert(miner(droppedStop).task == FleetTask::None);

    // A Stop already sent on the old slow link must not arrive later and
    // cancel the new work either. Start reissues its cancellation first.
    MoveFleetOrder stopOrder{1, s.stars[1].position, 5}; stopOrder.clearRoute = true;
    auto stopping = processor.process(s, {{1, {MoveFleetOrder{1, {1000, 0}, 5}, stopOrder}}});
    assert(miner(stopping).pendingCommands.size() == 2);
    MainWindowTestAccess::install(w, stopping); settle(); chooseMining(w); here->click(); settle();
    auto resumed = processor.process(stopping, {MainWindowTestAccess::orders(w)});
    assert(miner(resumed).pendingCommands.empty());
    for (; resumed.turn <= oldDelivery + 2; resumed = processor.process(resumed, {})) {
        assert(miner(resumed).task == FleetTask::RemoteMining && !miner(resumed).destination);
        assert(same_position(miner(resumed).position, s.stars[1].position));
    }

    // Last-click Stop still cancels both new mining intent and old traffic.
    MainWindowTestAccess::install(w, boundary); settle(); stop->click(); settle();
    chooseMining(w); here->click(); settle(); stop->click(); settle();
    const auto stoppedOrders = MainWindowTestAccess::orders(w);
    assert(stoppedOrders.orders.size() == 1 && std::get<MoveFleetOrder>(stoppedOrders.orders[0]).clearRoute);
    auto stopped = processor.process(boundary, {stoppedOrders});
    for (; stopped.turn <= oldDelivery + 2; stopped = processor.process(stopped, {})) {
        assert(miner(stopped).task == FleetTask::None && !miner(stopped).destination);
        assert(miner(stopped).pendingCommands.empty());
        assert(same_position(miner(stopped).position, s.stars[1].position));
    }
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv); QTemporaryDir dir; assert(dir.isValid());
    QCoreApplication::setOrganizationName("SunsRemoteMiningEditorTests");
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir.path());
    MainWindow w; attachRouteProgramDock(w);
    auto* here = w.findChild<QPushButton*>("routeWorkHereButton"); assert(here);
    auto* tree = w.findChild<QTreeWidget*>("routeProgramQueue"); assert(tree);
    const TurnProcessor processor;

    // The real button submits a native task, creates no waypoint, and mines on
    // the next annual mining phase with no extra stationary "arrival" year.
    auto s = fixture(); assert(communication_delay_turns(s, 1, miner(s).position) == 0);
    MainWindowTestAccess::install(w, s); settle(); chooseMining(w); here->click(); settle();
    auto orders = MainWindowTestAccess::orders(w);
    assert(orders.orders.size() == 1 && std::holds_alternative<SetRemoteMiningOrder>(orders.orders[0]));
    assert(std::get<SetRemoteMiningOrder>(orders.orders[0]).enabled);
    assert(tree->topLevelItemCount() == 0 && w.selectedFleetRouteProgramPolyline().size() < 2);
    assert(w.selectedFleetRouteProgramSummary().contains("Queued work"));
    assert(w.selectedFleetRouteProgramSummary().contains("surface stockpile"));
    auto next = processor.process(s, {orders});
    assert(miner(next).task == FleetTask::RemoteMining && !miner(next).destination);
    assert(next.planets[1].minerals.ironium == 4 && next.planets[1].minerals.boranium == 2);
    assert(mineral_cargo_mass(miner(next).minerals) == 0);
    assert(miner(next).electronics.mode == EmissionMode::Passive);
    next = processor.process(next, {}); assert(next.planets[1].minerals.ironium == 8);

    // Repeated clicks replace intent. Stop -> Start -> Stop -> Start must honor
    // the last action even when Stop had occupied an earlier order slot.
    here->click(); settle(); assert(MainWindowTestAccess::orders(w).orders.size() == 1);
    assert(w.clearSelectedFleetRouteProgram());
    next = processor.process(s, {MainWindowTestAccess::orders(w)});
    assert(miner(next).task == FleetTask::None && next.planets[1].minerals.ironium == 0);
    assert(w.startSelectedFleetWorkHere({FleetArrivalActionKind::RemoteMining}));
    assert(w.clearSelectedFleetRouteProgram());
    assert(w.startSelectedFleetWorkHere({FleetArrivalActionKind::RemoteMining}));
    orders = MainWindowTestAccess::orders(w);
    assert(orders.orders.size() == 2 && std::get<MoveFleetOrder>(orders.orders[0]).clearRoute);
    assert(std::holds_alternative<SetRemoteMiningOrder>(orders.orders[1]));
    assert(w.selectedFleetRouteProgramSummary().contains("Queued work"));
    assert(processor.process(s, {orders}).planets[1].minerals.ironium == 4);

    // Existing save/order serialization carries the UI's actual native order.
    TurnOrderFileData packet; packet.campaignId = 1; packet.turn = s.turn; packet.turnToken = 1;
    packet.orders = orders; packet.descriptions = MainWindowTestAccess::descriptions(w);
    assert(packet.descriptions.size() == int(orders.orders.size()));
    QString error; const auto path = dir.filePath("mining.sunsorders");
    assert(write_turn_order_file(path, packet, error)); TurnOrderFileData restored;
    assert(read_turn_order_file(path, restored, error));
    assert(processor.process(s, {restored.orders}).planets[1].minerals.ironium == 4);

    // The dropdown alone is a draft; Add waypoint stores its arrival action.
    s = fixture(10, false); MainWindowTestAccess::install(w, s); settle(); chooseMining(w);
    auto* add = w.findChild<QPushButton*>("routeAddButton"); assert(add);
    auto* destinations = w.findChild<QComboBox*>("routeDestinationCombo"); assert(destinations);
    const auto destinationIndex = destinations->findData(2u, Qt::UserRole + 1);
    assert(destinationIndex >= 0 && destinations->itemData(destinationIndex).toInt() == 1);
    destinations->setCurrentIndex(destinationIndex); settle(); assert(add->isEnabled());
    add->click(); settle(); orders = MainWindowTestAccess::orders(w);
    assert(orders.orders.size() == 1);
    assert(std::get<MoveFleetOrder>(orders.orders[0]).arrivalAction.kind == FleetArrivalActionKind::RemoteMining);
    next = processor.process(s, {orders});
    for (int turn = 0; turn < 20 && miner(next).task != FleetTask::RemoteMining; ++turn)
        next = processor.process(next, {});
    assert(miner(next).task == FleetTask::RemoteMining && same_position(miner(next).position, s.stars[1].position));
    assert(next.planets[1].minerals.ironium == 0); // Travel does not grant a mining year.
    assert(processor.process(next, {}).planets[1].minerals.ironium == 4);

    // Route geometry remains visible and mouse-transparent, without map numbers.
    int routeSegments = 0;
    for (auto* item : w.routeProgramScene()->items()) {
        if (!item->data(73).toBool()) continue;
        assert(!dynamic_cast<QGraphicsTextItem*>(item));
        if (dynamic_cast<QGraphicsLineItem*>(item)) ++routeSegments;
        assert(item->acceptedMouseButtons() == Qt::NoButton);
    }
    assert(routeSegments == 1);

    // Editing an existing leg also preserves mining through command delay.
    s = fixture(410, false); s.fleets[0].position = {400, 0}; s.fleets[0].telemetry.position = {400, 0};
    assert(communication_delay_turns(s, 1, miner(s).position) > 1);
    MainWindowTestAccess::install(w, s); settle();
    assert(w.appendSelectedStarWaypoint(5, {}));
    assert(w.setSelectedFleetRouteProgramLegAction(0, {FleetArrivalActionKind::RemoteMining}));
    next = processor.process(s, {MainWindowTestAccess::orders(w)});
    assert(miner(next).task == FleetTask::None && !miner(next).pendingCommands.empty());
    for (int turn = 0; turn < 30 && miner(next).task != FleetTask::RemoteMining; ++turn)
        next = processor.process(next, {});
    assert(miner(next).task == FleetTask::RemoteMining && next.planets[1].minerals.ironium == 0);
    assert(processor.process(next, {}).planets[1].minerals.ironium == 4);

    // A remote stationary command waits for physical delivery; no forecast or
    // direct mutation starts it early. It still works with passive emissions.
    s = fixture(1000); const auto delay = communication_delay_turns(s, 1, miner(s).position); assert(delay > 1);
    MainWindowTestAccess::install(w, s); settle(); chooseMining(w); here->click(); settle();
    next = processor.process(s, {MainWindowTestAccess::orders(w)});
    assert(miner(next).task == FleetTask::None && !miner(next).pendingCommands.empty());
    assert(next.planets[1].minerals.ironium == 0);
    MainWindowTestAccess::install(w, next); settle();
    assert(w.selectedFleetRouteProgramSummary().contains("command in flight"));
    for (int turn = 0; turn < 30 && miner(next).task != FleetTask::RemoteMining; ++turn) {
        assert(next.planets[1].minerals.ironium == 0);
        next = processor.process(next, {});
    }
    // Commands arriving at the new planning boundary do not rewrite the
    // elapsed year. The next mining phase produces normally.
    assert(miner(next).task == FleetTask::RemoteMining && next.planets[1].minerals.ironium == 0);
    assert(processor.process(next, {}).planets[1].minerals.ironium == 4);

    // Reject unsupported hulls, owned/unknown worlds, deep space and a moving
    // fleet even after a Stop has been queued, without staging invalid orders.
    for (int kind = 0; kind < 5; ++kind) {
        auto bad = fixture(); auto& f = bad.fleets.front();
        if (kind == 0) { f.design = 1; f.ships = {{1, 1}}; f.telemetry.ships = f.ships; }
        if (kind == 1) { bad.planets[1].owner = 1; set_survey_level(bad, 1, 2, SurveyLevel::OrbitalSurvey, bad.turn); }
        if (kind == 2) { bad.players[0].surveyKnowledge.clear(); bad.players[0].surveyedStars.clear(); }
        if (kind == 3) { f.position = {300, 300}; f.telemetry.position = f.position; }
        if (kind == 4) { f.destination = Position{30, 0}; f.telemetry.destination = f.destination; }
        MainWindowTestAccess::install(w, bad); settle();
        if (kind == 4) assert(w.clearSelectedFleetRouteProgram());
        const auto count = MainWindowTestAccess::orders(w).orders.size();
        assert(!w.startSelectedFleetWorkHere({FleetArrivalActionKind::RemoteMining}));
        assert(MainWindowTestAccess::orders(w).orders.size() == count);
    }

    // Admission uses received ownership, not a host-only change since the
    // survey. The real host still validates physical ownership on delivery.
    auto hidden = fixture(); hidden.players.push_back({2, "Other"}); hidden.planets[1].owner = 2;
    assert(known_planet_owner(hidden, 1, hidden.planets[1].id) == std::optional<PlayerId>{0});
    MainWindowTestAccess::install(w, hidden); settle();
    assert(w.startSelectedFleetWorkHere({FleetArrivalActionKind::RemoteMining}));
    assert(processor.process(hidden, {MainWindowTestAccess::orders(w)}).planets[1].minerals.ironium == 0);
    hidden = fixture(10, false); hidden.players.push_back({2, "Other"}); hidden.planets[1].owner = 2;
    MainWindowTestAccess::install(w, hidden); settle();
    assert(w.appendSelectedStarWaypoint(5, {FleetArrivalActionKind::RemoteMining}));
    assert(w.setSelectedFleetRouteProgramLegAction(0, {}));
    assert(w.setSelectedFleetRouteProgramLegAction(0, {FleetArrivalActionKind::RemoteMining}));

    for (const auto owner : {PlayerId{1}, PlayerId{2}}) {
        auto colony = fixture(10, false); colony.players.push_back({2, "Other"});
        colony.planets[1].owner = owner;
        set_survey_level(colony, 1, 2, SurveyLevel::OrbitalSurvey, colony.turn);
        MainWindowTestAccess::install(w, colony); settle();
        assert(!w.appendSelectedStarWaypoint(5, {FleetArrivalActionKind::RemoteMining}));
        assert(w.appendSelectedStarWaypoint(5, {}));
        assert(!w.setSelectedFleetRouteProgramLegAction(0, {FleetArrivalActionKind::RemoteMining}));
    }

    // Unknown ownership may be attempted on arrival, with the host deciding
    // from physical truth then. Changing hidden ownership cannot change UI
    // admission, either when appending or when editing a pre-existing leg.
    for (const auto owner : {PlayerId{0}, PlayerId{2}}) {
        auto unknown = fixture(10, false); unknown.players.push_back({2, "Other"});
        unknown.planets[1].owner = owner;
        unknown.players[0].surveyKnowledge.clear(); unknown.players[0].surveyedStars.clear();
        MainWindowTestAccess::install(w, unknown); settle();
        assert(w.appendSelectedStarWaypoint(5, {FleetArrivalActionKind::RemoteMining}));
        assert(w.setSelectedFleetRouteProgramLegAction(0, {}));
        assert(w.setSelectedFleetRouteProgramLegAction(0, {FleetArrivalActionKind::RemoteMining}));
    }
    stopThenMineCancelsSentNavigation(w);
}
