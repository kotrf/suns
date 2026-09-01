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
    const auto minerHull = suns::hull_spec(suns::ShipHullType::RemoteMiner);
    const auto utilityHull = suns::hull_spec(suns::ShipHullType::Utility);

    assert(scoutHull.requiredEngines == 1);
    assert(mediumHull.requiredEngines == 2);
    assert(minerHull.requiredEngines == 2);
    assert(utilityHull.requiredEngines == 2);
    assert(scoutHull.generalSlots == 2);
    assert(lightHull.generalSlots == 3);
    assert(mediumHull.generalSlots == 5);
    assert(mediumHull.baseCargoCapacity > lightHull.baseCargoCapacity);
    assert(utilityHull.baseCargoCapacity == 0.0);
    assert(utilityHull.generalSlots > mediumHull.generalSlots);
    assert(minerHull.miningSlots == 2);
    assert(scoutHull.miningSlots == 0);
    assert(scoutHull.slots.size() == 3);
    assert(scoutHull.slots[0].id == 100);
    assert(scoutHull.slots[0].category == suns::ShipSlotCategory::Engine);
    assert(scoutHull.slots[1].id == 200);
    assert(scoutHull.slots[1].category == suns::ShipSlotCategory::General);
    assert(minerHull.slots[3].id == 300);
    assert(minerHull.slots[3].category == suns::ShipSlotCategory::Mining);

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

    suns::ShipDesign invalidScoutMiner{
        12, 1, "Impossible Scout Miner", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::RemoteMiningModule},
    };
    assert(!suns::ship_design_valid(invalidScoutMiner));

    suns::ShipDesign validMiner{
        13, 1, "Dedicated Miner", suns::ShipHullType::RemoteMiner,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::RemoteMiningModule,
         suns::ShipComponentType::RemoteMiningModule, suns::ShipComponentType::FuelTank},
    };
    assert(suns::ship_design_valid(validMiner));
    assert(suns::ship_design_mining_slots_used(validMiner) == 2);
    assert(suns::ship_design_general_slots_used(validMiner) == 1);
    assert(suns::ship_design_can_remote_mine(validMiner));

    suns::ShipDesign mixedEngineBank{
        14, 1, "Mixed Engines", suns::ShipHullType::Utility,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::RamScoopDrive},
    };
    assert(!suns::ship_design_valid(mixedEngineBank));
    assert(suns::ship_design_validation_error(mixedEngineBank)
        == "Every engine in a hull's propulsion bank must be the same model.");

    suns::ShipDesign legacyUtility{
        15, 1, "Legacy Utility", suns::ShipHullType::Utility,
        {suns::ShipComponentType::RamScoopDrive, suns::ShipComponentType::CargoPod},
    };
    suns::normalize_ship_design_engine_bank(legacyUtility);
    assert(suns::ship_design_engine_slots_used(legacyUtility) == 2);
    assert(legacyUtility.components[0] == suns::ShipComponentType::RamScoopDrive);
    assert(legacyUtility.components[1] == suns::ShipComponentType::RamScoopDrive);
    assert(legacyUtility.placements[0].slot == 100);
    assert(legacyUtility.placements[1].slot == 101);
    assert(suns::ship_design_valid(legacyUtility));

    const auto automatic = suns::autoplace_ship_components(valid.hull, valid.components);
    assert(automatic.size() == valid.components.size());
    assert(automatic[0].slot == 100);
    assert(automatic[1].slot == 200);
    assert(automatic[2].slot == 201);
    auto normalized = valid;
    suns::normalize_ship_design_placement(normalized);
    assert(normalized.placements == automatic);
    assert(suns::ship_design_valid(normalized));

    auto duplicateSlot = normalized;
    duplicateSlot.placements[2].slot = duplicateSlot.placements[1].slot;
    assert(!suns::ship_design_valid(duplicateSlot));
    assert(suns::ship_design_validation_error(duplicateSlot)
        == "Only one component may occupy a hull slot.");

    auto incompatibleSlot = normalized;
    incompatibleSlot.placements[1].slot = 100;
    incompatibleSlot.placements[0].slot = 200;
    assert(!suns::ship_design_valid(incompatibleSlot));
    assert(suns::ship_design_validation_error(incompatibleSlot)
        == "A component is placed in an incompatible hull slot.");
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

    const auto fusion = suns::component_spec(suns::ShipComponentType::FusionDrive);
    const auto scoop = suns::component_spec(suns::ShipComponentType::RamScoopDrive);
    const auto advanced = suns::component_spec(suns::ShipComponentType::AdvancedFusionDrive);
    assert(advanced.maxWarp == 9);
    assert(advanced.mass < scoop.mass);
    assert(advanced.buildCost > scoop.buildCost);
    assert(advanced.fuelPer100MassLy[4] > 0.0);
    assert(scoop.fuelPer100MassLy[4] < 0.0);
    assert(advanced.maxWarp > fusion.maxWarp);

    const auto standardScanner = suns::component_spec(suns::ShipComponentType::LongRangeScanner);
    const auto extendedScanner = suns::component_spec(suns::ShipComponentType::ExtendedRangeScanner);
    assert(extendedScanner.sensorRange > standardScanner.sensorRange);
    assert(extendedScanner.mass > standardScanner.mass);
    assert(extendedScanner.buildCost > standardScanner.buildCost);
    assert(!extendedScanner.penetratesPlanets);
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
        {{100, suns::ShipComponentType::RamScoopDrive},
         {201, suns::ShipComponentType::LongRangeScanner},
         {200, suns::ShipComponentType::FuelTank}},
    });

    const auto created = processor.process(initial, {create});
    const auto* design = suns::find_ship_design(created, suns::kFirstCustomShipDesignId);
    assert(design != nullptr);
    assert(design->name == "Long Range Surveyor");
    assert(design->owner == 1);
    assert(suns::ship_design_valid(*design));
    assert(design->placements.size() == 3);
    assert(design->placements[1].slot == 201);
    assert(design->placements[2].slot == 200);
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

    suns::PlayerOrders lockedMiner{1, {}};
    lockedMiner.orders.emplace_back(suns::CreateShipDesignOrder{
        "Too Early Miner",
        suns::ShipHullType::RemoteMiner,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::RemoteMiningModule},
    });
    const auto stillLocked = processor.process(rejected, {lockedMiner});
    assert(stillLocked.shipDesigns.size() == rejected.shipDesigns.size());

    auto unlocked = stillLocked;
    unlocked.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 1;
    const auto minerCreated = processor.process(unlocked, {lockedMiner});
    assert(minerCreated.shipDesigns.size() == unlocked.shipDesigns.size() + 1);
    assert(minerCreated.shipDesigns.back().hull == suns::ShipHullType::RemoteMiner);

    suns::PlayerOrders advancedDrive{1, {}};
    advancedDrive.orders.emplace_back(suns::CreateShipDesignOrder{
        "Fast Courier",
        suns::ShipHullType::Scout,
        {suns::ShipComponentType::AdvancedFusionDrive},
    });
    const auto driveLocked = processor.process(stillLocked, {advancedDrive});
    assert(driveLocked.shipDesigns.size() == stillLocked.shipDesigns.size());
    assert(!suns::component_available_to_player(
        stillLocked, 1, suns::ShipComponentType::AdvancedFusionDrive));

    auto propulsionUnlocked = stillLocked;
    propulsionUnlocked.players.front().technology.levels[
        static_cast<std::size_t>(suns::ResearchField::Propulsion)] = 1;
    const auto courierCreated = processor.process(propulsionUnlocked, {advancedDrive});
    assert(courierCreated.shipDesigns.size() == propulsionUnlocked.shipDesigns.size() + 1);
    assert(courierCreated.shipDesigns.back().components.front()
        == suns::ShipComponentType::AdvancedFusionDrive);
    assert(suns::component_available_to_player(
        propulsionUnlocked, 1, suns::ShipComponentType::AdvancedFusionDrive));

    suns::PlayerOrders extendedScanner{1, {}};
    extendedScanner.orders.emplace_back(suns::CreateShipDesignOrder{
        "Deep Space Surveyor",
        suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::ExtendedRangeScanner},
    });
    const auto scannerLocked = processor.process(stillLocked, {extendedScanner});
    assert(scannerLocked.shipDesigns.size() == stillLocked.shipDesigns.size());
    assert(!suns::component_available_to_player(
        stillLocked, 1, suns::ShipComponentType::ExtendedRangeScanner));

    auto electronicsUnlocked = stillLocked;
    electronicsUnlocked.players.front().technology.levels[
        static_cast<std::size_t>(suns::ResearchField::Electronics)] = 2;
    const auto surveyorCreated = processor.process(electronicsUnlocked, {extendedScanner});
    assert(surveyorCreated.shipDesigns.size() == electronicsUnlocked.shipDesigns.size() + 1);
    assert(surveyorCreated.shipDesigns.back().components.back()
        == suns::ShipComponentType::ExtendedRangeScanner);
    assert(suns::component_available_to_player(
        electronicsUnlocked, 1, suns::ShipComponentType::ExtendedRangeScanner));

    suns::PlayerOrders antimatterGenerator{1, {}};
    antimatterGenerator.orders.emplace_back(suns::CreateShipDesignOrder{
        "Fuel Tender",
        suns::ShipHullType::LightTransport,
        {suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::AntimatterGenerator},
    });
    const auto generatorLocked = processor.process(stillLocked, {antimatterGenerator});
    assert(generatorLocked.shipDesigns.size() == stillLocked.shipDesigns.size());
    assert(!suns::component_available_to_player(
        stillLocked, 1, suns::ShipComponentType::AntimatterGenerator));

    auto energyUnlocked = stillLocked;
    energyUnlocked.players.front().technology.levels[
        static_cast<std::size_t>(suns::ResearchField::Energy)] = 1;
    const auto tenderCreated = processor.process(energyUnlocked, {antimatterGenerator});
    assert(tenderCreated.shipDesigns.size() == energyUnlocked.shipDesigns.size() + 1);
    assert(tenderCreated.shipDesigns.back().components.back()
        == suns::ShipComponentType::AntimatterGenerator);
    assert(suns::component_available_to_player(
        energyUnlocked, 1, suns::ShipComponentType::AntimatterGenerator));
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
    const auto queued = processor.process(state, {queue});
    assert(queued.planets.front().productionQueue.size() == 1);
    assert(queued.planets.front().productionQueue.front().remainingCost == 4);
    const auto built = processor.process(queued, {});

    // Production capacity is annual and no longer stockpiles between turns, so
    // the 10-point design completes from two years of Earth output.
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
