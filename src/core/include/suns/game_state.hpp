#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace suns {

using PlayerId = std::uint32_t;
using StarId = std::uint32_t;
using PlanetId = std::uint32_t;
using FleetId = std::uint32_t;
using ShipDesignId = std::uint32_t;
using OrbitalStationId = std::uint32_t;
using WormholeEndpointId = std::uint32_t;
using UnknownObjectId = std::uint32_t;

enum class UnknownObjectKind : std::uint8_t { AncientProbe, CompactRemnant };
enum class ObjectKnowledgeLevel : std::uint8_t { Detected, Classified, Studied };

inline constexpr std::uint32_t kColonyShipCost = 12;
inline constexpr std::uint32_t kFactoryCost = 6;
inline constexpr std::uint32_t kMineCost = 5;
inline constexpr std::uint32_t kOrbitalDockCost = 24;
inline constexpr std::uint32_t kTerraformingCost = 12;
inline constexpr double kScoutTravelSpeed = 100.0;       // Legacy UI compatibility.
inline constexpr double kColonyShipTravelSpeed = 70.0;  // Legacy UI compatibility.
inline constexpr double kScoutSensorRange = 50.0;
inline constexpr double kColonySensorRange = 150.0;
inline constexpr double kLocalColonySensorRange = 30.0;
inline constexpr ShipDesignId kScoutDesignId = 1;
inline constexpr ShipDesignId kColonyShipDesignId = 2;
inline constexpr ShipDesignId kFirstCustomShipDesignId = 3;
inline constexpr std::uint8_t kMaxWarp = 10;
inline constexpr std::uint8_t kScoutCruiseWarp = 8;
inline constexpr std::uint8_t kColonyShipCruiseWarp = 7;
// Cargo and ship mass are in kilotonnes (1 kt = 1,000,000 kg).
// Stars! reference ratio: 100 colonists occupy 1 kt of cargo (10,000 kg each).
inline constexpr double kColonistMassKg = 10'000.0;
inline constexpr double kColonistsPerCargoUnit = 1'000'000.0 / kColonistMassKg;
inline constexpr std::uint64_t kInitialHomePopulation = 1'000'000;
inline constexpr std::uint64_t kPopulationPerHabitability = 25'000;
inline constexpr std::uint64_t kPopulationPerProductionUnit = 500'000;
inline constexpr double kRadiatingDriveSafeTolerance = 0.85;
inline constexpr double kRadiatingDriveColonistLossFraction = 0.10;
inline constexpr std::uint32_t kFirstResearchLevelCost = 18;

// First communications slice: established friendly colonies temporarily act as
// access nodes to the empire's instantaneous subspace backbone. Outside the
// mesh a finite-speed subspace signal propagates between real transceivers. Orbital
// stations will later move this capability to explicit station/ship modules
// without changing packet and latency semantics.
inline constexpr double kCommunicationSignalSpeed = 150.0;
// Strategic contact envelope used by continuous within-turn interception. It
// is deliberately tiny relative to ordinary Warp travel distances and absorbs
// only navigation/tactical closure that the annual movement model cannot show.
inline constexpr double kFleetEncounterRadius = 0.01;

struct Position {
    double x{};
    double y{};
    bool operator==(const Position&) const = default;
};

struct FleetEncounterGeometry {
    double closestTimeFraction{};
    double closestDistance{};
    std::optional<double> encounterTimeFraction;
    Position encounterPosition;
};

using VisitorId = std::uint32_t;
struct VisitorObservation {
    VisitorId id{};
    Position position, velocity;
    std::uint64_t observedTurn{}, receivedTurn{}, departureTurn{};
    bool available{true}, departed{};
};
enum class VisitorReportKind : std::uint8_t { Sighted, Exchanged, Failed, Departed };
struct PendingVisitorReport {
    VisitorObservation observation;
    VisitorReportKind kind{VisitorReportKind::Sighted};
    std::uint64_t observedTurn{}, deliveryTurn{};
    FleetId transmissionFleet{};
};
struct IntergalacticVisitor {
    VisitorId id{};
    Position position, velocity;
    std::uint64_t arrivalTurn{}, departureTurn{};
    PlayerId customer{}; // Host-only: the first physical exchange consumes the offer.
};
struct VisitorMission {
    VisitorObservation track; // A physically received immutable track, never a host lookup.
    std::uint8_t warp{8};
};

enum class StarClass {
    BlueWhite,
    White,
    YellowWhite,
    Yellow,
    Orange,
    Red,
};

struct StellarVariability {
    // A zero period denotes a stable star. Variable stars use a deterministic
    // sinusoidal cycle measured in planning turns.
    std::uint16_t periodTurns{};
    std::uint8_t amplitudePercent{};
    std::uint16_t phaseOffset{};
};

struct StarSystem {
    StarId id{};
    std::string name;
    Position position;
    StarClass stellarClass{StarClass::Yellow};
    StellarVariability variability;
};

struct MineralCargo {
    double ironium{};
    double boranium{};
    double germanium{};
};

enum class ShipHullType {
    Scout,
    LightTransport,
    MediumTransport,
    RemoteMiner,
    Utility,
    HeavyTransport,
    MiniColonyShip,
    SmallFreighter,
    MediumFreighter,
    LargeFreighter,
    SuperFreighter,
    StarsScout,
    Frigate,
    Destroyer,
    Cruiser,
    BattleCruiser,
    Battleship,
    Dreadnought,
    Privateer,
    Rogue,
    Galleon,
    StarsMiniColonyShip,
    ColonyShip,
    MiniBomber,
    B17Bomber,
    StealthBomber,
    B52Bomber,
    MidgetMiner,
    MiniMiner,
    Miner,
    MaxiMiner,
    UltraMiner,
    FuelTransport,
    SuperFuelXport,
    MiniMineLayer,
    SuperMineLayer,
    Nubian,
    MiniMorph,
    MetaMorph,
    ExpeditionBase,
    SurveyProbe,
};

// Hull access families only; population, combat and economy traits are separate.
enum class HullAccess : std::uint8_t {
    Standard, InnerStrength, SuperStealth, WarMonger, SpaceDemolition, HyperExpansion,
};

using ShipSlotId = std::uint16_t;

enum class ShipSlotCategory : std::uint8_t {
    Engine,
    General,
    Mining,
    Scanner, Mechanical, Electrical, Shield, Armor, Weapon, Bomb, MineLayer, Mixed,
};

struct ShipSlotSpec {
    ShipSlotId id{};
    ShipSlotCategory category{ShipSlotCategory::General};
    std::uint8_t column{};
    std::uint8_t row{};
    // Original equipment mask and bank identity. Zero retains legacy fitting.
    std::uint16_t allowedEquipment{};
    std::uint8_t bank{};
    std::uint8_t bankCapacity{1};
};

struct ShipHullSpec {
    ShipHullType type{ShipHullType::Scout};
    std::string name;
    double mass{};
    std::uint32_t buildCost{};
    double baseFuelCapacity{};
    double baseCargoCapacity{};
    std::uint8_t requiredEngines{1};
    std::uint8_t generalSlots{};
    std::uint8_t miningSlots{};
    std::vector<ShipSlotSpec> fittingSlots;
    MineralCargo baseMineralCost;
    std::uint16_t armor{};
    std::uint8_t initiative{};
    std::uint8_t constructionLevel{};
    HullAccess access{HullAccess::Standard};
    bool requiresAdvancedRemoteMining{};
    bool excludesBasicRemoteMining{};
    bool mysteryTrader{};
    double fuelGenerationPerTurn{};
    double fleetRepairBonus{};
    bool doublesMineLaying{};
};

enum class ShipComponentType {
    FusionDrive,
    RamScoopDrive,
    RadiatingRamScoopDrive,
    LongRangeScanner,
    ColonyModule,
    FuelTank,
    CargoPod,
    AntimatterGenerator,
    PenetratingScanner,
    CompactLongRangeScanner,
    RemoteMiningModule,
    AdvancedFusionDrive,
    ExtendedRangeScanner,
    HighWarpDrive,
    EfficientRamScoopDrive,
    DeepPenetratingScanner,
    RelayArray,
    FieldRepairBay,
    AnomalyDetector,
    SettlersDelight,
    QuickJump5,
    LongHump6,
    DaddyLongLegs7,
    AlphaDrive8,
    TransGalacticDrive,
    Interspace10,
    TransStar10,
    FuelMizer,
    RadiatingHydroRamScoop,
    SubGalacticFuelScoop,
    TransGalacticFuelScoop,
    TransGalacticSuperScoop,
    TransGalacticMizerScoop,
    GalaxyScoop,
    BatScanner, RhinoScanner, MoleScanner, DnaScanner, PossumScanner, PickPocketScanner,
    ChameleonScanner, FerretScanner, DolphinScanner, GazelleScanner, RnaScanner,
    CheetahScanner, ElephantScanner, EagleEyeScanner, RobberBaronScanner, PeerlessScanner,
    RoboMidgetMiner, RoboMiniMiner, RoboMiner, RoboMaxiMiner, RoboSuperMiner, RoboUltraMiner,
    // Appended Stars! equipment IDs; existing saved component IDs stay stable.
    Laser,
    XRayLaser,
    MiniGun,
    YakimoraLightPhaser,
    Blackjack,
    PhaserBazooka,
    PulsedSapper,
    ColloidalPhaser,
    GatlingGun,
    MiniBlaster,
    Bludgeon,
    MarkIVBlaster,
    PhasedSapper,
    HeavyBlaster,
    GatlingNeutrinoCannon,
    MyopicDisruptor,
    Blunderbuss,
    Disruptor,
    MultiContainedMunition,
    SyncroSapper,
    MegaDisruptor,
    BigMuthaCannon,
    StreamingPulverizer,
    AntiMatterPulverizer,
    AlphaTorpedo,
    BetaTorpedo,
    DeltaTorpedo,
    EpsilonTorpedo,
    RhoTorpedo,
    UpsilonTorpedo,
    OmegaTorpedo,
    AntiMatterTorpedo,
    JihadMissile,
    JuggernautMissile,
    DoomsdayMissile,
    ArmageddonMissile,
    LadyFingerBomb,
    BlackCatBomb,
    M70Bomb,
    M80Bomb,
    CherryBomb,
    Lbu17Bomb,
    Lbu32Bomb,
    Lbu74Bomb,
    HushABoom,
    RetroBomb,
    SmartBomb,
    NeutronBomb,
    EnrichedNeutronBomb,
    PeerlessBomb,
    AnnihilatorBomb,
    AlienMiner,
    OrbitalAdjuster,
    MineDispenser40,
    MineDispenser50,
    MineDispenser80,
    MineDispenser130,
    HeavyDispenser50,
    HeavyDispenser110,
    HeavyDispenser200,
    SpeedTrap20,
    SpeedTrap30,
    SpeedTrap50,
    StarsColonizationModule,
    OrbitalConstructionModule,
    StarsCargoPod,
    SuperCargoPod,
    MultiCargoPod,
    StarsFuelTank,
    SuperFuelTank,
    ManeuveringJet,
    Overthruster,
    JumpGate,
    BeamDeflector,
    TransportCloaking,
    StealthCloak,
    SuperStealthCloak,
    UltraStealthCloak,
    MultiFunctionPod,
    BattleComputer,
    BattleSuperComputer,
    BattleNexus,
    Jammer10,
    Jammer20,
    Jammer30,
    Jammer50,
    EnergyCapacitor,
    FluxCapacitor,
    EnergyDampener,
    TachyonDetector,
    StarsAntimatterGenerator,
    MoleSkinShield,
    CowHideShield,
    WolverineDiffuseShield,
    CrobySharmor,
    ShadowShield,
    BearNeutrinoBarrier,
    LangstonShell,
    GorillaDelagator,
    ElephantHideFortress,
    CompletePhaseShield,
    Tritanium,
    Crobmnium,
    CarbonicArmor,
    Strobnium,
    OrganicArmor,
    Kelarium,
    FieldedKelarium,
    DepletedNeutronium,
    Neutronium,
    MegaPolyShell,
    Valanium,
    Superlatanium,
    // Append-only wire IDs. The following two gates fit orbital stations only.
    NetworkStargate,
    InfinityStargate,
    FuelDepotKit,
    DeepSpaceRelayKit,
    ExpeditionDock,
    ExpeditionWorkshop,
    SurveyAutopilot,
};

inline constexpr auto kLastShipComponent = ShipComponentType::SurveyAutopilot;

enum class ShipComponentKind {
    Engine,
    Scanner,
    Fuel,
    Cargo,
    Mining,
    Special,
    BeamWeapon, Torpedo, Bomb, MineLayer, Mechanical, Electrical, Shield, Armor,
};

struct ShipComponentSpec {
    ShipComponentType type{ShipComponentType::FusionDrive};
    std::string name;
    ShipComponentKind kind{ShipComponentKind::Engine};
    double mass{};
    std::uint32_t buildCost{};

    double engineThrust{};
    std::uint8_t maxWarp{};
    std::array<double, kMaxWarp + 1> fuelPer100MassLy{};
    // Reference engines collect fuel per fitted engine and ly, independent of mass.
    std::array<double, kMaxWarp + 1> fuelCollectedPerEngineLy{};
    // Hull damage added by one full turn of travel at each Warp setting.
    // Entries above maxWarp are engine-specific emergency overdrive ratings.
    std::array<double, kMaxWarp + 1> overdriveDamagePercent{};
    std::uint8_t optimalWarp{};
    std::uint8_t freeWarp{};

    double sensorRange{};
    bool penetratesPlanets{};
    double relayRange{};
    double fuelCapacity{};
    double cargoCapacity{};
    double fuelGenerationPerTurn{};
    bool enablesColonization{};
    double remoteMiningUnits{}; // kt per mineral per year at concentration 100.
    double radiationHazard{};
    double fieldRepairPerTurn{};
    // Ordinary and penetrating channels combine independently.
    double penetratingSensorRange{};
    // Reference fitting ratings. Their operational effects are resolved by combat
    // and strategic_operations; special race/acquisition rules remain separate.
    double armor{};
    double shields{};
    double weaponPower{};
    std::uint8_t weaponRange{};
    std::uint8_t weaponInitiative{};
    double weaponAccuracy{};
    bool shieldOnly{};
    bool gatling{};
    bool missile{};
    double bombPopulationPercent{};
    std::uint32_t bombMinimumKills{};
    std::uint16_t bombInstallations{};
    bool smartBomb{};
    bool unterraformingBomb{};
    std::uint8_t bombContamination{}; // Suns! residual hazard per fitted bomb/year.
    double minesPerYear{};
    std::uint8_t mineFieldKind{}; // 0 standard, 1 heavy, 2 speed trap.
    double cloakPercent{};
    double jammingPercent{};
    double accuracyBonusPercent{};
    std::uint8_t initiativeBonus{};
    double beamBonusPercent{};
    double beamDeflectionPercent{};
    double battleMovementBonus{};
    double battleMovementPenalty{};
    double tachyonPercent{};
};

struct ShipComponentPlacement {
    ShipSlotId slot{};
    ShipComponentType component{ShipComponentType::FusionDrive};

    bool operator==(const ShipComponentPlacement&) const = default;
};

struct ShipDesign {
    ShipDesignId id{};
    PlayerId owner{};
    std::string name;
    ShipHullType hull{ShipHullType::Scout};
    std::vector<ShipComponentType> components;
    std::vector<ShipComponentPlacement> placements;
};

enum class OrbitalStationHullType : std::uint8_t { OrbitalDock, Starbase, Fortress };
enum class OrbitalStationModule : std::uint8_t { Shipyard, RefuelingDepot };

struct StationLoadout {
    OrbitalStationHullType hull{OrbitalStationHullType::OrbitalDock};
    std::vector<ShipComponentType> components;
    bool shipyard{true};
    bool refueling{true};
    bool operator==(const StationLoadout&) const = default;
};
struct StationConstruction {
    StationLoadout loadout;
    OrbitalStationId refitStation{}; // Zero builds; a nonzero stable ID refits that station.
    bool operator==(const StationConstruction&) const = default;
};

enum class ProductionKind {
    ColonyShip,
    Factory,
    Mine,
    Research,
    OrbitalStation,
    Terraforming,
    ColonyDefense,
    LocalRadar,
    PlanetaryRadar,
    ReconstructFactory,
    ReconstructMine,
    CleanupContamination,
    OrbitalMirrors,
    OrbitalShades,
    ResonanceLaboratory,
};

enum class ProductionAutomation : std::uint8_t { None, Annual, MinTerraform, MaxTerraform };
inline constexpr std::uint32_t kMaximumProductionBatch = 1000;

struct ProductionItem {
    ProductionKind kind{ProductionKind::ColonyShip};
    std::uint32_t remainingCost{};
    ShipDesignId shipDesign{};
    ProductionAutomation automation{ProductionAutomation::None};
    std::uint32_t quantity{1}; // Remaining units, or the annual limit for an auto item.
    bool autoGenerated{}; // A partially built unit generated by the following auto item.
    std::optional<StationConstruction> station;
    bool operator==(const ProductionItem&) const = default;
};

struct ProductionTemplate {
    std::string name;
    std::vector<ProductionItem> items;
    bool defaultForNewColonies{};
    bool operator==(const ProductionTemplate&) const = default;
};

struct ProductionCompletionEstimate {
    std::optional<std::uint64_t> completionTurn;
    bool beyondForecastHorizon{};
    std::optional<std::uint64_t> retiredTurn; // Recovery work made unnecessary by decay/earlier projects.
};

struct PlanetEnvironment {
    // Normalized physical indices: 50 is temperate / Earth-like for
    // temperature and gravity; radiation runs from quiet to severe.
    std::uint8_t temperature{50};
    std::uint8_t gravity{50};
    std::uint8_t radiation{15};
};

struct PrecursorArtifactSite {
    // `present` is authoritative simulation truth and must not be exposed by
    // ordinary scans. Claimed sites are retained as planetary history.
    bool present{};
    bool claimed{};
    PlayerId discoveredBy{};
    std::uint16_t researchPoints{};
};

enum class PlanetaryRadarLevel : std::uint8_t { None, Local, Full };
inline constexpr double planetary_radar_range(PlanetaryRadarLevel level)
{
    return level == PlanetaryRadarLevel::Full ? kColonySensorRange
        : level == PlanetaryRadarLevel::Local ? kLocalColonySensorRange : 0.0;
}

struct PlanetWarDamage {
    std::uint64_t factoriesDestroyed{}, minesDestroyed{};
    std::uint64_t factoriesReconstructed{}, minesReconstructed{};
    std::uint64_t lastBombardmentTurn{}, lastReconstructionTurn{};
    std::uint8_t contamination{}; // Independent of natural/terraformed environment, 0..100.
    std::uint64_t lastCleanupTurn{};
    bool operator==(const PlanetWarDamage&) const = default;
};
[[nodiscard]] constexpr bool reconstruction_kind(ProductionKind kind)
{ return kind==ProductionKind::ReconstructFactory || kind==ProductionKind::ReconstructMine; }
[[nodiscard]] constexpr bool recovery_kind(ProductionKind kind)
{ return reconstruction_kind(kind) || kind==ProductionKind::CleanupContamination; }

// Physical landmarks; withheld entirely from player exports.
struct UnknownObject {
    UnknownObjectId id{};
    Position position;
    UnknownObjectKind kind{UnknownObjectKind::AncientProbe};
    bool sampleRecovered{}; // Host-only finite physical stock, including lost/consumed samples.
};

struct ScienceSample {
    UnknownObjectId source{}; // One indivisible specimen per physical landmark, galaxy-wide.
    UnknownObjectKind kind{UnknownObjectKind::AncientProbe};
    bool retained{}; // Operates only at a populated colony with the required received observation.
    bool operator==(const ScienceSample&) const = default;
};

struct ObjectObservation {
    UnknownObjectId id{};
    Position position;
    double uncertainty{};
    ObjectKnowledgeLevel level{ObjectKnowledgeLevel::Detected};
    std::optional<UnknownObjectKind> kind;
    std::uint64_t observedTurn{}, receivedTurn{};
    bool operator==(const ObjectObservation&) const = default;
};

// Broad, stationary density profiles. Raw profiles belong to the host only.
struct InterstellarCloud {
    std::uint32_t id{};
    Position center;
    double radiusX{1}, radiusY{1}, density{};
    bool operator==(const InterstellarCloud&) const = default;
};
enum class MediumKnowledgeLevel : std::uint8_t { Detected, Characterized };
struct MediumObservation {
    std::uint32_t id{};
    Position center;
    double radiusX{1}, radiusY{1}, densityMinimum{}, densityMaximum{1};
    MediumKnowledgeLevel level{MediumKnowledgeLevel::Detected};
    std::uint64_t observedTurn{}, receivedTurn{};
    bool operator==(const MediumObservation&) const = default;
};

// Immutable evidence, not a copy of another empire's physical planet state.
struct PlanetAftermathObservation {
    PlanetId planet{};
    std::uint64_t observedTurn{}, receivedTurn{}; // receivedTurn is zero while in transit.
    std::uint64_t factoriesMinimum{}, factoriesMaximum{}, minesMinimum{}, minesMaximum{};
    std::uint8_t contaminationMinimum{}, contaminationMaximum{};
    bool exact{};
    bool operator==(const PlanetAftermathObservation&) const = default;
};

enum class PlanetaryEngineeringKind : std::uint8_t { None, OrbitalMirrors, OrbitalShades };
enum class EngineeringAction : std::uint8_t { StartOrResume, Pause, Dismantle, SetOutput };
enum class EngineeringBlock : std::uint8_t { None, NoProject, Paused, NoColony, MissingStation, Technology, Minerals };
struct PlanetaryEngineering {
    PlanetaryEngineeringKind kind{PlanetaryEngineeringKind::None};
    std::uint32_t work{};
    MineralCargo invested;
    std::uint8_t strength{10};
    bool operational{};
    PlayerId builder{};
    std::uint64_t startedTurn{}, completedTurn{};
};

// Whole 1 kt units. Deposit and surface cache are physical and independent.
struct XenocrystalSite {
    std::uint32_t original{}, remaining{}, surface{}, extractedLastYear{};
    bool operator==(const XenocrystalSite&) const = default;
};
struct XenocrystalObservation {
    XenocrystalSite site;
    std::uint64_t observedTurn{};
    bool depositSurveyed{};
    std::uint64_t depositObservedTurn{};
    std::uint8_t stockPhase{}; // 0: observation during the year; 1: completed planning boundary.
};
struct ResonanceLaboratory {
    bool started{}, active{true}, operational{};
    std::uint32_t work{}, crystalsPaid{};
    MineralCargo invested;
    PlayerId builder{};
    std::uint64_t startedTurn{}, completedTurn{};
};
enum class ResonanceAction : std::uint8_t { StartOrResume, Pause, Dismantle, SetActive };

struct Planet {
    PlanetId id{};
    StarId star{};
    std::string name;
    std::int32_t habitability{};
    PlayerId owner{};
    std::uint64_t population{};
    std::uint32_t industry{1};
    std::vector<ProductionItem> productionQueue;
    MineralCargo minerals;
    std::uint32_t mines{};
    bool productionWaitingForMinerals{};
    bool productionWaitingForShipyard{};
    PlanetEnvironment environment;
    PrecursorArtifactSite precursorArtifacts;
    // Values delivered in a player turn; absent in the authoritative simulation.
    std::optional<std::int32_t> observedHabitability;
    std::optional<MineralCargo> observedConcentration;
    std::optional<PlanetEnvironment> naturalEnvironment;
    std::uint16_t defenses{};
    PlanetaryRadarLevel radar{PlanetaryRadarLevel::None};
    bool homeworld{}; // A capital relay roots the shared sensor/communications mesh.
    PlanetWarDamage warDamage; // Physical history survives capture and abandonment.
    bool aftermathKnownLocally{true}; // False on foreign projections, even if ownership intel is stale.
    PlanetaryEngineering engineering;
    std::vector<ScienceSample> scienceSamples;
    XenocrystalSite xenocrystal;
    ResonanceLaboratory resonanceLab;
};

struct OrbitalStation {
    OrbitalStationId id{};
    PlayerId owner{};
    PlanetId planet{};
    std::string name;
    OrbitalStationHullType hull{OrbitalStationHullType::OrbitalDock};
    std::vector<OrbitalStationModule> modules;
    std::vector<ShipComponentType> components;
    double damagePercent{};
};

struct PendingStationContact {
    OrbitalStation lastContact;
    std::uint64_t observedTurn{};
    std::uint64_t deliveryTurn{};
};

enum class SurveyLevel : std::uint8_t {
    Detected,
    SystemScan,
    BasicScan,
    OrbitalSurvey,
    GeologicalSurvey,
    DeepSurvey,
};

struct PlanetSurveySnapshot {
    // Basic scans contain only a frozen rough Hab estimate. Orbital scans
    // retain the scalar baseline and environmental axes for known-cycle forecasts.
    std::int32_t habitability{};
    std::optional<PlanetEnvironment> environment;
    std::optional<PlanetEnvironment> naturalEnvironment;
    PlanetaryEngineering engineering; // Dated visible work; private investment is redacted.
    std::optional<XenocrystalObservation> xenocrystal; // DeepSurvey only, frozen at observation.
    ResonanceLaboratory resonanceLab; // Private project evidence captured for its owner only.
    std::optional<std::uint32_t> resonanceQueueIndex; // Frozen scheduling, including completion/removal.
};

struct SystemSurveyKnowledge {
    StarId star{};
    SurveyLevel level{SurveyLevel::Detected};
    std::uint64_t observedTurn{};
    // Ownership is a snapshot from the last orbital-quality observation, not
    // a live view into the authoritative simulation.
    std::optional<PlayerId> observedOwner;
    std::optional<PlanetSurveySnapshot> planetSnapshot;
};

struct StellarVariabilityIntel {
    bool variable{};
    bool characterized{};
    std::uint16_t periodTurns{};
    std::uint8_t amplitudePercent{};
    std::uint16_t phaseOffset{};
};

struct PendingSurveyReport {
    StarId star{};
    FleetId sourceFleet{}; // Zero means a stationary colony sensor source.
    std::uint64_t observedTurn{};
    std::uint64_t deliveryTurn{};
    SurveyLevel level{SurveyLevel::SystemScan};
    std::optional<PlayerId> observedOwner;
    FleetId transmissionFleet{}; // Nonzero only while awaiting an onboard transmission.
    std::optional<PlanetSurveySnapshot> planetSnapshot;
};

enum class ResearchField : std::uint8_t {
    Energy,
    Propulsion,
    Construction,
    Electronics,
    Biology,
    Weapons,
};

inline constexpr std::size_t kResearchFieldCount = 6;

enum class PlayerReportKind {
    FleetArrived,
    RouteCompleted,
    FleetStalledForFuel,
    ProductionCompleted,
    ColonyFounded,
    ProductionWaitingForMinerals,
    FleetTargetLost,
    FleetsMerged,
    ProductionWaitingForShipyard,
    PrecursorArtifactsDiscovered,
    ResearchLevelCompleted,
    GroundInvasionWon,
    GroundInvasionLost,
    GroundDefenseWon,
    ColonyLost,
    FreightDelivered,
    EnemyFleetDetected,
    EnemyFleetLost,
    FleetMobilityRestored,
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
};

// A report is an immutable observation, independent of surviving objects and
// designs. Only battle participants receive it, after communication delivery.
struct SpaceBattleUnit {
    PlayerId owner{};
    FleetId fleet{};
    ShipDesignId design{};
    std::string fleetName;
    std::string designName;
    std::uint32_t initialShips{};
    std::uint32_t survivingShips{};
    double remainingArmor{};
    double remainingShields{};
    OrbitalStationId station{}; // Exactly one of fleet/station identifies the combat unit.
};

struct SpaceBattleShot {
    std::uint8_t round{};
    std::uint32_t attacker{}; // Indices into SpaceBattleReport::units.
    std::uint32_t target{};
    ShipComponentType weapon{ShipComponentType::Laser};
    std::uint64_t fired{};
    std::uint64_t hits{};
    double range{};
    double shieldDamage{};
    double armorDamage{};
    std::uint32_t shipsDestroyed{};
};

struct SpaceBattleReport {
    std::uint64_t observedTurn{};
    Position position;
    std::uint8_t rounds{};
    bool stalemate{};
    bool logTruncated{};
    std::vector<SpaceBattleUnit> units;
    std::vector<SpaceBattleShot> shots;
};

enum class OperationalLossKind : std::uint8_t { Ground, Bombardment, Mines };
inline constexpr std::size_t kOperationalLossKindCount = 3;

// Immutable casualties, relative to the report recipient. These facts must
// survive changes of ownership and destruction of the referenced objects.
struct OperationalLossReport {
    OperationalLossKind kind{OperationalLossKind::Ground};
    std::uint64_t ownColonistsLost{};
    std::uint64_t opposingColonistsLost{};
    std::uint64_t ownInstallationsLost{};
    std::uint64_t opposingInstallationsLost{};
    std::uint64_t ownShipsLost{};
};

enum class ContactQuality : std::uint8_t { Probable, Confirmed, Identified };
enum class ContactSensor : std::uint8_t { Active, Passive };
struct SensorStatus {
    FleetId fleet{}; PlanetId colony{};
    Position position;
    std::uint64_t observedTurn{}, deliveryTurn{};
    std::uint8_t interferencePercent{};
    double nominalRange{}, activeRange{}, passiveRange{}, emissionRange{};
    FleetId transmissionFleet{};
};
// Immutable hull evidence. Lost records retain the date of their last position.
struct ReconContact {
    FleetId fleet{};
    PlayerId owner{}; // Unknown until confirmed; emissions never use this record.
    Position position;
    double uncertainty{};
    ContactQuality quality{ContactQuality::Probable};
    ContactSensor sensor{ContactSensor::Active};
    std::uint64_t observedTurn{}, lastSeenTurn{}, deliveryTurn{};
    FleetId sourceFleet{};
    PlanetId sourceColony{};
    StarId star{}; // Confirmed association with an observed system.
    double massMinimum{}, massMaximum{};
    std::uint32_t shipCount{}; // Only identified contacts reveal counts/hull classes.
    std::vector<ShipHullType> hulls;
    bool lost{}, jammed{};
    FleetId transmissionFleet{};
};

enum class SignalChannel : std::uint8_t { Fast, Slow };
struct EmissionObservation {
    std::uint64_t emittedTurn{};
    SignalChannel channel{SignalChannel::Fast};
};

// Player-facing operational facts travel independently from fleet telemetry.
// This keeps the simulation truth authoritative while ensuring that Turn
// Messages cannot reveal a remote result before its communication packet lands.
struct PendingPlayerReport {
    PlayerReportKind kind{PlayerReportKind::FleetArrived};
    std::uint64_t observedTurn{};
    std::uint64_t deliveryTurn{};
    StarId star{};
    PlanetId planet{};
    FleetId fleet{};
    ShipDesignId shipDesign{};
    ProductionKind productionKind{ProductionKind::ColonyShip};
    Position position;
    std::uint32_t quantity{};
    ResearchField researchField{ResearchField::Electronics};
    std::uint8_t technologyLevel{};
    MineralCargo deliveredMinerals;
    std::uint64_t deliveredColonists{};
    Position contactPosition;
    PlayerId contactOwner{};
    std::optional<SpaceBattleReport> battle;
    std::optional<OperationalLossReport> losses;
    std::optional<ReconContact> contact;
    std::optional<SensorStatus> sensorStatus;
    FleetId transmissionFleet{};
    std::optional<EmissionObservation> emission;
    std::optional<PlanetAftermathObservation> aftermath;
    std::optional<ObjectObservation> objectObservation;
    std::optional<MediumObservation> mediumObservation;
};

struct TechnologyState {
    std::array<std::uint8_t, kResearchFieldCount> levels{};
    std::array<std::uint32_t, kResearchFieldCount> progress{};
    ResearchField focus{ResearchField::Electronics};
    std::vector<ResearchField> queuedFocuses;
    bool researchActive{true};
    std::uint8_t researchAllocationPercent{};
};

// Stable core identity. Generalist and StargateSpecialist are playable;
// the remaining IDs are reserved for later structural rules.
enum class PrimaryRaceTrait : std::uint8_t {
    Generalist,
    StargateSpecialist,
    HabitatCivilization,
    RemoteLogisticsSpecialist,
};

struct RaceEnvironmentRange {
    std::uint8_t minimum{};
    std::uint8_t maximum{100};
};

struct RaceProfile {
    PrimaryRaceTrait primaryTrait{PrimaryRaceTrait::Generalist};
    double radiationTolerance{0.50};
    bool radiationImmune{};
    // Inclusive physical ranges that this race regards as naturally habitable.
    // They are stored independently from propulsion-radiation tolerance.
    RaceEnvironmentRange habitableTemperature{25, 75};
    RaceEnvironmentRange habitableGravity{30, 70};
    RaceEnvironmentRange habitableRadiation{0, 40};
    bool environmentBased{}; // Legacy campaigns retain their scalar habitability.
    // Propulsion access only; these do not imply a complete Stars! race trait.
    bool improvedFuelEfficiency{};
    bool noRamScoopEngines{};
    bool settlerEngineAccess{};
    HullAccess hullAccess{HullAccess::Standard};
    bool advancedRemoteMining{};
    bool basicRemoteMining{};
    bool legacyBiologyAdaptation{}; // Preserve pre-terraforming saves without changing their colonies.
};

struct ColonyTurnStatistics {
    PlanetId planet{};
    std::uint64_t population{};
    std::uint32_t factories{};
    std::uint32_t mines{};
    std::uint32_t productionOutput{};
    MineralCargo minerals;
    MineralCargo extraction; // Actual minerals mined during the preceding year.
    MineralCargo freightDelivered; // Fleet cargo unloaded here during the preceding year.
    std::uint64_t colonistsDelivered{};
};

struct ColonyExtraction {
    PlanetId planet{};
    PlayerId owner{}; // Owner when mining occurred; ownership may change before year end.
    MineralCargo minerals;
};

struct ColonyFreightDelivery {
    PlanetId planet{};
    PlayerId owner{}; // Owner at delivery, even if the colony changes hands later.
    MineralCargo minerals;
    std::uint64_t colonists{};
};

struct RemoteExtraction {
    PlanetId planet{};
    PlayerId owner{}; // Fleet owner when ore was deposited on the neutral surface.
    MineralCargo minerals;
};

struct RemoteMineTurnStatistics {
    PlanetId planet{};
    MineralCargo extraction;
};

struct FleetTurnStatistics {
    FleetId fleet{};
    std::uint32_t ships{};
    double grossMass{};
    double fuel{};
    MineralCargo minerals;
    std::uint64_t colonists{};
    double damagePercent{};
};

enum class HistoryMilestoneKind : std::uint8_t {
    ColonyFounded,
    ColonyLost,
    ResearchCompleted,
    FleetStalledForFuel,
    EnemyFleetDetected,
    EnemyFleetLost,
    GroundInvasionWon,
    GroundInvasionLost,
    GroundDefenseWon,
    SpaceBattle,
    GroundDefenseLost,
    Bombardment,
    MineStrike,
    EnemyFleetReclassified,
    SensorInterferenceChanged,
    Reconstruction,
    PlanetaryAftermath,
    ContaminationCleanup,
    UnknownObjectReported,
    UnknownObjectStudied,
    EngineeringStarted,
    EngineeringProgress,
    EngineeringCompleted,
    EngineeringPaused,
    EngineeringResumed,
    EngineeringCaptured,
    EngineeringDismantled,
};

[[nodiscard]] inline std::optional<OperationalLossKind> milestone_loss_kind(HistoryMilestoneKind kind)
{
    switch (kind) {
    case HistoryMilestoneKind::GroundInvasionWon:
    case HistoryMilestoneKind::GroundInvasionLost:
    case HistoryMilestoneKind::GroundDefenseWon:
    case HistoryMilestoneKind::GroundDefenseLost: return OperationalLossKind::Ground;
    case HistoryMilestoneKind::Bombardment: return OperationalLossKind::Bombardment;
    case HistoryMilestoneKind::MineStrike: return OperationalLossKind::Mines;
    default: return std::nullopt;
    }
}

struct HistoryMilestone {
    std::uint64_t eventId{};
    std::uint64_t observedTurn{};
    HistoryMilestoneKind kind{HistoryMilestoneKind::ColonyFounded};
    PlanetId planet{};
    ResearchField researchField{ResearchField::Electronics};
    std::uint8_t technologyLevel{};
    FleetId fleet{};
    UnknownObjectId object{};
};

// Totals from immutable space-battle reports received at this boundary.
// Opposing losses are confirmed casualties, not kills attributed to this player.
struct SpaceBattleTurnStatistics {
    std::uint64_t reports{};
    std::uint64_t ownShipsLost{};
    std::uint64_t opposingShipsLost{};
    std::uint64_t ownStationsLost{};
    std::uint64_t opposingStationsLost{};
};

struct OperationalLossTurnStatistics {
    bool recorded{}; // Initial and pre-v64 boundaries remain unknown.
    std::uint64_t reports{};
    std::uint64_t ownColonistsLost{};
    std::uint64_t opposingColonistsLost{};
    std::uint64_t ownInstallationsLost{};
    std::uint64_t opposingInstallationsLost{};
    std::uint64_t ownShipsLost{};
};

// Compact player history: owned assets and delivered reports only, never
// hidden authoritative truth. Safe for statistics, PBEM exports and replays.
struct EmpireTurnStatistics {
    std::uint64_t turn{};
    std::uint64_t population{};
    std::uint32_t colonies{};
    std::uint32_t factories{};
    std::uint32_t mines{};
    std::uint32_t productionOutput{};
    MineralCargo minerals;
    MineralCargo extraction;
    bool extractionRecorded{}; // False for the initial year and pre-v38 history.
    MineralCargo freightDelivered;
    std::uint64_t colonistsDelivered{};
    bool freightRecorded{}; // False for the initial year and pre-v41 history.
    MineralCargo remoteExtraction;
    bool remoteExtractionRecorded{}; // False for the initial year and pre-v42 history.
    std::uint32_t fleets{};
    std::uint32_t ships{};
    double fleetMass{};
    std::array<std::uint8_t, kResearchFieldCount> technologyLevels{};
    std::array<std::uint32_t, kResearchFieldCount> technologyProgress{};
    // Only colonies owned at this boundary; absent years are not zero values.
    std::vector<ColonyTurnStatistics> colonyHistory;
    // Only neutral sites actually mined by this player's fleets in this year.
    std::vector<RemoteMineTurnStatistics> remoteMineHistory;
    // Only owned fleets with current, immediately confirmed telemetry. Missing
    // years mean the fleet was out of contact or no longer existed.
    std::vector<FleetTurnStatistics> fleetHistory;
    bool fleetCargoRecorded{}; // False for pre-v49 history.
    bool fleetDamageRecorded{}; // False for pre-v50 history.
    // Only events delivered to this player at this planning boundary.
    std::vector<HistoryMilestone> milestones;
    SpaceBattleTurnStatistics battles;
    bool battleStatisticsRecorded{}; // Initial and pre-v63 boundaries are unknown.
    std::array<OperationalLossTurnStatistics, kOperationalLossKindCount> operationalLosses;
};

enum class WormholeSignature : std::uint8_t { Strong, Faint, Weak };
enum class WormholeStability : std::uint8_t { Unknown, Unstable, Variable, Stable };

struct WormholeKnowledge {
    WormholeEndpointId endpoint{};
    Position lastPosition;
    std::uint64_t observedTurn{};
    WormholeStability stability{WormholeStability::Unknown};
    WormholeEndpointId linkedEndpoint{}; // Learned only from a delivered emergence report.
    bool collapsed{};
};

enum class WormholeReportKind : std::uint8_t { Observation, Entered, Emerged, EntryMissed, Collapsed };
struct PendingWormholeReport {
    WormholeReportKind kind{WormholeReportKind::Observation};
    std::uint64_t observedTurn{};
    std::uint64_t deliveryTurn{};
    FleetId fleet{};
    WormholeKnowledge knowledge;
    FleetId transmissionFleet{};
};

// Host-only physical pulses. Receiver receipts prevent the same moving receiver
// from intercepting a pulse twice. Neither source identities nor pulses export.
struct EmissionReceipt { PlayerId player{}; FleetId fleet{}; PlanetId colony{}; };
struct EmissionPulse {
    std::uint32_t id{};
    PlayerId owner{};
    Position position;
    std::uint64_t emittedTurn{};
    SignalChannel channel{SignalChannel::Fast};
    double maximumRange{}, deceptionStrength{};
    FleetId fleet{};
    PlanetId colony{};
    std::vector<EmissionReceipt> receipts;
};
enum class StrategicObjectKind : std::uint8_t { Minefield, Wreck, Emission };
// Immutable sensor evidence, never a reference to current hidden truth.
struct StrategicIntel {
    std::uint32_t id{};
    StrategicObjectKind type{StrategicObjectKind::Minefield};
    Position position;
    double radius{};
    PlayerId owner{};
    std::uint64_t observedTurn{};
    std::uint64_t deliveryTurn{};
    double quantity{};
    std::uint8_t kind{};
    FleetId transmissionFleet{};
    std::optional<EmissionObservation> emission;
};
struct FieldScienceRecord {
    std::uint64_t key{};
    std::uint64_t deliveryTurn{};
    FleetId transmissionFleet{};
};

enum class PlayerController : std::uint8_t { Human, Computer };
inline constexpr std::size_t kMaximumComputerDecisionRecords = 32;
inline constexpr std::size_t kMaximumComputerDecisionReasonBytes = 240;
inline constexpr std::size_t kMaximumComputerFleetGoals = 128;

enum class ComputerFleetGoalKind : std::uint8_t {
    Survey, Colonize, Resupply, Gather, Defend, Raid, Bombard, Invade, InvestigateObject,
    ExoticMining, ExoticHauling
};

struct ComputerFleetGoal {
    FleetId fleet{};
    ComputerFleetGoalKind kind{ComputerFleetGoalKind::Survey};
    StarId target{}; // An UnknownObjectId for InvestigateObject; otherwise a StarId.
    std::uint64_t issuedTurn{};
    // Earliest retry after a command and its acknowledgement can make the trip.
    // This is an estimate from received knowledge, never a host packet deadline.
    std::uint64_t reviewTurn{};
    bool operator==(const ComputerFleetGoal&) const = default;
};

struct ComputerResearchGoal {
    ResearchField field{ResearchField::Electronics};
    std::uint8_t level{};
    bool operator==(const ComputerResearchGoal&) const = default;
};

struct ComputerDecision {
    std::uint64_t turn{};
    std::uint32_t orderCount{};
    std::string reason;
    bool operator==(const ComputerDecision&) const = default;
};

struct ComputerState {
    bool initialized{};
    std::vector<ComputerDecision> decisions;
    std::vector<ComputerFleetGoal> fleetGoals;
    std::optional<ComputerResearchGoal> researchGoal;
    bool operator==(const ComputerState&) const = default;
};

struct Player {
    PlayerId id{};
    std::string name;
    std::vector<StarId> surveyedStars;
    std::vector<SystemSurveyKnowledge> surveyKnowledge;
    std::vector<PendingSurveyReport> pendingSurveyReports;
    std::vector<PendingPlayerReport> pendingPlayerReports;
    // Host-only sensor transitions. The player export strips this physical
    // observation ledger until a queued briefing actually arrives.
    struct EnemyContact {
        FleetId fleet{};
        PlayerId owner{};
        Position lastPosition;
        FleetId reportingFleet{};
        PlanetId reportingColony{};
        ReconContact observation;
    };
    std::vector<EnemyContact> observedEnemyFleets;
    std::vector<ReconContact> enemyContacts;
    std::vector<ReconContact> pendingEnemyContacts;
    // Independent hull measurements of owned fleets; never refresh onboard
    // telemetry or imply that the target's radio is accepting commands.
    std::vector<ReconContact> friendlyFleetContacts, pendingFriendlyFleetContacts;
    std::vector<SensorStatus> sensorStatus, pendingSensorStatus, observedSensorStatus;
    RaceProfile race;
    TechnologyState technology;
    std::vector<EmpireTurnStatistics> history;
    std::vector<WormholeKnowledge> wormholeKnowledge;
    std::vector<PendingWormholeReport> pendingWormholeReports;
    std::vector<ProductionTemplate> productionTemplates;
    std::vector<StrategicIntel> strategicIntel;
    std::vector<StrategicIntel> pendingStrategicIntel;
    std::vector<FieldScienceRecord> fieldScience;
    PlayerController controller{PlayerController::Human};
    ComputerState computer;
    std::vector<VisitorObservation> visitorKnowledge;
    std::vector<PendingVisitorReport> pendingVisitorReports;
    std::vector<ShipComponentType> acquiredEquipment;
    std::vector<PlanetAftermathObservation> planetaryAftermath;
    std::vector<ObjectObservation> objectKnowledge;
    std::vector<MediumObservation> mediumKnowledge;
};

enum class FleetRole {
    Scout,
    ColonyShip,
};

enum class FleetTask {
    None,
    RemoteMining,
    Bombardment,
    LayMines,
    SweepMines,
    FieldResearch,
    Salvage,
    DeployOutpost,
    RecoverSample,
};

// A paid constructor hull becomes a permanent, fixed installation. It keeps
// its stable FleetId, fitted hull, cargo and the normal combat/contact model.
enum class OutpostKind : std::uint8_t { None, FuelDepot, CommunicationsRelay };

enum class EmissionMode : std::uint8_t { Standard, Passive, RadioSilence };
struct ElectronicsProgram {
    EmissionMode mode{EmissionMode::Standard};
    std::uint64_t resumeTurn{}; // Mandatory finite wake-up for radio silence.
    bool deception{}; // Deliberate false emissions; requires an active jammer.
    std::uint8_t burstInterval{}; // 0: none; otherwise a one-year window every N years.
    std::uint64_t firstBurstTurn{};
    bool activeBurst{};
    bool wakeOnArrival{};
};

enum class FleetArrivalActionKind {
    None,
    LoadAllAvailable,
    UnloadAll,
    Refuel,
    Colonize,
    RemoteMining,
    MergeWithFleet,
    EnterWormhole,
    LayMinefields,
    SweepMinefields,
    BombardColony,
    FieldResearch,
    RecoverWreckage,
    DeployOutpost,
    SupplyOutpost,
    RecoverSample,
};

// Work belongs to the final waypoint and starts autonomously there. Deployment
// is a one-time conversion; specimen recovery makes one attempt. Other work persists.
[[nodiscard]] constexpr std::optional<FleetTask> arrival_work_task(FleetArrivalActionKind kind)
{
    switch (kind) {
    case FleetArrivalActionKind::RemoteMining: return FleetTask::RemoteMining;
    case FleetArrivalActionKind::LayMinefields: return FleetTask::LayMines;
    case FleetArrivalActionKind::SweepMinefields: return FleetTask::SweepMines;
    case FleetArrivalActionKind::BombardColony: return FleetTask::Bombardment;
    case FleetArrivalActionKind::FieldResearch: return FleetTask::FieldResearch;
    case FleetArrivalActionKind::RecoverWreckage: return FleetTask::Salvage;
    case FleetArrivalActionKind::DeployOutpost: return FleetTask::DeployOutpost;
    case FleetArrivalActionKind::RecoverSample: return FleetTask::RecoverSample;
    default: return std::nullopt;
    }
}

[[nodiscard]] constexpr bool terminal_arrival_action(FleetArrivalActionKind kind)
{
    return arrival_work_task(kind).has_value() || kind == FleetArrivalActionKind::Colonize
        || kind == FleetArrivalActionKind::MergeWithFleet || kind == FleetArrivalActionKind::EnterWormhole;
}

enum class FleetCargoKind {
    Colonists,
    Ironium,
    Boranium,
    Germanium,
    All,
    Xenocrystal,
};

struct FleetArrivalAction {
    FleetArrivalActionKind kind{FleetArrivalActionKind::None};
    std::uint64_t reservePopulation{1};
    FleetCargoKind cargo{FleetCargoKind::Colonists};
    WormholeEndpointId wormholeEndpoint{};
    // Frozen at order creation; later template edits never change a voyage.
    std::optional<ProductionTemplate> colonyPlan;
    double reserveFuel{}; // SupplyOutpost only, in mg. Older action payloads stay unchanged.
    bool operator==(const FleetArrivalAction&) const = default;
};

struct FleetWaypoint {
    Position destination;
    std::uint8_t warp{kScoutCruiseWarp};
    FleetArrivalAction arrivalAction{};
    FleetId targetFleet{};
};

struct FleetRouteProgram {
    Position destination;
    std::uint8_t warp{kScoutCruiseWarp};
    FleetArrivalAction arrivalAction{};
    std::vector<FleetWaypoint> queuedWaypoints;
    bool clearRoute{};
    bool repeatOrders{};
    FleetId targetFleet{};
};

struct FleetShipStack {
    ShipDesignId design{};
    std::uint32_t count{};
};

// Persistent service policy; stable IDs deliberately survive missing clients.
struct MobileBaseProgram {
    bool enabled{true};
    double fuelReserve{100.0};
    std::vector<FleetId> clients;
    bool operator==(const MobileBaseProgram&) const = default;
};

enum class ProbeFlightMode : std::uint8_t { SafeCruise, Economical };
enum class ProbeAction : std::uint8_t { Configure, Stop, Resume, UpdateKnowledge };
enum class ProbeStatus : std::uint8_t { Idle, Travelling, Surveying, Returning, Complete, AwaitingFuel, Damaged, MissingEquipment, ManualPause, Stopped };
struct ProbeProgram {
    Position center;
    double radius{300};
    SurveyLevel depth{SurveyLevel::DeepSurvey};
    ProbeFlightMode flight{ProbeFlightMode::SafeCruise};
    std::uint8_t maxWarp{6};
    double fuelReserve{20};
    std::uint32_t maxSystems{12}, maxTurns{100};
    bool returnHome{true};
};
struct ProbeSurvey {
    StarId star{};
    SurveyLevel level{SurveyLevel::Detected};
    std::uint64_t observedTurn{};
};
struct ProbeCommand {
    ProbeAction action{ProbeAction::Configure};
    ProbeProgram program;
    std::vector<ProbeSurvey> knowledge; // Immutable HQ evidence at issue time.
};
struct ProbeMission {
    bool configured{}, enabled{};
    ProbeProgram program;
    ProbeStatus status{ProbeStatus::Idle};
    Position origin;
    std::uint64_t startedTurn{};
    StarId target{};
    std::vector<StarId> completed;
    std::vector<ProbeSurvey> surveys; // Host-only onboard knowledge, omitted from telemetry.
};

struct FleetTelemetry {
    std::uint64_t observedTurn{};
    Position position;
    std::optional<Position> destination;
    std::uint8_t warp{kScoutCruiseWarp};
    double fuel{};
    std::uint64_t colonists{};
    std::optional<FleetArrivalAction> arrivalAction;
    std::vector<FleetWaypoint> waypointQueue;
    MineralCargo minerals;
    FleetTask task{FleetTask::None};
    bool repeatOrders{};
    std::vector<FleetWaypoint> routeTemplate;
    std::vector<FleetShipStack> ships;
    FleetId targetFleet{};
    double damagePercent{};
    ElectronicsProgram electronics;
    OutpostKind outpost{OutpostKind::None};
    MobileBaseProgram mobileBase;
    std::uint64_t lastMovedTurn{};
    std::uint64_t lastServiceTurn{};
    std::vector<ScienceSample> scienceSamples;
    std::uint32_t xenocrystal{};
    ProbeMission probe;
    VisitorMission visitor;
};

struct PendingFleetCommand {
    std::uint64_t issuedTurn{};
    std::uint64_t deliveryTurn{};
    FleetRouteProgram program;
    std::optional<FleetTask> task;
    std::optional<ElectronicsProgram> electronics;
    std::optional<MobileBaseProgram> mobileBase;
    std::optional<ProbeCommand> probe;
    std::optional<VisitorMission> visitor;
};

struct PendingFleetTelemetry {
    std::uint64_t deliveryTurn{};
    FleetTelemetry telemetry;
};

struct Fleet {
    FleetId id{};
    PlayerId owner{};
    std::string name;
    FleetRole role{FleetRole::Scout};
    ShipDesignId design{kScoutDesignId};
    Position position;
    std::optional<Position> destination;
    std::uint8_t warp{kScoutCruiseWarp};
    double fuel{300.0};
    std::uint64_t colonists{};
    std::optional<FleetArrivalAction> arrivalAction;
    std::vector<FleetWaypoint> waypointQueue;
    MineralCargo minerals;

    // Simulation truth stays in the fields above. These trailing fields model
    // confirmed owner knowledge and command/telemetry packets physically in flight.
    std::vector<PendingFleetCommand> pendingCommands;
    FleetTelemetry telemetry;
    std::vector<PendingFleetTelemetry> telemetryInTransit;
    bool fuelStalled{};
    FleetTask task{FleetTask::None};
    bool repeatOrders{};
    std::vector<FleetWaypoint> routeTemplate;

    // Authoritative fleet composition. Empty means one ship of the legacy
    // `design` above and is accepted for old saves and compact test fixtures.
    std::vector<FleetShipStack> ships;

    // Non-zero means the active route follows this FleetId. `destination`
    // remains a resolved position snapshot for rendering and compatibility.
    FleetId targetFleet{};
    double damagePercent{};
    ElectronicsProgram electronics;
    OutpostKind outpost{OutpostKind::None};
    MobileBaseProgram mobileBase;
    std::uint64_t lastMovedTurn{};
    std::uint64_t lastServiceTurn{};
    std::vector<ScienceSample> scienceSamples;
    std::uint32_t xenocrystal{};
    ProbeMission probe;
    VisitorMission visitor;
};

struct Minefield {
    std::uint32_t id{};
    PlayerId owner{};
    Position position;
    double mines{};
    std::uint8_t kind{}; // Standard, heavy, speed trap.
};
struct SalvageWreck {
    std::uint32_t id{};
    Position position;
    std::uint64_t expiresTurn{};
    std::array<std::uint8_t, kResearchFieldCount> technology{};
};

struct PendingFleetLossContact {
    Fleet lastContact;
    std::uint64_t deliveryTurn{};
};

struct WormholeEndpoint {
    WormholeEndpointId id{};
    Position position;
    WormholeSignature signature{WormholeSignature::Strong};
    Position driftDirection;
};

struct Wormhole {
    std::array<WormholeEndpoint, 2> endpoints;
    std::uint64_t createdTurn{};
    std::uint64_t collapseTurn{};
    double stability{0.5};
};

// Balance lives in a persisted policy, independent of deterministic RNG state.
struct WormholeRules {
    double width{900.0};
    double height{650.0};
    double spawnChancePerTurn{0.025};
    std::uint32_t maximumPairs{3};
    std::uint32_t minimumLifetime{5};
    std::uint32_t maximumLifetime{30};
    double driftPerTurn{3.0};
    double relocationChance{0.025};
    double minimumLossChance{0.01};
    double instabilityLossChance{0.12};
    double detectorRiskMultiplier{0.65};
    std::uint32_t overdueGraceTurns{2};
    std::uint32_t presumedLostGraceTurns{5};
};

enum class WormholeTransitStatus : std::uint8_t { AwaitingEntryReport, AwaitingEmergence, Overdue, PresumedLost, EmergenceConfirmed };
struct WormholeTransit {
    // A retained contact remains available after authoritative destruction.
    // Pre-entry telemetry traffic is host-only and stripped from player views.
    // It contains no outcome flag or hidden exit coordinate.
    Fleet lastContact;
    WormholeEndpointId endpoint{};
    std::uint64_t enteredTurn{};
    std::uint64_t overdueTurn{};
    std::uint64_t presumedLostTurn{};
    WormholeTransitStatus status{WormholeTransitStatus::AwaitingEntryReport};
};

// Host-only logistics command in physical transit to a mobile base. No stocks
// are reserved or transferred until the command reaches a service phase.
struct PendingMobileBaseTransfer {
    PlayerId owner{};
    FleetId base{}, source{}, destination{};
    std::uint64_t issuedTurn{}, deliveryTurn{}, colonists{};
    MineralCargo minerals;
    double fuelAmount{}, sourceReserve{};
    bool fuelOnly{};
    std::uint32_t xenocrystal{};
};

struct GameState {
    std::uint64_t turn{1};
    std::uint64_t galaxySeed{};
    VisitorId nextVisitorId{1};
    std::vector<IntergalacticVisitor> visitors;
    UnknownObjectId nextUnknownObjectId{1};
    std::vector<UnknownObject> unknownObjects;
    bool interstellarMediumEnabled{};
    // Nonzero only for projections: density then uses that empire's received
    // profiles, never hidden physical clouds. Player files retain this marker.
    PlayerId mediumObserver{};
    std::vector<InterstellarCloud> interstellarClouds;
    std::vector<PendingMobileBaseTransfer> pendingMobileBaseTransfers;
    FleetId nextFleetId{1};
    ShipDesignId nextShipDesignId{kFirstCustomShipDesignId};
    OrbitalStationId nextOrbitalStationId{1};
    std::vector<Player> players;
    std::vector<ShipDesign> shipDesigns;
    std::vector<StarSystem> stars;
    std::vector<Planet> planets;
    std::vector<Fleet> fleets;
    std::vector<OrbitalStation> orbitalStations;
    std::vector<PendingStationContact> pendingStationContacts;
    WormholeEndpointId nextWormholeEndpointId{1};
    WormholeRules wormholeRules;
    std::vector<Wormhole> wormholes;
    std::vector<WormholeTransit> wormholeTransits;
    // Host-only pre-battle owner knowledge; never participates in simulation.
    std::vector<PendingFleetLossContact> pendingFleetLossContacts;
    std::uint32_t nextMinefieldId{1};
    std::vector<Minefield> minefields;
    std::uint32_t nextWreckId{1};
    std::vector<SalvageWreck> wrecks;
    std::uint32_t nextEmissionId{1};
    std::vector<EmissionPulse> emissions;
};

struct GalaxyConfig {
    std::uint64_t seed{20260817};
    std::size_t starCount{32};
    double width{400.0};
    double height{400.0};
    double minimumSeparation{15.0};
};

enum class GalaxySize : std::uint8_t { Tiny, Small, Medium, Large, Huge };
enum class GalaxyDensity : std::uint8_t { Sparse, Normal, Dense, Packed };
inline constexpr std::size_t kMaximumGalaxySystems = 1000;
[[nodiscard]] GalaxyConfig galaxy_preset(GalaxySize size, GalaxyDensity density, std::uint64_t seed);
[[nodiscard]] GalaxyConfig normalized_galaxy_config(GalaxyConfig config);

[[nodiscard]] const StarSystem* find_star(const GameState& state, StarId id);
[[nodiscard]] const Planet* find_planet(const GameState& state, PlanetId id);
[[nodiscard]] const Planet* find_planet_at_star(const GameState& state, StarId star);
[[nodiscard]] const Player* find_player(const GameState& state, PlayerId id);
[[nodiscard]] const ShipDesign* find_ship_design(const GameState& state, ShipDesignId id);
[[nodiscard]] const OrbitalStation* find_orbital_station(
    const GameState& state, OrbitalStationId id);
[[nodiscard]] const OrbitalStation* find_orbital_station_at_planet(
    const GameState& state, PlanetId planet);
[[nodiscard]] bool orbital_station_has_module(
    const OrbitalStation& station, OrbitalStationModule module);
[[nodiscard]] bool colony_has_orbital_service(
    const GameState& state, PlanetId planet, PlayerId owner, OrbitalStationModule module);
[[nodiscard]] const ShipDesign* fleet_design(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::vector<FleetShipStack> fleet_ship_stacks(const Fleet& fleet);
[[nodiscard]] std::uint32_t fleet_ship_count(const Fleet& fleet);
[[nodiscard]] std::uint32_t fleet_ship_count(const Fleet& fleet, ShipDesignId design);
void normalize_fleet_composition(Fleet& fleet);

[[nodiscard]] ShipHullSpec hull_spec(ShipHullType type);
[[nodiscard]] ShipComponentSpec component_spec(ShipComponentType type);
[[nodiscard]] ShipSlotCategory ship_component_slot_category(ShipComponentType component);
[[nodiscard]] std::vector<ShipComponentPlacement> autoplace_ship_components(
    ShipHullType hull, const std::vector<ShipComponentType>& components);
void normalize_ship_design_engine_bank(ShipDesign& design);
void normalize_ship_design_placement(ShipDesign& design);
[[nodiscard]] std::string ship_design_validation_error(const ShipDesign& design);
[[nodiscard]] std::size_t ship_design_engine_slots_used(const ShipDesign& design);
[[nodiscard]] std::size_t ship_design_general_slots_used(const ShipDesign& design);
[[nodiscard]] std::size_t ship_design_mining_slots_used(const ShipDesign& design);
[[nodiscard]] bool ship_design_valid(const ShipDesign& design);
[[nodiscard]] bool ship_design_available_to_player(
    const GameState& state, PlayerId player, const ShipDesign& design);
[[nodiscard]] double ship_design_mass(const ShipDesign& design);
[[nodiscard]] std::uint32_t ship_design_cost(const ShipDesign& design);
[[nodiscard]] MineralCargo component_mineral_cost(ShipComponentType component);
[[nodiscard]] MineralCargo ship_design_mineral_cost(const ShipDesign& design);
[[nodiscard]] double ship_design_speed(const ShipDesign& design);
[[nodiscard]] bool ship_design_has_scanner(const ShipDesign& design);
[[nodiscard]] double ship_design_sensor_range(const ShipDesign& design);
[[nodiscard]] double ship_design_communication_range(const ShipDesign& design);
[[nodiscard]] double ship_design_ordinary_sensor_range(const ShipDesign& design);
[[nodiscard]] double ship_design_penetrating_sensor_range(const ShipDesign& design);
[[nodiscard]] bool ship_design_can_colonize(const ShipDesign& design);
[[nodiscard]] bool ship_design_can_remote_mine(const ShipDesign& design);
[[nodiscard]] std::uint8_t ship_design_max_warp(const ShipDesign& design);
// Initial speed only; reference engines use their catalog optimum, legacy
// designs retain role-based defaults. Explicit/saved routes are never changed.
[[nodiscard]] std::uint8_t ship_design_cruise_warp(const ShipDesign& design);
[[nodiscard]] double ship_design_fuel_rate(const ShipDesign& design, std::uint8_t warp);
[[nodiscard]] double ship_design_overdrive_damage(
    const ShipDesign& design, std::uint8_t warp);
[[nodiscard]] double ship_design_fuel_capacity(const ShipDesign& design);
[[nodiscard]] double ship_design_cargo_capacity(const ShipDesign& design);
[[nodiscard]] double ship_design_fuel_generation(const ShipDesign& design);
[[nodiscard]] double ship_design_radiation_hazard(const ShipDesign& design);
[[nodiscard]] std::string research_field_name(ResearchField field);
[[nodiscard]] std::uint8_t technology_level(
    const GameState& state, PlayerId player, ResearchField field);
[[nodiscard]] std::uint32_t research_level_cost(ResearchField field, std::uint8_t level);
[[nodiscard]] bool component_available_to_player(
    const GameState& state, PlayerId player, ShipComponentType component);

[[nodiscard]] bool same_position(Position a, Position b);
[[nodiscard]] double distance_between(Position a, Position b);
[[nodiscard]] FleetEncounterGeometry analyze_fleet_encounter(
    Position pursuerStart,
    Position pursuerEnd,
    Position targetStart,
    Position targetEnd,
    double encounterRadius = kFleetEncounterRadius);
[[nodiscard]] double warp_distance(std::uint8_t warp);
[[nodiscard]] double colonist_cargo_mass(std::uint64_t colonists);
// The first ground-combat slice deliberately uses a compact population-only
// model. Equal forces have even odds; a numerical advantage grows rapidly but
// never guarantees victory while both sides have population remaining.
[[nodiscard]] double ground_invasion_success_chance(
    std::uint64_t attackers, std::uint64_t defenders);
[[nodiscard]] double mineral_cargo_mass(const MineralCargo& minerals);
[[nodiscard]] bool mineral_cargo_sufficient(const MineralCargo& available, const MineralCargo& required);
void subtract_minerals(MineralCargo& available, const MineralCargo& required);

[[nodiscard]] MineralCargo planet_mineral_concentration(const GameState& state, const Planet& planet);
[[nodiscard]] PlanetEnvironment generated_planet_environment(
    std::uint64_t galaxySeed, PlanetId planet, StarClass stellarClass);
[[nodiscard]] PrecursorArtifactSite generated_precursor_artifact_site(
    std::uint64_t galaxySeed, PlanetId planet);
[[nodiscard]] MineralCargo projected_mineral_mining(const GameState& state, const Planet& planet);
[[nodiscard]] MineralCargo projected_remote_mining(
    const GameState& state, const Planet& planet, const ShipDesign& design);
[[nodiscard]] MineralCargo production_item_mineral_cost(const GameState& state, const ProductionItem& item);
[[nodiscard]] MineralCargo fleet_colonization_salvage(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::vector<ProductionCompletionEstimate> forecast_production_queue(
    const GameState& state,
    const Planet& planet,
    const std::vector<ProductionItem>& queue,
    std::uint32_t maximumTurns = 10000);

[[nodiscard]] double fleet_speed(FleetRole role);
[[nodiscard]] double fleet_sensor_range(FleetRole role);
[[nodiscard]] std::uint32_t fleet_eta(const Fleet& fleet);
[[nodiscard]] double fleet_speed(const GameState& state, const Fleet& fleet);
[[nodiscard]] bool fleet_has_scanner(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_sensor_range(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_ordinary_sensor_range(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_communication_range(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_penetrating_sensor_range(const GameState& state, const Fleet& fleet);
[[nodiscard]] bool fleet_can_colonize(const GameState& state, const Fleet& fleet);
[[nodiscard]] bool fleet_can_remote_mine(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint8_t fleet_max_warp(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_generation(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_radiation_hazard(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint32_t fleet_eta(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_capacity(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_cargo_capacity(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_cargo_used(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_gross_mass(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_rate(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_fuel_change_for_distance(const GameState& state, const Fleet& fleet, double distance);
[[nodiscard]] double fleet_fuel_consumption_for_distance(const GameState& state, const Fleet& fleet, double distance);
[[nodiscard]] double fleet_fuel_affordable_distance(const GameState& state, const Fleet& fleet, double maximumDistance);
[[nodiscard]] std::uint8_t fleet_free_warp(const GameState& state, const Fleet& fleet);
[[nodiscard]] double fleet_overdrive_damage_rate(
    const GameState& state, const Fleet& fleet, std::uint8_t warp);
[[nodiscard]] bool fleet_warp_valid(const GameState& state, const Fleet& fleet, std::uint8_t warp);
[[nodiscard]] bool fleet_radiation_safe(const GameState& state, const Fleet& fleet);
[[nodiscard]] std::uint64_t projected_fleet_radiation_losses(const GameState& state, const Fleet& fleet);
void apply_fleet_radiation_attrition(GameState& state, Fleet& fleet);

[[nodiscard]] bool within_range(Position source, Position target, double range);
[[nodiscard]] std::uint32_t travel_turns(Position from, Position to, double speed);
[[nodiscard]] bool is_surveyed(const GameState& state, PlayerId player, StarId star);
[[nodiscard]] SurveyLevel survey_level(const GameState& state, PlayerId player, StarId star);
[[nodiscard]] std::optional<std::uint64_t> system_intel_age(
    const GameState& state, PlayerId player, StarId star);
[[nodiscard]] std::optional<PlayerId> known_planet_owner(
    const GameState& state, PlayerId player, PlanetId planet);
[[nodiscard]] std::optional<StellarVariabilityIntel> known_stellar_variability(
    const GameState& state, PlayerId player, StarId star);
[[nodiscard]] std::optional<std::int32_t> known_planet_habitability(
    const GameState& state, PlayerId player, PlanetId planet);
[[nodiscard]] bool planet_geology_known(const GameState& state, PlayerId player, PlanetId planet);
// nullopt means that a deep survey has not been completed. A true value is a
// deliberately vague archaeological hint, never the site's exact reward.
[[nodiscard]] std::optional<bool> known_precursor_artifact_hint(
    const GameState& state, PlayerId player, PlanetId planet);
void set_survey_level(
    GameState& state,
    PlayerId player,
    StarId star,
    SurveyLevel level,
    std::uint64_t observedTurn,
    bool captureObservation = true);
[[nodiscard]] std::optional<PlanetSurveySnapshot> capture_planet_survey(
    const GameState& state, PlayerId player, StarId star, SurveyLevel level,
    std::uint64_t observedTurn);
void mark_surveyed(GameState& state, PlayerId player, StarId star);
void refresh_sensor_intel(GameState& state);

[[nodiscard]] constexpr std::uint32_t production_cost(ProductionKind kind)
{
    switch (kind) {
    case ProductionKind::ColonyShip: return kColonyShipCost;
    case ProductionKind::Factory: return kFactoryCost;
    case ProductionKind::ReconstructFactory: return 4;
    case ProductionKind::ReconstructMine: return 3;
    case ProductionKind::CleanupContamination: return 6;
    case ProductionKind::Mine: return kMineCost;
    case ProductionKind::Research: return 0;
    case ProductionKind::OrbitalStation: return kOrbitalDockCost;
    case ProductionKind::ColonyDefense: return 5;
    case ProductionKind::LocalRadar: return 2;
    case ProductionKind::PlanetaryRadar: return 16;
    case ProductionKind::Terraforming: return kTerraformingCost;
    case ProductionKind::OrbitalMirrors: case ProductionKind::OrbitalShades: return 2400;
    case ProductionKind::ResonanceLaboratory: return 600;
    }
    return kColonyShipCost;
}

[[nodiscard]] std::uint32_t production_item_cost(const GameState& state, const ProductionItem& item);
[[nodiscard]] std::uint64_t population_capacity(const Planet& planet);
[[nodiscard]] std::uint64_t population_capacity(
    const GameState& state, const Planet& planet, std::uint64_t turn);
[[nodiscard]] std::int64_t projected_population_growth(const Planet& planet);
[[nodiscard]] std::int64_t projected_population_growth(
    const GameState& state, const Planet& planet, std::uint64_t turn);
[[nodiscard]] std::uint32_t colony_output(const Planet& planet);
[[nodiscard]] int stellar_habitability_bias(StarClass stellarClass);
[[nodiscard]] bool star_is_variable(const StarSystem& star);
[[nodiscard]] double stellar_luminosity(const StarSystem& star, std::uint64_t turn);
[[nodiscard]] std::int32_t current_planet_habitability(
    const GameState& state, const Planet& planet, std::uint64_t turn);
[[nodiscard]] EmpireTurnStatistics empire_turn_statistics(
    const GameState& state, PlayerId player);
void record_empire_turn_statistics(
    GameState& state, const std::vector<ColonyExtraction>& extraction = {}, bool elapsedYear = false,
    const std::vector<ColonyFreightDelivery>& freight = {},
    const std::vector<RemoteExtraction>& remoteExtraction = {});

[[nodiscard]] GameState generate_game(const GalaxyConfig& config);
GameState make_demo_game();

} // namespace suns
