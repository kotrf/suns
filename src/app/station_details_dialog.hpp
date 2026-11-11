#pragma once

#include "suns/game_state.hpp"
#include <QDialog>
#include <QIcon>

namespace suns {

QIcon stationIcon(OrbitalStationHullType hull);

// A read-only snapshot. Callers must pass a station from make_player_view,
// never a physical host station or an unfinished production loadout.
class StationDetailsDialog final : public QDialog {
public:
    StationDetailsDialog(const OrbitalStation& station, std::uint64_t turn, QWidget* parent = nullptr,
        const QString& radiationText = {});
};

} // namespace suns
