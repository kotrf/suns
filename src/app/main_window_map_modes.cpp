#include "main_window.hpp"
#include "suns/terraforming.hpp"
#include "suns/campaign.hpp"
#include "star_item.hpp"
#include "mineral_stock_panel.hpp"
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>

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

QColor habitabilityColor(std::int32_t habitability)
{
    if (habitability < 0) {
        const auto severity = std::clamp(static_cast<double>(-habitability) / 100.0, 0.0, 1.0);
        return QColor::fromHsvF(0.0, 0.72 + 0.23 * severity, 0.82 - 0.22 * severity);
    }
    const auto normalized = std::clamp(static_cast<double>(habitability) / 100.0, 0.0, 1.0);
    return QColor::fromHsvF(normalized / 3.0, 0.78, 0.95);
}

qreal populationScale(std::uint64_t population)
{
    if (population == 0) return 0.58;
    constexpr double referencePopulation = 2500000.0;
    const auto normalized = std::clamp(static_cast<double>(population) / referencePopulation, 0.0, 1.0);
    return static_cast<qreal>(0.68 + 1.20 * std::sqrt(normalized));
}

QString modeLegend(int mode)
{
    if (mode == 1) return "size = hab  ·  dark red negative  ·  red 0%  ·  yellow 50%  ·  green 100%  ·  dim estimated  ·  grey unknown";
    if (mode == 2) return "marker size = population  ·  green = your colony  ·  grey = empty/unknown";
    if(mode==3)return "I / B / G concentration — shared 0–100% scale";
    if(mode==4)return "I / B / G surface kt — shared System Details scale";
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
    mode->addItem("Mineral Concentration",3);
    mode->addItem("Surface Minerals",4);
    mode->setToolTip(
        "Spectral: real stellar-class colours\n"
        "Habitability: larger markers for higher known hab; red-to-green world value; yellow means hostile but terraformable\n"
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
    const auto& received = receivedPlayerState();

    bool changed = false;
    for (auto* item : scene_->items()) {
        auto* marker = dynamic_cast<StarItem*>(item);
        if (!marker) continue;

        const auto starId = static_cast<StarId>(marker->data(0).toUInt());
        const auto* star = find_star(received, starId);
        if (!star) continue;

        const bool surveyed = is_surveyed(received, pendingOrders_.player, starId);
        const auto* planet = find_planet_at_star(received, starId);
        const auto knownHabitability = planet
            ? known_planet_habitability(received, pendingOrders_.player, planet->id)
            : std::optional<std::int32_t>{};

        QColor color = spectralColor(star->stellarClass);
        qreal scale = 1.0;

        if (mapDisplayMode_ == 1) {
            scale = 1.15 + 0.25 * std::clamp(knownHabitability.value_or(0), 0, 100) / 100.0;
            color = knownHabitability ? habitabilityColor(*knownHabitability) : QColor("#687381");
            if (planet && knownHabitability && *knownHabitability < 0
                && (planet->owner == pendingOrders_.player || survey_level(received, pendingOrders_.player, starId) >= SurveyLevel::OrbitalSurvey)) {
                const auto* player = find_player(received, pendingOrders_.player);
                if (player && player->race.environmentBased
                    && player_planet_habitability(received, pendingOrders_.player,
                        terraforming_potential(received, pendingOrders_.player, *planet), received.turn) >= 0)
                    color = QColor("#e6bf59");
            }
            if (survey_level(received, pendingOrders_.player, starId) == SurveyLevel::BasicScan) color = color.darker(145);
        } else if (mapDisplayMode_ == 2) {
            const auto knownOwner = planet
                ? known_planet_owner(received, pendingOrders_.player, planet->id)
                : std::optional<PlayerId>{};
            if (!surveyed) {
                color = QColor("#687381");
                scale = 0.58;
            } else if (planet && knownOwner && *knownOwner != 0) {
                const bool reportedOwn = *knownOwner == pendingOrders_.player;
                color = reportedOwn ? QColor("#67d796") : QColor("#d77777");
                scale = reportedOwn && planet->owner == pendingOrders_.player
                    ? populationScale(planet->population) : 0.9;
            } else {
                color = QColor("#7f8997");
                scale = 0.62;
            }
        }

        QGraphicsItemGroup* bars=nullptr;
        for(auto* child:marker->childItems())if(child->data(0).toString()=="mineralOverlay")bars=static_cast<QGraphicsItemGroup*>(child);
        if(mapDisplayMode_==3 || mapDisplayMode_==4) {
            std::optional<MineralCargo> values;
            std::optional<std::uint64_t> observed;
            if(planet) {
                if(mapDisplayMode_==3 && (planet_geology_known(received,pendingOrders_.player,planet->id)
                    || known_planet_owner(received,pendingOrders_.player,planet->id)==pendingOrders_.player))
                    values=planet_mineral_concentration(received,*planet);
                if(mapDisplayMode_==4)if(const auto stock=known_surface_minerals(received,pendingOrders_.player,planet->id)) {
                    values=stock->minerals;observed=stock->observedTurn;
                    if(known_planet_owner(received,pendingOrders_.player,planet->id)==pendingOrders_.player)
                        if(const auto* p=find_planet(fleetPlanningState(),planet->id))values=p->minerals;
                }
            }
            if(values) {
                if(!bars) {
                    bars=new QGraphicsItemGroup(marker);bars->setData(0,"mineralOverlay");
                    bars->setFlag(QGraphicsItem::ItemIgnoresTransformations);bars->setZValue(5);
                    for(int i=0;i<3;++i){auto* b=new QGraphicsRectItem(bars);b->setData(1,i);b->setPen(QPen(QColor("#7d91a0"),0.5));b->setBrush(QColor(i==0?"#6db7e7":i==1?"#75c586":"#dfcc75"));}
                }
                const auto* panel=findChild<MineralStockPanel*>("mineralStockPanel");
                const double maximum=mapDisplayMode_==3?100:panel?panel->stockScale():1000;
                const std::array<double,3> v{values->ironium,values->boranium,values->germanium};
                for(auto* b:bars->childItems()){const auto i=b->data(1).toInt();const auto h=20*std::clamp(v[i]/maximum,0.0,1.0);static_cast<QGraphicsRectItem*>(b)->setRect(-6+i*5,-12-h,3,h);}
                const auto tip=QString("I %1 / B %2 / G %3 %4\n%5")
                    .arg(v[0],0,'f',1).arg(v[1],0,'f',1).arg(v[2],0,'f',1)
                    .arg(mapDisplayMode_==3?"%":"kt")
                    .arg(observed?QString("Last stock report: year %1").arg(qulonglong(*observed)):"Received geology");
                if(bars->toolTip()!=tip) {
                    bars->setToolTip(tip);
                    // The visible rectangles participate in scene hit-testing.
                    // An empty item group alone is not a tooltip hover target.
                    for(auto* bar:bars->childItems())bar->setToolTip(tip);
                }
                bars->setVisible(true);
            } else if(bars)bars->setVisible(false);
        } else if(bars)bars->setVisible(false);
        changed |= marker->setVisualStyle(color, scale);
    }
    if (changed) queueMapLabelRefresh();
}

} // namespace suns
