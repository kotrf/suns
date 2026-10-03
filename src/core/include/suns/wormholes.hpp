#pragma once

#include "suns/game_event.hpp"

namespace suns {

[[nodiscard]] const WormholeKnowledge* known_wormhole(const GameState&, PlayerId, WormholeEndpointId);
[[nodiscard]] bool fleet_has_anomaly_detector(const GameState&, const Fleet&);
void advance_wormholes(GameState&);
void observe_wormhole_sweep(GameState&, const Fleet&, Position start, Position end, std::uint64_t);
void observe_current_wormholes(GameState&, std::uint64_t);
// Follow a moving mouth only when this fleet can physically detect it locally.
void resolve_wormhole_approaches(GameState&);
// true means the authoritative fleet was destroyed and must be removed.
[[nodiscard]] bool enter_wormhole(GameState&, Fleet&, WormholeEndpointId);
[[nodiscard]] std::vector<GameEvent> deliver_wormhole_reports(GameState&);
[[nodiscard]] std::vector<Fleet> wormhole_missing_contacts(const GameState&, PlayerId);

} // namespace suns
