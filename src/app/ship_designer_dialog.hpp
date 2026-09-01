#pragma once

#include "suns/game_state.hpp"

#include <QDialog>

#include <optional>
#include <string>
#include <vector>

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QWidget;

namespace suns {

struct ShipDesignDraft {
    std::string name;
    ShipHullType hull{ShipHullType::Scout};
    std::vector<ShipComponentType> components;
    std::vector<ShipComponentPlacement> placements;
};

class ShipDesignerDialog final : public QDialog {
public:
    ShipDesignerDialog(const GameState& state, PlayerId player, QWidget* parent = nullptr);

    [[nodiscard]] ShipDesignDraft draft() const;

private:
    void rebuildSlotGrid();
    void fitComponent(ShipComponentType component, ShipSlotId target, ShipSlotId source = 0);
    void removeComponent(ShipSlotId slot);
    void selectSlot(ShipSlotId slot);
    void updatePreview();
    [[nodiscard]] ShipDesign previewDesign() const;
    [[nodiscard]] std::optional<ShipComponentType> selectedCatalogComponent() const;

    PlayerId player_{};
    bool remoteMiningAvailable_{};

    QLineEdit* nameEdit_{};
    QComboBox* hullCombo_{};
    QListWidget* componentCatalog_{};
    QWidget* slotPanel_{};
    QGridLayout* slotGrid_{};
    QLabel* fitMessage_{};
    QPushButton* fitButton_{};
    QPushButton* removeButton_{};
    QLabel* previewLabel_{};
    QPushButton* saveButton_{};
    std::vector<ShipComponentPlacement> placements_;
    ShipSlotId selectedSlot_{};
};

} // namespace suns
