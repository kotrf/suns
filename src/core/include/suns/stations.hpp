#pragma once
#include "suns/turn_processor.hpp"

namespace suns {
struct StationHullSpec {
    OrbitalStationHullType hull;
    const char* name;
    std::uint8_t construction;
    std::uint8_t weapons, defenses, electronics;
    double armor;
    std::uint32_t cost;
    MineralCargo minerals;
};
inline constexpr std::uint16_t kMaximumColonyDefenses = 100;
// Initial finite-range fleet gates; these are paid station fittings, not
// universal movement shortcuts. Full race-specific networks come later.
inline constexpr double kStargateRangeLy = 160.0;
inline constexpr double kStargateMaximumMassKt = 450.0;
inline constexpr std::uint8_t kStargateEnergyLevel = 8;
inline constexpr std::uint8_t kStargateElectronicsLevel = 8;
[[nodiscard]] const StationHullSpec& station_hull_spec(OrbitalStationHullType);
[[nodiscard]] bool station_component_supported(ShipComponentType);
[[nodiscard]] bool stargate_station_available(const GameState&, PlayerId, PlanetId);
[[nodiscard]] bool stargate_transfer_valid(
    const GameState&, PlayerId, FleetId, PlanetId source, PlanetId destination);
bool transfer_fleet_through_stargate(
    GameState&, PlayerId, FleetId, PlanetId source, PlanetId destination);
[[nodiscard]] bool station_loadout_valid(const StationLoadout&);
[[nodiscard]] bool station_loadout_available(const GameState&, PlayerId, const StationLoadout&);
[[nodiscard]] StationLoadout station_loadout(const OrbitalStation&);
[[nodiscard]] std::uint32_t station_build_cost(const StationLoadout&);
[[nodiscard]] MineralCargo station_mineral_cost(const StationLoadout&);
[[nodiscard]] double station_armor(const OrbitalStation&);
[[nodiscard]] double station_shields(const OrbitalStation&);
[[nodiscard]] bool station_has_weapons(const OrbitalStation&);
[[nodiscard]] bool station_construction_valid(const GameState&, const Planet&, const StationConstruction&);
bool queue_station_construction(GameState&, PlayerId, const QueueStationOrder&);
bool complete_station_construction(GameState&, const Planet&, const StationConstruction&);
[[nodiscard]] const OrbitalStation* known_orbital_station(const GameState&, PlayerId, PlanetId);
[[nodiscard]] bool colony_defenses_available(const GameState&, PlayerId);
[[nodiscard]] double colony_defense_coverage(const Planet&);
} // namespace suns
