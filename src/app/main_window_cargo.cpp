#include "main_window.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace suns {
namespace {

constexpr int kMineralScale = 100;
constexpr double kEpsilon = 0.000001;

struct CargoEndpointView {
    CargoTransferEndpoint endpoint;
    const Planet* planet{};
    const Fleet* fleet{};
    QString name;
};

struct MineralControl {
    QSlider* slider{};
    QDoubleSpinBox* spin{};
};

double mineralValue(const MineralCargo& minerals, int index)
{
    if (index == 0) return minerals.ironium;
    if (index == 1) return minerals.boranium;
    return minerals.germanium;
}

std::uint64_t endpointColonists(const CargoEndpointView& endpoint)
{
    return endpoint.planet ? endpoint.planet->population : endpoint.fleet->colonists;
}

MineralCargo endpointMinerals(const CargoEndpointView& endpoint)
{
    return endpoint.planet ? endpoint.planet->minerals : endpoint.fleet->minerals;
}

int sliderMaximum(double amount)
{
    if (!(amount > 0.0)) return 0;
    return static_cast<int>(std::min(
        std::floor(amount * kMineralScale + kEpsilon),
        static_cast<double>(std::numeric_limits<int>::max())));
}

QString cargoValues(std::uint64_t colonists, const MineralCargo& minerals)
{
    return QString("colonists %1 • I %2 • B %3 • G %4")
        .arg(static_cast<qulonglong>(colonists))
        .arg(minerals.ironium, 0, 'f', 2)
        .arg(minerals.boranium, 0, 'f', 2)
        .arg(minerals.germanium, 0, 'f', 2);
}

} // namespace

void MainWindow::openCargoManifestDialog()
{
    const auto* selectedStar = this->selectedStar();
    const auto* authoritativeFleet = selectedFleet();
    if (!selectedStar || !authoritativeFleet
        || !same_position(authoritativeFleet->position, selectedStar->position)) {
        QMessageBox::information(
            this,
            "Cargo Transfer",
            "Select a friendly fleet currently stationed at the selected star system.");
        return;
    }

    // Show the state that earlier local cargo orders will produce at End Turn.
    // The real processor performs validation on this disposable copy, so a
    // second transfer dialog starts from the already planned manifests.
    GameState planned = state_;
    for (auto& fleet : planned.fleets) fleet.destination.reset();
    PlayerOrders cargoOrders{pendingOrders_.player, {}};
    for (const auto& order : pendingOrders_.orders) {
        if (std::holds_alternative<SetFleetColonistsOrder>(order)
            || std::holds_alternative<SetFleetMineralCargoOrder>(order)
            || std::holds_alternative<TransferCargoOrder>(order)) {
            cargoOrders.orders.push_back(order);
        }
    }
    if (!cargoOrders.orders.empty()) planned = processor_.process(planned, {cargoOrders});

    const auto* star = find_star(planned, selectedStar->id);
    if (!star) return;

    std::vector<CargoEndpointView> endpoints;
    if (const auto* planet = find_planet_at_star(planned, star->id);
        planet && (planet->owner == 0 || planet->owner == pendingOrders_.player)) {
        endpoints.push_back({
            {planet->id, 0},
            planet,
            nullptr,
            QString("Planetary surface — %1").arg(QString::fromStdString(planet->name)),
        });
    }
    for (const auto& fleet : planned.fleets) {
        if (fleet.owner != pendingOrders_.player || !same_position(fleet.position, star->position)) continue;
        endpoints.push_back({
            {0, fleet.id},
            nullptr,
            &fleet,
            QString("Fleet — %1").arg(QString::fromStdString(fleet.name)),
        });
    }

    if (endpoints.size() < 2) {
        QMessageBox::information(
            this,
            "Cargo Transfer",
            "Cargo transfer needs a friendly or uncolonized planetary surface and/or at least two friendly fleets at this system.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString("Cargo Transfer — %1").arg(QString::fromStdString(star->name)));
    dialog.setMinimumWidth(650);

    auto* layout = new QVBoxLayout(&dialog);
    auto* introduction = new QLabel(
        "Choose a source and destination, then move any combination of population and minerals. "
        "The order resolves locally at End Turn.",
        &dialog);
    introduction->setWordWrap(true);
    layout->addWidget(introduction);

    auto* endpointForm = new QGridLayout;
    auto* sourceCombo = new QComboBox(&dialog);
    auto* destinationCombo = new QComboBox(&dialog);
    for (const auto& endpoint : endpoints) {
        sourceCombo->addItem(endpoint.name);
        destinationCombo->addItem(endpoint.name);
    }
    endpointForm->addWidget(new QLabel("Source", &dialog), 0, 0);
    endpointForm->addWidget(sourceCombo, 0, 1);
    endpointForm->addWidget(new QLabel("Destination", &dialog), 1, 0);
    endpointForm->addWidget(destinationCombo, 1, 1);
    layout->addLayout(endpointForm);

    int selectedFleetIndex = -1;
    for (std::size_t index = 0; index < endpoints.size(); ++index) {
        if (endpoints[index].fleet && endpoints[index].fleet->id == authoritativeFleet->id) {
            selectedFleetIndex = static_cast<int>(index);
            break;
        }
    }
    sourceCombo->setCurrentIndex(0);
    destinationCombo->setCurrentIndex(selectedFleetIndex > 0 ? selectedFleetIndex : 1);

    auto* transferGrid = new QGridLayout;
    transferGrid->addWidget(new QLabel("Cargo", &dialog), 0, 0);
    transferGrid->addWidget(new QLabel("Amount", &dialog), 0, 1);
    transferGrid->addWidget(new QLabel("Exact", &dialog), 0, 2);

    auto* colonistSlider = new QSlider(Qt::Horizontal, &dialog);
    auto* colonistSpin = new QSpinBox(&dialog);
    colonistSpin->setSingleStep(100);
    transferGrid->addWidget(new QLabel("Colonists", &dialog), 1, 0);
    transferGrid->addWidget(colonistSlider, 1, 1);
    transferGrid->addWidget(colonistSpin, 1, 2);

    const char* mineralNames[] = {"Ironium", "Boranium", "Germanium"};
    std::vector<MineralControl> mineralControls;
    for (int index = 0; index < 3; ++index) {
        auto* slider = new QSlider(Qt::Horizontal, &dialog);
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setDecimals(2);
        spin->setSingleStep(1.0);
        transferGrid->addWidget(new QLabel(mineralNames[index], &dialog), index + 2, 0);
        transferGrid->addWidget(slider, index + 2, 1);
        transferGrid->addWidget(spin, index + 2, 2);
        mineralControls.push_back({slider, spin});
    }
    layout->addLayout(transferGrid);

    auto* sourceAfter = new QLabel(&dialog);
    auto* destinationAfter = new QLabel(&dialog);
    auto* capacitySummary = new QLabel(&dialog);
    auto* validation = new QLabel(&dialog);
    for (auto* label : {sourceAfter, destinationAfter, capacitySummary, validation}) label->setWordWrap(true);
    layout->addWidget(sourceAfter);
    layout->addWidget(destinationAfter);
    layout->addWidget(capacitySummary);
    layout->addWidget(validation);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* acceptButton = buttons->button(QDialogButtonBox::Ok);
    acceptButton->setText("Queue transfer");
    layout->addWidget(buttons);

    const auto transferMinerals = [&] {
        return MineralCargo{
            mineralControls[0].spin->value(),
            mineralControls[1].spin->value(),
            mineralControls[2].spin->value(),
        };
    };

    const auto refresh = [&] {
        const auto sourceIndex = sourceCombo->currentIndex();
        const auto destinationIndex = destinationCombo->currentIndex();
        if (sourceIndex < 0 || destinationIndex < 0) return;
        const auto& source = endpoints[static_cast<std::size_t>(sourceIndex)];
        const auto& destination = endpoints[static_cast<std::size_t>(destinationIndex)];
        const auto colonists = static_cast<std::uint64_t>(colonistSpin->value());
        const auto minerals = transferMinerals();

        const auto sourceCurrentColonists = endpointColonists(source);
        const auto sourceCurrentMinerals = endpointMinerals(source);
        MineralCargo sourceRemainder{
            sourceCurrentMinerals.ironium - minerals.ironium,
            sourceCurrentMinerals.boranium - minerals.boranium,
            sourceCurrentMinerals.germanium - minerals.germanium,
        };
        const auto destinationCurrentColonists = endpointColonists(destination);
        const auto destinationCurrentMinerals = endpointMinerals(destination);
        MineralCargo destinationResult{
            destinationCurrentMinerals.ironium + minerals.ironium,
            destinationCurrentMinerals.boranium + minerals.boranium,
            destinationCurrentMinerals.germanium + minerals.germanium,
        };

        sourceAfter->setText(QString("<b>Source after:</b> %1 — %2")
            .arg(source.name)
            .arg(cargoValues(sourceCurrentColonists - colonists, sourceRemainder)));
        destinationAfter->setText(QString("<b>Destination after:</b> %1 — %2")
            .arg(destination.name)
            .arg(cargoValues(destinationCurrentColonists + colonists, destinationResult)));

        bool capacityValid = true;
        if (destination.fleet) {
            const auto added = colonist_cargo_mass(colonists) + mineral_cargo_mass(minerals);
            const auto used = fleet_cargo_used(planned, *destination.fleet) + added;
            const auto capacity = fleet_cargo_capacity(planned, *destination.fleet);
            const auto grossMass = fleet_gross_mass(planned, *destination.fleet) + added;
            capacityValid = used <= capacity + kEpsilon;
            capacitySummary->setText(QString("<b>Destination hold:</b> %1 / %2 cargo units • gross mass %3 kt")
                .arg(used, 0, 'f', 2)
                .arg(capacity, 0, 'f', 2)
                .arg(grossMass, 0, 'f', 2));
        } else {
            capacitySummary->setText("<b>Destination hold:</b> planetary surface has no cargo-capacity limit.");
        }

        const bool distinct = sourceIndex != destinationIndex;
        const bool hasCargo = colonists > 0 || mineral_cargo_mass(minerals) > kEpsilon;
        const bool valid = distinct && hasCargo && capacityValid;
        acceptButton->setEnabled(valid);
        if (!distinct) {
            validation->setText("<span style='color:#e4b77d'><b>Choose two different endpoints.</b></span>");
        } else if (!hasCargo) {
            validation->setText("Choose at least one cargo amount to transfer.");
        } else if (!capacityValid) {
            validation->setText("<span style='color:#e08d7c'><b>The destination fleet would exceed its shared cargo capacity.</b></span>");
        } else {
            validation->setText("<span style='color:#85d5a5'><b>Transfer is valid.</b></span>");
        }
    };

    const auto configureSource = [&] {
        const auto sourceIndex = sourceCombo->currentIndex();
        const auto destinationIndex = destinationCombo->currentIndex();
        if (sourceIndex < 0 || destinationIndex < 0) return;
        const auto& source = endpoints[static_cast<std::size_t>(sourceIndex)];
        const auto& destination = endpoints[static_cast<std::size_t>(destinationIndex)];

        std::uint64_t availableColonists = endpointColonists(source);
        if (source.planet) {
            availableColonists = source.planet->owner == pendingOrders_.player && availableColonists > 0
                ? availableColonists - 1
                : 0;
        }
        if (destination.planet && destination.planet->owner != pendingOrders_.player) availableColonists = 0;
        const auto colonistMaximum = static_cast<int>(std::min<std::uint64_t>(
            availableColonists, static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
        {
            const QSignalBlocker blockSlider(colonistSlider);
            const QSignalBlocker blockSpin(colonistSpin);
            colonistSlider->setRange(0, colonistMaximum);
            colonistSpin->setRange(0, colonistMaximum);
            colonistSlider->setValue(0);
            colonistSpin->setValue(0);
        }

        const auto minerals = endpointMinerals(source);
        for (int index = 0; index < 3; ++index) {
            const auto available = std::max(0.0, mineralValue(minerals, index));
            const QSignalBlocker blockSlider(mineralControls[index].slider);
            const QSignalBlocker blockSpin(mineralControls[index].spin);
            mineralControls[index].slider->setRange(0, sliderMaximum(available));
            mineralControls[index].spin->setRange(0.0, available);
            mineralControls[index].slider->setValue(0);
            mineralControls[index].spin->setValue(0.0);
        }
        refresh();
    };

    connect(colonistSlider, &QSlider::valueChanged, &dialog, [=](int value) {
        const QSignalBlocker blocker(colonistSpin);
        colonistSpin->setValue(value);
        refresh();
    });
    connect(colonistSpin, &QSpinBox::valueChanged, &dialog, [=](int value) {
        const QSignalBlocker blocker(colonistSlider);
        colonistSlider->setValue(value);
        refresh();
    });
    for (const auto control : mineralControls) {
        connect(control.slider, &QSlider::valueChanged, &dialog, [=](int value) {
            const QSignalBlocker blocker(control.spin);
            control.spin->setValue(static_cast<double>(value) / kMineralScale);
            refresh();
        });
        connect(control.spin, &QDoubleSpinBox::valueChanged, &dialog, [=](double value) {
            const QSignalBlocker blocker(control.slider);
            control.slider->setValue(static_cast<int>(std::lround(value * kMineralScale)));
            refresh();
        });
    }
    connect(sourceCombo, &QComboBox::currentIndexChanged, &dialog, [=](int) { configureSource(); });
    connect(destinationCombo, &QComboBox::currentIndexChanged, &dialog, [=](int) { configureSource(); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    configureSource();
    if (dialog.exec() != QDialog::Accepted) return;

    const auto& source = endpoints[static_cast<std::size_t>(sourceCombo->currentIndex())];
    const auto& destination = endpoints[static_cast<std::size_t>(destinationCombo->currentIndex())];
    const auto colonists = static_cast<std::uint64_t>(colonistSpin->value());
    const auto minerals = transferMinerals();
    appendPendingOrder(
        TransferCargoOrder{source.endpoint, destination.endpoint, colonists, minerals},
        QString("Transfer cargo from %1 to %2 — colonists %3, I %4, B %5, G %6")
            .arg(source.name)
            .arg(destination.name)
            .arg(static_cast<qulonglong>(colonists))
            .arg(minerals.ironium, 0, 'f', 2)
            .arg(minerals.boranium, 0, 'f', 2)
            .arg(minerals.germanium, 0, 'f', 2));
}

} // namespace suns
