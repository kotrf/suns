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

private:
    QColor color_;
    bool surveyed_{};
    bool colony_{};
};

} // namespace suns
