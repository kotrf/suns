#include "star_item.hpp"

#include <QCursor>
#include <QPainter>
#include <QRadialGradient>
#include <QStyleOptionGraphicsItem>

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
    return {-19.0, -19.0, 38.0, 38.0};
}

void StarItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const qreal visibility = surveyed_ ? 1.0 : 0.42;

    QColor outerHalo = color_;
    outerHalo.setAlphaF(0.10 * visibility);
    painter->setPen(QPen(Qt::NoPen));
    painter->setBrush(outerHalo);
    painter->drawEllipse(QPointF(0.0, 0.0), 17.0, 17.0);

    QColor innerHalo = color_;
    innerHalo.setAlphaF(0.22 * visibility);
    painter->setBrush(innerHalo);
    painter->drawEllipse(QPointF(0.0, 0.0), 11.0, 11.0);

    QRadialGradient gradient(QPointF(0.0, 0.0), 6.5);
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
    painter->drawEllipse(QPointF(0.0, 0.0), surveyed_ ? 6.0 : 4.7, surveyed_ ? 6.0 : 4.7);

    if (!surveyed_) {
        QPen unknownPen(QColor(130, 140, 155, 105));
        unknownPen.setWidthF(1.0);
        unknownPen.setStyle(Qt::DashLine);
        painter->setPen(unknownPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), 9.5, 9.5);
    }

    if (colony_) {
        QPen colonyPen(QColor(92, 210, 142, 190));
        colonyPen.setWidthF(1.2);
        painter->setPen(colonyPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), 10.8, 10.8);
    }

    if (isSelected()) {
        QPen selectionPen(QColor(105, 165, 255, 235));
        selectionPen.setWidthF(1.7);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), 14.5, 14.5);

        QPen outerSelection(QColor(105, 165, 255, 85));
        outerSelection.setWidthF(1.0);
        painter->setPen(outerSelection);
        painter->drawEllipse(QPointF(0.0, 0.0), 18.0, 18.0);
    }
}

} // namespace suns
