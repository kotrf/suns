#include "map_marker_items.hpp"

#include <QCursor>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPainterPath>

#include <utility>

namespace suns {

OrbitRingItem::OrbitRingItem(StarId star, QColor color, bool selected, QGraphicsItem* parent)
    : QGraphicsItem(parent), color_(std::move(color)), selected_(selected)
{
    setData(0, static_cast<unsigned int>(star));
    setData(1, kMapItemOrbit);
    setFlag(ItemIsSelectable);
    setCursor(QCursor(Qt::PointingHandCursor));
    setZValue(2);
}

QRectF OrbitRingItem::boundingRect() const { return {-12, -12, 24, 24}; }

QPainterPath OrbitRingItem::shape() const
{
    QPainterPath path;
    path.addEllipse(QPointF{}, 11.5, 11.5);
    path.addEllipse(QPointF{}, 7.5, 7.5);
    path.setFillRule(Qt::OddEvenFill);
    return path;
}

void OrbitRingItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color_, selected_ ? 1.6 : 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF{}, 9.5, 9.5);
}

FleetMarkerItem::FleetMarkerItem(FleetId fleet, QColor color, bool selected, const QString& name,
    std::function<void()> labelVisibilityChanged)
    : color_(std::move(color)), selected_(selected), labelVisibilityChanged_(std::move(labelVisibilityChanged))
{
    setData(0, static_cast<unsigned int>(fleet));
    setData(1, 2);
    setFlag(ItemIsSelectable);
    setFlag(ItemIgnoresTransformations);
    setAcceptHoverEvents(true);
    setCursor(QCursor(Qt::PointingHandCursor));
    setZValue(10);
    label_ = new QGraphicsSimpleTextItem(this);
    QFont font;
    font.setPixelSize(11);
    label_->setFont(font);
    label_->setText(QFontMetricsF(font).elidedText(name, Qt::ElideRight, 150));
    label_->setBrush(color_.lighter(150));
    label_->setPos(8, -7);
    label_->setAcceptedMouseButtons(Qt::NoButton);
    label_->setVisible(selected_);
}

QRectF FleetMarkerItem::boundingRect() const { return {-8, -8, 16, 16}; }

QPainterPath FleetMarkerItem::shape() const
{
    QPainterPath path;
    path.addEllipse(QPointF{}, 5, 5);
    return path;
}

void FleetMarkerItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color_.lighter(130), 0.8));
    painter->setBrush(color_);
    painter->drawPolygon(QPolygonF{{0, -4}, {3.5, 3}, {0, 1.5}, {-3.5, 3}});
    if (data(2).toList().size() > 1) {
        painter->drawLine(QPointF(-3, 5), QPointF(3, 5));
    }
    if (selected_) {
        painter->setPen(QPen(color_.lighter(160), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(-6.5, -6.5, 13, 13));
    }
}

void FleetMarkerItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    label_->show();
    if (labelVisibilityChanged_) labelVisibilityChanged_();
}

void FleetMarkerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    label_->setVisible(selected_);
    if (labelVisibilityChanged_) labelVisibilityChanged_();
}

} // namespace suns
