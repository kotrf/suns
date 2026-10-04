#include "star_item.hpp"

#include <QCursor>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>
#include <utility>

namespace suns {

StarItem::StarItem(StarId id, QColor color, bool surveyed, bool colony, QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , color_(std::move(color))
    , surveyed_(surveyed)
    , colony_(colony)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    setData(0, static_cast<unsigned int>(id));
    setCursor(QCursor(Qt::PointingHandCursor));
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
}

QRectF StarItem::boundingRect() const
{
    // Screen-sized, including the largest population marker and small glow.
    return {-15.0, -15.0, 30.0, 30.0};
}

QPainterPath StarItem::shape() const
{
    // Rendering needs room for the glow and selection rings, but those empty
    // pixels must not steal clicks from neighboring systems.
    const auto coreRadius = (surveyed_ ? 3.2 : 2.8) * visualScale_;
    const auto hitRadius = std::max<qreal>(4.0, coreRadius + 1.5);
    QPainterPath path;
    path.addEllipse(QPointF{}, hitRadius, hitRadius);
    return path;
}

bool StarItem::setVisualStyle(const QColor& color, qreal scale)
{
    scale = std::clamp<qreal>(scale, 0.50, 2.0);
    if (color_ == color && std::abs(visualScale_ - scale) < 0.0001) return false;
    if (std::abs(visualScale_ - scale) >= 0.0001) prepareGeometryChange();
    color_ = color;
    visualScale_ = scale;
    update();
    return true;
}

void StarItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const qreal visibility = surveyed_ ? 1.0 : 0.42;
    const qreal scale = visualScale_;

    QColor outerHalo = color_;
    outerHalo.setAlphaF(0.10 * visibility);
    painter->setPen(QPen(Qt::NoPen));
    painter->setBrush(outerHalo);
    painter->drawEllipse(QPointF(0.0, 0.0), 6.5 * scale, 6.5 * scale);

    QColor innerHalo = color_;
    innerHalo.setAlphaF(0.22 * visibility);
    painter->setBrush(innerHalo);
    painter->drawEllipse(QPointF(0.0, 0.0), 4.5 * scale, 4.5 * scale);

    QRadialGradient gradient(QPointF(0.0, 0.0), 3.5 * scale);
    QColor center = Qt::white;
    center.setAlphaF(0.95 * visibility + 0.05);
    QColor middle = color_.lighter(118);
    middle.setAlphaF(0.95 * visibility + 0.05);
    QColor edge = color_.darker(125);
    edge.setAlphaF(0.9 * visibility + 0.05);
    gradient.setColorAt(0.0, center);
    gradient.setColorAt(0.38, middle);
    gradient.setColorAt(1.0, edge);
    painter->setBrush(gradient);
    const auto coreRadius = (surveyed_ ? 3.2 : 2.8) * scale;
    painter->drawEllipse(QPointF(0.0, 0.0), coreRadius, coreRadius);

    if (!surveyed_) {
        QPen unknownPen(QColor(130, 140, 155, 105));
        unknownPen.setWidthF(1.0);
        unknownPen.setStyle(Qt::DashLine);
        painter->setPen(unknownPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), 4.8 * scale, 4.8 * scale);
    }

    if (colony_) {
        QPen colonyPen(QColor(92, 210, 142, 190));
        colonyPen.setWidthF(0.8);
        painter->setPen(colonyPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), 5.8 * scale, 5.8 * scale);
    }

    if (isSelected()) {
        const auto selectionRadius = std::max<qreal>(6.5, coreRadius + 2.5);
        QPen selectionPen(QColor(105, 165, 255, 235));
        selectionPen.setWidthF(1.1);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        // Corner brackets distinguish selection from an occupied-orbit ring.
        for (const auto x : {-1.0, 1.0}) for (const auto y : {-1.0, 1.0}) {
            const QPointF corner(x * selectionRadius, y * selectionRadius);
            painter->drawLine(corner, corner - QPointF(x * 3.0, 0));
            painter->drawLine(corner, corner - QPointF(0, y * 3.0));
        }
    }
}

} // namespace suns
