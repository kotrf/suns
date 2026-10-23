// v94: bounded habitat truth and owner-only frozen operating evidence.
// v95 adds the received radar tier to the same evidence as the production queue.
constexpr quint32 kOrbitalHabitatsMagic=0x48414249u; // HABI
void writeHabitat(QDataStream& out,const OrbitalHabitat& h)
{
    out<<quint64(h.population)<<quint8(h.reserveYears)<<quint8(h.service)<<quint64(h.servicedTurn)
        <<quint32(h.productionPaid)<<quint64(h.populationLost);
}
void readHabitat(QDataStream& in,OrbitalHabitat& h,std::uint64_t turn)
{
    quint64 population{},serviced{},lost{};quint8 reserve{},service{};quint32 paid{};
    in>>population>>reserve>>service>>serviced>>paid>>lost;
    if(population>kOrbitalPopulationCapacity || reserve>kHabitatReserveYears || service>quint8(HabitatService::Failing)
        || serviced>turn || (paid!=0 && paid!=2 && paid!=4) || lost>kOrbitalPopulationCapacity/10
        || (service!=quint8(HabitatService::Failing) && lost)) markCorrupt(in);
    h={population,reserve,HabitatService(service),serviced,paid,lost};
}
void writeHabitatEvidence(QDataStream& out,const auto& record,PlayerId owner)
{
    const auto* evidence=record.planetSnapshot && record.planetSnapshot->habitat?&*record.planetSnapshot->habitat:nullptr;
    if(evidence && (record.observedOwner!=owner || record.level<SurveyLevel::OrbitalSurvey)) {
        out.setStatus(QDataStream::WriteFailed);return;
    }
    out<<quint8(bool(evidence));
    if(evidence) {
        writeHabitat(out,evidence->habitat);out<<quint8(evidence->stationSupported);out<<quint64(evidence->surfacePopulation)<<quint32(evidence->industry);
        writeMinerals(out,evidence->minerals);out<<quint32(evidence->mines);
        writeVector(out,evidence->queue,[](QDataStream& stream,const ProductionItem& item) {
            writeProductionItem(stream,item);writeProductionExtras(stream,item);stream<<quint8(bool(item.station));
            if(item.station)writeStationFit(stream,*item.station);
        });
        out<<quint64(evidence->stationEvidenceTime)<<quint8(bool(evidence->station));
        if(evidence->station) {
            writeOrbitalStation(out,*evidence->station);out<<evidence->station->damagePercent;
            writeVector(out,evidence->station->components,[](QDataStream& stream,ShipComponentType c){writeEnum(stream,c);});
        }
        writeEnum(out,evidence->radar);
    }
}
void readHabitatEvidence(QDataStream& in,auto& record,PlayerId owner,const GameState& state)
{
    quint8 present{};in>>present;
    if(present>1 || (present && (!record.planetSnapshot || record.observedOwner!=owner || record.level<SurveyLevel::OrbitalSurvey))) {markCorrupt(in);return;}
    if(!present) return;
    HabitatObservation value;readHabitat(in,value.habitat,state.turn);
    quint8 supported{};in>>supported;if(supported>1)markCorrupt(in);value.stationSupported=supported;
    quint64 surface{};in>>surface>>value.industry;value.surfacePopulation=surface;readMinerals(in,value.minerals);in>>value.mines;
    readVector(in,value.queue,[](QDataStream& stream,ProductionItem& item) {
        readProductionItem(stream,item);readProductionExtras(stream,item);quint8 station{};stream>>station;
        if(station>1 || (station && (item.kind!=ProductionKind::OrbitalStation || item.quantity!=1 || item.automation!=ProductionAutomation::None))) {markCorrupt(stream);return;}
        if(station){item.station.emplace();readStationFit(stream,*item.station);}
    });
    quint64 stationTime{};in>>stationTime;value.stationEvidenceTime=stationTime;
    if(stationTime/3>state.turn)markCorrupt(in);
    quint8 stationPresent{};in>>stationPresent;if(stationPresent>1)markCorrupt(in);
    if(stationPresent) {
        value.station.emplace();auto& station=*value.station;readOrbitalStation(in,station);in>>station.damagePercent;
        readVector(in,station.components,[](QDataStream& stream,ShipComponentType& c){readEnum(stream,c,newestShipComponent());});
        const auto* planet=find_planet_at_star(state,record.star);
        if((planet && station.planet!=planet->id) || !station.planet || !station.id || station.owner!=owner || !std::isfinite(station.damagePercent)
            || station.damagePercent<0 || station.damagePercent>=100 || !station_loadout_valid(station_loadout(station)))markCorrupt(in);
    }
    if(gReadSaveFormatVersion>=95) readEnum(in,value.radar,quint8(PlanetaryRadarLevel::Full));
    const bool support=value.station && value.station->hull>=OrbitalStationHullType::Starbase && value.station->damagePercent<100
        && orbital_station_has_module(*value.station,OrbitalStationModule::Shipyard);
    if(support!=value.stationSupported)markCorrupt(in);
    if(value.habitat.population && (record.planetSnapshot->engineering.kind!=PlanetaryEngineeringKind::OrbitalHabitat
        || record.planetSnapshot->engineering.work!=engineering_work(PlanetaryEngineeringKind::OrbitalHabitat))) markCorrupt(in);
    if(value.surfacePopulation>std::numeric_limits<std::uint64_t>::max()-value.habitat.population)markCorrupt(in);
    std::size_t engineeringItems=0;
    for(const auto& item:value.queue) if(engineering_kind(item.kind)) {
        const auto& e=record.planetSnapshot->engineering;
        if(++engineeringItems>1 || engineering_project_kind(item.kind)!=e.kind || item.remainingCost!=engineering_work(e.kind)-e.work
            || e.work==engineering_work(e.kind) || item.quantity!=1 || item.automation!=ProductionAutomation::None || item.autoGenerated || item.station || item.shipDesign)markCorrupt(in);
    }
    record.planetSnapshot->habitat=value;
}
void writeOrbitalHabitats(QDataStream& stream,const GameState& state)
{
    QByteArray payload;QDataStream out(&payload,QIODevice::WriteOnly);out.setVersion(stream.version());
    out<<quint32(state.planets.size());
    for(const auto& p:state.planets) {out<<quint32(p.id);writeHabitat(out,p.habitat);}
    out<<quint32(state.players.size());
    for(const auto& p:state.players) {
        out<<quint32(p.id)<<quint32(p.surveyKnowledge.size());
        for(const auto& k:p.surveyKnowledge) {out<<quint32(k.star);writeHabitatEvidence(out,k,p.id);}
        out<<quint32(p.pendingSurveyReports.size());
        for(const auto& r:p.pendingSurveyReports) {out<<quint32(r.star)<<quint64(r.observedTurn);writeHabitatEvidence(out,r,p.id);}
    }
    if(out.status()!=QDataStream::Ok || payload.size()>32*1024*1024) {stream.setStatus(QDataStream::WriteFailed);return;}
    stream<<kOrbitalHabitatsMagic<<quint32(payload.size());stream.writeRawData(payload.data(),int(payload.size()));
}
void readOrbitalHabitats(QDataStream& stream,GameState& state)
{
    quint32 magic{},size{},count{},id{};stream>>magic>>size;
    if(magic!=kOrbitalHabitatsMagic || size>32U*1024U*1024U) {markCorrupt(stream);return;}
    QByteArray payload(size,'\0');if(stream.readRawData(payload.data(),int(size))!=int(size)) {markCorrupt(stream);return;}
    QDataStream in(payload);in.setVersion(stream.version());
    if(!readCount(in,count) || count!=state.planets.size()) {markCorrupt(stream);return;}
    for(auto& p:state.planets) {
        in>>id;if(id!=p.id)markCorrupt(in);readHabitat(in,p.habitat,state.turn);
        if(p.habitat.population && (!p.owner || !habitat_completed(p))) markCorrupt(in);
        if(!habitat_completed(p) && p.habitat!=OrbitalHabitat{})markCorrupt(in);
        if(p.population>std::numeric_limits<std::uint64_t>::max()-p.habitat.population)markCorrupt(in);
        if(p.engineering.kind!=PlanetaryEngineeringKind::OrbitalHabitat && p.habitat!=OrbitalHabitat{}) markCorrupt(in);
    }
    if(!readCount(in,count) || count!=state.players.size()) {markCorrupt(stream);return;}
    for(auto& p:state.players) {
        in>>id;if(id!=p.id || !readCount(in,count) || count!=p.surveyKnowledge.size()) {markCorrupt(stream);return;}
        for(auto& k:p.surveyKnowledge) {in>>id;if(id!=k.star)markCorrupt(in);readHabitatEvidence(in,k,p.id,state);}
        if(!readCount(in,count) || count!=p.pendingSurveyReports.size()) {markCorrupt(stream);return;}
        for(auto& r:p.pendingSurveyReports) {
            quint64 observed{};in>>id>>observed;if(id!=r.star || observed!=r.observedTurn)markCorrupt(in);
            readHabitatEvidence(in,r,p.id,state);
        }
    }
    if(in.status()!=QDataStream::Ok || !in.atEnd())markCorrupt(stream);
}
