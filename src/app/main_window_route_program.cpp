#include "main_window.hpp"

#include "suns/communications.hpp"

#include <QGraphicsView>
#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <iterator>
#include <utility>

namespace suns {

namespace {

Fleet* findFleet(GameState& state, FleetId id)
{
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
    return it == state.fleets.end() ? nullptr : &*it;
}

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
    return QString("(%1, %2)").arg(position.x, 0, 'f', 0).arg(position.y, 0, 'f', 0);
}

QString actionName(const FleetArrivalAction& action)
{
    const auto cargoName = [&] {
        switch (action.cargo) {
        case FleetCargoKind::Colonists: return QString("colonists");
        case FleetCargoKind::Ironium: return QString("Ironium");
        case FleetCargoKind::Boranium: return QString("Boranium");
        case FleetCargoKind::Germanium: return QString("Germanium");
        case FleetCargoKind::All: return QString("cargo");
        }
        return QString("cargo");
    };
    switch (action.kind) {
    case FleetArrivalActionKind::None:
        return "no action";
    case FleetArrivalActionKind::LoadAllAvailable:
        return action.cargo == FleetCargoKind::Colonists
            ? QString("Load all %1, leave %2 — dynamic")
                  .arg(cargoName())
                  .arg(static_cast<qulonglong>(action.reservePopulation))
            : QString("Load all available %1 — dynamic").arg(cargoName());
    case FleetArrivalActionKind::UnloadAll:
        return QString("Unload all %1 — dynamic").arg(cargoName());
    case FleetArrivalActionKind::Refuel:
        return "Refuel on arrival";
    case FleetArrivalActionKind::Colonize:
        return "Colonize world — dismantles entire fleet";
    case FleetArrivalActionKind::RemoteMining:
        return "Remote Mining — persistent";
    case FleetArrivalActionKind::MergeWithFleet:
        return "Merge with fleet — terminal";
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

std::optional<MoveFleetOrder> effectiveRoute(const GameState&, const PlayerOrders& pending, const Fleet& fleet)
{
    if (const auto* move = pendingMove(pending, fleet.id)) return *move;
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

QString routeForecast(
    const GameState& state,
    const PlayerOrders& pending,
    const TurnProcessor& processor,
    FleetId fleetId,
    const MoveFleetOrder& route,
    std::vector<std::optional<std::uint32_t>>* arrivalTurns = nullptr,
    QString* fuelWarning = nullptr)
{
    constexpr std::uint32_t kForecastHorizon = 96;
    const auto legs = routeLegs(route);
    if (legs.empty()) return {};
    if (arrivalTurns) arrivalTurns->assign(legs.size(), std::nullopt);
    if (fuelWarning) fuelWarning->clear();

    GameState simulated = state;
    if (auto* simulatedFleet = findFleet(simulated, fleetId)) {
        *simulatedFleet = fleet_player_view(state, *simulatedFleet);
    }
    QStringList lines;
    lines << "<br><b>Forecast if no further orders are issued:</b>";

    bool firstTurn = true;
    bool dependsOnDynamicResult = false;
    std::size_t legIndex = 0;

    for (std::uint32_t step = 1; step <= kForecastHorizon && legIndex < legs.size(); ++step) {
        const auto* beforeFleet = findFleet(simulated, fleetId);
        if (!beforeFleet) {
            lines << "Fleet no longer exists in the projected state.";
            break;
        }
        const auto fleetOwner = beforeFleet->owner;
        PlayerId targetOwnerBeforeTurn{};
        if (const auto* targetStar = findStarAtPosition(simulated, legs[legIndex].destination)) {
            if (const auto* targetPlanet = find_planet_at_star(simulated, targetStar->id)) {
                targetOwnerBeforeTurn = targetPlanet->owner;
            }
        }

        std::vector<PlayerOrders> submissions;
        if (firstTurn && !pending.orders.empty()) submissions.push_back(pending);
        firstTurn = false;

        auto next = processor.process(simulated, submissions);
        const auto* after = findFleet(next, fleetId);
        const auto& leg = legs[legIndex];
        if (!after) {
            if (leg.arrivalAction.kind == FleetArrivalActionKind::MergeWithFleet
                && findFleet(next, leg.targetFleet)) {
                lines << QString("%1. %2 — intercept T+%3, W%4 — <b>rendezvous completed; fleets merged</b>")
                             .arg(static_cast<qulonglong>(legIndex + 1))
                             .arg(waypointName(state, leg.destination, leg.targetFleet))
                             .arg(step)
                             .arg(leg.warp);
                if (arrivalTurns) (*arrivalTurns)[legIndex] = step;
                legIndex = legs.size();
                simulated = std::move(next);
                break;
            }
            if (leg.arrivalAction.kind == FleetArrivalActionKind::Colonize) {
                const auto* targetStar = findStarAtPosition(next, leg.destination);
                const auto* targetPlanet = targetStar ? find_planet_at_star(next, targetStar->id) : nullptr;
                if (targetPlanet && targetPlanet->owner == fleetOwner) {
                    lines << QString("%1. %2 — T+%3, W%4 — <b>colonized successfully; fleet dismantled</b>")
                                 .arg(static_cast<qulonglong>(legIndex + 1))
                                 .arg(waypointName(state, leg.destination, leg.targetFleet))
                                 .arg(step)
                                 .arg(leg.warp);
                    if (legIndex + 1 < legs.size()) {
                        lines << "<i>Later waypoints cannot execute because successful colonization dismantles the fleet.</i>";
                    }
                    if (arrivalTurns) (*arrivalTurns)[legIndex] = step;
                    legIndex = legs.size();
                    simulated = std::move(next);
                    break;
                }
            }
            lines << QString("%1. fleet removed before completing the route")
                         .arg(static_cast<qulonglong>(legIndex + 1));
            simulated = std::move(next);
            break;
        }

        if (after->damagePercent >= 100.0) {
            lines << QString("<b>T+%1: critical hull damage (100%); fleet immobilized before completing its program.</b>")
                         .arg(step);
            return lines.join("<br>");
        }

        if (fuelWarning && after->destination
            && (after->fuelStalled || (after->fuel <= 0.000001
                && fleet_fuel_change_for_distance(next, *after, 1.0) > 0.000001
                && distance_between(beforeFleet->position, after->position) + 0.000001 < warp_distance(after->warp)
                && same_position(*after->destination, leg.destination)))) {
            *fuelWarning = QString("Fuel shortage forecast at T+%1, before waypoint %2 (%3). "
                "This preview includes earlier legs, cargo changes, fuel generation and orbital refuelling. "
                "Reduce Warp or add a refuelling stop.")
                .arg(step).arg(static_cast<qulonglong>(legIndex + 1))
                .arg(waypointName(state, leg.destination, leg.targetFleet));
            break;
        }

        const auto remainingLegs = (after->destination ? std::size_t{1} : std::size_t{0})
            + after->waypointQueue.size();
        const auto expectedRemaining = legs.size() - legIndex - 1;
        const auto expectedAfterRepeatRestart = route.repeatOrders && legIndex + 1 == legs.size()
            ? after->routeTemplate.size()
            : expectedRemaining;
        const auto* movingTarget = findFleet(next, leg.targetFleet);
        const auto arrivalPosition = movingTarget ? movingTarget->position : leg.destination;
        const bool arrived = same_position(after->position, arrivalPosition)
            && remainingLegs == expectedAfterRepeatRestart;

        if (arrived) {
            if (arrivalTurns) (*arrivalTurns)[legIndex] = step;
            const auto navigationCertainty = leg.targetFleet != 0
                ? (dependsOnDynamicResult ? "projected continuous intercept" : "continuous intercept")
                : (dependsOnDynamicResult ? "projected navigation" : "exact navigation");
            const auto* arrivalStar = findStarAtPosition(next, arrivalPosition);
            const auto* arrivalPlanet = arrivalStar
                ? find_planet_at_star(next, arrivalStar->id)
                : nullptr;
            QString outcome;
            bool invasionOutcomeUncertain = false;
            switch (leg.arrivalAction.kind) {
            case FleetArrivalActionKind::None:
                if (leg.targetFleet != 0) outcome = "; moving-target rendezvous completed";
                break;
            case FleetArrivalActionKind::LoadAllAvailable:
                if (leg.arrivalAction.cargo == FleetCargoKind::Colonists) {
                    outcome = QString("; projected colonists aboard: %1")
                                  .arg(static_cast<qulonglong>(after->colonists));
                } else if (leg.arrivalAction.cargo != FleetCargoKind::All) {
                    const auto amount = leg.arrivalAction.cargo == FleetCargoKind::Ironium
                        ? after->minerals.ironium
                        : leg.arrivalAction.cargo == FleetCargoKind::Boranium
                            ? after->minerals.boranium
                            : after->minerals.germanium;
                    outcome = QString("; projected mineral cargo aboard: %1").arg(amount, 0, 'f', 1);
                }
                break;
            case FleetArrivalActionKind::UnloadAll:
                if ((leg.arrivalAction.cargo == FleetCargoKind::Colonists
                        || leg.arrivalAction.cargo == FleetCargoKind::All)
                    && targetOwnerBeforeTurn != 0
                    && targetOwnerBeforeTurn != fleetOwner) {
                    outcome = "; ground invasion attempted; outcome hidden because the defending population may change before arrival";
                    invasionOutcomeUncertain = true;
                } else {
                    outcome = leg.arrivalAction.cargo == FleetCargoKind::All
                        ? "; all cargo unloaded to the planetary surface"
                        : "; selected cargo unloaded to the planetary surface";
                }
                break;
            case FleetArrivalActionKind::Refuel:
                outcome = arrivalPlanet
                        && colony_has_orbital_service(
                            next, arrivalPlanet->id, fleetOwner,
                            OrbitalStationModule::RefuelingDepot)
                    ? QString("; projected fuel after refuel: %1").arg(after->fuel, 0, 'f', 1)
                    : "; refueling unavailable — no friendly orbital depot";
                break;
            case FleetArrivalActionKind::Colonize:
                outcome = "; colonization could not be completed; fleet remains";
                break;
            case FleetArrivalActionKind::RemoteMining:
                outcome = after->task == FleetTask::RemoteMining
                    ? "; Remote Mining assigned; extraction begins next turn"
                    : "; Remote Mining could not be assigned";
                break;
            case FleetArrivalActionKind::MergeWithFleet:
                outcome = "; rendezvous still pending";
                break;
            }

            lines << QString("%1. %2 — T+%3, W%4, fuel %5, damage %6%, colonists %7 — <b>%8</b>%9")
                         .arg(static_cast<qulonglong>(legIndex + 1))
                         .arg(waypointName(state, leg.destination, leg.targetFleet))
                         .arg(step)
                         .arg(leg.warp)
                         .arg(after->fuel, 0, 'f', 1)
                         .arg(after->damagePercent, 0, 'f', 1)
                         .arg(static_cast<qulonglong>(after->colonists))
                         .arg(navigationCertainty)
                         .arg(outcome);

            if (leg.arrivalAction.kind == FleetArrivalActionKind::LoadAllAvailable) {
                dependsOnDynamicResult = true;
            }
            ++legIndex;
            if (invasionOutcomeUncertain) {
                lines << "<i>Later route results depend on the ground battle, so the forecast stops here.</i>";
                legIndex = legs.size();
                simulated = std::move(next);
                break;
            }
        }

        simulated = std::move(next);
    }

    if (legIndex < legs.size()) {
        if (legs[legIndex].targetFleet != 0) {
            lines << QString("<i>No intercept is predicted inside the %1-turn preview horizon; pursuit remains active and the ETA is uncertain.</i>")
                         .arg(kForecastHorizon);
        } else {
            lines << QString("<i>Program does not complete inside the %1-turn preview horizon; it may be fuel-limited or very long.</i>")
                         .arg(kForecastHorizon);
        }
    } else if (dependsOnDynamicResult) {
        lines << "<i>Projected legs after Load All depend on the colony state that actually exists on arrival.</i>";
    }
    if (route.repeatOrders && legIndex == legs.size()) {
        lines << "<i>Repeat Orders restarts the complete program after the final arrival.</i>";
    }

    return lines.join("<br>");
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
    return fleet_overdrive_damage_rate(state_, fleet_player_view(state_, *fleet), warp);
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
        const auto task = fleet->task == FleetTask::RemoteMining ? "Remote Mining" : "No Task";
        auto summary = QString("<b>%1 route:</b> none<br><b>Current task:</b> %2")
                           .arg(QString::fromStdString(fleet->name))
                           .arg(task);
        if (route && routeIsClearIntent(*fleet, *route)) {
            summary += "<br><i>Pending: clear route and set No Task when the command arrives.</i>";
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

QString MainWindow::selectedFleetRouteProgramForecast() const
{
    const auto* authoritative = selectedFleet();
    if (!authoritative) return "Select a fleet first.";
    const auto fleet = fleet_player_view(state_, *authoritative);
    const auto route = effectiveRoute(state_, pendingOrders_, fleet);
    if (!route || routeIsClearIntent(fleet, *route)) return "No route to forecast.";
    return routeForecast(state_, pendingOrders_, processor_, fleet.id, *route);
}

QString MainWindow::selectedFleetWaypointFuelWarning(
    std::uint8_t warp, FleetId targetFleet, FleetArrivalAction action) const
{
    const auto key = QString("%1:%2:%3:%4:%5:%6:%7:%8")
        .arg(planningRevision_).arg(selectedFleetForRouteProgram())
        .arg(selection_.star.value_or(0)).arg(warp).arg(targetFleet)
        .arg(static_cast<int>(action.kind)).arg(action.reservePopulation)
        .arg(static_cast<int>(action.cargo));
    if (key == waypointFuelKey_) return waypointFuelWarning_;
    waypointFuelKey_ = key;
    waypointFuelWarning_.clear();
    const auto* source = selectedFleet();
    if (!source) return {};
    const auto fleet = fleet_player_view(state_, *source);
    Position destination;
    if (targetFleet != 0) {
        const auto* target = findFleet(state_, targetFleet);
        if (!target || targetFleet == fleet.id) return {};
        destination = fleet_player_view(state_, *target).position;
        if (target->owner != fleet.owner) targetFleet = 0;
    } else {
        const auto* star = selectedStar();
        if (!star) return {};
        destination = star->position;
    }
    auto route = effectiveRoute(state_, pendingOrders_, fleet);
    if (!route || routeIsClearIntent(fleet, *route)) {
        route = MoveFleetOrder{fleet.id, destination, warp, action, {}, false, targetFleet};
    } else {
        route->queuedWaypoints.push_back({destination, warp, action, targetFleet});
    }
    auto pending = pendingOrders_;
    if (auto* existing = pendingMove(pending, fleet.id)) *existing = *route;
    else pending.orders.push_back(*route);
    (void)routeForecast(state_, pending, processor_, fleet.id, *route, nullptr, &waypointFuelWarning_);
    return waypointFuelWarning_;
}

std::vector<RouteProgramDisplayRow> MainWindow::selectedFleetRouteProgramRows() const
{
    const auto* authoritativeFleet = selectedFleet();
    const auto visibleFleetStorage = authoritativeFleet
        ? std::optional<Fleet>{fleet_player_view(state_, *authoritativeFleet)}
        : std::nullopt;
    const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
    if (!fleet) return {};
    const auto route = effectiveRoute(state_, pendingOrders_, *fleet);
    if (!route || routeIsClearIntent(*fleet, *route)) return {};

    std::vector<RouteProgramDisplayRow> rows;
    const auto legs = routeLegs(*route);
    rows.reserve(legs.size());
    if (routeEtaRevision_ != planningRevision_ || routeEtaFleet_ != fleet->id) {
        (void)routeForecast(state_, pendingOrders_, processor_, fleet->id, *route, &routeEtas_);
        routeEtaRevision_ = planningRevision_;
        routeEtaFleet_ = fleet->id;
    }
    for (std::size_t index = 0; index < legs.size(); ++index) {
        const auto& leg = legs[index];
        rows.push_back({
            waypointName(state_, leg.destination, leg.targetFleet),
            actionName(leg.arrivalAction),
            leg.warp,
            index == 0,
            index < routeEtas_.size() && routeEtas_[index]
                ? QString::number(*routeEtas_[index]) : QString("—"),
        });
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
        if (const auto* targetFleet = findFleet(state_, target)) return targetFleet->position;
        return snapshot;
    };
    points.push_back(resolvedPosition(route->destination, route->targetFleet));
    for (const auto& waypoint : route->queuedWaypoints) {
        points.push_back(resolvedPosition(waypoint.destination, waypoint.targetFleet));
    }
    return points;
}

std::vector<FleetId> MainWindow::availableFleetTargetsForRouteProgram() const
{
    const auto source = selectedFleetForRouteProgram();
    std::vector<FleetId> targets;
    for (const auto& fleet : state_.fleets) {
        if (fleet.id != source) targets.push_back(fleet.id);
    }
    std::sort(targets.begin(), targets.end());
    return targets;
}

std::vector<FleetId> MainWindow::availableOwnedFleetsForRouteProgram() const
{
    std::vector<FleetId> fleets;
    for (const auto& fleet : state_.fleets) {
        if (fleet.owner == pendingOrders_.player) fleets.push_back(fleet.id);
    }
    std::sort(fleets.begin(), fleets.end());
    return fleets;
}

QString MainWindow::fleetTargetNameForRouteProgram(FleetId fleetId) const
{
    if (const auto* fleet = findFleet(state_, fleetId)) {
        return QString("%1 (Fleet %2)").arg(QString::fromStdString(fleet->name)).arg(fleetId);
    }
    return QString("Fleet %1").arg(fleetId);
}

std::vector<RouteProgramTargetOption> MainWindow::routeProgramTargetsAtSelectedSystem() const
{
    const auto* star = selectedStar();
    if (!star) return {};

    std::vector<RouteProgramTargetOption> targets;
    targets.push_back({
        QString("System — %1").arg(QString::fromStdString(star->name)),
        1,
        star->id,
        false,
    });

    const auto source = selectedFleetForRouteProgram();
    for (const auto& fleet : state_.fleets) {
        if (fleet.id == source) continue;
        const auto visible = fleet_player_view(state_, fleet);
        if (!same_position(visible.position, star->position)) continue;
        const bool enemy = fleet.owner != pendingOrders_.player;
        targets.push_back({
            QString(enemy ? "Enemy fleet — %1" : "Fleet — %1")
                .arg(QString::fromStdString(visible.name)),
            enemy ? 3 : 2,
            fleet.id,
            enemy,
        });
    }
    return targets;
}

bool MainWindow::selectFleetForRouteProgram(FleetId fleetId)
{
    const auto* fleet = findFleet(state_, fleetId);
    if (!fleet || fleet->owner != pendingOrders_.player) return false;
    if (selection_.fleet == fleetId) return true;
    warpControlFleetId_.reset();
    logisticsControlFleetId_.reset();
    if (!selectWorkspaceObject(2, fleetId)) return false;
    rebuildScene();
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
    statusBar()->showMessage("Route target mode: click a star or another fleet; Esc cancels");
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
        if (const auto* fleet = findFleet(state_, static_cast<FleetId>(id))) {
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
        const auto* target = findFleet(state_, static_cast<FleetId>(id));
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

bool MainWindow::appendSelectedStarWaypoint(std::uint8_t warp, FleetArrivalAction arrivalAction)
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

    return appendRouteWaypoint(star->position, 0, warp, arrivalAction);
}

bool MainWindow::appendFleetTargetWaypoint(
    FleetId targetFleetId, std::uint8_t warp, FleetArrivalAction arrivalAction)
{
    const auto* source = selectedFleet();
    const auto* target = findFleet(state_, targetFleetId);
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
            fleet_player_view(state_, *target).position, 0, warp, arrivalAction);
    }
    if (arrivalAction.kind != FleetArrivalActionKind::None
        && arrivalAction.kind != FleetArrivalActionKind::MergeWithFleet) {
        statusBar()->showMessage("A moving fleet target supports No action or Merge with fleet", 3000);
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

    if (const auto existing = effectiveRoute(state_, pendingOrders_, *fleet)) {
        const auto& finalAction = existing->queuedWaypoints.empty()
            ? existing->arrivalAction
            : existing->queuedWaypoints.back().arrivalAction;
        if (finalAction.kind == FleetArrivalActionKind::RemoteMining
            || finalAction.kind == FleetArrivalActionKind::MergeWithFleet) {
            statusBar()->showMessage("The current terminal action must be cleared before adding another waypoint", 3000);
            return false;
        }
        if (existing->repeatOrders
            && (arrivalAction.kind == FleetArrivalActionKind::Colonize
                || arrivalAction.kind == FleetArrivalActionKind::RemoteMining
                || arrivalAction.kind == FleetArrivalActionKind::MergeWithFleet)) {
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

    if (fleet->destination) {
        MoveFleetOrder route{
            fleet->id,
            *fleet->destination,
            fleet->warp,
            fleet->arrivalAction.value_or(FleetArrivalAction{}),
            fleet->waypointQueue,
            fleet->repeatOrders,
            fleet->targetFleet,
        };
        route.queuedWaypoints.push_back({destination, warp, arrivalAction, targetFleet});
        appendPendingOrder(route, routeDescription(state_, *fleet, route));
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
        return action.kind == FleetArrivalActionKind::Colonize
            || action.kind == FleetArrivalActionKind::RemoteMining
            || action.kind == FleetArrivalActionKind::MergeWithFleet;
    };
    if (std::any_of(legs.begin(), std::prev(legs.end()), [&](const FleetWaypoint& leg) {
            return terminal(leg.arrivalAction);
        })) {
        statusBar()->showMessage("Colonize, Remote Mining and Merge must remain the final route point", 3000);
        return false;
    }

    route->destination = legs.front().destination;
    route->warp = legs.front().warp;
    route->arrivalAction = legs.front().arrivalAction;
    route->targetFleet = legs.front().targetFleet;
    route->queuedWaypoints.assign(std::next(legs.begin()), legs.end());
    return stageSelectedFleetRoute(*fleet, *route, "Fleet route point moved");
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
            return action.kind == FleetArrivalActionKind::Colonize
                || action.kind == FleetArrivalActionKind::RemoteMining
                || action.kind == FleetArrivalActionKind::MergeWithFleet;
        };
        if (incompatible(route->arrivalAction)
            || std::any_of(route->queuedWaypoints.begin(), route->queuedWaypoints.end(),
                [&](const FleetWaypoint& waypoint) { return incompatible(waypoint.arrivalAction); })) {
            statusBar()->showMessage("Colonize, Remote Mining and fleet merging cannot be repeated", 3000);
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
