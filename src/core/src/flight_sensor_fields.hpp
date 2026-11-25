#pragma once

#include "suns/strategic_operations.hpp"

#include <set>

namespace suns::detail {

// Historical interference for flights still being observed while arrival
// actions change the live world. Only actual field geometry is retained.
struct FlightSensorFields {
    struct Field {
        PlayerId owner{};
        Position center;
        double radius{}, strength{};
    };
    std::vector<Field> fields;
    std::set<FleetId> recordedFleets;
};

[[nodiscard]] FlightSensorFields stationary_flight_sensor_fields(const GameState&, std::uint64_t turn);
void record_fleet_sensor_fields(const GameState&, const Fleet&, std::uint64_t turn, FlightSensorFields&);
void finish_flight_sensor_fields(const GameState&, std::uint64_t turn, FlightSensorFields&);
[[nodiscard]] double flight_sensor_interference(const FlightSensorFields&, PlayerId, Position);
[[nodiscard]] std::vector<SensorInterferenceSpan> flight_sensor_interference_spans(
    const FlightSensorFields&, PlayerId, Position start, Position end);

} // namespace suns::detail
