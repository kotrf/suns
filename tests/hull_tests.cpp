#include "suns/hulls.hpp"
#include "suns/campaign.hpp"

#include <algorithm>
#include <cassert>
#include <set>

using namespace suns;

namespace {

void verify_catalog_and_starter_designs()
{
    assert(reference_hulls().size() == 32);
    std::set<ShipHullType> types;
    for (const auto& hull : reference_hulls()) {
        assert(types.insert(hull.type).second);
        std::set<ShipSlotId> ids;
        int engines = 0;
        for (const auto& slot : hull.fittingSlots) {
            assert(ids.insert(slot.id).second && slot.bank && slot.allowedEquipment);
            engines += slot.category == ShipSlotCategory::Engine;
        }
        assert(engines == hull.requiredEngines);
    }
    const auto scout = hull_spec(ShipHullType::StarsScout);
    assert(scout.mass == 8 && scout.baseFuelCapacity == 50 && scout.armor == 20 && scout.initiative == 1);
    const auto large = hull_spec(ShipHullType::LargeFreighter);
    assert(large.mass == 125 && large.baseCargoCapacity == 1200 && large.baseFuelCapacity == 2600 && large.requiredEngines == 2);
    const auto nubian = hull_spec(ShipHullType::Nubian);
    assert(nubian.constructionLevel == 26 && nubian.requiredEngines == 3 && nubian.generalSlots == 36);
    assert(nubian.armor == 5000 && nubian.baseFuelCapacity == 5000);
    auto state = generate_campaign({}, {{"Hull tests", RacePreset::Terran}});
    assert(!player_uses_legacy_hulls(state, 1));
    assert(state.shipDesigns[0].hull == ShipHullType::StarsScout);
    assert(state.shipDesigns[1].hull == ShipHullType::ColonyShip);
    for (const auto& design : state.shipDesigns) assert(ship_design_valid(design) && ship_design_available_to_player(state, 1, design));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::Scout));
    auto legacy = make_demo_game();
    assert(player_uses_legacy_hulls(legacy, 1));
    assert(hull_spec(ShipHullType::Scout).mass == 34.5);
    assert(ship_design_available_to_player(legacy, 1, legacy.shipDesigns.front()));
}

void verify_unlocks_and_trait_gates()
{
    auto state = generate_campaign({}, {{"Hull tests", RacePreset::Terran}});
    auto& race = state.players[0].race;
    auto& construction = state.players[0].technology.levels[static_cast<std::size_t>(ResearchField::Construction)];
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::MediumFreighter));
    construction = 3;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::MediumFreighter));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::LargeFreighter));
    construction = 26;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::Nubian));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::Dreadnought));
    race.hullAccess = HullAccess::WarMonger;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::Dreadnought));
    assert(ship_hull_available_to_player(state, 1, ShipHullType::BattleCruiser));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::SuperFreighter));
    race.hullAccess = HullAccess::InnerStrength;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::SuperFreighter));
    assert(ship_hull_available_to_player(state, 1, ShipHullType::FuelTransport));
    race.hullAccess = HullAccess::SuperStealth;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::Rogue));
    assert(ship_hull_available_to_player(state, 1, ShipHullType::StealthBomber));
    race.hullAccess = HullAccess::SpaceDemolition;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::SuperMineLayer));
    race.hullAccess = HullAccess::HyperExpansion;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::StarsMiniColonyShip));
    assert(ship_hull_available_to_player(state, 1, ShipHullType::MetaMorph));
    assert(ship_hull_available_to_player(state, 1, ShipHullType::MiniMiner));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::MidgetMiner));
    race.advancedRemoteMining = true;
    assert(ship_hull_available_to_player(state, 1, ShipHullType::MidgetMiner));
    assert(ship_hull_available_to_player(state, 1, ShipHullType::UltraMiner));
    race.advancedRemoteMining = false;
    race.basicRemoteMining = true;
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::MiniMiner));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::Miner));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::MiniMorph));
    state.shipDesigns.push_back({42, 1, "Acquired Morph", ShipHullType::MiniMorph,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}});
    assert(ship_hull_available_to_player(state, 1, ShipHullType::MiniMorph));
}

void verify_slot_types_and_homogeneous_banks()
{
    ShipDesign colony{40, 1, "Colonizer", ShipHullType::ColonyShip,
        {ShipComponentType::QuickJump5, ShipComponentType::ColonyModule}};
    assert(ship_design_valid(colony));
    colony.components[1] = ShipComponentType::LongRangeScanner;
    assert(!ship_design_valid(colony));
    ShipDesign frigate{41, 1, "Frigate", ShipHullType::Frigate,
        {ShipComponentType::QuickJump5, ShipComponentType::LongRangeScanner, ShipComponentType::FuelTank}};
    normalize_ship_design_placement(frigate);
    assert(ship_design_valid(frigate));
    assert(frigate.placements[1].slot == 200); // Dedicated scanner, leaving general bank for tank.
    frigate.components.push_back(ShipComponentType::CompactLongRangeScanner);
    frigate.placements.push_back({201, ShipComponentType::CompactLongRangeScanner});
    assert(ship_design_validation_error(frigate) == "Every component in an equipment bank must be the same model.");
    const auto hull = hull_spec(ShipHullType::SmallFreighter);
    assert(!ship_slot_accepts(hull.fittingSlots.back(), ShipComponentType::CargoPod)); // Shield/armor only.
    ShipDesign miner{42, 1, "Miner", ShipHullType::MidgetMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RemoteMiningModule, ShipComponentType::RemoteMiningModule}};
    assert(ship_design_valid(miner) && ship_design_can_remote_mine(miner));
    miner.components.push_back(ShipComponentType::RemoteMiningModule);
    assert(!ship_design_valid(miner));
    ShipDesign freighter{43, 1, "Freighter", ShipHullType::SmallFreighter, {ShipComponentType::QuickJump5}};
    const auto cost = ship_design_mineral_cost(freighter);
    assert(cost.ironium == 15 && cost.boranium == 0 && cost.germanium == 18);
}

void verify_host_rejects_locked_hull()
{
    auto state = generate_campaign({}, {{"Host", RacePreset::Terran}});
    const auto create = CreateShipDesignOrder{"Battleship", ShipHullType::Battleship,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5, ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}};
    auto next = TurnProcessor{}.process(state, {PlayerOrders{1, {create}}});
    assert(next.shipDesigns.size() == state.shipDesigns.size());
    state.players[0].technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 13;
    next = TurnProcessor{}.process(state, {PlayerOrders{1, {create}}});
    assert(next.shipDesigns.size() == state.shipDesigns.size() + 1);
}

void verify_tanker_generation_and_fleet_support()
{
    GameState state;
    state.players.push_back({1, "Tankers", {}});
    state.shipDesigns.push_back({40, 1, "Supply", ShipHullType::SuperFuelXport,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}});
    Fleet fleet{1, 1, "Supply", FleetRole::Scout, 40, {0, 0}, {}, 1, 0, 0};
    fleet.ships = {{40, 2}};
    fleet.damagePercent = 50;
    state.fleets.push_back(fleet);
    const auto next = TurnProcessor{}.process(state, {});
    assert(next.fleets.front().fuel == 400);
    assert(next.fleets.front().damagePercent == 40); // Support does not stack with tanker count.
    state.fleets.front().destination = Position{10, 0};
    const auto moved = TurnProcessor{}.process(state, {});
    assert(!same_position(moved.fleets.front().position, state.fleets.front().position));
    assert(moved.fleets.front().damagePercent == 50); // Tanker repair applies only while stopped.
    state.fleets.front().destination = Position{0.01, 0};
    const auto arrived = TurnProcessor{}.process(state, {});
    assert(!arrived.fleets.front().destination);
    assert(arrived.fleets.front().damagePercent == 50);
    const auto stopped = TurnProcessor{}.process(arrived, {});
    assert(stopped.fleets.front().damagePercent == 40);
}

void verify_reference_miner_surface_extraction()
{
    auto state = make_demo_game();
    state.players[0].race.advancedRemoteMining = true;
    state.players[0].technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 1;
    ShipDesign miner{40, 1, "Midget", ShipHullType::MidgetMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RemoteMiningModule, ShipComponentType::RemoteMiningModule}};
    state.shipDesigns.push_back(miner);
    Fleet fleet{1, 1, "Mine", FleetRole::Scout, 40, state.stars[1].position, {}, 1, 210, 0};
    fleet.task = FleetTask::RemoteMining;
    state.fleets = {fleet};
    const auto expected = projected_remote_mining(state, state.planets[1], miner);
    assert(expected.ironium > 0 && fleet_cargo_capacity(state, fleet) == 0);
    const auto mined = TurnProcessor{}.process(state, {});
    assert(mined.planets[1].minerals.ironium == expected.ironium);
    assert(mined.fleets[0].minerals.ironium == 0); // Extraction goes onto the surface.
    state.players[0].race.advancedRemoteMining = false;
    const auto rejected = TurnProcessor{}.process(state, {});
    assert(rejected.planets[1].minerals.ironium == 0);
}

} // namespace

int main()
{
    verify_catalog_and_starter_designs();
    verify_unlocks_and_trait_gates();
    verify_slot_types_and_homogeneous_banks();
    verify_host_rejects_locked_hull();
    verify_tanker_generation_and_fleet_support();
    verify_reference_miner_surface_extraction();
}
