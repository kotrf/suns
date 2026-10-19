// v88: keep physical notebooks, dated telemetry and command payloads separate.
void writeProbeProgram(QDataStream& out, const ProbeProgram& p)
{
    writePosition(out, p.center); out << p.radius; writeEnum(out, p.depth); writeEnum(out, p.flight);
    out << quint8(p.maxWarp) << p.fuelReserve << quint32(p.maxSystems) << quint32(p.maxTurns) << quint8(p.returnHome);
}
void readProbeProgram(QDataStream& in, ProbeProgram& p)
{
    readPosition(in, p.center); in >> p.radius;
    if (!readEnum(in, p.depth, quint8(SurveyLevel::DeepSurvey)) || !readEnum(in, p.flight, quint8(ProbeFlightMode::Economical))) return;
    quint8 warp{}, home{}; in >> warp >> p.fuelReserve >> p.maxSystems >> p.maxTurns >> home;
    p.maxWarp = warp; p.returnHome = home != 0;
    if (home > 1 || !probe_program_valid(p)) markCorrupt(in);
}
void writeProbeKnowledge(QDataStream& out, const std::vector<ProbeSurvey>& knowledge)
{
    out << quint32(knowledge.size());
    for (const auto& k : knowledge) { out << quint32(k.star); writeEnum(out, k.level); out << quint64(k.observedTurn); }
}
void readProbeKnowledge(QDataStream& in, std::vector<ProbeSurvey>& knowledge, const GameState& state, std::uint64_t latest)
{
    quint32 count{}; if (!readCount(in, count) || count > state.stars.size()) { markCorrupt(in); return; }
    std::vector<StarId> ids;
    for (quint32 i = 0; i < count; ++i) {
        ProbeSurvey k; quint64 turn{}; in >> k.star;
        if (!readEnum(in, k.level, quint8(SurveyLevel::DeepSurvey))) return;
        in >> turn; k.observedTurn = turn;
        if (!find_star(state, k.star) || turn > latest) { markCorrupt(in); return; }
        ids.push_back(k.star); knowledge.push_back(k);
    }
    std::sort(ids.begin(), ids.end()); if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) markCorrupt(in);
}
void writeProbeMission(QDataStream& out, const ProbeMission& m)
{
    out << quint8(m.configured) << quint8(m.enabled); writeProbeProgram(out, m.program); writeEnum(out, m.status);
    writePosition(out, m.origin); out << quint64(m.startedTurn) << quint32(m.target) << quint32(m.completed.size());
    for (const auto star : m.completed) out << quint32(star);
    writeProbeKnowledge(out, m.surveys);
}
void readProbeMission(QDataStream& in, ProbeMission& m, const GameState& s, std::uint64_t latest, bool telemetry)
{
    quint8 configured{}, enabled{}; in >> configured >> enabled; m.configured = configured; m.enabled = enabled;
    readProbeProgram(in, m.program); if (!readEnum(in, m.status, quint8(ProbeStatus::Stopped))) return;
    readPosition(in, m.origin); quint64 start{}; quint32 count{}; in >> start >> m.target; m.startedTurn = start;
    if (configured > 1 || enabled > 1 || (enabled && !configured) || start > latest
        || (configured && !start) || (m.target && !find_star(s, m.target))
        || !readCount(in, count) || count > 1000 || count > s.stars.size()) { markCorrupt(in); return; }
    for (quint32 i = 0; i < count; ++i) { quint32 star{}; in >> star; if (!find_star(s, star)) markCorrupt(in); m.completed.push_back(star); }
    auto ids = m.completed; std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) markCorrupt(in);
    readProbeKnowledge(in, m.surveys, s, latest);
    if ((telemetry && !m.surveys.empty()) || (!configured && (enabled || start || m.target || !m.completed.empty() || m.status != ProbeStatus::Idle))) markCorrupt(in);
}
void writeSurveyProbes(QDataStream& stream, const GameState& s)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(s.fleets.size() + s.pendingFleetLossContacts.size() + s.wormholeTransits.size());
    const auto write = [&](const Fleet& f) {
        out << quint32(f.id); writeProbeMission(out, f.probe); writeProbeMission(out, f.telemetry.probe);
        out << quint32(f.telemetryInTransit.size());
        for (const auto& packet : f.telemetryInTransit) writeProbeMission(out, packet.telemetry.probe);
        out << quint32(f.pendingCommands.size());
        for (const auto& c : f.pendingCommands) {
            out << quint8(c.probe.has_value());
            if (c.probe) { writeEnum(out, c.probe->action); writeProbeProgram(out, c.probe->program); writeProbeKnowledge(out, c.probe->knowledge); }
        }
    };
    for (const auto& f : s.fleets) write(f);
    for (const auto& f : s.pendingFleetLossContacts) write(f.lastContact);
    for (const auto& f : s.wormholeTransits) write(f.lastContact);
    if (out.status() != QDataStream::Ok || payload.size() > 32 * 1024 * 1024) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << quint32{0x50524f42} << quint32(payload.size()); stream.writeRawData(payload.data(), payload.size());
}
void readSurveyProbes(QDataStream& stream, GameState& s, bool playerTurn)
{
    quint32 magic{}, size{}, count{}; stream >> magic >> size;
    if (magic != 0x50524f42 || size > 32U * 1024U * 1024U) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0'); if (stream.readRawData(payload.data(), int(size)) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in, count) || count != s.fleets.size() + s.pendingFleetLossContacts.size() + s.wormholeTransits.size()) { markCorrupt(stream); return; }
    const auto read = [&](Fleet& f) {
        quint32 id{}, packets{}; in >> id;
        if (id != f.id) { markCorrupt(in); return; }
        readProbeMission(in, f.probe, s, s.turn, playerTurn); readProbeMission(in, f.telemetry.probe, s, s.turn, true);
        if (!readCount(in, packets) || packets != f.telemetryInTransit.size()) { markCorrupt(in); return; }
        for (auto& packet : f.telemetryInTransit) readProbeMission(in, packet.telemetry.probe, s, packet.telemetry.observedTurn, true);
        if (!readCount(in, packets) || packets != f.pendingCommands.size()) { markCorrupt(in); return; }
        for (auto& c : f.pendingCommands) {
            quint8 present{}; in >> present; if (present > 1) { markCorrupt(in); return; }
            if (present) {
                ProbeCommand command; if (!readEnum(in, command.action, quint8(ProbeAction::UpdateKnowledge))) return;
                readProbeProgram(in, command.program); readProbeKnowledge(in, command.knowledge, s, c.issuedTurn);
                if (c.task || c.electronics || c.mobileBase || playerTurn) markCorrupt(in);
                c.probe = std::move(command);
            }
        }
    };
    for (auto& f : s.fleets) read(f);
    for (auto& f : s.pendingFleetLossContacts) read(f.lastContact);
    for (auto& f : s.wormholeTransits) read(f.lastContact);
    if (in.status() != QDataStream::Ok || !in.atEnd()) markCorrupt(stream);
}
