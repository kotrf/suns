#pragma once
#include "suns/game_state.hpp"
#include <QDialog>

class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class QTextBrowser;

namespace suns {
class TechnologyCatalogDialog final : public QDialog {
    Q_OBJECT
public:
    TechnologyCatalogDialog(const GameState& received, PlayerId player, QWidget* parent = nullptr);
    // Only received own-empire knowledge affects availability. Catalog specs
    // themselves are public reference data, not physical fleets or objects.
    void refresh(const GameState& received, PlayerId player);
private:
    void filter();
    void updateDetails();
    QTreeWidget* entries_{};
    QComboBox* category_{};
    QLineEdit* search_{};
    QCheckBox* availableOnly_{};
    QLabel* count_{};
    QLabel* context_{};
    QTextBrowser* details_{};
};
} // namespace suns
