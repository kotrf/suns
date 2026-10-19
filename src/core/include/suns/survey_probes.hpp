#pragma once
#include "suns/turn_processor.hpp"

namespace suns {
bool probe_program_valid(const ProbeProgram&);
bool probe_capable(const GameState&, const Fleet&);
bool submit_probe_command(GameState&, PlayerId, const SetProbeProgramOrder&);
void apply_probe_command(const GameState&, Fleet&, const ProbeCommand&);
void pause_probe(Fleet&, ProbeStatus = ProbeStatus::ManualPause);
void combine_probe_knowledge(Fleet&, const Fleet&);
SurveyLevel probe_survey_level(const Fleet&, StarId);
void record_probe_survey(Fleet&, StarId, SurveyLevel, std::uint64_t);
void advance_survey_probes(GameState&);
std::string probe_status_name(ProbeStatus);
ShipDesign basic_survey_probe_design(PlayerId, ShipDesignId);
}
