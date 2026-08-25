#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>

namespace {

void verify_hulls_and_slot_validation()
{
    const auto scoutHull = suns::hull_spec(suns::ShipHullType::Scout);
    const auto lightHull = suns::hull_spec(suns::ShipHullType::LightTransport);
    const auto mediumHull = suns::hull_spec(suns::ShipHullType::MediumTransport);

    assert(scoutHull.engineSlots == 1);
    assert(scoutHull.generalSlots == 2);
    assert(lightHull.generalSlots == 3);
    assert(mediumHull.generalSlots == 5);
    assert(mediumHull.baseCargoCapacity > lightHull.baseCargoCapacity);

    suns::ShipDesign valid{
        10, 1, "Surveyor", suns::ShipHullType::Scout,
        {suns::ShipComponentType::RamScoopDrive, suns::ShipComponentType::LongRangeScanner,
         suns::ShipComponentType::FuelTank},
    };
    assert(suns::ship_design_valid(valid));
    assert(suns::ship_design_engine_slots_used(valid) == 1);
    assert(suns::ship_design_general_slots_used(valid) == 2);

    auto overfilled = valid;
    overfilled.components.push_back(suns::ShipComponentType::CargoPod);
    assert(!suns::ship_design_valid(overfilled));

    auto twoEngines = valid;
    twoEngines.components.push_back(suns::ShipComponentType::FusionDrive);
    assert(!suns::ship_design_valid(twoEngines));

    suns::ShipDesign noEngine{
        11, 1, "Drifter", suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::CargoPod},
    };
    assert(!suns::ship_design_valid(noEngine));
}

void verify_component_tradeoffs()
{
    suns::ShipDesign surveyor{
        10, 1, "Surveyor", suns::ShipHullType::Scout,
        {suns::ShipComponentType::RamScoopDrive, suns::ShipComponentType::LongRangeScanner},
    };
    auto longRange = surveyor;
    longRange.components.push_back(suns::ShipComponentType::FuelTank);

    assert(suns::ship_design_valid(longRange));
    assert(suns::ship_design_mass(longRange) > suns::ship_design_mass(surveyor));
    assert(suns::ship_design_cost(longRange) > suns::ship_design_cost(surveyor));
    assert(suns::ship_design_fuel_capacity(longRange) > suns::ship_design_fuel_capacity(surveyor));
    assert(suns::ship_design_sensor_range(longRange) == suns::ship_design_sensor_range(surveyor));

    suns::ShipDesign deepSurveyor{
        12, 1, "Deep Surveyor", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::PenetratingScanner},
    };
    assert(suns::ship_design_valid(deepSurveyor));
    assert(suns::ship_design_sensor_range(deepSurveyor) > 0.0);
    assert(suns::ship_design_penetrating_sensor_range(surveyor) == 0.0);
    assert(suns::ship_design_penetrating_sensor_range(deepSurveyor) > 0.0);
}

void verify_create_design_order()
{
    const suns::TurnProcessor processor;
    const auto initial = suns::make_demo_game();
    assert(initial.nextShipDesignId == suns::kFirstCustomShipDesignId);

    suns::PlayerOrders create{1, {}};
    create.orders.emplace_back(suns::CreateShipDesignOrder{
        "Long Range Surveyor",
        suns::ShipHullType::Scout,
        {suns::ShipComponentType::RamScoopDrive,
         suns::ShipComponentType::LongRangeScanner,
         suns::ShipComponentType::FuelTank},
    });

    const auto created = processor.process(initial, {create});
    const auto* design = suns::find_ship_design(created, suns::kFirstCustomShipDesignId);
    assert(design != nullptr);
    assert(design->name == "Long Range Surveyor");
    assert(design->owner == 1);
    assert(suns::ship_design_valid(*design));
    assert(created.nextShipDesignId == suns::kFirstCustomShipDesignId + 1);

    // Duplicate names and slot-invalid designs are rejected without consuming IDs.
    suns::PlayerOrders invalid{1, {}};
    invalid.orders.emplace_back(suns::CreateShipDesignOrder{
        "Long Range Surveyor",
        suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive},
    });
    invalid.orders.emplace_back(suns::CreateShipDesignOrder{
        "Impossible",
        suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::LongRangeScanner,
         suns::ShipComponentType::FuelTank,
         suns::ShipComponentType::CargoPod},
    });
    const auto rejected = processor.process(created, {invalid});
    assert(rejected.shipDesigns.size() == created.shipDesigns.size());
    assert(rejected.nextShipDesignId == created.nextShipDesignId);
}

void verify_custom_design_can_enter_production()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();

    suns::PlayerOrders create{1, {}};
    create.orders.emplace_back(suns::CreateShipDesignOrder{
        "Cargo Runner",
        suns::ShipHullType::LightTransport,
        {suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::CargoPod,
         suns::ShipComponentType::FuelTank},
    });
    state = processor.process(state, {create});
    const auto designId = suns::kFirstCustomShipDesignId;
    const auto* design = suns::find_ship_design(state, designId);
    assert(design != nullptr);

    suns::PlayerOrders queue{1, {}};
    queue.orders.emplace_back(suns::QueueShipDesignOrder{1, designId});
    const auto built = processor.process(state, {queue});

    // Earth already has enough accumulated production to complete this design
    // during the same turn, so verify the resulting fleet rather than queue state.
    const auto fleet = std::find_if(built.fleets.begin(), built.fleets.end(), [designId](const suns::Fleet& candidate) {
        return candidate.design == designId;
    });
    assert(fleet != built.fleets.end());
    assert(fleet->name == "Cargo Runner 2");
    assert(fleet->colonists == 0);
    assert(built.planets.front().productionQueue.empty());
}

} // namespace

int main()
{
    verify_hulls_and_slot_validation();
    verify_component_tradeoffs();
    verify_create_design_order();
    verify_custom_design_can_enter_production();
    return 0;
}
