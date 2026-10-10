#include "suns/campaign.hpp"
#include "suns/equipment.hpp"
#include "suns/hulls.hpp"

#include <algorithm>
#include <cassert>
#include <set>

using namespace suns;
using C = ShipComponentType;

GameState equipped_empire()
{
    auto state = generate_campaign({}, {{"Fitting"}});
    state.players.front().technology.levels.fill(26);
    return state;
}

void reference_values_and_legacy()
{
    assert(equipment_technologies().size() == 113);
    std::set<C> ids;
    for (const auto& equipment : equipment_technologies()) assert(ids.insert(equipment.component).second);
    const auto tank = component_spec(C::StarsFuelTank);
    assert(tank.mass == 3 && tank.buildCost == 4 && tank.fuelCapacity == 250);
    assert(component_mineral_cost(C::StarsFuelTank).ironium == 6);
    const auto cargo = component_spec(C::StarsCargoPod);
    assert(cargo.mass == 5 && cargo.cargoCapacity == 50 && cargo.buildCost == 10);
    const auto colony = component_spec(C::StarsColonizationModule);
    assert(colony.mass == 32 && colony.buildCost == 10 && colony.enablesColonization);
    const auto superTank = component_spec(C::SuperFuelTank);
    assert(superTank.mass == 8 && superTank.fuelCapacity == 500);
    assert(component_spec(C::Tritanium).armor == 50);
    assert(component_spec(C::CompletePhaseShield).shields == 500);
    assert(component_spec(C::CrobySharmor).armor == 65);
    assert(component_spec(C::FieldedKelarium).shields == 50);
    const auto missile = component_spec(C::ArmageddonMissile);
    assert(missile.weaponPower == 525 && missile.weaponRange == 6 && missile.weaponAccuracy == 30 && missile.missile);
    assert(component_spec(C::Laser).weaponPower == 10);
    assert(component_spec(C::PulsedSapper).shieldOnly);
    assert(component_spec(C::MineDispenser130).minesPerYear == 130);
    assert(component_spec(C::StarsAntimatterGenerator).fuelGenerationPerTurn == 50);
    // Old fitted projects must retain their previous balance.
    assert(component_spec(C::FuelTank).mass == 8 && component_spec(C::FuelTank).fuelCapacity == 300);
    assert(component_spec(C::CargoPod).cargoCapacity == 100 && component_spec(C::ColonyModule).mass == 25);
    auto state = equipped_empire();
    assert(!player_uses_legacy_equipment(state, 1));
    assert(!component_available_to_player(state, 1, C::FuelTank));
    assert(component_available_to_player(state, 1, C::StarsFuelTank));
    assert(ship_design_can_colonize(state.shipDesigns[1]));
    assert(state.shipDesigns[1].components.back() == C::StarsColonizationModule);
    state = make_demo_game();
    assert(player_uses_legacy_equipment(state, 1));
    assert(component_available_to_player(state, 1, C::FuelTank));
}

void fit_and_production()
{
    auto state = equipped_empire();
    ShipDesign destroyer{90, 1, "Gunboat", ShipHullType::Destroyer,
        {C::QuickJump5, C::Laser, C::Laser, C::MoleSkinShield, C::Tritanium,
         C::StarsFuelTank, C::BattleComputer}};
    assert(ship_design_valid(destroyer));
    assert(ship_design_available_to_player(state, 1, destroyer));
    assert(ship_design_armor(destroyer) == 250 && ship_design_shields(destroyer) == 25);
    assert(ship_design_mass(destroyer) == 101);
    assert(ship_design_cost(destroyer) == 72);
    const auto minerals = ship_design_mineral_cost(destroyer);
    assert(minerals.ironium == 30 && minerals.boranium == 15 && minerals.germanium == 22);
    assert(ship_design_fuel_capacity(destroyer) == 530);
    auto bad = destroyer;
    bad.placements = autoplace_ship_components(bad.hull, bad.components);
    const auto hull = hull_spec(bad.hull);
    const auto armor = std::find_if(hull.fittingSlots.begin(), hull.fittingSlots.end(), [](const auto& slot) {
        return slot.category == ShipSlotCategory::Armor;
    });
    const auto beam = std::find_if(bad.placements.begin(), bad.placements.end(), [](const auto& p) { return p.component == C::Laser; });
    beam->slot = armor->id;
    assert(!ship_design_valid(bad));
    auto freighter = ShipDesign{91, 1, "Cloaked hauler", ShipHullType::SmallFreighter,
        {C::QuickJump5, C::TransportCloaking}};
    assert(ship_design_valid(freighter));
    freighter.hull = ShipHullType::StarsScout;
    assert(!ship_design_valid(freighter));
    auto unknown = destroyer;
    unknown.components.push_back(static_cast<C>(255));
    assert(!ship_design_valid(unknown) && !component_available_to_player(state, 1, static_cast<C>(255)));
    state.players.front().technology.levels.fill(0);
    assert(!component_available_to_player(state, 1, C::CompletePhaseShield));
    auto locked = destroyer;
    locked.components.back() = C::BattleNexus;
    assert(!ship_design_available_to_player(state, 1, locked));
    PlayerOrders forged{1, {CreateShipDesignOrder{locked.name, locked.hull, locked.components, {}}}};
    const auto denied = TurnProcessor{}.process_with_events(state, {forged});
    assert(denied.state.shipDesigns.size() == state.shipDesigns.size());

    // Host actually accepts, prices and builds a fit with the new IDs.
    state = equipped_empire();
    auto& planet = state.planets.front();
    assert(planet.owner == 1);
    planet.industry = 1000; planet.minerals = {1000, 1000, 1000};
    state.orbitalStations.push_back({999, 1, planet.id, "Yard", OrbitalStationHullType::OrbitalDock,
        {OrbitalStationModule::Shipyard}});
    PlayerOrders orders{1, {CreateShipDesignOrder{destroyer.name, destroyer.hull, destroyer.components, {}}}};
    auto accepted = TurnProcessor{}.process_with_events(state, {orders}).state;
    const auto newDesign = std::find_if(accepted.shipDesigns.begin(), accepted.shipDesigns.end(), [](const auto& d) { return d.name == "Gunboat"; });
    assert(newDesign != accepted.shipDesigns.end());
    const auto id = newDesign->id;
    PlayerOrders build{1, {QueueShipDesignOrder{planet.id, id}}};
    const auto built = TurnProcessor{}.process_with_events(accepted, {build}).state;
    assert(std::any_of(built.fleets.begin(), built.fleets.end(), [id](const auto& fleet) {
        return fleet.design == id;
    }));
}

void racial_and_research_gates()
{
    auto state = equipped_empire();
    assert(!component_available_to_player(state, 1, C::Jammer10));
    assert(!component_available_to_player(state, 1, C::MineDispenser40));
    assert(component_available_to_player(state, 1, C::MineDispenser50));
    state.players.front().race.hullAccess = HullAccess::InnerStrength;
    assert(component_available_to_player(state, 1, C::Jammer10));
    assert(!component_available_to_player(state, 1, C::SmartBomb));
    assert(component_available_to_player(state, 1, C::SpeedTrap20));
    assert(!component_available_to_player(state, 1, C::StarsAntimatterGenerator)); // IT is not implemented.
    state.players.front().race.hullAccess = HullAccess::WarMonger;
    assert(!component_available_to_player(state, 1, C::MineDispenser50));
    assert(component_available_to_player(state, 1, C::Blunderbuss));
    assert(!component_available_to_player(state, 1, C::RetroBomb));
    assert(!component_available_to_player(state, 1, C::OrbitalConstructionModule));
    assert(!component_available_to_player(state, 1, C::LangstonShell));
    state.shipDesigns.push_back({500, 1, "Acquired", ShipHullType::StarsScout, {C::QuickJump5, C::LangstonShell}});
    assert(component_available_to_player(state, 1, C::LangstonShell));
    state.shipDesigns.back().owner = 2;
    assert(!component_available_to_player(state, 1, C::LangstonShell));
    // Every prerequisite is enforced independently, including multi-field items.
    for (const auto& equipment : equipment_technologies()) {
        state = equipped_empire();
        switch (equipment.access) {
        case EquipmentAccess::InnerStrength: case EquipmentAccess::InnerStrengthOrSpaceDemolition:
            state.players.front().race.hullAccess = HullAccess::InnerStrength; break;
        case EquipmentAccess::SuperStealth: state.players.front().race.hullAccess = HullAccess::SuperStealth; break;
        case EquipmentAccess::WarMonger: state.players.front().race.hullAccess = HullAccess::WarMonger; break;
        case EquipmentAccess::SpaceDemolition: state.players.front().race.hullAccess = HullAccess::SpaceDemolition; break;
        case EquipmentAccess::HyperExpansion: state.players.front().race.hullAccess = HullAccess::HyperExpansion; break;
        case EquipmentAccess::MysteryTrader:
            state.shipDesigns.push_back({500, 1, "Acquired", ShipHullType::Nubian, {equipment.component}}); break;
        default: break;
        }
        if (equipment.access == EquipmentAccess::ClaimAdjuster || equipment.access == EquipmentAccess::AlternateReality
            || equipment.access == EquipmentAccess::InterstellarTraveler) continue;
        assert(component_available_to_player(state, 1, equipment.component));
        for (std::size_t field = 0; field < kResearchFieldCount; ++field) {
            if (!equipment.levels[field]) continue;
            state.players.front().technology.levels[field] = equipment.levels[field] - 1;
            assert(!component_available_to_player(state, 1, equipment.component));
            state.players.front().technology.levels[field] = 26;
        }
    }
}

int main()
{
    reference_values_and_legacy();
    fit_and_production();
    racial_and_research_gates();
}
