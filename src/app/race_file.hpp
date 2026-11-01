#pragma once

#include "suns/campaign.hpp"
#include <QString>

namespace suns {

// Portable pre-game settings only. Controller and campaign/player identity
// belong to the host, not to a player's race file.
[[nodiscard]] QString race_setup_error(const EmpireSetup& setup);
bool read_race_file(const QString& path, EmpireSetup& setup, QString& error);
bool write_race_file(const QString& path, const EmpireSetup& setup, QString& error);

} // namespace suns
