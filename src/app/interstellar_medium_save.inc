// v80: independently bounded medium profiles and received immutable evidence.
constexpr quint32 kInterstellarMediumMagic=0x4d454449u; // MEDI
void writeInterstellarMedium(QDataStream& stream,const GameState& state)
{
    QByteArray payload; QDataStream out(&payload,QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint8(state.interstellarMediumEnabled) << quint32(state.mediumObserver);
    writeVector(out,state.interstellarClouds,[](QDataStream& s,const InterstellarCloud& c) {
        s << quint32(c.id); writePosition(s,c.center); s << c.radiusX << c.radiusY << c.density;
    });
    out << quint32(state.players.size());
    for (const auto& p:state.players) { out << quint32(p.id); writeVector(out,p.mediumKnowledge,writeMediumObservation); }
    stream << kInterstellarMediumMagic << quint32(payload.size());
    stream.writeRawData(payload.data(),payload.size());
}
void readInterstellarMedium(QDataStream& stream,GameState& state)
{
    quint32 magic{},size{},count{}; stream >> magic >> size;
    if (magic!=kInterstellarMediumMagic || size>64*1024*1024) { markCorrupt(stream); return; }
    QByteArray payload(size,'\0');
    if (stream.readRawData(payload.data(),size)!=int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    quint8 enabled{}; in >> enabled >> state.mediumObserver; state.interstellarMediumEnabled=enabled;
    if (enabled>1 || (state.mediumObserver && !find_player(state,state.mediumObserver))
        || !readVector(in,state.interstellarClouds,[](QDataStream& s,InterstellarCloud& c) {
            s >> c.id; readPosition(s,c.center); s >> c.radiusX >> c.radiusY >> c.density;
            if (!interstellar_cloud_valid(c)) markCorrupt(s);
        }) || state.interstellarClouds.size()>4
        || ((!enabled || state.mediumObserver) && !state.interstellarClouds.empty())) { markCorrupt(stream); return; }
    for (std::size_t n=0;n<state.interstellarClouds.size();++n)
        for (std::size_t j=0;j<n;++j) if (state.interstellarClouds[n].id==state.interstellarClouds[j].id) { markCorrupt(stream); return; }
    if (!readCount(in,count) || count!=state.players.size()) { markCorrupt(stream); return; }
    for (auto& p:state.players) {
        quint32 id{}; in >> id;
        if (id!=p.id || !readVector(in,p.mediumKnowledge,readMediumObservation) || p.mediumKnowledge.size()>4
            || (!enabled && !p.mediumKnowledge.empty())) { markCorrupt(stream); return; }
        for (std::size_t n=0;n<p.mediumKnowledge.size();++n) {
            const auto& x=p.mediumKnowledge[n];
            if (!x.receivedTurn || x.receivedTurn>state.turn || x.observedTurn>state.turn) { markCorrupt(stream); return; }
            for (std::size_t j=0;j<n;++j) if (p.mediumKnowledge[j].id==x.id) { markCorrupt(stream); return; }
        }
        if (!enabled && std::any_of(p.pendingPlayerReports.begin(),p.pendingPlayerReports.end(),
            [](const auto& r) { return bool(r.mediumObservation); })) { markCorrupt(stream); return; }
    }
    if (in.status()!=QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
