#include "ship_catalog_info.hpp"
#include "suns/hulls.hpp"
#include "suns/scanners.hpp"
#include "suns/mining.hpp"
#include "suns/equipment.hpp"
#include "suns/campaign.hpp"
#include <QStringList>

namespace suns {
namespace {
QString signedFuelRate(double value)
{
    if (value < -0.000001) return QString("%1 (gain)").arg(-value, 0, 'f', 2);
    if (value > 0.000001) return QString::number(value, 'f', 2);
    return "0.00";
}
}
QString componentCatalogRequirements(ShipComponentType component)
{
    for (const auto& unlock : research_unlocks())
        if (unlock.component == component) return QString::fromStdString(research_unlock_requirement(unlock));
    return {};
}

QString componentCatalogDescription(ShipComponentType component)
{
    const auto spec = component_spec(component);
    QStringList facts{
        QString("%1 equipment").arg(QString::fromStdString(ship_component_equipment_name(component))),
        QString("Mass %1 kt").arg(spec.mass, 0, 'f', 1),
        QString("Cost %1").arg(spec.buildCost),
    };
    const auto minerals = component_mineral_cost(component);
    if (const auto* equipment = equipment_technology(component))
        facts << QString::fromStdString(equipment_effect_description(*equipment)).trimmed();
    facts << QString("Minerals (kt): I %1 / B %2 / G %3")
        .arg(minerals.ironium).arg(minerals.boranium).arg(minerals.germanium);
    if (spec.maxWarp > 0) {
        facts << QString("Safe maximum Warp %1 • Thrust %2").arg(spec.maxWarp).arg(spec.engineThrust);
        if (spec.optimalWarp)
            facts << QString("Optimal Warp %1 • Free through Warp %2").arg(spec.optimalWarp).arg(spec.freeWarp);
        facts << "One engine model fills the entire propulsion bank. Costs and mass above are per engine.";
        facts << (spec.optimalWarp
            ? "Base fuel consumption per 100 kt per ly (Fuel efficiency applies in flight):"
            : "Fuel use/gain per 100 kt per ly:");
        for (std::uint8_t warp = 1; warp <= kMaxWarp; ++warp) {
            auto line = QString("W%1: %2").arg(warp).arg(signedFuelRate(spec.fuelPer100MassLy[warp]));
            if (spec.fuelCollectedPerEngineLy[warp] > 0.0)
                line += QString(" • collects %1 mg per engine per ly").arg(spec.fuelCollectedPerEngineLy[warp]);
            if (spec.overdriveDamagePercent[warp] > 0.0)
                line += QString(" • hull damage %1%/turn").arg(spec.overdriveDamagePercent[warp]);
            facts << line;
        }
    }
    if (spec.kind == ShipComponentKind::Scanner) {
        facts << QString("Ordinary scanner range %1 ly; penetrating %2 ly")
            .arg(spec.sensorRange, 0, 'f', 0).arg(spec.penetratingSensorRange, 0, 'f', 0);
        if (spec.sensorRange > 0)
            facts << "Ordinary: detects ships and extends the communications network.";
        if (spec.penetratingSensorRange > 0)
            facts << "Penetrating: surveys planets remotely.";
        if (spec.sensorRange == 0 && spec.penetratingSensorRange == 0)
            facts << "Orbital surveys only; no remote ship detection or communications coverage.";
        facts << "Within one ship, each range is the fourth root of the sum of scanner ranges to the fourth power. Fleet coverage uses the best ship.";
    }
    if (component == ShipComponentType::AnomalyDetector)
        facts << "Dedicated anomaly range 200 ly; faint signatures 90 ly, weak signatures 70 ly. Does not extend ordinary radar or communications.";
    for (const auto& unlock : research_unlocks())
        if (unlock.component == component && (component == ShipComponentType::PickPocketScanner
            || component == ShipComponentType::ChameleonScanner || component == ShipComponentType::RobberBaronScanner))
            facts << QString::fromStdString(unlock.description);
    if (spec.relayRange > 0.0)
        facts << QString("Comms relay +%1 ly; no ship detection or planet survey")
            .arg(spec.relayRange, 0, 'f', 0);
    if (spec.fuelCapacity > 0.0) facts << QString("Fuel capacity +%1").arg(spec.fuelCapacity, 0, 'f', 0);
    if (spec.fuelGenerationPerTurn > 0.0) {
        facts << QString("Fuel generation +%1/turn").arg(spec.fuelGenerationPerTurn, 0, 'f', 0);
    }
    if (spec.cargoCapacity > 0.0) facts << QString("Cargo +%1 kt").arg(spec.cargoCapacity, 0, 'f', 0);
    if (spec.remoteMiningUnits > 0.0)
        facts << QString("Remote mining: %1 kt of each mineral/year at concentration 100, scaled by concentration. Output stays on the unowned planet's surface.")
            .arg(spec.remoteMiningUnits);
    if (spec.fieldRepairPerTurn > 0.0)
        facts << "Field repair: 8 hull-damage points per year per equipped ship; mixed fleets scale by equipped ship fraction. A friendly shipyard repairs 20 instead.";
    if (spec.radiationHazard > 0.0) facts << "Radiation hazard: 10% colonist losses per travel turn unless the race is immune or has radiation tolerance ≥ 85%.";
    if (component == ShipComponentType::OrbitalShipyardKit)
        facts << "Convert one Expedition Base into a permanent orbital shipyard: six supplied assembly years, real workers and finite fuel/minerals. No surface colony needed.";
    if (component >= ShipComponentType::BulkRadiationScreen && component <= ShipComponentType::AdvancedRadiationScreen)
        facts << "One passive radiation screen per base; reduces orbital exposure by 20 / 40 / 60. No combat shield bonus.";
    if (component == ShipComponentType::FuelDepotKit || component == ShipComponentType::DeepSpaceRelayKit)
        facts << "One paid kit per constructor. Fly to deep space, then deploy permanently (one ship, no colonists, 5 ly from stars, 1 ly from another outpost). Upkeep 1 mg/year; tanker deliveries replenish fuel. Depot stock is finite; the relay adds 300 ly only after deployment.";
    if (component == ShipComponentType::SensorArrayKit)
        facts << "One paid kit per constructor. Permanently deploy in deep space for 250 ly ordinary / 75 ly penetrating sensors; planetary penetration estimates habitability. Upkeep 1 mg/year; tankers replenish fuel. The kit supplies no scanning field in transit. Reports use normal communication delays.";
    if (spec.type == ShipComponentType::ExpeditionDock)
        facts << "Expedition Base only. Enables finite-stock refueling and cargo docking for separate friendly fleets; no external services in a year the base moves.";
    if (spec.type == ShipComponentType::ExpeditionWorkshop)
        facts << "Expedition Base + Expedition Dock required. Shared priority queue; restores 100 kt dry structure/year per module at 0.10 I / 0.05 B / 0.05 G per kt. Does not replace onboard Field Repair Bays.";
    if (spec.enablesColonization) facts << "Enables colonization";
    return facts.join("\n");
}

} // namespace suns
