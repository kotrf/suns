#include "ship_designer_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>

namespace suns {

namespace {

template <typename Enum>
void addEnumItem(QComboBox* combo, const QString& text, Enum value)
{
    combo->addItem(text, static_cast<int>(value));
}

QString signedFuelRate(double value)
{
    if (value < -0.000001) return QString("%1 (gain)").arg(-value, 0, 'f', 2);
    if (value > 0.000001) return QString::number(value, 'f', 2);
    return "0.00";
}

} // namespace

ShipDesignerDialog::ShipDesignerDialog(const GameState& state, PlayerId player, QWidget* parent)
    : QDialog(parent)
    , player_(player)
{
    setWindowTitle("Suns! — Ship Designer");
    resize(560, 600);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(
        "Choose a hull, one engine and equipment for its general slots. "
        "Every fitted component adds mass, production cost and a mineral bill; fuel use later scales with gross ship mass.",
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    nameEdit_ = new QLineEdit("New Design", this);
    nameEdit_->setMaxLength(48);
    form->addRow("Design name", nameEdit_);

    hullCombo_ = new QComboBox(this);
    addEnumItem(hullCombo_, "Scout Hull", ShipHullType::Scout);
    addEnumItem(hullCombo_, "Light Transport", ShipHullType::LightTransport);
    addEnumItem(hullCombo_, "Medium Transport", ShipHullType::MediumTransport);
    form->addRow("Hull", hullCombo_);

    engineCombo_ = new QComboBox(this);
    addEnumItem(engineCombo_, "Fusion Drive", ShipComponentType::FusionDrive);
    addEnumItem(engineCombo_, "Ram Scoop Drive", ShipComponentType::RamScoopDrive);
    addEnumItem(engineCombo_, "Radiating Ram Scoop", ShipComponentType::RadiatingRamScoopDrive);
    form->addRow("Engine slot", engineCombo_);

    scannerCount_ = new QSpinBox(this);
    compactScannerCount_ = new QSpinBox(this);
    penetratingScannerCount_ = new QSpinBox(this);
    remoteMiningModuleCount_ = new QSpinBox(this);
    colonyModuleCount_ = new QSpinBox(this);
    fuelTankCount_ = new QSpinBox(this);
    cargoPodCount_ = new QSpinBox(this);
    antimatterCount_ = new QSpinBox(this);
    for (auto* spin : {scannerCount_, compactScannerCount_, penetratingScannerCount_, remoteMiningModuleCount_,
             colonyModuleCount_, fuelTankCount_, cargoPodCount_, antimatterCount_}) {
        spin->setRange(0, 5);
    }
    scannerCount_->setValue(1);

    form->addRow("Long Range Scanner", scannerCount_);
    const auto compactAvailable = component_available_to_player(
        state, player, ShipComponentType::CompactLongRangeScanner);
    compactScannerCount_->setEnabled(compactAvailable);
    compactScannerCount_->setToolTip(compactAvailable
        ? "Electronics 1: lighter and cheaper, with a shorter field"
        : "Locked — requires Electronics 1");
    form->addRow("Compact Scanner (E1)", compactScannerCount_);
    const auto penetratingAvailable = component_available_to_player(
        state, player, ShipComponentType::PenetratingScanner);
    penetratingScannerCount_->setEnabled(penetratingAvailable);
    penetratingScannerCount_->setToolTip(penetratingAvailable
        ? "Approximate planetary conditions without entering orbit"
        : "Locked — requires Electronics 3");
    form->addRow("Penetrating Scanner (E3)", penetratingScannerCount_);
    const auto remoteMiningAvailable = component_available_to_player(
        state, player, ShipComponentType::RemoteMiningModule);
    remoteMiningModuleCount_->setEnabled(remoteMiningAvailable);
    remoteMiningModuleCount_->setToolTip(remoteMiningAvailable
        ? "Construction 1: mines uncolonized worlds into their surface stockpiles"
        : "Locked — requires Construction 1");
    form->addRow("Remote Mining Module (C1)", remoteMiningModuleCount_);
    form->addRow("Colony Module", colonyModuleCount_);
    form->addRow("Fuel Tank", fuelTankCount_);
    form->addRow("Cargo Pod", cargoPodCount_);
    form->addRow("Antimatter Generator", antimatterCount_);
    layout->addLayout(form);

    previewLabel_ = new QLabel(this);
    previewLabel_->setWordWrap(true);
    previewLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(previewLabel_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    saveButton_ = buttons->button(QDialogButtonBox::Save);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(nameEdit_, &QLineEdit::textChanged, this, [this] { updatePreview(); });
    connect(hullCombo_, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
    connect(engineCombo_, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
    for (auto* spin : {scannerCount_, compactScannerCount_, penetratingScannerCount_, remoteMiningModuleCount_,
             colonyModuleCount_, fuelTankCount_, cargoPodCount_, antimatterCount_}) {
        connect(spin, &QSpinBox::valueChanged, this, [this] { updatePreview(); });
    }

    updatePreview();
}

ShipDesign ShipDesignerDialog::previewDesign() const
{
    ShipDesign design;
    design.id = 0;
    design.owner = player_;
    design.name = nameEdit_->text().trimmed().toStdString();
    design.hull = static_cast<ShipHullType>(hullCombo_->currentData().toInt());
    design.components.push_back(static_cast<ShipComponentType>(engineCombo_->currentData().toInt()));

    const auto append = [&](ShipComponentType type, int count) {
        for (int i = 0; i < count; ++i) design.components.push_back(type);
    };
    append(ShipComponentType::LongRangeScanner, scannerCount_->value());
    append(ShipComponentType::CompactLongRangeScanner, compactScannerCount_->value());
    append(ShipComponentType::PenetratingScanner, penetratingScannerCount_->value());
    append(ShipComponentType::RemoteMiningModule, remoteMiningModuleCount_->value());
    append(ShipComponentType::ColonyModule, colonyModuleCount_->value());
    append(ShipComponentType::FuelTank, fuelTankCount_->value());
    append(ShipComponentType::CargoPod, cargoPodCount_->value());
    append(ShipComponentType::AntimatterGenerator, antimatterCount_->value());
    return design;
}

ShipDesignDraft ShipDesignerDialog::draft() const
{
    const auto design = previewDesign();
    return {design.name, design.hull, design.components};
}

void ShipDesignerDialog::updatePreview()
{
    const auto design = previewDesign();
    const auto hull = hull_spec(design.hull);
    const auto generalUsed = ship_design_general_slots_used(design);
    const auto valid = ship_design_valid(design);

    const auto maxGeneral = static_cast<int>(hull.generalSlots);
    for (auto* spin : {scannerCount_, compactScannerCount_, penetratingScannerCount_, remoteMiningModuleCount_,
             colonyModuleCount_, fuelTankCount_, cargoPodCount_, antimatterCount_}) {
        spin->setMaximum(maxGeneral);
    }

    QStringList fuelCurve;
    const auto maxWarp = ship_design_max_warp(design);
    for (std::uint8_t warp = 1; warp <= maxWarp; ++warp) {
        fuelCurve.push_back(
            QString("W%1: %2")
                .arg(warp)
                .arg(signedFuelRate(ship_design_fuel_rate(design, warp))));
    }

    QString capabilities;
    if (const auto sensor = ship_design_sensor_range(design); sensor > 0.0) {
        capabilities += QString("Scanner %1 ly").arg(sensor, 0, 'f', 0);
    }
    if (ship_design_can_colonize(design)) {
        if (!capabilities.isEmpty()) capabilities += " • ";
        capabilities += "Colony capable";
    }
    if (capabilities.isEmpty()) capabilities = "No special mission capability";

    QString warning;
    if (generalUsed > hull.generalSlots) {
        warning = QString("<br><b>Too many general modules: %1 used, %2 available.</b>")
                      .arg(static_cast<qulonglong>(generalUsed))
                      .arg(hull.generalSlots);
    } else if (design.name.empty()) {
        warning = "<br><b>Design needs a name.</b>";
    }

    const auto radiation = ship_design_radiation_hazard(design);
    const auto mineralCost = ship_design_mineral_cost(design);
    previewLabel_->setText(
        QString("<hr><b>%1</b><br>"
                "Hull: %2 — engine slots %3, general slots <b>%4/%5</b><br>"
                "Dry mass: <b>%6 kt</b> &nbsp; Build cost: <b>%7</b><br>"
                "Minerals: <b>I %8 / B %9 / G %10</b><br>"
                "Max Warp: <b>%11</b> &nbsp; Fuel capacity: <b>%12</b> &nbsp; Fuel generation: <b>%13/turn</b><br>"
                "Cargo capacity: <b>%14</b> (%15 colonists max)<br>"
                "%16%17<br><br>"
                "<b>Engine fuel curve</b> — rate per 100 kt per ly:<br>%18%19")
            .arg(QString::fromStdString(design.name.empty() ? std::string("Unnamed design") : design.name))
            .arg(QString::fromStdString(hull.name))
            .arg(hull.engineSlots)
            .arg(static_cast<qulonglong>(generalUsed))
            .arg(hull.generalSlots)
            .arg(ship_design_mass(design), 0, 'f', 1)
            .arg(ship_design_cost(design))
            .arg(mineralCost.ironium, 0, 'f', 0)
            .arg(mineralCost.boranium, 0, 'f', 0)
            .arg(mineralCost.germanium, 0, 'f', 0)
            .arg(maxWarp)
            .arg(ship_design_fuel_capacity(design), 0, 'f', 0)
            .arg(ship_design_fuel_generation(design), 0, 'f', 0)
            .arg(ship_design_cargo_capacity(design), 0, 'f', 0)
            .arg(ship_design_cargo_capacity(design) * kColonistsPerCargoUnit, 0, 'f', 0)
            .arg(capabilities)
            .arg(radiation > 0.0 ? " • <b>Radiation hazard</b>" : "")
            .arg(fuelCurve.join(" &nbsp; "))
            .arg(warning));

    saveButton_->setEnabled(valid);
}

} // namespace suns
