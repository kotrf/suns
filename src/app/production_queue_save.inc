// v128: per-colony production identities. Historical item/order layouts stay intact.
constexpr quint32 kProductionQueueIdsMagic=0x50514944u; // PQID
void writeQueueIds(QDataStream& out,const std::vector<ProductionItem>& queue)
{
    out<<quint32(queue.size());std::set<std::uint64_t> used;
    for(const auto& item:queue) {
        if(item.queueId==std::numeric_limits<std::uint64_t>::max() || (item.queueId && !used.insert(item.queueId).second)) {
            out.setStatus(QDataStream::WriteFailed);return;
        }
        out<<quint64(item.queueId);
    }
}
void readQueueIds(QDataStream& in,std::vector<ProductionItem>& queue)
{
    quint32 count{};if(!readCount(in,count) || count!=queue.size()){markCorrupt(in);return;}
    std::set<std::uint64_t> used;
    for(auto& item:queue) {
        quint64 id{};in>>id;item.queueId=id;
        if(id==std::numeric_limits<std::uint64_t>::max() || (id && !used.insert(id).second)){markCorrupt(in);return;}
    }
}
void writeQueueIdsEvidence(QDataStream& out,const auto& record)
{
    const auto* h=record.planetSnapshot && record.planetSnapshot->habitat?&*record.planetSnapshot->habitat:nullptr;
    out<<quint8(bool(h));if(h)writeQueueIds(out,h->queue);
}
void readQueueIdsEvidence(QDataStream& in,auto& record)
{
    auto* h=record.planetSnapshot && record.planetSnapshot->habitat?&*record.planetSnapshot->habitat:nullptr;
    quint8 present{};in>>present;
    if(present>1 || bool(present)!=bool(h)){markCorrupt(in);return;}
    if(h)readQueueIds(in,h->queue);
}
void writeProductionQueueIds(QDataStream& stream,const GameState& state)
{
    QByteArray payload;QDataStream out(&payload,QIODevice::WriteOnly);out.setVersion(stream.version());
    out<<quint32(state.planets.size());
    for(const auto& p:state.planets) {
        if(!p.nextProductionQueueId || std::any_of(p.productionQueue.begin(),p.productionQueue.end(),[&](const auto& item){return item.queueId>=p.nextProductionQueueId;})) {
            stream.setStatus(QDataStream::WriteFailed);return;
        }
        out<<quint32(p.id)<<quint64(p.nextProductionQueueId);writeQueueIds(out,p.productionQueue);
    }
    out<<quint32(state.players.size());
    for(const auto& p:state.players) {
        out<<quint32(p.id)<<quint32(p.surveyKnowledge.size());
        for(const auto& k:p.surveyKnowledge){out<<quint32(k.star);writeQueueIdsEvidence(out,k);}
        out<<quint32(p.pendingSurveyReports.size());
        for(const auto& r:p.pendingSurveyReports){out<<quint32(r.star)<<quint64(r.observedTurn);writeQueueIdsEvidence(out,r);}
    }
    if(out.status()!=QDataStream::Ok || payload.size()>32*1024*1024){stream.setStatus(QDataStream::WriteFailed);return;}
    stream<<kProductionQueueIdsMagic<<quint32(payload.size());stream.writeRawData(payload.data(),int(payload.size()));
}
void readProductionQueueIds(QDataStream& stream,GameState& state)
{
    quint32 magic{},size{},count{},id{};stream>>magic>>size;
    if(magic!=kProductionQueueIdsMagic || size>32U*1024U*1024U){markCorrupt(stream);return;}
    QByteArray payload(size,'\0');if(stream.readRawData(payload.data(),int(size))!=int(size)){markCorrupt(stream);return;}
    QDataStream in(payload);in.setVersion(stream.version());
    if(!readCount(in,count) || count!=state.planets.size()){markCorrupt(stream);return;}
    for(auto& p:state.planets) {
        quint64 next{};in>>id>>next;if(id!=p.id || !next){markCorrupt(stream);return;}
        p.nextProductionQueueId=next;readQueueIds(in,p.productionQueue);
        if(std::any_of(p.productionQueue.begin(),p.productionQueue.end(),[&](const auto& item){return item.queueId>=next;})){markCorrupt(stream);return;}
    }
    if(!readCount(in,count) || count!=state.players.size()){markCorrupt(stream);return;}
    for(auto& p:state.players) {
        in>>id;if(id!=p.id || !readCount(in,count) || count!=p.surveyKnowledge.size()){markCorrupt(stream);return;}
        for(auto& k:p.surveyKnowledge){in>>id;if(id!=k.star){markCorrupt(stream);return;}readQueueIdsEvidence(in,k);}
        if(!readCount(in,count) || count!=p.pendingSurveyReports.size()){markCorrupt(stream);return;}
        for(auto& r:p.pendingSurveyReports) {
            quint64 observed{};in>>id>>observed;if(id!=r.star || observed!=r.observedTurn){markCorrupt(stream);return;}
            readQueueIdsEvidence(in,r);
        }
    }
    if(in.status()!=QDataStream::Ok || !in.atEnd())markCorrupt(stream);
}
