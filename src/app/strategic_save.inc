// Included inside save_game.cpp's private namespace. The bounded trailing
// extension preserves the old record layout for v12-61 migrations.
constexpr quint32 kStrategicExtensionMagic = 0x53545241u; // STRA

void writeElectronics(QDataStream& stream, const ElectronicsProgram& value)
{ writeEnum(stream, value.mode); stream << quint64(value.resumeTurn) << quint8(value.deception)
    << quint8(value.burstInterval) << quint64(value.firstBurstTurn) << quint8(value.activeBurst) << quint8(value.wakeOnArrival); }
void readElectronics(QDataStream& stream, ElectronicsProgram& value)
{
    quint64 resume{}; quint8 deception{};
    if (!readEnum(stream, value.mode, quint8(EmissionMode::RadioSilence))) return;
    stream >> resume >> deception; value.resumeTurn = resume; value.deception = deception != 0;
    if(gReadSaveFormatVersion>=66) {
        quint64 first{}; quint8 active{},arrival{};
        stream >> value.burstInterval >> first >> active >> arrival;
        value.firstBurstTurn=first; value.activeBurst=active; value.wakeOnArrival=arrival;
        if(active>1 || arrival>1) markCorrupt(stream);
    }
    if(deception>1 || !electronics_program_valid(value)) markCorrupt(stream);
}
void writeStrategicFleet(QDataStream& stream, const Fleet& f)
{
    stream << quint32(f.id);
    writeElectronics(stream, f.electronics); writeElectronics(stream, f.telemetry.electronics);
    stream << quint32(f.telemetryInTransit.size());
    for (const auto& p : f.telemetryInTransit) writeElectronics(stream, p.telemetry.electronics);
    stream << quint32(f.pendingCommands.size());
    for (const auto& p : f.pendingCommands) {
        stream << quint8(p.electronics.has_value());
        if (p.electronics) writeElectronics(stream, *p.electronics);
    }
}
void readStrategicFleet(QDataStream& stream, Fleet& f)
{
    quint32 id{}, count{}; stream >> id;
    if (id != f.id) { markCorrupt(stream); return; }
    readElectronics(stream, f.electronics); readElectronics(stream, f.telemetry.electronics);
    if (!readCount(stream, count) || count != f.telemetryInTransit.size()) { markCorrupt(stream); return; }
    for (auto& p : f.telemetryInTransit) readElectronics(stream, p.telemetry.electronics);
    if (!readCount(stream, count) || count != f.pendingCommands.size()) { markCorrupt(stream); return; }
    for (auto& p : f.pendingCommands) {
        quint8 present{}; stream >> present;
        if (present > 1 || (present && p.task)) { markCorrupt(stream); return; }
        if (present) { ElectronicsProgram e; readElectronics(stream, e); p.electronics = e; }
    }
}
void writeStrategicIntel(QDataStream& stream, const StrategicIntel& i)
{
    stream << quint32(i.id); writeEnum(stream, i.type); writePosition(stream, i.position);
    stream << i.radius << quint32(i.owner) << quint64(i.observedTurn) << quint64(i.deliveryTurn)
           << i.quantity << quint8(i.kind);
    writeEmissionObservation(stream,i.emission);
}
void readStrategicIntel(QDataStream& stream, StrategicIntel& i)
{
    quint32 id{}, owner{}; quint64 observed{}, delivery{}; quint8 kind{};
    stream >> id; i.id = id;
    if (!readEnum(stream, i.type, quint8(StrategicObjectKind::Emission))) return;
    readPosition(stream, i.position);
    stream >> i.radius >> owner >> observed >> delivery >> i.quantity >> kind;
    i.owner = owner; i.observedTurn = observed; i.deliveryTurn = delivery; i.kind = kind;
    if(gReadSaveFormatVersion>=76) {
        readEmissionObservation(stream,i.emission,observed);
        if(i.emission && (i.type!=StrategicObjectKind::Emission || kind!=quint8(i.emission->channel))) markCorrupt(stream);
    }
    if (!std::isfinite(i.radius) || i.radius < 0 || i.radius > 10000 || !std::isfinite(i.quantity)
        || i.quantity < 0 || kind > 2 || delivery < observed
        || (i.type == StrategicObjectKind::Emission && owner != 0)) markCorrupt(stream);
}
void writeEmissionPulses(QDataStream& stream,const GameState& state)
{
    stream << quint32(state.nextEmissionId) << quint32(state.emissions.size());
    for(const auto& pulse:state.emissions) {
        stream << quint32(pulse.id) << quint32(pulse.owner); writePosition(stream,pulse.position);
        stream << quint64(pulse.emittedTurn) << quint8(pulse.channel) << pulse.maximumRange << pulse.deceptionStrength
            << quint32(pulse.fleet) << quint32(pulse.colony) << quint32(pulse.receipts.size());
        for(const auto& receipt:pulse.receipts) stream << quint32(receipt.player) << quint32(receipt.fleet) << quint32(receipt.colony);
    }
}
void readEmissionPulses(QDataStream& stream,GameState& state)
{
    quint32 count{}; stream >> state.nextEmissionId;
    if(!state.nextEmissionId || state.nextEmissionId>0x7fffffffU || !readCount(stream,count)) { markCorrupt(stream); return; }
    for(quint32 n=0;n<count;++n) {
        EmissionPulse pulse; quint64 turn{}; quint8 channel{}; quint32 receipts{};
        stream >> pulse.id >> pulse.owner; readPosition(stream,pulse.position);
        stream >> turn >> channel >> pulse.maximumRange >> pulse.deceptionStrength >> pulse.fleet >> pulse.colony;
        pulse.emittedTurn=turn; pulse.channel=SignalChannel(channel);
        if(!pulse.id || pulse.id>=state.nextEmissionId || !find_player(state,pulse.owner)
            || !turn || turn>state.turn || channel>quint8(SignalChannel::Slow)
            || !std::isfinite(pulse.maximumRange) || pulse.maximumRange<0 || pulse.maximumRange>20000
            || !std::isfinite(pulse.deceptionStrength) || pulse.deceptionStrength<0 || pulse.deceptionStrength>0.95
            || (pulse.fleet && pulse.colony) || (pulse.colony && pulse.deceptionStrength)
            || std::any_of(state.emissions.begin(),state.emissions.end(),[&](const auto& p){return p.id==pulse.id;})
            || !readCount(stream,receipts)) { markCorrupt(stream); return; }
        for(quint32 i=0;i<receipts;++i) {
            EmissionReceipt receipt; stream >> receipt.player >> receipt.fleet >> receipt.colony;
            if(!find_player(state,receipt.player) || receipt.player==pulse.owner || bool(receipt.fleet)==bool(receipt.colony)
                || std::any_of(pulse.receipts.begin(),pulse.receipts.end(),[&](const auto& r) {
                    return r.player==receipt.player && r.fleet==receipt.fleet && r.colony==receipt.colony;
                })) { markCorrupt(stream); return; }
            pulse.receipts.push_back(receipt);
        }
        state.emissions.push_back(std::move(pulse));
    }
}
void writePlanetWarDamage(QDataStream& stream,const GameState& state)
{
    stream << quint32(state.planets.size());
    for(const auto& planet:state.planets) {
        const auto& d=planet.warDamage;
        stream << quint32(planet.id) << quint64(d.factoriesDestroyed) << quint64(d.minesDestroyed)
            << quint64(d.factoriesReconstructed) << quint64(d.minesReconstructed)
            << quint64(d.lastBombardmentTurn) << quint64(d.lastReconstructionTurn);
    }
}
void readPlanetWarDamage(QDataStream& stream,GameState& state)
{
    quint32 count{};
    if(!readCount(stream,count) || count!=state.planets.size()) { markCorrupt(stream); return; }
    for(auto& planet:state.planets) {
        quint32 id{}; quint64 factories{},mines{},rebuiltFactories{},rebuiltMines{},bombed{},rebuilt{};
        stream >> id >> factories >> mines >> rebuiltFactories >> rebuiltMines >> bombed >> rebuilt;
        if(id!=planet.id || rebuiltFactories>factories || rebuiltMines>mines || bombed>state.turn || rebuilt>state.turn
            || ((factories || mines) && !bombed) || ((rebuiltFactories || rebuiltMines) && !rebuilt)) { markCorrupt(stream); return; }
        planet.warDamage={factories,mines,rebuiltFactories,rebuiltMines,bombed,rebuilt};
    }
}
// v78: separate physical contamination and delivered observer knowledge.
void writePlanetaryRecovery(QDataStream& stream,const GameState& state)
{
    stream << quint32(state.planets.size());
    for(const auto& p:state.planets)
        stream << quint32(p.id) << quint8(p.warDamage.contamination) << quint64(p.warDamage.lastCleanupTurn)
            << quint8(p.aftermathKnownLocally);
    stream << quint32(state.players.size());
    for(const auto& p:state.players) {
        stream << quint32(p.id); writeVector(stream,p.planetaryAftermath,writeAftermathObservation);
    }
}
void readPlanetaryRecovery(QDataStream& stream,GameState& state)
{
    quint32 count{};
    if(!readCount(stream,count) || count!=state.planets.size()) { markCorrupt(stream); return; }
    for(auto& p:state.planets) {
        quint32 id{}; quint64 cleanup{}; quint8 local{}; stream >> id >> p.warDamage.contamination >> cleanup >> local;
        p.warDamage.lastCleanupTurn=cleanup;
        p.aftermathKnownLocally=local;
        if(id!=p.id || local>1 || p.warDamage.contamination>100 || cleanup>state.turn
            || (p.warDamage.contamination && !p.warDamage.lastBombardmentTurn)) { markCorrupt(stream); return; }
    }
    if(!readCount(stream,count) || count!=state.players.size()) { markCorrupt(stream); return; }
    for(auto& p:state.players) {
        quint32 id{}; stream >> id;
        if(id!=p.id || !readVector(stream,p.planetaryAftermath,readAftermathObservation)) { markCorrupt(stream); return; }
        for(std::size_t n=0;n<p.planetaryAftermath.size();++n) {
            const auto& x=p.planetaryAftermath[n];
            if(!x.receivedTurn || x.receivedTurn>state.turn || x.observedTurn>state.turn
                || std::any_of(p.planetaryAftermath.begin(),p.planetaryAftermath.begin()+n,
                    [&](const auto& old){return old.planet==x.planet;})) { markCorrupt(stream); return; }
        }
    }
}
void writeStrategicExtension(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(stream.version());
    out << quint32(state.nextMinefieldId) << quint32(state.minefields.size());
    for (const auto& f : state.minefields) {
        out << quint32(f.id) << quint32(f.owner); writePosition(out, f.position);
        out << f.mines << quint8(f.kind);
    }
    out << quint32(state.nextWreckId) << quint32(state.wrecks.size());
    for (const auto& w : state.wrecks) {
        out << quint32(w.id); writePosition(out, w.position); out << quint64(w.expiresTurn);
        for (auto level : w.technology) out << quint8(level);
    }
    out << quint32(state.players.size());
    for (const auto& p : state.players) {
        out << quint32(p.id);
        writeVector(out, p.strategicIntel, writeStrategicIntel);
        writeVector(out, p.pendingStrategicIntel, writeStrategicIntel);
        out << quint32(p.fieldScience.size());
        for (const auto& r : p.fieldScience) out << quint64(r.key) << quint64(r.deliveryTurn);
    }
    out << quint32(state.fleets.size());
    for (const auto& f : state.fleets) writeStrategicFleet(out, f);
    out << quint32(state.pendingFleetLossContacts.size());
    for (const auto& f : state.pendingFleetLossContacts) writeStrategicFleet(out, f.lastContact);
    out << quint32(state.wormholeTransits.size());
    for (const auto& f : state.wormholeTransits) writeStrategicFleet(out, f.lastContact);
    // Compact receipt-year battle history, keyed to the existing snapshots.
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id) << quint32(player.history.size());
        for (const auto& snapshot : player.history) {
            out << quint64(snapshot.turn) << quint8(snapshot.battleStatisticsRecorded ? 1 : 0)
                << quint64(snapshot.battles.reports) << quint64(snapshot.battles.ownShipsLost)
                << quint64(snapshot.battles.opposingShipsLost);
            for (const auto& losses : snapshot.operationalLosses)
                out << quint8(losses.recorded) << quint64(losses.reports)
                    << quint64(losses.ownColonistsLost) << quint64(losses.opposingColonistsLost)
                    << quint64(losses.ownInstallationsLost) << quint64(losses.opposingInstallationsLost)
                    << quint64(losses.ownShipsLost);
        }
    }
    // v65: station fitting, colony defenses and delayed station outcomes.
    out << quint32(state.planets.size());
    for(const auto& p:state.planets) {
        out << quint32(p.id) << quint16(p.defenses) << quint32(p.productionQueue.size());
        for(const auto& item:p.productionQueue) {
            out << quint8(item.station.has_value()); if(item.station) writeStationFit(out,*item.station);
        }
    }
    out << quint32(state.orbitalStations.size());
    for(const auto& station:state.orbitalStations) {
        out << quint32(station.id); writeStationFit(out,{station_loadout(station),0}); out << station.damagePercent;
    }
    out << quint32(state.pendingStationContacts.size());
    for(const auto& c:state.pendingStationContacts) {
        writeOrbitalStation(out,c.lastContact); writeStationFit(out,{station_loadout(c.lastContact),0});
        out << c.lastContact.damagePercent << quint64(c.observedTurn) << quint64(c.deliveryTurn);
    }
    out << quint32(state.players.size());
    for(const auto& p:state.players) {
        out << quint32(p.id) << quint32(p.history.size());
        for(const auto& h:p.history) out << quint64(h.turn) << quint64(h.battles.ownStationsLost) << quint64(h.battles.opposingStationsLost);
    }
    // v66: immutable delivered/pending contacts and observer measurements.
    out << quint32(state.players.size());
    for(const auto& p:state.players) {
        out << quint32(p.id);
        writeVector(out,p.enemyContacts,writeReconContact); writeVector(out,p.pendingEnemyContacts,writeReconContact);
        writeVector(out,p.sensorStatus,writeSensorStatus); writeVector(out,p.pendingSensorStatus,writeSensorStatus);
        writeVector(out,p.observedSensorStatus,writeSensorStatus);
        out << quint32(p.observedEnemyFleets.size());
        for(const auto& c:p.observedEnemyFleets) { out << quint32(c.fleet) << quint8(bool(c.observation.fleet)); if(c.observation.fleet) writeReconContact(out,c.observation); }
        out << quint32(p.pendingPlayerReports.size());
        for(const auto& r:p.pendingPlayerReports) writeReconEvidence(out,r);
    }
    writeComputerControllers(out, state);
    writeEmissionPulses(out,state);
    writePlanetWarDamage(out,state);
    writePlanetaryRecovery(out,state);
    writeUnknownObjects(out,state);
    writeInterstellarMedium(out,state);
    writeOutposts(out,state);
    writeSurveySnapshots(out,state);
    writeMobileBases(out,state);
    writePlanetaryEngineering(out,state);
    if (out.status()!=QDataStream::Ok) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kStrategicExtensionMagic << quint32(payload.size());
    stream.writeRawData(payload.constData(), payload.size());
}
void readStrategicExtension(QDataStream& stream, GameState& state)
{
    quint32 magic{}, size{}, count{}; stream >> magic >> size;
    if (magic != kStrategicExtensionMagic || size > 64*1024*1024) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), size) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    in >> state.nextMinefieldId;
    if (!state.nextMinefieldId || !readCount(in, count)) { markCorrupt(stream); return; }
    for (quint32 n = 0; n < count; ++n) {
        Minefield f; in >> f.id >> f.owner; readPosition(in, f.position); in >> f.mines >> f.kind;
        if (!f.id || f.id >= state.nextMinefieldId || !find_player(state, f.owner) || f.kind > 2
            || !std::isfinite(f.mines) || f.mines < 0 || f.mines > 1e8
            || std::any_of(state.minefields.begin(), state.minefields.end(), [&](const auto& x) { return x.id == f.id; })) {
            markCorrupt(stream); return;
        }
        state.minefields.push_back(f);
    }
    in >> state.nextWreckId;
    if (!state.nextWreckId || !readCount(in, count)) { markCorrupt(stream); return; }
    for (quint32 n = 0; n < count; ++n) {
        SalvageWreck w; quint64 expires{}; in >> w.id; readPosition(in, w.position); in >> expires; w.expiresTurn = expires;
        for (auto& level : w.technology) in >> level;
        if (!w.id || w.id >= state.nextWreckId || std::any_of(state.wrecks.begin(), state.wrecks.end(),
            [&](const auto& x) { return x.id == w.id; })) { markCorrupt(stream); return; }
        state.wrecks.push_back(w);
    }
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& p : state.players) {
        quint32 id{}; in >> id;
        if (id != p.id || !readVector(in, p.strategicIntel, readStrategicIntel)
            || !readVector(in, p.pendingStrategicIntel, readStrategicIntel)
            || !readCount(in, count)) { markCorrupt(stream); return; }
        for (quint32 n = 0; n < count; ++n) {
            quint64 key{}, delivery{}; in >> key >> delivery;
            if ((key >> 32) < 1 || (key >> 32) > (gReadSaveFormatVersion>=79 ? 5U : 4U) || std::any_of(p.fieldScience.begin(), p.fieldScience.end(),
                [&](const auto& r) { return r.key == key; })) { markCorrupt(stream); return; }
            p.fieldScience.push_back({key, delivery});
        }
    }
    if (!readCount(in, count) || count != state.fleets.size()) { markCorrupt(stream); return; }
    for (auto& f : state.fleets) readStrategicFleet(in, f);
    if (!readCount(in, count) || count != state.pendingFleetLossContacts.size()) { markCorrupt(stream); return; }
    for (auto& f : state.pendingFleetLossContacts) readStrategicFleet(in, f.lastContact);
    if (!readCount(in, count) || count != state.wormholeTransits.size()) { markCorrupt(stream); return; }
    for (auto& f : state.wormholeTransits) readStrategicFleet(in, f.lastContact);
    if (gReadSaveFormatVersion >= 63) {
        if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
        for (auto& player : state.players) {
            quint32 id{}; in >> id;
            if (id != player.id || !readCount(in, count) || count != player.history.size()) {
                markCorrupt(stream); return;
            }
            for (auto& snapshot : player.history) {
                quint64 turn{}, reports{}, own{}, opposing{}; quint8 recorded{};
                in >> turn >> recorded >> reports >> own >> opposing;
                const auto markers = std::count_if(snapshot.milestones.begin(), snapshot.milestones.end(),
                    [](const auto& marker) { return marker.kind == HistoryMilestoneKind::SpaceBattle; });
                if (turn != snapshot.turn || recorded > 1
                    || (!recorded && (reports || own || opposing))
                    || (recorded && reports != std::uint64_t(markers))
                    || (!reports && (own || opposing))) { markCorrupt(stream); return; }
                snapshot.battleStatisticsRecorded = recorded == 1;
                snapshot.battles = {reports, own, opposing};
                if (gReadSaveFormatVersion >= 64) {
                    for (std::size_t i = 0; i < snapshot.operationalLosses.size(); ++i) {
                        quint8 measured{}; quint64 reports{}, own{}, opposing{}, ownInstallations{}, opposingInstallations{}, ships{};
                        in >> measured >> reports >> own >> opposing >> ownInstallations >> opposingInstallations >> ships;
                        const auto category = static_cast<OperationalLossKind>(i);
                        const auto markers = std::count_if(snapshot.milestones.begin(), snapshot.milestones.end(),
                            [&](const auto& marker) { return milestone_loss_kind(marker.kind) == category; });
                        if (measured > 1 || (!measured && (reports || own || opposing || ownInstallations || opposingInstallations || ships))
                            || (measured && reports != std::uint64_t(markers))
                            || (!reports && (own || opposing || ownInstallations || opposingInstallations || ships))
                            || (category != OperationalLossKind::Bombardment && (ownInstallations || opposingInstallations))
                            || (category != OperationalLossKind::Mines && ships)
                            || (category == OperationalLossKind::Mines && opposing)) { markCorrupt(stream); return; }
                        snapshot.operationalLosses[i] = {measured == 1, reports, own, opposing,
                            ownInstallations, opposingInstallations, ships};
                    }
                }
            }
        }
    }
    if(gReadSaveFormatVersion>=65) {
        if(!readCount(in,count) || count!=state.planets.size()) { markCorrupt(stream); return; }
        for(auto& p:state.planets) {
            quint32 id{},items{}; in >> id >> p.defenses;
            if(id!=p.id || p.defenses>kMaximumColonyDefenses || !readCount(in,items) || items!=p.productionQueue.size()) { markCorrupt(stream); return; }
            for(auto& item:p.productionQueue) {
                quint8 present{}; in >> present;
                if(present>1 || (present && (item.kind!=ProductionKind::OrbitalStation || item.quantity!=1 || item.automation!=ProductionAutomation::None))) { markCorrupt(stream); return; }
                if(present) { StationConstruction work; readStationFit(in,work); item.station=std::move(work); }
            }
        }
        if(!readCount(in,count) || count!=state.orbitalStations.size()) { markCorrupt(stream); return; }
        for(auto& station:state.orbitalStations) {
            quint32 id{}; StationConstruction fit; in >> id; readStationFit(in,fit); in >> station.damagePercent;
            if(id!=station.id || fit.refitStation || fit.loadout.hull!=station.hull
                || fit.loadout.shipyard!=orbital_station_has_module(station,OrbitalStationModule::Shipyard)
                || fit.loadout.refueling!=orbital_station_has_module(station,OrbitalStationModule::RefuelingDepot)
                || !std::isfinite(station.damagePercent) || station.damagePercent<0 || station.damagePercent>=100) { markCorrupt(stream); return; }
            station.components=std::move(fit.loadout.components);
        }
        if(!readCount(in,count)) { markCorrupt(stream); return; }
        for(quint32 i=0;i<count;++i) {
            PendingStationContact c; StationConstruction fit;
            readOrbitalStation(in,c.lastContact); readStationFit(in,fit);
            quint64 observed{},delivery{}; in >> c.lastContact.damagePercent >> observed >> delivery;
            c.observedTurn=observed; c.deliveryTurn=delivery;
            if(fit.refitStation || c.lastContact.id==0 || c.lastContact.id>=state.nextOrbitalStationId
                || !find_player(state,c.lastContact.owner) || c.observedTurn>state.turn || c.deliveryTurn<=c.observedTurn
                || std::none_of(state.planets.begin(),state.planets.end(),[&](const auto& p) { return p.id==c.lastContact.planet; })
                || fit.loadout.hull!=c.lastContact.hull
                || fit.loadout.shipyard!=orbital_station_has_module(c.lastContact,OrbitalStationModule::Shipyard)
                || fit.loadout.refueling!=orbital_station_has_module(c.lastContact,OrbitalStationModule::RefuelingDepot)
                || !std::isfinite(c.lastContact.damagePercent) || c.lastContact.damagePercent<0 || c.lastContact.damagePercent>=100) { markCorrupt(stream); return; }
            c.lastContact.components=std::move(fit.loadout.components); state.pendingStationContacts.push_back(std::move(c));
        }
        if(!readCount(in,count) || count!=state.players.size()) { markCorrupt(stream); return; }
        for(auto& p:state.players) {
            quint32 id{}; in >> id;
            if(id!=p.id || !readCount(in,count) || count!=p.history.size()) { markCorrupt(stream); return; }
            for(auto& h:p.history) {
                quint64 turn{},own{},opposing{}; in >> turn >> own >> opposing;
                if(turn!=h.turn || ((!h.battleStatisticsRecorded || !h.battles.reports) && (own || opposing))) { markCorrupt(stream); return; }
                h.battles.ownStationsLost=own; h.battles.opposingStationsLost=opposing;
            }
        }
    }
    if(gReadSaveFormatVersion>=66) {
        if(!readCount(in,count) || count!=state.players.size()) { markCorrupt(stream); return; }
        for(auto& p:state.players) {
            quint32 id{}; in >> id;
            if(id!=p.id || !readVector(in,p.enemyContacts,readReconContact) || !readVector(in,p.pendingEnemyContacts,readReconContact)
                || !readVector(in,p.sensorStatus,readSensorStatus) || !readVector(in,p.pendingSensorStatus,readSensorStatus)
                || !readVector(in,p.observedSensorStatus,readSensorStatus)) { markCorrupt(stream); return; }
            for(std::size_t n=0;n<p.enemyContacts.size();++n) {
                const auto& c=p.enemyContacts[n];
                if(c.observedTurn>state.turn || c.deliveryTurn>state.turn || c.owner==p.id
                    || std::any_of(p.enemyContacts.begin(),p.enemyContacts.begin()+n,[&](const auto& x){return x.fleet==c.fleet;})) { markCorrupt(stream); return; }
            }
            for(const auto& c:p.pendingEnemyContacts) if(c.observedTurn>state.turn || c.owner==p.id) { markCorrupt(stream); return; }
            const auto repeatedSensors=[](const auto& values) {
                for(std::size_t n=0;n<values.size();++n) if(std::any_of(values.begin(),values.begin()+n,[&](const auto& x){return x.fleet==values[n].fleet && x.colony==values[n].colony;})) return true;
                return false;
            };
            if(repeatedSensors(p.sensorStatus) || repeatedSensors(p.observedSensorStatus)) { markCorrupt(stream); return; }
            for(const auto& x:p.sensorStatus) if(x.observedTurn>state.turn || x.deliveryTurn>state.turn) { markCorrupt(stream); return; }
            for(const auto& x:p.pendingSensorStatus) if(x.observedTurn>state.turn) { markCorrupt(stream); return; }
            if(!readCount(in,count) || count!=p.observedEnemyFleets.size()) { markCorrupt(stream); return; }
            for(auto& c:p.observedEnemyFleets) {
                quint32 id{}; quint8 present{}; in >> id >> present;
                if(id!=c.fleet || present>1) { markCorrupt(stream); return; }
                if(present) {
                    readReconContact(in,c.observation);
                    if(c.observation.fleet!=c.fleet || c.observation.sourceFleet!=c.reportingFleet
                        || c.observation.sourceColony!=c.reportingColony || c.observation.lost || !same_position(c.observation.position,c.lastPosition)) { markCorrupt(stream); return; }
                }
            }
            if(!readCount(in,count) || count!=p.pendingPlayerReports.size()) { markCorrupt(stream); return; }
            for(auto& r:p.pendingPlayerReports) readReconEvidence(in,r);
        }
    }
    if (gReadSaveFormatVersion >= 67) readComputerControllers(in, state);
    if (gReadSaveFormatVersion >= 76) readEmissionPulses(in,state);
    if (gReadSaveFormatVersion >= 77) readPlanetWarDamage(in,state);
    if (gReadSaveFormatVersion >= 78) readPlanetaryRecovery(in,state);
    if (gReadSaveFormatVersion >= 79) readUnknownObjects(in,state);
    if (gReadSaveFormatVersion >= 80) readInterstellarMedium(in,state);
    if (gReadSaveFormatVersion >= 82) readOutposts(in,state);
    if (gReadSaveFormatVersion >= 83) readSurveySnapshots(in,state);
    if (gReadSaveFormatVersion >= 84) readMobileBases(in,state);
    if (gReadSaveFormatVersion >= 85) readPlanetaryEngineering(in,state);
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
