#pragma once
#include "suns/game_state.hpp"
namespace suns {
[[nodiscard]] const char* unknown_object_name(UnknownObjectKind);
[[nodiscard]] const char* object_knowledge_name(ObjectKnowledgeLevel);
[[nodiscard]] ResearchField unknown_object_research_field(UnknownObjectKind);
[[nodiscard]] std::uint32_t unknown_object_research_points(UnknownObjectKind);
[[nodiscard]] bool object_observation_valid(const ObjectObservation&);
[[nodiscard]] const ObjectObservation* known_unknown_object(const GameState&,PlayerId,UnknownObjectId);
void generate_unknown_objects(GameState&);
void observe_unknown_objects(GameState&,std::uint64_t);
void research_unknown_objects(GameState&,Fleet&,std::uint64_t);
} // namespace suns
