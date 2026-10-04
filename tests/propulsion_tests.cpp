#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"
#include "suns/campaign.hpp"
#include "suns/propulsion.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>

namespace {

bool near(double a, double b, double epsilon = 0.0001)
{
    return std::abs(a - b) < epsilon;
}

void verify_starting_scout_range()
{
    using namespace suns;
    auto state = generate_game(GalaxyConfig{});
    auto scout = state.fleets.front();
    assert(fleet_gross_mass(state, scout) == 22);
    assert(fleet_fuel_capacity(state, scout) == 50);
    scout.warp = 8;
    assert(fleet_fuel_consumption_for_distance(state, scout, 64) == 57);
    scout.warp = 5;
    assert(fleet_fuel_consumption_for_distance(state, scout, 25) == 3);

    // A generated fleet must already choose the economical engine speed.
    assert(state.fleets.front().warp == 5);
    assert(state.fleets.front().telemetry.warp == 5);
    // Travel away from every dock so refuelling cannot mask the range regression.
    state.fleets.front().position = {1000, 1000};
    state.fleets.front().destination = Position{2000, 1000};
    const auto manualFastRoute = state;
    const TurnProcessor processor;
    for (int turn = 1; turn <= 16; ++turn) {
        state = processor.process(state, {});
        assert(near(state.fleets.front().position.x, 1000 + turn * 25));
        assert(near(state.fleets.front().fuel, 50 - turn * 3));
        assert(state.fleets.front().warp == 5);
    }

    // Explicit/saved Warp 8 routes still work and retain their fuel penalty.
    auto fast = manualFastRoute;
    fast.fleets.front().warp = 8;
    const auto next = processor.process(fast, {});
    assert(next.fleets.front().warp == 8);
    assert(next.fleets.front().position.x > 1055 && next.fleets.front().position.x < 1064);
    assert(next.fleets.front().fuel < 1);

    const auto campaign = generate_campaign(GalaxyConfig{},
        {{"Terrans", RacePreset::Terran}, {"Ice", RacePreset::Cryophile}});
    for (const auto& fleet : campaign.fleets) {
        assert(fleet.warp == 5 && fleet.telemetry.warp == 5);
        assert(fleet_fuel_consumption_for_distance(campaign, fleet, 25) == 3);
    }
}

void verify_built_fleets_start_at_engine_cruise_warp()
{
    using namespace suns;
    auto build = [](GameState state, ShipDesignId designId, std::uint8_t expectedWarp) {
        const auto* design = find_ship_design(state, designId);
        assert(design && ship_design_valid(*design));
        state.planets.front().minerals = {1000, 1000, 1000};
        state.planets.front().productionQueue = {{ProductionKind::ColonyShip, 0, designId}};
        const auto newFleetId = state.nextFleetId;
        const auto next = TurnProcessor{}.process(state, {});
        const auto built = std::find_if(next.fleets.begin(), next.fleets.end(),
            [&](const Fleet& fleet) { return fleet.id == newFleetId; });
        assert(built != next.fleets.end() && built->design == designId);
        assert(built->warp == expectedWarp && built->telemetry.warp == expectedWarp);
    };
    auto reference = generate_game(GalaxyConfig{});
    build(reference, kScoutDesignId, 5);
    build(reference, kColonyShipDesignId, 5);
    // Engine changes must propagate to the production default, including W10.
    for (const auto engine : {ShipComponentType::LongHump6, ShipComponentType::TransStar10}) {
        reference.shipDesigns.front().components.front() = engine;
        reference.shipDesigns.front().placements.clear();
        normalize_ship_design_placement(reference.shipDesigns.front());
        build(reference, kScoutDesignId, engine == ShipComponentType::LongHump6 ? 6 : 10);
    }
    // Multiple copies of the same engine in a required bank do not change cruise speed.
    reference.shipDesigns.push_back({10, 1, "Hauler", ShipHullType::LargeFreighter,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}});
    build(reference, 10, 5);

    const auto legacy = make_demo_game();
    assert(legacy.fleets.front().warp == 8);
    build(legacy, kScoutDesignId, 8);
    build(legacy, kColonyShipDesignId, 7);
}

void verify_warp_squared_movement()
{
    assert(suns::warp_distance(1) == 1.0);
    assert(suns::warp_distance(6) == 36.0);
    assert(suns::warp_distance(9) == 81.0);
    assert(suns::warp_distance(10) == 100.0);
    assert(suns::warp_distance(0) == 0.0);
}

void verify_stars_progression_and_multifield_gates()
{
    using namespace suns;
    auto state = generate_campaign(GalaxyConfig{}, {{"New empire", RacePreset::Terran}});
    assert(propulsion_technologies().size() == 15);
    assert(find_ship_design(state, 1)->components.front() == ShipComponentType::QuickJump5);
    assert(!component_available_to_player(state, 1, ShipComponentType::HighWarpDrive));
    assert(component_available_to_player(state, 1, ShipComponentType::QuickJump5));
    auto& tech = state.players.front().technology;
    const auto prop = static_cast<std::size_t>(ResearchField::Propulsion);
    const auto energy = static_cast<std::size_t>(ResearchField::Energy);
    tech.levels[prop] = 8;
    tech.levels[energy] = 1;
    const auto scoop = ShipComponentType::SubGalacticFuelScoop;
    assert(!component_available_to_player(state, 1, scoop));
    const PlayerOrders order{1, {CreateShipDesignOrder{"Scoop scout", ShipHullType::StarsScout, {scoop}}}};
    assert(TurnProcessor{}.process(state, {order}).shipDesigns.size() == state.shipDesigns.size());
    tech.levels[energy] = 2;
    assert(component_available_to_player(state, 1, scoop));
    assert(TurnProcessor{}.process(state, {order}).shipDesigns.size() == state.shipDesigns.size() + 1);
    tech.levels[prop] = 22;
    assert(!component_available_to_player(state, 1, ShipComponentType::TransStar10));
    tech.levels[prop] = 23;
    assert(component_available_to_player(state, 1, ShipComponentType::TransStar10));
    assert(!component_available_to_player(state, 1, ShipComponentType::Interspace10));
    state.players.front().race.noRamScoopEngines = true;
    assert(component_available_to_player(state, 1, ShipComponentType::Interspace10));
    assert(!component_available_to_player(state, 1, scoop));
    assert(!component_available_to_player(state, 1, ShipComponentType::FuelMizer));
    state.players.front().race.improvedFuelEfficiency = true;
    assert(component_available_to_player(state, 1, ShipComponentType::FuelMizer));
    tech.levels[energy] = 5;
    assert(!component_available_to_player(state, 1, ShipComponentType::GalaxyScoop));
    state.players.front().race.noRamScoopEngines = false;
    assert(component_available_to_player(state, 1, ShipComponentType::GalaxyScoop));
    assert(research_level_cost(ResearchField::Propulsion, 23) == 4572);
    assert(research_level_cost(ResearchField::Propulsion, 255) < 1'000'000);
}

void verify_stars_speed_bands_and_settler_restriction()
{
    using namespace suns;
    ShipDesign quick{40, 1, "Quick", ShipHullType::Scout, {ShipComponentType::QuickJump5}};
    ShipDesign mizer{41, 1, "Mizer", ShipHullType::Scout, {ShipComponentType::FuelMizer}};
    ShipDesign scoop{42, 1, "Scoop", ShipHullType::Scout, {ShipComponentType::RadiatingHydroRamScoop}};
    ShipDesign late{43, 1, "Late", ShipHullType::Scout, {ShipComponentType::TransStar10}};
    assert(ship_design_max_warp(quick) == 9); // Optimal Warp 5 is not the safe-speed limit.
    assert(ship_design_fuel_rate(quick, 1) == 0 && ship_design_fuel_rate(quick, 8) > 0);
    assert(ship_design_fuel_rate(mizer, 4) == 0 && ship_design_fuel_rate(mizer, 5) > 0);
    assert(ship_design_fuel_rate(scoop, 6) == 0 && ship_design_fuel_rate(scoop, 7) > 0);
    assert(component_spec(ShipComponentType::FuelMizer).fuelCollectedPerEngineLy[4] == 1);
    assert(ship_design_radiation_hazard(scoop) > 0);
    assert(ship_design_overdrive_damage(quick, 10) > 0);
    assert(ship_design_overdrive_damage(late, 10) == 0);
    assert(ship_design_fuel_rate(late, 9) < ship_design_fuel_rate(quick, 9));
    const auto minerals = ship_design_mineral_cost(late);
    assert(minerals.ironium == 7 && minerals.boranium == 1 && minerals.germanium == 4);
    auto state = generate_campaign(GalaxyConfig{}, {{"Settlers", RacePreset::Terran, false, true, true}});
    ShipDesign settler{44, 1, "Settler", ShipHullType::StarsMiniColonyShip,
        {ShipComponentType::SettlersDelight, ShipComponentType::ColonyModule}};
    assert(ship_design_valid(settler) && ship_design_available_to_player(state, 1, settler));
    assert(ship_design_cargo_capacity(settler) == 10);
    settler.hull = ShipHullType::Scout;
    assert(!ship_design_valid(settler));
    settler.hull = ShipHullType::StarsMiniColonyShip;
    state.players.front().race.settlerEngineAccess = false;
    assert(!ship_design_available_to_player(state, 1, settler));
}

void verify_original_fuel_tables_and_rounding()
{
    using namespace suns;
    // Independent regression values from Player's Guide B-6. In particular,
    // Mizer beats the P5/P7 standard engines at W9; late TS10 halves IS10 use.
    const auto qj = component_spec(ShipComponentType::QuickJump5);
    assert(near(qj.fuelPer100MassLy[2], 0.125));
    assert(near(qj.fuelPer100MassLy[3], qj.fuelPer100MassLy[5]));
    const auto mizer = component_spec(ShipComponentType::FuelMizer);
    assert(mizer.fuelPer100MassLy[9] < component_spec(ShipComponentType::DaddyLongLegs7).fuelPer100MassLy[9]);
    assert(mizer.fuelPer100MassLy[9] < component_spec(ShipComponentType::AlphaDrive8).fuelPer100MassLy[9]);
    assert(near(component_spec(ShipComponentType::TransStar10).fuelPer100MassLy[10],
        component_spec(ShipComponentType::Interspace10).fuelPer100MassLy[10] / 2));
    assert(propulsion_technology(ShipComponentType::SettlersDelight)->fuelEfficiency[7] == 140);

    GameState state;
    state.players.push_back({1, "Fuel tests", {}});
    state.shipDesigns.push_back({10, 1, "Mizer hauler", ShipHullType::HeavyTransport,
        {ShipComponentType::FuelMizer, ShipComponentType::FuelMizer, ShipComponentType::FuelMizer}});
    Fleet fleet{1, 1, "Fuel", FleetRole::Scout, 10, {0, 0}, Position{100, 0}, 9, 600, 0};
    fleet.minerals.ironium = 42; // Exactly 200 kt including the three engines.
    assert(fleet_gross_mass(state, fleet) == 200);
    assert(fleet_fuel_consumption_for_distance(state, fleet, 100) == 360);
    state.players.front().race.improvedFuelEfficiency = true;
    assert(fleet_fuel_consumption_for_distance(state, fleet, 100) == 306);
    assert(fleet_fuel_consumption_for_distance(state, fleet, 0) == 0);
    assert(fleet_fuel_consumption_for_distance(state, fleet, 1.01) == 7);
    fleet.fuel = 6;
    assert(near(fleet_fuel_affordable_distance(state, fleet, 81), 1));

    // IFE rounds the drive percentage up: 35 * .85 -> 30, not 29.75.
    fleet.warp = 5;
    assert(fleet_fuel_consumption_for_distance(state, fleet, 100) == 30);
}

void verify_original_collection_is_per_engine()
{
    using namespace suns;
    GameState state;
    state.players.push_back({1, "Collectors", {}});
    state.shipDesigns.push_back({10, 1, "Mizer scout", ShipHullType::Scout, {ShipComponentType::FuelMizer}});
    state.shipDesigns.push_back({11, 1, "Mizer hauler", ShipHullType::HeavyTransport,
        {ShipComponentType::FuelMizer, ShipComponentType::FuelMizer, ShipComponentType::FuelMizer}});
    Fleet scout{1, 1, "Scout", FleetRole::Scout, 10, {0, 0}, Position{100, 0}, 4, 0, 0};
    assert(fleet_fuel_change_for_distance(state, scout, 16) == -16);
    scout.warp = 3;
    assert(fleet_fuel_change_for_distance(state, scout, 9) == -27);
    scout.warp = 2;
    assert(fleet_fuel_change_for_distance(state, scout, 4) == -24);
    scout.warp = 1;
    assert(fleet_fuel_change_for_distance(state, scout, 1) == -10);
    scout.design = 11;
    scout.warp = 4;
    assert(fleet_fuel_change_for_distance(state, scout, 16) == -48);
    scout.minerals.ironium = 250;
    assert(fleet_fuel_change_for_distance(state, scout, 16) == -48);
    scout.ships = {{11, 2}};
    assert(fleet_fuel_change_for_distance(state, scout, 16) == -96);
    scout.ships.clear();
    scout.design = 10;
    state.shipDesigns.front().components.front() = ShipComponentType::QuickJump5;
    scout.warp = 1;
    assert(fleet_fuel_change_for_distance(state, scout, 1) == -1);
    scout.warp = 2;
    assert(fleet_fuel_change_for_distance(state, scout, 10) > 0);
}

void verify_mixed_fleet_cargo_and_fuel_collection()
{
    using namespace suns;
    GameState state;
    state.players.push_back({1, "Mixed fleet", {}});
    state.shipDesigns.push_back({10, 1, "Freighter", ShipHullType::HeavyTransport,
        {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}});
    state.shipDesigns.push_back({11, 1, "Escort", ShipHullType::Scout, {ShipComponentType::TransStar10}});
    Fleet fleet{1, 1, "Convoy", FleetRole::Scout, 10, {0, 0}, Position{100, 0}, 9, 600, 0};
    fleet.ships = {{10, 1}, {11, 1}};
    fleet.minerals.ironium = 100;
    // All 100 kt cargo is on the freighter: ceil(918.5) + ceil(7.1).
    assert(fleet_fuel_consumption_for_distance(state, fleet, 81) == 927);
    std::reverse(fleet.ships.begin(), fleet.ships.end());
    assert(fleet_fuel_consumption_for_distance(state, fleet, 81) == 927);
    state.shipDesigns.front().components.assign(3, ShipComponentType::GalaxyScoop);
    fleet.warp = 9;
    fleet.fuel = 0;
    // The scoop's future collection cannot finance the escort's initial burn.
    assert(fleet_fuel_change_for_distance(state, fleet, 81) < 0);
    assert(fleet_fuel_consumption_for_distance(state, fleet, 81) == 8);
    assert(fleet_fuel_affordable_distance(state, fleet, 81) < 81);
    assert(fleet_free_warp(state, fleet) == 1);
}

void verify_reference_fuel_exhaustion_and_free_speed()
{
    using namespace suns;
    GameState state;
    state.players.push_back({1, "Mizer", {}});
    state.players.front().race.improvedFuelEfficiency = true;
    state.shipDesigns.push_back({10, 1, "Mizer scout", ShipHullType::Scout, {ShipComponentType::FuelMizer}});
    state.fleets.push_back({1, 1, "Scout", FleetRole::Scout, 10, {0, 0}, Position{200, 0}, 9, 0, 0});
    auto next = TurnProcessor{}.process(state, {});
    assert(near(next.fleets.front().position.x, 16));
    assert(next.fleets.front().fuel == 16);
    assert(next.fleets.front().warp == 9 && !next.fleets.front().fuelStalled);

    state.fleets.front().fuel = 10;
    next = TurnProcessor{}.process(state, {});
    // 16 ly at W9 costs 10 mg; use the remaining 65/81 of a year at W4.
    assert(near(next.fleets.front().position.x, 16 + 16 * 65.0 / 81));
    assert(next.fleets.front().fuel == 12);
    state.fleets.front().fuel = 5;
    state.fleets.front().warp = 10;
    next = TurnProcessor{}.process(state, {});
    assert(near(next.fleets.front().position.x, 7 + 16 * .93));
    assert(near(next.fleets.front().damagePercent, 18 * .07));
    assert(next.fleets.front().fuel == 14);

    state.shipDesigns.front().components.front() = ShipComponentType::QuickJump5;
    state.fleets.front().warp = 9;
    state.fleets.front().fuel = 0;
    next = TurnProcessor{}.process(state, {});
    assert(near(next.fleets.front().position.x, 1));
    assert(next.fleets.front().fuel == 1);
}

void verify_default_propulsion_and_cargo()
{
    const auto state = suns::make_demo_game();
    const auto* scoutDesign = suns::find_ship_design(state, suns::kScoutDesignId);
    const auto* colonyDesign = suns::find_ship_design(state, suns::kColonyShipDesignId);
    assert(scoutDesign != nullptr);
    assert(colonyDesign != nullptr);

    assert(suns::ship_design_max_warp(*scoutDesign) == 8);
    assert(suns::ship_design_fuel_capacity(*scoutDesign) == 300.0);
    assert(suns::ship_design_fuel_capacity(*colonyDesign) == 400.0);
    assert(suns::ship_design_cargo_capacity(*colonyDesign) == 5.0);
    assert(suns::colonist_cargo_mass(25'000) == 2.5);

    suns::Fleet empty{
        2, 1, "Empty Colony Ship", suns::FleetRole::ColonyShip,
        suns::kColonyShipDesignId, {0.0, 0.0}, std::nullopt, 8, 400.0, 0,
    };
    auto loaded = empty;
    loaded.colonists = 25'000;

    assert(suns::fleet_gross_mass(state, loaded) > suns::fleet_gross_mass(state, empty));
    assert(suns::fleet_fuel_change_for_distance(state, loaded, 64.0)
        > suns::fleet_fuel_change_for_distance(state, empty, 64.0));
}

void verify_researched_warp_ten_drive()
{
    auto state = suns::make_demo_game();
    const auto highWarp = suns::ShipComponentType::HighWarpDrive;
    assert(!suns::component_available_to_player(state, 1, highWarp));
    state.players.front().technology.levels[
        static_cast<std::size_t>(suns::ResearchField::Propulsion)] = 2;
    assert(suns::component_available_to_player(state, 1, highWarp));

    const auto id = state.nextShipDesignId++;
    state.shipDesigns.push_back({id, 1, "Warp Ten Scout", suns::ShipHullType::Scout, {highWarp}});
    assert(suns::ship_design_valid(state.shipDesigns.back()));
    assert(suns::ship_design_max_warp(state.shipDesigns.back()) == 10);
    assert(near(suns::ship_design_overdrive_damage(state.shipDesigns.back(), 10), 0.0));
    assert(suns::ship_design_overdrive_damage(
        *suns::find_ship_design(state, suns::kScoutDesignId), 10) > 0.0);
    assert(suns::component_spec(highWarp).mass
        > suns::component_spec(suns::ShipComponentType::AdvancedFusionDrive).mass);

    auto& fleet = state.fleets.front();
    fleet.design = id;
    fleet.ships = {{id, 1}};
    fleet.fuel = suns::fleet_fuel_capacity(state, fleet);
    const auto start = fleet.position;
    const suns::PlayerOrders order{1, {suns::MoveFleetOrder{
        fleet.id, {start.x + 100.0, start.y}, 10}}};
    const auto result = suns::TurnProcessor{}.process(state, {order});
    assert(near(result.fleets.front().position.x - start.x, 100.0));
    assert(near(result.fleets.front().damagePercent, fleet.damagePercent));
}

void verify_fuel_limits_actual_travel()
{
    auto state = suns::make_demo_game();
    state.planets.clear();
    auto& scout = state.fleets.front();
    scout.position = {0.0, 0.0};
    scout.destination.reset();
    scout.warp = 8;
    scout.fuel = 10.0;

    suns::PlayerOrders orders{1, {}};
    orders.orders.emplace_back(suns::MoveFleetOrder{scout.id, {64.0, 0.0}, 8});

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {orders});
    const auto& moved = next.fleets.front();
    assert(moved.position.x > 0.0);
    assert(moved.position.x < 64.0);
    assert(near(moved.fuel, 0.0));
    assert(moved.destination.has_value());
}

void verify_ram_scoop_can_generate_fuel_in_flight()
{
    suns::GameState state;
    state.players.push_back({1, "Terrans", {}});
    state.shipDesigns.push_back({
        10, 1, "Scoop Test", suns::ShipHullType::Scout,
        {suns::ShipComponentType::RamScoopDrive},
    });
    state.fleets.push_back({
        1, 1, "Scoop", suns::FleetRole::Scout, 10,
        {0.0, 0.0}, suns::Position{50.0, 0.0}, 4, 0.0, 0,
    });

    assert(suns::ship_design_valid(state.shipDesigns.front()));
    assert(suns::fleet_fuel_rate(state, state.fleets.front()) < 0.0);

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {});
    const auto& scoop = next.fleets.front();
    assert(near(scoop.position.x, 16.0));
    assert(scoop.fuel > 0.0);
}

void verify_antimatter_generator_and_radiating_drive_metadata()
{
    suns::GameState state;
    state.players.push_back({1, "Terrans", {}});
    state.shipDesigns.push_back({
        20, 1, "Generator Test", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::AntimatterGenerator},
    });
    state.fleets.push_back({
        1, 1, "Generator", suns::FleetRole::Scout, 20,
        {25.0, 25.0}, std::nullopt, 8, 0.0, 0,
    });

    const auto* design = suns::find_ship_design(state, 20);
    assert(design != nullptr);
    assert(suns::ship_design_fuel_capacity(*design) == 500.0);
    assert(suns::ship_design_fuel_generation(*design) == 50.0);

    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {});
    assert(near(next.fleets.front().fuel, 50.0));

    suns::ShipDesign radiating{
        21, 1, "Radiating Test", suns::ShipHullType::Scout,
        {suns::ShipComponentType::RadiatingRamScoopDrive},
    };
    assert(suns::ship_design_valid(radiating));
    assert(suns::ship_design_radiation_hazard(radiating) > 0.0);
    assert(suns::ship_design_max_warp(radiating) == 9);
    assert(suns::ship_design_fuel_rate(radiating, 4) < 0.0);
}

void verify_advanced_fusion_drive()
{
    suns::ShipDesign courier{
        22, 1, "Fast Courier", suns::ShipHullType::Scout,
        {suns::ShipComponentType::AdvancedFusionDrive},
    };
    assert(suns::ship_design_valid(courier));
    assert(suns::ship_design_max_warp(courier) == 9);
    assert(suns::ship_design_radiation_hazard(courier) == 0.0);
    assert(suns::ship_design_fuel_rate(courier, 4) > 0.0);
    assert(suns::ship_design_fuel_rate(courier, 9) > 0.0);
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

void verify_unsafe_warp_accumulates_engine_damage()
{
    auto state = suns::make_demo_game();
    state.planets.clear();
    auto& scout = state.fleets.front();
    scout.position = {0.0, 0.0};
    scout.destination.reset();
    scout.fuel = suns::fleet_fuel_capacity(state, scout);

    assert(suns::fleet_max_warp(state, scout) == 8);
    assert(suns::fleet_warp_valid(state, scout, 10));
    assert(suns::fleet_overdrive_damage_rate(state, scout, 10) == 35.0);

    suns::PlayerOrders orders{1, {}};
    orders.orders.emplace_back(suns::MoveFleetOrder{scout.id, {100.0, 0.0}, 10});
    const suns::TurnProcessor processor;
    const auto next = processor.process(state, {orders});
    assert(near(next.fleets.front().position.x, 100.0));
    assert(near(next.fleets.front().damagePercent, 35.0));

    // A short leg exposes the engines for only part of the year.
    auto shortOrders = orders;
    std::get<suns::MoveFleetOrder>(shortOrders.orders.front()).destination = {50.0, 0.0};
    const auto shortLeg = processor.process(state, {shortOrders});
    assert(near(shortLeg.fleets.front().damagePercent, 17.5));

    // Critical damage stops travel at the point where hull integrity runs out.
    state.fleets.front().damagePercent = 82.5;
    const auto disabled = processor.process(state, {orders});
    assert(near(disabled.fleets.front().position.x, 50.0));
    assert(near(disabled.fleets.front().damagePercent, 100.0));
    assert(!disabled.fleets.front().destination);
    assert(!suns::fleet_warp_valid(disabled, disabled.fleets.front(), 1));
}

} // namespace

int main()
{
    verify_starting_scout_range();
    verify_built_fleets_start_at_engine_cruise_warp();
    verify_original_fuel_tables_and_rounding();
    verify_original_collection_is_per_engine();
    verify_mixed_fleet_cargo_and_fuel_collection();
    verify_reference_fuel_exhaustion_and_free_speed();
    verify_stars_progression_and_multifield_gates();
    verify_stars_speed_bands_and_settler_restriction();
    verify_warp_squared_movement();
    verify_default_propulsion_and_cargo();
    verify_researched_warp_ten_drive();
    verify_fuel_limits_actual_travel();
    verify_ram_scoop_can_generate_fuel_in_flight();
    verify_antimatter_generator_and_radiating_drive_metadata();
    verify_advanced_fusion_drive();
    verify_invalid_warp_order_is_rejected();
    verify_unsafe_warp_accumulates_engine_damage();
    return 0;
}
