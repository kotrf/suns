#include "main_window.hpp"

#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

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

    researchFocusCombo_ = new QComboBox(content);
    researchFocusCombo_->setObjectName("researchFocusCombo");
    researchNextCombo_ = new QComboBox(content);
    researchNextCombo_->setObjectName("researchNextCombo");
    researchNextCombo_->addItem("Continue the same field", -1);
    for (const auto field : kResearchFields) {
        researchFocusCombo_->addItem(fieldName(field), static_cast<int>(field));
        researchNextCombo_->addItem(fieldName(field), static_cast<int>(field));
    }

    auto* form = new QFormLayout;
    form->addRow("Current focus", researchFocusCombo_);
    form->addRow("After next level", researchNextCombo_);
    layout->addLayout(form);

    applyResearchPlanButton_ = new QPushButton("Apply research plan", content);
    applyResearchPlanButton_->setObjectName("applyResearchPlanButton");
    colonyResearchButton_ = new QPushButton("Start ongoing colony research", content);
    colonyResearchButton_->setObjectName("colonyResearchButton");
    layout->addWidget(applyResearchPlanButton_);
    layout->addWidget(colonyResearchButton_);
    layout->addStretch(1);

    researchDock_->setWidget(content);
    addDockWidget(Qt::RightDockWidgetArea, researchDock_);

    QMenu* view = menuBar()->findChild<QMenu*>("sunsViewMenu");
    if (!view) {
        view = menuBar()->addMenu("&View");
        view->setObjectName("sunsViewMenu");
    }
    view->addAction(researchDock_->toggleViewAction());

    connect(applyResearchPlanButton_, &QPushButton::clicked, this, [this] { queueResearchPlan(); });
    connect(colonyResearchButton_, &QPushButton::clicked, this, [this] { toggleSelectedColonyResearch(); });
    refreshResearchPanel();
}

void MainWindow::refreshResearchPanel()
{
    if (!researchDock_) return;
    const auto* player = find_player(state_, 1);
    if (!player) return;

    auto focus = player->technology.focus;
    auto nextFocus = player->technology.nextFocus;
    for (const auto& order : pendingOrders_.orders) {
        if (const auto* plan = std::get_if<SetResearchPlanOrder>(&order)) {
            focus = plan->focus;
            nextFocus = plan->nextFocus;
        }
    }

    const QSignalBlocker focusBlocker(researchFocusCombo_);
    const QSignalBlocker nextBlocker(researchNextCombo_);
    researchFocusCombo_->setCurrentIndex(researchFocusCombo_->findData(static_cast<int>(focus)));
    researchNextCombo_->setCurrentIndex(nextFocus
        ? researchNextCombo_->findData(static_cast<int>(*nextFocus))
        : 0);

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
    }
    researchUnlock_->setText(unlock);

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
    const auto focus = static_cast<ResearchField>(researchFocusCombo_->currentData().toInt());
    const auto nextValue = researchNextCombo_->currentData().toInt();
    const auto next = nextValue < 0
        ? std::optional<ResearchField>{}
        : std::optional<ResearchField>{static_cast<ResearchField>(nextValue)};
    const auto description = QString("Research %1%2")
        .arg(fieldName(focus))
        .arg(next ? QString("; then %1").arg(fieldName(*next)) : "; continue same field");

    for (std::size_t index = 0; index < pendingOrders_.orders.size(); ++index) {
        if (!std::holds_alternative<SetResearchPlanOrder>(pendingOrders_.orders[index])) continue;
        pendingOrders_.orders[index] = SetResearchPlanOrder{focus, next};
        pendingDescriptions_[static_cast<int>(index)] = description;
        rebuildScene();
        statusBar()->showMessage(description);
        return;
    }
    appendPendingOrder(SetResearchPlanOrder{focus, next}, description);
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
