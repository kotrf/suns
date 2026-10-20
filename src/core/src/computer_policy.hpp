#pragma once

#include "suns/computer_player.hpp"

namespace suns::computer_detail {
bool idle(const Fleet&);
bool confirmed_idle(const Fleet&, std::uint64_t issuedTurn);
const Planet* colony_at(const GameState&, PlayerId, Position);
std::optional<std::uint8_t> route_warp(const GameState&, Fleet, const std::vector<Position>&);
std::uint64_t review_turn(const GameState&, const Fleet&);
void explain(std::string&, const std::string&);
bool military_fleet(const GameState&, const Fleet&);
bool hostile_intel(const PlayerView&);
void plan_military(const PlayerView&, ComputerTurnPlan&, std::string&);
std::optional<GateFleetOrder> gate_staging_order(
    const GameState&, const Fleet&, Position target, bool requireRefueling = false, const PlayerOrders* pending = nullptr);
void plan_gate_construction(const PlayerView&, ComputerTurnPlan&, std::string&);
} // namespace suns::computer_detail
