#pragma once

#include "suns/game_state.hpp"

#include <QColor>
#include <QGraphicsItem>
#include <functional>

class QGraphicsSimpleTextItem;
class QPainterPath;

namespace suns {

inline constexpr int kMapItemOrbit = 4;
inline constexpr int kMapLabelStar = 5;

// One screen-sized marker per occupied location; fleet IDs are stored in data(2).
class OrbitRingItem final : public QGraphicsItem {
public:
    OrbitRingItem(StarId star, QColor color, bool selected, QGraphicsItem* parent);
    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
private:
    QColor color_;
    bool selected_{};
};

class FleetMarkerItem final : public QGraphicsItem {
public:
    FleetMarkerItem(FleetId fleet, QColor color, bool selected, const QString& name,
        std::function<void()> labelVisibilityChanged);
    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
private:
    QColor color_;
    bool selected_{};
    QGraphicsSimpleTextItem* label_{};
    std::function<void()> labelVisibilityChanged_;
};

} // namespace suns
