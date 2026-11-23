#pragma once
#include "suns/game_state.hpp"
namespace suns {
[[nodiscard]] constexpr bool unknown_object_is_moving(UnknownObjectKind kind) noexcept
{ return kind==UnknownObjectKind::DriftingDerelict || kind==UnknownObjectKind::RoguePlanetoid
    || kind==UnknownObjectKind::RogueComet; }
inline constexpr double kIonVortexRadiusLy = 18.0;
inline constexpr double kIonVortexInterference = 0.30;
[[nodiscard]] const char* unknown_object_name(UnknownObjectKind);
[[nodiscard]] const char* object_knowledge_name(ObjectKnowledgeLevel);
[[nodiscard]] ResearchField unknown_object_research_field(UnknownObjectKind);
[[nodiscard]] std::uint32_t unknown_object_research_points(UnknownObjectKind);
[[nodiscard]] bool object_observation_valid(const ObjectObservation&);
[[nodiscard]] bool unknown_object_valid(const UnknownObject&);
// Cheap received-course estimate for a route, not a future turn simulation.
[[nodiscard]] Position object_approach_position(const ObjectObservation&,std::uint64_t,Position,std::uint8_t);
[[nodiscard]] bool object_course_contains(const ObjectObservation&,Position);
// Planning uses fitted range; actual work additionally accounts for local jamming.
[[nodiscard]] double object_study_radius(const GameState&,const Fleet&);
[[nodiscard]] const ObjectObservation* known_unknown_object(const GameState&,PlayerId,UnknownObjectId);
void generate_unknown_objects(GameState&,bool includeMoving=true);
void advance_unknown_objects(GameState&);
void observe_unknown_objects(GameState&,std::uint64_t);
void research_unknown_objects(GameState&,Fleet&,std::uint64_t);
} // namespace suns
