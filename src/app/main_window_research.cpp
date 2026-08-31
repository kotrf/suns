#include "main_window.hpp"

#include <QComboBox>
#include <QDockWidget>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <iterator>

namespace suns {

namespace {

constexpr std::array<ResearchField, kResearchFieldCount> kResearchFields = {
    ResearchField::Energy,
    ResearchField::Propulsion,
    ResearchField::Construction,
    ResearchField::Electronics,
    ResearchField::Biology,
    ResearchField::Weapons,
};

QString fieldName(ResearchField field)
{
    return QString::fromStdString(research_field_name(field));
}

} // namespace

void MainWindow::installResearch()
{
    if (researchDock_) return;

    researchDock_ = new QDockWidget("Research", this);
    researchDock_->setObjectName("researchDock");
    researchDock_->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    auto* content = new QWidget(researchDock_);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);

    researchSummary_ = new QLabel(content);
    researchSummary_->setObjectName("researchSummary");
    researchSummary_->setWordWrap(true);
    layout->addWidget(researchSummary_);

    researchProgress_ = new QProgressBar(content);
    researchProgress_->setObjectName("researchProgress");
    researchProgress_->setTextVisible(true);
    layout->addWidget(researchProgress_);

    researchUnlock_ = new QLabel(content);
    researchUnlock_->setObjectName("researchUnlock");
    researchUnlock_->setWordWrap(true);
    layout->addWidget(researchUnlock_);

    auto* allocationRow = new QHBoxLayout;
    allocationRow->addWidget(new QLabel("Guaranteed research allocation", content));
    researchAllocationSpin_ = new QSpinBox(content);
    researchAllocationSpin_->setObjectName("researchAllocationSpin");
    researchAllocationSpin_->setRange(0, 100);
    researchAllocationSpin_->setSingleStep(5);
    researchAllocationSpin_->setSuffix("%");
    researchAllocationSpin_->setToolTip(
        "This share of every colony's yearly output goes to empire research before local production. "
        "Output left after a colony finishes its queue also becomes research.");
    allocationRow->addWidget(researchAllocationSpin_);
    layout->addLayout(allocationRow);

    auto* planLabel = new QLabel(
        "Research plan — every row may be reordered or removed; invested RP remains in its field.",
        content);
    planLabel->setWordWrap(true);
    layout->addWidget(planLabel);

    researchPlanTree_ = new QTreeWidget(content);
    researchPlanTree_->setObjectName("researchPlanTree");
    researchPlanTree_->setColumnCount(5);
    researchPlanTree_->setHeaderLabels({"#", "Field", "Target", "Work", "Status"});
    researchPlanTree_->setRootIsDecorated(false);
    researchPlanTree_->setAlternatingRowColors(true);
    researchPlanTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    researchPlanTree_->setMinimumHeight(190);
    researchPlanTree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    researchPlanTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    researchPlanTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    researchPlanTree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    researchPlanTree_->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    layout->addWidget(researchPlanTree_, 1);

    auto* addRow = new QHBoxLayout;
    researchAddCombo_ = new QComboBox(content);
    researchAddCombo_->setObjectName("researchAddCombo");
    for (const auto field : kResearchFields) {
        researchAddCombo_->addItem(fieldName(field), static_cast<int>(field));
    }
    researchAddButton_ = new QPushButton("Add to plan", content);
    researchAddButton_->setObjectName("researchAddButton");
    addRow->addWidget(researchAddCombo_, 1);
    addRow->addWidget(researchAddButton_);
    layout->addLayout(addRow);

    auto* editRow = new QHBoxLayout;
    researchMoveUpButton_ = new QPushButton("Move up", content);
    researchMoveDownButton_ = new QPushButton("Move down", content);
    researchRemoveButton_ = new QPushButton("Remove", content);
    researchMoveUpButton_->setToolTip("Move the selected queued level earlier");
    researchMoveDownButton_->setToolTip("Move the selected queued level later");
    researchRemoveButton_->setToolTip("Remove the selected queued level");
    editRow->addWidget(researchMoveUpButton_);
    editRow->addWidget(researchMoveDownButton_);
    editRow->addWidget(researchRemoveButton_);
    layout->addLayout(editRow);

    researchDock_->setWidget(content);
    addDockWidget(Qt::RightDockWidgetArea, researchDock_);

    QMenu* view = menuBar()->findChild<QMenu*>("sunsViewMenu");
    if (!view) {
        view = menuBar()->addMenu("&View");
        view->setObjectName("sunsViewMenu");
    }
    view->addAction(researchDock_->toggleViewAction());

    connect(researchAddButton_, &QPushButton::clicked, this, &MainWindow::addResearchPlanItem);
    connect(researchMoveUpButton_, &QPushButton::clicked, this,
        [this] { moveSelectedResearchPlanItem(-1); });
    connect(researchMoveDownButton_, &QPushButton::clicked, this,
        [this] { moveSelectedResearchPlanItem(1); });
    connect(researchRemoveButton_, &QPushButton::clicked, this, &MainWindow::removeSelectedResearchPlanItem);
    connect(researchAllocationSpin_, &QSpinBox::valueChanged,
        this, &MainWindow::queueResearchAllocation);
    connect(researchPlanTree_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto row = researchPlanTree_->indexOfTopLevelItem(researchPlanTree_->currentItem());
        const auto count = researchPlanTree_->topLevelItemCount();
        researchMoveUpButton_->setEnabled(row > 0);
        researchMoveDownButton_->setEnabled(row >= 0 && row + 1 < count);
        researchRemoveButton_->setEnabled(row >= 0);
    });
    refreshResearchPanel();
}

void MainWindow::refreshResearchPanel()
{
    if (!researchDock_) return;
    const auto* player = find_player(state_, 1);
    if (!player) return;

    auto planActive = player->technology.researchActive;
    auto focus = player->technology.focus;
    auto queuedFocuses = player->technology.queuedFocuses;
    auto allocationPercent = player->technology.researchAllocationPercent;
    for (const auto& order : pendingOrders_.orders) {
        if (const auto* plan = std::get_if<SetResearchPlanOrder>(&order)) {
            planActive = plan->active;
            focus = plan->focus;
            queuedFocuses = plan->queuedFocuses;
        } else if (const auto* allocation = std::get_if<SetResearchAllocationOrder>(&order)) {
            allocationPercent = allocation->percent;
        }
    }

    {
        const QSignalBlocker blocker(researchAllocationSpin_);
        researchAllocationSpin_->setValue(allocationPercent);
    }
    std::uint32_t guaranteedRp = 0;
    if (planActive) {
        for (const auto& planet : state_.planets) {
            if (planet.owner != player->id) continue;
            guaranteedRp += static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(colony_output(planet)) * allocationPercent / 100U);
        }
    }

    QStringList levels;
    for (const auto field : kResearchFields) {
        const auto index = static_cast<std::size_t>(field);
        levels << QString("%1 %2").arg(fieldName(field)).arg(player->technology.levels[index]);
    }
    const auto allocationSummary = planActive
        ? QString("Guaranteed RP next turn: %1, plus unused colony output").arg(guaranteedRp)
        : QString("Research is paused; the allocation is not deducted from colony output");
    researchSummary_->setText(QString("<b>Empire technology</b><br>%1<br>%2")
        .arg(levels.join(" • "), allocationSummary));

    const auto index = static_cast<std::size_t>(focus);
    const auto level = player->technology.levels[index];
    if (planActive) {
        const auto progress = player->technology.progress[index];
        const auto cost = research_level_cost(focus, static_cast<std::uint8_t>(level + 1));
        researchProgress_->setRange(0, static_cast<int>(cost));
        researchProgress_->setValue(static_cast<int>(std::min(progress, cost)));
        researchProgress_->setFormat(QString("%1 %2 → %3: %4 / %5 RP")
            .arg(fieldName(focus)).arg(level).arg(level + 1).arg(progress).arg(cost));
    } else {
        researchProgress_->setRange(0, 1);
        researchProgress_->setValue(0);
        researchProgress_->setFormat("Research paused — add a field to the plan");
    }

    QString unlock = "No concrete unlock is assigned to the next level in this first slice.";
    if (!planActive) {
        unlock = "No research target is selected. Accumulated RP is preserved in each field.";
    } else if (focus == ResearchField::Energy && level == 0) {
        unlock = "Next unlock: Antimatter Generator — onboard fuel production plus 200 units of reserve capacity.";
    } else if (focus == ResearchField::Propulsion && level == 0) {
        unlock = "Next unlock: Advanced Fusion Drive — a light, safe Warp-9 engine that consumes fuel instead of scooping it.";
    } else if (focus == ResearchField::Electronics) {
        if (level == 0) unlock = "Next unlock: Compact Long Range Scanner — lighter and cheaper, with a 55 ly field.";
        else if (level == 1) unlock = "Next unlock: Extended Range Scanner — a heavy 160 ly ordinary sensor for deep-space coverage.";
        else if (level == 2) unlock = "Next unlock: Penetrating Scanner — approximate planetary data without entering orbit.";
        else unlock = "Higher Electronics levels will later support communications, classification and electronic warfare.";
    } else if (focus == ResearchField::Construction && level == 0) {
        unlock = "Next unlock: Remote Mining Module — mines uncolonized worlds into surface stockpiles for cargo fleets to collect.";
    }
    researchUnlock_->setText(unlock);

    const auto previousRow = researchPlanTree_->indexOfTopLevelItem(researchPlanTree_->currentItem());
    researchPlanTree_->clear();
    auto projectedLevels = player->technology.levels;
    auto projectedProgress = player->technology.progress;
    std::vector<ResearchField> planFields;
    if (planActive) planFields.push_back(focus);
    planFields.insert(planFields.end(), queuedFocuses.begin(), queuedFocuses.end());
    for (std::size_t planIndex = 0; planIndex < planFields.size(); ++planIndex) {
        const auto field = planFields[planIndex];
        const auto fieldIndex = static_cast<std::size_t>(field);
        const auto fromLevel = projectedLevels[fieldIndex];
        const auto toLevel = static_cast<std::uint8_t>(fromLevel + 1);
        const auto levelCost = research_level_cost(field, toLevel);
        const auto completedRp = std::min(projectedProgress[fieldIndex], levelCost);

        auto* row = new QTreeWidgetItem(researchPlanTree_);
        row->setText(0, QString::number(planIndex + 1));
        row->setText(1, fieldName(field));
        row->setText(2, QString("L%1 → L%2").arg(fromLevel).arg(toLevel));
        row->setText(3, QString("%1 / %2 RP").arg(completedRp).arg(levelCost));
        const bool alreadyActive = planIndex == 0
            && player->technology.researchActive
            && field == player->technology.focus;
        row->setText(4, planIndex == 0
            ? alreadyActive ? "In progress" : "Next after End Turn"
            : "Queued");
        row->setData(0, Qt::UserRole, static_cast<int>(field));

        projectedLevels[fieldIndex] = toLevel;
        projectedProgress[fieldIndex] = 0;
    }
    const auto rowCount = researchPlanTree_->topLevelItemCount();
    if (rowCount > 0) {
        const auto selectedRow = std::clamp(previousRow, 0, rowCount - 1);
        researchPlanTree_->setCurrentItem(researchPlanTree_->topLevelItem(selectedRow));
    }
    const auto row = researchPlanTree_->indexOfTopLevelItem(researchPlanTree_->currentItem());
    researchMoveUpButton_->setEnabled(row > 0);
    researchMoveDownButton_->setEnabled(row >= 0 && row + 1 < rowCount);
    researchRemoveButton_->setEnabled(row >= 0);

}

void MainWindow::queueResearchAllocation(int percent)
{
    const auto* player = find_player(state_, pendingOrders_.player);
    if (!player || percent < 0 || percent > 100) return;

    const auto pending = std::find_if(
        pendingOrders_.orders.begin(), pendingOrders_.orders.end(), [](const Order& order) {
            return std::holds_alternative<SetResearchAllocationOrder>(order);
        });
    if (percent == player->technology.researchAllocationPercent) {
        if (pending == pendingOrders_.orders.end()) return;
        const auto index = static_cast<int>(std::distance(pendingOrders_.orders.begin(), pending));
        pendingOrders_.orders.erase(pending);
        pendingDescriptions_.removeAt(index);
        rebuildScene();
        statusBar()->showMessage("Research allocation reverted to the committed value");
        return;
    }

    const auto description = QString("Allocate %1% of every colony's output to research").arg(percent);
    if (pending != pendingOrders_.orders.end()) {
        const auto index = static_cast<int>(std::distance(pendingOrders_.orders.begin(), pending));
        *pending = SetResearchAllocationOrder{static_cast<std::uint8_t>(percent)};
        pendingDescriptions_[index] = description;
        rebuildScene();
        statusBar()->showMessage(description);
        return;
    }
    appendPendingOrder(SetResearchAllocationOrder{static_cast<std::uint8_t>(percent)}, description);
}

void MainWindow::queueResearchPlan()
{
    const auto* player = find_player(state_, pendingOrders_.player);
    if (!player || !researchPlanTree_) return;
    std::vector<ResearchField> planFields;
    planFields.reserve(static_cast<std::size_t>(researchPlanTree_->topLevelItemCount()));
    for (int row = 0; row < researchPlanTree_->topLevelItemCount(); ++row) {
        planFields.push_back(static_cast<ResearchField>(
            researchPlanTree_->topLevelItem(row)->data(0, Qt::UserRole).toInt()));
    }

    const bool active = !planFields.empty();
    const auto focus = active ? planFields.front() : player->technology.focus;
    std::vector<ResearchField> queuedFocuses;
    if (active) queuedFocuses.assign(planFields.begin() + 1, planFields.end());

    const auto pendingPlan = std::find_if(
        pendingOrders_.orders.begin(), pendingOrders_.orders.end(), [](const Order& order) {
            return std::holds_alternative<SetResearchPlanOrder>(order);
        });
    if (active == player->technology.researchActive
        && (!active || focus == player->technology.focus)
        && queuedFocuses == player->technology.queuedFocuses) {
        if (pendingPlan == pendingOrders_.orders.end()) return;
        const auto index = static_cast<int>(std::distance(pendingOrders_.orders.begin(), pendingPlan));
        pendingOrders_.orders.erase(pendingPlan);
        pendingDescriptions_.removeAt(index);
        rebuildScene();
        statusBar()->showMessage("Research plan reverted to the current committed queue");
        return;
    }

    QStringList futureNames;
    for (const auto field : queuedFocuses) futureNames << fieldName(field);
    const auto description = !active
        ? QString("Pause empire research; preserve accumulated RP")
        : futureNames.empty()
        ? QString("Research %1").arg(fieldName(focus))
        : QString("Research %1; then %2").arg(fieldName(focus), futureNames.join(" → "));

    if (pendingPlan != pendingOrders_.orders.end()) {
        const auto index = static_cast<int>(std::distance(pendingOrders_.orders.begin(), pendingPlan));
        *pendingPlan = SetResearchPlanOrder{focus, queuedFocuses, active};
        pendingDescriptions_[index] = description;
        rebuildScene();
        statusBar()->showMessage(description);
        return;
    }
    appendPendingOrder(SetResearchPlanOrder{focus, queuedFocuses, active}, description);
}

void MainWindow::addResearchPlanItem()
{
    if (!researchPlanTree_ || !researchAddCombo_) return;
    auto* row = new QTreeWidgetItem(researchPlanTree_);
    row->setData(0, Qt::UserRole, researchAddCombo_->currentData());
    const auto selectedRow = researchPlanTree_->topLevelItemCount() - 1;
    queueResearchPlan();
    if (selectedRow < researchPlanTree_->topLevelItemCount()) {
        researchPlanTree_->setCurrentItem(researchPlanTree_->topLevelItem(selectedRow));
    }
}

void MainWindow::moveSelectedResearchPlanItem(int direction)
{
    if (!researchPlanTree_) return;
    const auto from = researchPlanTree_->indexOfTopLevelItem(researchPlanTree_->currentItem());
    const auto to = from + direction;
    if (from < 0 || to < 0 || to >= researchPlanTree_->topLevelItemCount()) return;
    auto* item = researchPlanTree_->takeTopLevelItem(from);
    researchPlanTree_->insertTopLevelItem(to, item);
    researchPlanTree_->setCurrentItem(item);
    queueResearchPlan();
    if (to < researchPlanTree_->topLevelItemCount()) {
        researchPlanTree_->setCurrentItem(researchPlanTree_->topLevelItem(to));
    }
}

void MainWindow::removeSelectedResearchPlanItem()
{
    if (!researchPlanTree_) return;
    const auto row = researchPlanTree_->indexOfTopLevelItem(researchPlanTree_->currentItem());
    if (row < 0) return;
    delete researchPlanTree_->takeTopLevelItem(row);
    queueResearchPlan();
    const auto rowCount = researchPlanTree_->topLevelItemCount();
    if (rowCount > 0) {
        const auto selectedRow = std::min(row, rowCount - 1);
        researchPlanTree_->setCurrentItem(researchPlanTree_->topLevelItem(selectedRow));
    }
}

} // namespace suns
