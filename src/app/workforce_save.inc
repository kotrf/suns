// Racial operation rates only; working counts are derived from population.
constexpr quint32 kWorkforceMagic = 0x5746524bu; // WFRK

bool workforceRatesValid(const RaceProfile& race)
{
    return race.factoriesPerTenThousand >= kMinWorkforceRate && race.factoriesPerTenThousand <= kMaxWorkforceRate
        && race.minesPerTenThousand >= kMinWorkforceRate && race.minesPerTenThousand <= kMaxWorkforceRate;
}

void writeWorkforce(QDataStream& stream, const GameState& state)
{
    stream << kWorkforceMagic << quint32(state.players.size());
    for (const auto& player : state.players) {
        if (!workforceRatesValid(player.race)) { stream.setStatus(QDataStream::WriteFailed); return; }
        stream << quint32(player.id) << quint8(player.race.factoriesPerTenThousand) << quint8(player.race.minesPerTenThousand);
    }
}

void readWorkforce(QDataStream& stream, GameState& state)
{
    quint32 magic{}, count{};
    stream >> magic;
    if (magic != kWorkforceMagic || !readCount(stream, count) || count != state.players.size()) {
        markCorrupt(stream); return;
    }
    for (auto& player : state.players) {
        quint32 id{};
        quint8 factories{}, mines{};
        stream >> id >> factories >> mines;
        player.race.factoriesPerTenThousand = factories;
        player.race.minesPerTenThousand = mines;
        if (id != player.id || !workforceRatesValid(player.race)) { markCorrupt(stream); return; }
    }
}
