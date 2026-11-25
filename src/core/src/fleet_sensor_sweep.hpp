#pragma once

#include "suns/communications.hpp"

#include <optional>
#include <vector>

namespace suns::detail {

// A travelled segment keeps the instruments and source it had during flight.
struct FleetSensorSweep {
    PlayerId owner{};
    FleetId fleet{};
    Position start, end;
    std::optional<double> scannerRange; // The Bat Scanner has a range of zero.
    double penetratingRange{}, geologicalRange{};
    bool anomalyDetector{};
};

// Observations are frozen before later actions can change physical knowledge.
// Only packet routing uses the network at the end of the movement phase.
struct FleetSensorReadings {
    PlayerId owner{};
    FleetId fleet{};
    Position source;
    std::uint64_t observedTurn{};
    std::vector<PendingSurveyReport> surveys;
    std::vector<PendingPlayerReport> geology;
    std::vector<WormholeKnowledge> wormholes;
};

[[nodiscard]] std::optional<FleetSensorSweep> prepare_fleet_sensor_sweep(
    GameState&, const Fleet&, Position start, Position end, std::uint64_t turn);
[[nodiscard]] std::optional<FleetSensorReadings> read_fleet_sensor_sweep(
    const GameState&, const FleetSensorSweep&, std::uint64_t turn);
void dispatch_fleet_sensor_readings(GameState&, FleetSensorReadings&&);
void read_wormhole_sweep(const GameState&, const FleetSensorSweep&, double effectiveRange, FleetSensorReadings&);
void dispatch_wormhole_readings(GameState&, const FleetSensorReadings&, const CommunicationRoute&);

// The source's permission to transmit was established while taking the observation.
void record_dispatched_report(GameState&, PlayerId, Position, std::uint64_t turn, const CommunicationRoute&);

} // namespace suns::detail
