#include "main_window.hpp"

#include "suns/communications.hpp"

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

    QString name;
    if (item.kind == ProductionKind::Research) {
        name = "Ongoing Research";
    } else if (item.kind == ProductionKind::Factory) {
        name = "Factory";
    } else if (item.kind == ProductionKind::Mine) {
        name = "Mine";
    } else {
        const auto designId = item.shipDesign != 0 ? item.shipDesign : kColonyShipDesignId;
        if (const auto* design = find_ship_design(state, designId)) name = QString::fromStdString(design->name);
        else name = "Ship";
    }

    if (item.kind == ProductionKind::Research) {
        QString text = "Ongoing Research — all available production becomes empire RP";
        if (planet.productionQueue.size() > 1) {
            text += QString(" • +%1 queued").arg(static_cast<qulonglong>(planet.productionQueue.size() - 1));
        }
        return text;
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
    const auto cargo = [&] {
        switch (action.cargo) {
        case FleetCargoKind::Colonists: return QString("colonists");
        case FleetCargoKind::Ironium: return QString("Ironium");
        case FleetCargoKind::Boranium: return QString("Boranium");
        case FleetCargoKind::Germanium: return QString("Germanium");
        }
        return QString("cargo");
    };
    switch (action.kind) {
    case FleetArrivalActionKind::None: return "none";
    case FleetArrivalActionKind::LoadAllAvailable: return QString("load all %1").arg(cargo());
    case FleetArrivalActionKind::UnloadAll: return QString("unload all %1").arg(cargo());
    case FleetArrivalActionKind::Refuel: return "refuel";
    case FleetArrivalActionKind::Colonize: return "colonize";
    case FleetArrivalActionKind::RemoteMining: return "remote mining";
    case FleetArrivalActionKind::MergeWithFleet: return "merge with fleet";
    }
    return "none";
}

MineralCargo remoteMiningAtPlanet(const GameState& state, PlayerId player, const Planet& planet)
{
    MineralCargo total;
    const auto* star = find_star(state, planet.star);
    if (!star) return total;
    for (const auto& fleet : state.fleets) {
        if (fleet.owner != player || fleet.task != FleetTask::RemoteMining
            || !same_position(fleet.position, star->position)) {
            continue;
        }
        for (const auto& stack : fleet_ship_stacks(fleet)) {
            const auto* design = find_ship_design(state, stack.design);
            if (!design || !ship_design_can_remote_mine(*design)) continue;
            const auto yield = projected_remote_mining(state, planet, *design);
            total.ironium += yield.ironium * stack.count;
            total.boranium += yield.boranium * stack.count;
            total.germanium += yield.germanium * stack.count;
        }
    }
    return total;
}

} // namespace

QString MainWindow::selectedPlanetPanelSummary() const
{
    const auto* star = selectedStar();
    if (!star) return "<span style='color:#8090a2'>No system selected.</span>";

    QStringList lines;
    lines << QString("<b>%1</b>").arg(QString::fromStdString(star->name));

    if (!is_surveyed(state_, 1, star->id)) {
        if (survey_level(state_, 1, star->id) >= SurveyLevel::SystemScan) {
            lines << "<b>SYSTEM CONTACT</b> — ordinary scanner data received";
            lines << "Planetary parameters require orbit or a penetrating scanner.";
        } else {
            lines << "<b>UNSURVEYED</b> — planetary data unknown";
            lines << "Bring the star inside friendly sensor coverage to detect it.";
        }
        return lines.join("<br>");
    }

    const auto* planet = find_planet_at_star(state_, star->id);
    if (!planet) {
        lines << "No planet in this system.";
        return lines.join("<br>");
    }

    const auto knownHabitability = known_planet_habitability(state_, 1, planet->id);
    const auto estimated = survey_level(state_, 1, star->id) == SurveyLevel::BasicScan;
    lines << QString("%1 • Habitability <b>%2%3%</b>")
                 .arg(QString::fromStdString(planet->name))
                 .arg(estimated ? "~" : "")
                 .arg(knownHabitability.value_or(0));
    if (estimated) lines << "Basic scan estimate — enter orbit to confirm";

    if (planet->owner == 1) {
        lines << QString("<span style='color:#85d5a5'><b>Terran colony</b></span>");
        lines << QString("Population %1 / %2 • +%3 next turn")
                     .arg(static_cast<qulonglong>(planet->population))
                     .arg(static_cast<qulonglong>(population_capacity(*planet)))
                     .arg(static_cast<qulonglong>(projected_population_growth(*planet)));
        lines << QString("Factories %1 • Mines %2 • Output %3 / turn • Stored %4")
                     .arg(planet->industry)
                     .arg(planet->mines)
                     .arg(colony_output(*planet))
                     .arg(planet->stockpile);
        lines << QString("Production: <b>%1</b>").arg(productionLine(state_, *planet));
        lines << QString("Mineral stocks — I %1 • B %2 • G %3")
                     .arg(planet->minerals.ironium, 0, 'f', 1)
                     .arg(planet->minerals.boranium, 0, 'f', 1)
                     .arg(planet->minerals.germanium, 0, 'f', 1);
    } else if (planet->owner == 0) {
        lines << "<span style='color:#c6b57c'><b>Uncolonized</b></span>";
        lines << QString("%1 population capacity: %2")
                     .arg(estimated ? "Estimated" : "Potential")
                     .arg(static_cast<qulonglong>(knownHabitability.value_or(0)) * 25ULL);
        if (planet_geology_known(state_, 1, planet->id)) {
            lines << QString("Surface stockpiles — I %1 • B %2 • G %3")
                         .arg(planet->minerals.ironium, 0, 'f', 1)
                         .arg(planet->minerals.boranium, 0, 'f', 1)
                         .arg(planet->minerals.germanium, 0, 'f', 1);
            const auto remoteYield = remoteMiningAtPlanet(state_, 1, *planet);
            if (mineral_cargo_mass(remoteYield) > 0.000001) {
                lines << QString("Remote extraction / turn — I %1 • B %2 • G %3")
                             .arg(remoteYield.ironium, 0, 'f', 2)
                             .arg(remoteYield.boranium, 0, 'f', 2)
                             .arg(remoteYield.germanium, 0, 'f', 2);
            }
        } else {
            lines << "Mineral geology unknown — remain in orbit for a geological survey";
        }
    } else {
        lines << "<b>Foreign world</b>";
    }

    return lines.join("<br>");
}

QString MainWindow::selectedFleetPanelSummary() const
{
    const auto* authoritativeFleet = selectedFleet();
    if (!authoritativeFleet) return "<span style='color:#8090a2'>No fleet selected.</span>";
    const auto visibleFleet = fleet_player_view(state_, *authoritativeFleet);
    const auto* fleet = &visibleFleet;

    const auto* design = fleet_design(state_, *fleet);
    QStringList lines;
    lines << QString("<b>%1</b>").arg(QString::fromStdString(fleet->name));
    QStringList composition;
    for (const auto& stack : fleet_ship_stacks(*fleet)) {
        const auto* stackDesign = find_ship_design(state_, stack.design);
        composition << QString("%1× %2")
                           .arg(stack.count)
                           .arg(stackDesign
                               ? QString::fromStdString(stackDesign->name)
                               : QString("Design %1").arg(stack.design));
    }
    lines << QString("Ships: <b>%1</b> • %2 design%3 • max W%4")
                 .arg(fleet_ship_count(*fleet))
                 .arg(static_cast<qulonglong>(composition.size()))
                 .arg(composition.size() == 1 ? "" : "s")
                 .arg(fleet_max_warp(state_, *fleet));
    lines << composition.join(" • ");

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

    if (fleet->task == FleetTask::RemoteMining) {
        const auto* star = findStarAtPosition(state_, fleet->position);
        const auto* planet = star ? find_planet_at_star(state_, star->id) : nullptr;
        if (planet) {
            MineralCargo yield;
            for (const auto& stack : fleet_ship_stacks(*fleet)) {
                const auto* stackDesign = find_ship_design(state_, stack.design);
                if (!stackDesign || !ship_design_can_remote_mine(*stackDesign)) continue;
                const auto perShip = projected_remote_mining(state_, *planet, *stackDesign);
                yield.ironium += perShip.ironium * stack.count;
                yield.boranium += perShip.boranium * stack.count;
                yield.germanium += perShip.germanium * stack.count;
            }
            lines << QString("<span style='color:#d7bf78'><b>Remote Mining assigned</b></span> — I %1 / B %2 / G %3 per turn")
                         .arg(yield.ironium, 0, 'f', 2)
                         .arg(yield.boranium, 0, 'f', 2)
                         .arg(yield.germanium, 0, 'f', 2);
        } else {
            lines << "<span style='color:#d7bf78'><b>Remote Mining assigned</b></span>";
        }
    } else if (std::any_of(
                   authoritativeFleet->pendingCommands.begin(),
                   authoritativeFleet->pendingCommands.end(),
                   [](const PendingFleetCommand& command) {
                       return command.task == FleetTask::RemoteMining;
                   })) {
        lines << "<span style='color:#d7bf78'><b>Remote Mining command in flight</b></span>";
    }

    const auto sensor = fleet_sensor_range(state_, *fleet);
    const auto penetrating = fleet_penetrating_sensor_range(state_, *fleet);
    lines << QString("Sensors: %1")
                 .arg(sensor <= 0.0
                         ? "none"
                         : penetrating > 0.0
                             ? QString("%1 ly detection • %2 ly penetrating")
                                   .arg(sensor, 0, 'f', 0).arg(penetrating, 0, 'f', 0)
                             : QString("%1 ly detection").arg(sensor, 0, 'f', 0));

    if (fleet_radiation_hazard(state_, *fleet) > 0.0) {
        if (fleet_radiation_safe(state_, *fleet)) {
            lines << "<span style='color:#83c99a'>Radiating drive: safe for this race</span>";
        } else {
            lines << QString("<span style='color:#df9b78'><b>Radiating drive:</b> projected loss %1 colonists per travel turn</span>")
                         .arg(static_cast<qulonglong>(projected_fleet_radiation_losses(state_, *fleet)));
        }
    }

    if (composition.size() == 1) lines << QString("Components: %1").arg(componentLine(design));
    return lines.join("<br>");
}

} // namespace suns
