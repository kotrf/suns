// Included inside save_game.cpp's private namespace; nested in STRA v67.
constexpr quint32 kComputerControllerMagic = 0x4354524Cu; // CTRL

void writeComputerControllers(QDataStream& stream, const GameState& state)
{
    stream << kComputerControllerMagic << quint32(state.players.size());
    for (const auto& player : state.players) {
        stream << quint32(player.id);
        writeEnum(stream, player.controller);
        stream << quint8(player.computer.initialized) << quint32(player.computer.decisions.size());
        for (const auto& record : player.computer.decisions) {
            stream << quint64(record.turn) << quint32(record.orderCount);
            writeString(stream, record.reason);
        }
    }
}

void readComputerControllers(QDataStream& stream, GameState& state)
{
    quint32 magic{}, count{};
    stream >> magic;
    if (magic != kComputerControllerMagic || !readCount(stream, count)
        || count != state.players.size()) { markCorrupt(stream); return; }
    for (auto& player : state.players) {
        quint32 id{};
        quint8 initialized{};
        stream >> id;
        if (id != player.id || !readEnum(stream, player.controller, quint8(PlayerController::Computer))) {
            markCorrupt(stream); return;
        }
        stream >> initialized;
        if (initialized > 1 || !readCount(stream, count) || count > kMaximumComputerDecisionRecords) {
            markCorrupt(stream); return;
        }
        player.computer.initialized = initialized != 0;
        for (quint32 i = 0; i < count; ++i) {
            ComputerDecision record;
            quint64 turn{};
            stream >> turn >> record.orderCount;
            record.turn = turn;
            readString(stream, record.reason);
            player.computer.decisions.push_back(std::move(record));
        }
        if (!computer_state_valid(player, state.turn)) { markCorrupt(stream); return; }
    }
}
