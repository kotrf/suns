// v110: dated ordinary surface stocks in received and in-flight survey evidence.
constexpr quint32 kSurfaceMineralsMagic = 0x534d494eu; // SMIN
bool surfaceMineralsValid(const SurfaceMineralObservation& value, std::uint64_t turn)
{
    const auto valid = [](double x) { return std::isfinite(x) && x >= 0 && x <= 1e15; };
    return value.observedTurn <= turn && valid(value.minerals.ironium)
        && valid(value.minerals.boranium) && valid(value.minerals.germanium);
}
void writeSurfaceMineralObservation(QDataStream& out, const auto& record, std::uint64_t turn)
{
    const auto* value = record.planetSnapshot && record.planetSnapshot->surfaceMinerals
        ? &*record.planetSnapshot->surfaceMinerals : nullptr;
    if (value && (record.level < SurveyLevel::OrbitalSurvey || value->observedTurn > record.observedTurn || !surfaceMineralsValid(*value, turn))) {
        out.setStatus(QDataStream::WriteFailed); return;
    }
    out << quint8(value != nullptr);
    if (value) { writeMinerals(out, value->minerals); out << quint64(value->observedTurn) << quint8(value->finalSurface); }
}
void readSurfaceMineralObservation(QDataStream& in, auto& record, std::uint64_t turn)
{
    quint8 present{}; in >> present;
    if (present > 1 || (present && (!record.planetSnapshot || record.level < SurveyLevel::OrbitalSurvey))) { markCorrupt(in); return; }
    if (!present) return;
    SurfaceMineralObservation value; quint64 observed{}; quint8 phase{};
    readMinerals(in, value.minerals); in >> observed >> phase;
    value.observedTurn = observed; value.finalSurface = phase;
    if (phase > 1 || value.observedTurn > record.observedTurn || !surfaceMineralsValid(value, turn)) { markCorrupt(in); return; }
    record.planetSnapshot->surfaceMinerals = value;
}
void writeSurfaceMinerals(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id) << quint32(player.surveyKnowledge.size());
        for (const auto& k : player.surveyKnowledge) { out << quint32(k.star); writeSurfaceMineralObservation(out, k, state.turn); }
        out << quint32(player.pendingSurveyReports.size());
        for (const auto& r : player.pendingSurveyReports) {
            out << quint32(r.star) << quint64(r.observedTurn); writeSurfaceMineralObservation(out, r, state.turn);
        }
    }
    if (out.status() != QDataStream::Ok || payload.size() > 32 * 1024 * 1024) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kSurfaceMineralsMagic << quint32(payload.size()); stream.writeRawData(payload.data(), payload.size());
}
void readSurfaceMinerals(QDataStream& stream, GameState& state)
{
    quint32 magic{}, size{}, count{}, id{}; stream >> magic >> size;
    if (magic != kSurfaceMineralsMagic || size > 32U * 1024U * 1024U) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), int(size)) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& player : state.players) {
        in >> id;
        if (id != player.id || !readCount(in, count) || count != player.surveyKnowledge.size()) { markCorrupt(stream); return; }
        for (auto& k : player.surveyKnowledge) {
            in >> id; if (id != k.star) { markCorrupt(in); break; }
            readSurfaceMineralObservation(in, k, state.turn);
        }
        if (!readCount(in, count) || count != player.pendingSurveyReports.size()) { markCorrupt(stream); return; }
        for (auto& r : player.pendingSurveyReports) {
            quint64 observed{}; in >> id >> observed;
            if (id != r.star || observed != r.observedTurn) { markCorrupt(in); break; }
            readSurfaceMineralObservation(in, r, state.turn);
        }
    }
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
