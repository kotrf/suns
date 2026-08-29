#pragma once

#include "suns/game_state.hpp"

#include <QDialog>

#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace suns {

struct ShipDesignDraft {
    std::string name;
    ShipHullType hull{ShipHullType::Scout};
    std::vector<ShipComponentType> components;
};

class ShipDesignerDialog final : public QDialog {
public:
    ShipDesignerDialog(const GameState& state, PlayerId player, QWidget* parent = nullptr);

    [[nodiscard]] ShipDesignDraft draft() const;

private:
    void updatePreview();
    [[nodiscard]] ShipDesign previewDesign() const;

    PlayerId player_{};
    bool remoteMiningAvailable_{};

    QLineEdit* nameEdit_{};
    QComboBox* hullCombo_{};
    QComboBox* engineCombo_{};
    QSpinBox* scannerCount_{};
    QSpinBox* compactScannerCount_{};
    QSpinBox* penetratingScannerCount_{};
    QSpinBox* remoteMiningModuleCount_{};
    QSpinBox* colonyModuleCount_{};
    QSpinBox* fuelTankCount_{};
    QSpinBox* cargoPodCount_{};
    QSpinBox* antimatterCount_{};
    QLabel* previewLabel_{};
    QPushButton* saveButton_{};
};

} // namespace suns
