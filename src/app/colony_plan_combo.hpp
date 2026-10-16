#pragma once

#include "suns/game_state.hpp"
#include <QComboBox>
#include <QSignalBlocker>
#include <utility>

namespace suns {

// Each entry owns its value, including a carried copy of a renamed/deleted plan.
class ColonyPlanCombo final : public QComboBox {
public:
    explicit ColonyPlanCombo(QWidget* parent = nullptr) : QComboBox(parent)
    {
        setMinimumContentsLength(14);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setPlan(std::nullopt);
        setToolTip("A copy travels with this Colonize order. Later template changes do not alter it. None leaves an empty queue and no automatic radar.");
    }

    [[nodiscard]] std::optional<ProductionTemplate> plan() const
    {
        const auto index = currentIndex();
        if (index <= 0 || std::size_t(index) > entries_.size()) return std::nullopt;
        return entries_[std::size_t(index) - 1];
    }

    void setTemplates(std::vector<ProductionTemplate> templates)
    {
        if (templates_ == templates) return;
        const auto selected = plan();
        templates_ = std::move(templates);
        setPlan(selected);
    }

    void setPlan(const std::optional<ProductionTemplate>& value)
    {
        const QSignalBlocker blocked(this);
        clear();
        addItem("None — empty queue");
        entries_ = templates_;
        int selected = 0;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            addItem(QString::fromStdString(entries_[i].name));
            if (value && entries_[i] == *value) selected = int(i) + 1;
        }
        if (value && selected == 0) {
            entries_.push_back(*value);
            addItem(QString::fromStdString(value->name) + " (order copy)");
            selected = count() - 1;
        }
        setCurrentIndex(selected);
    }

private:
    std::vector<ProductionTemplate> templates_;
    std::vector<ProductionTemplate> entries_;
};

} // namespace suns
