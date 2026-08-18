#include "main_window.hpp"

#include <QStatusBar>

#include <algorithm>
#include <cmath>

namespace suns {

namespace {

const Fleet* findFleet(const GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    return it == state.fleets.end() ? nullptr : &*it;
}

const StarSystem* findStarAtPosition(const GameState& state, Position position)
{
    const auto it = std::find_if(state.stars.begin(), state.stars.end(), [&](const StarSystem& star) {
        return same_position(star.position, position);
    });
    return it == state.stars.end() ? nullptr : &*it;
}

QString waypointName(const GameState& state, Position position)
{
    if (const auto* star = findStarAtPosition(state, position)) {
        return QString::fromStdString(star->name);
    }
    return QString("(%1, %2)").arg(position.x, 0, 'f', 0).arg(position.y, 0, 'f', 0);
}

QString actionName(const FleetArrivalAction& action)
{
    switch (action.kind) {
    case FleetArrivalActionKind::None:
        return "no action";
    case FleetArrivalActionKind::LoadColonistsToCapacity:
        return QString("Load All, leave %1 — dynamic")
            .arg(static_cast<qulonglong>(action.reservePopulation));
    case FleetArrivalActionKind::UnloadAllColonists:
        return "Unload All — dynamic";
    case FleetArrivalActionKind::Refuel:
        return "Refuel on arrival";
    }
    return "no action";
}

MoveFleetOrder* pendingMove(PlayerOrders& pending, FleetId fleet)
{
    for (auto& order : pending.orders) {
        if (auto* move = std::get_if<MoveFleetOrder>(&order); move && move->fleet == fleet) return move;
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
    if (const auto* move = pendingMove(pending, fleet.id)) return *move;
    if (!fleet.destination) return std::nullopt;

    return MoveFleetOrder{
        fleet.id,
        *fleet.destination,
        fleet.warp,
        fleet.arrivalAction.value_or(FleetArrivalAction{}),
        fleet.waypointQueue,
    };
}

bool routeIsClearIntent(const Fleet& fleet, const MoveFleetOrder& move)
{
    return same_position(fleet.position, move.destination)
        && move.arrivalAction.kind == FleetArrivalActionKind::None
        && move.queuedWaypoints.empty();
}

QString routeDescription(const GameState& state, const Fleet& fleet, const MoveFleetOrder& route)
{
    if (routeIsClearIntent(fleet, route)) {
        return QString("Clear route program for %1").arg(QString::fromStdString(fleet.name));
    }

    const auto legs = static_cast<qulonglong>(1 + route.queuedWaypoints.size());
    return QString("Program %1 — %2 leg%3, next %4 at W%5")
        .arg(QString::fromStdString(fleet.name))
        .arg(legs)
        .arg(legs == 1 ? "" : "s")
        .arg(waypointName(state, route.destination))
        .arg(route.warp);
}

} // namespace

FleetId MainWindow::selectedFleetForRouteProgram() const
{
    const auto* fleet = selectedFleet();
    return fleet ? fleet->id : 0;
}

std::uint8_t MainWindow::selectedFleetMaxWarpForRouteProgram() const
{
    const auto* fleet = selectedFleet();
    if (!fleet) return 0;
    const auto* design = fleet_design(state_, *fleet);
    return design ? ship_design_max_warp(*design) : 0;
}

std::uint8_t MainWindow::selectedFleetSuggestedWarpForRouteProgram() const
{
    const auto* fleet = selectedFleet();
    if (!fleet) return 1;
    const auto maxWarp = selectedFleetMaxWarpForRouteProgram();
    return maxWarp == 0 ? 1 : std::clamp<std::uint8_t>(fleet->warp, 1, maxWarp);
}

QString MainWindow::selectedFleetRouteProgramSummary() const
{
    const auto* fleet = selectedFleet();
    if (!fleet) return "<b>Route program:</b> select a fleet on the map.";

    const auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) {
        return QString("<b>%1 route:</b> none")
            .arg(QString::fromStdString(fleet->name));
    }

    QStringList lines;
    lines << QString("<b>%1 route — %2 leg%3</b>")
                 .arg(QString::fromStdString(fleet->name))
                 .arg(static_cast<qulonglong>(1 + route->queuedWaypoints.size()))
                 .arg(route->queuedWaypoints.empty() ? "" : "s");

    lines << QString("1. %1 — W%2 — %3 <b>[active]</b>")
                 .arg(waypointName(state_, route->destination))
                 .arg(route->warp)
                 .arg(actionName(route->arrivalAction));

    std::size_t index = 2;
    for (const auto& waypoint : route->queuedWaypoints) {
        lines << QString("%1. %2 — W%3 — %4")
                     .arg(static_cast<qulonglong>(index++))
                     .arg(waypointName(state_, waypoint.destination))
                     .arg(waypoint.warp)
                     .arg(actionName(waypoint.arrivalAction));
    }

    if (pendingMove(pendingOrders_, fleet->id)) {
        lines << "<i>Pending program becomes authoritative on End Turn.</i>";
    }
    return lines.join("<br>");
}

std::vector<Position> MainWindow::selectedFleetRouteProgramPolyline() const
{
    const auto* fleet = selectedFleet();
    if (!fleet) return {};

    std::vector<Position> points;
    points.push_back(fleet->position);

    const auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) return points;

    points.push_back(route->destination);
    for (const auto& waypoint : route->queuedWaypoints) points.push_back(waypoint.destination);
    return points;
}

bool MainWindow::appendSelectedStarWaypoint(std::uint8_t warp, FleetArrivalAction arrivalAction)
{
    const auto* fleet = selectedFleet();
    const auto* star = selectedStar();
    if (!fleet || !star || !fleet_warp_valid(state_, *fleet, warp)) {
        statusBar()->showMessage("Select a fleet and destination star with a valid Warp first");
        return false;
    }

    if (auto* move = pendingMove(pendingOrders_, fleet->id)) {
        if (routeIsClearIntent(*fleet, *move)) {
            move->destination = star->position;
            move->warp = warp;
            move->arrivalAction = arrivalAction;
            move->queuedWaypoints.clear();
        } else {
            move->queuedWaypoints.push_back({star->position, warp, arrivalAction});
        }

        for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
            if (&pendingOrders_.orders[index] == nullptr) continue;
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

    if (fleet->destination) {
        MoveFleetOrder route{
            fleet->id,
            *fleet->destination,
            fleet->warp,
            fleet->arrivalAction.value_or(FleetArrivalAction{}),
            fleet->waypointQueue,
        };
        route.queuedWaypoints.push_back({star->position, warp, arrivalAction});
        appendPendingOrder(route, routeDescription(state_, *fleet, route));
        return true;
    }

    MoveFleetOrder route{fleet->id, star->position, warp, arrivalAction, {}};
    appendPendingOrder(route, routeDescription(state_, *fleet, route));
    return true;
}

bool MainWindow::clearSelectedFleetRouteProgram()
{
    const auto* fleet = selectedFleet();
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

    if (!fleet->destination && fleet->waypointQueue.empty()) {
        statusBar()->showMessage("Selected fleet has no route program");
        return false;
    }

    appendPendingOrder(clear, routeDescription(state_, *fleet, clear));
    return true;
}

} // namespace suns
