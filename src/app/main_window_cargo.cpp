#include "suns/orbital_habitats.hpp"
#include "suns/planetary_engineering.hpp"
#include "main_window.hpp"
#include "suns/communications.hpp"
#include "suns/campaign.hpp"

#include <QAction>
#include <QComboBox>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
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
    return endpoint.planet ? (endpoint.endpoint.orbital?endpoint.planet->habitat.population:endpoint.planet->population) : endpoint.fleet->colonists;
}

MineralCargo endpointMinerals(const CargoEndpointView& endpoint)
{
    if(endpoint.endpoint.orbital) return {};
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

const StarSystem* MainWindow::selectedFleetCargoStar(QString* unavailableReason) const
{
    const auto unavailable = [unavailableReason](const QString& reason) -> const StarSystem* {
        if (unavailableReason) *unavailableReason = reason;
        return nullptr;
    };
    const auto* fleet = selectedFleet();
    if (!fleet) return unavailable("Select a friendly fleet to transfer cargo.");
    const auto age = fleet_telemetry_age(state_, *fleet);
    // Radar can locate a silent hull without receiving its current manifest.
    // Never inspect remote truth to choose or populate a cargo endpoint.
    if (age > 0) {
        if (fleet_radar_position(state_, *fleet)) return unavailable(QString(
            "Radar locates this fleet, but cargo transfer needs current onboard telemetry and a live link. "
            "Last telemetry is %1 turn(s) old.").arg(static_cast<qulonglong>(age)));
        return unavailable(QString(
            "Cargo transfer needs a confirmed current orbit. Last telemetry is %1 turn(s) old; "
            "the displayed position is an estimate.").arg(static_cast<qulonglong>(age)));
    }
    const bool currentFleet = std::any_of(state_.fleets.begin(), state_.fleets.end(),
        [fleet](const Fleet& candidate) { return &candidate == fleet; });
    if (!currentFleet || !fleet_has_instant_link(state_, *fleet))
        return unavailable("Cargo transfer needs a live link at the fleet's current orbit. "
            "Remote fleets can use on-arrival cargo orders.");
    const auto visible = fleet_player_view(state_, *fleet);
    const auto star = std::find_if(state_.stars.begin(), state_.stars.end(),
        [&](const StarSystem& candidate) { return same_position(candidate.position, visible.position); });
    if (star == state_.stars.end())
        return unavailable("Cargo transfer is available when the selected fleet is at a star system.");
    if (unavailableReason) unavailableReason->clear();
    return &*star;
}

void MainWindow::refreshCargoTransferControls()
{
    QString reason;
    const auto* star = selectedFleetCargoStar(&reason);
    const auto tooltip = star
        ? QString("Transfer cargo at %1 for the selected fleet. No separate system selection is needed.")
            .arg(QString::fromStdString(star->name))
        : reason;
    if (auto* button = findChild<QPushButton*>("fleetTransferCargoButton")) {
        button->setEnabled(star != nullptr);
        button->setToolTip(tooltip);
    }
    for (const auto* name : {"fleetTransferCargoAction", "fleetTransferCargoToolAction"}) {
        if (auto* action = findChild<QAction*>(name)) {
            action->setEnabled(star != nullptr);
            action->setToolTip(tooltip);
        }
    }
}

void MainWindow::openCargoManifestDialog()
{
    QString unavailableReason;
    const auto* cargoStar = selectedFleetCargoStar(&unavailableReason);
    if (!cargoStar) {
        statusBar()->showMessage(unavailableReason, 7000);
        return;
    }
    const auto selectedFleetId = *selection_.fleet;

    // Exact command-boundary projection. Replaying End Turn here would also
    // grow population, mine ore, run combat, and move ships. Repeated dialog
    // openings must not advance the world or alter a single colonist.
    GameState planned = projected_cargo_order_state(make_player_view(state_, pendingOrders_.player).state, pendingOrders_);

    const auto* star = find_star(planned, cargoStar->id);
    if (!star) return;

    std::vector<CargoEndpointView> endpoints;
    if (const auto* planet = find_planet_at_star(planned, star->id); planet) {
        const auto prefix = planet->owner != 0 && planet->owner != pendingOrders_.player
            ? QString("Enemy colony")
            : QString("Planetary surface");
        endpoints.push_back({
            {planet->id, 0},
            planet,
            nullptr,
            QString("%1 — %2").arg(prefix, QString::fromStdString(planet->name)),
        });
    }
    if(const auto* p=find_planet_at_star(planned,star->id);p && p->owner==pendingOrders_.player && habitat_completed(*p))
        endpoints.push_back({{p->id,0,true},p,nullptr,QString("Orbital settlement — %1").arg(QString::fromStdString(p->name))});
    for (const auto& fleet : planned.fleets) {
        if (fleet.owner != pendingOrders_.player || !same_position(fleet.position, star->position)
            || fleet_telemetry_age(planned, fleet) != 0 || !fleet_has_instant_link(planned, fleet)) continue;
        endpoints.push_back({
            {0, fleet.id},
            nullptr,
            &fleet,
            QString("Fleet — %1").arg(QString::fromStdString(fleet.name)),
        });
    }

    if (endpoints.size() < 2) {
        statusBar()->showMessage(
            "Cargo transfer needs a planetary surface and a friendly fleet, or two friendly fleets at this system.", 7000);
        return;
    }

    QDialog dialog(this);
    dialog.setObjectName("cargoTransferDialog");
    dialog.setWindowTitle(QString("Transfer cargo — %1").arg(QString::fromStdString(star->name)));
    dialog.setMinimumWidth(920);

    auto* layout = new QVBoxLayout(&dialog);
    auto* introduction = new QLabel(
        "Left: choose the source and the amount to send. Right: choose the destination and "
        "see its complete projected manifest, including previously queued loads. "
        "The same sliders can be adjusted on either side; cargo is not physically moved until End Turn.",
        &dialog);
    introduction->setWordWrap(true);
    layout->addWidget(introduction);

    auto* sideBySide = new QHBoxLayout;
    auto* sourceGroup = new QGroupBox("FROM — stock and outgoing cargo", &dialog);
    auto* destinationGroup = new QGroupBox("TO — complete destination manifest", &dialog);
    sourceGroup->setObjectName("cargoSourceContainer");
    destinationGroup->setObjectName("cargoDestinationContainer");
    auto* sourceLayout = new QVBoxLayout(sourceGroup);
    auto* destinationLayout = new QVBoxLayout(destinationGroup);
    sideBySide->addWidget(sourceGroup, 1);
    sideBySide->addWidget(destinationGroup, 1);
    layout->addLayout(sideBySide);
    auto* sourceCombo = new QComboBox(sourceGroup);
    auto* destinationCombo = new QComboBox(destinationGroup);
    sourceCombo->setObjectName("cargoSourceCombo");
    destinationCombo->setObjectName("cargoDestinationCombo");
    for (std::size_t index = 0; index < endpoints.size(); ++index) {
        if (endpoints[index].planet
            && endpoints[index].planet->owner != 0
            && endpoints[index].planet->owner != pendingOrders_.player) {
            continue;
        }
        sourceCombo->addItem(endpoints[index].name, static_cast<int>(index));
    }
    sourceLayout->addWidget(new QLabel("Source container", sourceGroup));
    sourceLayout->addWidget(sourceCombo);
    destinationLayout->addWidget(new QLabel("Destination container", destinationGroup));
    destinationLayout->addWidget(destinationCombo);

    int selectedFleetIndex = -1;
    for (std::size_t index = 0; index < endpoints.size(); ++index) {
        if (endpoints[index].fleet && endpoints[index].fleet->id == selectedFleetId) {
            selectedFleetIndex = static_cast<int>(index);
            break;
        }
    }
    // Never reverse the transfer merely because the ship already carries
    // cargo. Reopening the window must still show Planet -> selected Fleet,
    // with the existing manifest clearly visible on the right.
    sourceCombo->setCurrentIndex(0);
    const auto rebuildDestinations = [&](int preferred) {
        const QSignalBlocker blocker(destinationCombo);
        destinationCombo->clear();
        for (std::size_t index = 0; index < endpoints.size(); ++index) {
            if (static_cast<int>(index) != sourceCombo->currentData().toInt()) {
                destinationCombo->addItem(endpoints[index].name, static_cast<int>(index));
                if (endpoints[index].planet
                    && endpoints[index].planet->owner != 0
                    && endpoints[index].planet->owner != pendingOrders_.player) {
                    destinationCombo->setItemData(
                        destinationCombo->count() - 1, QColor("#e08d7c"), Qt::ForegroundRole);
                }
            }
        }
        const auto preferredRow = destinationCombo->findData(preferred);
        destinationCombo->setCurrentIndex(preferredRow >= 0 ? preferredRow : 0);
    };
    rebuildDestinations(selectedFleetIndex >= 0 ? selectedFleetIndex : 0);

    auto* transferGrid = new QGridLayout;
    transferGrid->addWidget(new QLabel("Cargo", &dialog), 0, 0);
    transferGrid->addWidget(new QLabel("Amount", &dialog), 0, 1);
    transferGrid->addWidget(new QLabel("Exact", &dialog), 0, 2);

    auto* colonistSlider = new QSlider(Qt::Horizontal, &dialog);
    auto* colonistSpin = new QSpinBox(&dialog);
    colonistSlider->setObjectName("cargoColonistSlider");
    colonistSpin->setObjectName("cargoColonistSpin");
    colonistSpin->setSingleStep(100);
    colonistSpin->setGroupSeparatorShown(true);
    colonistSpin->setToolTip("People. 100 colonists occupy 1 kt of cargo (10,000 kg per person).");
    transferGrid->addWidget(new QLabel("Colonists", &dialog), 1, 0);
    transferGrid->addWidget(colonistSlider, 1, 1);
    transferGrid->addWidget(colonistSpin, 1, 2);

    const char* mineralNames[] = {"Ironium", "Boranium", "Germanium"};
    std::vector<MineralControl> mineralControls;
    for (int index = 0; index < 3; ++index) {
        auto* slider = new QSlider(Qt::Horizontal, &dialog);
        auto* spin = new QDoubleSpinBox(&dialog);
        slider->setObjectName(QString("cargoMineralSlider%1").arg(index));
        spin->setObjectName(QString("cargoMineralSpin%1").arg(index));
        spin->setDecimals(2);
        spin->setSingleStep(1.0);
        transferGrid->addWidget(new QLabel(mineralNames[index], &dialog), index + 2, 0);
        transferGrid->addWidget(slider, index + 2, 1);
        transferGrid->addWidget(spin, index + 2, 2);
        mineralControls.push_back({slider, spin});
    }
    sourceLayout->addLayout(transferGrid);
    auto* receivingGrid = new QGridLayout;
    receivingGrid->addWidget(new QLabel("Cargo after order", destinationGroup), 0, 0);
    receivingGrid->addWidget(new QLabel("Destination stock", destinationGroup), 0, 1);
    auto* receivedColonists = new QSlider(Qt::Horizontal, destinationGroup);
    receivedColonists->setObjectName("cargoDestinationColonistSlider");
    receivingGrid->addWidget(new QLabel("Colonists", destinationGroup), 1, 0);
    receivingGrid->addWidget(receivedColonists, 1, 1);
    std::vector<QSlider*> receivedMinerals;
    for (int index = 0; index < 3; ++index) {
        auto* slider = new QSlider(Qt::Horizontal, destinationGroup);
        slider->setObjectName(QString("cargoDestinationMineralSlider%1").arg(index));
        receivingGrid->addWidget(new QLabel(mineralNames[index], destinationGroup), index + 2, 0);
        receivingGrid->addWidget(slider, index + 2, 1);
        receivedMinerals.push_back(slider);
    }
    destinationLayout->addLayout(receivingGrid);

    auto* sourceBefore = new QLabel(&dialog);
    auto* destinationBefore = new QLabel(&dialog);
    auto* sourceAfter = new QLabel(&dialog);
    auto* destinationAfter = new QLabel(&dialog);
    auto* capacitySummary = new QLabel(&dialog);
    auto* validation = new QLabel(&dialog);
    for (auto* label : {sourceBefore, destinationBefore, sourceAfter, destinationAfter, capacitySummary, validation}) label->setWordWrap(true);
    sourceBefore->setObjectName("cargoSourceBeforeLabel");
    destinationBefore->setObjectName("cargoDestinationBeforeLabel");
    sourceAfter->setObjectName("cargoSourceAfterLabel");
    destinationAfter->setObjectName("cargoDestinationAfterLabel");
    capacitySummary->setObjectName("cargoCapacityLabel");
    sourceLayout->addWidget(sourceBefore);
    sourceLayout->addWidget(sourceAfter);
    destinationLayout->addWidget(destinationBefore);
    destinationLayout->addWidget(destinationAfter);
    destinationLayout->addWidget(capacitySummary);
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
        if (sourceCombo->currentIndex() < 0 || destinationCombo->currentIndex() < 0) return;
        const auto sourceIndex = sourceCombo->currentData().toInt();
        const auto destinationIndex = destinationCombo->currentData().toInt();
        const auto& source = endpoints[static_cast<std::size_t>(sourceIndex)];
        const auto& destination = endpoints[static_cast<std::size_t>(destinationIndex)];
        const bool invasion = destination.planet
            && destination.planet->owner != 0
            && destination.planet->owner != pendingOrders_.player;

        // Every cargo type shares the same hold. Its right edge is the amount
        // that fits after existing cargo and the other selected transfers.
        const auto freeHold = destination.fleet
            ? std::max(0.0, fleet_cargo_capacity(planned, *destination.fleet)
                - fleet_cargo_used(planned, *destination.fleet))
            : std::numeric_limits<double>::infinity();
        const auto sliderHold = destination.fleet ? freeHold
            : source.fleet?fleet_cargo_capacity(planned,*source.fleet):colonist_cargo_mass(kOrbitalPopulationCapacity);
        auto availableColonists = endpointColonists(source);
        if (source.planet && !source.endpoint.orbital && !source.planet->habitat.population && !destination.endpoint.orbital) {
            availableColonists = source.planet->owner == pendingOrders_.player && availableColonists > 0
                ? availableColonists - 1 : 0;
        }
        if (destination.planet && destination.planet->owner == 0) availableColonists = 0;
        const bool orbital=source.endpoint.orbital || destination.endpoint.orbital;
        const auto orbitalRoom=destination.endpoint.orbital?kOrbitalPopulationCapacity-destination.planet->habitat.population:std::numeric_limits<std::uint64_t>::max();
        const auto colonistRoom = std::max(0.0, freeHold - mineral_cargo_mass(transferMinerals()));
        const auto colonistMaximum = static_cast<int>(std::min({
            static_cast<double>(std::min(availableColonists,orbitalRoom)),
            std::floor(colonistRoom * kColonistsPerCargoUnit + kEpsilon),
            static_cast<double>(std::numeric_limits<int>::max())}));
        const auto colonistScale = static_cast<int>(std::min(
            std::floor(std::max(0.0, sliderHold - mineral_cargo_mass(transferMinerals()))
                * kColonistsPerCargoUnit + kEpsilon),
            static_cast<double>(std::numeric_limits<int>::max())));
        {
            const QSignalBlocker blockSlider(colonistSlider), blockSpin(colonistSpin);
            colonistSpin->setRange(0, colonistMaximum);
            colonistSlider->setRange(0, colonistScale);
            colonistSlider->setValue(colonistSpin->value());
            colonistSlider->setToolTip(QString("Hold scale: %1 people; available to transfer: %2 people")
                .arg(colonistScale).arg(colonistMaximum));
        }
        const auto availableMinerals = (invasion || orbital) ? MineralCargo{} : endpointMinerals(source);
        for (int index = 0; index < 3; ++index) {
            const auto selected = transferMinerals();
            const auto otherCargo = colonist_cargo_mass(static_cast<std::uint64_t>(colonistSpin->value()))
                + mineral_cargo_mass(selected) - mineralValue(selected, index);
            const auto maximum = sliderMaximum(std::min(
                std::max(0.0, mineralValue(availableMinerals, index)), std::max(0.0, freeHold - otherCargo)));
            const auto& control = mineralControls[index];
            const QSignalBlocker blockSlider(control.slider), blockSpin(control.spin);
            control.spin->setRange(0.0, static_cast<double>(maximum) / kMineralScale);
            control.slider->setRange(
                0,
                invasion ? 0 : sliderMaximum(std::max(0.0, sliderHold - otherCargo)));
            control.slider->setValue(static_cast<int>(std::lround(control.spin->value() * kMineralScale)));
            control.slider->setEnabled(!invasion && !orbital);
            control.spin->setEnabled(!invasion && !orbital);
            control.slider->setToolTip(QString("Hold scale: %1 kt; available to transfer: %2 kt")
                .arg(static_cast<double>(control.slider->maximum()) / kMineralScale, 0, 'f', 2)
                .arg(control.spin->maximum(), 0, 'f', 2));
        }
        // Destination-side sliders show the *entire* resulting cargo,
        // rather than resetting to zero whenever the dialog is reopened.
        // For a full 2500-person ship its handle stays at the far right.
        const auto destinationPeople = endpointColonists(destination);
        const auto displayedPeople = std::min<std::uint64_t>(
            std::numeric_limits<int>::max(), destinationPeople);
        const auto peopleMax = static_cast<int>(std::min<std::uint64_t>(
            std::numeric_limits<int>::max(), destinationPeople
                + static_cast<std::uint64_t>(colonistMaximum)));
        {
            const QSignalBlocker blocker(receivedColonists);
            receivedColonists->setRange(0, peopleMax);
            receivedColonists->setValue(static_cast<int>(std::min<std::uint64_t>(
                peopleMax, displayedPeople + static_cast<std::uint64_t>(colonistSpin->value()))));
            receivedColonists->setToolTip(QString("Destination stock after queued transfers: %1 people")
                .arg(static_cast<qulonglong>(destinationPeople + colonistSpin->value())));
        }
        for (int index = 0; index < 3; ++index) {
            const auto before = mineralValue(endpointMinerals(destination), index);
            const auto after = before + mineralControls[index].spin->value();
            const auto totalMax = sliderMaximum(before + mineralControls[index].spin->maximum());
            const QSignalBlocker blocker(receivedMinerals[index]);
            receivedMinerals[index]->setRange(0, totalMax);
            receivedMinerals[index]->setValue(sliderMaximum(after));
            receivedMinerals[index]->setToolTip(QString("Destination stock: %1 kt").arg(after, 0, 'f', 2));
        }
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

        sourceBefore->setText(QString("<b>Current source (including queued orders):</b> %1")
            .arg(cargoValues(sourceCurrentColonists, sourceCurrentMinerals)));
        destinationBefore->setText(QString("<b>Current destination (including queued orders):</b> %1")
            .arg(cargoValues(destinationCurrentColonists, destinationCurrentMinerals)));
        sourceAfter->setText(QString("<b>Source after this order:</b> %1 — %2")
            .arg(source.name)
            .arg(cargoValues(sourceCurrentColonists - colonists, sourceRemainder)));
        if (invasion) {
            const auto chance = ground_invasion_success_chance(
                colonists, destinationCurrentColonists) * 100.0;
            destinationAfter->setText(QString(
                "<b>Ground invasion:</b> %1 attackers vs %2 defenders — estimated success chance %3%. "
                "If orbit is clear, the selected landing force is committed and the winner keeps its survivors. "
                "Any hostile station blocks landing, even without weapons; colonists remain aboard.")
                .arg(static_cast<qulonglong>(colonists))
                .arg(static_cast<qulonglong>(destinationCurrentColonists))
                .arg(chance, 0, 'f', 1));
        } else {
            destinationAfter->setText(QString("<b>Destination after this order:</b> %1 — %2")
                .arg(destination.name)
                .arg(cargoValues(destinationCurrentColonists + colonists, destinationResult)));
        }

        bool capacityValid = true;
        if (destination.fleet) {
            const auto added = colonist_cargo_mass(colonists) + mineral_cargo_mass(minerals);
            const auto used = fleet_cargo_used(planned, *destination.fleet) + added;
            const auto capacity = fleet_cargo_capacity(planned, *destination.fleet);
            const auto grossMass = fleet_gross_mass(planned, *destination.fleet) + added;
            capacityValid = used <= capacity + kEpsilon;
            capacitySummary->setText(QString("<b>Destination hold:</b> %1 / %2 kt • gross mass %3 kt")
                .arg(used, 0, 'f', 2)
                .arg(capacity, 0, 'f', 2)
                .arg(grossMass, 0, 'f', 2));
        } else {
            capacitySummary->setText(destination.endpoint.orbital
                ?QString("<b>Orbital housing:</b> %1 / %2 residents after transfer").arg(qulonglong(destinationCurrentColonists+colonists)).arg(qulonglong(kOrbitalPopulationCapacity))
                :"<b>Destination hold:</b> planetary surface has no cargo-capacity limit.");
        }

        const bool distinct = sourceIndex != destinationIndex;
        const bool hasCargo = colonists > 0 || mineral_cargo_mass(minerals) > kEpsilon;
        auto validationState=planned;
        const PlayerOrders validationOrders{pendingOrders_.player,{TransferCargoOrder{source.endpoint,destination.endpoint,colonists,minerals}}};
        const auto validated=projected_cargo_order_state(validationState,validationOrders);
        const auto* targetPlanet=destination.planet?find_planet(validated,destination.planet->id):nullptr;
        const auto* sourcePlanet=source.planet?find_planet(validated,source.planet->id):nullptr;
        const bool orbitalValid=!orbital || (colonists>0 && ((destination.endpoint.orbital && targetPlanet && targetPlanet->habitat.population==destinationCurrentColonists+colonists)
            || (source.endpoint.orbital && sourcePlanet && sourcePlanet->habitat.population+colonists==sourceCurrentColonists)));
        const bool valid = orbitalValid && distinct && hasCargo && capacityValid
            && (!invasion || (colonists > 0 && mineral_cargo_mass(minerals) <= kEpsilon));
        acceptButton->setEnabled(valid);
        acceptButton->setText(invasion ? "Queue invasion" : "Queue transfer");
        if (!distinct) {
            validation->setText("<span style='color:#e4b77d'><b>Choose two different endpoints.</b></span>");
        } else if (!hasCargo) {
            validation->setText("Choose at least one cargo amount to transfer.");
        } else if (invasion && colonists == 0) {
            validation->setText("<span style='color:#e08d7c'><b>An invasion requires colonists.</b></span>");
        } else if (invasion && mineral_cargo_mass(minerals) > kEpsilon) {
            validation->setText("<span style='color:#e08d7c'><b>Minerals cannot be transferred during an invasion.</b></span>");
        } else if (!capacityValid) {
            validation->setText("<span style='color:#e08d7c'><b>The destination fleet would exceed its shared cargo capacity.</b></span>");
        } else if (invasion) {
            validation->setText("<span style='color:#e08d7c'><b>This order starts ground combat against the colony.</b></span>");
        } else {
            validation->setText("<span style='color:#85d5a5'><b>Transfer is valid.</b></span>");
        }
    };

    const auto configureSource = [&] {
        {
            const QSignalBlocker blockSlider(colonistSlider);
            const QSignalBlocker blockSpin(colonistSpin);
            colonistSlider->setValue(0);
            colonistSpin->setValue(0);
        }
        for (int index = 0; index < 3; ++index) {
            const QSignalBlocker blockSlider(mineralControls[index].slider);
            const QSignalBlocker blockSpin(mineralControls[index].spin);
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
    // Adjusting the destination slider changes the requested extra load;
    // the already queued manifest remains the immutable baseline.
    connect(receivedColonists, &QSlider::valueChanged, &dialog, [=](int amount) {
        const auto destinationIndex = destinationCombo->currentData().toInt();
        if (destinationIndex < 0) return;
        const auto current = endpointColonists(endpoints[static_cast<std::size_t>(destinationIndex)]);
        const auto delta = static_cast<std::int64_t>(amount) - static_cast<std::int64_t>(current);
        colonistSpin->setValue(static_cast<int>(std::clamp<std::int64_t>(
            delta, 0, colonistSpin->maximum())));
    });
    for (int index = 0; index < 3; ++index) {
        connect(receivedMinerals[index], &QSlider::valueChanged, &dialog, [=](int amount) {
            const auto destinationIndex = destinationCombo->currentData().toInt();
            if (destinationIndex < 0) return;
            const auto before = mineralValue(
                endpointMinerals(endpoints[static_cast<std::size_t>(destinationIndex)]), index);
            const auto request = std::max(0.0, static_cast<double>(amount) / kMineralScale - before);
            mineralControls[index].spin->setValue(request);
        });
    }
    connect(sourceCombo, &QComboBox::currentIndexChanged, &dialog, [=](int) {
        rebuildDestinations(destinationCombo->currentData().toInt());
        configureSource();
    });
    connect(destinationCombo, &QComboBox::currentIndexChanged, &dialog, [=](int) { configureSource(); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    configureSource();
    if (dialog.exec() != QDialog::Accepted) return;

    const auto& source = endpoints[static_cast<std::size_t>(sourceCombo->currentData().toInt())];
    const auto& destination = endpoints[static_cast<std::size_t>(destinationCombo->currentData().toInt())];
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
