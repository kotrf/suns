#pragma once

#include "suns/game_state.hpp"

#include <cstdint>

namespace suns {

enum class GameEventKind {
    SystemSurveyed,
    FleetArrived,
    RouteCompleted,
    FleetStalledForFuel,
    ProductionCompleted,
    ColonyFounded,
    ProductionWaitingForMinerals,
    ResearchLevelCompleted,
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
    ShipDesignId shipDesign{};
    ProductionKind productionKind{ProductionKind::ColonyShip};
    Position position;
    std::uint32_t quantity{};
    SurveyLevel surveyLevel{SurveyLevel::Detected};
    ResearchField researchField{ResearchField::Electronics};
    std::uint8_t technologyLevel{};
};

} // namespace suns
