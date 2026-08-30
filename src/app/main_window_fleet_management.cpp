#include "main_window.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>

namespace suns {

namespace {

bool readyForReorganization(const Fleet& fleet)
{
    return !fleet.destination
        && fleet.waypointQueue.empty()
        && fleet.pendingCommands.empty()
        && fleet.task == FleetTask::None
        && !fleet.repeatOrders
        && fleet.routeTemplate.empty();
}

QString compositionText(const GameState& state, const Fleet& fleet)
{
    QStringList parts;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        parts << QString("%1× %2")
                     .arg(stack.count)
                     .arg(design ? QString::fromStdString(design->name) : QString("Design %1").arg(stack.design));
    }
    return parts.join(" • ");
}

} // namespace

void MainWindow::openMergeFleetsDialog()
{
    const auto* destination = selectedFleet();
    if (!destination) {
        statusBar()->showMessage("Select the fleet whose FleetId should survive", 3000);
        return;
    }
    if (!readyForReorganization(*destination)) {
        QMessageBox::information(this, "Merge fleets",
            "The selected fleet must be stationary, idle, and have no commands in flight.");
        return;
    }

    std::vector<const Fleet*> candidates;
    for (const auto& fleet : state_.fleets) {
        if (fleet.id != destination->id
            && fleet.owner == destination->owner
            && same_position(fleet.position, destination->position)
            && readyForReorganization(fleet)) {
            candidates.push_back(&fleet);
        }
    }
    if (candidates.empty()) {
        QMessageBox::information(this, "Merge fleets",
            "No other stationary, idle friendly fleet is at this position.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Merge fleets");
    auto* layout = new QVBoxLayout(&dialog);
    auto* explanation = new QLabel(
        QString("<b>%1</b> keeps FleetId %2.<br>%3")
            .arg(QString::fromStdString(destination->name))
            .arg(destination->id)
            .arg(compositionText(state_, *destination)),
        &dialog);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* form = new QFormLayout;
    auto* sourceCombo = new QComboBox(&dialog);
    for (const auto* fleet : candidates) {
        sourceCombo->addItem(
            QString("%1 — %2").arg(QString::fromStdString(fleet->name), compositionText(state_, *fleet)),
            static_cast<quint32>(fleet->id));
    }
    form->addRow("Fleet to consume", sourceCombo);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Queue merge");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;

    const auto source = static_cast<FleetId>(sourceCombo->currentData().toUInt());
    appendPendingOrder(
        MergeFleetsOrder{destination->id, source},
        QString("Merge fleet %1 into %2 (FleetId %3 survives)")
            .arg(source)
            .arg(QString::fromStdString(destination->name))
            .arg(destination->id));
    rebuildScene();
}

void MainWindow::openSplitFleetDialog()
{
    const auto* source = selectedFleet();
    if (!source) {
        statusBar()->showMessage("Select a fleet to split", 3000);
        return;
    }
    if (!readyForReorganization(*source)) {
        QMessageBox::information(this, "Split fleet",
            "The selected fleet must be stationary, idle, and have no commands in flight.");
        return;
    }
    const auto stacks = fleet_ship_stacks(*source);
    if (fleet_ship_count(*source) < 2) {
        QMessageBox::information(this, "Split fleet", "A fleet needs at least two ships before it can be split.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Split fleet");
    auto* layout = new QVBoxLayout(&dialog);
    auto* explanation = new QLabel(
        QString("Select ships for the new fleet. <b>%1</b> keeps FleetId %2; the detachment receives a new ID. "
                "Fuel and cargo are divided in proportion to capacity.")
            .arg(QString::fromStdString(source->name))
            .arg(source->id),
        &dialog);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* form = new QFormLayout;
    std::vector<QSpinBox*> counts;
    counts.reserve(stacks.size());
    for (const auto& stack : stacks) {
        const auto* design = find_ship_design(state_, stack.design);
        auto* spin = new QSpinBox(&dialog);
        spin->setRange(0, static_cast<int>(stack.count));
        spin->setValue(0);
        form->addRow(
            design ? QString::fromStdString(design->name) : QString("Design %1").arg(stack.design),
            spin);
        counts.push_back(spin);
    }
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Queue split");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    while (dialog.exec() == QDialog::Accepted) {
        SplitFleetOrder order;
        order.source = source->id;
        std::uint32_t moved = 0;
        for (std::size_t index = 0; index < stacks.size(); ++index) {
            const auto count = static_cast<std::uint32_t>(counts[index]->value());
            if (count == 0) continue;
            order.ships.push_back({stacks[index].design, count});
            moved += count;
        }
        if (moved == 0 || moved >= fleet_ship_count(*source)) {
            QMessageBox::warning(&dialog, "Split fleet",
                "Move at least one ship, but leave at least one ship in the original fleet.");
            continue;
        }

        appendPendingOrder(
            std::move(order),
            QString("Split %1 ship(s) from %2; FleetId %3 remains with the source")
                .arg(moved)
                .arg(QString::fromStdString(source->name))
                .arg(source->id));
        rebuildScene();
        return;
    }
}

} // namespace suns
