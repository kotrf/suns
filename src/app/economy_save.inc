// v111: economy settings and advanced subsets. Legacy buildings remain basic.
constexpr quint32 kEconomyMagic = 0x45434f4eu; // ECON

void writeAdvancedIndustry(QDataStream& out, const auto& value)
{
    if (value.advancedFactories > value.industry || value.advancedMines > value.mines) {
        out.setStatus(QDataStream::WriteFailed); return;
    }
    out << quint32(value.advancedFactories) << quint32(value.advancedMines);
}
void readAdvancedIndustry(QDataStream& in, auto& value)
{
    in >> value.advancedFactories >> value.advancedMines;
    if (value.advancedFactories > value.industry || value.advancedMines > value.mines) markCorrupt(in);
}
void writeEconomyEvidence(QDataStream& out, const auto& record)
{
    const auto* h = record.planetSnapshot && record.planetSnapshot->habitat ? &*record.planetSnapshot->habitat : nullptr;
    out << quint8(bool(h));
    if (h) writeAdvancedIndustry(out, *h);
}
void readEconomyEvidence(QDataStream& in, auto& record)
{
    quint8 present{}; in >> present;
    auto* h = record.planetSnapshot && record.planetSnapshot->habitat ? &*record.planetSnapshot->habitat : nullptr;
    if (present > 1 || bool(present) != bool(h)) { markCorrupt(in); return; }
    if (h) readAdvancedIndustry(in, *h);
}
void writeEconomy(QDataStream& out, const GameState& state)
{
    out << kEconomyMagic << quint8(state.slowResearch) << quint32(state.planets.size());
    for (const auto& planet : state.planets) {
        out << quint32(planet.id); writeAdvancedIndustry(out, planet);
    }
    out << quint32(state.players.size());
    for (const auto& player : state.players) {
        out << quint32(player.id);
        for (auto cost : player.race.researchCosts) {
            if (cost > ResearchCost::Expensive) { out.setStatus(QDataStream::WriteFailed); return; }
            writeEnum(out, cost);
        }
        out << quint32(player.surveyKnowledge.size());
        for (const auto& k : player.surveyKnowledge) { out << quint32(k.star); writeEconomyEvidence(out, k); }
        out << quint32(player.pendingSurveyReports.size());
        for (const auto& r : player.pendingSurveyReports) {
            out << quint32(r.star) << quint64(r.observedTurn); writeEconomyEvidence(out, r);
        }
    }
}
void readEconomy(QDataStream& in, GameState& state)
{
    quint32 magic{}, count{}, id{}; quint8 slow{};
    in >> magic >> slow;
    if (magic != kEconomyMagic || slow > 1 || !readCount(in, count) || count != state.planets.size()) {
        markCorrupt(in); return;
    }
    state.slowResearch = slow;
    for (auto& planet : state.planets) {
        in >> id; if (id != planet.id) { markCorrupt(in); return; }
        readAdvancedIndustry(in, planet);
    }
    if (!readCount(in, count) || count != state.players.size()) { markCorrupt(in); return; }
    for (auto& player : state.players) {
        in >> id; if (id != player.id) { markCorrupt(in); return; }
        for (auto& cost : player.race.researchCosts) readEnum(in, cost, quint8(ResearchCost::Expensive));
        if (!readCount(in, count) || count != player.surveyKnowledge.size()) { markCorrupt(in); return; }
        for (auto& k : player.surveyKnowledge) {
            in >> id; if (id != k.star) { markCorrupt(in); return; } readEconomyEvidence(in, k);
        }
        if (!readCount(in, count) || count != player.pendingSurveyReports.size()) { markCorrupt(in); return; }
        for (auto& r : player.pendingSurveyReports) {
            quint64 observed{}; in >> id >> observed;
            if (id != r.star || observed != r.observedTurn) { markCorrupt(in); return; } readEconomyEvidence(in, r);
        }
    }
}

// v112: configurable surface growth. Counts, identity and range are checked.
void writePopulationRules(QDataStream& out, const GameState& state)
{
    out << quint32(0x504f5052) << quint32(state.players.size()); // POPR
    for (const auto& player : state.players) {
        if (player.race.populationGrowthRate < 1 || player.race.populationGrowthRate > 20) {
            out.setStatus(QDataStream::WriteFailed); return;
        }
        out << quint32(player.id) << quint8(player.race.populationGrowthRate);
    }
}
void readPopulationRules(QDataStream& in, GameState& state)
{
    quint32 magic{}, count{}, id{}; quint8 rate{};
    in >> magic;
    if (magic != 0x504f5052 || !readCount(in, count) || count != state.players.size()) {
        markCorrupt(in); return;
    }
    for (auto& player : state.players) {
        in >> id >> rate;
        if (id != player.id || rate < 1 || rate > 20) { markCorrupt(in); return; }
        player.race.populationGrowthRate = rate;
    }
}
