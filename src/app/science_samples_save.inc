// Physical manifests and every immutable telemetry snapshot stay separate.
constexpr quint32 kScienceSamplesMagic = 0x53414d50u; // SAMP
void writeSampleList(QDataStream& stream, const std::vector<ScienceSample>& samples)
{
    stream << quint32(samples.size());
    for (const auto& s : samples) stream << quint32(s.source) << quint8(s.kind) << quint8(s.retained);
}
void readSampleList(QDataStream& stream, std::vector<ScienceSample>& samples, bool fleet)
{
    quint32 count{};
    if (!readCount(stream, count) || count > kMaximumScienceSamples) { markCorrupt(stream); return; }
    for (quint32 i = 0; i < count; ++i) {
        quint32 source{}; quint8 kind{}, retained{}; stream >> source >> kind >> retained;
        if (!source || source > 0x7fffffffU || kind > quint8(UnknownObjectKind::CompactRemnant)
            || retained > 1 || (fleet && retained) || std::any_of(samples.begin(), samples.end(),
                [&](const auto& s) { return s.source == source; })) { markCorrupt(stream); return; }
        samples.push_back({source, UnknownObjectKind(kind), retained != 0});
    }
}
void writeScienceSamples(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.unknownObjects.size());
    for (const auto& object : state.unknownObjects) out << quint32(object.id) << quint8(object.sampleRecovered);
    out << quint32(state.planets.size());
    for (const auto& p : state.planets) { out << quint32(p.id); writeSampleList(out, p.scienceSamples); }
    out << quint32(state.fleets.size() + state.pendingFleetLossContacts.size() + state.wormholeTransits.size());
    const auto write = [&](const Fleet& f) {
        out << quint32(f.id); writeSampleList(out, f.scienceSamples); writeSampleList(out, f.telemetry.scienceSamples);
        out << quint32(f.telemetryInTransit.size());
        for (const auto& packet : f.telemetryInTransit) writeSampleList(out, packet.telemetry.scienceSamples);
    };
    for (const auto& f : state.fleets) write(f);
    for (const auto& f : state.pendingFleetLossContacts) write(f.lastContact);
    for (const auto& f : state.wormholeTransits) write(f.lastContact);
    if (out.status() != QDataStream::Ok) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kScienceSamplesMagic << quint32(payload.size()); stream.writeRawData(payload.data(), payload.size());
}
void readScienceSamples(QDataStream& stream, GameState& state, bool playerTurn)
{
    quint32 magic{}, size{}, count{}; stream >> magic >> size;
    if (magic != kScienceSamplesMagic || size > 32U * 1024U * 1024U) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), int(size)) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in, count) || count != state.unknownObjects.size()) { markCorrupt(stream); return; }
    for (auto& object : state.unknownObjects) {
        quint32 id{}; quint8 recovered{}; in >> id >> recovered;
        if (id != object.id || recovered > 1) { markCorrupt(in); break; }
        object.sampleRecovered = recovered != 0;
    }
    if (!readCount(in, count) || count != state.planets.size()) { markCorrupt(stream); return; }
    for (auto& p : state.planets) {
        quint32 id{}; in >> id; if (id != p.id) { markCorrupt(in); break; }
        readSampleList(in, p.scienceSamples, false);
    }
    if (!readCount(in, count) || count != state.fleets.size() + state.pendingFleetLossContacts.size() + state.wormholeTransits.size()) {
        markCorrupt(stream); return;
    }
    const auto read = [&](Fleet& f) {
        quint32 id{}, packets{}; in >> id;
        if (id != f.id) { markCorrupt(in); return; }
        readSampleList(in, f.scienceSamples, true); readSampleList(in, f.telemetry.scienceSamples, true);
        if (!readCount(in, packets) || packets != f.telemetryInTransit.size()) { markCorrupt(in); return; }
        for (auto& packet : f.telemetryInTransit) readSampleList(in, packet.telemetry.scienceSamples, true);
    };
    for (auto& f : state.fleets) read(f);
    for (auto& f : state.pendingFleetLossContacts) read(f.lastContact);
    for (auto& f : state.wormholeTransits) read(f.lastContact);
    // Received manifests can legitimately overlap in time. Host truth cannot.
    std::vector<UnknownObjectId> physical;
    const auto check = [&](const auto& samples) {
        for (const auto& s : samples) {
            if (std::find(physical.begin(), physical.end(), s.source) != physical.end()) { markCorrupt(in); return; }
            const auto object = std::find_if(state.unknownObjects.begin(), state.unknownObjects.end(),
                [&](const auto& x) { return x.id == s.source && x.kind == s.kind && x.sampleRecovered; });
            if (object == state.unknownObjects.end()) { markCorrupt(in); return; }
            physical.push_back(s.source);
        }
    };
    if (!playerTurn) {
        for (const auto& p : state.planets) check(p.scienceSamples);
        for (const auto& f : state.fleets) {
            check(f.scienceSamples);
            if (!f.scienceSamples.empty() && fleet_cargo_used(state, f) > fleet_cargo_capacity(state, f) + 1e-6) markCorrupt(in);
        }
    }
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
