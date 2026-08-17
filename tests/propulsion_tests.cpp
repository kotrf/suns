#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>

namespace {

bool near(double a, double b, double epsilon = 0.0001)
{
    return std::abs(a - b) < epsilon;
}

void verify_warp_squared_movement()
{
    assert(suns::warp_distance(1) == 1.0);
    assert(suns::warp_distance(6) == 36.0);
    assert(suns::warp_distance(9) == 81.0);
    assert(suns::warp_distance(10) == 100.0);
    assert(suns::warp_distance(0) == 0.0);
}

void verify_default_propulsion_and_cargo()
{
    const auto state = suns::make_demo_game();
    const auto* scoutDesign = suns::find_ship_design(state, suns::kScoutDesignId);
    const auto* colonyDesign = suns::find_ship_design(state, suns::kColonyShipDesignId);
    assert(scoutDesign != nullptr);
    assert(colonyDesign != nullptr);

    assert(suns::ship_design_max_warp(*scoutDesign) == 10);
    assert(suns::ship_design_fuel_capacity(*scoutDesign) == 300.0);
    assert(suns::ship_design_fuel_capacity(*colonyDesign) == 400.0);
    assert(suns::ship_design_cargo_capacity(*colonyDesign) == 5.0);
    assert(suns::colonist_cargo_mass(250) == 2.5);

    suns::Fleet empty{
        2, 1, "Empty Colony Ship", suns::FleetRole::ColonyShip,
        suns::kColonyShipDesignId, {0.0, 0.0}, std::nullopt, 8, 400.0, 0,
    };
    auto loaded = empty;
    loaded.colonists = 250;

    assert(suns::fleet_gross_mass(state, loaded) > suns::fleet_gross_mass(state, empty));
    assert(suns::fleet_fuel_change_for_distance(state, loaded, 64.0)
        > suns::fleet_fuel_change_for_distance(state, empty, 64.0));
}

void verify_fuel_limits_actual_travel()
{
    auto state = suns::make_demo_game();
    state.planets.clear(); // Prevent automatic colony refuelling at the origin.
    auto& scout = state.fleets.front();
    scout.position = {0.0, 0.0};
    scout.destination.reset();
    scout.warp = 10;
    scout.fuel = 10.0;

    suns::PlayerOrders orders{1, {}};
    orders.orders.emplace_back(suns::MoveFleetOrder{scout.id, {100.0, 0.0}, 10});

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {orders});
    const auto& moved = next.fleets.front();

    assert(moved.position.x > 0.0);
    assert(moved.position.x < 100.0);
    assert(near(moved.fuel, 0.0));
    assert(moved.destination.has_value());
}

void verify_ram_scoop_can_generate_fuel_in_flight()
{
    suns::GameState state;
    state.players.push_back({1, "Terrans", {}});
    state.shipDesigns.push_back({
        10, 1, "Scoop Test", 40.0, 1,
        {suns::ShipComponentType::RamScoopDrive}, 100.0, 0.0,
    });
    state.fleets.push_back({
        1, 1, "Scoop", suns::FleetRole::Scout, 10,
        {0.0, 0.0}, suns::Position{50.0, 0.0}, 4, 0.0, 0,
    });

    assert(suns::fleet_fuel_rate(state, state.fleets.front()) < 0.0);

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {});
    const auto& scoop = next.fleets.front();

    assert(near(scoop.position.x, 16.0)); // Warp 4 => 16 ly/turn.
    assert(scoop.fuel > 0.0);
}

void verify_antimatter_generator_and_radiating_drive_metadata()
{
    suns::GameState state;
    state.players.push_back({1, "Terrans", {}});
    state.shipDesigns.push_back({
        20, 1, "Generator Test", 40.0, 1,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::AntimatterGenerator}, 0.0, 0.0,
    });
    state.fleets.push_back({
        1, 1, "Generator", suns::FleetRole::Scout, 20,
        {25.0, 25.0}, std::nullopt, 8, 0.0, 0,
    });

    const auto* design = suns::find_ship_design(state, 20);
    assert(design != nullptr);
    assert(suns::ship_design_fuel_capacity(*design) == 200.0);
    assert(suns::ship_design_fuel_generation(*design) == 50.0);

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {});
    assert(near(next.fleets.front().fuel, 50.0));

    suns::ShipDesign radiating{
        21, 1, "Radiating Test", 40.0, 1,
        {suns::ShipComponentType::RadiatingRamScoopDrive}, 100.0, 0.0,
    };
    assert(suns::ship_design_radiation_hazard(radiating) > 0.0);
    assert(suns::ship_design_max_warp(radiating) == 10);
    assert(suns::ship_design_fuel_rate(radiating, 4) < 0.0);
}

void verify_invalid_warp_order_is_rejected()
{
    auto state = suns::make_demo_game();
    const auto original = state.fleets.front();

    suns::PlayerOrders orders{1, {}};
    orders.orders.emplace_back(suns::MoveFleetOrder{original.id, {100.0, 0.0}, 11});

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {orders});
    const auto& fleet = next.fleets.front();
    assert(!fleet.destination.has_value());
    assert(fleet.warp == original.warp);
}

} // namespace

int main()
{
    verify_warp_squared_movement();
    verify_default_propulsion_and_cargo();
    verify_fuel_limits_actual_travel();
    verify_ram_scoop_can_generate_fuel_in_flight();
    verify_antimatter_generator_and_radiating_drive_metadata();
    verify_invalid_warp_order_is_rejected();
    return 0;
}
