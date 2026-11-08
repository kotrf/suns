#pragma once
#include "suns/game_state.hpp"

namespace suns {
// Population/cargo foundation. Setup stays disabled until the paid starting
// economy and Computer expansion acceptance are ready.
[[nodiscard]] bool native_orbital_race(const GameState&, PlayerId);
[[nodiscard]] std::uint64_t native_orbital_capacity(OrbitalStationHullType);
[[nodiscard]] const OrbitalStation* native_orbital_station(const GameState&, const Planet&);
// Partial arrival unload: excess stays aboard. Does not move minerals/found a colony.
std::uint64_t unload_native_orbital_colonists(GameState&, Planet&, Fleet&);
// Actual battle destruction only. Paid refits never call this operation.
void destroy_native_orbital_population(GameState&, OrbitalStationId);
}
