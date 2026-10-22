#pragma once
#include "suns/game_state.hpp"

namespace suns {
inline constexpr std::uint64_t kOrbitalPopulationCapacity = 2'000'000;
inline constexpr std::uint8_t kHabitatReserveYears = 3;
[[nodiscard]] bool habitat_completed(const Planet&);
[[nodiscard]] std::uint64_t total_colony_population(const Planet&);
[[nodiscard]] std::int64_t projected_orbital_growth(const Planet&);
// Next annual growth after maintenance, without changing the received baseline.
[[nodiscard]] std::int64_t projected_orbital_growth(const GameState&,const Planet&);
[[nodiscard]] std::string habitat_service_name(HabitatService);
[[nodiscard]] std::uint32_t net_colony_output(const GameState&,const Planet&);
// Same annual maintenance resolver in host and forecast. Returns the mandatory
// production debit; the persisted turn guard prevents duplicate payment/loss.
std::uint32_t service_orbital_habitat(const GameState&, Planet&, bool supported);
void publish_habitat_reports(GameState&, const GameState& previous);
void apply_received_station_battle(Player&,const PendingPlayerReport&);
void merge_received_habitat_station(std::optional<PlanetSurveySnapshot>&,const std::optional<PlanetSurveySnapshot>&);
}
