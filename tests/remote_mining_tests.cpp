#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>

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
        {2, 1, "Miner 2", suns::FleetRole::Scout, 3, alpha->position, {}, 1, 0.0, 0},
        {3, 1, "Hauler 3", suns::FleetRole::Scout, 4, alpha->position, {}, 1, 0.0, 0},
    };

    const auto expected = suns::projected_remote_mining(state, planet(state, 2), state.shipDesigns[2]);
    const suns::TurnProcessor processor;
    const auto first = processor.process(state, {});
    assert(close(planet(first, 2).minerals.ironium, expected.ironium));
    assert(close(planet(first, 2).minerals.boranium, expected.boranium));
    assert(close(planet(first, 2).minerals.germanium, expected.germanium));
    assert(close(suns::mineral_cargo_mass(fleet(first, 2).minerals), 0.0));

    // A transport, not the miner, may take the accumulated surface stock.
    suns::PlayerOrders collect{1, {}};
    collect.orders.emplace_back(suns::SetFleetMineralCargoOrder{2, 3, expected});
    const auto collected = processor.process(first, {collect});
    assert(close(fleet(collected, 3).minerals.ironium, expected.ironium));
    assert(close(fleet(collected, 3).minerals.boranium, expected.boranium));
    assert(close(fleet(collected, 3).minerals.germanium, expected.germanium));
    // The miner continues to replenish the surface stock on the collection turn.
    assert(close(planet(collected, 2).minerals.ironium, expected.ironium));
    assert(close(planet(collected, 2).minerals.boranium, expected.boranium));
    assert(close(planet(collected, 2).minerals.germanium, expected.germanium));

    // Construction 1 is an actual gate, not merely a Ship Designer hint.
    auto locked = state;
    locked.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 0;
    const auto noMining = processor.process(locked, {});
    assert(close(suns::mineral_cargo_mass(planet(noMining, 2).minerals), 0.0));
}
