#include "save_game.hpp"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>

#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace suns {

namespace {

constexpr quint32 kSaveMagic = 0x53554E53u; // "SUNS"
constexpr quint32 kSaveFormatVersion = 6;
constexpr quint32 kMaxCollectionItems = 100000;

void markCorrupt(QDataStream& stream)
{
    stream.setStatus(QDataStream::ReadCorruptData);
}

bool readCount(QDataStream& stream, quint32& count)
{
    stream >> count;
    if (stream.status() != QDataStream::Ok || count > kMaxCollectionItems) {
        markCorrupt(stream);
        return false;
    }
    return true;
}

template <typename Enum>
void writeEnum(QDataStream& stream, Enum value)
{
    stream << static_cast<quint8>(value);
}

template <typename Enum>
bool readEnum(QDataStream& stream, Enum& value, quint8 maxValue)
{
    quint8 raw{};
    stream >> raw;
    if (stream.status() != QDataStream::Ok || raw > maxValue) {
        markCorrupt(stream);
        return false;
    }
    value = static_cast<Enum>(raw);
    return true;
}

void writeString(QDataStream& stream, const std::string& value)
{
    stream << QString::fromStdString(value);
}

void readString(QDataStream& stream, std::string& value)
{
    QString text;
    stream >> text;
    value = text.toStdString();
}

void writePosition(QDataStream& stream, const Position& value)
{
    stream << value.x << value.y;
}

void readPosition(QDataStream& stream, Position& value)
{
    stream >> value.x >> value.y;
}

void writeMinerals(QDataStream& stream, const MineralCargo& value)
{
    stream << value.ironium << value.boranium << value.germanium;
}

void readMinerals(QDataStream& stream, MineralCargo& value)
{
    stream >> value.ironium >> value.boranium >> value.germanium;
}

void writeArrivalAction(QDataStream& stream, const FleetArrivalAction& value)
{
    writeEnum(stream, value.kind);
    stream << static_cast<quint64>(value.reservePopulation);
}

void readArrivalAction(QDataStream& stream, FleetArrivalAction& value)
{
    if (!readEnum(stream, value.kind, static_cast<quint8>(FleetArrivalActionKind::Colonize))) return;
    quint64 reserve{};
    stream >> reserve;
    value.reservePopulation = static_cast<std::uint64_t>(reserve);
}

void writeWaypoint(QDataStream& stream, const FleetWaypoint& value)
{
    writePosition(stream, value.destination);
    stream << static_cast<quint8>(value.warp);
    writeArrivalAction(stream, value.arrivalAction);
}

void readWaypoint(QDataStream& stream, FleetWaypoint& value)
{
    readPosition(stream, value.destination);
    quint8 warp{};
    stream >> warp;
    value.warp = static_cast<std::uint8_t>(warp);
    readArrivalAction(stream, value.arrivalAction);
}


void writeOptionalPosition(QDataStream& stream, const std::optional<Position>& value)
{
    stream << static_cast<quint8>(value.has_value() ? 1 : 0);
    if (value) writePosition(stream, *value);
}

bool readOptionalPosition(QDataStream& stream, std::optional<Position>& value)
{
    quint8 present{};
    stream >> present;
    if (present > 1) {
        markCorrupt(stream);
        return false;
    }
    value.reset();
    if (present) {
        Position position;
        readPosition(stream, position);
        value = position;
    }
    return stream.status() == QDataStream::Ok;
}

void writeRouteProgram(QDataStream& stream, const FleetRouteProgram& value)
{
    writePosition(stream, value.destination);
    stream << static_cast<quint8>(value.warp);
    writeArrivalAction(stream, value.arrivalAction);
    stream << static_cast<quint32>(value.queuedWaypoints.size());
    for (const auto& waypoint : value.queuedWaypoints) writeWaypoint(stream, waypoint);
    stream << static_cast<quint8>(value.clearRoute ? 1 : 0);
}

void readRouteProgram(QDataStream& stream, FleetRouteProgram& value)
{
    readPosition(stream, value.destination);
    quint8 warp{};
    stream >> warp;
    value.warp = static_cast<std::uint8_t>(warp);
    readArrivalAction(stream, value.arrivalAction);
    quint32 count{};
    if (!readCount(stream, count)) return;
    value.queuedWaypoints.clear();
    value.queuedWaypoints.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        FleetWaypoint waypoint;
        readWaypoint(stream, waypoint);
        if (stream.status() != QDataStream::Ok) return;
        value.queuedWaypoints.push_back(waypoint);
    }
    quint8 clearRoute{};
    stream >> clearRoute;
    if (clearRoute > 1) {
        markCorrupt(stream);
        return;
    }
    value.clearRoute = clearRoute != 0;
}

void writeTelemetry(QDataStream& stream, const FleetTelemetry& value)
{
    stream << static_cast<quint64>(value.observedTurn);
    writePosition(stream, value.position);
    writeOptionalPosition(stream, value.destination);
    stream << static_cast<quint8>(value.warp)
           << value.fuel
           << static_cast<quint64>(value.colonists);
    stream << static_cast<quint8>(value.arrivalAction.has_value() ? 1 : 0);
    if (value.arrivalAction) writeArrivalAction(stream, *value.arrivalAction);
    stream << static_cast<quint32>(value.waypointQueue.size());
    for (const auto& waypoint : value.waypointQueue) writeWaypoint(stream, waypoint);
    writeMinerals(stream, value.minerals);
}

void readTelemetry(QDataStream& stream, FleetTelemetry& value)
{
    quint64 observed{};
    stream >> observed;
    value.observedTurn = static_cast<std::uint64_t>(observed);
    readPosition(stream, value.position);
    if (!readOptionalPosition(stream, value.destination)) return;

    quint8 warp{};
    quint64 colonists{};
    stream >> warp >> value.fuel >> colonists;
    value.warp = static_cast<std::uint8_t>(warp);
    value.colonists = static_cast<std::uint64_t>(colonists);

    quint8 hasArrival{};
    stream >> hasArrival;
    if (hasArrival > 1) {
        markCorrupt(stream);
        return;
    }
    value.arrivalAction.reset();
    if (hasArrival) {
        FleetArrivalAction action;
        readArrivalAction(stream, action);
        if (stream.status() != QDataStream::Ok) return;
        value.arrivalAction = action;
    }

    quint32 count{};
    if (!readCount(stream, count)) return;
    value.waypointQueue.clear();
    value.waypointQueue.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        FleetWaypoint waypoint;
        readWaypoint(stream, waypoint);
        if (stream.status() != QDataStream::Ok) return;
        value.waypointQueue.push_back(waypoint);
    }
    readMinerals(stream, value.minerals);
}

void writePendingCommand(QDataStream& stream, const PendingFleetCommand& value)
{
    stream << static_cast<quint64>(value.issuedTurn)
           << static_cast<quint64>(value.deliveryTurn);
    writeRouteProgram(stream, value.program);
}

void readPendingCommand(QDataStream& stream, PendingFleetCommand& value)
{
    quint64 issued{};
    quint64 delivery{};
    stream >> issued >> delivery;
    value.issuedTurn = static_cast<std::uint64_t>(issued);
    value.deliveryTurn = static_cast<std::uint64_t>(delivery);
    readRouteProgram(stream, value.program);
}

void writePendingTelemetry(QDataStream& stream, const PendingFleetTelemetry& value)
{
    stream << static_cast<quint64>(value.deliveryTurn);
    writeTelemetry(stream, value.telemetry);
}

void readPendingTelemetry(QDataStream& stream, PendingFleetTelemetry& value)
{
    quint64 delivery{};
    stream >> delivery;
    value.deliveryTurn = static_cast<std::uint64_t>(delivery);
    readTelemetry(stream, value.telemetry);
}

void writeStar(QDataStream& stream, const StarSystem& value)
{
    stream << static_cast<quint32>(value.id);
    writeString(stream, value.name);
    writePosition(stream, value.position);
    writeEnum(stream, value.stellarClass);
}

void readStar(QDataStream& stream, StarSystem& value)
{
    quint32 id{};
    stream >> id;
    value.id = static_cast<StarId>(id);
    readString(stream, value.name);
    readPosition(stream, value.position);
    readEnum(stream, value.stellarClass, static_cast<quint8>(StarClass::Red));
}

void writeShipDesign(QDataStream& stream, const ShipDesign& value)
{
    stream << static_cast<quint32>(value.id) << static_cast<quint32>(value.owner);
    writeString(stream, value.name);
    writeEnum(stream, value.hull);
    stream << static_cast<quint32>(value.components.size());
    for (const auto component : value.components) writeEnum(stream, component);
}

void readShipDesign(QDataStream& stream, ShipDesign& value)
{
    quint32 id{};
    quint32 owner{};
    stream >> id >> owner;
    value.id = static_cast<ShipDesignId>(id);
    value.owner = static_cast<PlayerId>(owner);
    readString(stream, value.name);
    if (!readEnum(stream, value.hull, static_cast<quint8>(ShipHullType::MediumTransport))) return;

    quint32 count{};
    if (!readCount(stream, count)) return;
    value.components.clear();
    value.components.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        ShipComponentType component{};
        if (!readEnum(stream, component, static_cast<quint8>(ShipComponentType::AntimatterGenerator))) return;
        value.components.push_back(component);
    }
}

void writeProductionItem(QDataStream& stream, const ProductionItem& value)
{
    writeEnum(stream, value.kind);
    stream << static_cast<quint32>(value.remainingCost)
           << static_cast<quint32>(value.shipDesign);
}

void readProductionItem(QDataStream& stream, ProductionItem& value)
{
    if (!readEnum(stream, value.kind, static_cast<quint8>(ProductionKind::Mine))) return;
    quint32 cost{};
    quint32 design{};
    stream >> cost >> design;
    value.remainingCost = static_cast<std::uint32_t>(cost);
    value.shipDesign = static_cast<ShipDesignId>(design);
}

void writePlanet(QDataStream& stream, const Planet& value)
{
    stream << static_cast<quint32>(value.id)
           << static_cast<quint32>(value.star);
    writeString(stream, value.name);
    stream << static_cast<quint32>(value.habitability)
           << static_cast<quint32>(value.owner)
           << static_cast<quint64>(value.population)
           << static_cast<quint32>(value.industry)
           << static_cast<quint32>(value.stockpile);

    stream << static_cast<quint32>(value.productionQueue.size());
    for (const auto& item : value.productionQueue) writeProductionItem(stream, item);
    writeMinerals(stream, value.minerals);
    stream << static_cast<quint32>(value.mines)
           << static_cast<quint8>(value.productionWaitingForMinerals ? 1 : 0);
}

void readPlanet(QDataStream& stream, Planet& value)
{
    quint32 id{};
    quint32 star{};
    quint32 habitability{};
    quint32 owner{};
    quint64 population{};
    quint32 industry{};
    quint32 stockpile{};

    stream >> id >> star;
    readString(stream, value.name);
    stream >> habitability >> owner >> population >> industry >> stockpile;

    value.id = static_cast<PlanetId>(id);
    value.star = static_cast<StarId>(star);
    value.habitability = static_cast<std::uint32_t>(habitability);
    value.owner = static_cast<PlayerId>(owner);
    value.population = static_cast<std::uint64_t>(population);
    value.industry = static_cast<std::uint32_t>(industry);
    value.stockpile = static_cast<std::uint32_t>(stockpile);

    quint32 count{};
    if (!readCount(stream, count)) return;
    value.productionQueue.clear();
    value.productionQueue.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        ProductionItem item;
        readProductionItem(stream, item);
        if (stream.status() != QDataStream::Ok) return;
        value.productionQueue.push_back(item);
    }
    readMinerals(stream, value.minerals);

    quint32 mines{};
    quint8 waitingForMinerals{};
    stream >> mines >> waitingForMinerals;
    if (waitingForMinerals > 1) {
        markCorrupt(stream);
        return;
    }
    value.mines = static_cast<std::uint32_t>(mines);
    value.productionWaitingForMinerals = waitingForMinerals != 0;
}

void writePlayer(QDataStream& stream, const Player& value)
{
    stream << static_cast<quint32>(value.id);
    writeString(stream, value.name);
    stream << static_cast<quint32>(value.surveyedStars.size());
    for (const auto star : value.surveyedStars) stream << static_cast<quint32>(star);
    stream << static_cast<quint32>(value.pendingSurveyReports.size());
    for (const auto& report : value.pendingSurveyReports) {
        stream << static_cast<quint32>(report.star)
               << static_cast<quint32>(report.sourceFleet)
               << static_cast<quint64>(report.observedTurn)
               << static_cast<quint64>(report.deliveryTurn);
    }
    stream << static_cast<quint32>(value.pendingPlayerReports.size());
    for (const auto& report : value.pendingPlayerReports) {
        writeEnum(stream, report.kind);
        stream << static_cast<quint64>(report.observedTurn)
               << static_cast<quint64>(report.deliveryTurn)
               << static_cast<quint32>(report.star)
               << static_cast<quint32>(report.planet)
               << static_cast<quint32>(report.fleet)
               << static_cast<quint32>(report.shipDesign);
        writeEnum(stream, report.productionKind);
        writePosition(stream, report.position);
        stream << static_cast<quint32>(report.quantity);
    }
    stream << value.radiationTolerance << static_cast<quint8>(value.radiationImmune ? 1 : 0);
}

void readPlayer(QDataStream& stream, Player& value)
{
    quint32 id{};
    stream >> id;
    value.id = static_cast<PlayerId>(id);
    readString(stream, value.name);

    quint32 count{};
    if (!readCount(stream, count)) return;
    value.surveyedStars.clear();
    value.surveyedStars.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        quint32 star{};
        stream >> star;
        value.surveyedStars.push_back(static_cast<StarId>(star));
    }

    if (!readCount(stream, count)) return;
    value.pendingSurveyReports.clear();
    value.pendingSurveyReports.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        quint32 star{};
        quint32 sourceFleet{};
        quint64 observedTurn{};
        quint64 deliveryTurn{};
        stream >> star >> sourceFleet >> observedTurn >> deliveryTurn;
        value.pendingSurveyReports.push_back({
            static_cast<StarId>(star),
            static_cast<FleetId>(sourceFleet),
            static_cast<std::uint64_t>(observedTurn),
            static_cast<std::uint64_t>(deliveryTurn),
        });
    }

    if (!readCount(stream, count)) return;
    value.pendingPlayerReports.clear();
    value.pendingPlayerReports.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        PendingPlayerReport report;
        if (!readEnum(stream, report.kind, static_cast<quint8>(PlayerReportKind::ProductionWaitingForMinerals))) return;
        quint64 observedTurn{};
        quint64 deliveryTurn{};
        quint32 star{};
        quint32 planet{};
        quint32 fleet{};
        quint32 shipDesign{};
        stream >> observedTurn >> deliveryTurn >> star >> planet >> fleet >> shipDesign;
        if (!readEnum(stream, report.productionKind, static_cast<quint8>(ProductionKind::Mine))) return;
        readPosition(stream, report.position);
        quint32 quantity{};
        stream >> quantity;
        report.observedTurn = static_cast<std::uint64_t>(observedTurn);
        report.deliveryTurn = static_cast<std::uint64_t>(deliveryTurn);
        report.star = static_cast<StarId>(star);
        report.planet = static_cast<PlanetId>(planet);
        report.fleet = static_cast<FleetId>(fleet);
        report.shipDesign = static_cast<ShipDesignId>(shipDesign);
        report.quantity = static_cast<std::uint32_t>(quantity);
        value.pendingPlayerReports.push_back(report);
    }

    quint8 immune{};
    stream >> value.radiationTolerance >> immune;
    if (immune > 1) {
        markCorrupt(stream);
        return;
    }
    value.radiationImmune = immune != 0;
}

void writeFleet(QDataStream& stream, const Fleet& value)
{
    stream << static_cast<quint32>(value.id)
           << static_cast<quint32>(value.owner);
    writeString(stream, value.name);
    writeEnum(stream, value.role);
    stream << static_cast<quint32>(value.design);
    writePosition(stream, value.position);

    stream << static_cast<quint8>(value.destination.has_value() ? 1 : 0);
    if (value.destination) writePosition(stream, *value.destination);

    stream << static_cast<quint8>(value.warp)
           << value.fuel
           << static_cast<quint64>(value.colonists);

    stream << static_cast<quint8>(value.arrivalAction.has_value() ? 1 : 0);
    if (value.arrivalAction) writeArrivalAction(stream, *value.arrivalAction);

    stream << static_cast<quint32>(value.waypointQueue.size());
    for (const auto& waypoint : value.waypointQueue) writeWaypoint(stream, waypoint);
    writeMinerals(stream, value.minerals);

    stream << static_cast<quint32>(value.pendingCommands.size());
    for (const auto& command : value.pendingCommands) writePendingCommand(stream, command);
    writeTelemetry(stream, value.telemetry);
    stream << static_cast<quint32>(value.telemetryInTransit.size());
    for (const auto& packet : value.telemetryInTransit) writePendingTelemetry(stream, packet);
    stream << static_cast<quint8>(value.fuelStalled ? 1 : 0);
}

void readFleet(QDataStream& stream, Fleet& value)
{
    quint32 id{};
    quint32 owner{};
    stream >> id >> owner;
    value.id = static_cast<FleetId>(id);
    value.owner = static_cast<PlayerId>(owner);
    readString(stream, value.name);
    if (!readEnum(stream, value.role, static_cast<quint8>(FleetRole::ColonyShip))) return;

    quint32 design{};
    stream >> design;
    value.design = static_cast<ShipDesignId>(design);
    readPosition(stream, value.position);

    quint8 hasDestination{};
    stream >> hasDestination;
    if (hasDestination > 1) {
        markCorrupt(stream);
        return;
    }
    value.destination.reset();
    if (hasDestination) {
        Position destination;
        readPosition(stream, destination);
        value.destination = destination;
    }

    quint8 warp{};
    quint64 colonists{};
    stream >> warp >> value.fuel >> colonists;
    value.warp = static_cast<std::uint8_t>(warp);
    value.colonists = static_cast<std::uint64_t>(colonists);

    quint8 hasArrivalAction{};
    stream >> hasArrivalAction;
    if (hasArrivalAction > 1) {
        markCorrupt(stream);
        return;
    }
    value.arrivalAction.reset();
    if (hasArrivalAction) {
        FleetArrivalAction action;
        readArrivalAction(stream, action);
        if (stream.status() != QDataStream::Ok) return;
        value.arrivalAction = action;
    }

    quint32 count{};
    if (!readCount(stream, count)) return;
    value.waypointQueue.clear();
    value.waypointQueue.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        FleetWaypoint waypoint;
        readWaypoint(stream, waypoint);
        if (stream.status() != QDataStream::Ok) return;
        value.waypointQueue.push_back(waypoint);
    }
    readMinerals(stream, value.minerals);

    quint32 pendingCount{};
    if (!readCount(stream, pendingCount)) return;
    value.pendingCommands.clear();
    value.pendingCommands.reserve(pendingCount);
    for (quint32 index = 0; index < pendingCount; ++index) {
        PendingFleetCommand command;
        readPendingCommand(stream, command);
        if (stream.status() != QDataStream::Ok) return;
        value.pendingCommands.push_back(std::move(command));
    }

    readTelemetry(stream, value.telemetry);
    if (stream.status() != QDataStream::Ok) return;

    quint32 telemetryCount{};
    if (!readCount(stream, telemetryCount)) return;
    value.telemetryInTransit.clear();
    value.telemetryInTransit.reserve(telemetryCount);
    for (quint32 index = 0; index < telemetryCount; ++index) {
        PendingFleetTelemetry packet;
        readPendingTelemetry(stream, packet);
        if (stream.status() != QDataStream::Ok) return;
        value.telemetryInTransit.push_back(std::move(packet));
    }

    quint8 fuelStalled{};
    stream >> fuelStalled;
    if (fuelStalled > 1) {
        markCorrupt(stream);
        return;
    }
    value.fuelStalled = fuelStalled != 0;
}

template <typename Value, typename Writer>
void writeVector(QDataStream& stream, const std::vector<Value>& values, Writer writer)
{
    stream << static_cast<quint32>(values.size());
    for (const auto& value : values) writer(stream, value);
}

template <typename Value, typename Reader>
bool readVector(QDataStream& stream, std::vector<Value>& values, Reader reader)
{
    quint32 count{};
    if (!readCount(stream, count)) return false;
    values.clear();
    values.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        Value value;
        reader(stream, value);
        if (stream.status() != QDataStream::Ok) return false;
        values.push_back(std::move(value));
    }
    return true;
}

void writeGameState(QDataStream& stream, const GameState& value)
{
    stream << static_cast<quint64>(value.turn)
           << static_cast<quint64>(value.galaxySeed)
           << static_cast<quint32>(value.nextFleetId)
           << static_cast<quint32>(value.nextShipDesignId);
    writeVector(stream, value.players, writePlayer);
    writeVector(stream, value.shipDesigns, writeShipDesign);
    writeVector(stream, value.stars, writeStar);
    writeVector(stream, value.planets, writePlanet);
    writeVector(stream, value.fleets, writeFleet);
}

void readGameState(QDataStream& stream, GameState& value)
{
    quint64 turn{};
    quint64 seed{};
    quint32 nextFleet{};
    quint32 nextDesign{};
    stream >> turn >> seed >> nextFleet >> nextDesign;
    value.turn = static_cast<std::uint64_t>(turn);
    value.galaxySeed = static_cast<std::uint64_t>(seed);
    value.nextFleetId = static_cast<FleetId>(nextFleet);
    value.nextShipDesignId = static_cast<ShipDesignId>(nextDesign);

    if (!readVector(stream, value.players, readPlayer)) return;
    if (!readVector(stream, value.shipDesigns, readShipDesign)) return;
    if (!readVector(stream, value.stars, readStar)) return;
    if (!readVector(stream, value.planets, readPlanet)) return;
    readVector(stream, value.fleets, readFleet);
}

void writeGalaxyConfig(QDataStream& stream, const GalaxyConfig& value)
{
    stream << static_cast<quint64>(value.seed)
           << static_cast<quint32>(value.starCount)
           << value.width
           << value.height
           << value.minimumSeparation;
}

void readGalaxyConfig(QDataStream& stream, GalaxyConfig& value)
{
    quint64 seed{};
    quint32 starCount{};
    stream >> seed >> starCount >> value.width >> value.height >> value.minimumSeparation;
    value.seed = static_cast<std::uint64_t>(seed);
    value.starCount = static_cast<std::size_t>(starCount);
}

void writeOrder(QDataStream& stream, const Order& order)
{
    std::visit([&](const auto& concrete) {
        using T = std::decay_t<decltype(concrete)>;
        if constexpr (std::is_same_v<T, MoveFleetOrder>) {
            stream << quint8{0} << static_cast<quint32>(concrete.fleet);
            writePosition(stream, concrete.destination);
            stream << static_cast<quint8>(concrete.warp);
            writeArrivalAction(stream, concrete.arrivalAction);
            stream << static_cast<quint32>(concrete.queuedWaypoints.size());
            for (const auto& waypoint : concrete.queuedWaypoints) writeWaypoint(stream, waypoint);
        } else if constexpr (std::is_same_v<T, QueueProductionOrder>) {
            stream << quint8{1} << static_cast<quint32>(concrete.colony);
            writeEnum(stream, concrete.kind);
        } else if constexpr (std::is_same_v<T, CreateShipDesignOrder>) {
            stream << quint8{2};
            writeString(stream, concrete.name);
            writeEnum(stream, concrete.hull);
            stream << static_cast<quint32>(concrete.components.size());
            for (const auto component : concrete.components) writeEnum(stream, component);
        } else if constexpr (std::is_same_v<T, QueueShipDesignOrder>) {
            stream << quint8{3} << static_cast<quint32>(concrete.colony)
                   << static_cast<quint32>(concrete.design);
        } else if constexpr (std::is_same_v<T, SetFleetColonistsOrder>) {
            stream << quint8{4} << static_cast<quint32>(concrete.colony)
                   << static_cast<quint32>(concrete.fleet)
                   << static_cast<quint64>(concrete.colonists);
        } else if constexpr (std::is_same_v<T, SetFleetMineralCargoOrder>) {
            stream << quint8{5} << static_cast<quint32>(concrete.colony)
                   << static_cast<quint32>(concrete.fleet);
            writeMinerals(stream, concrete.minerals);
        } else if constexpr (std::is_same_v<T, RefuelFleetOrder>) {
            stream << quint8{6} << static_cast<quint32>(concrete.colony)
                   << static_cast<quint32>(concrete.fleet);
        } else if constexpr (std::is_same_v<T, ColonizePlanetOrder>) {
            stream << quint8{7} << static_cast<quint32>(concrete.fleet)
                   << static_cast<quint32>(concrete.planet);
        }
    }, order);
}

bool readOrder(QDataStream& stream, Order& order)
{
    quint8 tag{};
    stream >> tag;
    if (stream.status() != QDataStream::Ok) return false;

    switch (tag) {
    case 0: {
        MoveFleetOrder value;
        quint32 fleet{};
        quint8 warp{};
        stream >> fleet;
        value.fleet = static_cast<FleetId>(fleet);
        readPosition(stream, value.destination);
        stream >> warp;
        value.warp = static_cast<std::uint8_t>(warp);
        readArrivalAction(stream, value.arrivalAction);
        quint32 count{};
        if (!readCount(stream, count)) return false;
        value.queuedWaypoints.reserve(count);
        for (quint32 index = 0; index < count; ++index) {
            FleetWaypoint waypoint;
            readWaypoint(stream, waypoint);
            if (stream.status() != QDataStream::Ok) return false;
            value.queuedWaypoints.push_back(waypoint);
        }
        order = std::move(value);
        return stream.status() == QDataStream::Ok;
    }
    case 1: {
        QueueProductionOrder value;
        quint32 colony{};
        stream >> colony;
        value.colony = static_cast<PlanetId>(colony);
        if (!readEnum(stream, value.kind, static_cast<quint8>(ProductionKind::Mine))) return false;
        order = value;
        return true;
    }
    case 2: {
        CreateShipDesignOrder value;
        readString(stream, value.name);
        if (!readEnum(stream, value.hull, static_cast<quint8>(ShipHullType::MediumTransport))) return false;
        quint32 count{};
        if (!readCount(stream, count)) return false;
        value.components.reserve(count);
        for (quint32 index = 0; index < count; ++index) {
            ShipComponentType component{};
            if (!readEnum(stream, component, static_cast<quint8>(ShipComponentType::AntimatterGenerator))) return false;
            value.components.push_back(component);
        }
        order = std::move(value);
        return true;
    }
    case 3: {
        QueueShipDesignOrder value;
        quint32 colony{};
        quint32 design{};
        stream >> colony >> design;
        value.colony = static_cast<PlanetId>(colony);
        value.design = static_cast<ShipDesignId>(design);
        order = value;
        return stream.status() == QDataStream::Ok;
    }
    case 4: {
        SetFleetColonistsOrder value;
        quint32 colony{};
        quint32 fleet{};
        quint64 colonists{};
        stream >> colony >> fleet >> colonists;
        value.colony = static_cast<PlanetId>(colony);
        value.fleet = static_cast<FleetId>(fleet);
        value.colonists = static_cast<std::uint64_t>(colonists);
        order = value;
        return stream.status() == QDataStream::Ok;
    }
    case 5: {
        SetFleetMineralCargoOrder value;
        quint32 colony{};
        quint32 fleet{};
        stream >> colony >> fleet;
        value.colony = static_cast<PlanetId>(colony);
        value.fleet = static_cast<FleetId>(fleet);
        readMinerals(stream, value.minerals);
        order = value;
        return stream.status() == QDataStream::Ok;
    }
    case 6: {
        RefuelFleetOrder value;
        quint32 colony{};
        quint32 fleet{};
        stream >> colony >> fleet;
        value.colony = static_cast<PlanetId>(colony);
        value.fleet = static_cast<FleetId>(fleet);
        order = value;
        return stream.status() == QDataStream::Ok;
    }
    case 7: {
        ColonizePlanetOrder value;
        quint32 fleet{};
        quint32 planet{};
        stream >> fleet >> planet;
        value.fleet = static_cast<FleetId>(fleet);
        value.planet = static_cast<PlanetId>(planet);
        order = value;
        return stream.status() == QDataStream::Ok;
    }
    default:
        markCorrupt(stream);
        return false;
    }
}

void writePlayerOrders(QDataStream& stream, const PlayerOrders& value)
{
    stream << static_cast<quint32>(value.player)
           << static_cast<quint32>(value.orders.size());
    for (const auto& order : value.orders) writeOrder(stream, order);
}

void readPlayerOrders(QDataStream& stream, PlayerOrders& value)
{
    quint32 player{};
    stream >> player;
    value.player = static_cast<PlayerId>(player);

    quint32 count{};
    if (!readCount(stream, count)) return;
    value.orders.clear();
    value.orders.reserve(count);
    for (quint32 index = 0; index < count; ++index) {
        Order order = MoveFleetOrder{};
        if (!readOrder(stream, order)) return;
        value.orders.push_back(std::move(order));
    }
}

void writeDescriptions(QDataStream& stream, const QStringList& values)
{
    stream << static_cast<quint32>(values.size());
    for (const auto& value : values) stream << value;
}

void readDescriptions(QDataStream& stream, QStringList& values)
{
    quint32 count{};
    if (!readCount(stream, count)) return;
    values.clear();
    values.reserve(static_cast<qsizetype>(count));
    for (quint32 index = 0; index < count; ++index) {
        QString value;
        stream >> value;
        values.push_back(value);
    }
}

template <typename Id>
void writeOptionalId(QDataStream& stream, const std::optional<Id>& value)
{
    stream << static_cast<quint8>(value.has_value() ? 1 : 0);
    if (value) stream << static_cast<quint32>(*value);
}

template <typename Id>
void readOptionalId(QDataStream& stream, std::optional<Id>& value)
{
    quint8 hasValue{};
    stream >> hasValue;
    if (hasValue > 1) {
        markCorrupt(stream);
        return;
    }
    value.reset();
    if (!hasValue) return;
    quint32 raw{};
    stream >> raw;
    value = static_cast<Id>(raw);
}

bool stateContainsStar(const GameState& state, StarId id)
{
    return std::any_of(state.stars.begin(), state.stars.end(), [id](const StarSystem& star) {
        return star.id == id;
    });
}

bool stateContainsFleet(const GameState& state, FleetId id)
{
    return std::any_of(state.fleets.begin(), state.fleets.end(), [id](const Fleet& fleet) {
        return fleet.id == id;
    });
}

} // namespace

bool write_save_game_file(const QString& filePath, const SaveGameData& data, QString& errorMessage)
{
    errorMessage.clear();
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        errorMessage = QString("Cannot open %1 for writing: %2").arg(filePath, file.errorString());
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_4);
    stream << kSaveMagic << kSaveFormatVersion;
    writeGalaxyConfig(stream, data.galaxyConfig);
    writeGameState(stream, data.state);
    writePlayerOrders(stream, data.pendingOrders);
    writeDescriptions(stream, data.pendingDescriptions);
    writeOptionalId(stream, data.selectedStar);
    writeOptionalId(stream, data.selectedFleet);
    stream << static_cast<quint8>(data.showSensorRanges ? 1 : 0);

    if (stream.status() != QDataStream::Ok) {
        file.cancelWriting();
        errorMessage = QString("Failed while serializing save game to %1").arg(filePath);
        return false;
    }
    if (!file.commit()) {
        errorMessage = QString("Cannot commit save game %1: %2").arg(filePath, file.errorString());
        return false;
    }
    return true;
}

bool read_save_game_file(const QString& filePath, SaveGameData& data, QString& errorMessage)
{
    errorMessage.clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = QString("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_4);

    quint32 magic{};
    quint32 version{};
    stream >> magic >> version;
    if (stream.status() != QDataStream::Ok || magic != kSaveMagic) {
        errorMessage = "This is not a Suns! save file, or the file is damaged.";
        return false;
    }
    if (version != kSaveFormatVersion) {
        errorMessage = QString("Unsupported Suns! save version %1 (this prototype build reads version %2 only).")
                           .arg(version)
                           .arg(kSaveFormatVersion);
        return false;
    }

    SaveGameData loaded;
    readGalaxyConfig(stream, loaded.galaxyConfig);
    readGameState(stream, loaded.state);
    readPlayerOrders(stream, loaded.pendingOrders);
    readDescriptions(stream, loaded.pendingDescriptions);
    readOptionalId(stream, loaded.selectedStar);
    readOptionalId(stream, loaded.selectedFleet);
    quint8 showSensors{};
    stream >> showSensors;
    if (showSensors > 1) markCorrupt(stream);
    loaded.showSensorRanges = showSensors != 0;

    if (stream.status() != QDataStream::Ok) {
        errorMessage = "The Suns! save file is truncated or contains invalid data.";
        return false;
    }
    if (loaded.pendingDescriptions.size() != static_cast<qsizetype>(loaded.pendingOrders.orders.size())) {
        errorMessage = "The Suns! save file has an inconsistent planning-order section.";
        return false;
    }
    if (loaded.state.turn == 0 || loaded.state.players.empty() || loaded.state.stars.empty()) {
        errorMessage = "The Suns! save file does not contain a valid game state.";
        return false;
    }

    // UI context is expendable. If a later build removes a selected object,
    // keep the game loadable and simply fall back to a valid map selection.
    if (loaded.selectedStar && !stateContainsStar(loaded.state, *loaded.selectedStar)) loaded.selectedStar.reset();
    if (loaded.selectedFleet && !stateContainsFleet(loaded.state, *loaded.selectedFleet)) loaded.selectedFleet.reset();

    data = std::move(loaded);
    return true;
}

} // namespace suns
