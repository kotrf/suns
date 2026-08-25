#!/usr/bin/env python3
from pathlib import Path
import subprocess


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing anchor: {label}")
    return text.replace(old, new, 1)

p = Path("src/app/main_window.cpp")
text = p.read_text()

old = '''        for (const auto& fleet : state_.fleets) {
            if (fleet.owner != 1) continue;
            const auto range = fleet_sensor_range(state_, fleet);
            if (range > 0.0) {
                addSensorRange(scene_, fleet.position, range,
                    QColor(90, 165, 255, 115), QColor(90, 165, 255, 10));
            }
        }
'''
new = '''        for (const auto& fleet : state_.fleets) {
            if (fleet.owner != 1) continue;
            const auto visibleFleet = fleet_player_view(state_, fleet);
            const auto range = fleet_sensor_range(state_, visibleFleet);
            if (range > 0.0) {
                addSensorRange(scene_, visibleFleet.position, range,
                    QColor(90, 165, 255, 115), QColor(90, 165, 255, 10));
            }
        }
'''
text = replace_once(text, old, new, "sensor range player view")

old = '''    for (const auto& fleet : state_.fleets) {
        if (!fleet.destination || hasPendingMove(pendingOrders_, fleet.id)) continue;
        const auto routeColor = fleetColor(fleet.role, 105);
        QPen routePen(routeColor);
        routePen.setWidthF(1.15);
        routePen.setStyle(Qt::DotLine);
        auto* route = scene_->addLine(fleet.position.x, fleet.position.y,
            fleet.destination->x, fleet.destination->y, routePen);
        route->setZValue(-20.0);
        QString routeText = QString("W%1 • ETA %2").arg(fleet.warp).arg(turnCount(fleet_eta(fleet)));
        if (fleet.arrivalAction) routeText += QString(" • %1").arg(arrivalActionSummary(*fleet.arrivalAction));
        addTravelLabel(scene_, fleet.position, *fleet.destination, routeText, fleetColor(fleet.role, 155));
    }
'''
new = '''    for (const auto& fleet : state_.fleets) {
        const auto visibleFleet = fleet_player_view(state_, fleet);
        if (!visibleFleet.destination || hasPendingMove(pendingOrders_, fleet.id)) continue;
        const auto routeColor = fleetColor(fleet.role, 105);
        QPen routePen(routeColor);
        routePen.setWidthF(1.15);
        routePen.setStyle(Qt::DotLine);
        auto* route = scene_->addLine(visibleFleet.position.x, visibleFleet.position.y,
            visibleFleet.destination->x, visibleFleet.destination->y, routePen);
        route->setZValue(-20.0);
        QString routeText = QString("W%1 • ETA %2").arg(visibleFleet.warp).arg(turnCount(fleet_eta(visibleFleet)));
        if (visibleFleet.arrivalAction) routeText += QString(" • %1").arg(arrivalActionSummary(*visibleFleet.arrivalAction));
        addTravelLabel(scene_, visibleFleet.position, *visibleFleet.destination, routeText, fleetColor(fleet.role, 155));
    }
'''
text = replace_once(text, old, new, "known route player view")

old = '''            if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                const auto* fleet = findFleet(state_, concreteOrder.fleet);
                if (!fleet) return;
                const auto routeColor = fleetColor(fleet->role, 190);
                QPen routePen(routeColor);
                routePen.setWidthF(1.45);
                routePen.setStyle(Qt::DashLine);
                auto* route = scene_->addLine(fleet->position.x, fleet->position.y,
                    concreteOrder.destination.x, concreteOrder.destination.y, routePen);
                route->setZValue(-18.0);
                const auto routeWarp = concreteOrder.warp == 0 ? fleet->warp : concreteOrder.warp;
                const auto eta = travel_turns(fleet->position, concreteOrder.destination, warp_distance(routeWarp));
                QString routeText = QString("course W%1 • %2").arg(routeWarp).arg(turnCount(eta));
                if (concreteOrder.arrivalAction.kind != FleetArrivalActionKind::None) {
                    routeText += QString(" • %1").arg(arrivalActionSummary(concreteOrder.arrivalAction));
                }
                addTravelLabel(scene_, fleet->position, concreteOrder.destination, routeText, fleetColor(fleet->role, 210));
            }
'''
new = '''            if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                const auto* fleet = findFleet(state_, concreteOrder.fleet);
                if (!fleet) return;
                const auto visibleFleet = fleet_player_view(state_, *fleet);
                const auto routeColor = fleetColor(fleet->role, 190);
                QPen routePen(routeColor);
                routePen.setWidthF(1.45);
                routePen.setStyle(Qt::DashLine);
                auto* route = scene_->addLine(visibleFleet.position.x, visibleFleet.position.y,
                    concreteOrder.destination.x, concreteOrder.destination.y, routePen);
                route->setZValue(-18.0);
                const auto routeWarp = concreteOrder.warp == 0 ? visibleFleet.warp : concreteOrder.warp;
                const auto eta = travel_turns(visibleFleet.position, concreteOrder.destination, warp_distance(routeWarp));
                QString routeText = QString("course W%1 • %2").arg(routeWarp).arg(turnCount(eta));
                if (concreteOrder.arrivalAction.kind != FleetArrivalActionKind::None) {
                    routeText += QString(" • %1").arg(arrivalActionSummary(concreteOrder.arrivalAction));
                }
                addTravelLabel(scene_, visibleFleet.position, concreteOrder.destination, routeText, fleetColor(fleet->role, 210));
            }
'''
text = replace_once(text, old, new, "pending route player view")
p.write_text(text)

# Silence the intentional ignored bool return without weakening the nodiscard API.
p = Path("src/core/src/turn_processor.cpp")
text = p.read_text()
text = replace_once(text,
    "                        submit_fleet_route_command(\n",
    "                        (void)submit_fleet_route_command(\n",
    "nodiscard communication submission")
p.write_text(text)

# Keep docs precise about every map layer that follows delayed knowledge.
p = Path("docs/communications.md")
text = p.read_text()
text = text.replace(
    "The galaxy map, fleet dashboard, gauges and Route Program consume the owner player-view for remote fleets:",
    "The galaxy map (fleet marker, sensor circle and route overlays), fleet dashboard, gauges and Route Program consume the owner player-view for remote fleets:")
p.write_text(text)

Path("scripts/fix_map_player_view_once.py").unlink(missing_ok=True)
Path(".github/workflows/fix-map-player-view-once.yml").unlink(missing_ok=True)
subprocess.run(["git", "config", "user.name", "Suns CI Rewriter"], check=True)
subprocess.run(["git", "config", "user.email", "actions@users.noreply.github.com"], check=True)
subprocess.run(["git", "add", "-A"], check=True)
subprocess.run(["git", "commit", "-m", "Use delayed player view for all fleet map overlays [skip ci]"], check=True)
subprocess.run(["git", "push", "origin", "HEAD:feature/delayed-communications"], check=True)
