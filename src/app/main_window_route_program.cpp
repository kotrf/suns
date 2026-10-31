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
    return QString("(%1, %2)").arg(position.x, 0, 'f', 0).arg(position.y, 0, 'f', 0);
}

QString gateDestinationName(const GameState& state, PlanetId id)
{
    const auto* planet=find_planet(state,id);
    const auto* star=planet?find_star(state,planet->star):nullptr;
    return star?QString::fromStdString(star->name):QString("Gate %1").arg(id);
}

QString actionName(const GameState& state, const FleetArrivalAction& action)
{
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

QString routeForecast(
    const GameState& state,
    const PlayerOrders& pending,
    const TurnProcessor& processor,
    FleetId fleetId,
    const MoveFleetOrder& route,
    std::vector<RouteProgramLegForecast>* legForecasts = nullptr,
    QString* fuelWarning = nullptr)
{
    constexpr std::uint32_t kForecastHorizon = 96;
    const auto legs = routeLegs(route);
    if (legs.empty()) return {};
    if (legForecasts) legForecasts->assign(legs.size(), {});
    if (fuelWarning) fuelWarning->clear();
    if (std::any_of(legs.begin(), legs.end(), [](const auto& leg) {
        return leg.arrivalAction.kind == FleetArrivalActionKind::EnterWormhole;
    })) return "WH approach uses last observed coordinates. Local sensors can reacquire a drifting mouth. "
        "Entry, lifetime, exit and survival cannot be forecast with certainty.";

    // Every route uses received fittings, queues and stocks. Physical hidden
    // station losses must not invalidate a previously delivered programme.
    GameState simulated = make_player_view(state,pending.player).state;
    // Chart-only systems have no received physical planet to service a route.
    // Re-observing every chart entry for 96 simulated years adds no navigation
    // evidence; keep all received planets and their stellar environments.
    std::erase_if(simulated.stars,[&](const auto& star) {
        return std::none_of(simulated.planets.begin(),simulated.planets.end(),[&](const auto& p){return p.star==star.id;});
    });
    simulated.pendingMobileBaseTransfers.clear(); // Host-only undelivered logistics are not planning evidence.
    project_interstellar_medium(simulated,pending.player);
    // Navigation previews must never resolve battles using hidden enemy fits,
    // routes or the authoritative seed. Combat outcomes are not forecasts.
    std::erase_if(simulated.fleets, [&](const auto& f) { return f.owner != pending.player; });
    simulated.minefields.clear(); simulated.wrecks.clear();
    if (const auto* p = find_player(state,pending.player)) for (const auto& i : p->strategicIntel)
        if (i.type == StrategicObjectKind::Minefield && i.quantity > 0)
            simulated.minefields.push_back({i.id,i.owner,i.position,i.quantity,i.kind});

    simulated.wormholes.clear();
    simulated.wormholeRules.spawnChancePerTurn = 0.0;
    // PlayerView already includes last contacts for unconfirmed fleet losses.
    // Appending them again would duplicate their projected mining/logistics.
    for (auto& f : simulated.fleets) f = fleet_player_view(state, f);
    for (auto& f : simulated.fleets) {
        // These are the owner's sent commands, not incoming telemetry or host
        // logistics. Keep their original issue/delivery dates in the forecast.
        if (const auto* original=findFleet(state,f.id); original && original->owner==pending.player)
            f.pendingCommands=original->pendingCommands;
    }
    for (auto& f : simulated.fleets) if (f.task != FleetTask::RemoteMining && f.task != FleetTask::DeployOutpost) f.task = FleetTask::None;
    QStringList lines;
    lines << "<br><b>Forecast if no further orders are issued:</b>";
    lines << "<i>Navigation only: hostile encounters can interrupt this route; combat is not forecast.</i>";
    if (state.interstellarMediumEnabled) lines << "<i>Medium estimate: received cloud profiles only. Undiscovered or approximately detected density can change fuel, coasting and ETA. "
        "Fuel ranges cover density 0–1 along the forecast flown segments; actual arrival/refuelling may change. Density changes engine safe Warp and hull risk; the estimate can miss unknown hazards.</i>";

    bool firstTurn = true;
    bool dependsOnDynamicResult = false;
    std::size_t legIndex = 0;
    bool fuelShortageSeen = false;

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

        auto result = processor.process_with_events(simulated, submissions);
        auto next = std::move(result.state);
        const auto* after = findFleet(next, fleetId);
        const auto& leg = legs[legIndex];
        for (const auto& movement : result.movementFuel) {
            if (movement.fleet != fleetId
                || (leg.targetFleet != 0 ? movement.targetFleet != leg.targetFleet
                    : movement.targetFleet != 0 || !same_position(movement.destination, leg.destination))) continue;
            if (legForecasts) {
                auto& forecast = (*legForecasts)[legIndex];
                forecast.fuelUsed = forecast.fuelUsed.value_or(0.0) + movement.consumed;
                forecast.fuelRemaining = movement.remaining;
                forecast.fuelMinimum=forecast.fuelMinimum.value_or(0.0)+movement.consumedMinimum;
                forecast.fuelMaximum=forecast.fuelMaximum.value_or(0.0)+movement.consumedMaximum;
            }
            if (movement.fuelLimited && !fuelShortageSeen) {
                fuelShortageSeen = true;
                if (legForecasts) (*legForecasts)[legIndex].fuelShortageTurn = step;
                if (fuelWarning) {
                    *fuelWarning = QString("Fuel shortage forecast at T+%1, before waypoint %2 (%3). "
                        "This preview includes earlier legs, cargo changes, fuel generation and orbital refuelling. "
                        "Reduce Warp or add a refuelling stop.")
                        .arg(step).arg(static_cast<qulonglong>(legIndex + 1))
                        .arg(waypointName(state, leg.destination, leg.targetFleet));
                }
            }
        }
        const auto markArrival = [&] {
            if (!legForecasts) return;
            auto& forecast = (*legForecasts)[legIndex];
            forecast.arrivalTurn = step;
            if (!forecast.fuelUsed) forecast.fuelUsed = 0.0;
            if (after) forecast.fuelRemaining = after->fuel;
        };
        if (!after) {
            if (leg.arrivalAction.kind == FleetArrivalActionKind::MergeWithFleet
                && findFleet(next, leg.targetFleet)) {
                lines << QString("%1. %2 — intercept T+%3, W%4 — <b>rendezvous completed; fleets merged</b>")
                             .arg(static_cast<qulonglong>(legIndex + 1))
                             .arg(waypointName(state, leg.destination, leg.targetFleet))
                             .arg(step)
                             .arg(leg.warp);
                markArrival();
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
                    markArrival();
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

        if (fuelWarning && !fuelWarning->isEmpty() && !legForecasts) {
            break;
        }

        const auto remainingLegs = (after->destination ? std::size_t{1} : std::size_t{0})
            + after->waypointQueue.size();
        const auto* movingTarget = findFleet(next, leg.targetFleet);
        auto arrivalPosition = movingTarget ? movingTarget->position : leg.destination;
        if (leg.arrivalAction.kind == FleetArrivalActionKind::TransitStargate) {
            const auto* p = find_planet(next, leg.arrivalAction.gateDestination);
            if (const auto* star = p ? find_star(next,p->star) : nullptr) arrivalPosition = star->position;
            dependsOnDynamicResult = true;
            if (after->arrivalAction && after->arrivalAction->kind == FleetArrivalActionKind::TransitStargate
                && same_position(after->position, leg.destination)) {
                const auto error = stargate_transfer_error(next,after->owner,after->id,
                    leg.arrivalAction.gateSource,leg.arrivalAction.gateDestination,leg.arrivalAction.gateResonant,true);
                if (!error.empty()) {
                    lines << QString("%1. Gate step waiting (forecast): %2 Later legs remain blocked; retries once per year.")
                        .arg(qulonglong(legIndex+1)).arg(QString::fromStdString(error).toHtmlEscaped());
                    break;
                }
            }
        }
        if(leg.arrivalAction.kind==FleetArrivalActionKind::LoadMinerals && after->arrivalAction
            && after->arrivalAction->kind==FleetArrivalActionKind::LoadMinerals
            && same_position(after->position,leg.destination)) {
            lines << QString("%1. Load policy waiting (received-stock forecast): leave %2 kt at source; depart with at least %3 kt. "
                "Partial cargo remains aboard; retry annually. Future supplies are not promised; later ETA is unknown.")
                .arg(qulonglong(legIndex+1)).arg(leg.arrivalAction.mineralReserve).arg(leg.arrivalAction.mineralMinimum);
            break;
        }
        if(leg.arrivalAction.kind==FleetArrivalActionKind::UnloadTransportStop && after->arrivalAction
            && after->arrivalAction->kind==FleetArrivalActionKind::UnloadTransportStop && same_position(after->position,leg.destination)) {
            lines << QString("%1. Delivery visit blocked (received-state forecast); recipient ownership/availability is conditional. Cargo retained; later ETA unknown.").arg(qulonglong(legIndex+1));break;
        }
        if(leg.arrivalAction.kind==FleetArrivalActionKind::UnloadTransportLine && (leg.arrivalAction.destinationTarget || leg.arrivalAction.transportDestinationFleet)
            && after->arrivalAction && after->arrivalAction->kind==FleetArrivalActionKind::UnloadTransportLine
            && same_position(after->position,leg.destination)) {
            lines << QString("%1. Destination target waiting (received-stock forecast): stock target %2 kt (0 = unload within capacity). "
                "Surplus remains aboard; retry annually. Future demand, capacity and ownership are unconfirmed; later ETA is unknown.")
                .arg(qulonglong(legIndex+1)).arg(leg.arrivalAction.destinationTarget);
            break;
        }
        // Activation can consume multiple already-reached No action points,
        // including the receiving-system point immediately following a hop.
        std::size_t skipped=0;
        while (legIndex+1+skipped<legs.size()) {
            const auto& candidate=legs[legIndex+1+skipped];
            if (candidate.targetFleet || candidate.arrivalAction.kind!=FleetArrivalActionKind::None
                || !same_position(candidate.destination,arrivalPosition)) break;
            ++skipped;
        }
        const auto completed=legIndex+1+skipped;
        const auto expectedRemaining=route.repeatOrders && completed==legs.size()
            ? after->routeTemplate.size() : legs.size()-completed;
        const bool arrived = same_position(after->position, arrivalPosition)
            && remainingLegs == expectedRemaining;

        if (arrived) {
            markArrival();
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
            case FleetArrivalActionKind::TransitStargate:
                outcome = "; projected gate hop completed; no transit fuel charge; physical support and payment are rechecked";
                break;
            case FleetArrivalActionKind::None:
                if (leg.targetFleet != 0) outcome = "; moving-target rendezvous completed";
                break;
            case FleetArrivalActionKind::LoadMinerals:
            case FleetArrivalActionKind::LoadAllAvailable:
                if (leg.arrivalAction.cargo == FleetCargoKind::Xenocrystal) {
                    outcome = QString("; projected xenocrystals aboard: %1 (received surface-stock estimate)").arg(after->xenocrystal);
                } else if (leg.arrivalAction.cargo == FleetCargoKind::OrbitalColonists) {
                    outcome=QString("; projected orbital colonists aboard: %1 (received population/capacity estimate; zero reserve permits complete evacuation)").arg(qulonglong(after->colonists));
                } else if (leg.arrivalAction.cargo == FleetCargoKind::Colonists) {
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
            case FleetArrivalActionKind::UnloadTransportStop:
                outcome="; one bounded delivery visit using received stocks/capacity; carry surplus onward, recipient availability remains conditional";break;
            case FleetArrivalActionKind::UnloadTransportLine:
            case FleetArrivalActionKind::UnloadAll:
                if(leg.arrivalAction.kind==FleetArrivalActionKind::UnloadTransportLine && (leg.arrivalAction.destinationTarget || leg.arrivalAction.transportDestinationFleet)) {
                    outcome=QString("; projected delivery, target %1 kt (0 = unload within capacity), using received stock; onward travel depends on actual capacity/demand and retained cargo")
                        .arg(leg.arrivalAction.destinationTarget);
                    break;
                }
                if(leg.arrivalAction.cargo==FleetCargoKind::OrbitalColonists) {
                    outcome=QString("; projected orbital delivery, limited by received habitat capacity; colonists remaining aboard: %1").arg(qulonglong(after->colonists));
                    break;
                }
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
                outcome = QString("; projected fuel after arrival: %1 mg; refuel requires a working friendly depot. Deep-space stock is finite and may change before arrival")
                    .arg(after->fuel, 0, 'f', 1);
                break;
            case FleetArrivalActionKind::DeployOutpost:
                outcome = after->outpost != OutpostKind::None ? "; permanently deployed; upkeep begins next year"
                    : "; deployment could not be completed at these coordinates";
                break;
            case FleetArrivalActionKind::SupplyOutpost:
                outcome = QString("; projected tanker fuel after delivery: %1 mg; transfer is limited by actual receiver capacity and tanker stock").arg(after->fuel,0,'f',1);
                break;
            case FleetArrivalActionKind::Colonize:
                outcome = "; colonization could not be completed; fleet remains";
                break;
            case FleetArrivalActionKind::RemoteMining:
                outcome = after->task == FleetTask::RemoteMining
                    ? "; Remote Mining assigned; extraction begins next turn"
                    : "; Remote Mining could not be assigned";
                break;
            case FleetArrivalActionKind::LayMinefields:
            case FleetArrivalActionKind::SweepMinefields:
            case FleetArrivalActionKind::BombardColony:
            case FleetArrivalActionKind::FieldResearch:
            case FleetArrivalActionKind::RecoverWreckage:
            case FleetArrivalActionKind::RecoverSample:
                outcome = "; work begins locally on arrival if equipment and target still permit; future results are unknown";
                break;
            case FleetArrivalActionKind::EnterWormhole:
                outcome = "; wormhole outcome remains unknown";
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

            if (leg.arrivalAction.kind == FleetArrivalActionKind::UnloadTransportStop
                || leg.arrivalAction.kind == FleetArrivalActionKind::LoadMinerals
                || (leg.arrivalAction.kind==FleetArrivalActionKind::UnloadTransportLine && (leg.arrivalAction.destinationTarget || leg.arrivalAction.transportDestinationFleet))
                || leg.arrivalAction.kind == FleetArrivalActionKind::LoadAllAvailable
                || leg.arrivalAction.kind == FleetArrivalActionKind::SupplyOutpost
                || (leg.arrivalAction.kind == FleetArrivalActionKind::Refuel && !arrivalPlanet)) {
                dependsOnDynamicResult = true;
            }
            for (std::size_t i=0;i<skipped;++i) {
                ++legIndex;
                markArrival();
                lines << QString("%1. %2 — T+%3; already reached, no action")
                    .arg(qulonglong(legIndex+1)).arg(waypointName(state,legs[legIndex].destination)).arg(step);
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
        lines << "<i>Projected legs depend on actual arrival cargo and gate support/payment.</i>";
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
    auto received=state_;
    project_interstellar_medium(received,fleet->owner);
    return fleet_overdrive_damage_rate(received, fleet_player_view(state_, *fleet), warp);
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
        const auto target = knownFleetTarget(targetFleet);
        if (!target || targetFleet == fleet.id) return {};
        destination = target->position;
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
        (void)routeForecast(state_, pendingOrders_, processor_, fleet->id, *route, &routeLegForecasts_);
        routeEtaRevision_ = planningRevision_;
        routeEtaFleet_ = fleet->id;
    }
    for (std::size_t index = 0; index < legs.size(); ++index) {
        const auto& leg = legs[index];
        const auto forecast = index < routeLegForecasts_.size()
            ? routeLegForecasts_[index] : RouteProgramLegForecast{};
        QString tooltip = "Projected movement fuel for this leg, including turn-by-turn billing and cargo changes. "
            "Fuel left includes arrival refuelling. Preview horizon: 96 years; later orders and hostile encounters may change the result.";
        const bool medium=state_.interstellarMediumEnabled;
        if (medium) tooltip+="\nMedium estimate uses received profiles only. The fuel range covers density 0–1 for the forecast flown segments, with fitted engines and rounding. "
            "Unsurveyed density can change fuel-limited coasting, arrival time and refuelling; ETA and fuel left are estimates.";
        if (medium && forecast.fuelUsed) tooltip+=QString("\nReceived-profile consumption estimate: ≈%1 mg.").arg(*forecast.fuelUsed,0,'f',1);
        if (!forecast.arrivalTurn) tooltip += "\nArrival is not predicted: ≥ is consumption so far, not the complete leg's cost; — means no data.";
        if (forecast.fuelShortageTurn) tooltip += QString("\nFuel shortage at T+%1 on this leg: the ordered Warp cannot be maintained. "
            "Reduce Warp or add a refuelling stop; free-speed coasting may still allow a later arrival.").arg(*forecast.fuelShortageTurn);
        rows.push_back({
            leg.arrivalAction.kind == FleetArrivalActionKind::TransitStargate
                ? waypointName(state_,leg.destination) + " → " + gateDestinationName(state_,leg.arrivalAction.gateDestination) + " [gate]"
                : waypointName(state_, leg.destination, leg.targetFleet),
            actionName(state_,leg.arrivalAction),
            leg.warp,
            index == 0,
            forecast.arrivalTurn ? (medium?QString("≈%1").arg(*forecast.arrivalTurn):QString::number(*forecast.arrivalTurn)) : QString("—"),
            medium && forecast.fuelMinimum && forecast.fuelMaximum ? QString("%1%2–%3").arg(forecast.arrivalTurn?"":"≥")
                .arg(*forecast.fuelMinimum,0,'f',1).arg(*forecast.fuelMaximum,0,'f',1) :
            forecast.fuelUsed ? QString("%1%2").arg(forecast.arrivalTurn ? "" : "≥")
                .arg(*forecast.fuelUsed, 0, 'f', 1) : QString("—"),
            forecast.fuelRemaining ? (medium?QString("≈%1").arg(*forecast.fuelRemaining,0,'f',1):QString::number(*forecast.fuelRemaining, 'f', 1)) : QString("—"),
            tooltip,
            forecast.fuelShortageTurn.has_value(),
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
    for (const auto& waypoint : routeLegs(*route)) {
        points.push_back(resolvedPosition(waypoint.destination,waypoint.targetFleet));
        if (waypoint.arrivalAction.kind==FleetArrivalActionKind::TransitStargate) {
            const auto* p=find_planet(state_,waypoint.arrivalAction.gateDestination);
            if (const auto* star=p?find_star(state_,p->star):nullptr) points.push_back(star->position);
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
    for (const auto& fleet : state_.fleets) {
        if (fleet.owner == pendingOrders_.player && !unreported_orbital_ship(state_,fleet)) fleets.push_back(fleet.id);
    }
    for (const auto& contact : missing_fleet_contacts(state_, pendingOrders_.player)) fleets.push_back(contact.id);
    std::sort(fleets.begin(), fleets.end());
    return fleets;
}

std::vector<FleetId> MainWindow::ownedFleetsAtSelectedSystem() const
{
    const auto* star = selectedStar();
    if (!star) return {};
    std::vector<FleetId> fleets;
    for (const auto id : availableOwnedFleetsForRouteProgram()) {
        const auto* fleet = findFleet(state_, id);
        if (fleet && same_position(fleet_player_view(state_, *fleet).position, star->position))
            fleets.push_back(id);
    }
    return fleets;
}

QString MainWindow::fleetSourceNameForRouteProgram(FleetId id) const
{
    const auto* fleet = findFleet(state_, id);
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
    if(const auto* f=findFleet(state_,id); f && f->owner==pendingOrders_.player && !unreported_orbital_ship(state_,*f)) return fleet_player_view(state_,*f);
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
    if (!star) return {};

    std::vector<RouteProgramTargetOption> targets;
    targets.push_back({
        QString("System — %1").arg(QString::fromStdString(star->name)),
        1,
        star->id,
        false,
    });

    const auto source = selectedFleetForRouteProgram();
    for (const auto& fleet : make_player_view(state_,pendingOrders_.player).state.fleets) {
        if (fleet.id == source) continue;
        const auto& visible = fleet;
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
    const auto missing = missing_fleet_contacts(state_, pendingOrders_.player);
    const bool ownContact = std::any_of(missing.begin(), missing.end(), [=](const auto& contact) { return contact.id == fleetId; });
    if (fleet && unreported_orbital_ship(state_,*fleet)) return false;
    if ((!fleet || fleet->owner != pendingOrders_.player) && !ownContact) return false;
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
    if (!source || source->owner != pendingOrders_.player || !arrival_work_task(action.kind)) return false;
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
        status->setText("Forecast from received gates and fleet telemetry. Arrival mass, source stocks and physical gate support are rechecked locally; "
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
        "Use Route forecast after choosing to check the complete programme.",choices,0,false,&accepted);
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
            && action.kind != FleetArrivalActionKind::MergeWithFleet) {
            statusBar()->showMessage("Fleet targets support only No action or Merge", 4000);
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
