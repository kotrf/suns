#pragma once

#include "suns/game_state.hpp"
#include <QWidget>

class QComboBox;
class QLabel;
class QSpinBox;

namespace suns {

// Presets are expanded into the existing explicit save-game configuration.
// A saved custom map stays custom until the player chooses a new preset.
class GalaxySetupWidget final : public QWidget {
public:
    explicit GalaxySetupWidget(const GalaxyConfig& current, QWidget* parent = nullptr);
    [[nodiscard]] GalaxyConfig config(std::uint64_t seed) const;
private:
    void refresh();
    GalaxyConfig original_;
    QComboBox* size_{};
    QComboBox* density_{};
    QSpinBox* systems_{};
    QLabel* dimensions_{};
};

} // namespace suns
