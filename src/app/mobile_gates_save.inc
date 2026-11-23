constexpr quint32 kMobileGatesMagic = 0x4d474154; // MGAT

void writeMobileGateTransit(QDataStream& stream, const MobileGateTransit& transit)
{
    if (!mobile_gate_transit_valid(transit)) { stream.setStatus(QDataStream::WriteFailed); return; }
    for (const auto& endpoint : {transit.source, transit.destination}) {
        stream << quint32(endpoint.colony) << quint32(endpoint.base);
        writePosition(stream, endpoint.position);
    }
}

bool readMobileGateTransit(QDataStream& stream, MobileGateTransit& transit)
{
    for (auto* endpoint : {&transit.source, &transit.destination}) {
        stream >> endpoint->colony >> endpoint->base;
        readPosition(stream, endpoint->position);
    }
    if (!mobile_gate_transit_valid(transit)) markCorrupt(stream);
    return stream.status() == QDataStream::Ok;
}

void writeMobileGateCommands(QDataStream& stream, const GameState& state, bool playerTurn)
{
    const auto write = [&](const std::optional<MobileGateTransit>& transit) {
        if (playerTurn && transit) { stream.setStatus(QDataStream::WriteFailed); return; }
        stream << quint8(transit.has_value());
        if (transit) writeMobileGateTransit(stream, *transit);
    };
    stream << kMobileGatesMagic << quint32(state.fleets.size());
    for (const auto& fleet : state.fleets) {
        stream << quint32(fleet.id);
        write(fleet.mobileGate);
        stream << quint32(fleet.pendingCommands.size());
        for (const auto& command : fleet.pendingCommands) write(command.mobileGate);
    }
}

void readMobileGateCommands(QDataStream& stream, GameState& state, bool playerTurn)
{
    const auto read = [&](std::optional<MobileGateTransit>& transit) {
        quint8 present{};
        stream >> present;
        if (present > 1 || (present && playerTurn)) { markCorrupt(stream); return; }
        if (present) {
            MobileGateTransit value;
            if (readMobileGateTransit(stream, value)) transit = value;
        }
    };
    quint32 magic{}, fleets{};
    stream >> magic;
    if (magic != kMobileGatesMagic || !readCount(stream, fleets) || fleets != state.fleets.size()) {
        markCorrupt(stream);
        return;
    }
    for (auto& fleet : state.fleets) {
        quint32 id{}, packets{};
        stream >> id;
        read(fleet.mobileGate);
        if (id != fleet.id || !readCount(stream, packets) || packets != fleet.pendingCommands.size()) {
            markCorrupt(stream);
            return;
        }
        for (auto& command : fleet.pendingCommands) {
            read(command.mobileGate);
            const auto& route = command.program;
            if (command.mobileGate && (command.task || command.electronics || command.mobileBase || command.probe
                || command.visitor || command.orbitalBase || command.recoveryTarget || command.issuedTurn > state.turn
                || command.deliveryTurn <= state.turn || command.deliveryTurn <= command.issuedTurn
                || route.clearRoute || route.repeatOrders || route.targetFleet || !route.queuedWaypoints.empty()
                || route.arrivalAction.kind != FleetArrivalActionKind::None || route.destination != Position{})) {
                markCorrupt(stream);
                return;
            }
            if (stream.status() != QDataStream::Ok) return;
        }
    }
}
