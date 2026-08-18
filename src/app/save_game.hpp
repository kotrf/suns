#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QString>
#include <QStringList>

#include <optional>

namespace suns {

struct SaveGameData {
    GalaxyConfig galaxyConfig;
    GameState state;
    PlayerOrders pendingOrders;
    QStringList pendingDescriptions;
    std::optional<StarId> selectedStar;
    std::optional<FleetId> selectedFleet;
    bool showSensorRanges{true};
};

[[nodiscard]] bool write_save_game_file(
    const QString& filePath,
    const SaveGameData& data,
    QString& errorMessage);

[[nodiscard]] bool read_save_game_file(
    const QString& filePath,
    SaveGameData& data,
    QString& errorMessage);

} // namespace suns
