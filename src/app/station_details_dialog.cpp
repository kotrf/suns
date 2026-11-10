#include "station_details_dialog.hpp"
#include "suns/stations.hpp"
#include "suns/orbital_bases.hpp"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

namespace suns {
namespace {

int componentGroup(ShipComponentType component)
{
    const auto kind = component_spec(component).kind;
    if (kind == ShipComponentKind::BeamWeapon || kind == ShipComponentKind::Torpedo) return 0;
    if (kind == ShipComponentKind::Armor || kind == ShipComponentKind::Shield || radiation_screen_protection(component)) return 1;
    return 2;
}

QString componentName(ShipComponentType component)
{
    if (const auto* gate = stargate_spec(component)) return QString::fromUtf8(gate->name);
    return QString::fromStdString(component_spec(component).name);
}

QGroupBox* fittingGroup(const OrbitalStation& station, int group, int capacity, int columns, QWidget* parent)
{
    const std::array names{"Weapons", "Defenses", "Electronics"};
    std::vector<ShipComponentType> fitted;
    for (auto component : station.components) if (componentGroup(component) == group) fitted.push_back(component);
    auto* box = new QGroupBox(QString("%1 — %2 / %3").arg(names[group]).arg(fitted.size()).arg(capacity), parent);
    box->setObjectName(QString("stationFitGroup%1").arg(group));
    auto* grid = new QGridLayout(box);
    grid->setSpacing(5);
    grid->setAlignment(Qt::AlignTop);
    for (int i = 0; i < capacity; ++i) {
        const bool occupied = i < int(fitted.size());
        auto* cell = new QFrame(box);
        cell->setObjectName(QString("stationFitCell%1_%2").arg(group).arg(i));
        cell->setProperty("occupied", occupied);
        cell->setMinimumWidth(106);
        auto* layout = new QVBoxLayout(cell);
        layout->setContentsMargins(6, 4, 6, 5);
        layout->setSpacing(2);
        auto* number = new QLabel(QString::number(i + 1), cell);
        number->setObjectName("stationCellNumber");
        layout->addWidget(number);
        auto* name = new QLabel(occupied ? componentName(fitted[i]) : QString("Empty"), cell);
        name->setObjectName("stationCellEquipment");
        name->setTextFormat(Qt::PlainText);
        name->setWordWrap(true);
        name->setMinimumHeight(34);
        name->setAlignment(Qt::AlignCenter);
        layout->addWidget(name, 1);
        if (occupied) {
            const auto info = component_spec(fitted[i]);
            cell->setToolTip(QString("%1\nArmor %2 • Shields %3\nWeapon power %4 • Range %5")
                .arg(componentName(fitted[i])).arg(info.armor).arg(info.shields).arg(info.weaponPower).arg(info.weaponRange));
            if (const auto protection = radiation_screen_protection(fitted[i]))
                cell->setToolTip(cell->toolTip() + QString("\nRadiation protection: %1 (one screen per station)").arg(protection));
            if (const auto* gate = stargate_spec(fitted[i])) cell->setToolTip(cell->toolTip()
                + QString("\nGate range: %1 ly • Maximum mass: %2 kt")
                    .arg(gate->rangeLy == 0 ? QString("Unlimited") : QString::number(gate->rangeLy))
                    .arg(gate->maximumMassKt == 0 ? QString("Unlimited") : QString::number(gate->maximumMassKt)));
        }
        grid->addWidget(cell, i / columns, i % columns);
    }
    return box;
}

} // namespace

QIcon stationIcon(OrbitalStationHullType hull)
{
    static std::array<QIcon, 3> icons;
    auto& icon = icons.at(std::size_t(hull));
    if (!icon.isNull()) return icon;
    for (int size : {32, 64, 160, 320}) {
        QPixmap image(size, size);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 100.0, size / 100.0);
        painter.translate(50, 50);
        painter.setPen(QPen(QColor("#78bdcf"), 1.6));
        painter.setBrush(QColor("#162e3d"));
        painter.drawEllipse(QPointF(), 32, 32);
        const int arms = hull == OrbitalStationHullType::OrbitalDock ? 4
            : hull == OrbitalStationHullType::Starbase ? 6 : 8;
        for (int i = 0; i < arms; ++i) {
            painter.save();
            painter.rotate(360.0 * i / arms);
            painter.drawRect(QRectF(-5, -43, 10, 26));
            painter.setBrush(QColor("#426c83"));
            painter.drawRoundedRect(QRectF(-10, -45, 20, 12), 2, 2);
            painter.restore();
        }
        painter.setBrush(QColor("#253e50"));
        painter.drawEllipse(QPointF(), 19, 19);
        painter.setBrush(QColor("#d6ad62"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(), 7, 7);
        painter.end();
        icon.addPixmap(image);
    }
    return icon;
}

StationDetailsDialog::StationDetailsDialog(const OrbitalStation& station, std::uint64_t turn, QWidget* parent,
    const QString& radiationText)
    : QDialog(parent)
{
    setObjectName("stationDetailsDialog");
    setWindowTitle(QString("Station — %1").arg(QString::fromStdString(station.name)));
    resize(860, station.hull == OrbitalStationHullType::OrbitalDock ? 500
        : station.hull == OrbitalStationHullType::Starbase ? 600 : 740);
    setStyleSheet("QDialog#stationDetailsDialog, QWidget#stationFitDeck { background: #0b141e; color: #d6e5f2; }"
        "QLabel { color: #d6e5f2; }"
        "QGroupBox { color: #a9cadf; border: 1px solid #344b5b; border-radius: 4px; margin-top: 9px; padding-top: 9px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 9px; }"
        "QFrame[occupied=\"true\"] { background: #1b3446; border: 1px solid #527b93; border-radius: 3px; }"
        "QFrame[occupied=\"false\"] { background: #0d1924; border: 1px dashed #2b414f; border-radius: 3px; }"
        "QFrame[occupied=\"false\"] QLabel, QLabel#stationCellNumber { color: #8298a8; }"
        "QLabel#stationCellNumber { font-size: 9px; }");
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QString::fromStdString(station.name), this);
    heading->setObjectName("stationDetailsName");
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    auto font = heading->font(); font.setBold(true); font.setPointSize(font.pointSize() + 2); heading->setFont(font);
    layout->addWidget(heading);
    const auto& hull = station_hull_spec(station.hull);
    auto* summary = new QLabel(QString("%1 • Station #%2 • Empire %3\nArmor (total) %4 • Shields %5 • Damage %6%")
        .arg(hull.name).arg(station.id).arg(station.owner).arg(station_armor(station), 0, 'f', 0)
        .arg(station_shields(station), 0, 'f', 0).arg(station.damagePercent, 0, 'f', 1), this);
    summary->setObjectName("stationDetailsSummary"); summary->setWordWrap(true); layout->addWidget(summary);
    auto* knowledge = new QLabel(QString("Received station data • Player turn %1. Unreported changes are not shown.").arg(turn), this);
    knowledge->setObjectName("stationDetailsKnowledge"); knowledge->setWordWrap(true); layout->addWidget(knowledge);
    if (!radiationText.isEmpty()) {
        auto* radiation = new QLabel(radiationText, this);
        radiation->setObjectName("stationRadiationStatus"); radiation->setTextFormat(Qt::PlainText);
        radiation->setWordWrap(true); layout->addWidget(radiation);
    }

    auto* scroll = new QScrollArea(this); scroll->setObjectName("stationFitScroll"); scroll->setWidgetResizable(true);
    auto* deck = new QWidget(scroll); deck->setObjectName("stationFitDeck");
    auto* grid = new QGridLayout(deck);
    grid->addWidget(fittingGroup(station, 0, hull.weapons, std::min(6, int(hull.weapons)), deck), 0, 0, 1, 3);
    grid->addWidget(fittingGroup(station, 1, hull.defenses, 2, deck), 1, 0);
    auto* portrait = new QLabel(deck); portrait->setObjectName("stationHullPortrait");
    portrait->setPixmap(stationIcon(station.hull).pixmap(160, 160)); portrait->setAlignment(Qt::AlignCenter);
    grid->addWidget(portrait, 1, 1);
    grid->addWidget(fittingGroup(station, 2, hull.electronics, 2, deck), 1, 2);
    grid->setRowStretch(2, 1);
    scroll->setWidget(deck); layout->addWidget(scroll, 1);
    const auto has = [&](OrbitalStationModule module) {
        return std::find(station.modules.begin(), station.modules.end(), module) != station.modules.end();
    };
    auto* services = new QLabel(QString("Shipyard / fleet repair: %1 • Refueling depot: %2")
        .arg(has(OrbitalStationModule::Shipyard) ? "Fitted" : "Not fitted")
        .arg(has(OrbitalStationModule::RefuelingDepot) ? "Fitted" : "Not fitted"), this);
    services->setObjectName("stationDetailsServices"); services->setWordWrap(true); layout->addWidget(services);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

} // namespace suns
