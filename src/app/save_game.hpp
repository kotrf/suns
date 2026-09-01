#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace suns {

struct SaveGameData {
    std::uint64_t campaignId{};
    std::uint64_t turnToken{};
    GalaxyConfig galaxyConfig;
    GameState state;
    PlayerOrders pendingOrders;
    QStringList pendingDescriptions;
    std::optional<StarId> selectedStar;
    std::optional<FleetId> selectedFleet;
    bool showSensorRanges{true};
};

// Transport-neutral payload for PBEM today and a host/server transport later.
// It deliberately contains orders only, never the authoritative GameState.
struct TurnOrderFileData {
    std::uint64_t campaignId{};
    std::uint64_t turn{};
    std::uint64_t turnToken{};
    PlayerOrders orders;
    QStringList descriptions;
};

[[nodiscard]] bool write_save_game_file(
    const QString& filePath,
    const SaveGameData& data,
    QString& errorMessage);

[[nodiscard]] bool read_save_game_file(
    const QString& filePath,
    SaveGameData& data,
    QString& errorMessage);

[[nodiscard]] bool write_turn_order_file(
    const QString& filePath,
    const TurnOrderFileData& data,
    QString& errorMessage);

[[nodiscard]] bool read_turn_order_file(
    const QString& filePath,
    TurnOrderFileData& data,
    QString& errorMessage);

} // namespace suns
