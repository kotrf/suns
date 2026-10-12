#pragma once
#include "suns/game_event.hpp"
namespace suns {
[[nodiscard]] const char* contact_quality_name(ContactQuality);
[[nodiscard]] bool recon_contact_valid(const ReconContact&);
// Deliveries and zero-delay connected sensors are the only sources of owner knowledge.
[[nodiscard]] std::vector<ReconContact> known_enemy_contacts(const GameState&, PlayerId);
[[nodiscard]] bool sensor_status_valid(const SensorStatus&);
[[nodiscard]] std::vector<SensorStatus> known_sensor_status(const GameState&, PlayerId);
[[nodiscard]] Fleet recon_contact_fleet(const ReconContact&, std::uint64_t currentTurn);
void observe_reconnaissance(GameState&, std::uint64_t observationTurn);
void deliver_reconnaissance(GameState&);
[[nodiscard]] bool fleet_active_sensors(const Fleet&, std::uint64_t);
[[nodiscard]] bool electronics_program_valid(const ElectronicsProgram&);
}
