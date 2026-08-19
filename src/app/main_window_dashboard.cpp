#include "main_window.hpp"

#include <algorithm>

namespace suns {

namespace {

const StarSystem* findStarAtPosition(const GameState& state, Position position)
{
    const auto it = std::find_if(state.stars.begin(), state.stars.end(), [&](const StarSystem& star) {
        return same_position(star.position, position);
    });
    return it == state.stars.end() ? nullptr : &*it;
}

QString mineralBill(const MineralCargo& minerals)
{
    return QString("I %1 / B %2 / G %3")
        .arg(minerals.ironium, 0, 'f', 0)
        .arg(minerals.boranium, 0, 'f', 0)
        .arg(minerals.germanium, 0, 'f', 0);
}

QString productionLine(const GameState& state, const Planet& planet)
{
    if (planet.productionQueue.empty()) return "Idle";
    const auto& item = planet.productionQueue.front();
    QString name = "Factory";
    if (item.kind != ProductionKind::Factory) {
        const auto designId = item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId;
        if (const auto* design = find_ship_design(state, designId)) name = QString::fromStdString(design->name);
        else name = "Ship";
    }

    const auto minerals = production_item_mineral_cost(state, item);
    QString text;
    if (item.remainingCost == 0 && !mineral_cargo_sufficient(planet.minerals, minerals)) {
        text = QString("%1 — <span style='color:#e4b77d'><b>waiting for minerals</b></span> (%2)")
                   .arg(name, mineralBill(minerals));
    } else {
        text = QString("%1 — %2 production remaining • %3")
                   .arg(name)
                   .arg(item.remainingCost)
                   .arg(mineralBill(minerals));
    }
    if (planet.productionQueue.size() > 1) {
        text += QString(" • +%1 queued").arg(static_cast<qulonglong>(planet.productionQueue.size() - 1));
    }
    return text;
}

QString componentLine(const ShipDesign* design)
{
    if (!design || design->components.empty()) return "none";
    QStringList names;
    for (const auto component : design->components) {
        names << QString::fromStdString(component_spec(component).name);
    }
    return names.join(", ");
}

QString arrivalName(const FleetArrivalAction& action)
{
    switch (action.kind) {
    case FleetArrivalActionKind::None: return "none";
    case FleetArrivalActionKind::LoadColonistsToCapacity: return "load colonists";
    case FleetArrivalActionKind::UnloadAllColonists: return "unload colonists";
    case FleetArrivalActionKind::Refuel: return "refuel";
    case FleetArrivalActionKind::Colonize: return "colonize";
    }
    return "none";
}

} // namespace

QString MainWindow::selectedPlanetPanelSummary() const
{
    const auto* star = selectedStar();
    if (!star) return "<span style='color:#8090a2'>No system selected.</span>";

    QStringList lines;
    lines << QString("<b>%1</b>").arg(QString::fromStdString(star->name));

    if (!is_surveyed(state_, 1, star->id)) {
        lines << "<b>UNSURVEYED</b> — planetary data unknown";
        lines << "Bring the star inside friendly sensor coverage to reveal it.";
        return lines.join("<br>");
    }

    const auto* planet = find_planet_at_star(state_, star->id);
    if (!planet) {
        lines << "No planet in this system.";
        return lines.join("<br>");
    }

    lines << QString("%1 • Habitability <b>%2%</b>")
                 .arg(QString::fromStdString(planet->name))
                 .arg(planet->habitability);

    if (planet->owner == 1) {
        lines << QString("<span style='color:#85d5a5'><b>Terran colony</b></span>");
        lines << QString("Population %1 / %2 • +%3 next turn")
                     .arg(static_cast<qulonglong>(planet->population))
                     .arg(static_cast<qulonglong>(population_capacity(*planet)))
                     .arg(static_cast<qulonglong>(projected_population_growth(*planet)));
        lines << QString("Infrastructure %1 • Output %2 / turn • Stored %3")
                     .arg(planet->industry)
                     .arg(colony_output(*planet))
                     .arg(planet->stockpile);
        lines << QString("Production: <b>%1</b>").arg(productionLine(state_, *planet));
        lines << QString("Mineral stocks — I %1 • B %2 • G %3")
                     .arg(planet->minerals.ironium, 0, 'f', 1)
                     .arg(planet->minerals.boranium, 0, 'f', 1)
                     .arg(planet->minerals.germanium, 0, 'f', 1);
    } else if (planet->owner == 0) {
        lines << "<span style='color:#c6b57c'><b>Uncolonized</b></span>";
        lines << QString("Potential population capacity: %1")
                     .arg(static_cast<qulonglong>(population_capacity(*planet)));
        lines << QString("Mineral stocks — I %1 • B %2 • G %3")
                     .arg(planet->minerals.ironium, 0, 'f', 1)
                     .arg(planet->minerals.boranium, 0, 'f', 1)
                     .arg(planet->minerals.germanium, 0, 'f', 1);
    } else {
        lines << "<b>Foreign world</b>";
    }

    return lines.join("<br>");
}

QString MainWindow::selectedFleetPanelSummary() const
{
    const auto* fleet = selectedFleet();
    if (!fleet) return "<span style='color:#8090a2'>No fleet selected.</span>";

    const auto* design = fleet_design(state_, *fleet);
    const auto hull = design ? hull_spec(design->hull) : ShipHullSpec{};
    QStringList lines;
    lines << QString("<b>%1</b>").arg(QString::fromStdString(fleet->name));
    if (design) {
        lines << QString("%1 • %2 • max W%3")
                     .arg(QString::fromStdString(design->name))
                     .arg(QString::fromStdString(hull.name))
                     .arg(ship_design_max_warp(*design));
    }

    if (fleet->destination) {
        const auto* target = findStarAtPosition(state_, *fleet->destination);
        QString route = QString("Route: <b>%1</b> • W%2 • ETA %3")
                            .arg(target ? QString::fromStdString(target->name) : "course target")
                            .arg(fleet->warp)
                            .arg(fleet_eta(state_, *fleet));
        if (fleet->arrivalAction) route += QString(" • arrival: %1").arg(arrivalName(*fleet->arrivalAction));
        if (!fleet->waypointQueue.empty()) {
            route += QString(" • +%1 waypoint%2")
                         .arg(static_cast<qulonglong>(fleet->waypointQueue.size()))
                         .arg(fleet->waypointQueue.size() == 1 ? "" : "s");
        }
        lines << route;
    } else {
        lines << "Route: stationary";
    }

    lines << QString("Fuel %1 / %2 • Gross mass %3 kt")
                 .arg(fleet->fuel, 0, 'f', 1)
                 .arg(fleet_fuel_capacity(state_, *fleet), 0, 'f', 1)
                 .arg(fleet_gross_mass(state_, *fleet), 0, 'f', 1);
    lines << QString("Cargo %1 / %2 • Colonists %3")
                 .arg(fleet_cargo_used(state_, *fleet), 0, 'f', 1)
                 .arg(fleet_cargo_capacity(state_, *fleet), 0, 'f', 1)
                 .arg(static_cast<qulonglong>(fleet->colonists));
    lines << QString("Minerals — I %1 • B %2 • G %3")
                 .arg(fleet->minerals.ironium, 0, 'f', 0)
                 .arg(fleet->minerals.boranium, 0, 'f', 0)
                 .arg(fleet->minerals.germanium, 0, 'f', 0);

    const auto sensor = fleet_sensor_range(state_, *fleet);
    lines << QString("Survey sensor: %1")
                 .arg(sensor > 0.0 ? QString("%1 ly").arg(sensor, 0, 'f', 0) : "none");

    if (design && ship_design_radiation_hazard(*design) > 0.0) {
        if (fleet_radiation_safe(state_, *fleet)) {
            lines << "<span style='color:#83c99a'>Radiating drive: safe for this race</span>";
        } else {
            lines << QString("<span style='color:#df9b78'><b>Radiating drive:</b> projected loss %1 colonists per travel turn</span>")
                         .arg(static_cast<qulonglong>(projected_fleet_radiation_losses(state_, *fleet)));
        }
    }

    lines << QString("Components: %1").arg(componentLine(design));
    return lines.join("<br>");
}

} // namespace suns
