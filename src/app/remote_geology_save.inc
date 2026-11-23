constexpr quint32 kRemoteGeologyMagic = 0x47454f31; // GEO1

void writeRemoteGeology(QDataStream& stream, const GameState& state)
{
    stream << kRemoteGeologyMagic;
    for (const auto& player : state.players) {
        stream << quint32(player.remoteGeology.size());
        for (const auto& estimate : player.remoteGeology) {
            stream << quint32(estimate.planet) << quint32(estimate.source) << quint64(estimate.observedTurn);
            writeMinerals(stream, estimate.bandLower);
        }
    }
}

void readRemoteGeology(QDataStream& stream, GameState& state)
{
    quint32 magic{};
    stream >> magic;
    if (magic != kRemoteGeologyMagic) { markCorrupt(stream); return; }
    for (auto& player : state.players) {
        quint32 count{};
        if (!readCount(stream, count) || count > state.planets.size()) { markCorrupt(stream); return; }
        PlanetId previous{};
        for (quint32 i = 0; i < count; ++i) {
            RemoteGeologyObservation estimate;
            quint64 observed{};
            stream >> estimate.planet >> estimate.source >> observed;
            estimate.observedTurn = observed;
            readMinerals(stream, estimate.bandLower);
            if (estimate.planet <= previous || !find_planet(state, estimate.planet) || !estimate.source
                || !observed || observed > state.turn || !remote_geology_bands_valid(estimate.bandLower)) {
                markCorrupt(stream); return;
            }
            previous = estimate.planet;
            player.remoteGeology.push_back(estimate);
        }
        for (const auto& report : player.pendingPlayerReports) {
            if (report.kind != PlayerReportKind::RemoteGeologyObserved) continue;
            const auto* planet = find_planet(state, report.planet);
            if (!planet || planet->star != report.star || !report.fleet || !report.observedTurn
                || report.observedTurn > state.turn || !remote_geology_bands_valid(report.deliveredMinerals)) {
                markCorrupt(stream); return;
            }
        }
    }
}
