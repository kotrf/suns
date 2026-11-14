#pragma once

#include "suns/game_state.hpp"

#include <QDialog>

#include <optional>
#include <functional>
#include <set>
#include <string>
#include <vector>

class QComboBox;
class QCheckBox;
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
    ~ShipDesignerDialog() override;

    [[nodiscard]] ShipDesignDraft draft() const;
    // Refresh access from received own-empire knowledge without replacing the draft.
    void refreshAvailability(const GameState& received);
    // Recheck against live planning state before accepting a nonmodal draft.
    void setSaveValidator(std::function<QString(const ShipDesignDraft&)> validator);
    void accept() override;
    void showDesign(ShipDesignId id, bool readOnly = true);
    void setDeleteHandler(std::function<QString(ShipDesignId)> handler);

private:
    void suggestName(const std::string& base);
    void rebuildSlotGrid();
    void refreshSlotGrid();
    void fitComponent(ShipComponentType component, ShipSlotId target, ShipSlotId source = 0);
    void removeComponent(ShipSlotId slot);
    void selectSlot(ShipSlotId slot);
    void focusAdjacentSlot(ShipSlotId slot, int rowDirection, int columnDirection);
    void updatePreview();
    void updateComponentDetails();
    void filterCatalog(bool preserveScroll = false);
    [[nodiscard]] ShipDesign previewDesign() const;
    [[nodiscard]] std::optional<ShipComponentType> selectedCatalogComponent() const;

    std::function<QString(ShipDesignId)> deleteHandler_;
    QPushButton* deleteButton_{};
    PlayerId player_{};
    ShipHullType initialHull_{ShipHullType::Scout};
    ShipComponentType initialScanner_{ShipComponentType::LongRangeScanner};
    ShipComponentType initialEngine_{ShipComponentType::FusionDrive};

    QLineEdit* nameEdit_{};
    QComboBox* hullCombo_{};
    QComboBox* templateCombo_{};
    std::vector<ShipDesign> templates_;
    std::vector<ShipDesign> viewDesigns_;
    QListWidget* componentCatalog_{};
    QLineEdit* componentSearch_{};
    QComboBox* componentCategory_{};
    QCheckBox* componentFitsHull_{};
    std::set<ShipComponentType> availableComponents_;
    QLabel* componentDetails_{};
    QLabel* hullPortrait_{};
    QWidget* slotPanel_{};
    QGridLayout* slotGrid_{};
    QLabel* fitMessage_{};
    QPushButton* fitButton_{};
    QPushButton* removeButton_{};
    QLabel* previewLabel_{};
    QPushButton* saveButton_{};
    std::vector<ShipComponentPlacement> placements_;
    std::vector<ShipSlotSpec> displaySlots_;
    ShipSlotId selectedSlot_{};
    std::function<QString(const ShipDesignDraft&)> saveValidator_;
};

} // namespace suns
