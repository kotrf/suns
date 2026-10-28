#pragma once
#include "suns/campaign.hpp"
#include "suns/turn_processor.hpp"
namespace suns {
inline constexpr std::size_t kMaximumTransportLines = 128;
inline constexpr std::size_t kMaximumLineAssignments = 256;
[[nodiscard]] bool transport_line_spec_valid(const TransportLineSpec&);
[[nodiscard]] std::string transport_line_error(const PlayerView&, const TransportLineSpec&);
[[nodiscard]] FleetRouteProgram transport_line_program(const TransportLine&);
[[nodiscard]] bool set_transport_line(GameState&, PlayerId, const SetTransportLineOrder&);
[[nodiscard]] bool assign_transport_line(GameState&, PlayerId, const AssignTransportLineOrder&);
[[nodiscard]] std::optional<MoveFleetOrder> pending_transport_route(const GameState&,const PlayerOrders&,FleetId);
[[nodiscard]] std::optional<FleetId> transport_manual_override(const Order&);
void release_transport_assignment(GameState&, PlayerId, FleetId);
// Received facts only; the registry itself records the owner's intended roster.
[[nodiscard]] std::string transport_assignment_status(const PlayerView&, const TransportLine&, const TransportAssignment&);
}
