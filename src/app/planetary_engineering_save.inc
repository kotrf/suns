// v85: permanent investment and frozen observer evidence are different records.
constexpr quint32 kEngineeringMagic=0x454e4749u; // ENGI
void writeEngineeringState(QDataStream& out,const PlanetaryEngineering& e)
{
    if(!engineering_state_valid(e)) { out.setStatus(QDataStream::WriteFailed); return; }
    writeEnum(out,e.kind); out<<quint32(e.work); writeMinerals(out,e.invested);
    out<<quint8(e.strength)<<quint8(e.operational)<<quint32(e.builder)<<quint64(e.startedTurn)<<quint64(e.completedTurn);
    if(e.kind==PlanetaryEngineeringKind::ClimateStabilizer)out<<quint8(e.targetTemperature);
    if(e.kind==PlanetaryEngineeringKind::AtmosphericRegulator)out<<quint8(e.targetRadiation);
    if(e.kind==PlanetaryEngineeringKind::RadicalTransformation) {
        const auto& t=e.transformation;writeEnum(out,t.phase);
        for(const auto& axis:{t.original,t.natural,t.target})out<<quint8(axis.temperature)<<quint8(axis.gravity)<<quint8(axis.radiation);
        out<<quint32(t.initiator)<<quint8(t.years)<<quint64(t.committedTurn)<<quint64(t.advancedTurn)<<quint64(t.shelterCapacity)<<quint8(t.paused);
    }
    if(e.kind==PlanetaryEngineeringKind::WormholeExperiment)
        out<<quint64(e.experiment.activationTurn)<<quint64(e.experiment.openedTurn)<<quint32(e.experiment.entrance);
}
void readEngineeringState(QDataStream& in,PlanetaryEngineering& e)
{
    quint32 work{},builder{}; quint8 strength{},online{}; quint64 started{},completed{};
    if(!readEnum(in,e.kind,newestEngineeringKind()))return;
    in>>work; readMinerals(in,e.invested); in>>strength>>online>>builder>>started>>completed;
    e.work=work; e.strength=strength; e.operational=online; e.builder=builder; e.startedTurn=started; e.completedTurn=completed;
    e.targetTemperature=50;e.targetRadiation=50;
    e.transformation={};e.experiment={};
    if(e.kind==PlanetaryEngineeringKind::ClimateStabilizer) {quint8 target{};in>>target;e.targetTemperature=target;}
    if(e.kind==PlanetaryEngineeringKind::AtmosphericRegulator) {quint8 target{};in>>target;e.targetRadiation=target;}
    if(e.kind==PlanetaryEngineeringKind::RadicalTransformation) {
        auto& t=e.transformation;readEnum(in,t.phase,quint8(TransformationPhase::Complete));
        for(auto* axis:{&t.original,&t.natural,&t.target}) {
            quint8 temperature{},gravity{},radiation{};in>>temperature>>gravity>>radiation;*axis={temperature,gravity,radiation};
        }
        quint32 initiator{};quint8 years{},paused{};quint64 committed{},advanced{},capacity{};
        in>>initiator>>years>>committed>>advanced>>capacity>>paused;
        t.initiator=initiator;t.years=years;t.committedTurn=committed;t.advancedTurn=advanced;t.shelterCapacity=capacity;t.paused=paused;
        if(paused>1)markCorrupt(in);
    }
    if(e.kind==PlanetaryEngineeringKind::WormholeExperiment) {
        quint64 activation{},opened{};quint32 entrance{};in>>activation>>opened>>entrance;
        e.experiment={activation,opened,entrance};
    }
    if(online>1 || !engineering_state_valid(e))markCorrupt(in);
}
bool publicEngineeringValid(const PlanetaryEngineering& e,SurveyLevel level,bool owned)
{
    return engineering_state_valid(e) && e.invested.ironium==0 && e.invested.boranium==0 && e.invested.germanium==0
        && (level>=SurveyLevel::OrbitalSurvey || e.kind==PlanetaryEngineeringKind::None)
        && (owned || e.transformation.shelterCapacity==0);
}
void writePlanetaryEngineering(QDataStream& stream,const GameState& state)
{
    QByteArray payload; QDataStream out(&payload,QIODevice::WriteOnly); out.setVersion(stream.version());
    out<<quint32(state.planets.size());
    for(const auto& p:state.planets) { out<<quint32(p.id); writeEngineeringState(out,p.engineering); }
    const auto snapshot=[&](const auto& record,PlayerId observer) {
        out<<quint8(bool(record.planetSnapshot));
        if(record.planetSnapshot) {
            if(!publicEngineeringValid(record.planetSnapshot->engineering,record.level,record.observedOwner==observer)) {out.setStatus(QDataStream::WriteFailed);return;}
            writeEngineeringState(out,record.planetSnapshot->engineering);
        }
    };
    out<<quint32(state.players.size());
    for(const auto& p:state.players) {
        out<<quint32(p.id)<<quint32(p.surveyKnowledge.size());
        for(const auto& k:p.surveyKnowledge) { out<<quint32(k.star); snapshot(k,p.id); }
        out<<quint32(p.pendingSurveyReports.size());
        for(const auto& r:p.pendingSurveyReports) { out<<quint32(r.star)<<quint64(r.observedTurn); snapshot(r,p.id); }
    }
    if(out.status()!=QDataStream::Ok) {stream.setStatus(QDataStream::WriteFailed);return;}
    stream<<kEngineeringMagic<<quint32(payload.size()); stream.writeRawData(payload.data(),payload.size());
}
void readPlanetaryEngineering(QDataStream& stream,GameState& state)
{
    quint32 magic{},size{},count{}; stream>>magic>>size;
    if(magic!=kEngineeringMagic || size>64*1024*1024) {markCorrupt(stream);return;}
    QByteArray payload(size,'\0'); if(stream.readRawData(payload.data(),size)!=int(size)) {markCorrupt(stream);return;}
    QDataStream in(payload); in.setVersion(stream.version());
    if(!readCount(in,count) || count!=state.planets.size()) {markCorrupt(stream);return;}
    for(auto& p:state.planets) {
        quint32 id{}; in>>id; if(id!=p.id) {markCorrupt(in);break;}
        readEngineeringState(in,p.engineering);
        if(!engineering_dates_valid(p.engineering,state.turn)) {markCorrupt(in);break;}
        std::size_t workItems=0;
        for(const auto& item:p.productionQueue) if(engineering_kind(item.kind)) {
            ++workItems;
            if(engineering_project_kind(item.kind)!=p.engineering.kind || item.remainingCost!=engineering_work(p.engineering.kind)-p.engineering.work
                || item.quantity!=1 || item.automation!=ProductionAutomation::None || item.autoGenerated || item.station || item.shipDesign)
                markCorrupt(in);
        }
        if(workItems>1 || (workItems && p.engineering.work==engineering_work(p.engineering.kind)))markCorrupt(in);
    }
    const auto snapshot=[&](auto& record,PlayerId observer) {
        quint8 present{}; in>>present;
        if(present>1 || bool(present)!=bool(record.planetSnapshot)) {markCorrupt(in);return;}
        if(record.planetSnapshot) {
            readEngineeringState(in,record.planetSnapshot->engineering);
            if(!publicEngineeringValid(record.planetSnapshot->engineering,record.level,record.observedOwner==observer)
                || !engineering_dates_valid(record.planetSnapshot->engineering,record.observedTurn))markCorrupt(in);
        }
    };
    if(!readCount(in,count) || count!=state.players.size()) {markCorrupt(stream);return;}
    for(auto& p:state.players) {
        quint32 id{}; in>>id;
        if(id!=p.id || !readCount(in,count) || count!=p.surveyKnowledge.size()) {markCorrupt(stream);return;}
        for(auto& k:p.surveyKnowledge) {quint32 star{};in>>star;if(star!=k.star)markCorrupt(in);snapshot(k,p.id);}
        if(!readCount(in,count) || count!=p.pendingSurveyReports.size()) {markCorrupt(stream);return;}
        for(auto& r:p.pendingSurveyReports) {quint32 star{};quint64 observed{};in>>star>>observed;if(star!=r.star || observed!=r.observedTurn)markCorrupt(in);snapshot(r,p.id);}
    }
    if(in.status()!=QDataStream::Ok || !in.atEnd())markCorrupt(stream);
}
