// Save v102 / orders v42: owner-authored logistics catalog and ordinary route dispatch.
void writeTransportSpec(QDataStream& out,const TransportLineSpec& d) {
    out<<quint32(d.id)<<quint64(d.revision);writeString(out,d.name);
    out<<quint32(d.source)<<quint32(d.destination)<<quint8(d.warp)<<quint8(d.cargo)<<quint8(d.active)
        <<quint32(d.sourceReserve)<<quint32(d.departureMinimum);
}
void readTransportSpec(QDataStream& in,TransportLineSpec& d) {
    quint64 revision{};quint8 cargo{},active{};
    in>>d.id>>revision;d.revision=revision;readString(in,d.name);
    in>>d.source>>d.destination>>d.warp>>cargo>>active;d.cargo=TransportCargo(cargo);d.active=active;
    d.sourceReserve=d.departureMinimum=0;
    if(gReadSaveFormatVersion>=103)in>>d.sourceReserve>>d.departureMinimum;
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
