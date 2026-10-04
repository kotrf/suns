#include "galaxy_setup_widget.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>

namespace suns {

GalaxySetupWidget::GalaxySetupWidget(const GalaxyConfig& current, QWidget* parent)
    : QWidget(parent), original_(current)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    size_ = new QComboBox(this);
    size_->setObjectName("galaxySizeCombo");
    size_->addItems({"Tiny — 400 × 400 ly", "Small — 800 × 800 ly", "Medium — 1200 × 1200 ly",
        "Large — 1600 × 1600 ly", "Huge — 2000 × 2000 ly", "Custom / saved map"});
    density_ = new QComboBox(this);
    density_->setObjectName("galaxyDensityCombo");
    density_->addItems({"Sparse", "Normal", "Dense", "Packed"});
    density_->setCurrentIndex(1);
    systems_ = new QSpinBox(this);
    systems_->setObjectName("galaxySystemsSpin");
    systems_->setRange(2, static_cast<int>(kMaximumGalaxySystems));
    systems_->setValue(static_cast<int>(current.starCount));
    dimensions_ = new QLabel(this);
    dimensions_->setObjectName("galaxyDimensionsSummary");
    dimensions_->setWordWrap(true);
    form->addRow("Size", size_);
    form->addRow("Density", density_);
    form->addRow("Systems", systems_);
    form->addRow(dimensions_);
    size_->setCurrentIndex(5);
    for (int size = 0; size < 5; ++size) {
        for (int density = 0; density < 4; ++density) {
            const auto preset = galaxy_preset(static_cast<GalaxySize>(size),
                static_cast<GalaxyDensity>(density), current.seed);
            if (current.width == preset.width && current.height == preset.height
                && current.starCount == preset.starCount
                && current.minimumSeparation == preset.minimumSeparation) {
                size_->setCurrentIndex(size);
                density_->setCurrentIndex(density);
            }
        }
    }
    connect(size_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(density_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(systems_, &QSpinBox::valueChanged, this, [this] { refresh(); });
    refresh();
}

GalaxyConfig GalaxySetupWidget::config(std::uint64_t seed) const
{
    if (size_->currentIndex() < 5) {
        return galaxy_preset(static_cast<GalaxySize>(size_->currentIndex()),
            static_cast<GalaxyDensity>(density_->currentIndex()), seed);
    }
    auto result = original_;
    result.seed = seed;
    result.starCount = static_cast<std::size_t>(systems_->value());
    return result;
}

void GalaxySetupWidget::refresh()
{
    const bool custom = size_->currentIndex() == 5;
    density_->setEnabled(!custom);
    systems_->setReadOnly(!custom);
    systems_->setButtonSymbols(custom ? QAbstractSpinBox::UpDownArrows : QAbstractSpinBox::NoButtons);
    const auto selected = config(original_.seed);
    if (systems_->value() != static_cast<int>(selected.starCount))
        systems_->setValue(static_cast<int>(selected.starCount));
    dimensions_->setText(QString("%1 × %2 ly • %3 systems • %4 sq ly per system")
        .arg(selected.width, 0, 'f', 0).arg(selected.height, 0, 'f', 0)
        .arg(selected.starCount).arg(selected.width * selected.height / selected.starCount, 0, 'f', 0));
}

} // namespace suns
