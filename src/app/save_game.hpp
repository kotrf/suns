#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <map>
#include <vector>

namespace suns {

enum class SessionMode : std::uint8_t { Solo, Host, PlayerTurn };

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
    std::vector<GameEvent> strategicMessages;
    std::vector<std::uint64_t> readStrategicMessageIds;
    SessionMode mode{SessionMode::Solo};
    std::map<PlayerId, std::uint64_t> playerTokens;
    std::vector<PlayerOrders> inbox;
    bool migratedPopulation{}; // Read-time information; not persisted.
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

// Returns a reason without modifying the inbox. Reimport replaces a player's
// submission only after the caller explicitly chooses to do so.
[[nodiscard]] QString validate_turn_submission(
    const SaveGameData& host, const TurnOrderFileData& packet);
[[nodiscard]] SaveGameData make_player_turn(const SaveGameData& host, PlayerId player);

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
