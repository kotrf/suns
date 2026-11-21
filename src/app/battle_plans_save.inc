// v133: current plans and owner telemetry have independent delivery dates.
constexpr quint32 kBattlePlansMagic = 0x42504c4eu; // BPLN

void writeBattlePlan(QDataStream& stream, const FleetBattlePlan& plan)
{
    if (!fleet_battle_plan_valid(plan)) {
        stream.setStatus(QDataStream::WriteFailed);
        return;
    }
    stream << quint8(plan.attackOnContact) << quint8(plan.allowOtherTargets);
    writeEnum(stream, plan.preferredTarget);
    writeEnum(stream, plan.tactic);
}

void readBattlePlan(QDataStream& stream, FleetBattlePlan& plan)
{
    quint8 attack{}, fallback{};
    stream >> attack >> fallback;
    if (attack > 1 || fallback > 1) { markCorrupt(stream); return; }
    plan.attackOnContact = attack != 0;
    plan.allowOtherTargets = fallback != 0;
    if (!readEnum(stream, plan.preferredTarget, quint8(BattleTarget::Stations))) return;
    readEnum(stream, plan.tactic, quint8(BattleTactic::Withdraw));
}

void writeBattlePlans(QDataStream& stream, const GameState& state)
{
    stream << kBattlePlansMagic
           << quint32(state.fleets.size() + state.pendingFleetLossContacts.size() + state.wormholeTransits.size());
    const auto writeFleetPlans = [&](const Fleet& fleet) {
        stream << quint32(fleet.id) << quint64(fleet.appliedBattlePlanSequence);
        writeBattlePlan(stream, fleet.battlePlan);
        writeBattlePlan(stream, fleet.telemetry.battlePlan);
        stream << quint32(fleet.telemetryInTransit.size());
        for (const auto& packet : fleet.telemetryInTransit) writeBattlePlan(stream, packet.telemetry.battlePlan);
        stream << quint32(fleet.battlePlansInTransit.size());
        for (const auto& pending : fleet.battlePlansInTransit) {
            stream << quint64(pending.deliveryTurn) << quint64(pending.sequence);
            writeBattlePlan(stream, pending.plan);
        }
    };
    for (const auto& fleet : state.fleets) writeFleetPlans(fleet);
    for (const auto& loss : state.pendingFleetLossContacts) writeFleetPlans(loss.lastContact);
    for (const auto& transit : state.wormholeTransits) writeFleetPlans(transit.lastContact);
}

void readBattlePlans(QDataStream& stream, GameState& state, bool playerTurn)
{
    quint32 magic{}, count{};
    stream >> magic;
    if (magic != kBattlePlansMagic || !readCount(stream, count)
        || count != state.fleets.size() + state.pendingFleetLossContacts.size() + state.wormholeTransits.size()) {
        markCorrupt(stream);
        return;
    }
    const auto readFleetPlans = [&](Fleet& fleet) {
        quint32 id{};
        quint64 applied{};
        stream >> id >> applied;
        if (id != fleet.id || (playerTurn && applied)) { markCorrupt(stream); return; }
        fleet.appliedBattlePlanSequence = applied;
        readBattlePlan(stream, fleet.battlePlan);
        readBattlePlan(stream, fleet.telemetry.battlePlan);
        if (playerTurn && fleet.battlePlan != fleet.telemetry.battlePlan) { markCorrupt(stream); return; }
        if (!readCount(stream, count) || count != fleet.telemetryInTransit.size()) {
            markCorrupt(stream);
            return;
        }
        for (auto& packet : fleet.telemetryInTransit) readBattlePlan(stream, packet.telemetry.battlePlan);
        if (!readCount(stream, count) || (count && playerTurn)) {
            markCorrupt(stream);
            return;
        }
        std::set<std::uint64_t> sequences;
        for (quint32 index = 0; index < count; ++index) {
            quint64 delivery{}, sequence{};
            stream >> delivery >> sequence;
            if (delivery <= state.turn || !sequence || !sequences.insert(sequence).second) {
                markCorrupt(stream);
                return;
            }
            PendingBattlePlan pending{{}, delivery, sequence};
            readBattlePlan(stream, pending.plan);
            fleet.battlePlansInTransit.push_back(pending);
        }
    };
    for (auto& fleet : state.fleets) readFleetPlans(fleet);
    for (auto& loss : state.pendingFleetLossContacts) readFleetPlans(loss.lastContact);
    for (auto& transit : state.wormholeTransits) readFleetPlans(transit.lastContact);
}
