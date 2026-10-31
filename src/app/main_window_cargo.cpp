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
    dialog.setStyleSheet("QDialog#cargoTransferDialog { background: #0d141d; }");
    dialog.setWindowTitle(QString("Transfer cargo — %1").arg(QString::fromStdString(star->name)));
    dialog.setMinimumWidth(920);

    auto* layout = new QVBoxLayout(&dialog);
    auto* introduction = new QLabel(
        "Left: choose the source and the amount to send. Right: choose the destination and "
        "see its complete projected manifest, including previously queued loads. "
        "Adjust either side to load or return cargo. Changes appear immediately in your current-turn plan.",
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
    transferGrid->addWidget(new QLabel("Amount (negative returns)", &dialog), 0, 1);
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

    std::vector<TransferCargoOrder> draftTransfers;
    const auto refresh = [&] {
        if (sourceCombo->currentIndex() < 0 || destinationCombo->currentIndex() < 0) return;
        const auto sourceIndex = sourceCombo->currentData().toInt();
        const auto destinationIndex = destinationCombo->currentData().toInt();
        const auto& source = endpoints[static_cast<std::size_t>(sourceIndex)];
        const auto& destination = endpoints[static_cast<std::size_t>(destinationIndex)];
        const bool invasion = destination.planet && destination.planet->owner != 0
            && destination.planet->owner != pendingOrders_.player;
        const bool orbital = source.endpoint.orbital || destination.endpoint.orbital;
        const auto mass = [](const MineralCargo& m) { return m.ironium + m.boranium + m.germanium; };
        const auto room = [&](const CargoEndpointView& endpoint) {
            return endpoint.fleet ? std::max(0.0, fleet_cargo_capacity(planned, *endpoint.fleet)
                - fleet_cargo_used(planned, *endpoint.fleet)) : std::numeric_limits<double>::infinity();
        };
        const auto peopleAvailable = [&](const CargoEndpointView& from, const CargoEndpointView& to) {
            auto people = endpointColonists(from);
            if (from.planet) {
                if (from.planet->owner != pendingOrders_.player) return std::uint64_t{0};
                if (!from.endpoint.orbital && !from.planet->habitat.population && !to.endpoint.orbital && people) --people;
            }
            if (to.planet && !to.planet->owner) return std::uint64_t{0};
            if (to.endpoint.orbital) people = std::min(people, kOrbitalPopulationCapacity - to.planet->habitat.population);
            return people;
        };
        const auto peopleLimit = [](std::uint64_t available, double hold) {
            return static_cast<int>(std::min({double(available),
                std::floor(std::max(0.0, hold) * kColonistsPerCargoUnit + kEpsilon),
                double(std::numeric_limits<int>::max())}));
        };
        const double sourceRoom = room(source), destinationRoom = room(destination);
        const auto mineralDelta = mass(transferMinerals());
        const int forwardPeople = peopleLimit(peopleAvailable(source, destination), destinationRoom - mineralDelta);
        const int backPeople = invasion ? 0 : peopleLimit(peopleAvailable(destination, source), sourceRoom + mineralDelta);
        {
            const QSignalBlocker sliderBlock(colonistSlider), spinBlock(colonistSpin);
            colonistSpin->setRange(-backPeople, forwardPeople);
            const auto holdScale = destination.fleet ? destinationRoom
                : source.fleet ? fleet_cargo_capacity(planned, *source.fleet) : colonist_cargo_mass(kOrbitalPopulationCapacity);
            const int forwardScale = std::max(forwardPeople, peopleLimit(std::numeric_limits<std::uint64_t>::max(), holdScale - mineralDelta));
            colonistSlider->setRange(-backPeople, forwardScale);
            colonistSlider->setValue(colonistSpin->value());
            colonistSlider->setToolTip("Positive loads the destination; negative returns colonists to the source.");
        }
        for (int index = 0; index < 3; ++index) {
            const auto selected = transferMinerals();
            const double otherCargo = double(colonistSpin->value()) / kColonistsPerCargoUnit
                + mass(selected) - mineralValue(selected, index);
            const int forward = invasion || orbital ? 0 : sliderMaximum(std::min(
                mineralValue(endpointMinerals(source), index), std::max(0.0, destinationRoom - otherCargo)));
            const int back = invasion || orbital ? 0 : sliderMaximum(std::min(
                mineralValue(endpointMinerals(destination), index), std::max(0.0, sourceRoom + otherCargo)));
            const auto& control = mineralControls[index];
            const QSignalBlocker sliderBlock(control.slider), spinBlock(control.spin);
            control.spin->setRange(-double(back) / kMineralScale, double(forward) / kMineralScale);
            const auto holdScale = destination.fleet ? destinationRoom
                : source.fleet ? fleet_cargo_capacity(planned, *source.fleet) : colonist_cargo_mass(kOrbitalPopulationCapacity);
            control.slider->setRange(-back, invasion || orbital ? 0
                : std::max(forward, sliderMaximum(std::max(0.0, holdScale - otherCargo))));
            control.slider->setValue(int(std::lround(control.spin->value() * kMineralScale)));
            control.slider->setEnabled(!invasion && !orbital);
            control.spin->setEnabled(!invasion && !orbital);
            control.slider->setToolTip("Positive loads the destination; negative returns minerals to the source.");
        }
        const auto colonists = std::int64_t(colonistSpin->value());
        const auto minerals = transferMinerals();
        const auto sourcePeople = endpointColonists(source), destinationPeople = endpointColonists(destination);
        const auto sourceStock = endpointMinerals(source), destinationStock = endpointMinerals(destination);
        const auto peopleAfter = [](std::uint64_t before, std::int64_t delta) {
            return delta >= 0 ? before + std::uint64_t(delta) : before - std::uint64_t(-delta);
        };
        const auto sourceAfterPeople = peopleAfter(sourcePeople, -colonists);
        const auto destinationAfterPeople = peopleAfter(destinationPeople, colonists);
        const MineralCargo sourceAfterStock{sourceStock.ironium - minerals.ironium,
            sourceStock.boranium - minerals.boranium, sourceStock.germanium - minerals.germanium};
        const MineralCargo destinationAfterStock{destinationStock.ironium + minerals.ironium,
            destinationStock.boranium + minerals.boranium, destinationStock.germanium + minerals.germanium};
        const auto boundedPeople = [](std::uint64_t n) { return int(std::min<std::uint64_t>(n, std::numeric_limits<int>::max())); };
        {
            const QSignalBlocker blocker(receivedColonists);
            receivedColonists->setRange(boundedPeople(destinationPeople - backPeople), boundedPeople(destinationPeople + forwardPeople));
            receivedColonists->setValue(boundedPeople(destinationAfterPeople));
            receivedColonists->setToolTip("Destination population after this transfer; drag left to return colonists.");
        }
        for (int index = 0; index < 3; ++index) {
            const double before = mineralValue(destinationStock, index);
            const auto& control = mineralControls[index];
            const QSignalBlocker blocker(receivedMinerals[index]);
            receivedMinerals[index]->setRange(sliderMaximum(before + control.spin->minimum()),
                sliderMaximum(before + control.spin->maximum()));
            receivedMinerals[index]->setValue(sliderMaximum(before + control.spin->value()));
            receivedMinerals[index]->setEnabled(!invasion && !orbital);
            receivedMinerals[index]->setToolTip("Destination stock after this transfer; drag left to return minerals.");
        }
        sourceBefore->setText(QString("<b>Current source (including queued orders):</b> %1").arg(cargoValues(sourcePeople, sourceStock)));
        destinationBefore->setText(QString("<b>Current destination (including queued orders):</b> %1").arg(cargoValues(destinationPeople, destinationStock)));
        sourceAfter->setText(QString("<b>Source after this transfer:</b> %1 — %2").arg(source.name).arg(cargoValues(sourceAfterPeople, sourceAfterStock)));
        if (invasion) {
            destinationAfter->setText(QString(
                "<b>Ground invasion:</b> %1 attackers vs %2 defenders — estimated success chance %3%. "
                "If orbit is clear, the selected landing force is committed and the winner keeps its survivors. "
                "Any hostile station blocks landing, even without weapons; colonists remain aboard.")
                .arg(qulonglong(colonists)).arg(qulonglong(destinationPeople))
                .arg(ground_invasion_success_chance(std::uint64_t(colonists), destinationPeople) * 100.0, 0, 'f', 1));
        } else destinationAfter->setText(QString("<b>Destination after this transfer:</b> %1 — %2")
            .arg(destination.name).arg(cargoValues(destinationAfterPeople, destinationAfterStock)));

        const auto addedMass = double(colonists) / kColonistsPerCargoUnit + mass(minerals);
        bool capacityValid = sourceRoom + addedMass >= -kEpsilon && destinationRoom - addedMass >= -kEpsilon;
        if (destination.fleet) {
            capacitySummary->setText(QString("<b>Destination hold:</b> %1 / %2 kt • gross mass %3 kt")
                .arg(fleet_cargo_used(planned, *destination.fleet) + addedMass, 0, 'f', 2)
                .arg(fleet_cargo_capacity(planned, *destination.fleet), 0, 'f', 2)
                .arg(fleet_gross_mass(planned, *destination.fleet) + addedMass, 0, 'f', 2));
        } else capacitySummary->setText(destination.endpoint.orbital
            ? QString("<b>Orbital housing:</b> %1 / %2 residents after transfer").arg(qulonglong(destinationAfterPeople)).arg(qulonglong(kOrbitalPopulationCapacity))
            : "<b>Destination hold:</b> planetary surface has no cargo-capacity limit.");

        draftTransfers.clear();
        const MineralCargo forwardMinerals{std::max(0.0, minerals.ironium), std::max(0.0, minerals.boranium), std::max(0.0, minerals.germanium)};
        const MineralCargo backMinerals{std::max(0.0, -minerals.ironium), std::max(0.0, -minerals.boranium), std::max(0.0, -minerals.germanium)};
        if (colonists < 0 || mass(backMinerals) > kEpsilon)
            draftTransfers.push_back({destination.endpoint, source.endpoint, std::uint64_t(std::max<std::int64_t>(0, -colonists)), backMinerals});
        if (colonists > 0 || mass(forwardMinerals) > kEpsilon)
            draftTransfers.push_back({source.endpoint, destination.endpoint, std::uint64_t(std::max<std::int64_t>(0, colonists)), forwardMinerals});
        const auto matchesPlan = [&] {
            PlayerOrders orders{pendingOrders_.player, {}};
            for (const auto& transfer : draftTransfers) orders.orders.push_back(transfer);
            const auto validated = projected_cargo_order_state(planned, orders);
            const auto matches = [&](const CargoEndpointView& endpoint, std::uint64_t people, const MineralCargo& stock) {
                CargoEndpointView actual{endpoint.endpoint};
                if (endpoint.fleet) {
                    const auto fleet = std::find_if(validated.fleets.begin(), validated.fleets.end(),
                        [&](const auto& f) { return f.id == endpoint.fleet->id; });
                    if (fleet != validated.fleets.end()) actual.fleet = &*fleet;
                }
                else actual.planet = find_planet(validated, endpoint.planet->id);
                if (!actual.fleet && !actual.planet) return false;
                const auto actualStock = endpointMinerals(actual);
                return endpointColonists(actual) == people && std::abs(actualStock.ironium - stock.ironium) < kEpsilon
                    && std::abs(actualStock.boranium - stock.boranium) < kEpsilon
                    && std::abs(actualStock.germanium - stock.germanium) < kEpsilon;
            };
            return matches(source, sourceAfterPeople, sourceAfterStock) && matches(destination, destinationAfterPeople, destinationAfterStock);
        };
        bool validProjection = invasion || matchesPlan();
        if (!validProjection && draftTransfers.size() == 2) {
            std::reverse(draftTransfers.begin(), draftTransfers.end());
            validProjection = matchesPlan();
        }
        const bool hasCargo = !draftTransfers.empty();
        const bool distinct = sourceIndex != destinationIndex;
        const bool valid = distinct && hasCargo && capacityValid && validProjection
            && (!invasion || (colonists > 0 && mass(minerals) <= kEpsilon));
        acceptButton->setEnabled(valid);
        acceptButton->setText(invasion ? "Queue invasion" : "Apply transfer");
        if (!distinct) validation->setText("Choose two different endpoints.");
        else if (!hasCargo) validation->setText("Choose at least one cargo amount to transfer.");
        else if (!capacityValid || !validProjection) validation->setText("This transfer cannot be completed with the current stocks, capacity and services.");
        else if (invasion) validation->setText("This order starts ground combat against the colony.");
        else validation->setText("Transfer is valid. Both manifests will update in your current-turn plan.");
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
    // Signed deltas let either container return already queued cargo without
    // editing the physical state or requiring a turn to pass.
    connect(receivedColonists, &QSlider::valueChanged, &dialog, [=](int amount) {
        const auto destinationIndex = destinationCombo->currentData().toInt();
        if (destinationIndex < 0) return;
        const auto current = endpointColonists(endpoints[static_cast<std::size_t>(destinationIndex)]);
        const auto delta = static_cast<std::int64_t>(amount) - static_cast<std::int64_t>(current);
        colonistSpin->setValue(static_cast<int>(std::clamp<std::int64_t>(
            delta, colonistSpin->minimum(), colonistSpin->maximum())));
    });
    for (int index = 0; index < 3; ++index) {
        connect(receivedMinerals[index], &QSlider::valueChanged, &dialog, [=](int amount) {
            const auto destinationIndex = destinationCombo->currentData().toInt();
            if (destinationIndex < 0) return;
            const auto before = mineralValue(
                endpointMinerals(endpoints[static_cast<std::size_t>(destinationIndex)]), index);
            const auto request = static_cast<double>(amount) / kMineralScale - before;
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

    for (const auto& transfer : draftTransfers) {
        const auto endpointName = [&](const CargoTransferEndpoint& endpoint) {
            const auto found = std::find_if(endpoints.begin(), endpoints.end(), [&](const auto& view) {
                return view.endpoint.planet == endpoint.planet && view.endpoint.fleet == endpoint.fleet
                    && view.endpoint.orbital == endpoint.orbital;
            });
            return found == endpoints.end() ? QString{} : found->name;
        };
        appendPendingOrder(transfer, QString("Transfer cargo from %1 to %2 — colonists %3, I %4, B %5, G %6")
            .arg(endpointName(transfer.source)).arg(endpointName(transfer.destination))
            .arg(qulonglong(transfer.colonists)).arg(transfer.minerals.ironium, 0, 'f', 2)
            .arg(transfer.minerals.boranium, 0, 'f', 2).arg(transfer.minerals.germanium, 0, 'f', 2));
    }
}

} // namespace suns
