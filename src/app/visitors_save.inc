// v89: neutral physical visitors, immutable reports and delayed missions.
void writeVisitorObservation(QDataStream& out, const VisitorObservation& x)
{
    out << quint32(x.id); writePosition(out,x.position); writePosition(out,x.velocity);
    out << quint64(x.observedTurn) << quint64(x.receivedTurn) << quint64(x.departureTurn) << quint8(x.available) << quint8(x.departed);
}
void readVisitorObservation(QDataStream& in, VisitorObservation& x, std::uint64_t latest)
{
    quint8 available{},departed{};
    in >> x.id; readPosition(in,x.position); readPosition(in,x.velocity);
    quint64 observed{},received{},departure{}; in >> observed >> received >> departure >> available >> departed;
    x.observedTurn=observed; x.receivedTurn=received; x.departureTurn=departure;
    x.available=available; x.departed=departed;
    if (available>1 || departed>1 || !visitor_observation_valid(x) || x.observedTurn>latest || x.receivedTurn>latest) markCorrupt(in);
}
void writeVisitorMission(QDataStream& out, const VisitorMission& m)
{ out << quint8(bool(m.track.id)); if(m.track.id) { writeVisitorObservation(out,m.track); out << quint8(m.warp); } }
void readVisitorMission(QDataStream& in, VisitorMission& m, std::uint64_t latest)
{
    quint8 present{}; in >> present; if(present>1) markCorrupt(in);
    if(present) { readVisitorObservation(in,m.track,latest); in >> m.warp; if(!m.warp || m.warp>kMaxWarp) markCorrupt(in); }
}
void writeVisitors(QDataStream& stream, const GameState& s)
{
    QByteArray payload; QDataStream out(&payload,QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(s.nextVisitorId);
    writeVector(out,s.visitors,[](QDataStream& out,const IntergalacticVisitor& v) {
        out << quint32(v.id); writePosition(out,v.position); writePosition(out,v.velocity);
        out << quint64(v.arrivalTurn) << quint64(v.departureTurn) << quint32(v.customer);
    });
    out << quint32(s.players.size());
    for(const auto& p:s.players) {
        out << quint32(p.id); writeVector(out,p.visitorKnowledge,writeVisitorObservation);
        writeVector(out,p.pendingVisitorReports,[](QDataStream& out,const PendingVisitorReport& r) {
            writeVisitorObservation(out,r.observation); writeEnum(out,r.kind);
            out << quint64(r.observedTurn) << quint64(r.deliveryTurn) << quint32(r.transmissionFleet);
        });
        writeVector(out,p.acquiredEquipment,[](QDataStream& out,ShipComponentType c){writeEnum(out,c);});
    }
    out << quint32(s.fleets.size()+s.pendingFleetLossContacts.size()+s.wormholeTransits.size());
    const auto write=[&](const Fleet& f) {
        out << quint32(f.id); writeVisitorMission(out,f.visitor); writeVisitorMission(out,f.telemetry.visitor);
        out << quint32(f.telemetryInTransit.size()); for(const auto& packet:f.telemetryInTransit) writeVisitorMission(out,packet.telemetry.visitor);
        out << quint32(f.pendingCommands.size()); for(const auto& c:f.pendingCommands) {
            out << quint8(c.visitor.has_value()); if(c.visitor) writeVisitorMission(out,*c.visitor);
        }
    };
    for(const auto& f:s.fleets) write(f);
    for(const auto& f:s.pendingFleetLossContacts) write(f.lastContact);
    for(const auto& f:s.wormholeTransits) write(f.lastContact);
    if(out.status()!=QDataStream::Ok || payload.size()>32*1024*1024) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << quint32{0x56495349} << quint32(payload.size()); stream.writeRawData(payload.data(),payload.size());
}
void readVisitors(QDataStream& stream, GameState& s, bool playerTurn)
{
    quint32 magic{},size{},count{}; stream >> magic >> size;
    if(magic!=0x56495349 || size>32U*1024U*1024U) { markCorrupt(stream); return; }
    QByteArray payload(size,'\0'); if(stream.readRawData(payload.data(),int(size))!=int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version()); in >> s.nextVisitorId;
    if(!s.nextVisitorId || s.nextVisitorId>0x7fffffffU || !readCount(in,count) || count>128 || (playerTurn && (count || s.nextVisitorId!=1))) { markCorrupt(stream); return; }
    std::vector<VisitorId> ids;
    for(quint32 n=0;n<count;++n) {
        IntergalacticVisitor v; in >> v.id; readPosition(in,v.position); readPosition(in,v.velocity);
        quint64 arrival{},departure{}; in >> arrival >> departure >> v.customer; v.arrivalTurn=arrival; v.departureTurn=departure;
        VisitorObservation x{v.id,v.position,v.velocity,std::min(s.turn,v.departureTurn),0,v.departureTurn,false,s.turn>=v.departureTurn};
        if(!visitor_observation_valid(x) || v.id>=s.nextVisitorId || !v.arrivalTurn || v.arrivalTurn>s.turn
            || v.departureTurn<=v.arrivalTurn || (v.customer && !find_player(s,v.customer))
            || std::find(ids.begin(),ids.end(),v.id)!=ids.end()) markCorrupt(in);
        ids.push_back(v.id); s.visitors.push_back(v);
    }
    if(!readCount(in,count) || count!=s.players.size()) { markCorrupt(stream); return; }
    for(auto& p:s.players) {
        quint32 id{}; in >> id;
        if(id!=p.id || !readCount(in,count) || count>128) { markCorrupt(stream); return; }
        ids.clear();
        for(quint32 n=0;n<count;++n) {
            VisitorObservation x; readVisitorObservation(in,x,s.turn);
            if(!x.receivedTurn || std::find(ids.begin(),ids.end(),x.id)!=ids.end()) markCorrupt(in);
            ids.push_back(x.id); p.visitorKnowledge.push_back(x);
        }
        if(!readCount(in,count) || count>10000 || (playerTurn && count)) { markCorrupt(stream); return; }
        for(quint32 n=0;n<count;++n) {
            PendingVisitorReport r; readVisitorObservation(in,r.observation,s.turn);
            if(!readEnum(in,r.kind,quint8(VisitorReportKind::Departed))) { markCorrupt(stream); return; }
            quint64 observed{},delivery{}; in >> observed >> delivery >> r.transmissionFleet; r.observedTurn=observed; r.deliveryTurn=delivery;
            if(!r.observedTurn || r.observedTurn>s.turn || r.observation.observedTurn>r.observedTurn
                || r.deliveryTurn<r.observedTurn || r.observation.receivedTurn
                || bool(r.transmissionFleet)!=(r.deliveryTurn==kAwaitingTransmission)) markCorrupt(in);
            p.pendingVisitorReports.push_back(r);
        }
        if(!readCount(in,count) || count>1) { markCorrupt(stream); return; }
        for(quint32 n=0;n<count;++n) { ShipComponentType c;
            if(!readEnum(in,c,quint8(kLastShipComponent))) { markCorrupt(stream); return; }
            if(c!=kVisitorGift) markCorrupt(in); p.acquiredEquipment.push_back(c); }
    }
    if(!readCount(in,count) || count!=s.fleets.size()+s.pendingFleetLossContacts.size()+s.wormholeTransits.size()) { markCorrupt(stream); return; }
    const auto read=[&](Fleet& f) {
        quint32 id{},packets{}; in >> id; if(id!=f.id) markCorrupt(in);
        readVisitorMission(in,f.visitor,s.turn); readVisitorMission(in,f.telemetry.visitor,f.telemetry.observedTurn);
        if(!readCount(in,packets) || packets!=f.telemetryInTransit.size()) { markCorrupt(in); return; }
        for(auto& packet:f.telemetryInTransit) readVisitorMission(in,packet.telemetry.visitor,packet.telemetry.observedTurn);
        if(!readCount(in,packets) || packets!=f.pendingCommands.size()) { markCorrupt(in); return; }
        for(auto& c:f.pendingCommands) { quint8 present{}; in >> present;
            if(present>1) markCorrupt(in); if(present) { VisitorMission m; readVisitorMission(in,m,c.issuedTurn);
                if(playerTurn || c.probe || c.task || c.electronics || c.mobileBase) markCorrupt(in); c.visitor=m; }
        }
    };
    for(auto& f:s.fleets) read(f);
    for(auto& f:s.pendingFleetLossContacts) read(f.lastContact);
    for(auto& f:s.wormholeTransits) read(f.lastContact);
    if(in.status()!=QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
