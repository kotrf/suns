// v84: physical, confirmed and in-flight base policies are separate evidence.
constexpr quint32 kMobileBaseMagic = 0x4d4f4249u; // MOBI
void writeMobileBases(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload,QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.fleets.size()+state.pendingFleetLossContacts.size()+state.wormholeTransits.size());
    const auto write = [&](const Fleet& f) {
        out << quint32(f.id); writeMobileBaseState(out,f); writeMobileBaseState(out,f.telemetry);
        out << quint32(f.telemetryInTransit.size());
        for (const auto& t:f.telemetryInTransit) writeMobileBaseState(out,t.telemetry);
        out << quint32(f.pendingCommands.size());
        for (const auto& c:f.pendingCommands) {
            out << quint8(c.mobileBase.has_value());
            if (c.mobileBase) writeMobileBaseProgram(out,*c.mobileBase);
        }
    };
    for (const auto& f:state.fleets) write(f);
    for (const auto& f:state.pendingFleetLossContacts) write(f.lastContact);
    for (const auto& f:state.wormholeTransits) write(f.lastContact);
    out << quint32(state.pendingMobileBaseTransfers.size());
    for (const auto& p:state.pendingMobileBaseTransfers) {
        out << quint32(p.owner) << quint32(p.base) << quint32(p.source) << quint32(p.destination)
            << quint64(p.issuedTurn) << quint64(p.deliveryTurn) << quint64(p.colonists);
        writeMinerals(out,p.minerals); out << p.fuelAmount << p.sourceReserve << quint8(p.fuelOnly);
    }
    if (out.status()!=QDataStream::Ok) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kMobileBaseMagic << quint32(payload.size()); stream.writeRawData(payload.data(),payload.size());
}
void readMobileBases(QDataStream& stream, GameState& state)
{
    quint32 magic{},size{},count{}; stream >> magic >> size;
    if (magic!=kMobileBaseMagic || size>64*1024*1024) { markCorrupt(stream); return; }
    QByteArray payload(size,'\0');
    if (stream.readRawData(payload.data(),size)!=int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in,count) || count!=state.fleets.size()+state.pendingFleetLossContacts.size()+state.wormholeTransits.size()) {
        markCorrupt(stream); return;
    }
    const auto read = [&](Fleet& f) {
        quint32 id{},packets{}; in >> id;
        if (id!=f.id) { markCorrupt(in); return; }
        readMobileBaseState(in,f); readMobileBaseState(in,f.telemetry);
        if (!readCount(in,packets) || packets!=f.telemetryInTransit.size()) { markCorrupt(in); return; }
        for (auto& t:f.telemetryInTransit) readMobileBaseState(in,t.telemetry);
        if (!readCount(in,packets) || packets!=f.pendingCommands.size()) { markCorrupt(in); return; }
        for (auto& c:f.pendingCommands) {
            quint8 has{}; in >> has; if (has>1) { markCorrupt(in); return; }
            if (has) { MobileBaseProgram p; readMobileBaseProgram(in,p); c.mobileBase=p; if (c.task || c.electronics) markCorrupt(in); }
        }
        if (f.lastMovedTurn>state.turn || f.lastServiceTurn>state.turn) markCorrupt(in);
    };
    for (auto& f:state.fleets) read(f);
    for (auto& f:state.pendingFleetLossContacts) read(f.lastContact);
    for (auto& f:state.wormholeTransits) read(f.lastContact);
    if (!readCount(in,count)) { markCorrupt(stream); return; }
    for (quint32 i=0;i<count;++i) {
        PendingMobileBaseTransfer p; quint32 owner{},base{},source{},destination{}; quint64 issued{},delivery{},colonists{}; quint8 fuelOnly{};
        in >> owner >> base >> source >> destination >> issued >> delivery >> colonists;
        readMinerals(in,p.minerals); in >> p.fuelAmount >> p.sourceReserve >> fuelOnly;
        p.owner=owner; p.base=base; p.source=source; p.destination=destination; p.issuedTurn=issued; p.deliveryTurn=delivery; p.colonists=colonists; p.fuelOnly=fuelOnly;
        if (!owner || !base || !source || !destination || source==destination || (base!=source && base!=destination)
            || !find_player(state,owner) || issued>state.turn || delivery<issued || fuelOnly>1
            || !std::isfinite(p.fuelAmount) || p.fuelAmount<0 || !std::isfinite(p.sourceReserve) || p.sourceReserve<0
            || (p.fuelOnly && (p.fuelAmount<=0 || p.colonists || mineral_cargo_mass(p.minerals)>0))
            || (!p.fuelOnly && (p.fuelAmount!=0 || p.sourceReserve!=0))) { markCorrupt(in); break; }
        state.pendingMobileBaseTransfers.push_back(p);
    }
    if (in.status()!=QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
