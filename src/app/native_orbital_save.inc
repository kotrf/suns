// v115: native station residents and their received/pending owner evidence.
// v116: independent delivered station-destruction identities/times.
// v117: received asset observation ranges for comparable History totals.
// v118: native mining manifests are received events, not physical History output.
constexpr quint32 kNativeOrbitalMagic = 0x4e4f5242u; // NORB
void writeNativePopulation(QDataStream& out, const NativeOrbitalPopulation& value)
{
    out << quint32(value.station) << quint64(value.population);
}
void readNativePopulation(QDataStream& in, NativeOrbitalPopulation& value)
{
    quint32 station{}; quint64 population{};
    in >> station >> population;
    value = {station, population};
    if ((!station && population) || population > native_orbital_capacity(OrbitalStationHullType::Fortress)) markCorrupt(in);
}
void writeNativeEvidence(QDataStream& out, const auto& record)
{
    writeNativePopulation(out, record.planetSnapshot && record.planetSnapshot->habitat
        ? record.planetSnapshot->habitat->nativeOrbit : NativeOrbitalPopulation{});
}
void readNativeEvidence(QDataStream& in, auto& record, const Player& player)
{
    NativeOrbitalPopulation native;
    readNativePopulation(in, native);
    if (player.race.primaryTrait == PrimaryRaceTrait::HabitatCivilization
        && record.observedOwner == player.id && record.planetSnapshot && record.planetSnapshot->habitat
        && (record.planetSnapshot->habitat->surfacePopulation || record.planetSnapshot->habitat->habitat.population)) markCorrupt(in);
    if (native == NativeOrbitalPopulation{}) return;
    if (player.race.primaryTrait != PrimaryRaceTrait::HabitatCivilization
        || record.observedOwner != player.id || !record.planetSnapshot || !record.planetSnapshot->habitat) {
        markCorrupt(in); return;
    }
    auto& evidence = *record.planetSnapshot->habitat;
    if (evidence.surfacePopulation || evidence.habitat.population) markCorrupt(in);
    if (evidence.station) {
        if (evidence.station->id != native.station || native.population > native_orbital_capacity(evidence.station->hull)) markCorrupt(in);
    } else if (native.population || evidence.stationEvidenceTime % 3 != 1) {
        // A delivered destruction report retains the station identity as a
        // tombstone, so older survey packets cannot resurrect its residents.
        markCorrupt(in);
    }
    evidence.nativeOrbit = native;
}
void writeNativeOrbit(QDataStream& stream, const GameState& state)
{
    QByteArray payload; QDataStream out(&payload, QIODevice::WriteOnly); out.setVersion(stream.version());
    out << quint32(state.planets.size());
    for (const auto& planet : state.planets) { out << quint32(planet.id); writeNativePopulation(out, planet.nativeOrbit); }
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id) << quint32(player.surveyKnowledge.size());
        for (const auto& record : player.surveyKnowledge) { out << quint32(record.star); writeNativeEvidence(out, record); }
        out << quint32(player.pendingSurveyReports.size());
        for (const auto& record : player.pendingSurveyReports) {
            out << quint32(record.star) << quint64(record.observedTurn); writeNativeEvidence(out, record);
        }
    }
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id) << quint32(player.stationDestructions.size());
        for (const auto& fact : player.stationDestructions)
            out << quint32(fact.station) << quint32(fact.planet) << quint64(fact.observedTurn);
    }
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id) << quint32(player.history.size());
        for (const auto& h : player.history)
            out << quint64(h.turn) << quint8(h.assetTotalsSynchronized)
                << quint64(h.oldestAssetObservation) << quint64(h.newestAssetObservation);
    }
    if (out.status() != QDataStream::Ok || payload.size() > 32*1024*1024) { stream.setStatus(QDataStream::WriteFailed); return; }
    stream << kNativeOrbitalMagic << quint32(payload.size());
    stream.writeRawData(payload.data(), int(payload.size()));
}
void readNativeOrbit(QDataStream& stream, GameState& state)
{
    quint32 magic{}, size{}, count{}, id{}; stream >> magic >> size;
    if (magic != kNativeOrbitalMagic || size > 32U*1024U*1024U) { markCorrupt(stream); return; }
    QByteArray payload(size, '\0');
    if (stream.readRawData(payload.data(), int(size)) != int(size)) { markCorrupt(stream); return; }
    QDataStream in(payload); in.setVersion(stream.version());
    if (!readCount(in, count) || count != state.planets.size()) { markCorrupt(stream); return; }
    for (auto& planet : state.planets) {
        in >> id; if (id != planet.id) markCorrupt(in);
        readNativePopulation(in, planet.nativeOrbit);
        if (native_orbital_race(state, planet.owner) && (planet.population || planet.habitat != OrbitalHabitat{})) markCorrupt(in);
        if (planet.nativeOrbit.station) {
            const auto* station = native_orbital_station(state, planet);
            if (!station || planet.nativeOrbit.population > native_orbital_capacity(station->hull)) markCorrupt(in);
        }
    }
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& player : state.players) {
        in >> id;
        if (id != player.id || !readCount(in, count) || count != player.surveyKnowledge.size()) { markCorrupt(stream); return; }
        for (auto& record : player.surveyKnowledge) {
            in >> id; if (id != record.star) markCorrupt(in); readNativeEvidence(in, record, player);
        }
        if (!readCount(in, count) || count != player.pendingSurveyReports.size()) { markCorrupt(stream); return; }
        for (auto& record : player.pendingSurveyReports) {
            quint64 observed{}; in >> id >> observed;
            if (id != record.star || observed != record.observedTurn) markCorrupt(in);
            readNativeEvidence(in, record, player);
        }
    }
    if (gReadSaveFormatVersion >= 116) {
        if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
        for (auto& player : state.players) {
            in >> id;
            if (id != player.id || !readCount(in, count)) { markCorrupt(stream); return; }
            OrbitalStationId previous{};
            for (quint32 index = 0; index < count; ++index) {
                quint32 station{}, planet{}; quint64 observed{};
                in >> station >> planet >> observed;
                if (!station || station <= previous || observed > state.turn) { markCorrupt(stream); return; }
                player.stationDestructions.push_back({station, planet, observed});
                previous = station;
            }
        }
    } else {
        // Preserve already delivered v115 native tombstones when upgrading.
        // Pending reports are not received facts and never seed this ledger.
        for (auto& player : state.players) for (const auto& record : player.surveyKnowledge) {
            if (record.observedOwner != player.id || !record.planetSnapshot || !record.planetSnapshot->habitat) continue;
            const auto& h = *record.planetSnapshot->habitat;
            if (!h.nativeOrbit.station || h.nativeOrbit.population || h.station || h.stationEvidenceTime % 3 != 1) continue;
            const auto* planet = find_planet_at_star(state, record.star);
            remember_station_destruction(player, {h.nativeOrbit.station, planet ? planet->id : 0, h.stationEvidenceTime/3});
        }
    }
    if (gReadSaveFormatVersion >= 117) {
        if (!readCount(in, count) || count != state.players.size()) { markCorrupt(stream); return; }
        for (auto& player : state.players) {
            in >> id;
            if (id != player.id || !readCount(in, count) || count != player.history.size()) { markCorrupt(stream); return; }
            for (auto& h : player.history) {
                quint64 turn{}, oldest{}, newest{}; quint8 synchronized{};
                in >> turn >> synchronized >> oldest >> newest;
                if (turn != h.turn || synchronized > 1 || oldest > newest || newest > h.turn
                    || (synchronized && oldest != newest)) { markCorrupt(stream); return; }
                h.assetTotalsSynchronized = synchronized;
                h.oldestAssetObservation = oldest; h.newestAssetObservation = newest;
            }
        }
    } else {
        // Older native rows have no provenance for their combined cargo sum.
        // Do not retroactively certify an asynchronous subtotal as exact.
        for (auto& player : state.players) if (player.race.primaryTrait == PrimaryRaceTrait::HabitatCivilization)
            for (auto& h : player.history) h.assetTotalsSynchronized = false;
    }
    if (in.status() != QDataStream::Ok || !in.atEnd()) { markCorrupt(stream); return; }
    if (gReadSaveFormatVersion < 118)
        for (auto& player : state.players) if (player.race.primaryTrait == PrimaryRaceTrait::HabitatCivilization)
            for (auto& h : player.history) h.remoteExtractionRecorded = false;
    for (auto& player : state.players) for (auto& record : player.surveyKnowledge)
        if (record.observedOwner == player.id)
            merge_received_habitat_station(record.planetSnapshot, std::nullopt, player);
}
