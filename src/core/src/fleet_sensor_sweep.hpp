#pragma once

#include "suns/communications.hpp"

#include <optional>

namespace suns::detail {

// A travelled segment keeps the instruments and source it had during flight.
// Reports ready to transmit use the radio network after all movement is complete.
struct FleetSensorSweep {
    PlayerId owner{};
    FleetId fleet{};
    Position start, end;
    std::optional<double> scannerRange; // The Bat Scanner has a range of zero.
    double penetratingRange{}, geologicalRange{};
    bool anomalyDetector{};
};

[[nodiscard]] std::optional<FleetSensorSweep> prepare_fleet_sensor_sweep(
    GameState&, const Fleet&, Position start, Position end, std::uint64_t turn);
void observe_fleet_sensor_sweep(GameState&, const FleetSensorSweep&, std::uint64_t turn);
void observe_transmitting_wormhole_sweep(GameState&, const FleetSensorSweep&, double effectiveRange,
    std::uint64_t turn, const CommunicationRoute&);

// The source's permission to transmit was established while taking the observation.
void record_dispatched_report(GameState&, PlayerId, Position, std::uint64_t turn, const CommunicationRoute&);

} // namespace suns::detail
