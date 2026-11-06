#pragma once
#include "suns/turn_processor.hpp"
#include <span>
namespace suns {
struct VisitorOffer {
    VisitorProfile profile;
    const char* name;
    const char* greeting;
    MineralCargo request;
    ShipComponentType gift;
};
[[nodiscard]] std::span<const VisitorOffer> visitor_offers();
[[nodiscard]] const VisitorOffer& visitor_offer(VisitorProfile);
[[nodiscard]] bool visitor_gift_valid(ShipComponentType);
inline constexpr double kVisitorGermaniumPrice = 50;
inline constexpr auto kVisitorGift = ShipComponentType::MultiCargoPod;
inline constexpr std::uint64_t kMaxVisitorTurn = 1'000'000'000ULL;
[[nodiscard]] Position visitor_prediction(const VisitorObservation&, double turn);
[[nodiscard]] bool visitor_observation_valid(const VisitorObservation&);
// Validate the private pending manoeuvre using the same bounded geometry as execution.
[[nodiscard]] bool visitor_course_schedule_valid(const GameState&, const IntergalacticVisitor&);
[[nodiscard]] const VisitorObservation* known_visitor(const GameState&, PlayerId, VisitorId);
[[nodiscard]] std::optional<Position> visitor_intercept_destination(const Fleet&, const VisitorObservation&, std::uint64_t turn, std::uint8_t warp);
bool submit_visitor_command(GameState&, PlayerId, const EncounterVisitorOrder&);
void apply_visitor_command(GameState&, Fleet&, const VisitorMission&);
void prepare_visitor_navigation(GameState&);
void complete_visitor_encounter(GameState&, Fleet&, VisitorId, Position);
void advance_visitors(GameState&);
void observe_visitors(GameState&);
[[nodiscard]] std::vector<GameEvent> deliver_visitor_reports(GameState&);
[[nodiscard]] GameState make_visitor_demo(VisitorProfile profile = VisitorProfile::Freight);
} // namespace suns
