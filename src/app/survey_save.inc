// Frozen planetary evidence; the pre-v83 body remains unchanged.
constexpr quint32 kSurveySnapshotMagic = 0x53555256u; // SURV

bool surveySnapshotValid(const std::optional<PlanetSurveySnapshot>& snapshot, SurveyLevel level)
{
    if (!snapshot) return true; // Older evidence requires a fresh observation.
    const auto axesValid = [](const auto& axes) {
        return !axes || (axes->temperature <= 100 && axes->gravity <= 100 && axes->radiation <= 100);
    };
    return level >= SurveyLevel::BasicScan && snapshot->habitability >= -100 && snapshot->habitability <= 100
        && bool(snapshot->environment) == (level >= SurveyLevel::OrbitalSurvey)
        && (snapshot->environment || !snapshot->naturalEnvironment)
        && axesValid(snapshot->environment) && axesValid(snapshot->naturalEnvironment);
}

void writeSurveySnapshot(QDataStream& out, const std::optional<PlanetSurveySnapshot>& snapshot,
    SurveyLevel level)
{
    if (!surveySnapshotValid(snapshot, level)) { out.setStatus(QDataStream::WriteFailed); return; }
    out << quint8(bool(snapshot));
    if (!snapshot) return;
    out << qint32(snapshot->habitability);
    for (const auto* axes : {&snapshot->environment, &snapshot->naturalEnvironment}) {
        out << quint8(bool(*axes));
        if (*axes) out << quint8((*axes)->temperature) << quint8((*axes)->gravity) << quint8((*axes)->radiation);
    }
}

void readSurveySnapshot(QDataStream& in, std::optional<PlanetSurveySnapshot>& snapshot, SurveyLevel level)
{
    quint8 present{}; in >> present;
    if (present > 1) { markCorrupt(in); return; }
    snapshot.reset();
    if (!present) return;
    PlanetSurveySnapshot value; qint32 hab{}; in >> hab; value.habitability = hab;
    for (auto* axes : {&value.environment, &value.naturalEnvironment}) {
        quint8 hasAxes{}; in >> hasAxes;
        if (hasAxes > 1) { markCorrupt(in); return; }
        if (hasAxes) {
            quint8 temperature{}, gravity{}, radiation{}; in >> temperature >> gravity >> radiation;
            *axes = PlanetEnvironment{temperature, gravity, radiation};
        }
    }
    snapshot = value;
    if (!surveySnapshotValid(snapshot, level)) markCorrupt(in);
}

void writeSurveySnapshots(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id) << quint32(player.surveyKnowledge.size());
        for (const auto& knowledge : player.surveyKnowledge) {
            out << quint32(knowledge.star);
            writeSurveySnapshot(out, knowledge.planetSnapshot, knowledge.level);
        }
        out << quint32(player.pendingSurveyReports.size());
        for (const auto& report : player.pendingSurveyReports) {
            out << quint32(report.star) << quint64(report.observedTurn);
            writeSurveySnapshot(out, report.planetSnapshot, report.level);
        }
    }
    if (out.status() != QDataStream::Ok) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kSurveySnapshotMagic << quint32(payload.size());
    stream.writeRawData(payload.constData(), payload.size());
}

void readSurveySnapshots(QDataStream& stream, GameState& state)
{
    quint32 magic{}, size{}, count{}; stream >> magic >> size;
    if (magic != kSurveySnapshotMagic || size > 32 * 1024 * 1024) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), size) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& player : state.players) {
        quint32 id{}; in >> id;
        if (id != player.id || !readCount(in, count) || count != player.surveyKnowledge.size()) { markCorrupt(stream); return; }
        for (auto& knowledge : player.surveyKnowledge) {
            quint32 star{}; in >> star;
            if (star != knowledge.star) { markCorrupt(stream); return; }
            readSurveySnapshot(in, knowledge.planetSnapshot, knowledge.level);
        }
        if (!readCount(in, count) || count != player.pendingSurveyReports.size()) { markCorrupt(stream); return; }
        for (auto& report : player.pendingSurveyReports) {
            quint32 star{}; quint64 observed{}; in >> star >> observed;
            if (star != report.star || observed != report.observedTurn) { markCorrupt(stream); return; }
            readSurveySnapshot(in, report.planetSnapshot, report.level);
        }
    }
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
