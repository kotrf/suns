#include "suns/orbital_habitats.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/megaprojects.hpp"
#include "main_window.hpp"
#include "suns/communications.hpp"
#include "suns/campaign.hpp"
#include "suns/mobile_base.hpp"
#include "suns/native_orbital.hpp"

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
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace suns {
namespace {

constexpr int kMineralScale = 100;
constexpr double kMineralStep = 1.0 / kMineralScale;
constexpr double kFuelStep = 0.1;
constexpr double kEpsilon = 0.000001;

struct CargoEndpointView {
    CargoTransferEndpoint endpoint;
    const Planet* planet{};
    const Fleet* fleet{};
    QString name;
    bool nativeOrbit{};
    std::uint64_t housingCapacity{kOrbitalPopulationCapacity};
};

struct CargoTrack {
    QSlider* slider{};
    double amountPerTick{1.0};

    [[nodiscard]] int tick(double amount) const
    {
        constexpr double limit = std::numeric_limits<int>::max() / 2;
        return static_cast<int>(std::round(std::clamp(amount / amountPerTick, -limit, limit)));
    }

    [[nodiscard]] double amount(int position) const { return position * amountPerTick; }

    void show(double minimum, double maximum, double value, double step)
    {
        constexpr double limit = std::numeric_limits<int>::max() / 2;
        amountPerTick = std::max(step, std::max(-minimum, maximum) / limit);
        const QSignalBlocker block(slider);
        slider->setRange(tick(minimum), tick(maximum));
        slider->setValue(tick(std::clamp(value, minimum, maximum)));
    }

    void showStock(double capacity, double stock, double step)
    {
        if (capacity > 0.0) show(0, capacity, stock, std::min(step, capacity));
        else show(0, step, stock > 0.0 ? step : 0.0, step);
    }
};

struct MineralControl {
    CargoTrack transfer;
    QDoubleSpinBox* spin{};
    CargoTrack destination;
    QLabel* stock{};
};

double transferScale(double receivingCapacity, double sendingCapacity, double available)
{
    if (std::isfinite(receivingCapacity)) return receivingCapacity;
    return std::isfinite(sendingCapacity) ? sendingCapacity : available;
}

double displayedFuelLimit(double amount)
{
    return amount > 0.0 ? std::max(kFuelStep, std::round(amount * 10.0) * kFuelStep) : 0.0;
}

double exactFuelDelta(double request, double limit)
{
    if (request <= 0.0) return 0.0;
    return request >= displayedFuelLimit(limit) - kEpsilon ? limit : std::min(request, limit);
}

void reserveStockWidth(QLabel* label, QString largest)
{
    const auto metrics = label->fontMetrics();
    QChar widest = '0';
    for (QChar digit : QStringLiteral("123456789"))
        if (metrics.horizontalAdvance(digit) > metrics.horizontalAdvance(widest)) widest = digit;
    for (auto& ch : largest) if (ch.isDigit()) ch = widest;
    label->setFixedWidth(metrics.horizontalAdvance(largest) + 2);
}

double mineralValue(const MineralCargo& minerals, int index)
{
    if (index == 0) return minerals.ironium;
    if (index == 1) return minerals.boranium;
    return minerals.germanium;
}

std::uint64_t endpointColonists(const CargoEndpointView& endpoint)
{
    if (!endpoint.planet) return endpoint.fleet->colonists;
    if (!endpoint.endpoint.orbital) return endpoint.planet->population;
    return endpoint.nativeOrbit ? endpoint.planet->nativeOrbit.population : endpoint.planet->habitat.population;
}

MineralCargo endpointMinerals(const CargoEndpointView& endpoint)
{
    if(endpoint.endpoint.orbital && !endpoint.nativeOrbit) return {};
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

std::optional<Position> MainWindow::selectedFleetCargoLocation(QString* unavailableReason) const
{
    const auto unavailable = [unavailableReason](const QString& reason) -> std::optional<Position> {
        if (unavailableReason) *unavailableReason = reason;
        return std::nullopt;
    };
    const auto* fleet = selectedFleet();
    if (!fleet) return unavailable("Select a friendly fleet to transfer cargo.");
    // Anchor this editor to the currently received location. A queued gate
    // jump may already move the command-boundary projection elsewhere; its
    // endpoints must not silently replace the orbit where Cargo was opened.
    std::optional<Fleet> received;
    for (const auto& original : state_.fleets) if (original.id == fleet->id
        && original.owner == pendingOrders_.player && !unreported_orbital_ship(state_, original)) {
        received = fleet_player_view(state_, original);
        fleet = &*received;
        break;
    }
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
    if (!fleet_has_instant_link(fleetPlanningState(), *fleet))
        return unavailable("Cargo transfer needs a live link at the fleet's current orbit. "
            "Remote fleets can use on-arrival cargo orders.");
    const auto& planned = fleetPlanningState();
    const bool system = std::any_of(planned.stars.begin(), planned.stars.end(),
        [&](const auto& star) { return same_position(star.position, fleet->position); });
    const bool partner = std::any_of(planned.fleets.begin(), planned.fleets.end(), [&](const auto& other) {
        return other.id != fleet->id && other.owner == fleet->owner && same_position(other.position, fleet->position)
            && fleet_telemetry_age(planned, other) == 0 && fleet_has_instant_link(planned, other);
    });
    if (!system && !partner) return unavailable("Cargo transfer needs a system or another friendly fleet at this position.");
    if (unavailableReason) unavailableReason->clear();
    return fleet->position;
}

const StarSystem* MainWindow::selectedFleetCargoStar(QString* unavailableReason) const
{
    const auto position = selectedFleetCargoLocation(unavailableReason);
    if (!position) return nullptr;
    const auto star = std::find_if(state_.stars.begin(), state_.stars.end(),
        [&](const auto& candidate) { return same_position(candidate.position, *position); });
    return star == state_.stars.end() ? nullptr : &*star;
}

void MainWindow::refreshCargoTransferControls()
{
    QString reason;
    const auto position = selectedFleetCargoLocation(&reason);
    const auto tooltip = position
        ? QString("Transfer cargo or fleet-to-fleet fuel at %1. Choose two fleets for fuel.")
            .arg(fleetLocationName(*position))
        : reason;
    if (auto* button = findChild<QPushButton*>("fleetTransferCargoButton")) {
        button->setEnabled(position.has_value());
        button->setToolTip(tooltip);
    }
    for (const auto* name : {"fleetTransferCargoAction", "fleetTransferCargoToolAction"}) {
        if (auto* action = findChild<QAction*>(name)) {
            action->setEnabled(position.has_value());
            action->setToolTip(tooltip);
        }
    }
}

void MainWindow::openCargoManifestDialog()
{
    QString unavailableReason;
    const auto position = selectedFleetCargoLocation(&unavailableReason);
    if (!position) {
        statusBar()->showMessage(unavailableReason, 7000);
        return;
    }
    const auto selectedFleetId = *selection_.fleet;

    // Exact command-boundary projection. Replaying End Turn here would also
    // grow population, mine ore, run combat, and move ships. Repeated dialog
    // openings must not advance the world or alter a single colonist.
    GameState planned = fleetPlanningState();
    const auto baseTransfers=session_->data_.fleetPlanningBaseTransfers;

    const auto foundStar = std::find_if(planned.stars.begin(), planned.stars.end(),
        [&](const auto& candidate) { return same_position(candidate.position, *position); });
    const auto* star = foundStar == planned.stars.end() ? nullptr : &*foundStar;

    std::vector<CargoEndpointView> endpoints;
    if (const auto* planet = star ? find_planet_at_star(planned, star->id) : nullptr; planet) {
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
    if (const auto* p = star ? find_planet_at_star(planned, star->id) : nullptr; p && p->owner == pendingOrders_.player) {
        const auto* native = native_orbital_station(planned, *p);
        if (native || habitat_completed(*p))
            endpoints.push_back({{p->id, 0, true}, p, nullptr,
                QString(native ? "Orbital station — %1" : "Orbital settlement — %1").arg(QString::fromStdString(p->name)),
                native != nullptr, native ? native_orbital_capacity(native->hull) : kOrbitalPopulationCapacity});
    }
    for (const auto& fleet : planned.fleets) {
        if (fleet.owner != pendingOrders_.player || !same_position(fleet.position, *position)
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
    dialog.setWindowTitle(QString("Transfer cargo — %1").arg(star ? QString::fromStdString(star->name) : fleetLocationName(*position)));
    dialog.setMinimumWidth(920);

    auto* layout = new QVBoxLayout(&dialog);
    auto* introduction = new QLabel(
        "Left: choose the source and the amount to send. Right: choose the destination and "
        "see its complete projected manifest, including previously queued loads. "
        "Adjust either side to load or return cargo. Choose two fleets to transfer fuel between their tanks. "
        "Changes appear immediately in your current-turn plan.",
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
    for (int row = 0; row < sourceCombo->count(); ++row)
        if (endpoints[std::size_t(sourceCombo->itemData(row).toInt())].nativeOrbit) {
            sourceCombo->setCurrentIndex(row);
            break;
        }
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
    CargoTrack colonistTrack{colonistSlider};

    const char* mineralNames[] = {"Ironium", "Boranium", "Germanium"};
    std::array<MineralControl, 3> mineralControls{};
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
        mineralControls[index].transfer.slider = slider;
        mineralControls[index].spin = spin;
    }
    auto* fuelSlider = new QSlider(Qt::Horizontal, &dialog);
    auto* fuelSpin = new QDoubleSpinBox(&dialog);
    fuelSlider->setObjectName("cargoFuelSlider"); fuelSpin->setObjectName("cargoFuelSpin");
    fuelSpin->setDecimals(1); fuelSpin->setSingleStep(kFuelStep); fuelSpin->setSuffix(" mg");
    transferGrid->addWidget(new QLabel("Fuel", &dialog), 5, 0);
    transferGrid->addWidget(fuelSlider, 5, 1); transferGrid->addWidget(fuelSpin, 5, 2);
    CargoTrack fuelTrack{fuelSlider};
    sourceLayout->addLayout(transferGrid);
    auto* receivingGrid = new QGridLayout;
    receivingGrid->addWidget(new QLabel("Cargo", destinationGroup), 0, 0);
    receivingGrid->addWidget(new QLabel("Destination stock after transfer", destinationGroup), 0, 1, 1, 2);
    receivingGrid->setColumnStretch(1, 1);
    const auto stockLabel = [&](int row, const QString& name) {
        auto* label = new QLabel(destinationGroup);
        label->setObjectName(name);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        receivingGrid->addWidget(label, row, 2, Qt::AlignRight);
        return label;
    };
    auto* receivedColonists = new QSlider(Qt::Horizontal, destinationGroup);
    receivedColonists->setObjectName("cargoDestinationColonistSlider");
    receivingGrid->addWidget(new QLabel("Colonists", destinationGroup), 1, 0);
    receivingGrid->addWidget(receivedColonists, 1, 1);
    CargoTrack destinationPeopleTrack{receivedColonists};
    auto* colonistStock = stockLabel(1, "cargoDestinationColonistStock");
    for (int index = 0; index < 3; ++index) {
        auto* slider = new QSlider(Qt::Horizontal, destinationGroup);
        slider->setObjectName(QString("cargoDestinationMineralSlider%1").arg(index));
        receivingGrid->addWidget(new QLabel(mineralNames[index], destinationGroup), index + 2, 0);
        receivingGrid->addWidget(slider, index + 2, 1);
        mineralControls[index].destination.slider = slider;
        mineralControls[index].stock = stockLabel(index + 2, QString("cargoDestinationMineralStock%1").arg(index));
    }
    auto* receivedFuel = new QSlider(Qt::Horizontal, destinationGroup);
    receivedFuel->setObjectName("cargoDestinationFuelSlider");
    receivingGrid->addWidget(new QLabel("Fuel", destinationGroup), 5, 0);
    receivingGrid->addWidget(receivedFuel, 5, 1);
    CargoTrack destinationFuelTrack{receivedFuel};
    auto* fuelStock = stockLabel(5, "cargoDestinationFuelStock");
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

    const auto addManifest = [](QVBoxLayout* column, QLabel* before, QLabel* after, const char* name) {
        auto* scroll = new QScrollArea(column->parentWidget());
        scroll->setObjectName(name);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
        scroll->setMinimumHeight(2 * scroll->fontMetrics().lineSpacing());

        auto* contents = new QWidget;
        auto* manifestLayout = new QVBoxLayout(contents);
        manifestLayout->setContentsMargins(0, 0, 0, 0);
        manifestLayout->setSizeConstraint(QLayout::SetMinimumSize);
        manifestLayout->addWidget(before);
        manifestLayout->addWidget(after);
        manifestLayout->addStretch();
        scroll->setWidget(contents);
        column->addWidget(scroll, 1);
    };
    addManifest(sourceLayout, sourceBefore, sourceAfter, "cargoSourceManifestScroll");
    destinationLayout->addWidget(capacitySummary);
    addManifest(destinationLayout, destinationBefore, destinationAfter, "cargoDestinationManifestScroll");
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
    std::optional<TransferFleetFuelOrder> draftFuel;
    std::optional<GameState> baseServicePlan;
    std::uint64_t beforePeople{};
    MineralCargo beforeMinerals;
    double beforeFuel{}, forwardFuel{}, backFuel{};
    const auto refresh = [&](bool endpointsChanged = false) {
        if (sourceCombo->currentIndex() < 0 || destinationCombo->currentIndex() < 0) return;
        const auto sourceIndex = sourceCombo->currentData().toInt();
        const auto destinationIndex = destinationCombo->currentData().toInt();
        auto source = endpoints[static_cast<std::size_t>(sourceIndex)];
        auto destination = endpoints[static_cast<std::size_t>(destinationIndex)];
        const bool baseService=(source.fleet && mobile_base_hulls(planned,*source.fleet)>0)
            || (destination.fleet && mobile_base_hulls(planned,*destination.fleet)>0);
        if(baseService && !baseServicePlan && !baseTransfers.empty())
            baseServicePlan=projected_cargo_service_state(planned,baseTransfers);
        if(baseService && baseServicePlan) {
            const auto serviceEndpoint=[&](CargoEndpointView& e) {
                if(e.fleet)e.fleet=find_service_fleet(*baseServicePlan,e.endpoint.fleet);
                else e.planet=find_planet(*baseServicePlan,e.endpoint.planet);
            };
            serviceEndpoint(source);serviceEndpoint(destination);
        }
        const bool invasion = destination.planet && destination.planet->owner != 0
            && destination.planet->owner != pendingOrders_.player;
        const bool residentOnly = (source.endpoint.orbital && !source.nativeOrbit)
            || (destination.endpoint.orbital && !destination.nativeOrbit);
        const bool fuelAvailable = source.fleet && destination.fleet
            && !mobile_base_hulls(planned, *source.fleet) && !mobile_base_hulls(planned, *destination.fleet);
        const double sourceTank = source.fleet ? fleet_fuel_capacity(planned, *source.fleet) : 0.0;
        const double destinationTank = destination.fleet ? fleet_fuel_capacity(planned, *destination.fleet) : 0.0;
        beforeFuel = destination.fleet ? destination.fleet->fuel : 0.0;
        forwardFuel = fuelAvailable ? std::min(source.fleet->fuel, std::max(0.0, destinationTank - beforeFuel)) : 0.0;
        backFuel = fuelAvailable ? std::min(beforeFuel, std::max(0.0, sourceTank - source.fleet->fuel)) : 0.0;
        double fuelRequest;
        {
            const QSignalBlocker block(fuelSpin);
            fuelSpin->setRange(-displayedFuelLimit(backFuel), displayedFuelLimit(forwardFuel));
            fuelRequest = fuelSpin->value();
        }
        const double fuelDelta = fuelRequest < 0.0
            ? -exactFuelDelta(-fuelRequest, backFuel) : exactFuelDelta(fuelRequest, forwardFuel);
        fuelTrack.show(fuelAvailable ? -sourceTank : 0.0, fuelAvailable ? destinationTank : 0.0, fuelRequest, kFuelStep);
        // A finer stock scale keeps the empty and full stops reachable near rounded display values.
        destinationFuelTrack.showStock(destinationTank, beforeFuel + fuelDelta, kFuelStep / 100.0);
        receivedFuel->setSingleStep(std::max(1, destinationFuelTrack.tick(kFuelStep)));
        receivedFuel->setPageStep(std::max(1, destinationFuelTrack.tick(1.0)));
        fuelSlider->setEnabled(fuelAvailable);
        fuelSpin->setEnabled(fuelAvailable);
        receivedFuel->setEnabled(fuelAvailable && destinationTank > 0.0);
        const auto fuelHelp = fuelAvailable
            ? "Transfer fuel in tenths of a mg. The final available step transfers the exact amount allowed by donor stock and tank space."
            : "Choose two ordinary friendly fleets for fuel. Mobile bases use their dedicated service panel.";
        fuelSpin->setToolTip(fuelHelp);
        fuelSlider->setToolTip(QString("Full tank scale. %1").arg(fuelHelp));
        receivedFuel->setToolTip("Destination fuel after the transfer. 100% is a full tank.");
        fuelStock->setText(destination.fleet ? QString("%1 mg").arg(beforeFuel + fuelDelta, 0, 'f', 1) : QString("—"));

        const auto sourcePeople = endpointColonists(source), destinationPeople = endpointColonists(destination);
        const auto sourceStock = endpointMinerals(source), destinationStock = endpointMinerals(destination);
        beforePeople = destinationPeople;
        beforeMinerals = destinationStock;
        const double sourceHold = source.fleet ? fleet_cargo_capacity(planned, *source.fleet) : std::numeric_limits<double>::infinity();
        const double destinationHold = destination.fleet ? fleet_cargo_capacity(planned, *destination.fleet) : std::numeric_limits<double>::infinity();
        const auto mass = [](const MineralCargo& m) { return m.ironium + m.boranium + m.germanium; };
        const double sourceRoom = source.fleet ? std::max(0.0, sourceHold - fleet_cargo_used(planned, *source.fleet)) : sourceHold;
        const double destinationRoom = destination.fleet ? std::max(0.0, destinationHold - fleet_cargo_used(planned, *destination.fleet)) : destinationHold;
        const auto peopleAvailable = [&](const CargoEndpointView& from, const CargoEndpointView& to) {
            if((from.fleet && from.fleet->outpost==OutpostKind::CargoCache)
                || (to.fleet && to.fleet->outpost==OutpostKind::CargoCache))return std::uint64_t{0};
            if (native_orbital_race(planned, pendingOrders_.player)
                && ((from.planet && !from.nativeOrbit) || (to.planet && !to.nativeOrbit))) return std::uint64_t{0};
            auto people = endpointColonists(from);
            if (from.planet) {
                if (from.planet->owner != pendingOrders_.player) return std::uint64_t{0};
                if (!from.endpoint.orbital && !from.planet->habitat.population && !surface_sheltered(*from.planet) && !to.endpoint.orbital && people) --people;
            }
            if (to.planet && !to.planet->owner) return std::uint64_t{0};
            // Shelter space limits friendly unloading. For an invasion, the host
            // uses it to place survivors after combat.
            if (to.planet && to.planet->owner == pendingOrders_.player
                && !to.endpoint.orbital && surface_sheltered(*to.planet)) {
                const auto capacity = to.planet->engineering.transformation.shelterCapacity;
                const auto residents = endpointColonists(to);
                people = std::min(people, capacity - std::min(capacity, residents));
            }
            if (to.endpoint.orbital) {
                const auto residents = endpointColonists(to);
                people = std::min(people, to.housingCapacity > residents ? to.housingCapacity-residents : 0);
            }
            return people;
        };
        const auto peopleLimit = [](std::uint64_t available, double hold) {
            return static_cast<int>(std::min({double(available),
                std::floor(std::max(0.0, hold) * kColonistsPerCargoUnit + kEpsilon),
                double(std::numeric_limits<int>::max())}));
        };
        const auto mineralDelta = mass(transferMinerals());
        const int forwardPeople = peopleLimit(peopleAvailable(source, destination), destinationRoom - mineralDelta);
        const int backPeople = invasion ? 0 : peopleLimit(peopleAvailable(destination, source), sourceRoom + mineralDelta);
        const double sourcePeopleCapacity = source.endpoint.orbital ? double(source.housingCapacity) : sourceHold * kColonistsPerCargoUnit;
        const double destinationPeopleCapacity = destination.endpoint.orbital ? double(destination.housingCapacity) : destinationHold * kColonistsPerCargoUnit;
        {
            const QSignalBlocker spinBlock(colonistSpin);
            colonistSpin->setRange(-backPeople, forwardPeople);
            const double forwardScale = transferScale(destinationPeopleCapacity, sourcePeopleCapacity, double(sourcePeople));
            const double backScale = invasion ? 0.0 : transferScale(sourcePeopleCapacity, destinationPeopleCapacity, double(destinationPeople));
            colonistTrack.show(-backScale, forwardScale, colonistSpin->value(), 1.0);
            colonistSlider->setToolTip("Positive loads the destination. Negative returns colonists to the source. The scale uses the full hold or housing capacity where available.");
        }
        for (int index = 0; index < 3; ++index) {
            const auto selected = transferMinerals();
            const double otherCargo = double(colonistSpin->value()) / kColonistsPerCargoUnit
                + mass(selected) - mineralValue(selected, index);
            const int forward = invasion || residentOnly ? 0 : sliderMaximum(std::min(
                mineralValue(sourceStock, index), std::max(0.0, destinationRoom - otherCargo)));
            const int back = invasion || residentOnly ? 0 : sliderMaximum(std::min(
                mineralValue(destinationStock, index), std::max(0.0, sourceRoom + otherCargo)));
            auto& control = mineralControls[index];
            const QSignalBlocker spinBlock(control.spin);
            control.spin->setRange(-double(back) / kMineralScale, double(forward) / kMineralScale);
            const bool adjustable = !invasion && !residentOnly;
            const double forwardScale = adjustable ? transferScale(destinationHold, sourceHold, mineralValue(sourceStock, index)) : 0.0;
            const double backScale = adjustable ? transferScale(sourceHold, destinationHold, mineralValue(destinationStock, index)) : 0.0;
            control.transfer.show(-backScale, forwardScale, control.spin->value(), kMineralStep);
            control.transfer.slider->setEnabled(adjustable);
            control.spin->setEnabled(adjustable);
            control.transfer.slider->setToolTip("Positive loads the destination. Negative returns minerals to the source. The scale uses the full hold where available.");
        }
        const bool materialCache=(source.fleet && source.fleet->outpost==OutpostKind::CargoCache)
            || (destination.fleet && destination.fleet->outpost==OutpostKind::CargoCache);
        colonistSpin->setEnabled(!materialCache);
        colonistSlider->setEnabled(!materialCache);
        colonistSpin->setToolTip(materialCache ? "Cargo caches store material cargo only; no population."
            : "People. 100 colonists occupy 1 kt of cargo (10,000 kg per person).");
        const auto colonists = std::int64_t(colonistSpin->value());
        const auto minerals = transferMinerals();
        const auto peopleAfter = [](std::uint64_t before, std::int64_t delta) {
            return delta >= 0 ? before + std::uint64_t(delta) : before - std::uint64_t(-delta);
        };
        const auto sourceAfterPeople = peopleAfter(sourcePeople, -colonists);
        const auto destinationAfterPeople = peopleAfter(destinationPeople, colonists);
        const MineralCargo sourceAfterStock{sourceStock.ironium - minerals.ironium,
            sourceStock.boranium - minerals.boranium, sourceStock.germanium - minerals.germanium};
        const MineralCargo destinationAfterStock{destinationStock.ironium + minerals.ironium,
            destinationStock.boranium + minerals.boranium, destinationStock.germanium + minerals.germanium};
        const double peopleScale = std::isfinite(destinationPeopleCapacity) ? destinationPeopleCapacity : double(destinationPeople);
        destinationPeopleTrack.showStock(peopleScale, double(destinationAfterPeople), 1.0);
        receivedColonists->setEnabled(!materialCache && peopleScale > 0.0);
        receivedColonists->setToolTip(destination.fleet ? "Destination population after transfer. 100% is the full hold at 100 colonists per kt."
            : destination.endpoint.orbital ? "Destination population after transfer. 100% is the housing capacity."
            : "Destination population after transfer. 100% is the population before this transfer. Use the amount field to add above that level.");
        colonistStock->setText(QString::number(qulonglong(destinationAfterPeople)));
        for (int index = 0; index < 3; ++index) {
            const double capacity = destination.fleet ? destinationHold : mineralValue(destinationStock, index);
            const double stock = mineralValue(destinationAfterStock, index);
            auto& control = mineralControls[index];
            control.destination.showStock(capacity, stock, kMineralStep);
            control.destination.slider->setEnabled(!invasion && !residentOnly && capacity > 0.0);
            control.destination.slider->setToolTip(destination.fleet
                ? "Destination stock after transfer. 100% is the full shared hold capacity."
                : "Destination stock after transfer. 100% is the stock of this mineral before the transfer. Use the amount field to add above that level.");
            control.stock->setText(QString("%1 kt").arg(stock, 0, 'f', 2));
        }
        if (endpointsChanged) {
            reserveStockWidth(colonistStock, QString::number(double(sourcePeople) + double(destinationPeople), 'f', 0));
            for (int index = 0; index < 3; ++index) {
                const double largest = mineralValue(sourceStock, index) + mineralValue(destinationStock, index);
                reserveStockWidth(mineralControls[index].stock, QString("%1 kt").arg(largest, 0, 'f', 2));
            }
            reserveStockWidth(fuelStock, destination.fleet
                ? QString("%1 mg").arg(std::max(destinationTank, beforeFuel), 0, 'f', 1) : QString("—"));
        }
        const auto when=baseService?"After scheduled base exchanges":"Available now";
        sourceBefore->setText(QString("<b>Source — %1:</b> %2").arg(when,cargoValues(sourcePeople, sourceStock)));
        destinationBefore->setText(QString("<b>Destination — %1:</b> %2").arg(when,cargoValues(destinationPeople, destinationStock)));
        sourceAfter->setText(QString("<b>Source after this transfer:</b> %1 — %2").arg(source.name).arg(cargoValues(sourceAfterPeople, sourceAfterStock)));
        if (source.fleet) {
            sourceBefore->setText(sourceBefore->text() + QString(" • fuel %1 mg").arg(source.fleet->fuel, 0, 'f', 1));
            sourceAfter->setText(sourceAfter->text() + QString(" • fuel %1 / %2 mg")
                .arg(source.fleet->fuel - fuelDelta, 0, 'f', 1).arg(sourceTank, 0, 'f', 1));
        }
        if (invasion) {
            destinationAfter->setText(QString(
                "<b>Ground invasion:</b> %1 attackers vs %2 defenders — estimated success chance %3%. "
                "If orbit is clear, the selected landing force is committed and the winner keeps its survivors. "
                "Any hostile station blocks landing, even without weapons; colonists remain aboard.")
                .arg(qulonglong(colonists)).arg(qulonglong(destinationPeople))
                .arg(ground_invasion_success_chance(std::uint64_t(colonists), destinationPeople) * 100.0, 0, 'f', 1));
        } else destinationAfter->setText(QString("<b>Destination after this transfer:</b> %1 — %2")
            .arg(destination.name).arg(cargoValues(destinationAfterPeople, destinationAfterStock)));
        if (destination.fleet) {
            destinationBefore->setText(destinationBefore->text() + QString(" • fuel %1 mg").arg(beforeFuel, 0, 'f', 1));
            destinationAfter->setText(destinationAfter->text() + QString(" • fuel %1 / %2 mg")
                .arg(beforeFuel + fuelDelta, 0, 'f', 1).arg(destinationTank, 0, 'f', 1));
        }

        const auto addedMass = double(colonists) / kColonistsPerCargoUnit + mass(minerals);
        bool capacityValid = sourceRoom + addedMass >= -kEpsilon && destinationRoom - addedMass >= -kEpsilon;
        if (destination.fleet) {
            capacitySummary->setText(QString("<b>Destination hold:</b> %1 / %2&nbsp;kt<br><b>Gross mass:</b> %3&nbsp;kt")
                .arg(fleet_cargo_used(planned, *destination.fleet) + addedMass, 0, 'f', 2)
                .arg(destinationHold, 0, 'f', 2)
                .arg(fleet_gross_mass(planned, *destination.fleet) + addedMass, 0, 'f', 2));
        } else capacitySummary->setText(destination.endpoint.orbital
            ? QString("<b>Orbital housing:</b> %1 / %2 residents after transfer").arg(qulonglong(destinationAfterPeople)).arg(qulonglong(destination.housingCapacity))
            : "<b>Destination hold:</b> planetary surface has no cargo-capacity limit.");

        draftTransfers.clear();
        const MineralCargo forwardMinerals{std::max(0.0, minerals.ironium), std::max(0.0, minerals.boranium), std::max(0.0, minerals.germanium)};
        const MineralCargo backMinerals{std::max(0.0, -minerals.ironium), std::max(0.0, -minerals.boranium), std::max(0.0, -minerals.germanium)};
        if (colonists < 0 || mass(backMinerals) > kEpsilon)
            draftTransfers.push_back({destination.endpoint, source.endpoint, std::uint64_t(std::max<std::int64_t>(0, -colonists)), backMinerals});
        if (colonists > 0 || mass(forwardMinerals) > kEpsilon)
            draftTransfers.push_back({source.endpoint, destination.endpoint, std::uint64_t(std::max<std::int64_t>(0, colonists)), forwardMinerals});
        draftFuel.reset();
        if (fuelAvailable && fuelDelta != 0.0)
            draftFuel = TransferFleetFuelOrder{fuelDelta > 0 ? source.fleet->id : destination.fleet->id,
                fuelDelta > 0 ? destination.fleet->id : source.fleet->id, std::abs(fuelDelta), 0};
        const auto matchesPlan = [&] {
            PlayerOrders orders{pendingOrders_.player, {}};
            for (const auto& transfer : draftTransfers) orders.orders.push_back(transfer);
            if (draftFuel) orders.orders.push_back(*draftFuel);
            const auto validated=baseService
                ? projected_cargo_service_draft(baseServicePlan?*baseServicePlan:planned,pendingOrders_.player,draftTransfers)
                : projected_cargo_order_state(planned,orders);
            const auto matches = [&](const CargoEndpointView& endpoint, std::uint64_t people, const MineralCargo& stock) {
                CargoEndpointView actual = endpoint;
                actual.planet = nullptr; actual.fleet = nullptr;
                if (endpoint.fleet) {
                    const auto fleet = std::find_if(validated.fleets.begin(), validated.fleets.end(),
                        [&](const auto& f) { return f.id == endpoint.fleet->id; });
                    if (fleet != validated.fleets.end()) actual.fleet = &*fleet;
                }
                else actual.planet = find_planet(validated, endpoint.planet->id);
                if (!actual.fleet && !actual.planet) return false;
                const auto actualStock = endpointMinerals(actual);
                const auto expectedFuel = endpoint.fleet ? endpoint.fleet->fuel + (endpoint.fleet->id == destination.endpoint.fleet ? fuelDelta : -fuelDelta) : 0.0;
                return (!actual.fleet || std::abs(actual.fleet->fuel - expectedFuel) < kEpsilon)
                    && endpointColonists(actual) == people && std::abs(actualStock.ironium - stock.ironium) < kEpsilon
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
        const bool hasCargo = !draftTransfers.empty() || draftFuel.has_value();
        const bool distinct = sourceIndex != destinationIndex;
        const bool valid = distinct && hasCargo && capacityValid && validProjection
            && (!invasion || (colonists > 0 && mass(minerals) <= kEpsilon));
        acceptButton->setEnabled(valid);
        acceptButton->setText(invasion ? "Queue invasion" : "Apply transfer");
        if (!distinct) validation->setText("Choose two different endpoints.");
        else if (!hasCargo) validation->setText("Choose cargo or fuel to transfer.");
        else if (!capacityValid || !validProjection) validation->setText("This transfer cannot be completed with the current stocks, capacity and services.");
        else if (invasion) validation->setText("This order starts ground combat against the colony.");
        else validation->setText(baseService
            ? "Base exchange is scheduled after movement. Delivered cargo becomes available for ordinary Cargo next turn."
            : "Transfer is valid. Both manifests will update in your current-turn plan.");
    };

    const auto configureSource = [&] {
        { const QSignalBlocker spinBlock(fuelSpin), sliderBlock(fuelSlider); fuelSpin->setValue(0); fuelSlider->setValue(0); }
        {
            const QSignalBlocker blockSlider(colonistSlider);
            const QSignalBlocker blockSpin(colonistSpin);
            colonistSlider->setValue(0);
            colonistSpin->setValue(0);
        }
        for (int index = 0; index < 3; ++index) {
            const QSignalBlocker blockSlider(mineralControls[index].transfer.slider);
            const QSignalBlocker blockSpin(mineralControls[index].spin);
            mineralControls[index].transfer.slider->setValue(0);
            mineralControls[index].spin->setValue(0.0);
        }
        refresh(true);
    };

    const auto setPeopleAmount = [&](double amount) {
        const QSignalBlocker blocker(colonistSpin);
        colonistSpin->setValue(int(std::lround(std::clamp(amount,
            double(colonistSpin->minimum()), double(colonistSpin->maximum())))));
        refresh();
    };
    connect(colonistSlider, &QSlider::valueChanged, &dialog, [&](int value) {
        setPeopleAmount(colonistTrack.amount(value));
    });
    const auto setFuelRequest = [&](double amount) {
        const QSignalBlocker block(fuelSpin);
        fuelSpin->setValue(amount);
        refresh();
    };
    connect(fuelSlider, &QSlider::valueChanged, &dialog, [&](int value) {
        setFuelRequest(fuelTrack.amount(value));
    });
    connect(fuelSpin, &QDoubleSpinBox::valueChanged, &dialog, [&](double) { refresh(); });
    connect(receivedFuel, &QSlider::valueChanged, &dialog, [&](int value) {
        const int before = destinationFuelTrack.tick(beforeFuel);
        if (value > before && value >= destinationFuelTrack.tick(beforeFuel + forwardFuel))
            setFuelRequest(fuelSpin->maximum());
        else if (value < before && value <= destinationFuelTrack.tick(beforeFuel - backFuel))
            setFuelRequest(fuelSpin->minimum());
        else setFuelRequest(destinationFuelTrack.amount(value) - beforeFuel);
    });
    connect(colonistSpin, &QSpinBox::valueChanged, &dialog, [&](int) { refresh(); });
    // The receiving tracks show post-transfer totals. Editing one requests a
    // signed change from the effective manifest before the current draft.
    connect(receivedColonists, &QSlider::valueChanged, &dialog, [&](int value) {
        setPeopleAmount(destinationPeopleTrack.amount(value) - double(beforePeople));
    });
    for (int index = 0; index < 3; ++index) {
        auto* control = &mineralControls[index];
        connect(control->transfer.slider, &QSlider::valueChanged, &dialog, [&, control](int value) {
            const QSignalBlocker blocker(control->spin);
            control->spin->setValue(control->transfer.amount(value));
            refresh();
        });
        connect(control->spin, &QDoubleSpinBox::valueChanged, &dialog, [&](double) { refresh(); });
        connect(control->destination.slider, &QSlider::valueChanged, &dialog, [&, control, index](int value) {
            const QSignalBlocker blocker(control->spin);
            control->spin->setValue(control->destination.amount(value) - mineralValue(beforeMinerals, index));
            refresh();
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
    if (draftFuel) appendPendingOrder(*draftFuel, QString("Transfer %1 mg fuel from fleet %2 to fleet %3")
        .arg(draftFuel->amount, 0, 'f', 1).arg(draftFuel->source).arg(draftFuel->destination));
}

} // namespace suns
