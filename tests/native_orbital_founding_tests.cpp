#include "native_orbital_fixture.hpp"
#include "orbital_base_fixture.hpp"
#include "suns/equipment.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

using namespace suns;
namespace {
bool same_minerals(const MineralCargo& a, const MineralCargo& b)
{
    return std::abs(a.ironium-b.ironium) < 1e-8 && std::abs(a.boranium-b.boranium) < 1e-8
        && std::abs(a.germanium-b.germanium) < 1e-8;
}
bool same_ships(const Fleet& a, const Fleet& b)
{
    return a.ships.size() == b.ships.size() && std::equal(a.ships.begin(),a.ships.end(),b.ships.begin(),
        [](const auto& x,const auto& y){return x.design == y.design && x.count == y.count;});
}
void kit_is_funded_at_manufacture_and_trait_specific()
{
    auto state = test::native_orbital_founding_fixture();
    const auto* reference = equipment_technology(ShipComponentType::OrbitalConstructionModule);
    assert(reference && reference->cost == 20 && same_minerals(reference->minerals, {20, 15, 15}));
    const auto spec = component_spec(reference->component);
    assert(spec.buildCost == 60 && spec.enablesColonization && spec.mass == 50);
    assert(same_minerals(component_mineral_cost(reference->component), {44, 34, 37}));
    const auto& settler = state.shipDesigns.back();
    assert(ship_design_valid(settler) && ship_design_available_to_player(state, 1, settler));
    assert(ship_design_cost(settler) == 83 && same_minerals(ship_design_mineral_cost(settler), {57, 34, 53}));
    assert(fleet_can_colonize(state, state.fleets.back()));
    assert(fleet_can_found_native_orbit(state, state.fleets.back()));
    assert(same_minerals(fleet_colonization_salvage(state, state.fleets.back()), {11, 5, 10}));
    auto unload = state;
    unload.fleets.back().destination = unload.fleets.back().position;
    unload.fleets.back().arrivalAction = FleetArrivalAction{FleetArrivalActionKind::UnloadAll, 0, FleetCargoKind::All};
    unload = TurnProcessor{}.process(unload, {});
    assert(!unload.planets[1].nativeOrbit.station && !unload.planets[1].population && unload.orbitalStations.size() == 1);
    assert(unload.fleets.back().colonists == 1000);
    auto ordinary = state; ordinary.players.front().race.primaryTrait = PrimaryRaceTrait::Generalist;
    assert(!component_available_to_player(ordinary, 1, reference->component));
    assert(!fleet_can_colonize(ordinary, ordinary.fleets.back()));
    state.shipDesigns.back().components.back() = ShipComponentType::StarsColonizationModule;
    assert(!fleet_can_colonize(state, state.fleets.back()));
}
void paid_manufacture_transport_and_arrival_found_a_second_colony()
{
    auto state = test::empty_native_orbital_start();
    const auto home = state.planets.front().id, target = state.planets[1].id;
    assert(initialize_native_orbital_start(state, 1, home, native_orbital_start_budget()));
    state.planets[1].precursorArtifacts.present = true;
    state.planets[1].precursorArtifacts.researchPoints = 100;
    const auto destination = Position{state.stars.front().position.x+25, state.stars.front().position.y};
    state.stars[1].position = destination;
    const PlayerOrders build{1, {SetResearchPlanOrder{ResearchField::Energy, {}, false},
        CreateShipDesignOrder{"Orbital Settler", ShipHullType::ColonyShip,
            {ShipComponentType::QuickJump5, ShipComponentType::OrbitalConstructionModule}},
        QueueShipDesignOrder{home, 0, "Orbital Settler"}}};
    state = TurnProcessor{}.process(state, {build});
    assert(state.fleets.size() == 1 && state.planets.front().productionQueue.front().remainingCost == 33);
    state = TurnProcessor{}.process(state, {});
    assert(state.fleets.size() == 2 && state.planets.front().productionQueue.empty());
    assert(same_minerals(state.planets.front().minerals, {51, 70, 49}));
    const auto id = state.fleets.back().id;
    const auto next = TurnProcessor{}.process_with_events(state, {{1, {
        TransferCargoOrder{{home, 0, true}, {0, id}, 1000, {}},
        MoveFleetOrder{id, destination, 5, {FleetArrivalActionKind::Colonize}}}}});
    state = next.state;
    const auto* colony = find_planet(state, target); assert(colony);
    assert(colony->owner == 1 && colony->nativeOrbit.population == 1000 && !colony->population);
    assert(!colony->industry && !colony->mines && !colony->habitat.population);
    assert(state.planets.front().nativeOrbit.population == 24'000);
    assert(state.fleets.size() == 1 && state.orbitalStations.size() == 2);
    const auto* station = native_orbital_station(state, *colony);
    assert(station && station->hull == OrbitalStationHullType::OrbitalDock);
    assert(colony->radar == PlanetaryRadarLevel::Full && !colony->homeworld);
    assert(colony_has_orbital_service(state, target, 1, OrbitalStationModule::Shipyard));
    assert(colony_has_orbital_service(state, target, 1, OrbitalStationModule::RefuelingDepot));
    assert(same_minerals(colony->minerals, {21, 15, 20}));
    assert(colony_output(state, *colony) == 10);
    assert(!colony->precursorArtifacts.claimed && technology_level(state, 1, ResearchField::Energy) == 0);
    assert(state.players.front().history.back().population == kInitialHomePopulation);
    assert(state.players.front().history.back().colonies == 2);
    assert(std::count_if(next.events.begin(), next.events.end(), [](const auto& e) { return e.kind == GameEventKind::ColonyFounded; }) == 1);
    const auto again = TurnProcessor{}.process(state, {{1, {ColonizePlanetOrder{id, target}}}});
    assert(again.orbitalStations.size() == 2 && same_minerals(find_planet(again, target)->minerals, colony->minerals));
}
void manual_and_arrival_paths_share_guards()
{
    for (const bool arrival : {false, true}) for (unsigned defect = 0; defect < 11; ++defect) {
        auto state = test::native_orbital_founding_fixture();
        auto& world = state.planets[1]; auto& fleet = state.fleets.back();
        if (defect == 0) state.shipDesigns.back().components.back() = ShipComponentType::StarsColonizationModule;
        if (defect == 1) world.owner = 1;
        if (defect == 2) { state.players.push_back({2, "Other"}); world.owner = 2; }
        if (defect == 3) fleet.colonists = 0;
        if (defect == 4) fleet.minerals.ironium = 1000; // Overloaded, not extra founding credit.
        if (defect == 5) fleet.position.x += 1;
        if (defect == 6) world.habitat.population = 1;
        if (defect == 7) state.nextOrbitalStationId = std::numeric_limits<OrbitalStationId>::max();
        if (defect == 8) state.orbitalStations.push_back({state.nextOrbitalStationId++, 1, world.id, "Existing"});
        if (defect == 9) { fleet.ships.front().count = 50; fleet.colonists = 100'001; }
        if (defect == 10) state.nextOrbitalStationId = state.orbitalStations.front().id;
        const auto id = fleet.id, target = world.id;
        const auto residents = fleet.colonists;
        const auto stations = state.orbitalStations.size();
        const auto counter = state.nextOrbitalStationId;
        std::vector<PlayerOrders> orders;
        if (arrival) { fleet.destination = fleet.position; fleet.arrivalAction = FleetArrivalAction{FleetArrivalActionKind::Colonize}; }
        else orders = {{1, {ColonizePlanetOrder{id, target}}}};
        const auto next = TurnProcessor{}.process(state, orders);
        assert(next.orbitalStations.size() == stations && next.nextOrbitalStationId == counter);
        assert(!find_planet(next, target)->nativeOrbit.station);
        const auto survivor = std::find_if(next.fleets.begin(), next.fleets.end(), [=](const auto& f) { return f.id == id; });
        assert(survivor != next.fleets.end() && survivor->colonists == residents);
        if (arrival) assert(survivor->arrivalAction && survivor->arrivalAction->kind == FleetArrivalActionKind::Colonize);
    }
}
void mixed_fleet_supplies_are_not_minted_from_repeated_kits()
{
    auto state = test::native_orbital_founding_fixture();
    auto& fleet = state.fleets.back(); fleet.ships.front().count = 2;
    const auto salvage = fleet_colonization_salvage(state, fleet);
    assert(same_minerals(salvage, {22, 10, 20}));
    const auto id = fleet.id, target = state.planets[1].id;
    const auto next = TurnProcessor{}.process(state, {{1, {ColonizePlanetOrder{id, target}, ColonizePlanetOrder{id, target}}}});
    assert(next.fleets.size() == 1 && next.orbitalStations.size() == 2);
    assert(same_minerals(find_planet(next, target)->minerals, {35, 22, 31}));
    assert(find_planet(next, target)->nativeOrbit.population == 1000);
    state = test::native_orbital_founding_fixture();
    state.shipDesigns.push_back({91, 1, "Surface settler", ShipHullType::ColonyShip,
        {ShipComponentType::QuickJump5, ShipComponentType::StarsColonizationModule}});
    state.nextShipDesignId = 92;
    state.fleets.back().ships.insert(state.fleets.back().ships.begin(), {91, 1});
    const auto mixed = TurnProcessor{}.process_with_events(state, {{1, {ColonizePlanetOrder{id, target}}}});
    assert(mixed.state.fleets.size() == 1 && mixed.state.orbitalStations.size() == 2);
    assert(same_minerals(find_planet(mixed.state, target)->minerals, {32, 20, 30}));
    const auto report = std::find_if(mixed.events.begin(), mixed.events.end(), [](const auto& e) { return e.kind == GameEventKind::ColonyFounded; });
    assert(report != mixed.events.end() && report->shipDesign == 90);
}
void deployed_shipyards_block_both_founding_paths()
{
    for (const bool arrival : {false, true}) for (const PlayerId owner : {1U, 2U}) {
        auto state = test::native_orbital_founding_fixture();
        const auto founder = state.fleets.back();
        const auto target = state.planets[1].id;
        if (owner == 2) state.players.push_back({2, "Other empire"});
        auto yardWorld = orbital_test::world();
        auto design = *find_ship_design(yardWorld, 200);
        design.id = state.nextShipDesignId++; design.owner = owner;
        state.shipDesigns.push_back(design);
        auto yard = yardWorld.fleets.front();
        yard.id = state.nextFleetId++; yard.owner = owner; yard.design = design.id;
        yard.ships = {{design.id, 1}}; yard.position = founder.position; yard.telemetry = {};
        state.fleets.push_back(yard);
        apply_orbital_base_command(state, state.fleets.back(), {OrbitalBaseAction::Deploy, target});
        assert(orbital_base_at(state, target) && !state.planets[1].owner);
        assert(!find_orbital_station_at_planet(state, target));
        const Order order = arrival ? Order{MoveFleetOrder{founder.id, founder.position, 5,
            {FleetArrivalActionKind::Colonize}}} : Order{ColonizePlanetOrder{founder.id, target}};
        const auto next = TurnProcessor{}.process(state, {{1, {order}}});
        assert(!find_planet(next, target)->owner && !find_planet(next, target)->nativeOrbit.station);
        assert(next.orbitalStations.size() == state.orbitalStations.size()
            && next.nextOrbitalStationId == state.nextOrbitalStationId);
        assert(orbital_base_at(next, target) && orbital_base_at(next, target)->id == yard.id);
        const auto survivor = std::find_if(next.fleets.begin(), next.fleets.end(),
            [&](const auto& f) { return f.id == founder.id; });
        assert(survivor != next.fleets.end() && survivor->colonists == founder.colonists
            && same_minerals(survivor->minerals, founder.minerals) && same_ships(*survivor,founder));
    }
}
void incompatible_carried_plans_reject_before_founding()
{
    for (const bool arrival : {false, true}) {
        for (const auto kind : {ProductionKind::Factory, ProductionKind::Mine, ProductionKind::AdvancedFactory,
            ProductionKind::AdvancedMine, ProductionKind::UpgradeFactory, ProductionKind::UpgradeMine,
            ProductionKind::ReconstructFactory, ProductionKind::ReconstructMine}) {
            auto state = test::native_orbital_founding_fixture();
            const auto founder = state.fleets.back(); const auto target = state.planets[1].id;
            ProductionTemplate plan{"Surface first", {{ProductionKind::LocalRadar,production_cost(ProductionKind::LocalRadar)},
                {kind, production_cost(kind)}}};
            // Reconstruction is already structurally excluded from templates;
            // the other six kinds require the native applicability guard.
            assert(production_template_valid(plan) == !reconstruction_kind(kind));
            FleetArrivalAction action{FleetArrivalActionKind::Colonize}; action.colonyPlan = plan;
            const Order order = arrival ? Order{MoveFleetOrder{founder.id, founder.position, 5, action}}
                : Order{ColonizePlanetOrder{founder.id, target, plan}};
            const auto next = TurnProcessor{}.process(state, {{1, {order}}});
            const auto* world = find_planet(next, target);
            assert(!world->owner && !world->nativeOrbit.station && world->productionQueue.empty());
            assert(same_minerals(world->minerals, state.planets[1].minerals));
            assert(next.orbitalStations.size() == state.orbitalStations.size()
                && next.nextOrbitalStationId == state.nextOrbitalStationId);
            const auto survivor = std::find_if(next.fleets.begin(), next.fleets.end(),
                [&](const auto& f) { return f.id == founder.id; });
            assert(survivor != next.fleets.end() && survivor->colonists == founder.colonists
                && same_minerals(survivor->minerals, founder.minerals) && same_ships(*survivor,founder));
        }
        auto state = test::native_orbital_founding_fixture();
        state.planets[1].minerals = {100, 100, 100};
        const auto founder = state.fleets.back();
        ProductionTemplate plan{"Receiver", {{ProductionKind::LocalRadar,production_cost(ProductionKind::LocalRadar)}}};
        assert(production_template_valid(plan));
        FleetArrivalAction action{FleetArrivalActionKind::Colonize}; action.colonyPlan = plan;
        const Order order = arrival ? Order{MoveFleetOrder{founder.id, founder.position, 5, action}}
            : Order{ColonizePlanetOrder{founder.id, state.planets[1].id, plan}};
        const auto next = TurnProcessor{}.process(state, {{1, {order}}});
        assert(next.planets[1].nativeOrbit.population == 1000 && next.planets[1].radar == PlanetaryRadarLevel::Full);
        assert(next.planets[1].productionQueue.empty()); // The prepaid full receiver already satisfies this plan.
    }
}
}
int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "orbit") { deployed_shipyards_block_both_founding_paths(); return 0; }
    if (argc == 2 && std::string(argv[1]) == "plan") { incompatible_carried_plans_reject_before_founding(); return 0; }
    kit_is_funded_at_manufacture_and_trait_specific();
    paid_manufacture_transport_and_arrival_found_a_second_colony();
    manual_and_arrival_paths_share_guards();
    mixed_fleet_supplies_are_not_minted_from_repeated_kits();
    deployed_shipyards_block_both_founding_paths();
    incompatible_carried_plans_reject_before_founding();
}
