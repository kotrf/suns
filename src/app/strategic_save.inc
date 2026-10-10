// Included inside save_game.cpp's private namespace. The bounded trailing
// extension preserves the old record layout for v12-61 migrations.
constexpr quint32 kStrategicExtensionMagic = 0x53545241u; // STRA

void writeElectronics(QDataStream& stream, const ElectronicsProgram& value)
{ writeEnum(stream, value.mode); stream << quint64(value.resumeTurn) << quint8(value.deception); }
void readElectronics(QDataStream& stream, ElectronicsProgram& value)
{
    quint64 resume{}; quint8 deception{};
    if (!readEnum(stream, value.mode, quint8(EmissionMode::RadioSilence))) return;
    stream >> resume >> deception; value.resumeTurn = resume; value.deception = deception != 0;
    if ((value.mode == EmissionMode::RadioSilence) != (resume != 0) || deception > 1
        || (deception && value.mode != EmissionMode::Standard)) markCorrupt(stream);
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
}
void readStrategicIntel(QDataStream& stream, StrategicIntel& i)
{
    quint32 id{}, owner{}; quint64 observed{}, delivery{}; quint8 kind{};
    stream >> id; i.id = id;
    if (!readEnum(stream, i.type, quint8(StrategicObjectKind::Emission))) return;
    readPosition(stream, i.position);
    stream >> i.radius >> owner >> observed >> delivery >> i.quantity >> kind;
    i.owner = owner; i.observedTurn = observed; i.deliveryTurn = delivery; i.kind = kind;
    if (!std::isfinite(i.radius) || i.radius < 0 || i.radius > 10000 || !std::isfinite(i.quantity)
        || i.quantity < 0 || kind > 2 || delivery < observed
        || (i.type == StrategicObjectKind::Emission && owner != 0)) markCorrupt(stream);
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
            if ((key >> 32) < 1 || (key >> 32) > 4 || std::any_of(p.fieldScience.begin(), p.fieldScience.end(),
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
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
