// A trailing extension keeps the historical STRA record layout unchanged.
constexpr quint32 kWreckMineralsMagic = 0x53435250u; // SCRP
void writeWreckMinerals(QDataStream& stream, const GameState& state)
{
    stream << kWreckMineralsMagic << quint32(state.wrecks.size());
    for (const auto& wreck : state.wrecks) { stream << quint32(wreck.id); writeMinerals(stream, wreck.minerals); }
}
void readWreckMinerals(QDataStream& stream, GameState& state)
{
    quint32 magic{}, count{}; stream >> magic;
    if (magic != kWreckMineralsMagic || !readCount(stream, count) || count != state.wrecks.size()) {
        markCorrupt(stream); return;
    }
    for (auto& wreck : state.wrecks) {
        quint32 id{}; stream >> id;
        if (id != wreck.id) { markCorrupt(stream); return; }
        readMinerals(stream, wreck.minerals);
    }
}
