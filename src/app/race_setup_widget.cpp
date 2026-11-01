#include "race_setup_widget.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace suns {
RaceSetupWidget::RaceSetupWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName("raceSetupWidget");
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    name_ = new QLineEdit(this); name_->setObjectName("raceEmpireName");
    form->addRow("Empire name", name_);
    environment_ = new QComboBox(this); environment_->setObjectName("raceEnvironment");
    environment_->addItem("Terran", int(RacePreset::Terran));
    environment_->addItem("Cryophile", int(RacePreset::Cryophile));
    environment_->addItem("Radiotroph", int(RacePreset::Radiotroph));
    form->addRow("Environment", environment_);
    environmentSummary_ = new QLabel(this); environmentSummary_->setWordWrap(true);
    environmentSummary_->setObjectName("raceEnvironmentSummary");
    form->addRow("Habitat", environmentSummary_);
    const auto updateEnvironment = [this] {
        const auto race = race_preset(static_cast<RacePreset>(environment_->currentData().toInt()));
        const auto range = [](const auto& axis) { return QString("%1–%2").arg(axis.minimum).arg(axis.maximum); };
        environmentSummary_->setText(QString("Temperature %1 • Gravity %2 • Radiation %3")
            .arg(range(race.habitableTemperature), range(race.habitableGravity),
                race.radiationImmune ? "immune" : range(race.habitableRadiation)));
    };
    connect(environment_, &QComboBox::currentIndexChanged, this, updateEnvironment);
    trait_ = new QComboBox(this); trait_->setObjectName("racePrimaryTrait");
    trait_->addItem("Generalist", int(PrimaryRaceTrait::Generalist));
    trait_->addItem("Stargate Specialist", int(PrimaryRaceTrait::StargateSpecialist));
    trait_->setToolTip("Generalist: finite unloaded gates. Stargate Specialist: cargo and colonists through gates, plus researched Network and Infinity/Infinity gates. Gates still require construction.");
    form->addRow("Primary trait", trait_);
    efficiency_ = new QCheckBox("15% lower reference-engine fuel use; Fuel Mizer / Galaxy Scoop access", this);
    efficiency_->setObjectName("raceFuelEfficiency");
    form->addRow("Fuel efficiency", efficiency_);
    noScoops_ = new QCheckBox("Block researchable ram scoops; unlock Interspace-10 at Propulsion 11", this);
    noScoops_->setObjectName("raceNoRamScoops");
    form->addRow("No ram scoops", noScoops_);
    settlers_ = new QCheckBox("Mini-Colony Ship and Settler's Delight access", this);
    settlers_->setObjectName("raceSettlerEngines");
    form->addRow("Settler engines", settlers_);
    hulls_ = new QComboBox(this); hulls_->setObjectName("raceHullFamily");
    hulls_->addItems({"Standard", "Inner Strength", "Super Stealth", "War Monger", "Space Demolition", "Hyper Expansion"});
    hulls_->setToolTip("Special hull access is independent of other racial economy and combat rules. Hyper Expansion includes Settler engines.");
    form->addRow("Hull family", hulls_);
    connect(hulls_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool included = index == int(HullAccess::HyperExpansion);
        if (included) settlers_->setChecked(true);
        settlers_->setEnabled(!included);
    });
    mining_ = new QComboBox(this); mining_->setObjectName("raceRemoteMining");
    mining_->addItems({"Standard", "Advanced (ARM)", "Basic (BRM)"});
    mining_->setToolTip("ARM adds Midget Miner and Ultra-Miner; BRM blocks reference miner hulls.");
    form->addRow("Remote mining", mining_);
    factories_ = new QSpinBox(this); factories_->setObjectName("raceFactoryOperation");
    mines_ = new QSpinBox(this); mines_->setObjectName("raceMineOperation");
    for (auto* rate : {factories_, mines_}) {
        rate->setRange(kMinWorkforceRate, kMaxWorkforceRate);
        rate->setToolTip("Operating installations per 10,000 surface colonists; factory and mine limits are independent. Extra buildings remain idle.");
    }
    form->addRow("Factories / 10,000 colonists", factories_);
    form->addRow("Mines / 10,000 colonists", mines_);
    layout->addLayout(form);
    auto* note = new QLabel("Choose from the supported race presets and traits. These settings use the same rules as New campaign.", this);
    note->setWordWrap(true); layout->addWidget(note);
    setSetup(EmpireSetup{"My empire"});
    // A preset already selected at index zero does not emit a change signal.
    updateEnvironment();
}

EmpireSetup RaceSetupWidget::setup() const
{
    EmpireSetup result;
    result.name = name_->text().trimmed().toUtf8().toStdString();
    result.race = static_cast<RacePreset>(environment_->currentData().toInt());
    result.primaryTrait = static_cast<PrimaryRaceTrait>(trait_->currentData().toInt());
    result.improvedFuelEfficiency = efficiency_->isChecked();
    result.noRamScoopEngines = noScoops_->isChecked();
    result.hullAccess = static_cast<HullAccess>(hulls_->currentIndex());
    result.settlerEngineAccess = settlers_->isChecked() || result.hullAccess == HullAccess::HyperExpansion;
    result.advancedRemoteMining = mining_->currentIndex() == 1;
    result.basicRemoteMining = mining_->currentIndex() == 2;
    result.factoriesPerTenThousand = static_cast<std::uint8_t>(factories_->value());
    result.minesPerTenThousand = static_cast<std::uint8_t>(mines_->value());
    return result;
}

void RaceSetupWidget::setSetup(const EmpireSetup& setup)
{
    name_->setText(QString::fromStdString(setup.name));
    environment_->setCurrentIndex(environment_->findData(int(setup.race)));
    trait_->setCurrentIndex(trait_->findData(int(setup.primaryTrait)));
    efficiency_->setChecked(setup.improvedFuelEfficiency);
    noScoops_->setChecked(setup.noRamScoopEngines);
    hulls_->setCurrentIndex(int(setup.hullAccess));
    settlers_->setChecked(setup.settlerEngineAccess || setup.hullAccess == HullAccess::HyperExpansion);
    mining_->setCurrentIndex(setup.advancedRemoteMining ? 1 : setup.basicRemoteMining ? 2 : 0);
    factories_->setValue(setup.factoriesPerTenThousand);
    mines_->setValue(setup.minesPerTenThousand);
}
} // namespace suns
