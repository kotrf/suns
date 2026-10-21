// v93: scientific delivery receipts and owner-only surface evidence.
constexpr quint32 kFieldScienceCompletionMagic = 0x53434943u; // SCIC

void writeSurfaceScience(QDataStream& out, const auto& record, PlayerId viewer)
{
    const auto* samples = record.planetSnapshot && record.planetSnapshot->scienceSamples
        ? &*record.planetSnapshot->scienceSamples : nullptr;
    if (samples && (record.observedOwner != viewer || record.level < SurveyLevel::OrbitalSurvey)) {
        out.setStatus(QDataStream::WriteFailed);
        return;
    }
    out << quint8(samples != nullptr);
    if (samples) writeSampleList(out, *samples);
}

void readSurfaceScience(QDataStream& in, auto& record, PlayerId viewer)
{
    quint8 present{};
    in >> present;
    if (present > 1 || (present && (!record.planetSnapshot || record.observedOwner != viewer
            || record.level < SurveyLevel::OrbitalSurvey))) {
        markCorrupt(in);
        return;
    }
    if (present) {
        record.planetSnapshot->scienceSamples.emplace();
        readSampleList(in, *record.planetSnapshot->scienceSamples, false);
    }
}

void writeFieldScienceCompletion(QDataStream& stream, const GameState& state)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(stream.version());
    out << quint32(state.players.size());
    for (const auto& p : state.players) {
        out << quint32(p.id) << quint32(p.receivedScientificData.size());
        for (const auto id : p.receivedScientificData) out << quint64(id);
        out << quint32(p.pendingPlayerReports.size());
        for (const auto& r : p.pendingPlayerReports) {
            const auto identity = r.scientificDataId ? r.scientificDataId
                : r.kind == PlayerReportKind::ScientificData ? scientific_data_id(r, p.id) : 0;
            out << quint64(r.observedTurn) << quint64(identity);
        }
        out << quint32(p.surveyKnowledge.size());
        for (const auto& k : p.surveyKnowledge) {
            out << quint32(k.star);
            writeSurfaceScience(out, k, p.id);
        }
        out << quint32(p.pendingSurveyReports.size());
        for (const auto& r : p.pendingSurveyReports) {
            out << quint32(r.star) << quint64(r.observedTurn);
            writeSurfaceScience(out, r, p.id);
        }
    }
    if (out.status() != QDataStream::Ok || payload.size() > 32 * 1024 * 1024) {
        stream.setStatus(QDataStream::WriteFailed);
        return;
    }
    stream << kFieldScienceCompletionMagic << quint32(payload.size());
    stream.writeRawData(payload.data(), int(payload.size()));
}

void readFieldScienceCompletion(QDataStream& stream, GameState& state)
{
    quint32 magic{}, size{}, count{}, id{};
    stream >> magic >> size;
    if (magic != kFieldScienceCompletionMagic || size > 32U * 1024U * 1024U) {
        markCorrupt(stream);
        return;
    }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), int(size)) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload);
    in.setVersion(stream.version());
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& p : state.players) {
        in >> id;
        if (id != p.id || !readCount(in, count)) { markCorrupt(stream); return; }
        quint64 previous{};
        for (quint32 i = 0; i < count; ++i) {
            quint64 receipt{};
            in >> receipt;
            if (!receipt || receipt <= previous) markCorrupt(in);
            p.receivedScientificData.push_back(receipt);
            previous = receipt;
        }
        if (!readCount(in, count) || count != p.pendingPlayerReports.size()) { markCorrupt(stream); return; }
        for (auto& r : p.pendingPlayerReports) {
            quint64 observed{}, identity{};
            in >> observed >> identity;
            if (observed != r.observedTurn || (bool(identity) != (r.kind == PlayerReportKind::ScientificData)))
                markCorrupt(in);
            r.scientificDataId = identity;
        }
        if (!readCount(in, count) || count != p.surveyKnowledge.size()) { markCorrupt(stream); return; }
        for (auto& k : p.surveyKnowledge) {
            in >> id;
            if (id != k.star) markCorrupt(in);
            readSurfaceScience(in, k, p.id);
        }
        if (!readCount(in, count) || count != p.pendingSurveyReports.size()) { markCorrupt(stream); return; }
        for (auto& r : p.pendingSurveyReports) {
            quint64 observed{};
            in >> id >> observed;
            if (id != r.star || observed != r.observedTurn) markCorrupt(in);
            readSurfaceScience(in, r, p.id);
        }
    }
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}

void migratePendingScientificData(GameState& state)
{
    // Old packets have no specimen/objective identity. Preserve separate equal
    // awards once; subsequent saves retain their IDs through rerouting/replay.
    for (auto& p : state.players) {
        std::vector<std::uint64_t> ids;
        for (auto& r : p.pendingPlayerReports) if (r.kind == PlayerReportKind::ScientificData) {
            std::uint64_t ordinal = 0, id = scientific_data_id(r, p.id);
            while (std::find(ids.begin(), ids.end(), id) != ids.end()) id = scientific_data_id(r, p.id, ++ordinal);
            r.scientificDataId = id;
            ids.push_back(id);
        }
    }
}
