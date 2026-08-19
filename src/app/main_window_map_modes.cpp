#include "main_window.hpp"
#include "star_item.hpp"

#include <QComboBox>
#include <QGraphicsScene>
#include <QLabel>
#include <QTimer>
#include <QToolBar>

#include <algorithm>
#include <cmath>

namespace suns {

namespace {

QColor spectralColor(StarClass stellarClass)
{
    switch (stellarClass) {
    case StarClass::BlueWhite: return QColor("#9bc5ff");
    case StarClass::White: return QColor("#e7eeff");
    case StarClass::YellowWhite: return QColor("#fff0b0");
    case StarClass::Yellow: return QColor("#ffd36b");
    case StarClass::Orange: return QColor("#ff9955");
    case StarClass::Red: return QColor("#ff6b62");
    }
    return QColor("#ffd36b");
}

QColor habitabilityColor(std::uint32_t habitability)
{
    const auto normalized = std::clamp(static_cast<double>(habitability) / 100.0, 0.0, 1.0);
    return QColor::fromHsvF(normalized / 3.0, 0.78, 0.95);
}

qreal populationScale(std::uint64_t population)
{
    if (population == 0) return 0.58;
    constexpr double referencePopulation = 2500.0;
    const auto normalized = std::clamp(static_cast<double>(population) / referencePopulation, 0.0, 1.0);
    return static_cast<qreal>(0.68 + 1.20 * std::sqrt(normalized));
}

QString modeLegend(int mode)
{
    if (mode == 1) return "red 0%  ·  yellow 50%  ·  green 100%  ·  grey unknown";
    if (mode == 2) return "marker size = population  ·  green = your colony  ·  grey = empty/unknown";
    return "stellar spectral class colours";
}

} // namespace

void MainWindow::installMapDisplayModes()
{
    auto* toolbar = new QToolBar("Map display", this);
    toolbar->setObjectName("mapDisplayToolbar");
    toolbar->setMovable(false);
    addToolBar(Qt::TopToolBarArea, toolbar);

    auto* title = new QLabel("Map mode:", toolbar);
    title->setContentsMargins(5, 0, 3, 0);
    toolbar->addWidget(title);

    auto* mode = new QComboBox(toolbar);
    mode->setObjectName("mapDisplayModeCombo");
    mode->addItem("Spectral", 0);
    mode->addItem("Habitability", 1);
    mode->addItem("Population", 2);
    mode->setToolTip(
        "Spectral: real stellar-class colours\n"
        "Habitability: red-to-green world value\n"
        "Population: marker size follows colony population");
    toolbar->addWidget(mode);

    auto* legend = new QLabel(modeLegend(0), toolbar);
    legend->setObjectName("mapDisplayLegend");
    legend->setContentsMargins(9, 0, 5, 0);
    toolbar->addWidget(legend);

    connect(mode, &QComboBox::currentIndexChanged, this, [this, mode, legend](int index) {
        mapDisplayMode_ = mode->itemData(index).toInt();
        legend->setText(modeLegend(mapDisplayMode_));
        applyMapDisplayMode();
    });

    connect(scene_, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) {
        if (shuttingDown_ || mapDisplayApplyPending_) return;
        mapDisplayApplyPending_ = true;
        QTimer::singleShot(0, this, [this] {
            mapDisplayApplyPending_ = false;
            if (!shuttingDown_) applyMapDisplayMode();
        });
    });

    applyMapDisplayMode();
}

void MainWindow::applyMapDisplayMode()
{
    if (!scene_) return;

    for (auto* item : scene_->items()) {
        auto* marker = dynamic_cast<StarItem*>(item);
        if (!marker) continue;

        const auto starId = static_cast<StarId>(marker->data(0).toUInt());
        const auto* star = find_star(state_, starId);
        if (!star) continue;

        const bool surveyed = is_surveyed(state_, 1, starId);
        const auto* planet = find_planet_at_star(state_, starId);

        QColor color = spectralColor(star->stellarClass);
        qreal scale = 1.0;

        if (mapDisplayMode_ == 1) {
            color = surveyed && planet ? habitabilityColor(planet->habitability) : QColor("#687381");
        } else if (mapDisplayMode_ == 2) {
            if (!surveyed) {
                color = QColor("#687381");
                scale = 0.58;
            } else if (planet && planet->owner != 0) {
                color = planet->owner == 1 ? QColor("#67d796") : QColor("#d77777");
                scale = populationScale(planet->population);
            } else {
                color = QColor("#7f8997");
                scale = 0.62;
            }
        }

        marker->setVisualStyle(color, scale);
    }
}

} // namespace suns
