// v92: owner-only scheduling evidence; the ENGI body remains compatible.
constexpr quint32 kEngineeringQueueMagic=0x45514958u; // EQIX
void writeEngineeringQueueObservation(QDataStream& out,const auto& record,PlayerId viewer)
{
    const auto index=record.planetSnapshot?record.planetSnapshot->engineeringQueueIndex:std::nullopt;
    if(index && (*index>100000 || record.observedOwner!=viewer
        || record.planetSnapshot->engineering.kind==PlanetaryEngineeringKind::None
        || record.planetSnapshot->engineering.work>=engineering_work(record.planetSnapshot->engineering.kind))) {
        out.setStatus(QDataStream::WriteFailed); return;
    }
    out<<quint8(index.has_value()); if(index) out<<quint32(*index);
}
void readEngineeringQueueObservation(QDataStream& in,auto& record,PlayerId viewer)
{
    quint8 present{}; in>>present;
    if(present>1) {markCorrupt(in);return;}
    if(!present) return;
    quint32 index{}; in>>index;
    if(!record.planetSnapshot || record.observedOwner!=viewer || index>100000
        || record.planetSnapshot->engineering.kind==PlanetaryEngineeringKind::None
        || record.planetSnapshot->engineering.work>=engineering_work(record.planetSnapshot->engineering.kind)) {markCorrupt(in);return;}
    record.planetSnapshot->engineeringQueueIndex=index;
}
void writeEngineeringQueues(QDataStream& stream,const GameState& state)
{
    QByteArray payload; QDataStream out(&payload,QIODevice::WriteOnly); out.setVersion(stream.version());
    out<<quint32(state.players.size());
    for(const auto& player:state.players) {
        out<<quint32(player.id)<<quint32(player.surveyKnowledge.size());
        for(const auto& k:player.surveyKnowledge) {out<<quint32(k.star);writeEngineeringQueueObservation(out,k,player.id);}
        out<<quint32(player.pendingSurveyReports.size());
        for(const auto& r:player.pendingSurveyReports) {out<<quint32(r.star)<<quint64(r.observedTurn);writeEngineeringQueueObservation(out,r,player.id);}
    }
    if(out.status()!=QDataStream::Ok || payload.size()>16*1024*1024) {stream.setStatus(QDataStream::WriteFailed);return;}
    stream<<kEngineeringQueueMagic<<quint32(payload.size());stream.writeRawData(payload.data(),int(payload.size()));
}
void readEngineeringQueues(QDataStream& stream,GameState& state)
{
    quint32 magic{},size{},count{},id{};stream>>magic>>size;
    if(magic!=kEngineeringQueueMagic || size>16U*1024U*1024U) {markCorrupt(stream);return;}
    QByteArray payload(size,'\0');if(stream.readRawData(payload.data(),int(size))!=int(size)) {markCorrupt(stream);return;}
    QDataStream in(payload);in.setVersion(stream.version());
    if(!readCount(in,count) || count!=state.players.size()) {markCorrupt(stream);return;}
    for(auto& player:state.players) {
        in>>id;
        if(id!=player.id || !readCount(in,count) || count!=player.surveyKnowledge.size()) {markCorrupt(stream);return;}
        for(auto& k:player.surveyKnowledge) {in>>id;if(id!=k.star)markCorrupt(in);readEngineeringQueueObservation(in,k,player.id);}
        if(!readCount(in,count) || count!=player.pendingSurveyReports.size()) {markCorrupt(stream);return;}
        for(auto& r:player.pendingSurveyReports) {
            quint64 observed{};in>>id>>observed;
            if(id!=r.star || observed!=r.observedTurn)markCorrupt(in);
            readEngineeringQueueObservation(in,r,player.id);
        }
    }
    if(in.status()!=QDataStream::Ok || !in.atEnd())markCorrupt(stream);
}
