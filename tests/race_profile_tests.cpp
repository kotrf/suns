#include "suns/game_state.hpp"

#include <cassert>

int main()
{
    auto state = suns::make_demo_game();
    auto& race = state.players.front().race;
    assert(race.primaryTrait == suns::PrimaryRaceTrait::Generalist);
    assert(race.radiationTolerance == 0.50);
    assert(!race.radiationImmune);
    assert(race.habitableTemperature.minimum == 25);
    assert(race.habitableTemperature.maximum == 75);
    assert(race.habitableGravity.minimum == 30);
    assert(race.habitableGravity.maximum == 70);
    assert(race.habitableRadiation.minimum == 0);
    assert(race.habitableRadiation.maximum == 40);

    race.primaryTrait = suns::PrimaryRaceTrait::StargateSpecialist;
    race.radiationTolerance = 0.85;
    race.radiationImmune = true;

    const auto copy = state;
    assert(copy.players.front().race.primaryTrait
        == suns::PrimaryRaceTrait::StargateSpecialist);
    assert(copy.players.front().race.radiationTolerance == 0.85);
    assert(copy.players.front().race.radiationImmune);

    return 0;
}
