#pragma once
#include "suns/turn_processor.hpp"
#include <array>

namespace suns {
inline constexpr std::uint32_t kMaximumXenocrystal = 1'000'000;
inline constexpr std::uint32_t kXenocrystalPerPlanetYear = 4;
inline constexpr std::uint32_t kResonanceWork = 600;
inline constexpr std::uint32_t kResonanceWorkPerYear = 60;
inline constexpr std::uint32_t kResonanceCrystals = 8;
inline constexpr std::uint32_t kResonanceResearchPerArtifact = 32;
inline constexpr MineralCargo kResonanceMineralsPerWork{0.2, 0.1, 0.2};
inline constexpr std::uint32_t kResonantGateCrystals = 8;
[[nodiscard]] bool resonant_gate_technology_available(const GameState&, PlayerId);
// Returns the number of geographically distinct placement centers actually used.
std::size_t generate_xenocrystal_deposits(GameState&);
[[nodiscard]] std::optional<XenocrystalObservation> known_xenocrystal(const GameState&, PlayerId, PlanetId);
[[nodiscard]] bool exotic_mining_available(const GameState&, PlayerId);
[[nodiscard]] std::uint32_t xenocrystal_mining_rate(const GameState&, const Fleet&);
[[nodiscard]] std::uint32_t colony_xenocrystal_mining_rate(const GameState&, const Planet&);
// Requires a PlayerView; evaluates received private operating inputs, never live colony truth.
[[nodiscard]] std::string known_colony_xenocrystal_block(const GameState&, PlayerId, const Planet&);
[[nodiscard]] std::uint32_t known_colony_xenocrystal_rate(const GameState&, PlayerId, const Planet&);
void mine_xenocrystals(GameState&);
void observe_xenocrystal_surface(GameState&, PlayerId, PlanetId, FleetId, std::uint64_t, bool finalSurfaceObservation = false);
void publish_exotic_material_reports(GameState&, const GameState&);
bool move_xenocrystals(std::uint32_t& source, std::uint32_t& destination, std::uint32_t amount);
bool transfer_xenocrystals(GameState&, PlayerId, const TransferXenocrystalOrder&, bool commandDelivered = false);
bool preview_xenocrystal_transfer(GameState&, PlayerId, const TransferXenocrystalOrder&, const PlayerOrders* pending = nullptr);
[[nodiscard]] bool resonance_technology_available(const GameState&, PlayerId);
[[nodiscard]] bool resonance_scheduled(const Planet&);
[[nodiscard]] bool resonance_operational(const GameState&, const Planet&);
[[nodiscard]] std::string resonance_block(const GameState&, const Planet&);
bool apply_resonance_order(const GameState&, Planet&, const SetResonanceLaboratoryOrder&);
std::uint32_t advance_resonance_laboratory(const GameState&, Planet&, std::uint32_t available, bool supported, std::uint32_t annualRemaining);
[[nodiscard]] std::array<std::uint32_t, kResearchFieldCount> resonance_research(const GameState&, const Planet&);
[[nodiscard]] bool resonance_state_valid(const ResonanceLaboratory&);
} // namespace suns
