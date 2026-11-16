#pragma once
#include "drifting_object_fixture.hpp"
namespace suns::test {
inline GameState rogue_planetoid_fixture()
{
    auto s=drifting_object_fixture();
    s.unknownObjects.front().kind=UnknownObjectKind::RoguePlanetoid;
    s.unknownObjects.front().velocity={0.5,0.25};
    return s;
}
}
