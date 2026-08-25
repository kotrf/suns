#pragma once

#include "suns/game_state.hpp"

#include <cstdint>

namespace suns {

enum class GameEventKind {
    SystemSurveyed,
};

enum class GameEventSeverity {
    Information,
    Warning,
    Critical,
};

struct GameEvent {
    std::uint64_t id{};
    std::uint64_t turn{};
    std::uint64_t observedTurn{};
    PlayerId recipient{};
    GameEventKind kind{GameEventKind::SystemSurveyed};
    GameEventSeverity severity{GameEventSeverity::Information};
    StarId star{};
    PlanetId planet{};
    FleetId fleet{};
};

} // namespace suns
