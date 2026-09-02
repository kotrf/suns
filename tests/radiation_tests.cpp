#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>

int main()
{
    suns::GameState state = suns::make_demo_game();
    state.shipDesigns.push_back({
        3,
        1,
        "Radiating Transport",
        suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::RadiatingRamScoopDrive,
         suns::ShipComponentType::RadiatingRamScoopDrive,
         suns::ShipComponentType::ColonyModule},
    });

    suns::Fleet radiating{
        2,
        1,
        "Radiating Transport 2",
        suns::FleetRole::ColonyShip,
        3,
        {0.0, 0.0},
        suns::Position{16.0, 0.0},
        4,
        0.0,
        1000,
    };

    assert(!suns::fleet_radiation_safe(state, radiating));
    assert(suns::projected_fleet_radiation_losses(state, radiating) == 100);

    auto tolerant = state;
    tolerant.players.front().race.radiationTolerance = suns::kRadiatingDriveSafeTolerance;
    assert(suns::fleet_radiation_safe(tolerant, radiating));
    assert(suns::projected_fleet_radiation_losses(tolerant, radiating) == 0);

    auto immune = state;
    immune.players.front().race.radiationImmune = true;
    assert(suns::fleet_radiation_safe(immune, radiating));

    // Attrition happens only when the radiating engine actually moves the ship.
    state.fleets = {radiating};
    const suns::TurnProcessor processor;
    const auto travelled = processor.process(state, {});
    assert(travelled.fleets.front().colonists == 900);
    assert(suns::same_position(travelled.fleets.front().position, {16.0, 0.0}));

    auto stationary = state;
    stationary.fleets.front().destination.reset();
    const auto parked = processor.process(stationary, {});
    assert(parked.fleets.front().colonists == 1000);

    return 0;
}
