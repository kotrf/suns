// Independently bounded v79 tail, keeping old strategic layouts readable.
constexpr quint32 kUnknownObjectsMagic=0x554e4b4fu; // UNKO
void writeUnknownObjects(QDataStream& stream,const GameState& state)
{
    QByteArray payload; QDataStream s(&payload,QIODevice::WriteOnly); s.setVersion(stream.version());
    s << quint32(state.nextUnknownObjectId);
    writeVector(s,state.unknownObjects,[](QDataStream& out,const UnknownObject& x) {
        out << quint32(x.id); writePosition(out,x.position); out << quint8(x.kind);
    });
    s << quint32(state.players.size());
    for(const auto& p:state.players) {
        s << quint32(p.id); writeVector(s,p.objectKnowledge,writeObjectObservation);
        s << quint32(p.history.size());
        for(const auto& h:p.history) {
            s << quint64(h.turn) << quint32(h.milestones.size());
            for(const auto& m:h.milestones) s << quint64(m.eventId) << quint32(m.object);
        }
    }
    stream << kUnknownObjectsMagic << quint32(payload.size()); stream.writeRawData(payload.data(),payload.size());
}
void readUnknownObjects(QDataStream& stream,GameState& state)
{
    quint32 magic{},size{},count{}; stream >> magic >> size;
    if(magic!=kUnknownObjectsMagic || size>64*1024*1024) { markCorrupt(stream); return; }
    QByteArray payload(size,'\0'); if(stream.readRawData(payload.data(),size)!=int(size)) { markCorrupt(stream); return; }
    QDataStream s(payload); s.setVersion(stream.version()); s >> state.nextUnknownObjectId;
    if(!state.nextUnknownObjectId || state.nextUnknownObjectId>0x7fffffffU
        || !readVector(s,state.unknownObjects,[](QDataStream& in,UnknownObject& x) {
            quint8 kind{}; in >> x.id; readPosition(in,x.position); in >> kind; x.kind=UnknownObjectKind(kind);
            if(!x.id || x.id>0x7fffffffU || kind>quint8(UnknownObjectKind::CompactRemnant)) markCorrupt(in);
        }) || state.unknownObjects.size()>1000) { markCorrupt(stream); return; }
    for(std::size_t n=0;n<state.unknownObjects.size();++n) {
        const auto& x=state.unknownObjects[n];
        if(x.id>=state.nextUnknownObjectId || std::any_of(state.unknownObjects.begin(),state.unknownObjects.begin()+n,
            [&](const auto& old){return old.id==x.id;})) { markCorrupt(stream); return; }
    }
    if(!readCount(s,count) || count!=state.players.size()) { markCorrupt(stream); return; }
    for(auto& p:state.players) {
        quint32 id{}; s >> id;
        if(id!=p.id || !readVector(s,p.objectKnowledge,readObjectObservation)) { markCorrupt(stream); return; }
        for(std::size_t n=0;n<p.objectKnowledge.size();++n) {
            const auto& x=p.objectKnowledge[n];
            if(!x.receivedTurn || x.receivedTurn>state.turn || x.observedTurn>state.turn
                || std::any_of(p.objectKnowledge.begin(),p.objectKnowledge.begin()+n,[&](const auto& old){return old.id==x.id;})) { markCorrupt(stream); return; }
        }
        if(!readCount(s,count) || count!=p.history.size()) { markCorrupt(stream); return; }
        for(auto& h:p.history) {
            quint64 turn{}; s >> turn;
            if(turn!=h.turn || !readCount(s,count) || count!=h.milestones.size()) { markCorrupt(stream); return; }
            for(auto& m:h.milestones) {
                quint64 event{}; s >> event >> m.object;
                const bool expected=m.kind==HistoryMilestoneKind::UnknownObjectReported || m.kind==HistoryMilestoneKind::UnknownObjectStudied;
                if(event!=m.eventId || expected!=bool(m.object) || m.object>0x7fffffffU) { markCorrupt(stream); return; }
            }
        }
    }
    if(s.status()!=QDataStream::Ok || !s.atEnd()) markCorrupt(stream);
}
