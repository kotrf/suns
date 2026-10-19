constexpr quint32 kExoticMaterialsMagic = 0x45584f54u; // EXOT
void writeXenocrystalSite(QDataStream& out, const XenocrystalSite& site)
{ out << quint32(site.original) << quint32(site.remaining) << quint32(site.surface) << quint32(site.extractedLastYear); }
void readXenocrystalSite(QDataStream& in, XenocrystalSite& site)
{
    in >> site.original >> site.remaining >> site.surface >> site.extractedLastYear;
    if (site.original > kMaximumXenocrystal || site.remaining > site.original || site.surface > kMaximumXenocrystal
        || site.extractedLastYear > kXenocrystalPerPlanetYear) markCorrupt(in);
}
void writeResonanceLab(QDataStream& out, const ResonanceLaboratory& lab)
{
    out << quint8(lab.started) << quint8(lab.active) << quint8(lab.operational) << quint32(lab.work) << quint32(lab.crystalsPaid);
    writeMinerals(out, lab.invested);
    out << quint32(lab.builder) << quint64(lab.startedTurn) << quint64(lab.completedTurn);
}
void readResonanceLab(QDataStream& in, ResonanceLaboratory& lab)
{
    quint8 started{}, active{}, operational{}; quint64 startedTurn{}, completedTurn{};
    in >> started >> active >> operational >> lab.work >> lab.crystalsPaid;
    readMinerals(in, lab.invested); in >> lab.builder >> startedTurn >> completedTurn;
    lab.startedTurn = startedTurn; lab.completedTurn = completedTurn;
    lab.started = started; lab.active = active; lab.operational = operational;
    if (started > 1 || active > 1 || operational > 1 || !resonance_state_valid(lab)) markCorrupt(in);
}
void writeExoticObservation(QDataStream& out, const std::optional<PlanetSurveySnapshot>& snapshot)
{
    out << quint8(snapshot && snapshot->xenocrystal);
    if (snapshot && snapshot->xenocrystal) {
        const auto& observation = *snapshot->xenocrystal;
        writeXenocrystalSite(out, observation.site);
        out << quint64(observation.observedTurn) << quint8(observation.depositSurveyed) << quint64(observation.depositObservedTurn) << quint8(observation.stockPhase);
    }
    writeResonanceLab(out, snapshot ? snapshot->resonanceLab : ResonanceLaboratory{});
}
void readExoticObservation(QDataStream& in, std::optional<PlanetSurveySnapshot>& snapshot, std::uint64_t latest)
{
    quint8 present{}; in >> present;
    if (present > 1 || (present && !snapshot)) { markCorrupt(in); return; }
    if (present) {
        XenocrystalObservation observation; readXenocrystalSite(in, observation.site);
        quint8 surveyed{}, phase{}; quint64 observed{}, depositObserved{}; in >> observed >> surveyed >> depositObserved >> phase;
        observation.observedTurn = observed; observation.depositObservedTurn = depositObserved;
        observation.depositSurveyed = surveyed != 0;
        observation.stockPhase = phase;
        if (surveyed > 1 || phase > 1 || !observation.observedTurn || observation.observedTurn > latest
            || observation.depositObservedTurn > latest
            || (surveyed ? !observation.depositObservedTurn : observation.depositObservedTurn != 0
                || observation.site.original != 0 || observation.site.remaining != 0)) markCorrupt(in);
        snapshot->xenocrystal = observation;
    }
    ResonanceLaboratory lab; readResonanceLab(in, lab);
    if (snapshot) snapshot->resonanceLab = lab;
    else if (lab.started) markCorrupt(in);
}
void writeExoticMaterials(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.planets.size());
    for (const auto& p : state.planets) {
        out << quint32(p.id); writeXenocrystalSite(out, p.xenocrystal);
        writeResonanceLab(out, p.resonanceLab);
    }
    out << quint32(state.fleets.size() + state.pendingFleetLossContacts.size() + state.wormholeTransits.size());
    const auto write = [&](const Fleet& f) {
        out << quint32(f.id) << quint32(f.xenocrystal) << quint32(f.telemetry.xenocrystal) << quint32(f.telemetryInTransit.size());
        for (const auto& packet : f.telemetryInTransit) out << quint32(packet.telemetry.xenocrystal);
    };
    for (const auto& f : state.fleets) write(f);
    for (const auto& f : state.pendingFleetLossContacts) write(f.lastContact);
    for (const auto& f : state.wormholeTransits) write(f.lastContact);
    out << quint32(state.players.size());
    for (const auto& p : state.players) {
        out << quint32(p.id) << quint32(p.surveyKnowledge.size());
        for (const auto& k : p.surveyKnowledge) { out << quint32(k.star); writeExoticObservation(out, k.planetSnapshot); }
        out << quint32(p.pendingSurveyReports.size());
        for (const auto& r : p.pendingSurveyReports) { out << quint32(r.star) << quint64(r.observedTurn); writeExoticObservation(out, r.planetSnapshot); }
    }
    if (out.status() != QDataStream::Ok || payload.size() > 32 * 1024 * 1024) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kExoticMaterialsMagic << quint32(payload.size()); stream.writeRawData(payload.data(), payload.size());
}
void readExoticMaterials(QDataStream& stream, GameState& state, bool playerTurn)
{
    quint32 magic{}, size{}, count{}; stream >> magic >> size;
    if (magic != kExoticMaterialsMagic || size > 32U * 1024U * 1024U) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), int(size)) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in, count) || count != state.planets.size()) { markCorrupt(stream); return; }
    for (auto& p : state.planets) {
        quint32 id{}; in >> id; if (id != p.id) { markCorrupt(in); break; }
        readXenocrystalSite(in, p.xenocrystal);
        readResonanceLab(in, p.resonanceLab);
    }
    if (!readCount(in, count) || count != state.fleets.size() + state.pendingFleetLossContacts.size() + state.wormholeTransits.size()) { markCorrupt(stream); return; }
    const auto read = [&](Fleet& f) {
        quint32 id{}, packets{}; in >> id >> f.xenocrystal >> f.telemetry.xenocrystal;
        if (id != f.id || f.xenocrystal > kMaximumXenocrystal || f.telemetry.xenocrystal > kMaximumXenocrystal
            || !readCount(in, packets) || packets != f.telemetryInTransit.size()) { markCorrupt(in); return; }
        for (auto& packet : f.telemetryInTransit) {
            in >> packet.telemetry.xenocrystal; if (packet.telemetry.xenocrystal > kMaximumXenocrystal) markCorrupt(in);
        }
    };
    for (auto& f : state.fleets) read(f);
    for (auto& f : state.pendingFleetLossContacts) read(f.lastContact);
    for (auto& f : state.wormholeTransits) read(f.lastContact);
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& p : state.players) {
        quint32 id{}; in >> id;
        if (id != p.id || !readCount(in, count) || count != p.surveyKnowledge.size()) { markCorrupt(in); break; }
        for (auto& k : p.surveyKnowledge) {
            in >> id; if (id != k.star) markCorrupt(in);
            readExoticObservation(in, k.planetSnapshot, state.turn);
        }
        if (!readCount(in, count) || count != p.pendingSurveyReports.size()) { markCorrupt(in); break; }
        for (auto& r : p.pendingSurveyReports) {
            quint64 observed{}; in >> id >> observed;
            if (id != r.star || observed != r.observedTurn) markCorrupt(in);
            readExoticObservation(in, r.planetSnapshot, r.observedTurn);
        }
    }
    if (!playerTurn) for (const auto& f : state.fleets)
        if (f.xenocrystal && fleet_cargo_used(state, f) > fleet_cargo_capacity(state, f) + 1e-6) markCorrupt(in);
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
