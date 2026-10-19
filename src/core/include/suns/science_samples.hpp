#pragma once
#include "suns/turn_processor.hpp"
#include <span>

namespace suns {
inline constexpr double kScienceSampleMass = 5.0;
inline constexpr std::size_t kMaximumScienceSamples = 64;
[[nodiscard]] double science_sample_mass(std::span<const ScienceSample>);
[[nodiscard]] const char* science_sample_name(UnknownObjectKind);
[[nodiscard]] std::uint32_t science_sample_research_points(UnknownObjectKind);
[[nodiscard]] bool sample_observation_received(const GameState&, PlayerId, UnknownObjectId);
[[nodiscard]] std::uint32_t retained_sample_research(const GameState&, const Planet&);
bool transfer_science_sample(GameState&, PlayerId, const TransferScienceSampleOrder&);
// Apply to a planning copy only when both fleet endpoints have current
// received telemetry. Historical positions cannot establish live contact.
bool preview_science_sample_transfer(GameState&, PlayerId, const TransferScienceSampleOrder&);
bool process_science_sample(GameState&, PlayerId, const ProcessScienceSampleOrder&);
void recover_science_samples(GameState&, Fleet&, std::uint64_t);
bool unload_science_samples(GameState&, Planet&, Fleet&, std::uint64_t);
} // namespace suns
