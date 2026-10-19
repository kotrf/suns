#pragma once
#include "suns/turn_processor.hpp"
namespace suns {
inline constexpr double kVisitorGermaniumPrice = 50;
inline constexpr auto kVisitorGift = ShipComponentType::MultiCargoPod;
[[nodiscard]] Position visitor_prediction(const VisitorObservation&, double turn);
[[nodiscard]] bool visitor_observation_valid(const VisitorObservation&);
[[nodiscard]] const VisitorObservation* known_visitor(const GameState&, PlayerId, VisitorId);
[[nodiscard]] std::optional<Position> visitor_intercept_destination(const Fleet&, const VisitorObservation&, std::uint64_t turn, std::uint8_t warp);
bool submit_visitor_command(GameState&, PlayerId, const EncounterVisitorOrder&);
void apply_visitor_command(GameState&, Fleet&, const VisitorMission&);
void prepare_visitor_navigation(GameState&);
void complete_visitor_encounter(GameState&, Fleet&, VisitorId, Position);
void advance_visitors(GameState&);
void observe_visitors(GameState&);
[[nodiscard]] std::vector<GameEvent> deliver_visitor_reports(GameState&);
[[nodiscard]] GameState make_visitor_demo();
} // namespace suns
