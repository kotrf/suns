// ORBI v97: physical state, immutable telemetry and command packets stay separate.
void writeOrbitalCommand(QDataStream& out, const OrbitalBaseCommand& c)
{
    if (!orbital_base_command_valid(c)) { out.setStatus(QDataStream::WriteFailed); return; }
    writeEnum(out,c.action); out << quint32(c.planet) << quint32(c.design) << quint64(c.item) << quint8(c.active);
}
void readOrbitalCommand(QDataStream& in, OrbitalBaseCommand& c)
{
    quint8 active{}; quint64 item{};
    if (!readEnum(in,c.action,quint8(OrbitalBaseAction::Refit))) return;
    in >> c.planet >> c.design >> item >> active; c.item=item; c.active=active;
    if (active>1 || !orbital_base_command_valid(c)) markCorrupt(in);
}
void writeOrbitalBase(QDataStream& out, const OrbitalBaseState& b)
{
    out << quint32(b.planet) << quint8(b.assemblyYears) << quint8(b.reserveYears)
        << quint8(b.active) << quint8(b.supported) << quint64(b.servicedTurn) << quint64(b.populationLost)
        << quint8(b.background) << quint8(b.exposure) << quint64(b.lastItemId) << quint32(b.acceptedItems.size());
    for(auto id:b.acceptedItems)out << quint64(id);
    out << quint32(b.queue.size());
    for (const auto& i:b.queue) out << quint32(i.design) << quint32(i.work) << quint8(i.refit) << quint64(i.id);
}
void readOrbitalBase(QDataStream& in, OrbitalBaseState& b, const GameState& s, PlayerId owner, std::uint64_t latest, OutpostKind kind)
{
    quint8 active{},supported{}; quint64 serviced{},lost{},lastItem{}; quint32 count{};
    in >> b.planet >> b.assemblyYears >> b.reserveYears >> active >> supported >> serviced >> lost >> b.background >> b.exposure >> lastItem; b.lastItemId=lastItem;
    b.active=active; b.supported=supported; b.servicedTurn=serviced; b.populationLost=lost;
    if (active>1 || supported>1 || b.assemblyYears>kOrbitalAssemblyYears || b.reserveYears>3
        || serviced>latest || b.background>100 || b.exposure>100) { markCorrupt(in); return; }
    if(!readCount(in,count))return;
    std::uint64_t previous=0;
    for(quint32 i=0;i<count;++i) {
        quint64 id{};in >> id;
        if(!id || id<=previous || id>b.lastItemId) {markCorrupt(in);return;}
        b.acceptedItems.push_back(id);previous=id;
    }
    if((b.lastItemId && (b.acceptedItems.empty() || b.acceptedItems.back()!=b.lastItemId)) || !readCount(in,count) || count>128) {markCorrupt(in);return;}
    if (kind==OutpostKind::OrbitalShipyard ? !find_planet(s,b.planet)
        : (b.planet || b.assemblyYears || b.reserveYears || serviced || lost || count || b.background || b.exposure || supported || b.lastItemId)) {
        markCorrupt(in); return;
    }
    bool hasRefit=false;
    for (quint32 index=0; index<count; ++index) {
        OrbitalBuildItem item; quint8 refit{}; quint64 id{};
        in >> item.design >> item.work >> refit >> id; item.id=id; item.refit=refit;
        const auto* d=find_ship_design(s,item.design);
        if (!std::binary_search(b.acceptedItems.begin(),b.acceptedItems.end(),item.id) || !item.id || item.id>b.lastItemId || std::any_of(b.queue.begin(),b.queue.end(),[&](auto& i){return i.id==item.id;}) || !d || d->owner!=owner || item.work>ship_design_cost(*d) || refit>1 || (refit && hasRefit)) { markCorrupt(in); return; }
        if (refit) {
            hasRefit=true;
            if (d->hull!=ShipHullType::ExpeditionBase || std::count(d->components.begin(),d->components.end(),ShipComponentType::OrbitalShipyardKit)!=1) { markCorrupt(in); return; }
        }
        b.queue.push_back(item);
    }
}
void writeOrbitalBases(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload,QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.fleets.size()+state.pendingFleetLossContacts.size()+state.wormholeTransits.size());
    const auto write=[&](const Fleet& f) {
        out << quint32(f.id) << quint8(f.orbitalShipyardBirth); writeOrbitalBase(out,f.orbitalBase); writeOrbitalBase(out,f.telemetry.orbitalBase);
        out << quint32(f.telemetryInTransit.size());
        for (const auto& p:f.telemetryInTransit) writeOrbitalBase(out,p.telemetry.orbitalBase);
        out << quint32(f.pendingCommands.size());
        for (const auto& p:f.pendingCommands) {
            out << quint8(p.orbitalBase.has_value());
            if (p.orbitalBase) writeOrbitalCommand(out,*p.orbitalBase);
        }
    };
    for (const auto& f:state.fleets) write(f);
    for (const auto& f:state.pendingFleetLossContacts) write(f.lastContact);
    for (const auto& f:state.wormholeTransits) write(f.lastContact);
    if (out.status()!=QDataStream::Ok) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << quint32(0x4f524249) << quint32(payload.size()); stream.writeRawData(payload.data(),payload.size());
}
void readOrbitalBases(QDataStream& stream, GameState& state)
{
    quint32 magic{},size{},count{}; stream >> magic >> size;
    if (magic!=0x4f524249 || size>64*1024*1024) { markCorrupt(stream); return; }
    QByteArray payload(size,'\0');
    if (stream.readRawData(payload.data(),size)!=int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in,count) || count!=state.fleets.size()+state.pendingFleetLossContacts.size()+state.wormholeTransits.size()) { markCorrupt(stream); return; }
    const auto read=[&](Fleet& f) {
        quint32 id{},packets{}; quint8 birth{}; in >> id >> birth; f.orbitalShipyardBirth=birth;
        if (birth>1) { markCorrupt(in); return; }
        if (id!=f.id) { markCorrupt(in); return; }
        readOrbitalBase(in,f.orbitalBase,state,f.owner,state.turn,f.outpost);
        readOrbitalBase(in,f.telemetry.orbitalBase,state,f.owner,f.telemetry.observedTurn,f.telemetry.outpost);
        if (!readCount(in,packets) || packets!=f.telemetryInTransit.size()) { markCorrupt(in); return; }
        for (auto& p:f.telemetryInTransit) readOrbitalBase(in,p.telemetry.orbitalBase,state,f.owner,p.telemetry.observedTurn,p.telemetry.outpost);
        if (!readCount(in,packets) || packets!=f.pendingCommands.size()) { markCorrupt(in); return; }
        for (auto& p:f.pendingCommands) {
            quint8 has{}; in >> has;
            if (has>1) { markCorrupt(in); return; }
            if (has) {
                OrbitalBaseCommand c; readOrbitalCommand(in,c); p.orbitalBase=c;
                if (p.task || p.electronics || p.mobileBase || p.probe || p.visitor) markCorrupt(in);
            }
        }
        if (f.outpost==OutpostKind::OrbitalShipyard) {
            const auto* p=find_planet(state,f.orbitalBase.planet); const auto* star=p?find_star(state,p->star):nullptr;
            if (!star || !same_position(f.position,star->position) || !std::isfinite(f.fuel)
                || f.fuel<0 || f.fuel>fleet_fuel_capacity(state,f)+1e-6
                || fleet_cargo_used(state,f)>fleet_cargo_capacity(state,f)+1e-6) markCorrupt(in);
        }
    };
    for (auto& f:state.fleets) read(f);
    for (const auto& f:state.fleets) if (f.outpost==OutpostKind::OrbitalShipyard) {
        if (find_orbital_station_at_planet(state,f.orbitalBase.planet)
            || std::count_if(state.fleets.begin(),state.fleets.end(),[&](const auto& other) {
                return other.outpost==OutpostKind::OrbitalShipyard && other.orbitalBase.planet==f.orbitalBase.planet;
            })!=1) markCorrupt(in);
    }
    for (auto& f:state.pendingFleetLossContacts) read(f.lastContact);
    for (auto& f:state.wormholeTransits) read(f.lastContact);
    if (in.status()!=QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
