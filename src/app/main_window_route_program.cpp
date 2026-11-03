#include "suns/transport_lines.hpp"
#include "suns/orbital_habitats.hpp"
#include "main_window.hpp"
#include "suns/deep_space.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/wormholes.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/campaign.hpp"
#include "suns/interstellar_medium.hpp"

#include "suns/communications.hpp"

#include <QGraphicsView>
#include <QInputDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>

namespace suns {

namespace {

const Fleet* findFleet(const GameState& state, FleetId id)
{
    for (const auto& transit : state.wormholeTransits) if (transit.lastContact.id == id
        && transit.status != WormholeTransitStatus::PresumedLost
        && transit.status != WormholeTransitStatus::EmergenceConfirmed) return &transit.lastContact;
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    if (it != state.fleets.end()) return &*it;
    return nullptr;
}

const StarSystem* findStarAtPosition(const GameState& state, Position position)
{
    const auto it = std::find_if(state.stars.begin(), state.stars.end(), [&](const StarSystem& star) {
        return same_position(star.position, position);
    });
    return it == state.stars.end() ? nullptr : &*it;
}

QString waypointName(const GameState& state, Position position, FleetId targetFleet = 0)
{
    if (targetFleet != 0) {
        if (const auto* fleet = findFleet(state, targetFleet)) {
            return QString("%1 [moving fleet]").arg(QString::fromStdString(fleet->name));
        }
        return QString("Fleet %1 [target lost]").arg(targetFleet);
    }
    if (const auto* star = findStarAtPosition(state, position)) {
        return QString::fromStdString(star->name);
    }
    return QString("(%1, %2)").arg(position.x, 0, 'f', 1).arg(position.y, 0, 'f', 1);
}

QString gateDestinationName(const GameState& state, PlanetId id)
{
    const auto* planet=find_planet(state,id);
    const auto* star=planet?find_star(state,planet->star):nullptr;
    return star?QString::fromStdString(star->name):QString("Gate %1").arg(id);
}

QString actionName(const GameState& state, const FleetArrivalAction& action)
{
    if (action.kind == FleetArrivalActionKind::ScrapFleet) return "Scrap Fleet (all ships)";
    if (action.kind == FleetArrivalActionKind::TransitStargate) return QString("Gate %1 → %2%3; wait and retry if unavailable")
        .arg(gateDestinationName(state,action.gateSource),gateDestinationName(state,action.gateDestination)).arg(action.gateResonant ? " (8 source xenocrystals on success)" : "");
    if (action.kind == FleetArrivalActionKind::DeployOutpost) return "Deploy outpost — permanent";
    if (action.kind == FleetArrivalActionKind::RecoverSample) return "Recover scientific specimen — one attempt";
    if (action.kind == FleetArrivalActionKind::SupplyOutpost) return QString("Supply fuel; keep %1 mg").arg(action.reserveFuel);
    if (const auto task = arrival_work_task(action.kind))
        return QString::fromStdString(fleet_task_name(*task)) + " — persistent";
    const auto cargoName = [&] {
        switch (action.cargo) {
        case FleetCargoKind::OrbitalColonists: return QString("orbital colonists");
        case FleetCargoKind::Colonists: return QString("colonists");
        case FleetCargoKind::Ironium: return QString("Ironium");
        case FleetCargoKind::Boranium: return QString("Boranium");
        case FleetCargoKind::Germanium: return QString("Germanium");
        case FleetCargoKind::All: return QString("cargo");
        case FleetCargoKind::Xenocrystal: return QString("xenocrystals");
        }
        return QString("cargo");
    };
    switch (action.kind) {
    case FleetArrivalActionKind::None:
        return "no action";
    case FleetArrivalActionKind::LoadMinerals:
        return QString("Load %1; leave %2 kt; departure minimum %3 kt (0 = no wait)")
            .arg(cargoName()).arg(action.mineralReserve).arg(action.mineralMinimum);
    case FleetArrivalActionKind::LoadAllAvailable:
        return action.cargo == FleetCargoKind::Colonists
            ? QString("Load all %1, leave %2 — dynamic")
                  .arg(cargoName())
                  .arg(static_cast<qulonglong>(action.reservePopulation))
            : QString("Load all available %1 — dynamic").arg(cargoName());
    case FleetArrivalActionKind::UnloadTransportStop:
        return QString("Deliver %1 up to target %2 kt (0 = all); carry surplus to next stop").arg(cargoName()).arg(action.destinationTarget);
    case FleetArrivalActionKind::UnloadTransportLine:
        if(action.transportDestinationFleet)return QString("Supply %1 to installation #%2; target %3 kt (0 = unload within capacity); retry with surplus aboard")
            .arg(cargoName()).arg(action.transportDestinationFleet).arg(action.destinationTarget);
        if(action.destinationTarget)return QString("Top up %1 to %2 kt; keep surplus aboard and retry annually").arg(cargoName()).arg(action.destinationTarget);
        [[fallthrough]];
    case FleetArrivalActionKind::UnloadAll:
        return QString("Unload all %1 — dynamic").arg(cargoName());
    case FleetArrivalActionKind::Refuel:
        return "Refuel on arrival";
    case FleetArrivalActionKind::Colonize:
        return QString("Colonize world — %1; dismantles entire fleet")
            .arg(action.colonyPlan ? QString::fromStdString(action.colonyPlan->name) : "no colony plan");
    case FleetArrivalActionKind::RemoteMining:
        return "Remote Mining — persistent";
    case FleetArrivalActionKind::MergeWithFleet:
        return "Merge with fleet — terminal";
    case FleetArrivalActionKind::EnterWormhole:
        return QString("Enter WH %1 — uncertain exit, fleet-loss risk").arg(action.wormholeEndpoint);
    }
    return "no action";
}

MoveFleetOrder* pendingMove(PlayerOrders& pending, FleetId fleet)
{
    bool transportAfter = false;
    for (auto it = pending.orders.rbegin(); it != pending.orders.rend(); ++it) {
        transportAfter |= std::holds_alternative<SetTransportLineOrder>(*it) || std::holds_alternative<AssignTransportLineOrder>(*it);
        if (auto* move = std::get_if<MoveFleetOrder>(&*it); move && move->fleet == fleet)
            return transportAfter ? nullptr : move;
    }
    return nullptr;
}

const MoveFleetOrder* pendingMove(const PlayerOrders& pending, FleetId fleet)
{
    for (const auto& order : pending.orders) {
        if (const auto* move = std::get_if<MoveFleetOrder>(&order); move && move->fleet == fleet) return move;
    }
    return nullptr;
}

std::optional<MoveFleetOrder> effectiveRoute(const GameState& state, const PlayerOrders& pending, const Fleet& fleet)
{
    if (const auto route = pending_transport_route(state,pending,fleet.id)) return route;
    if (const auto* move = pendingMove(pending, fleet.id)) return *move;
    // The owner's latest sent navigation intent outranks older telemetry,
    // whether that snapshot shows an idle fleet or a still-active old route.
    const auto actual = std::find_if(state.fleets.begin(), state.fleets.end(),
        [&](const Fleet& candidate) { return candidate.id == fleet.id && candidate.owner == fleet.owner; });
    if (actual != state.fleets.end()) {
        for (auto it = actual->pendingCommands.rbegin(); it != actual->pendingCommands.rend(); ++it) {
            if (it->task || it->electronics || it->mobileBase || it->probe || it->visitor || it->orbitalBase) continue;
            const auto& command = it->program;
            return MoveFleetOrder{fleet.id, command.destination, command.warp,
                command.arrivalAction, command.queuedWaypoints, command.repeatOrders,
                command.targetFleet, command.clearRoute};
        }
    }
    if (!fleet.destination) return std::nullopt;

    return MoveFleetOrder{
        fleet.id,
        *fleet.destination,
        fleet.warp,
        fleet.arrivalAction.value_or(FleetArrivalAction{}),
        fleet.waypointQueue,
        fleet.repeatOrders,
        fleet.targetFleet,
    };
}

bool routeIsClearIntent(const Fleet& fleet, const MoveFleetOrder& move)
{
    return move.clearRoute || (same_position(fleet.position, move.destination)
        && move.targetFleet == 0
        && move.arrivalAction.kind == FleetArrivalActionKind::None
        && move.queuedWaypoints.empty());
}

QString routeDescription(const GameState& state, const Fleet& fleet, const MoveFleetOrder& route)
{
    if (routeIsClearIntent(fleet, route)) {
        return QString("Clear route program for %1").arg(QString::fromStdString(fleet.name));
    }

    const auto legs = static_cast<qulonglong>(1 + route.queuedWaypoints.size());
    return QString("Program %1 — %2 leg%3%4, next %5 at W%6")
        .arg(QString::fromStdString(fleet.name))
        .arg(legs)
        .arg(legs == 1 ? "" : "s")
        .arg(route.repeatOrders ? ", repeating" : "")
        .arg(waypointName(state, route.destination, route.targetFleet))
        .arg(route.warp);
}

std::vector<FleetWaypoint> routeLegs(const MoveFleetOrder& route)
{
    std::vector<FleetWaypoint> legs;
    legs.reserve(1 + route.queuedWaypoints.size());
    legs.push_back({route.destination, route.warp, route.arrivalAction, route.targetFleet});
    legs.insert(legs.end(), route.queuedWaypoints.begin(), route.queuedWaypoints.end());
    return legs;
}

} // namespace

FleetId MainWindow::selectedFleetForRouteProgram() const
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    return fleet ? fleet->id : 0;
}

std::uint8_t MainWindow::selectedFleetMaxWarpForRouteProgram() const
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) return 0;
    return fleet_max_warp(state_, *fleet);
}

std::uint8_t MainWindow::selectedFleetSuggestedWarpForRouteProgram() const
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) return 1;
    const auto maxWarp = selectedFleetMaxWarpForRouteProgram();
    return maxWarp == 0 ? 1 : std::clamp<std::uint8_t>(fleet->warp, 1, kMaxWarp);
}

double MainWindow::selectedFleetOverdriveDamageForRouteProgram(std::uint8_t warp) const
{
    const auto* fleet = selectedFleet();
    if (!fleet) return 0.0;
    return fleet_overdrive_damage_rate(receivedPlayerState(), fleet_player_view(state_, *fleet), warp);
}

bool MainWindow::selectedFleetRepeatOrdersForRouteProgram() const
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) return false;
    const auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    return route && !routeIsClearIntent(*fleet, *route) && route->repeatOrders;
}

QString MainWindow::selectedFleetRouteProgramSummary() const
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) return "<b>Route program:</b> select a fleet on the map.";

    const auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) {
        const auto task = QString::fromStdString(fleet_task_name(fleet->task));
        auto summary = QString("<b>%1 route:</b> none<br><b>Current task:</b> %2")
                           .arg(QString::fromStdString(fleet->name))
                           .arg(task);
        if (route && routeIsClearIntent(*fleet, *route)) {
            summary += "<br><i>Pending: clear route and set No Task when the command arrives.</i>";
        }
        const auto age = authoritativeFleet ? fleet_telemetry_age(state_, *authoritativeFleet) : 0;
        if (!route && age > 0) {
            summary += QString("<br><i>No confirmed waypoint in telemetry (%1 year(s) old). "
                "A late command or unreported onboard Colonize action may still exist; "
                "an empty list is not confirmation that the order was cancelled.</i>").arg(age);
        }
        return summary;
    }

    QStringList lines;
    lines << QString("<b>%1 — %2 route point%3%4</b>")
                 .arg(QString::fromStdString(fleet->name))
                 .arg(static_cast<qulonglong>(1 + route->queuedWaypoints.size()))
                 .arg(route->queuedWaypoints.empty() ? "" : "s")
                 .arg(route->repeatOrders ? " — Repeat Orders" : "");

    if (pendingMove(pendingOrders_, fleet->id)) {
        lines << "<i>Pending program is transmitted on End Turn; a remote fleet keeps its known onboard program until the command arrives.</i>";
    }
    return lines.join("<br>");
}

QString MainWindow::selectedFleetRouteProgramEstimate() const
{
    if (!selectedFleet()) return "Select a fleet first.";
    const auto rows = selectedFleetRouteProgramRows();
    if (rows.empty()) return "No route to estimate.";
    QStringList lines;
    lines << "<b>Nominal travel time</b>"
          << "Distance / Warp², rounded up for each leg. Uses received information and the planned route. "
             "Command delivery, fuel, refuelling, clouds and arrival actions can change actual timing.";
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto& row = rows[index];
        lines << QString("%1. %2 — %3 years, W%4<br>%5")
            .arg(qulonglong(index + 1)).arg(row.destination.toHtmlEscaped(), row.eta)
            .arg(row.warp).arg(row.arrivalAction.toHtmlEscaped());
    }
    lines << "Only the first pass of Repeat Orders is listed. "
             "Gate transit, wormhole exits and arrival outcomes are unknown until resolved on End Turn; "
             "later arrival times remain unknown.";
    return lines.join("<br>");
}

std::vector<RouteProgramDisplayRow> MainWindow::selectedFleetRouteProgramRows() const
{
    const auto* selected = selectedFleet();
    if (!selected) return {};
    const auto fleet = fleet_player_view(state_, *selected);
    const auto route = effectiveRoute(state_, pendingOrders_, fleet);
    if (!route || routeIsClearIntent(fleet, *route)) return {};

    std::vector<RouteProgramDisplayRow> rows;
    const auto legs = routeLegs(*route);
    rows.reserve(legs.size());
    auto departure = fleet.position;
    std::uint64_t elapsed = 0;
    bool knownTime = true;
    const QString geometricNote = "Nominal travel time: cumulative distance / Warp², rounded up for each leg. "
        "Uses received information. Command delivery, fuel, refuelling, clouds and arrival actions are not predicted.";
    QString uncertainty;
    for (std::size_t index = 0; index < legs.size(); ++index) {
        const auto& leg = legs[index];
        auto destination = leg.destination;
        auto destinationName = waypointName(state_, destination);
        if (leg.targetFleet) {
            if (const auto target = knownFleetTarget(leg.targetFleet)) {
                destination = target->position;
                destinationName = QString("%1 [estimated position]").arg(QString::fromStdString(target->name));
            } else {
                destinationName = QString("Fleet %1 [position unknown]").arg(leg.targetFleet);
                knownTime = false;
                uncertainty = "The target's received position is unavailable.";
            }
        }
        const auto speed = warp_distance(leg.warp);
        const auto years = speed > 0 ? std::ceil(distance_between(departure, destination) / speed) : -1.0;
        if (!std::isfinite(years) || years < 0 || years > double(std::numeric_limits<std::uint32_t>::max())) {
            knownTime = false;
            uncertainty = "No geometric travel time is available for this leg.";
        }
        if (knownTime) elapsed += travel_turns(departure, destination, speed);
        auto note = geometricNote;
        if (!knownTime) note += "\n" + uncertainty;
        if (leg.arrivalAction.kind == FleetArrivalActionKind::TransitStargate) {
            destinationName += " → " + gateDestinationName(receivedPlayerState(), leg.arrivalAction.gateDestination) + " [gate]";
            note += "\nThis is the approach time to the source gate only. Gate availability, payment and transit are resolved on End Turn.";
        } else if (leg.arrivalAction.kind == FleetArrivalActionKind::EnterWormhole) {
            note += "\nThis is the approach time only. The wormhole exit and survival cannot be estimated.";
        } else if (leg.targetFleet) {
            note += "\nTime to the target position estimated from received information only; not an interception prediction.";
        }
        rows.push_back({destinationName, actionName(receivedPlayerState(), leg.arrivalAction), leg.warp, index == 0,
            knownTime ? QString("≈%1").arg(qulonglong(elapsed)) : QString("—"), note});
        departure = destination;
        if (leg.targetFleet || leg.arrivalAction.kind != FleetArrivalActionKind::None) {
            knownTime = false;
            uncertainty = "A preceding arrival action or moving target makes later arrival times unknown.";
        }
    }
    return rows;
}

std::vector<Position> MainWindow::selectedFleetRouteProgramPolyline() const
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) return {};

    std::vector<Position> points;
    points.push_back(fleet->position);

    const auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) return points;

    const auto resolvedPosition = [&](Position snapshot, FleetId target) {
        if (const auto targetFleet = knownFleetTarget(target)) return targetFleet->position;
        return snapshot;
    };
    for (const auto& waypoint : routeLegs(*route)) {
        points.push_back(resolvedPosition(waypoint.destination,waypoint.targetFleet));
        if (waypoint.arrivalAction.kind==FleetArrivalActionKind::TransitStargate) {
            const auto& received = receivedPlayerState();
            const auto* p=find_planet(received,waypoint.arrivalAction.gateDestination);
            if (const auto* star=p?find_star(received,p->star):nullptr) points.push_back(star->position);
        }
    }
    return points;
}

std::vector<FleetId> MainWindow::availableFleetTargetsForRouteProgram() const
{
    const auto source = selectedFleetForRouteProgram();
    auto targets = availableOwnedFleetsForRouteProgram();
    std::erase(targets, source);
    for (const auto& contact : known_enemy_contacts(state_, pendingOrders_.player))
        if (contact.fleet != source && !contact.lost && contact.quality != ContactQuality::Probable)
            targets.push_back(contact.fleet);
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    return targets;
}

std::vector<FleetId> MainWindow::availableOwnedFleetsForRouteProgram() const
{
    std::vector<FleetId> fleets;
    for (const auto& fleet : fleetPlanningState().fleets) {
        if (fleet.owner == pendingOrders_.player) fleets.push_back(fleet.id);
    }
    for (const auto& contact : missing_fleet_contacts(state_, pendingOrders_.player)) fleets.push_back(contact.id);
    std::sort(fleets.begin(), fleets.end());
    fleets.erase(std::unique(fleets.begin(), fleets.end()), fleets.end());
    return fleets;
}

std::vector<FleetId> MainWindow::ownedFleetsAtSelectedSystem() const
{
    const auto* star = selectedStar();
    if (!star) return {};
    std::vector<FleetId> fleets;
    for (const auto id : availableOwnedFleetsForRouteProgram()) {
        const auto* fleet = findFleet(fleetPlanningState(), id);
        if (fleet && same_position(fleet_player_view(state_, *fleet).position, star->position))
            fleets.push_back(id);
    }
    return fleets;
}

QString MainWindow::fleetSourceNameForRouteProgram(FleetId id) const
{
    const auto* fleet = findFleet(fleetPlanningState(), id);
    if (!fleet || unreported_orbital_ship(state_,*fleet)) return fleetTargetNameForRouteProgram(id);
    auto name = fleetTargetNameForRouteProgram(id);
    for (const auto& order : pendingOrders_.orders) {
        if (const auto* rename = std::get_if<RenameFleetOrder>(&order); rename && rename->fleet == id)
            name = QString("%1 (Fleet %2)").arg(QString::fromStdString(rename->name)).arg(id);
    }
    const auto location = fleetLocationName(fleet_player_view(state_, *fleet).position);
    const auto age = fleet_telemetry_age(state_, *fleet);
    if (const auto radar = fleet_radar_position(state_, *fleet))
        return QString("%1 — Radar: %2 • position %3 turn(s) old • uncertainty %4 ly")
            .arg(name, location).arg(qulonglong(fleet_position_age(state_, *fleet)))
            .arg(radar->uncertainty, 0, 'f', 1);
    return age == 0 ? QString("%1 — %2").arg(name, location)
        : QString("%1 — Estimated: %2 • report %3 turn(s) old")
            .arg(name, location).arg(static_cast<qulonglong>(age));
}

QString MainWindow::selectedSystemFleetHeading() const
{
    const auto* star = selectedStar();
    if (!star) return "All my fleets";
    const auto count = ownedFleetsAtSelectedSystem().size();
    return QString("At %1: %2 friendly fleet%3")
        .arg(QString::fromStdString(star->name)).arg(count).arg(count == 1 ? "" : "s");
}

std::optional<Fleet> MainWindow::knownFleetTarget(FleetId id) const
{
    if(const auto* f=findFleet(fleetPlanningState(),id); f && f->owner==pendingOrders_.player) return *f;
    for(const auto& f:missing_fleet_contacts(state_,pendingOrders_.player)) if(f.id==id) return f;
    for(const auto& c:known_enemy_contacts(state_,pendingOrders_.player))
        if(c.fleet==id && !c.lost && c.quality!=ContactQuality::Probable) return recon_contact_fleet(c,state_.turn);
    return {};
}

QString MainWindow::fleetTargetNameForRouteProgram(FleetId fleetId) const
{
    if (const auto fleet = knownFleetTarget(fleetId)) {
        return QString("%1 (Fleet %2)").arg(QString::fromStdString(fleet->name)).arg(fleetId);
    }
    for (const auto& contact : missing_fleet_contacts(state_, pendingOrders_.player)) if (contact.id == fleetId)
        return QString("%1 (Fleet %2)").arg(QString::fromStdString(contact.name)).arg(fleetId);
    return QString("Fleet %1").arg(fleetId);
}

std::vector<RouteProgramTargetOption> MainWindow::routeProgramTargetsAtSelectedSystem() const
{
    const auto* star = selectedStar();

    std::vector<RouteProgramTargetOption> targets;
    if (star) targets.push_back({
        QString("System — %1").arg(QString::fromStdString(star->name)),
        1,
        star->id,
        false,
    });

    const auto source = selectedFleetForRouteProgram();
    for (const auto& fleet : fleetPlanningState().fleets) {
        if (fleet.id == source) continue;
        const auto& visible = fleet;
        const bool local = star && same_position(visible.position, star->position);
        const bool enemy = fleet.owner != pendingOrders_.player;
        if (!local && enemy) continue;
        targets.push_back({
            QString(enemy ? "Enemy fleet — %1" : "Fleet — %1")
                .arg(QString::fromStdString(visible.name))
                + (local ? QString{} : " — " + fleetLocationName(visible.position)),
            enemy ? 3 : 2,
            fleet.id,
            enemy,
        });
    }
    return targets;
}

bool MainWindow::selectFleetForRouteProgram(FleetId fleetId)
{
    const auto* fleet = findFleet(fleetPlanningState(), fleetId);
    const auto missing = missing_fleet_contacts(state_, pendingOrders_.player);
    const bool ownContact = std::any_of(missing.begin(), missing.end(), [=](const auto& contact) { return contact.id == fleetId; });
    if (fleet && unreported_orbital_ship(state_,*fleet)) return false;
    if ((!fleet || fleet->owner != pendingOrders_.player) && !ownContact) return false;
    if (selection_.fleet == fleetId) return true;
    warpControlFleetId_.reset();
    logisticsControlFleetId_.reset();
    if (!selectWorkspaceObject(2, fleetId)) return false;
    rebuildScene(false);
    return true;
}

bool MainWindow::beginRouteProgramMapTargetPick()
{
    if (!selectedFleet()) {
        statusBar()->showMessage("Select a source fleet before choosing a route target", 3000);
        return false;
    }
    if (routeProgramMapTargetPickActive_) return true;
    routeProgramMapTargetPickActive_ = true;
    if (view_ && view_->viewport()) view_->viewport()->setCursor(Qt::CrossCursor);
    emit routeProgramMapTargetPickChanged(true);
    statusBar()->showMessage("Route target mode: click a point, star or another fleet; Esc cancels");
    return true;
}

void MainWindow::cancelRouteProgramMapTargetPick()
{
    if (!routeProgramMapTargetPickActive_) return;
    routeProgramMapTargetPickActive_ = false;
    if (view_ && view_->viewport()) view_->viewport()->unsetCursor();
    emit routeProgramMapTargetPickChanged(false);
}

bool MainWindow::routeProgramMapTargetPickActive() const
{
    return routeProgramMapTargetPickActive_;
}

QString MainWindow::routeProgramMapTargetName(int kind, std::uint32_t id) const
{
    if (kind == 1) {
        if (const auto* star = find_star(state_, static_cast<StarId>(id))) {
            return QString::fromStdString(star->name);
        }
    } else if (kind == 2 || kind == 3) {
        if (const auto fleet = knownFleetTarget(static_cast<FleetId>(id))) {
            return QString::fromStdString(fleet->name);
        }
    }
    return "unknown target";
}

bool MainWindow::selectRouteProgramMapTarget(int kind, std::uint32_t id)
{
    const auto* source = selectedFleet();
    if (!source) {
        statusBar()->showMessage("Select a source fleet before choosing a route target", 3000);
        return false;
    }

    if (kind == 1) {
        if (!find_star(state_, static_cast<StarId>(id))) {
            statusBar()->showMessage("The selected star is no longer available", 3000);
            return false;
        }
        if (!selectWorkspaceObject(1, id)) return false;
    } else if (kind == 2 || kind == 3) {
        const auto target = knownFleetTarget(static_cast<FleetId>(id));
        if (!target || target->id == source->id) {
            statusBar()->showMessage(
                "Choose another fleet as the route target", 3000);
            return false;
        }
        kind = target->owner == pendingOrders_.player ? 2 : 3;
    } else {
        return false;
    }

    if (kind != 1) rememberMapSelection(2, id);
    emit routeProgramMapTargetPicked(kind, id);
    return true;
}

bool MainWindow::startSelectedFleetWorkHere(FleetArrivalAction action)
{
    const auto* source = selectedFleet();
    if (!source || source->owner != pendingOrders_.player
        || (!arrival_work_task(action.kind) && action.kind != FleetArrivalActionKind::ScrapFleet)) return false;
    const auto fleet = fleet_player_view(state_, *source);
    const auto route = effectiveRoute(state_, pendingOrders_, fleet);
    if (fleet.destination || fleet.targetFleet || !fleet.waypointQueue.empty()
        || (route && !routeIsClearIntent(fleet, *route))) {
        statusBar()->showMessage("Assign work to the final waypoint, or stop travel before starting work here.", 5000);
        return false;
    }
    if (action.kind == FleetArrivalActionKind::RemoteMining) {
        const auto* star = findStarAtPosition(state_, fleet.position);
        const auto* planet = star ? find_planet_at_star(state_, star->id) : nullptr;
        const auto owner = planet ? known_planet_owner(state_, fleet.owner, planet->id) : std::nullopt;
        if (!owner || *owner != 0) {
            statusBar()->showMessage("Remote Mining requires a reported unowned planet at the fleet's position.", 5000);
            return false;
        }
    }
    return appendRouteWaypoint(fleet.position, 0, std::max<std::uint8_t>(1, fleet.warp), action);
}

bool MainWindow::appendStargateRouteStep(PlanetId source, PlanetId destination, std::uint8_t warp, bool resonant)
{
    const auto* selected = selectedFleet();
    if (!selected || selected->owner != pendingOrders_.player || source == destination) return false;
    const auto known = make_player_view(state_,pendingOrders_.player).state;
    if (!stargate_station_available(known,pendingOrders_.player,source)
        || !stargate_station_available(known,pendingOrders_.player,destination)) return false;
    const auto* planet = find_planet(known,source);
    const auto* star = planet ? find_star(known,planet->star) : nullptr;
    if (!star) return false;
    FleetArrivalAction action{FleetArrivalActionKind::TransitStargate};
    action.gateSource = source; action.gateDestination = destination; action.gateResonant = resonant;
    return appendRouteWaypoint(star->position,0,warp,action);
}

void MainWindow::openStargateRouteDialog()
{
    const auto* selected = selectedFleet();
    if (!selected || selected->owner != pendingOrders_.player) return;
    const auto known = make_player_view(state_,pendingOrders_.player).state;
    QDialog dialog(this); dialog.setObjectName("stargateRouteDialog");
    dialog.setWindowTitle("Add stargate route step");
    auto* layout = new QVBoxLayout(&dialog);
    auto* help = new QLabel("Approach the source using the selected Warp, then spend a separate year on the gate hop. "
        "The delivered onboard programme waits and retries once per year if the gates, cargo or payment are unavailable. "
        "Later waypoints stay blocked. Stop or a replacement takes effect after command delivery.",&dialog);
    help->setWordWrap(true); layout->addWidget(help);
    auto* source = new QComboBox(&dialog); source->setObjectName("routeGateSource");
    auto* destination = new QComboBox(&dialog); destination->setObjectName("routeGateDestination");
    for (const auto& planet : known.planets) if (stargate_station_available(known,pendingOrders_.player,planet.id)) {
        source->addItem(QString::fromStdString(planet.name),uint(planet.id));
        destination->addItem(QString::fromStdString(planet.name),uint(planet.id));
    }
    if (destination->count()>1) destination->setCurrentIndex(1);
    auto* form = new QFormLayout;
    form->addRow("Source gate",source); form->addRow("Destination gate",destination); layout->addLayout(form);
    auto* resonant = new QCheckBox("Request resonant pulse: 8 source xenocrystals on successful extended-range transit",&dialog);
    resonant->setObjectName("routeGateResonant");
    resonant->setEnabled(resonant_gate_technology_available(known,pendingOrders_.player)); layout->addWidget(resonant);
    auto* status = new QLabel(&dialog); status->setObjectName("routeGateEstimate"); status->setWordWrap(true); layout->addWidget(status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog); layout->addWidget(buttons);
    const auto refresh = [&] {
        const auto from = source->currentData().toUInt(), to = destination->currentData().toUInt();
        buttons->button(QDialogButtonBox::Ok)->setEnabled(from && to && from!=to);
        status->setText("Uses received gates and fleet telemetry. Arrival mass, source stocks and physical gate support are rechecked locally; "
            "no future supplies or successful transit are guaranteed. Command and report delivery use normal communications.");
    };
    connect(source,&QComboBox::currentIndexChanged,&dialog,refresh);
    connect(destination,&QComboBox::currentIndexChanged,&dialog,refresh); refresh();
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()==QDialog::Accepted && source->currentData()!=destination->currentData())
        appendStargateRouteStep(source->currentData().toUInt(),destination->currentData().toUInt(),
            selectedFleetSuggestedWarpForRouteProgram(),resonant->isChecked());
}

bool MainWindow::appendCoordinateWaypoint(Position destination, std::uint8_t warp, FleetArrivalAction arrivalAction)
{
    if (!std::isfinite(destination.x) || !std::isfinite(destination.y)) return false;
    switch (arrivalAction.kind) {
    case FleetArrivalActionKind::LoadAllAvailable:
    case FleetArrivalActionKind::UnloadAll:
    case FleetArrivalActionKind::Refuel:
    case FleetArrivalActionKind::Colonize:
    case FleetArrivalActionKind::RemoteMining:
    case FleetArrivalActionKind::BombardColony:
    case FleetArrivalActionKind::TransitStargate:
    case FleetArrivalActionKind::LoadMinerals:
    case FleetArrivalActionKind::UnloadTransportLine:
    case FleetArrivalActionKind::UnloadTransportStop:
        statusBar()->showMessage("Choose a system destination for this arrival action", 4000);
        return false;
    case FleetArrivalActionKind::MergeWithFleet:
        statusBar()->showMessage("Merge with fleet requires a moving fleet target", 4000);
        return false;
    default: break;
    }
    return appendRouteWaypoint(destination, 0, warp, arrivalAction);
}

bool MainWindow::appendSelectedStarWaypoint(std::uint8_t warp, FleetArrivalAction arrivalAction, bool compareRoutes)
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    const auto* star = selectedStar();
    if (!fleet || !star || !fleet_warp_valid(state_, *fleet, warp)) {
        statusBar()->showMessage("Select a fleet and destination star with a valid Warp first");
        return false;
    }
    if (arrivalAction.kind == FleetArrivalActionKind::MergeWithFleet) {
        statusBar()->showMessage("Merge with fleet requires a moving fleet target", 3000);
        return false;
    }

    if (arrivalAction.kind == FleetArrivalActionKind::Colonize && !fleet_can_colonize(state_, *fleet)) {
        statusBar()->showMessage("Selected ship design has no colonization module", 3000);
        return false;
    }
    if (arrivalAction.kind == FleetArrivalActionKind::Refuel) {
        const auto* planet = find_planet_at_star(state_, star->id);
        if (!planet || !colony_has_orbital_service(
                state_, planet->id, fleet->owner, OrbitalStationModule::RefuelingDepot)) {
            statusBar()->showMessage(
                "Refuel on arrival requires a friendly orbital refueling depot", 3000);
            return false;
        }
    }
    if (arrivalAction.kind == FleetArrivalActionKind::Colonize) {
        const auto* planet = find_planet_at_star(state_, star->id);
        if (planet && survey_level(state_, fleet->owner, star->id) >= SurveyLevel::OrbitalSurvey
            && planet->owner != 0) {
            statusBar()->showMessage("Selected destination has no unowned world to colonize", 3000);
            return false;
        }
    }
    if (arrivalAction.kind == FleetArrivalActionKind::RemoteMining) {
        if (!fleet_can_remote_mine(state_, *fleet)) {
            statusBar()->showMessage("Remote Mining requires a Remote Miner hull with mining equipment", 3000);
            return false;
        }
        const auto* planet = find_planet_at_star(state_, star->id);
        if (!planet || planet->owner != 0) {
            statusBar()->showMessage("Remote Mining requires an uncolonized destination world", 3000);
            return false;
        }
    }

    if (arrivalAction.kind == FleetArrivalActionKind::UnloadAll
        && (arrivalAction.cargo == FleetCargoKind::Colonists
            || arrivalAction.cargo == FleetCargoKind::All)) {
        const auto* planet = find_planet_at_star(state_, star->id);
        const auto knownOwner = planet
            ? known_planet_owner(state_, fleet->owner, planet->id)
            : std::optional<PlayerId>{};
        if (knownOwner && *knownOwner != 0 && *knownOwner != fleet->owner) {
            const auto answer = QMessageBox::warning(
                this,
                "Queue ground invasion",
                "Unloading colonists at this enemy colony will commit them to ground combat. "
                "The result is probabilistic and the entire landing force leaves the fleet. Continue?",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes) return false;
        }
    }

    if (arrivalAction.kind == FleetArrivalActionKind::Colonize) {
        const auto* planet = find_planet_at_star(state_, star->id);
        Planet unknown;
        unknown.star = star->id;
        unknown.name = "world at " + star->name;
        const bool known = planet && survey_level(state_, fleet->owner, star->id) >= SurveyLevel::BasicScan;
        if (!confirmFleetColonization(*fleet, known ? *planet : unknown, true)) return false;
    }

    if (!compareRoutes) return appendRouteWaypoint(star->position, 0, warp, arrivalAction);
    auto departure=*fleet;
    if (const auto route=effectiveRoute(state_,pendingOrders_,*fleet); route && !routeIsClearIntent(*fleet,*route)) {
        if (route->targetFleet || std::any_of(route->queuedWaypoints.begin(),route->queuedWaypoints.end(),
            [](const auto& p) { return p.targetFleet!=0; })) {
            statusBar()->showMessage("Compare a static route before adding moving fleet targets.",4000); return false;
        }
        departure.position=route->queuedWaypoints.empty()?route->destination:route->queuedWaypoints.back().destination;
        const auto& last=route->queuedWaypoints.empty()?route->arrivalAction:route->queuedWaypoints.back().arrivalAction;
        if(last.kind==FleetArrivalActionKind::TransitStargate) {
            const auto* p=find_planet(state_,last.gateDestination);
            if(const auto* exit=p?find_star(state_,p->star):nullptr) departure.position=exit->position;
        }
    }
    const auto alternatives=medium_route_alternatives(state_,departure,star->position,warp);
    if (alternatives.empty()) return false;
    QStringList choices;
    for (std::size_t i=0;i<alternatives.size();++i) {
        const auto& a=alternatives[i];
        choices << QString("%1: %2 ly, ~%3 years, ~%4 mg fuel, +%5% potential hull damage")
            .arg(i==0?QString("Direct"):QString("Bypass %1").arg(static_cast<qulonglong>(i)))
            .arg(a.distance,0,'f',1).arg(a.years,0,'f',1).arg(a.fuel,0,'f',1).arg(a.damage,0,'f',2);
    }
    bool accepted=false;
    const auto choice=QInputDialog::getItem(this,"Route alternatives",
        "Append to the selected star from the end of the current route.\n"
        "Received cloud profiles only; unknown density can change the result.\n"
        "Coarse travel time and gross fuel exclude command delay, per-year rounding, refuelling and prior legs.\n"
        "Travel time is geometric only; actual movement and arrival actions resolve on End Turn.",choices,0,false,&accepted);
    if (!accepted) return false;
    const auto index=choices.indexOf(choice);
    if (index<0 || !appendRouteWaypoint(star->position,0,warp,arrivalAction)) return false;
    const auto& positions=alternatives[static_cast<std::size_t>(index)].destinations;
    if (positions.size()==1) return true;
    auto route=effectiveRoute(state_,pendingOrders_,*fleet);
    if (!route) return false;
    auto legs=routeLegs(*route);
    const auto final=legs.back(); legs.pop_back();
    for (std::size_t i=0;i+1<positions.size();++i) legs.push_back({positions[i],warp,{}});
    legs.push_back(final);
    route->destination=legs[0].destination; route->warp=legs[0].warp;
    route->arrivalAction=legs[0].arrivalAction; route->targetFleet=legs[0].targetFleet;
    route->queuedWaypoints.assign(legs.begin()+1,legs.end());
    return stageSelectedFleetRoute(*fleet,*route,"Selected route appended for End Turn");
}

bool MainWindow::appendFleetTargetWaypoint(
    FleetId targetFleetId, std::uint8_t warp, FleetArrivalAction arrivalAction)
{
    const auto* source = selectedFleet();
    const auto target = knownFleetTarget(targetFleetId);
    if (!source || !target || source->id == target->id
        || !fleet_warp_valid(state_, *source, warp)) {
        statusBar()->showMessage("Select another fleet as the target", 3000);
        return false;
    }
    if (source->owner != target->owner) {
        if (arrivalAction.kind != FleetArrivalActionKind::None) {
            statusBar()->showMessage(
                "An enemy fleet target uses No action and its currently observed position", 3000);
            return false;
        }
        return appendRouteWaypoint(
            target->position, 0, warp, arrivalAction);
    }
    if (arrivalAction.kind != FleetArrivalActionKind::None
        && arrivalAction.kind != FleetArrivalActionKind::MergeWithFleet
        && arrivalAction.kind != FleetArrivalActionKind::SupplyOutpost) {
        statusBar()->showMessage("A fleet target supports No action, Merge or Transfer fuel", 3000);
        return false;
    }
    return appendRouteWaypoint(target->position, target->id, warp, arrivalAction);
}

bool MainWindow::appendRouteWaypoint(
    Position destination,
    FleetId targetFleet,
    std::uint8_t warp,
    FleetArrivalAction arrivalAction)
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet || !fleet_warp_valid(state_, *fleet, warp)) return false;
    if (const auto error = fleet_work_order_error(state_, *fleet, destination, arrivalAction.kind); !error.empty()) {
        statusBar()->showMessage(QString::fromStdString(error), 5000);
        return false;
    }

    if (const auto existing = effectiveRoute(state_, pendingOrders_, *fleet)) {
        const auto& finalAction = existing->queuedWaypoints.empty()
            ? existing->arrivalAction
            : existing->queuedWaypoints.back().arrivalAction;
        if (terminal_arrival_action(finalAction.kind)) {
            statusBar()->showMessage("The current terminal action must be cleared before adding another waypoint", 3000);
            return false;
        }
        if (existing->repeatOrders
            && (terminal_arrival_action(arrivalAction.kind))) {
            statusBar()->showMessage("Disable Repeat Orders before adding a terminal action", 3000);
            return false;
        }
    }

    if (auto* move = pendingMove(pendingOrders_, fleet->id)) {
        if (routeIsClearIntent(*fleet, *move)) {
            move->clearRoute = false;
            move->destination = destination;
            move->targetFleet = targetFleet;
            move->warp = warp;
            move->arrivalAction = arrivalAction;
            move->queuedWaypoints.clear();
        } else {
            move->queuedWaypoints.push_back({destination, warp, arrivalAction, targetFleet});
        }

        for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
            if (const auto* candidate = std::get_if<MoveFleetOrder>(&pendingOrders_.orders[index]);
                candidate && candidate->fleet == fleet->id) {
                pendingDescriptions_[static_cast<int>(index)] = routeDescription(state_, *fleet, *candidate);
                break;
            }
        }
        rebuildScene();
        statusBar()->showMessage("Waypoint appended to pending fleet program");
        return true;
    }

    if (auto existing = effectiveRoute(state_,pendingOrders_,*fleet); existing && !routeIsClearIntent(*fleet,*existing)) {
        // The latest sent programme remains editable while its command travels
        // to an idle remote fleet, including an unacknowledged gate step.
        existing->queuedWaypoints.push_back({destination,warp,arrivalAction,targetFleet});
        appendPendingOrder(*existing,routeDescription(state_,*fleet,*existing));
        return true;
    }

    MoveFleetOrder route{fleet->id, destination, warp, arrivalAction, {}, false, targetFleet};
    appendPendingOrder(route, routeDescription(state_, *fleet, route));
    return true;
}

bool MainWindow::stageSelectedFleetRoute(
    const Fleet& fleet, MoveFleetOrder route, const QString& statusMessage)
{
    if (auto* pending = pendingMove(pendingOrders_, fleet.id)) {
        *pending = std::move(route);
        for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
            const auto* candidate = std::get_if<MoveFleetOrder>(&pendingOrders_.orders[index]);
            if (!candidate || candidate->fleet != fleet.id) continue;
            if (index < static_cast<std::size_t>(pendingDescriptions_.size())) {
                pendingDescriptions_[static_cast<int>(index)] =
                    routeDescription(state_, fleet, *candidate);
            }
            break;
        }
        rebuildScene();
    } else {
        appendPendingOrder(route, routeDescription(state_, fleet, route));
    }
    statusBar()->showMessage(statusMessage, 2500);
    return true;
}

bool MainWindow::moveSelectedFleetRouteProgramLeg(std::size_t index, int direction)
{
    const auto* sourceFleet = selectedFleet();
    const auto fleetStorage = sourceFleet ? std::optional<Fleet>{fleet_player_view(state_, *sourceFleet)} : std::nullopt;
    const auto* fleet = fleetStorage ? &*fleetStorage : nullptr;
    if (!fleet || direction == 0) return false;
    auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) return false;
    auto legs = routeLegs(*route);
    const auto target = static_cast<std::ptrdiff_t>(index) + direction;
    if (index >= legs.size() || target < 0
        || target >= static_cast<std::ptrdiff_t>(legs.size())) return false;

    std::swap(legs[index], legs[static_cast<std::size_t>(target)]);
    const auto terminal = [](const FleetArrivalAction& action) {
        return terminal_arrival_action(action.kind);
    };
    if (std::any_of(legs.begin(), std::prev(legs.end()), [&](const FleetWaypoint& leg) {
            return terminal(leg.arrivalAction);
        })) {
        statusBar()->showMessage("Persistent work and other terminal actions must remain the final route point", 3000);
        return false;
    }

    route->destination = legs.front().destination;
    route->warp = legs.front().warp;
    route->arrivalAction = legs.front().arrivalAction;
    route->targetFleet = legs.front().targetFleet;
    route->queuedWaypoints.assign(std::next(legs.begin()), legs.end());
    return stageSelectedFleetRoute(*fleet, *route, "Fleet route point moved");
}

std::optional<FleetArrivalAction> MainWindow::selectedFleetRouteProgramLegAction(std::size_t index) const
{
    const auto* source = selectedFleet();
    if (!source) return std::nullopt;
    const auto fleet = fleet_player_view(state_, *source);
    const auto route = effectiveRoute(state_, pendingOrders_, fleet);
    if (!route || routeIsClearIntent(fleet, *route)) return std::nullopt;
    if (index == 0) return route->arrivalAction;
    if (index > route->queuedWaypoints.size()) return std::nullopt;
    return route->queuedWaypoints[index - 1].arrivalAction;
}

bool MainWindow::setSelectedFleetRouteProgramLegAction(std::size_t index, FleetArrivalAction action)
{
    const auto* source = selectedFleet();
    if (!source || source->owner != pendingOrders_.player) return false;
    const auto fleet = fleet_player_view(state_, *source);
    auto route = effectiveRoute(state_, pendingOrders_, fleet);
    if (!route || routeIsClearIntent(fleet, *route)) return false;
    auto legs = routeLegs(*route);
    if (index >= legs.size()) return false;
    auto& leg = legs[index];
    if (leg.arrivalAction == action) return true;
    if (action.colonyPlan && (action.kind != FleetArrivalActionKind::Colonize
        || !production_template_valid(*action.colonyPlan))) return false;

    const bool terminal = terminal_arrival_action(action.kind);
    if (terminal && (index + 1 != legs.size() || route->repeatOrders)) {
        statusBar()->showMessage("Colonize and other terminal actions require a final, non-repeating waypoint", 5000);
        return false;
    }
    if (leg.targetFleet != 0) {
        if (action.kind != FleetArrivalActionKind::None
            && action.kind != FleetArrivalActionKind::MergeWithFleet
            && action.kind != FleetArrivalActionKind::SupplyOutpost) {
            statusBar()->showMessage("Fleet targets support No action, Merge or Transfer fuel", 4000);
            return false;
        }
    } else if (action.kind == FleetArrivalActionKind::MergeWithFleet) {
        return false;
    }

    if (const auto error = fleet_work_order_error(state_, fleet, leg.destination, action.kind); !error.empty()) {
        statusBar()->showMessage(QString::fromStdString(error), 5000);
        return false;
    }
    const auto* star = leg.targetFleet == 0 ? findStarAtPosition(state_, leg.destination) : nullptr;
    const auto* planet = star ? find_planet_at_star(state_, star->id) : nullptr;
    if (action.kind == FleetArrivalActionKind::Colonize) {
        if (!star || !fleet_can_colonize(state_, fleet)
            || (planet && survey_level(state_, fleet.owner, star->id) >= SurveyLevel::OrbitalSurvey
                && known_planet_owner(state_, fleet.owner, planet->id).value_or(0) != 0)) {
            statusBar()->showMessage("Colonize needs a colony ship and an unowned destination", 4000);
            return false;
        }
        Planet unknown;
        unknown.star = star->id;
        unknown.name = "world at " + star->name;
        if (leg.arrivalAction.kind != FleetArrivalActionKind::Colonize && !confirmFleetColonization(fleet,
                planet && survey_level(state_, fleet.owner, star->id) >= SurveyLevel::BasicScan
                    ? *planet : unknown, true)) return false;
    }
    if (action.kind == FleetArrivalActionKind::Refuel
        && (!planet || !colony_has_orbital_service(
            state_, planet->id, fleet.owner, OrbitalStationModule::RefuelingDepot))) {
        statusBar()->showMessage("Refuel requires a friendly orbital depot at the waypoint", 4000);
        return false;
    }
    if (action.kind == FleetArrivalActionKind::RemoteMining
        && (!planet || planet->owner != 0 || !fleet_can_remote_mine(state_, fleet))) {
        statusBar()->showMessage("Remote Mining requires an unowned planet and mining fleet", 4000);
        return false;
    }
    if (action.kind == FleetArrivalActionKind::EnterWormhole) {
        statusBar()->showMessage("Use the wormhole navigator for this action", 4000);
        return false;
    }
    if (action.kind == FleetArrivalActionKind::UnloadAll
        && (action.cargo == FleetCargoKind::Colonists || action.cargo == FleetCargoKind::All)
        && planet) {
        const auto known = known_planet_owner(state_, fleet.owner, planet->id);
        if (known && *known != 0 && *known != fleet.owner) {
            const auto answer = QMessageBox::warning(this, "Queue ground invasion",
                "Unloading colonists at this enemy colony attempts a ground invasion. Continue?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) return false;
        }
    }
    if (action.kind == FleetArrivalActionKind::TransitStargate || action.kind == FleetArrivalActionKind::LoadMinerals
        || action.kind == FleetArrivalActionKind::UnloadTransportLine || action.kind == FleetArrivalActionKind::UnloadTransportStop) {
        if (leg.arrivalAction.kind != action.kind) return false;
        action = leg.arrivalAction;
    }
    leg.arrivalAction = action;
    route->destination = legs.front().destination;
    route->warp = legs.front().warp;
    route->arrivalAction = legs.front().arrivalAction;
    route->targetFleet = legs.front().targetFleet;
    route->queuedWaypoints.assign(std::next(legs.begin()), legs.end());
    return stageSelectedFleetRoute(fleet, *route, "Waypoint arrival action updated for End Turn");
}

bool MainWindow::setSelectedFleetRouteProgramLegWarp(std::size_t index, std::uint8_t warp)
{
    const auto* source = selectedFleet();
    if (!source || warp < 1 || warp > kMaxWarp) return false;
    const auto fleet = fleet_player_view(state_, *source);
    if (fleet_max_warp(state_, fleet) == 0) return false;
    auto route = effectiveRoute(state_, pendingOrders_, fleet);
    if (!route || routeIsClearIntent(fleet, *route)) return false;
    if (index == 0) {
        if (route->warp == warp) return true;
        route->warp = warp;
    } else {
        if (index > route->queuedWaypoints.size()) return false;
        auto& leg = route->queuedWaypoints[index - 1];
        if (leg.warp == warp) return true;
        leg.warp = warp;
    }
    return stageSelectedFleetRoute(fleet, *route, "Selected route leg Warp updated for End Turn");
}

bool MainWindow::removeSelectedFleetRouteProgramLeg(std::size_t index)
{
    const auto* sourceFleet = selectedFleet();
    const auto fleetStorage = sourceFleet ? std::optional<Fleet>{fleet_player_view(state_, *sourceFleet)} : std::nullopt;
    const auto* fleet = fleetStorage ? &*fleetStorage : nullptr;
    if (!fleet) return false;
    auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) return false;
    auto legs = routeLegs(*route);
    if (index >= legs.size()) return false;
    legs.erase(legs.begin() + static_cast<std::ptrdiff_t>(index));
    if (legs.empty()) return clearSelectedFleetRouteProgram();

    route->destination = legs.front().destination;
    route->warp = legs.front().warp;
    route->arrivalAction = legs.front().arrivalAction;
    route->targetFleet = legs.front().targetFleet;
    route->queuedWaypoints.assign(std::next(legs.begin()), legs.end());
    if (std::none_of(route->queuedWaypoints.begin(), route->queuedWaypoints.end(),
            [&](const FleetWaypoint& leg) {
                return leg.targetFleet != route->targetFleet
                    || !same_position(leg.destination, route->destination);
            })) route->repeatOrders = false;
    return stageSelectedFleetRoute(*fleet, *route, "Fleet route point removed");
}

bool MainWindow::setSelectedFleetRepeatOrdersForRouteProgram(bool enabled)
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) return false;

    auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) {
        statusBar()->showMessage("Add at least two route points before enabling Repeat Orders", 3000);
        return false;
    }
    if (enabled) {
        if (route->queuedWaypoints.empty()) {
            statusBar()->showMessage("Repeat Orders requires at least two route points", 3000);
            return false;
        }
        const auto incompatible = [](const FleetArrivalAction& action) {
            return terminal_arrival_action(action.kind);
        };
        if (incompatible(route->arrivalAction)
            || std::any_of(route->queuedWaypoints.begin(), route->queuedWaypoints.end(),
                [&](const FleetWaypoint& waypoint) { return incompatible(waypoint.arrivalAction); })) {
            statusBar()->showMessage("Persistent work and other terminal actions cannot be repeated", 3000);
            return false;
        }
        if (std::none_of(route->queuedWaypoints.begin(), route->queuedWaypoints.end(),
                [&](const FleetWaypoint& waypoint) {
                    return route->targetFleet != waypoint.targetFleet
                        || !same_position(route->destination, waypoint.destination);
                })) {
            statusBar()->showMessage("Repeat Orders requires two different destinations", 3000);
            return false;
        }
    }

    route->repeatOrders = enabled;
    if (auto* pending = pendingMove(pendingOrders_, fleet->id)) {
        *pending = *route;
        for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
            if (const auto* candidate = std::get_if<MoveFleetOrder>(&pendingOrders_.orders[index]);
                candidate && candidate->fleet == fleet->id) {
                pendingDescriptions_[static_cast<int>(index)] = routeDescription(state_, *fleet, *route);
                break;
            }
        }
        rebuildScene();
    } else {
        appendPendingOrder(*route, routeDescription(state_, *fleet, *route));
    }
    statusBar()->showMessage(enabled ? "Repeat Orders enabled" : "Repeat Orders disabled");
    return true;
}

bool MainWindow::clearSelectedFleetRouteProgram()
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) {
        statusBar()->showMessage("Select a fleet first");
        return false;
    }

    MoveFleetOrder clear{
        fleet->id,
        fleet->position,
        std::max<std::uint8_t>(1, fleet->warp),
        {},
        {},
    };

    clear.clearRoute = true;
    for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
        if (const auto* move = std::get_if<MoveFleetOrder>(&pendingOrders_.orders[index]);
            move && move->fleet == fleet->id) {
            pendingOrders_.orders[index] = clear;
            pendingDescriptions_[static_cast<int>(index)] = routeDescription(state_, *fleet, clear);
            rebuildScene();
            statusBar()->showMessage("Fleet route program cleared for End Turn");
            return true;
        }
    }

    appendPendingOrder(clear, routeDescription(state_, *fleet, clear));
    statusBar()->showMessage("Stop queued; the fleet cancels its program when the command arrives", 4000);
    return true;
}

} // namespace suns
