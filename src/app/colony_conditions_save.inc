// Save v100: exact owner conditions travel with the existing immutable colony packet.
void writeColonyConditions(QDataStream& stream,const GameState& state)
{
    QByteArray bytes;QDataStream out(&bytes,QIODevice::WriteOnly);out.setVersion(stream.version());
    out<<quint32(state.players.size());
    const auto write=[&](const auto& records) {
        out<<quint32(records.size());
        for(const auto& r:records) {
            const auto* c=r.planetSnapshot && r.planetSnapshot->habitat && r.planetSnapshot->habitat->conditions
                ? &*r.planetSnapshot->habitat->conditions:nullptr;
            out<<quint32(r.star)<<quint64(r.observedTurn)<<quint8(c!=nullptr);
            if(!c)continue;
            const auto& d=c->damage;
            out<<quint16(c->defenses)<<quint64(d.factoriesDestroyed)<<quint64(d.minesDestroyed)
                <<quint64(d.factoriesReconstructed)<<quint64(d.minesReconstructed)
                <<quint64(d.lastBombardmentTurn)<<quint64(d.lastReconstructionTurn)
                <<quint8(d.contamination)<<quint64(d.lastCleanupTurn);
        }
    };
    for(const auto& p:state.players) {out<<quint32(p.id);write(p.surveyKnowledge);write(p.pendingSurveyReports);}
    if(out.status()!=QDataStream::Ok || bytes.size()>32*1024*1024) {stream.setStatus(QDataStream::WriteFailed);return;}
    stream<<quint32(0x43434f4e)<<quint32(bytes.size());stream.writeRawData(bytes.data(),bytes.size());
}
void readColonyConditions(QDataStream& stream,GameState& state)
{
    quint32 magic{},size{},count{};stream>>magic>>size;
    if(magic!=0x43434f4e || size>32*1024*1024) {markCorrupt(stream);return;}
    QByteArray bytes(size,'\0');if(stream.readRawData(bytes.data(),size)!=int(size)) {markCorrupt(stream);return;}
    QDataStream in(bytes);in.setVersion(stream.version());
    if(!readCount(in,count) || count!=state.players.size()) {markCorrupt(stream);return;}
    const auto read=[&](auto& records) {
        quint32 count{};if(!readCount(in,count) || count!=records.size()) {markCorrupt(in);return;}
        for(auto& r:records) {
            quint32 star{};quint64 time{};quint8 has{};in>>star>>time>>has;
            if(star!=r.star || time!=r.observedTurn || has>1) {markCorrupt(in);return;}
            if(!has)continue;
            if(!r.planetSnapshot || !r.planetSnapshot->habitat) {markCorrupt(in);return;}
            ColonyConditions c;quint64 fd{},md{},fr{},mr{},bomb{},rebuild{},cleanup{};
            in>>c.defenses>>fd>>md>>fr>>mr>>bomb>>rebuild>>c.damage.contamination>>cleanup;
            if(c.defenses>kMaximumColonyDefenses || fr>fd || mr>md || bomb>time || rebuild>time || cleanup>time
                || c.damage.contamination>100 || ((fd || md || c.damage.contamination) && !bomb)
                || ((fr || mr) && !rebuild)) {markCorrupt(in);return;}
            c.damage={fd,md,fr,mr,bomb,rebuild,c.damage.contamination,cleanup};
            r.planetSnapshot->habitat->conditions=c;
        }
    };
    for(auto& p:state.players) {quint32 id{};in>>id;if(id!=p.id){markCorrupt(in);break;}read(p.surveyKnowledge);read(p.pendingSurveyReports);}
    if(in.status()!=QDataStream::Ok || !in.atEnd())markCorrupt(stream);
}
