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
    FleetTargetLost,
    FleetsMerged,
    ProductionWaitingForShipyard,
    PrecursorArtifactsDiscovered,
    GroundInvasionWon,
    GroundInvasionLost,
    GroundDefenseWon,
    ColonyLost,
    FreightDelivered,
    EnemyFleetDetected,
    EnemyFleetLost,
    FleetMobilityRestored,
    AnomalyDetected,
    WormholeClassified,
    WormholeEntered,
    WormholeEmerged,
    WormholeEntryMissed,
    WormholeOverdue,
    WormholePresumedLost,
    WormholeCollapsed,
    SpaceBattle,
    Bombardment,
    MineStrike,
    ScientificData,
    EmissionDetected,
    EnemyFleetReclassified,
    SensorInterferenceChanged,
    PlanetaryAftermath,
    UnknownObjectObserved,
    InterstellarMediumObserved,
    StargateTransit,
    StargateTransitFailed,
    OutpostDeployed,
    OutpostDeploymentFailed,
    OutpostFuelExhausted,
    EngineeringProgress,
    EngineeringPaused,
    EngineeringResumed,
    EngineeringCaptured,
    EngineeringDismantled,
    LandingBlocked,
    SampleRecovered,
    SampleRetained,
    SampleDelivered,
    SampleDepleted,
    SampleHoldFull,
    XenocrystalDiscovered,
    XenocrystalAcquired,
    XenocrystalDepleted,
    ResonanceProjectStatus,
    VisitorSighted,
    VisitorExchangeCompleted,
    VisitorExchangeFailed,
    VisitorDeparted,
    OrbitalBaseStatus,
    FleetScrapStatus,
    VisitorCourseChanged,
};

inline constexpr bool is_visitor_event(GameEventKind kind)
{
    return (kind >= GameEventKind::VisitorSighted && kind <= GameEventKind::VisitorDeparted)
        || kind == GameEventKind::VisitorCourseChanged;
}

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
    std::int32_t quantity{};
    SurveyLevel surveyLevel{SurveyLevel::Detected};
    ResearchField researchField{ResearchField::Electronics};
    std::uint8_t technologyLevel{};
    bool precursorArtifactHint{};
    MineralCargo deliveredMinerals;
    std::uint64_t deliveredColonists{};
    PlayerId contactOwner{};
    WormholeEndpointId wormholeEndpoint{};
    std::optional<SpaceBattleReport> battle;
    std::optional<OperationalLossReport> losses;
    std::optional<ReconContact> contact;
    std::optional<SensorStatus> sensorStatus;
    std::optional<EmissionObservation> emission;
    std::optional<PlanetAftermathObservation> aftermath;
    std::optional<ObjectObservation> objectObservation;
    std::optional<MediumObservation> mediumObservation;
    VisitorProfile visitorProfile{VisitorProfile::Freight}; // Delivered offer, never a current host lookup.
};

} // namespace suns
