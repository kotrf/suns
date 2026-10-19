// Private save helpers for immutable reconnaissance evidence.
void writeReconContact(QDataStream& s,const ReconContact& c)
{
    s << quint32(c.fleet) << quint32(c.owner); writePosition(s,c.position);
    s << c.uncertainty; writeEnum(s,c.quality); writeEnum(s,c.sensor);
    s << quint64(c.observedTurn) << quint64(c.lastSeenTurn) << quint64(c.deliveryTurn)
      << quint32(c.sourceFleet) << quint32(c.sourceColony) << quint32(c.star) << c.massMinimum << c.massMaximum
      << quint32(c.shipCount) << quint32(c.hulls.size());
    for(auto h:c.hulls) writeEnum(s,h);
    s << quint8(c.lost) << quint8(c.jammed);
}
void readReconContact(QDataStream& s,ReconContact& c)
{
    quint64 observed{},seen{},due{}; quint8 lost{},jammed{}; quint32 count{};
    s >> c.fleet >> c.owner; readPosition(s,c.position); s >> c.uncertainty;
    if(!readEnum(s,c.quality,quint8(ContactQuality::Identified)) || !readEnum(s,c.sensor,quint8(ContactSensor::Passive))) return;
    s >> observed >> seen >> due >> c.sourceFleet >> c.sourceColony >> c.star >> c.massMinimum >> c.massMaximum >> c.shipCount;
    c.observedTurn=observed; c.lastSeenTurn=seen; c.deliveryTurn=due;
    if(!readCount(s,count) || count>quint32(gReadSaveFormatVersion>=88 ? ShipHullType::SurveyProbe : gReadSaveFormatVersion>=84 ? ShipHullType::ExpeditionBase : ShipHullType::MetaMorph)+1) { markCorrupt(s); return; }
    for(quint32 i=0;i<count;++i) {
        ShipHullType h;
        if(!readEnum(s,h,quint8(gReadSaveFormatVersion>=88 ? ShipHullType::SurveyProbe : gReadSaveFormatVersion>=84 ? ShipHullType::ExpeditionBase : ShipHullType::MetaMorph))) return;
        c.hulls.push_back(h);
    }
    s >> lost >> jammed; c.lost=lost; c.jammed=jammed;
    if(lost>1 || jammed>1 || !recon_contact_valid(c)) markCorrupt(s);
}
void writeSensorStatus(QDataStream& s,const SensorStatus& x)
{
    s << quint32(x.fleet) << quint32(x.colony); writePosition(s,x.position);
    s << quint64(x.observedTurn) << quint64(x.deliveryTurn) << quint8(x.interferencePercent)
      << x.nominalRange << x.activeRange << x.passiveRange << x.emissionRange;
}
void readSensorStatus(QDataStream& s,SensorStatus& x)
{
    quint64 observed{},due{}; s >> x.fleet >> x.colony; readPosition(s,x.position);
    s >> observed >> due >> x.interferencePercent >> x.nominalRange >> x.activeRange >> x.passiveRange >> x.emissionRange;
    x.observedTurn=observed; x.deliveryTurn=due;
    if(!sensor_status_valid(x)) markCorrupt(s);
}
void writeEmissionObservation(QDataStream& s,const std::optional<EmissionObservation>& value)
{
    s << quint8(value.has_value());
    if(value) s << quint64(value->emittedTurn) << quint8(value->channel);
}
void readEmissionObservation(QDataStream& s,std::optional<EmissionObservation>& value,std::uint64_t observed)
{
    quint8 present{}; s >> present;
    if(present>1) { markCorrupt(s); return; }
    if(present) {
        quint64 emitted{}; quint8 channel{}; s >> emitted >> channel;
        if(!emitted || emitted>observed || channel>quint8(SignalChannel::Slow)) { markCorrupt(s); return; }
        value=EmissionObservation{emitted,SignalChannel(channel)};
    }
}
void writeAftermathObservation(QDataStream& s,const PlanetAftermathObservation& x)
{
    s << quint32(x.planet) << quint64(x.observedTurn) << quint64(x.receivedTurn)
        << quint64(x.factoriesMinimum) << quint64(x.factoriesMaximum)
        << quint64(x.minesMinimum) << quint64(x.minesMaximum)
        << quint8(x.contaminationMinimum) << quint8(x.contaminationMaximum) << quint8(x.exact);
}
void readAftermathObservation(QDataStream& s,PlanetAftermathObservation& x)
{
    quint64 observed{},received{},fmin{},fmax{},mmin{},mmax{}; quint8 exact{};
    s >> x.planet >> observed >> received >> fmin >> fmax >> mmin >> mmax
        >> x.contaminationMinimum >> x.contaminationMaximum >> exact;
    x.observedTurn=observed; x.receivedTurn=received; x.factoriesMinimum=fmin; x.factoriesMaximum=fmax;
    x.minesMinimum=mmin; x.minesMaximum=mmax; x.exact=exact;
    if(exact>1 || !aftermath_observation_valid(x)) markCorrupt(s);
}
void writeObjectObservation(QDataStream& s,const ObjectObservation& x)
{
    s << quint32(x.id); writePosition(s,x.position);
    s << x.uncertainty << quint8(x.level) << quint8(x.kind.has_value());
    if(x.kind) s << quint8(*x.kind);
    s << quint64(x.observedTurn) << quint64(x.receivedTurn);
}
void readObjectObservation(QDataStream& s,ObjectObservation& x)
{
    quint8 level{},present{},kind{}; quint64 observed{},received{};
    s >> x.id; readPosition(s,x.position); s >> x.uncertainty >> level >> present;
    if(present>1) { markCorrupt(s); return; }
    if(present) { s >> kind; x.kind=UnknownObjectKind(kind); }
    s >> observed >> received; x.level=ObjectKnowledgeLevel(level); x.observedTurn=observed; x.receivedTurn=received;
    if(!object_observation_valid(x)) markCorrupt(s);
}
void writeMediumObservation(QDataStream& s,const MediumObservation& x)
{
    s << quint32(x.id); writePosition(s,x.center);
    s << x.radiusX << x.radiusY << x.densityMinimum << x.densityMaximum << quint8(x.level)
        << quint64(x.observedTurn) << quint64(x.receivedTurn);
}
void readMediumObservation(QDataStream& s,MediumObservation& x)
{
    quint8 level{}; quint64 observed{},received{}; s >> x.id; readPosition(s,x.center);
    s >> x.radiusX >> x.radiusY >> x.densityMinimum >> x.densityMaximum >> level >> observed >> received;
    x.level=MediumKnowledgeLevel(level); x.observedTurn=observed; x.receivedTurn=received;
    if (!medium_observation_valid(x)) markCorrupt(s);
}
template<class T> void writeReconEvidence(QDataStream& s,const T& x)
{
    s << quint8(x.contact.has_value()); if(x.contact) writeReconContact(s,*x.contact);
    s << quint8(x.sensorStatus.has_value()); if(x.sensorStatus) writeSensorStatus(s,*x.sensorStatus);
    writeEmissionObservation(s,x.emission);
    s << quint8(x.aftermath.has_value()); if(x.aftermath) writeAftermathObservation(s,*x.aftermath);
    s << quint8(x.objectObservation.has_value()); if(x.objectObservation) writeObjectObservation(s,*x.objectObservation);
    s << quint8(x.mediumObservation.has_value()); if(x.mediumObservation) writeMediumObservation(s,*x.mediumObservation);
}
bool reconEvidenceMatches(const PendingPlayerReport& x)
{
    if(x.contact && (x.kind!=PlayerReportKind::EnemyFleetDetected && x.kind!=PlayerReportKind::EnemyFleetLost && x.kind!=PlayerReportKind::EnemyFleetReclassified)) return false;
    if(x.contact && (x.contact->fleet!=x.fleet || x.contact->owner!=x.contactOwner || x.contact->observedTurn!=x.observedTurn
        || x.contact->deliveryTurn!=x.deliveryTurn || !same_position(x.contact->position,x.contactPosition)
        || x.contact->lost!=(x.kind==PlayerReportKind::EnemyFleetLost))) return false;
    if(x.sensorStatus && (x.kind!=PlayerReportKind::SensorInterferenceChanged || x.sensorStatus->fleet!=x.fleet
        || x.sensorStatus->colony!=x.planet || x.sensorStatus->observedTurn!=x.observedTurn
        || x.sensorStatus->deliveryTurn!=x.deliveryTurn || x.sensorStatus->interferencePercent!=x.quantity
        || !same_position(x.sensorStatus->position,x.position))) return false;
    return (x.kind!=PlayerReportKind::EnemyFleetReclassified || x.contact) && (x.kind!=PlayerReportKind::SensorInterferenceChanged || x.sensorStatus);
}
bool reconEvidenceMatches(const GameEvent& x)
{
    if(x.contact && (x.kind!=GameEventKind::EnemyFleetDetected && x.kind!=GameEventKind::EnemyFleetLost && x.kind!=GameEventKind::EnemyFleetReclassified)) return false;
    if(x.contact && (x.contact->fleet!=x.fleet || x.contact->owner!=x.contactOwner || x.contact->observedTurn!=x.observedTurn
        || x.contact->deliveryTurn>x.turn || !same_position(x.contact->position,x.position)
        || x.contact->lost!=(x.kind==GameEventKind::EnemyFleetLost))) return false;
    if(x.sensorStatus && (x.kind!=GameEventKind::SensorInterferenceChanged || x.sensorStatus->fleet!=x.fleet
        || x.sensorStatus->colony!=x.planet || x.sensorStatus->observedTurn!=x.observedTurn
        || x.sensorStatus->deliveryTurn>x.turn || x.sensorStatus->interferencePercent!=x.quantity
        || !same_position(x.sensorStatus->position,x.position))) return false;
    return (x.kind!=GameEventKind::EnemyFleetReclassified || x.contact) && (x.kind!=GameEventKind::SensorInterferenceChanged || x.sensorStatus);
}
template<class T> void readReconEvidence(QDataStream& s,T& x)
{
    quint8 present{}; s >> present;
    if(present>1) { markCorrupt(s); return; }
    if(present) { ReconContact c; readReconContact(s,c); x.contact=c; }
    s >> present; if(present>1) { markCorrupt(s); return; }
    if(present) { SensorStatus status; readSensorStatus(s,status); x.sensorStatus=status; }
    if(gReadSaveFormatVersion>=76) {
        readEmissionObservation(s,x.emission,x.observedTurn);
        if(x.emission && x.kind!=decltype(x.kind)::EmissionDetected) markCorrupt(s);
    }
    if(gReadSaveFormatVersion>=78) {
        quint8 present{}; s >> present;
        if(present>1) { markCorrupt(s); return; }
        if(present) {
            PlanetAftermathObservation a; readAftermathObservation(s,a); x.aftermath=a;
            if(a.planet!=x.planet || a.observedTurn!=x.observedTurn
                || (x.kind!=decltype(x.kind)::PlanetaryAftermath && x.kind!=decltype(x.kind)::Bombardment
                    && x.kind!=decltype(x.kind)::ProductionCompleted)) markCorrupt(s);
            if constexpr(std::is_same_v<T,PendingPlayerReport>) { if(a.receivedTurn) markCorrupt(s); }
            else if(a.receivedTurn!=x.turn) markCorrupt(s);
        }
        if(x.kind==decltype(x.kind)::PlanetaryAftermath && !x.aftermath) markCorrupt(s);
    }
    if(gReadSaveFormatVersion>=79) {
        quint8 present{}; s >> present;
        if(present>1) { markCorrupt(s); return; }
        if(present) {
            ObjectObservation o; readObjectObservation(s,o); x.objectObservation=o;
            if(o.observedTurn!=x.observedTurn || x.aftermath || x.contact || x.sensorStatus || x.emission
                || (x.kind!=decltype(x.kind)::UnknownObjectObserved && x.kind!=decltype(x.kind)::ScientificData)
                || ((o.level==ObjectKnowledgeLevel::Studied)!=(x.kind==decltype(x.kind)::ScientificData))) markCorrupt(s);
            if(x.kind==decltype(x.kind)::ScientificData && (!o.kind || x.quantity!=unknown_object_research_points(*o.kind)
                || x.researchField!=unknown_object_research_field(*o.kind))) markCorrupt(s);
            if constexpr(std::is_same_v<T,PendingPlayerReport>) { if(o.receivedTurn) markCorrupt(s); }
            else if(o.receivedTurn!=x.turn || !same_position(o.position,x.position)) markCorrupt(s);
        }
        if(x.kind==decltype(x.kind)::UnknownObjectObserved && !x.objectObservation) markCorrupt(s);
    }
    if (gReadSaveFormatVersion>=80) {
        quint8 present{}; s >> present; if (present>1) { markCorrupt(s); return; }
        if (present) {
            MediumObservation medium; readMediumObservation(s,medium); x.mediumObservation=medium;
            if (x.kind!=decltype(x.kind)::InterstellarMediumObserved || medium.observedTurn!=x.observedTurn
                || x.contact || x.sensorStatus || x.emission || x.aftermath || x.objectObservation || x.battle || x.losses) markCorrupt(s);
            if constexpr (std::is_same_v<T,PendingPlayerReport>) { if (medium.receivedTurn) markCorrupt(s); }
            else if (medium.receivedTurn!=x.turn || !same_position(medium.center,x.position)) markCorrupt(s);
        }
        if (x.kind==decltype(x.kind)::InterstellarMediumObserved && !x.mediumObservation) markCorrupt(s);
    }
    if(!reconEvidenceMatches(x)) markCorrupt(s);
}
// An event-only trailing section keeps earlier record layouts intact.
void writeReconMessages(QDataStream& stream,const SaveGameData& save)
{
    QByteArray payload; QDataStream s(&payload,QIODevice::WriteOnly); s.setVersion(stream.version());
    s << quint32(save.strategicMessages.size());
    for(const auto& e:save.strategicMessages) { s << quint64(e.id); writeReconEvidence(s,e); }
    stream << quint32(0x5245434Eu) << quint32(payload.size()); stream.writeRawData(payload.data(),payload.size());
}
void readReconMessages(QDataStream& stream,SaveGameData& save)
{
    quint32 magic{},size{},count{}; stream >> magic >> size;
    if(magic!=0x5245434Eu || size>64*1024*1024) { markCorrupt(stream); return; }
    QByteArray bytes(size,'\0'); if(stream.readRawData(bytes.data(),size)!=int(size)) { markCorrupt(stream); return; }
    QDataStream s(bytes); s.setVersion(stream.version());
    if(!readCount(s,count) || count!=save.strategicMessages.size()) { markCorrupt(stream); return; }
    for(auto& e:save.strategicMessages) { quint64 id{}; s >> id; if(id!=e.id) { markCorrupt(stream); return; } readReconEvidence(s,e); }
    if(s.status()!=QDataStream::Ok || !s.atEnd()) markCorrupt(stream);
}

// Position evidence travels separately from the target fleet's radio packets.
void writeFriendlyRadar(QDataStream& stream,const GameState& state)
{
    stream << quint32(0x46524144u) << quint32(state.players.size()); // "FRAD"
    for(const auto& player:state.players) {
        stream << quint32(player.id);
        writeVector(stream,player.friendlyFleetContacts,writeReconContact);
        writeVector(stream,player.pendingFriendlyFleetContacts,writeReconContact);
    }
}
void readFriendlyRadar(QDataStream& stream,GameState& state)
{
    quint32 magic{},count{}; stream >> magic;
    if(magic!=0x46524144u || !readCount(stream,count) || count!=state.players.size()) { markCorrupt(stream); return; }
    for(auto& player:state.players) {
        quint32 id{}; stream >> id;
        if(id!=player.id || !readVector(stream,player.friendlyFleetContacts,readReconContact)
            || !readVector(stream,player.pendingFriendlyFleetContacts,readReconContact)) { markCorrupt(stream); return; }
        const auto valid=[&](const auto& contacts,bool pending) {
            for(std::size_t n=0;n<contacts.size();++n) {
                const auto& c=contacts[n];
                if(c.lost || (c.owner && c.owner!=player.id) || c.observedTurn>state.turn
                    || (!pending && c.deliveryTurn>state.turn)
                    || std::any_of(contacts.begin(),contacts.begin()+n,[&](const auto& old){return old.fleet==c.fleet && (!pending || old.observedTurn==c.observedTurn);})) return false;
            }
            return true;
        };
        if(!valid(player.friendlyFleetContacts,false) || !valid(player.pendingFriendlyFleetContacts,true)) { markCorrupt(stream); return; }
    }
}

void writeReportSources(QDataStream& stream,const GameState& state)
{
    quint32 count=0;
    // Visitor packets are serialized later in VISI; keep the historical TXOB
    // layout limited to reports already present when this extension is read.
    visit_fleet_report_outbox(state,[&](PlayerId,const auto& report){
        if constexpr (!std::is_same_v<std::remove_cvref_t<decltype(report)>,PendingVisitorReport>) ++count;
    });
    stream << quint32(0x54584F42u) << count; // "TXOB"
    visit_fleet_report_outbox(state,[&](PlayerId owner,const auto& report) {
        if constexpr (!std::is_same_v<std::remove_cvref_t<decltype(report)>,PendingVisitorReport>)
            stream << quint32(owner) << quint32(report.transmissionFleet);
    });
}
void readReportSources(QDataStream& stream,GameState& state)
{
    quint32 magic{},count{},seen{}; stream >> magic >> count;
    if(magic!=0x54584F42u) { markCorrupt(stream); return; }
    visit_fleet_report_outbox(state,[&](PlayerId owner,auto& report) {
        if constexpr (!std::is_same_v<std::remove_cvref_t<decltype(report)>,PendingVisitorReport>) {
            quint32 id{},source{}; stream >> id >> source; ++seen;
            if(id!=owner || seen>count || (source!=0)!=(report.deliveryTurn==kAwaitingTransmission)) markCorrupt(stream);
            report.transmissionFleet=source;
        }
    });
    if(seen!=count) markCorrupt(stream);
}
