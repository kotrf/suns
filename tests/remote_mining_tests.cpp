#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>
#include <utility>

namespace {

const suns::Planet& planet(const suns::GameState& state, suns::PlanetId id)
{
    for (const auto& candidate : state.planets) if (candidate.id == id) return candidate;
    assert(false);
    return state.planets.front();
}

const suns::Fleet& fleet(const suns::GameState& state, suns::FleetId id)
{
    for (const auto& candidate : state.fleets) if (candidate.id == id) return candidate;
    assert(false);
    return state.fleets.front();
}

bool close(double a, double b)
{
    return std::abs(a - b) < 0.000001;
}

suns::GameState advance_until_task(
    const suns::TurnProcessor& processor,
    suns::GameState state,
    suns::FleetId id,
    suns::FleetTask task)
{
    for (int turn = 0; turn < 12 && fleet(state, id).task != task; ++turn) {
        state = processor.process(state, {});
    }
    assert(fleet(state, id).task == task);
    return state;
}

} // namespace

int main()
{
    auto state = suns::make_demo_game();
    state.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 1;
    state.shipDesigns.push_back({
        3, 1, "Remote Miner", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::RemoteMiningModule},
    });
    state.shipDesigns.push_back({
        4, 1, "Ore Hauler", suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::FusionDrive},
    });

    const auto* alpha = suns::find_star(state, 2);
    assert(alpha != nullptr);
    state.fleets = {
        {2, 1, "Miner 2", suns::FleetRole::Scout, 3, alpha->position, {}, 1, 300.0, 0},
        {3, 1, "Hauler 3", suns::FleetRole::Scout, 4, alpha->position, {}, 1, 300.0, 0},
    };

    const auto expected = suns::projected_remote_mining(state, planet(state, 2), state.shipDesigns[2]);
    const suns::TurnProcessor processor;

    // Merely arriving in orbit does not start a mining operation.
    const auto first = processor.process(state, {});
    assert(close(suns::mineral_cargo_mass(planet(first, 2).minerals), 0.0));
    assert(fleet(first, 2).task == suns::FleetTask::None);

    suns::PlayerOrders startMining{1, {suns::SetRemoteMiningOrder{2, true}}};
    auto started = processor.process(first, {startMining});
    started = advance_until_task(processor, std::move(started), 2, suns::FleetTask::RemoteMining);
    if (close(suns::mineral_cargo_mass(planet(started, 2).minerals), 0.0)) {
        started = processor.process(started, {});
    }
    assert(close(planet(started, 2).minerals.ironium, expected.ironium));
    assert(close(planet(started, 2).minerals.boranium, expected.boranium));
    assert(close(planet(started, 2).minerals.germanium, expected.germanium));
    assert(fleet(started, 2).task == suns::FleetTask::RemoteMining);
    assert(close(suns::mineral_cargo_mass(fleet(first, 2).minerals), 0.0));

    // The assigned task remains active without being queued every year.
    const auto continued = processor.process(started, {});
    assert(close(planet(continued, 2).minerals.ironium, expected.ironium * 2.0));

    // A transport, not the miner, may take the accumulated surface stock.
    suns::PlayerOrders collect{1, {}};
    const suns::MineralCargo accumulated{
        expected.ironium * 2.0,
        expected.boranium * 2.0,
        expected.germanium * 2.0,
    };
    collect.orders.emplace_back(suns::SetFleetMineralCargoOrder{2, 3, accumulated});
    const auto collected = processor.process(continued, {collect});
    assert(close(fleet(collected, 3).minerals.ironium, accumulated.ironium));
    assert(close(fleet(collected, 3).minerals.boranium, accumulated.boranium));
    assert(close(fleet(collected, 3).minerals.germanium, accumulated.germanium));
    // The miner continues to replenish the surface stock on the collection turn.
    assert(close(planet(collected, 2).minerals.ironium, expected.ironium));
    assert(close(planet(collected, 2).minerals.boranium, expected.boranium));
    assert(close(planet(collected, 2).minerals.germanium, expected.germanium));

    // The player can stop the task without moving the fleet.
    suns::PlayerOrders stopMining{1, {suns::SetRemoteMiningOrder{2, false}}};
    auto stopped = processor.process(collected, {stopMining});
    stopped = advance_until_task(processor, std::move(stopped), 2, suns::FleetTask::None);
    assert(fleet(stopped, 2).task == suns::FleetTask::None);
    const auto surfaceAfterStop = planet(stopped, 2).minerals;
    const auto idle = processor.process(stopped, {});
    assert(close(planet(idle, 2).minerals.ironium, surfaceAfterStop.ironium));
    assert(close(planet(idle, 2).minerals.boranium, surfaceAfterStop.boranium));
    assert(close(planet(idle, 2).minerals.germanium, surfaceAfterStop.germanium));

    // A movement command cancels the task when it reaches the fleet, and the
    // fleet does not silently resume mining after the trip.
    auto restarted = processor.process(idle, {startMining});
    restarted = advance_until_task(processor, std::move(restarted), 2, suns::FleetTask::RemoteMining);
    suns::PlayerOrders depart{1, {suns::MoveFleetOrder{
        2,
        {alpha->position.x + 40.0, alpha->position.y},
        8,
    }}};
    auto departing = processor.process(restarted, {depart});
    for (int turn = 0; turn < 12 && fleet(departing, 2).task == suns::FleetTask::RemoteMining; ++turn) {
        departing = processor.process(departing, {});
    }
    assert(fleet(departing, 2).task == suns::FleetTask::None);
    for (int turn = 0; turn < 12 && fleet(departing, 2).destination; ++turn) {
        departing = processor.process(departing, {});
    }
    assert(!fleet(departing, 2).destination);
    const auto afterArrival = processor.process(departing, {});
    assert(fleet(afterArrival, 2).task == suns::FleetTask::None);

    // The 80 kt apparatus makes a miner substantially more fuel-hungry in transit.
    suns::ShipDesign light{
        5, 1, "Light Scout", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive},
    };
    state.shipDesigns.push_back(light);
    suns::Fleet lightFleet{4, 1, "Light 4", suns::FleetRole::Scout, 5, alpha->position, {}, 8, 300.0, 0};
    auto heavyFleet = state.fleets.front();
    heavyFleet.warp = 8;
    heavyFleet.fuel = 300.0;
    assert(suns::component_spec(suns::ShipComponentType::RemoteMiningModule).mass == 80.0);
    assert(suns::fleet_fuel_change_for_distance(state, heavyFleet, 64.0)
        > suns::fleet_fuel_change_for_distance(state, lightFleet, 64.0) * 2.5);

    // Construction 1 is an actual gate, not merely a Ship Designer hint.
    auto locked = state;
    locked.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 0;
    auto noMining = processor.process(locked, {startMining});
    for (int turn = 0; turn < 12 && !fleet(noMining, 2).pendingCommands.empty(); ++turn) {
        noMining = processor.process(noMining, {});
    }
    assert(close(suns::mineral_cargo_mass(planet(noMining, 2).minerals), 0.0));
    assert(fleet(noMining, 2).task == suns::FleetTask::None);
}
