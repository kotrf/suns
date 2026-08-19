#pragma once

#include "suns/game_state.hpp"

#include <QColor>
#include <QGraphicsItem>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace suns {

class StarItem final : public QGraphicsItem {
public:
    StarItem(StarId id, QColor color, bool surveyed, bool colony, QGraphicsItem* parent = nullptr);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void setVisualStyle(const QColor& color, qreal scale);

private:
    QColor color_;
    qreal visualScale_{1.0};
    bool surveyed_{};
    bool colony_{};
};

} // namespace suns
