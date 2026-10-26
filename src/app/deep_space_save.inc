// v82: keep deployment separate from the old fleet and telemetry layouts.
constexpr quint32 kOutpostMagic = 0x4f555450u; // OUTP
Fleet outpostSnapshot(const Fleet& carrier, const FleetTelemetry& t)
{
    auto f = carrier;
    f.position=t.position; f.destination=t.destination; f.warp=t.warp;
    f.fuel=t.fuel; f.damagePercent=t.damagePercent; f.colonists=t.colonists;
    f.arrivalAction=t.arrivalAction; f.waypointQueue=t.waypointQueue; f.task=t.task;
    f.repeatOrders=t.repeatOrders; f.routeTemplate=t.routeTemplate; f.targetFleet=t.targetFleet;
    f.outpost=t.outpost; if (!t.ships.empty()) f.ships=t.ships;
    return f;
}
bool outpostCarrierValid(const GameState& state, const Fleet& f)
{
    if (!outpost_state_valid(state,f) || !outpost_state_valid(state,outpostSnapshot(f,f.telemetry))) return false;
    for (const auto& t:f.telemetryInTransit) if (!outpost_state_valid(state,outpostSnapshot(f,t.telemetry))) return false;
    return true;
}
void writeOutposts(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload,QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.fleets.size()+state.pendingFleetLossContacts.size()+state.wormholeTransits.size());
    const auto write = [&](const Fleet& f) {
        if (!outpostCarrierValid(state,f)) { out.setStatus(QDataStream::WriteFailed); return; }
        out << quint32(f.id); writeEnum(out,f.outpost); writeEnum(out,f.telemetry.outpost);
        out << quint32(f.telemetryInTransit.size());
        for (const auto& t:f.telemetryInTransit) writeEnum(out,t.telemetry.outpost);
    };
    for (const auto& f:state.fleets) write(f);
    for (const auto& f:state.pendingFleetLossContacts) write(f.lastContact);
    for (const auto& f:state.wormholeTransits) write(f.lastContact);
    if (out.status()!=QDataStream::Ok) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kOutpostMagic << quint32(payload.size()); stream.writeRawData(payload.data(),payload.size());
}
void readOutposts(QDataStream& stream, GameState& state)
{
    quint32 magic{},size{},count{}; stream >> magic >> size;
    if (magic!=kOutpostMagic || size>64*1024*1024) { markCorrupt(stream); return; }
    QByteArray payload(size,'\0');
    if (stream.readRawData(payload.data(),size)!=int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in,count) || count!=state.fleets.size()+state.pendingFleetLossContacts.size()+state.wormholeTransits.size()) {
        markCorrupt(stream); return;
    }
    const auto read = [&](Fleet& f) {
        quint32 id{},packets{}; in >> id;
        if (id!=f.id || !readEnum(in,f.outpost,quint8(gReadSaveFormatVersion>=97?OutpostKind::OrbitalShipyard:OutpostKind::CommunicationsRelay))
            || !readEnum(in,f.telemetry.outpost,quint8(gReadSaveFormatVersion>=97?OutpostKind::OrbitalShipyard:OutpostKind::CommunicationsRelay))
            || !readCount(in,packets) || packets!=f.telemetryInTransit.size()) { markCorrupt(in); return; }
        for (auto& t:f.telemetryInTransit) if (!readEnum(in,t.telemetry.outpost,quint8(gReadSaveFormatVersion>=97?OutpostKind::OrbitalShipyard:OutpostKind::CommunicationsRelay))) return;
        if (!outpostCarrierValid(state,f)) markCorrupt(in);
    };
    for (auto& f:state.fleets) read(f);
    for (auto& f:state.pendingFleetLossContacts) read(f.lastContact);
    for (auto& f:state.wormholeTransits) read(f.lastContact);
    if (in.status()!=QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
