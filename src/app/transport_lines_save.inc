// Save v102 / orders v42: owner-authored logistics catalog and ordinary route dispatch.
void writeTransportSpec(QDataStream& out,const TransportLineSpec& d) {
    if(d.destination && d.destinationFleet){out.setStatus(QDataStream::WriteFailed);return;}
    out<<quint32(d.id)<<quint64(d.revision);writeString(out,d.name);
    out<<quint32(d.source)<<quint32(d.destination)<<quint8(d.warp)<<quint8(d.cargo)<<quint8(d.active)
        <<quint32(d.sourceReserve)<<quint32(d.departureMinimum)<<quint32(d.destinationTarget);
    if(!d.destination)out<<quint32(d.destinationFleet);
}
void readTransportSpec(QDataStream& in,TransportLineSpec& d) {
    quint64 revision{};quint8 cargo{},active{};
    in>>d.id>>revision;d.revision=revision;readString(in,d.name);
    in>>d.source>>d.destination>>d.warp>>cargo>>active;d.cargo=TransportCargo(cargo);d.active=active;
    d.sourceReserve=d.departureMinimum=0;
    if(gReadSaveFormatVersion>=103)in>>d.sourceReserve>>d.departureMinimum;
    d.destinationTarget=0;if(gReadSaveFormatVersion>=105)in>>d.destinationTarget;
    d.destinationFleet=0;if(gReadSaveFormatVersion>=106 && !d.destination)in>>d.destinationFleet;
    if(cargo>quint8(TransportCargo::Germanium) || active>1 || d.name.size()>64)markCorrupt(in);
}
void writeTransportLines(QDataStream& stream,const GameState& state) {
    QByteArray bytes;QDataStream out(&bytes,QIODevice::WriteOnly);out.setVersion(stream.version());
    out<<quint32(state.players.size());
    for(const auto& p:state.players) {
        out<<quint32(p.id)<<quint32(p.lastTransportLineId)<<quint32(p.transportLines.size());
        for(const auto& line:p.transportLines) {
            writeTransportSpec(out,line.spec);writePosition(out,line.sourcePosition);writePosition(out,line.destinationPosition);
            out<<quint32(line.assignments.size());
            for(const auto& a:line.assignments)out<<quint32(a.fleet)<<quint64(a.issuedTurn);
        }
    }
    if(out.status()!=QDataStream::Ok || bytes.size()>16*1024*1024){stream.setStatus(QDataStream::WriteFailed);return;}
    stream<<quint32(0x4c494e45)<<quint32(bytes.size());stream.writeRawData(bytes.data(),bytes.size());
}
void readTransportLines(QDataStream& stream,GameState& state) {
    quint32 magic{},size{},count{};stream>>magic>>size;
    if(magic!=0x4c494e45 || size>16*1024*1024){markCorrupt(stream);return;}
    QByteArray bytes(size,'\0');if(stream.readRawData(bytes.data(),size)!=int(size)){markCorrupt(stream);return;}
    QDataStream in(bytes);in.setVersion(stream.version());
    if(!readCount(in,count) || count!=state.players.size()){markCorrupt(stream);return;}
    for(auto& p:state.players) {
        quint32 id{},lines{};in>>id>>p.lastTransportLineId;
        if(id!=p.id || !readCount(in,lines) || lines>kMaximumTransportLines){markCorrupt(stream);return;}
        std::set<TransportLineId> ids;std::set<FleetId> assigned;
        for(quint32 i=0;i<lines;++i) {
            TransportLine line;readTransportSpec(in,line.spec);readPosition(in,line.sourcePosition);readPosition(in,line.destinationPosition);
            if(!transport_line_spec_valid(line.spec) || line.spec.id>p.lastTransportLineId || !ids.insert(line.spec.id).second
                || same_position(line.sourcePosition,line.destinationPosition)){markCorrupt(stream);return;}
            quint32 members{};if(!readCount(in,members) || members>kMaximumLineAssignments){markCorrupt(stream);return;}
            for(quint32 j=0;j<members;++j) {
                TransportAssignment a;quint64 issued{};in>>a.fleet>>issued;a.issuedTurn=issued;
                if(!a.fleet || issued>state.turn || !assigned.insert(a.fleet).second){markCorrupt(stream);return;}
                line.assignments.push_back(a);
            }
            p.transportLines.push_back(std::move(line));
        }
    }
    if(in.status()!=QDataStream::Ok || !in.atEnd())markCorrupt(stream);
}

// Save v104: bounded owner-private received delivery observations. Physical
// attribution travels with the ordinary versioned arrival action/report.
void writeTransportStatistics(QDataStream& stream,const GameState& state) {
    QByteArray bytes;QDataStream out(&bytes,QIODevice::WriteOnly);out.setVersion(stream.version());
    out<<quint32(state.players.size());
    for(const auto& p:state.players) {
        if(p.transportReceipts.size()>kMaximumTransportReceipts){stream.setStatus(QDataStream::WriteFailed);return;}
        out<<quint32(p.id)<<quint64(p.transportReceiptsTruncatedThrough)<<quint32(p.transportReceipts.size());
        for(const auto& r:p.transportReceipts) {
            if(r.planet && r.destinationFleet){stream.setStatus(QDataStream::WriteFailed);return;}
            out<<quint32(r.line)<<quint64(r.revision)<<quint32(r.fleet)<<quint32(r.planet)
                <<quint64(r.observedTurn)<<quint64(r.receivedTurn);writeMinerals(out,r.minerals);
            if(!r.planet)out<<quint32(r.destinationFleet);
        }
    }
    if(out.status()!=QDataStream::Ok || bytes.size()>16*1024*1024){stream.setStatus(QDataStream::WriteFailed);return;}
    stream<<quint32(0x544c5354)<<quint32(bytes.size());stream.writeRawData(bytes.data(),bytes.size());
}
void readTransportStatistics(QDataStream& stream,GameState& state) {
    quint32 magic{},size{},players{};stream>>magic>>size;
    if(magic!=0x544c5354 || size>16*1024*1024){markCorrupt(stream);return;}
    QByteArray bytes(size,'\0');if(stream.readRawData(bytes.data(),size)!=int(size)){markCorrupt(stream);return;}
    QDataStream in(bytes);in.setVersion(stream.version());
    if(!readCount(in,players) || players!=state.players.size()){markCorrupt(stream);return;}
    const auto key=[](const auto& r){return std::tuple{r.observedTurn,r.line,r.revision,r.fleet,r.planet,r.destinationFleet};};
    for(auto& p:state.players) {
        quint32 id{},count{};quint64 truncated{};in>>id>>truncated;p.transportReceiptsTruncatedThrough=truncated;
        if(id!=p.id || p.transportReceiptsTruncatedThrough>state.turn || !readCount(in,count) || count>kMaximumTransportReceipts){markCorrupt(stream);return;}
        for(const auto& r:p.pendingPlayerReports)if(r.kind==PlayerReportKind::TransportLineDelivery
            && (r.transportLine>p.lastTransportLineId || r.observedTurn>state.turn || r.deliveryTurn<r.observedTurn)) {
            markCorrupt(stream);return;
        }
        for(quint32 i=0;i<count;++i) {
            TransportLineReceipt r;quint64 revision{},observed{},received{};in>>r.line>>revision>>r.fleet>>r.planet>>observed>>received;
            r.revision=revision;r.observedTurn=observed;r.receivedTurn=received;readMinerals(in,r.minerals);
            if(gReadSaveFormatVersion>=106 && !r.planet)in>>r.destinationFleet;
            if(!transport_receipt_valid(r,state.turn) || r.line>p.lastTransportLineId
                || r.observedTurn<=p.transportReceiptsTruncatedThrough
                || (!p.transportReceipts.empty() && key(p.transportReceipts.back())>=key(r))){markCorrupt(stream);return;}
            p.transportReceipts.push_back(r);
        }
    }
    if(in.status()!=QDataStream::Ok || !in.atEnd())markCorrupt(stream);
}
