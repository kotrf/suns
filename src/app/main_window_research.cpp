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

bool colonyResearchEnabled(const Planet& planet)
{
    return std::any_of(planet.productionQueue.begin(), planet.productionQueue.end(), [](const ProductionItem& item) {
        return item.kind == ProductionKind::Research;
    });
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

    auto* planLabel = new QLabel(
        "Research plan — the active first row is locked; queued rows may be reordered or removed.",
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

    colonyResearchButton_ = new QPushButton("Start ongoing colony research", content);
    colonyResearchButton_->setObjectName("colonyResearchButton");
    layout->addWidget(colonyResearchButton_);

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
    connect(researchPlanTree_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto row = researchPlanTree_->indexOfTopLevelItem(researchPlanTree_->currentItem());
        const auto count = researchPlanTree_->topLevelItemCount();
        researchMoveUpButton_->setEnabled(row > 1);
        researchMoveDownButton_->setEnabled(row >= 1 && row + 1 < count);
        researchRemoveButton_->setEnabled(row >= 1);
    });
    connect(colonyResearchButton_, &QPushButton::clicked, this, [this] { toggleSelectedColonyResearch(); });
    refreshResearchPanel();
}

void MainWindow::refreshResearchPanel()
{
    if (!researchDock_) return;
    const auto* player = find_player(state_, 1);
    if (!player) return;

    const auto focus = player->technology.focus;
    auto queuedFocuses = player->technology.queuedFocuses;
    for (const auto& order : pendingOrders_.orders) {
        if (const auto* plan = std::get_if<SetResearchPlanOrder>(&order)) {
            if (plan->focus == focus) queuedFocuses = plan->queuedFocuses;
        }
    }

    QStringList levels;
    for (const auto field : kResearchFields) {
        const auto index = static_cast<std::size_t>(field);
        levels << QString("%1 %2").arg(fieldName(field)).arg(player->technology.levels[index]);
    }
    researchSummary_->setText(QString("<b>Empire technology</b><br>%1").arg(levels.join(" • ")));

    const auto index = static_cast<std::size_t>(focus);
    const auto level = player->technology.levels[index];
    const auto progress = player->technology.progress[index];
    const auto cost = research_level_cost(focus, static_cast<std::uint8_t>(level + 1));
    researchProgress_->setRange(0, static_cast<int>(cost));
    researchProgress_->setValue(static_cast<int>(std::min(progress, cost)));
    researchProgress_->setFormat(QString("%1 %2 → %3: %4 / %5 RP")
        .arg(fieldName(focus)).arg(level).arg(level + 1).arg(progress).arg(cost));

    QString unlock = "No concrete unlock is assigned to the next level in this first slice.";
    if (focus == ResearchField::Electronics) {
        if (level == 0) unlock = "Next unlock: Compact Long Range Scanner — lighter and cheaper, with a 55 ly field.";
        else if (level == 1) unlock = "Electronics 2: extended sensor array is planned as a heavy, long-range alternative.";
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
    std::vector<ResearchField> planFields{focus};
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
        row->setText(4, planIndex == 0 ? "In progress • locked" : "Queued");
        row->setData(0, Qt::UserRole, static_cast<int>(field));

        projectedLevels[fieldIndex] = toLevel;
        projectedProgress[fieldIndex] = 0;
    }
    const auto selectedRow = std::clamp(
        previousRow,
        0,
        std::max(0, researchPlanTree_->topLevelItemCount() - 1));
    researchPlanTree_->setCurrentItem(researchPlanTree_->topLevelItem(selectedRow));
    const auto row = researchPlanTree_->indexOfTopLevelItem(researchPlanTree_->currentItem());
    const auto rowCount = researchPlanTree_->topLevelItemCount();
    researchMoveUpButton_->setEnabled(row > 1);
    researchMoveDownButton_->setEnabled(row >= 1 && row + 1 < rowCount);
    researchRemoveButton_->setEnabled(row >= 1);

    const auto* planet = selectedPlanet();
    const bool owned = planet && planet->owner == 1;
    bool enabled = owned && colonyResearchEnabled(*planet);
    if (owned) {
        for (const auto& order : pendingOrders_.orders) {
            if (const auto* research = std::get_if<SetColonyResearchOrder>(&order);
                research && research->colony == planet->id) {
                enabled = research->enabled;
            }
        }
    }
    colonyResearchButton_->setEnabled(owned);
    colonyResearchButton_->setText(!owned
        ? "Select a friendly colony"
        : enabled
            ? "Stop ongoing research at selected colony"
            : "Start ongoing research after queued work");
}

void MainWindow::queueResearchPlan()
{
    const auto* player = find_player(state_, pendingOrders_.player);
    if (!player || !researchPlanTree_) return;
    const auto focus = player->technology.focus;
    std::vector<ResearchField> queuedFocuses;
    for (int row = 1; row < researchPlanTree_->topLevelItemCount(); ++row) {
        queuedFocuses.push_back(static_cast<ResearchField>(
            researchPlanTree_->topLevelItem(row)->data(0, Qt::UserRole).toInt()));
    }

    const auto pendingPlan = std::find_if(
        pendingOrders_.orders.begin(), pendingOrders_.orders.end(), [](const Order& order) {
            return std::holds_alternative<SetResearchPlanOrder>(order);
        });
    if (queuedFocuses == player->technology.queuedFocuses) {
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
    const auto description = futureNames.empty()
        ? QString("Research %1; continue the same field").arg(fieldName(focus))
        : QString("Research %1; then %2").arg(fieldName(focus), futureNames.join(" → "));

    if (pendingPlan != pendingOrders_.orders.end()) {
        const auto index = static_cast<int>(std::distance(pendingOrders_.orders.begin(), pendingPlan));
        *pendingPlan = SetResearchPlanOrder{focus, queuedFocuses};
        pendingDescriptions_[index] = description;
        rebuildScene();
        statusBar()->showMessage(description);
        return;
    }
    appendPendingOrder(SetResearchPlanOrder{focus, queuedFocuses}, description);
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
    if (from < 1 || to < 1 || to >= researchPlanTree_->topLevelItemCount()) return;
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
    if (row < 1) return;
    delete researchPlanTree_->takeTopLevelItem(row);
    queueResearchPlan();
    const auto selectedRow = std::min(row, researchPlanTree_->topLevelItemCount() - 1);
    researchPlanTree_->setCurrentItem(researchPlanTree_->topLevelItem(selectedRow));
}

void MainWindow::toggleSelectedColonyResearch()
{
    const auto* planet = selectedPlanet();
    if (!planet || planet->owner != 1) return;

    bool enabled = colonyResearchEnabled(*planet);
    for (const auto& order : pendingOrders_.orders) {
        if (const auto* research = std::get_if<SetColonyResearchOrder>(&order);
            research && research->colony == planet->id) {
            enabled = research->enabled;
        }
    }
    const bool target = !enabled;
    const auto description = QString("%1 ongoing Research at %2")
        .arg(target ? "Enable" : "Stop")
        .arg(QString::fromStdString(planet->name));

    for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
        const auto* research = std::get_if<SetColonyResearchOrder>(&pendingOrders_.orders[index]);
        if (!research || research->colony != planet->id) continue;
        pendingOrders_.orders[index] = SetColonyResearchOrder{planet->id, target};
        pendingDescriptions_[static_cast<int>(index)] = description;
        rebuildScene();
        statusBar()->showMessage(description);
        return;
    }
    appendPendingOrder(SetColonyResearchOrder{planet->id, target}, description);
}

} // namespace suns
