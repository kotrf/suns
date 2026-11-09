#pragma once
#include "suns/game_state.hpp"

namespace suns {
// Population/cargo foundation. Setup stays disabled until the paid starting
// economy and Computer expansion acceptance are ready.
[[nodiscard]] bool native_orbital_race(const GameState&, PlayerId);
[[nodiscard]] std::uint64_t native_orbital_capacity(OrbitalStationHullType);
[[nodiscard]] const OrbitalStation* native_orbital_station(const GameState&, const Planet&);
// Prototype orbital industry: floor(sqrt(supported residents * (Energy + 1) / 10)).
// Surface factories/population never contribute. Radiation/support comes next.
[[nodiscard]] std::uint32_t native_orbital_output(const GameState&, const Planet&);

struct NativeOrbitalStartBudget {
    std::uint64_t population{kInitialHomePopulation};
    std::uint32_t production{};
    MineralCargo minerals;
};
struct NativeOrbitalStartReceipt {
    OrbitalStationId station{};
    ShipDesignId minerDesign{};
    FleetId minerFleet{};
    std::uint32_t productionSpent{};
    MineralCargo mineralsSpent;
};
// Funds the ordinary-priced Dock/services, full receiver, basic miner and
// 100 kt of each mineral left in stock. No research levels are granted.
[[nodiscard]] NativeOrbitalStartBudget native_orbital_start_budget();
// Host construction kernel for an empty empire/world at turn one, not a
// campaign-availability bypass. Rejects atomically, including repeat starts.
[[nodiscard]] std::optional<NativeOrbitalStartReceipt> initialize_native_orbital_start(
    GameState&, PlayerId, PlanetId, const NativeOrbitalStartBudget&);
// Partial arrival unload: excess stays aboard. Does not move minerals/found a colony.
std::uint64_t unload_native_orbital_colonists(GameState&, Planet&, Fleet&);
// Actual battle destruction only. Paid refits never call this operation.
void destroy_native_orbital_population(GameState&, OrbitalStationId);
}
