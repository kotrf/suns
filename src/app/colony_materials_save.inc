// v91: private colony mining inputs follow the unchanged EXOT/PROB/VISI bodies.
constexpr quint32 kColonyMaterialsMagic = 0x434f4c58u; // COLX
void writeColonyMaterialObservation(QDataStream& out, const std::optional<PlanetSurveySnapshot>& snapshot)
{
    const auto* colony = snapshot && snapshot->xenocrystal && snapshot->xenocrystal->colony
        ? &*snapshot->xenocrystal->colony : nullptr;
    out << quint8(colony != nullptr);
    if (colony) out << quint32(colony->owner) << quint64(colony->population) << quint32(colony->mines);
}
void readColonyMaterialObservation(QDataStream& in, std::optional<PlanetSurveySnapshot>& snapshot, PlayerId owner)
{
    quint8 present{}; in >> present;
    if (present > 1 || (present && (!snapshot || !snapshot->xenocrystal))) { markCorrupt(in); return; }
    if (!present) return;
    quint32 player{}, mines{}; quint64 population{}; in >> player >> population >> mines;
    if (!player || player != owner) { markCorrupt(in); return; }
    snapshot->xenocrystal->colony = XenocrystalObservation::Colony{player, population, mines};
}
void writeColonyMaterials(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id) << quint32(player.surveyKnowledge.size());
        for (const auto& k : player.surveyKnowledge) { out << quint32(k.star); writeColonyMaterialObservation(out, k.planetSnapshot); }
        out << quint32(player.pendingSurveyReports.size());
        for (const auto& r : player.pendingSurveyReports) {
            out << quint32(r.star) << quint64(r.observedTurn); writeColonyMaterialObservation(out, r.planetSnapshot);
        }
    }
    if (out.status() != QDataStream::Ok || payload.size() > 32 * 1024 * 1024) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kColonyMaterialsMagic << quint32(payload.size()); stream.writeRawData(payload.data(), payload.size());
}
void readColonyMaterials(QDataStream& stream, GameState& state)
{
    quint32 magic{}, size{}, count{}, id{}; stream >> magic >> size;
    if (magic != kColonyMaterialsMagic || size > 32U * 1024U * 1024U) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), int(size)) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& player : state.players) {
        in >> id;
        if (id != player.id || !readCount(in, count) || count != player.surveyKnowledge.size()) { markCorrupt(stream); return; }
        for (auto& k : player.surveyKnowledge) {
            in >> id; if (id != k.star) { markCorrupt(in); break; }
            readColonyMaterialObservation(in, k.planetSnapshot, player.id);
        }
        if (!readCount(in, count) || count != player.pendingSurveyReports.size()) { markCorrupt(stream); return; }
        for (auto& r : player.pendingSurveyReports) {
            quint64 observed{}; in >> id >> observed;
            if (id != r.star || observed != r.observedTurn) { markCorrupt(in); break; }
            readColonyMaterialObservation(in, r.planetSnapshot, player.id);
        }
    }
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
