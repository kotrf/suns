#include "star_item.hpp"

#include <QCursor>
#include <QPainter>
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
    setData(0, static_cast<unsigned int>(id));
    setCursor(QCursor(Qt::PointingHandCursor));
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
}

QRectF StarItem::boundingRect() const
{
    // Large enough for the biggest population-mode marker and its selection halo.
    return {-38.0, -38.0, 76.0, 76.0};
}

void StarItem::setVisualStyle(const QColor& color, qreal scale)
{
    scale = std::clamp<qreal>(scale, 0.50, 2.0);
    if (color_ == color && std::abs(visualScale_ - scale) < 0.0001) return;
    color_ = color;
    visualScale_ = scale;
    update();
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
    painter->drawEllipse(QPointF(0.0, 0.0), 17.0 * scale, 17.0 * scale);

    QColor innerHalo = color_;
    innerHalo.setAlphaF(0.22 * visibility);
    painter->setBrush(innerHalo);
    painter->drawEllipse(QPointF(0.0, 0.0), 11.0 * scale, 11.0 * scale);

    QRadialGradient gradient(QPointF(0.0, 0.0), 6.5 * scale);
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
    const auto coreRadius = (surveyed_ ? 6.0 : 4.7) * scale;
    painter->drawEllipse(QPointF(0.0, 0.0), coreRadius, coreRadius);

    if (!surveyed_) {
        QPen unknownPen(QColor(130, 140, 155, 105));
        unknownPen.setWidthF(1.0);
        unknownPen.setStyle(Qt::DashLine);
        painter->setPen(unknownPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), 9.5 * scale, 9.5 * scale);
    }

    if (colony_) {
        QPen colonyPen(QColor(92, 210, 142, 190));
        colonyPen.setWidthF(1.2);
        painter->setPen(colonyPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), 10.8 * scale, 10.8 * scale);
    }

    if (isSelected()) {
        const auto selectionRadius = std::max<qreal>(14.5, 14.5 * scale);
        QPen selectionPen(QColor(105, 165, 255, 235));
        selectionPen.setWidthF(1.7);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), selectionRadius, selectionRadius);

        QPen outerSelection(QColor(105, 165, 255, 85));
        outerSelection.setWidthF(1.0);
        painter->setPen(outerSelection);
        painter->drawEllipse(QPointF(0.0, 0.0), selectionRadius + 3.5, selectionRadius + 3.5);
    }
}

} // namespace suns
