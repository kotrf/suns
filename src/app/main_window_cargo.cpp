#include "main_window.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include <algorithm>

namespace suns {

void MainWindow::openCargoManifestDialog()
{
    const auto* fleet = selectedFleet();
    if (!fleet) {
        QMessageBox::information(this, "Cargo Manifest", "Select a friendly fleet first.");
        return;
    }

    const auto* planet = selectedPlanet();
    const auto* star = planet ? find_star(state_, planet->star) : nullptr;
    if (!planet || !star || !same_position(fleet->position, star->position)
        || (planet->owner != fleet->owner && planet->owner != 0)) {
        QMessageBox::information(
            this,
            "Cargo Manifest",
            "Select a friendly colony or uncolonized planet underneath the selected fleet. Mineral transfers are local operations.");
        return;
    }

    const auto cargoCapacity = fleet_cargo_capacity(state_, *fleet);
    const auto colonistCargo = colonist_cargo_mass(fleet->colonists);
    const auto mineralCapacity = std::max(0.0, cargoCapacity - colonistCargo);

    QDialog dialog(this);
    dialog.setWindowTitle(QString("Cargo Manifest — %1").arg(QString::fromStdString(fleet->name)));
    dialog.setMinimumWidth(430);

    auto* layout = new QVBoxLayout(&dialog);
    auto* summary = new QLabel(
        QString("<b>%1</b> docked at <b>%2</b><br>"
                "Shared hold: %3 / %4 used; colonists occupy %5.<br>"
                "Available surface minerals: Fe %6, B %7, Ge %8")
            .arg(QString::fromStdString(fleet->name))
            .arg(QString::fromStdString(planet->name))
            .arg(fleet_cargo_used(state_, *fleet), 0, 'f', 1)
            .arg(cargoCapacity, 0, 'f', 1)
            .arg(colonistCargo, 0, 'f', 1)
            .arg(planet->minerals.ironium, 0, 'f', 1)
            .arg(planet->minerals.boranium, 0, 'f', 1)
            .arg(planet->minerals.germanium, 0, 'f', 1),
        &dialog);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    auto makeSpin = [&](double current, double surfaceAvailable) {
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setDecimals(1);
        spin->setSingleStep(1.0);
        spin->setRange(0.0, std::max(0.0, current + surfaceAvailable));
        spin->setValue(current);
        return spin;
    };

    auto* ironium = makeSpin(fleet->minerals.ironium, planet->minerals.ironium);
    auto* boranium = makeSpin(fleet->minerals.boranium, planet->minerals.boranium);
    auto* germanium = makeSpin(fleet->minerals.germanium, planet->minerals.germanium);

    auto* form = new QFormLayout;
    form->addRow("Ironium aboard", ironium);
    form->addRow("Boranium aboard", boranium);
    form->addRow("Germanium aboard", germanium);
    layout->addLayout(form);

    auto* note = new QLabel(
        QString("Minerals may use at most <b>%1</b> cargo units while the current %2 colonists remain aboard. "
                "Changing the manifest queues a local surface transfer for End Turn; it does not move cargo immediately.")
            .arg(mineralCapacity, 0, 'f', 1)
            .arg(static_cast<qulonglong>(fleet->colonists)),
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;

    const MineralCargo target{ironium->value(), boranium->value(), germanium->value()};
    if (mineral_cargo_mass(target) > mineralCapacity + 0.000001) {
        QMessageBox::warning(
            this,
            "Cargo Manifest",
            QString("That mineral manifest needs %1 cargo units, but only %2 remain after colonists.")
                .arg(mineral_cargo_mass(target), 0, 'f', 1)
                .arg(mineralCapacity, 0, 'f', 1));
        return;
    }

    appendPendingOrder(
        SetFleetMineralCargoOrder{planet->id, fleet->id, target},
        QString("Set %1 cargo: Fe %2, B %3, Ge %4")
            .arg(QString::fromStdString(fleet->name))
            .arg(target.ironium, 0, 'f', 1)
            .arg(target.boranium, 0, 'f', 1)
            .arg(target.germanium, 0, 'f', 1));
}

} // namespace suns
