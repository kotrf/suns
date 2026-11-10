#pragma once

#include "suns/game_state.hpp"
#include <QComboBox>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <algorithm>
#include <functional>
#include <utility>

namespace suns {

// Each entry owns its value, including a carried copy of a renamed/deleted plan.
class ColonyPlanCombo final : public QComboBox {
public:
    using PlanAllowed = std::function<bool(const ProductionTemplate&)>;
    explicit ColonyPlanCombo(QWidget* parent = nullptr, PlanAllowed allowed = {})
        : QComboBox(parent), allowed_(std::move(allowed))
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
        // Applicability can change even when the saved library is identical.
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
        std::erase_if(entries_, [this](const auto& plan) { return !isAllowed(plan); });
        int selected = 0;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            addItem(QString::fromStdString(entries_[i].name));
            if (value && entries_[i] == *value) selected = int(i) + 1;
        }
        if (value && selected == 0) {
            entries_.push_back(*value);
            const bool available = isAllowed(*value);
            addItem(QString::fromStdString(value->name)
                + (available ? " (order copy)" : " (unavailable order copy)"));
            selected = count() - 1;
            if (!available) {
                if (auto* items = qobject_cast<QStandardItemModel*>(model()))
                    items->item(selected)->setEnabled(false);
                setItemData(selected, "This carried plan is unavailable to this civilization. Choose None or a compatible plan.", Qt::ToolTipRole);
            }
        }
        setCurrentIndex(selected);
    }

private:
    [[nodiscard]] bool isAllowed(const ProductionTemplate& value) const { return !allowed_ || allowed_(value); }
    PlanAllowed allowed_;
    std::vector<ProductionTemplate> templates_;
    std::vector<ProductionTemplate> entries_;
};

} // namespace suns
