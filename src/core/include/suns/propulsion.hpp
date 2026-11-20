#pragma once

#include "suns/game_state.hpp"

#include <span>

namespace suns {

inline constexpr double kFuelContainersPerMg = 10.0;

// Opening a 0.1 mg container consumes it in full.
[[nodiscard]] double fuel_containers_required(double requiredMg);
[[nodiscard]] inline double fuel_container_cost(double requiredMg)
{
    return fuel_containers_required(requiredMg) / kFuelContainersPerMg;
}
[[nodiscard]] double fuel_in_complete_containers(double storedMg);
// Apply a signed fuel adjustment in mg while preserving fractional amounts.
[[nodiscard]] double fuel_after_change(double storedMg, double changeMg);
// Subtract a whole-container charge while carrying the fractional reserve forward.
[[nodiscard]] double fuel_after_consumption(double storedMg, double consumedMg);
// Tank accounting for a transfer whose participants have already been validated.
[[nodiscard]] double transfer_fuel(Fleet& source, Fleet& destination,
    double requestedMg, double sourceReserveMg, double destinationCapacityMg);

enum class EngineAccess { Any, ImprovedFuelEfficiency, NoRamScoops, Settler };

struct PropulsionTechnology {
    ShipComponentType component;
    const char* name;
    std::uint8_t propulsion;
    std::uint8_t energy;
    std::uint8_t optimalWarp;
    std::uint8_t freeWarp;
    std::uint8_t safeWarp;
    double mass;
    std::uint32_t cost;
    MineralCargo minerals;
    EngineAccess access{EngineAccess::Any};
    bool ramScoop{};
    bool radiating{};
    // Original fuel-use percentages, indexed by Warp (0..10).
    std::array<std::uint16_t, kMaxWarp + 1> fuelEfficiency{};
};

[[nodiscard]] std::span<const PropulsionTechnology> propulsion_technologies();
[[nodiscard]] const PropulsionTechnology* propulsion_technology(ShipComponentType component);
[[nodiscard]] ShipComponentSpec propulsion_component_spec(const PropulsionTechnology& technology);
[[nodiscard]] bool engine_access_available(const RaceProfile& race, EngineAccess access);
[[nodiscard]] bool legacy_propulsion_component(ShipComponentType component);
[[nodiscard]] bool player_uses_legacy_propulsion(const GameState& state, PlayerId player);

} // namespace suns
