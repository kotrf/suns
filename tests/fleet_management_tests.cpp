#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>

namespace {

using namespace suns;

const Fleet* find_fleet(const GameState& state, FleetId id)
{
    for (const auto& fleet : state.fleets) if (fleet.id == id) return &fleet;
    return nullptr;
}

bool close(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 0.000001;
}

GameState two_fleet_fixture()
{
    auto state = make_demo_game();
    auto& scout = state.fleets.front();
    scout.ships = {{kScoutDesignId, 2}};
    scout.fuel = 100.0;
    scout.colonists = 100;
    scout.minerals = {0.1, 0.1, 0.1};

    state.fleets.push_back({
        2,
        1,
        "Colony Group",
        FleetRole::ColonyShip,
        kColonyShipDesignId,
        scout.position,
        std::nullopt,
        kColonyShipCruiseWarp,
        200.0,
        200,
    });
    state.fleets.back().ships = {{kColonyShipDesignId, 1}};
    state.fleets.back().minerals = {0.2, 0.2, 0.2};
    state.nextFleetId = 3;
    return state;
}

void derived_properties_use_the_whole_composition()
{
    auto state = two_fleet_fixture();
    Fleet mixed = state.fleets.front();
    mixed.ships = {{kScoutDesignId, 2}, {kColonyShipDesignId, 1}};

    const auto* scout = find_ship_design(state, kScoutDesignId);
    const auto* colony = find_ship_design(state, kColonyShipDesignId);
    assert(scout && colony);
    assert(fleet_ship_count(mixed) == 3);
    assert(fleet_ship_count(mixed, kScoutDesignId) == 2);
    assert(fleet_can_colonize(state, mixed));
    assert(fleet_max_warp(state, mixed)
        == std::min(ship_design_max_warp(*scout), ship_design_max_warp(*colony)));
    assert(close(fleet_speed(state, mixed),
        std::min(ship_design_speed(*scout), ship_design_speed(*colony))));
    assert(close(fleet_sensor_range(state, mixed), ship_design_sensor_range(*scout)));
    assert(close(fleet_fuel_capacity(state, mixed),
        ship_design_fuel_capacity(*scout) * 2.0 + ship_design_fuel_capacity(*colony)));
    assert(close(fleet_cargo_capacity(state, mixed),
        ship_design_cargo_capacity(*scout) * 2.0 + ship_design_cargo_capacity(*colony)));
}

void remote_mining_output_sums_ship_counts()
{
    const TurnProcessor processor;
    auto state = make_demo_game();
    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 1;
    state.shipDesigns.push_back({
        3,
        1,
        "Remote Miner",
        ShipHullType::RemoteMiner,
        {ShipComponentType::FusionDrive, ShipComponentType::FusionDrive,
         ShipComponentType::RemoteMiningModule},
    });
    auto& miners = state.fleets.front();
    miners.design = 3;
    miners.ships = {{3, 2}};
    miners.position = state.stars[1].position;
    miners.task = FleetTask::RemoteMining;
    const auto expected = projected_remote_mining(state, state.planets[1], state.shipDesigns.back());

    const auto result = processor.process(state, {});
    assert(close(result.planets[1].minerals.ironium, expected.ironium * 2.0));
    assert(close(result.planets[1].minerals.boranium, expected.boranium * 2.0));
    assert(close(result.planets[1].minerals.germanium, expected.germanium * 2.0));
}

void merge_preserves_destination_id_and_all_stacks()
{
    const TurnProcessor processor;
    auto state = two_fleet_fixture();
    PlayerOrders orders{1, {MergeFleetsOrder{1, 2}}};
    const auto merged = processor.process(state, {orders});

    assert(merged.fleets.size() == 1);
    const auto* fleet = find_fleet(merged, 1);
    assert(fleet);
    assert(find_fleet(merged, 2) == nullptr);
    assert(fleet_ship_count(*fleet) == 3);
    assert(fleet_ship_count(*fleet, kScoutDesignId) == 2);
    assert(fleet_ship_count(*fleet, kColonyShipDesignId) == 1);
    assert(close(fleet->fuel, fleet_fuel_capacity(merged, *fleet)));
    assert(fleet->colonists == 300);
    assert(close(fleet->minerals.ironium, 0.3));
    assert(close(fleet->minerals.boranium, 0.3));
    assert(close(fleet->minerals.germanium, 0.3));
}

void split_preserves_source_id_and_resources()
{
    const TurnProcessor processor;
    auto state = two_fleet_fixture();
    state = processor.process(state, {{1, {MergeFleetsOrder{1, 2}}}});
    const auto before = *find_fleet(state, 1);

    const auto split = processor.process(state, {{1, {SplitFleetOrder{1, {{kScoutDesignId, 1}}}}}});
    const auto* source = find_fleet(split, 1);
    const auto* detached = find_fleet(split, 3);
    assert(source && detached);
    assert(fleet_ship_count(*source) == 2);
    assert(fleet_ship_count(*source, kScoutDesignId) == 1);
    assert(fleet_ship_count(*source, kColonyShipDesignId) == 1);
    assert(fleet_ship_count(*detached) == 1);
    assert(fleet_ship_count(*detached, kScoutDesignId) == 1);
    assert(close(source->fuel + detached->fuel, before.fuel));
    assert(source->colonists + detached->colonists == before.colonists);
    assert(close(source->minerals.ironium + detached->minerals.ironium, before.minerals.ironium));
    assert(close(source->minerals.boranium + detached->minerals.boranium, before.minerals.boranium));
    assert(close(source->minerals.germanium + detached->minerals.germanium, before.minerals.germanium));
    assert(fleet_cargo_used(split, *source) <= fleet_cargo_capacity(split, *source) + 0.000001);
    assert(fleet_cargo_used(split, *detached) <= fleet_cargo_capacity(split, *detached) + 0.000001);
}

void moving_fleets_cannot_be_reorganized()
{
    const TurnProcessor processor;
    auto state = two_fleet_fixture();
    state.fleets.front().destination = Position{10.0, 0.0};
    const auto result = processor.process(state, {{1, {MergeFleetsOrder{1, 2}}}});
    assert(result.fleets.size() == 2);
    assert(find_fleet(result, 1));
    assert(find_fleet(result, 2));
}

void colonization_dismantles_entire_fleet_and_recovers_minerals()
{
    const TurnProcessor processor;
    auto state = make_demo_game();
    auto& fleet = state.fleets.front();
    fleet.position = state.stars[1].position;
    fleet.ships = {{kScoutDesignId, 1}, {kColonyShipDesignId, 1}};
    fleet.colonists = 100;
    fleet.minerals = {0.5, 0.5, 0.5};
    set_survey_level(state, 1, 2, SurveyLevel::OrbitalSurvey, state.turn);
    const auto salvage = fleet_colonization_salvage(state, fleet);
    const auto before = state.planets[1].minerals;

    const auto result = processor.process(state, {{1, {ColonizePlanetOrder{1, 2}}}});
    assert(find_fleet(result, 1) == nullptr);
    assert(result.planets[1].owner == 1);
    assert(result.planets[1].population >= 100);
    assert(close(result.planets[1].minerals.ironium, before.ironium + 0.5 + salvage.ironium));
    assert(close(result.planets[1].minerals.boranium, before.boranium + 0.5 + salvage.boranium));
    assert(close(result.planets[1].minerals.germanium, before.germanium + 0.5 + salvage.germanium));
}

} // namespace

int main()
{
    derived_properties_use_the_whole_composition();
    remote_mining_output_sums_ship_counts();
    merge_preserves_destination_id_and_all_stacks();
    split_preserves_source_id_and_resources();
    moving_fleets_cannot_be_reorganized();
    colonization_dismantles_entire_fleet_and_recovers_minerals();
}
