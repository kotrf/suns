#pragma once

#include "suns/campaign.hpp"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace suns {
class RaceSetupWidget final : public QWidget {
public:
    explicit RaceSetupWidget(QWidget* parent = nullptr);
    [[nodiscard]] EmpireSetup setup() const;
    void setSetup(const EmpireSetup& setup);

private:
    QLineEdit* name_{};
    QComboBox* environment_{};
    QComboBox* trait_{};
    QCheckBox* efficiency_{};
    QCheckBox* noScoops_{};
    QCheckBox* settlers_{};
    QComboBox* hulls_{};
    QComboBox* mining_{};
    QSpinBox* factories_{};
    QSpinBox* mines_{};
    QLabel* environmentSummary_{};
};
} // namespace suns
