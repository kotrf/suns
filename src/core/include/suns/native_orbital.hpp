#pragma once
#include "suns/game_state.hpp"

namespace suns {
// Station-resident civilization: paid life support, remote mining and housing.
[[nodiscard]] bool native_orbital_race(const GameState&, PlayerId);
[[nodiscard]] std::uint64_t native_orbital_capacity(OrbitalStationHullType);
[[nodiscard]] const OrbitalStation* native_orbital_station(const GameState&, const Planet&);
// Prototype orbital industry: floor(sqrt(supported residents * (Energy + 1) / 10)).
// Surface factories/population never contribute. Actual radiation casualties
// reduce the next year's resident base. Annual service precedes production.
[[nodiscard]] std::uint32_t native_orbital_potential_output(const GameState&, const Planet&);
[[nodiscard]] std::uint32_t native_orbital_output(const GameState&, const Planet&);
struct NativeOrbitalRadiationStatus {
    std::uint8_t background{}, protection{}, exposure{}, tolerance{};
    std::uint64_t annualLoss{};
    std::int32_t safety{100}; // Safe orbital dose is 100; excess is negative.
};
[[nodiscard]] std::optional<NativeOrbitalRadiationStatus> native_orbital_radiation_status(
    const GameState&, PlayerId, StarId, std::uint64_t residents, const StationLoadout&);
[[nodiscard]] std::optional<NativeOrbitalRadiationStatus> native_orbital_radiation_status(const GameState&, const Planet&);
[[nodiscard]] std::int32_t native_orbital_habitability(const GameState&, PlayerId, const Planet&);
[[nodiscard]] std::optional<std::int32_t> known_native_orbital_habitability(const GameState&, PlayerId, PlanetId);
// The existing paid deployment is unshielded; protection requires a paid refit.
[[nodiscard]] StationLoadout native_orbital_founding_loadout();

// Added to the reference construction module price at ship manufacture.
// The station/receiver are deployed once; finite seed stock is not salvage.
struct NativeOrbitalFoundingKit {
    std::uint32_t production{};
    MineralCargo minerals;
    MineralCargo supplies;
};
[[nodiscard]] NativeOrbitalFoundingKit native_orbital_founding_kit();
[[nodiscard]] bool ship_design_can_found_native_orbit(const ShipDesign&);
[[nodiscard]] bool fleet_can_found_native_orbit(const GameState&, const Fleet&);

struct NativeOrbitalStartBudget {
    std::uint64_t population{kInitialHomePopulation};
    std::uint32_t production{};
    MineralCargo minerals;
    std::optional<ShipComponentType> radiationScreen{ShipComponentType::BulkRadiationScreen};
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
[[nodiscard]] NativeOrbitalStartBudget native_orbital_start_budget(
    std::optional<ShipComponentType> radiationScreen = ShipComponentType::BulkRadiationScreen);
// Host construction kernel for an empty empire/world at turn one, not a
// campaign-availability bypass. Rejects atomically, including repeat starts.
[[nodiscard]] std::optional<NativeOrbitalStartReceipt> initialize_native_orbital_start(
    GameState&, PlayerId, PlanetId, const NativeOrbitalStartBudget&);
// Partial arrival unload: excess stays aboard. Does not move minerals/found a colony.
std::uint64_t unload_native_orbital_colonists(GameState&, Planet&, Fleet&);
// Actual battle destruction only. Paid refits never call this operation.
void destroy_native_orbital_population(GameState&, OrbitalStationId);
}
