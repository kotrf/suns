#include "main_window.hpp"

#include <QColor>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace suns {
namespace {

bool hasComponent(const ShipDesign& design, ShipComponentType type)
{
    return std::find(design.components.begin(), design.components.end(), type) != design.components.end();
}

QColor hullAccent(ShipHullType hull)
{
    switch (hull) {
    case ShipHullType::Scout: return QColor("#73bdf0");
    case ShipHullType::LightTransport: return QColor("#79c79b");
    case ShipHullType::MediumTransport: return QColor("#d8a862");
    case ShipHullType::RemoteMiner: return QColor("#c58be2");
    case ShipHullType::Utility: return QColor("#68c7c2");
    }
    return QColor("#9eb3c8");
}

QPixmap emptyFleetPortrait()
{
    QPixmap pixmap(184, 104);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor("#09121c"));
    painter.setPen(QPen(QColor("#293d50"), 1.0));
    painter.drawRoundedRect(QRectF(1, 1, 182, 102), 6, 6);
    painter.setPen(QColor("#718396"));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "NO FLEET SELECTED");
    return pixmap;
}

void drawEngineGlow(QPainter& painter, qreal x, qreal y, const QColor& colour)
{
    QLinearGradient glow(x - 25, y, x + 3, y);
    QColor transparent = colour;
    transparent.setAlpha(0);
    QColor bright = colour;
    bright.setAlpha(190);
    glow.setColorAt(0.0, transparent);
    glow.setColorAt(0.65, bright);
    glow.setColorAt(1.0, colour.lighter(145));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawPolygon(QPolygonF{
        QPointF(x - 27, y), QPointF(x - 2, y - 5), QPointF(x + 2, y), QPointF(x - 2, y + 5)});
}

QPixmap renderShipPortrait(const ShipDesign& design)
{
    QPixmap pixmap(184, 104);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient background(0, 0, 184, 104);
    background.setColorAt(0.0, QColor("#07101a"));
    background.setColorAt(1.0, QColor("#0d1b28"));
    painter.setBrush(background);
    painter.setPen(QPen(QColor("#2b4053"), 1.0));
    painter.drawRoundedRect(QRectF(1, 1, 182, 102), 6, 6);

    // Deterministic sparse star field. It keeps portraits recognisable without
    // storing image assets or making save files depend on generated pixels.
    const quint32 seed = design.id * 2654435761u + static_cast<quint32>(design.components.size() * 97u);
    painter.setPen(QColor(115, 150, 182, 85));
    for (int i = 0; i < 18; ++i) {
        const int x = 8 + static_cast<int>((seed + i * 37u + i * i * 11u) % 167u);
        const int y = 7 + static_cast<int>((seed / 7u + i * 53u + i * i * 5u) % 88u);
        painter.drawPoint(x, y);
    }

    const QColor accent = hullAccent(design.hull);
    QColor hullFill("#617589");
    QColor hullEdge = accent.lighter(120);

    QPainterPath hull;
    switch (design.hull) {
    case ShipHullType::Scout:
        hull.moveTo(146, 52);
        hull.lineTo(105, 35);
        hull.lineTo(70, 39);
        hull.lineTo(50, 47);
        hull.lineTo(50, 57);
        hull.lineTo(70, 65);
        hull.lineTo(105, 69);
        hull.closeSubpath();
        break;
    case ShipHullType::LightTransport:
        hull.moveTo(148, 52);
        hull.cubicTo(132, 35, 112, 31, 81, 32);
        hull.cubicTo(59, 33, 47, 41, 45, 52);
        hull.cubicTo(47, 63, 59, 71, 81, 72);
        hull.cubicTo(112, 73, 132, 69, 148, 52);
        hull.closeSubpath();
        break;
    case ShipHullType::MediumTransport:
        hull.moveTo(151, 52);
        hull.cubicTo(135, 31, 111, 25, 76, 28);
        hull.cubicTo(50, 30, 38, 39, 37, 52);
        hull.cubicTo(38, 65, 50, 74, 76, 76);
        hull.cubicTo(111, 79, 135, 73, 151, 52);
        hull.closeSubpath();
        break;
    case ShipHullType::RemoteMiner:
        hull.moveTo(148, 52);
        hull.lineTo(126, 32);
        hull.lineTo(70, 27);
        hull.lineTo(39, 42);
        hull.lineTo(39, 62);
        hull.lineTo(70, 77);
        hull.lineTo(126, 72);
        hull.closeSubpath();
        break;
    case ShipHullType::Utility:
        hull.moveTo(150, 52);
        hull.lineTo(126, 31);
        hull.lineTo(72, 30);
        hull.lineTo(43, 43);
        hull.lineTo(43, 61);
        hull.lineTo(72, 74);
        hull.lineTo(126, 73);
        hull.closeSubpath();
        break;
    }

    painter.setPen(QPen(QColor(0, 0, 0, 110), 5.0));
    painter.setBrush(QColor(0, 0, 0, 100));
    painter.drawPath(hull.translated(2, 3));

    QLinearGradient hullGradient(45, 28, 145, 75);
    hullGradient.setColorAt(0.0, hullFill.lighter(145));
    hullGradient.setColorAt(0.5, hullFill);
    hullGradient.setColorAt(1.0, hullFill.darker(155));
    painter.setPen(QPen(hullEdge, 1.4));
    painter.setBrush(hullGradient);
    painter.drawPath(hull);

    // Engine identity is deliberately visible. This becomes useful once the
    // player learns to recognise scoop/radiating drives at a glance.
    QColor engineColour("#76b8ff");
    if (hasComponent(design, ShipComponentType::RamScoopDrive)) engineColour = QColor("#65d6cf");
    if (hasComponent(design, ShipComponentType::RadiatingRamScoopDrive)) engineColour = QColor("#ff8b63");
    const auto engineCount = hull_spec(design.hull).requiredEngines;
    for (std::uint8_t index = 0; index < engineCount; ++index) {
        const auto offset = (static_cast<qreal>(index) - (engineCount - 1.0) / 2.0) * 16.0;
        const auto engineY = 52.0 + offset;
        drawEngineGlow(painter, 47, engineY, engineColour);
        painter.setBrush(engineColour.darker(145));
        painter.setPen(QPen(engineColour.lighter(125), 1.0));
        painter.drawRect(QRectF(43, engineY - 7, 10, 14));
    }

    // Hull mass class gets a different silhouette even before optional modules.
    if (design.hull != ShipHullType::Scout) {
        painter.setBrush(accent.darker(170));
        painter.setPen(QPen(accent, 1.0));
        painter.drawRoundedRect(QRectF(70, 22, 31, 11), 4, 4);
        painter.drawRoundedRect(QRectF(70, 71, 31, 11), 4, 4);
    }
    if (design.hull == ShipHullType::MediumTransport) {
        painter.drawRoundedRect(QRectF(104, 23, 24, 12), 4, 4);
        painter.drawRoundedRect(QRectF(104, 69, 24, 12), 4, 4);
    }
    if (design.hull == ShipHullType::RemoteMiner) {
        painter.setBrush(QColor("#684876"));
        painter.setPen(QPen(QColor("#d7a6ec"), 1.0));
        painter.drawRoundedRect(QRectF(76, 18, 34, 15), 3, 3);
        painter.drawRoundedRect(QRectF(76, 71, 34, 15), 3, 3);
    }
    if (design.hull == ShipHullType::Utility) {
        painter.setBrush(QColor("#315f67"));
        painter.setPen(QPen(QColor("#85ded8"), 1.0));
        painter.drawRoundedRect(QRectF(72, 20, 25, 13), 3, 3);
        painter.drawRoundedRect(QRectF(101, 20, 25, 13), 3, 3);
        painter.drawRoundedRect(QRectF(72, 71, 25, 13), 3, 3);
        painter.drawRoundedRect(QRectF(101, 71, 25, 13), 3, 3);
    }

    if (hasComponent(design, ShipComponentType::CargoPod)) {
        painter.setBrush(QColor("#b47d43"));
        painter.setPen(QPen(QColor("#e1b170"), 1.0));
        painter.drawRoundedRect(QRectF(91, 26, 24, 12), 3, 3);
        painter.drawRoundedRect(QRectF(91, 66, 24, 12), 3, 3);
    }

    if (hasComponent(design, ShipComponentType::FuelTank)) {
        painter.setBrush(QColor("#347f9c"));
        painter.setPen(QPen(QColor("#74c4df"), 1.0));
        painter.drawEllipse(QRectF(61, 31, 12, 20));
        painter.drawEllipse(QRectF(61, 53, 12, 20));
    }

    if (hasComponent(design, ShipComponentType::LongRangeScanner)
        || hasComponent(design, ShipComponentType::CompactLongRangeScanner)
        || hasComponent(design, ShipComponentType::PenetratingScanner)) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(
            hasComponent(design, ShipComponentType::PenetratingScanner)
                ? QColor("#d9a6ff")
                : QColor("#9fd5ff"),
            1.5));
        painter.drawArc(QRectF(96, 17, 25, 20), 18 * 16, 142 * 16);
        painter.drawLine(QPointF(106, 35), QPointF(106, 24));
        painter.drawEllipse(QRectF(104, 21, 4, 4));
    }

    if (hasComponent(design, ShipComponentType::ColonyModule)) {
        painter.setBrush(QColor("#61b982"));
        painter.setPen(QPen(QColor("#b1e5bd"), 1.2));
        painter.drawEllipse(QRectF(104, 42, 20, 20));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRectF(100, 47, 28, 10));
    }

    if (hasComponent(design, ShipComponentType::AntimatterGenerator)) {
        painter.setBrush(QColor(150, 95, 208, 90));
        painter.setPen(QPen(QColor("#c6a3f0"), 1.3));
        painter.drawEllipse(QRectF(78, 43, 18, 18));
        painter.drawEllipse(QRectF(82, 47, 10, 10));
    }

    painter.setPen(accent.lighter(135));
    painter.drawLine(QPointF(121, 49), QPointF(139, 52));
    painter.drawLine(QPointF(121, 55), QPointF(139, 52));

    painter.setPen(QColor("#9fb0c1"));
    painter.drawText(QRectF(8, 80, 168, 17), Qt::AlignRight | Qt::AlignVCenter,
        QString::fromStdString(hull_spec(design.hull).name));

    return pixmap;
}

} // namespace

void MainWindow::installFleetPortraitPolish()
{
    auto* fleetGroup = findChild<QGroupBox*>("fleetGroup");
    if (!fleetGroup) return;
    auto* layout = qobject_cast<QVBoxLayout*>(fleetGroup->layout());
    if (!layout) return;

    auto* portrait = new QLabel(fleetGroup);
    portrait->setObjectName("fleetPortrait");
    portrait->setAlignment(Qt::AlignCenter);
    portrait->setMinimumHeight(108);
    portrait->setPixmap(emptyFleetPortrait());
    portrait->setToolTip("The silhouette is derived from the selected ship design's hull and fitted components.");
    layout->insertWidget(0, portrait);

    const auto refresh = [this, portrait] {
        if (shuttingDown_) return;
        const auto* fleet = selectedFleet();
        const auto* design = fleet ? fleet_design(state_, *fleet) : nullptr;
        if (!fleet || !design) {
            portrait->setPixmap(emptyFleetPortrait());
            portrait->setToolTip("No fleet selected.");
            return;
        }

        portrait->setPixmap(renderShipPortrait(*design));
        QStringList components;
        for (const auto component : design->components) {
            components << QString::fromStdString(component_spec(component).name);
        }
        portrait->setToolTip(
            QString("%1 — %2\nHull: %3\nComponents: %4")
                .arg(QString::fromStdString(fleet->name))
                .arg(QString::fromStdString(design->name))
                .arg(QString::fromStdString(hull_spec(design->hull).name))
                .arg(components.isEmpty() ? "none" : components.join(", ")));
    };

    auto* refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    refreshTimer->setInterval(0);
    connect(refreshTimer, &QTimer::timeout, this, refresh);
    connect(scene_, &QGraphicsScene::selectionChanged, this, [this, refreshTimer] {
        if (!shuttingDown_) refreshTimer->start();
    });
    connect(scene_, &QGraphicsScene::changed, this, [this, refreshTimer](const QList<QRectF>&) {
        if (!shuttingDown_) refreshTimer->start();
    });
    refresh();
}

} // namespace suns
