#include "main_window.hpp"
#include "map_marker_items.hpp"

#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QTimer>

#include <algorithm>

namespace suns {

void MainWindow::queueMapLabelRefresh()
{
    if (mapLabelsRefreshPending_ || shuttingDown_) return;
    mapLabelsRefreshPending_ = true;
    QTimer::singleShot(0, this, [this] {
        mapLabelsRefreshPending_ = false;
        if (!shuttingDown_) refreshMapLabels();
    });
}

void MainWindow::refreshMapLabels()
{
    if (!scene_ || !view_) return;
    std::vector<QGraphicsSimpleTextItem*> labels;
    std::vector<QRectF> markers;
    std::vector<QRectF> occupied;
    const auto transform = view_->viewportTransform();
    const QRectF viewport = view_->viewport()->rect();
    for (auto* item : scene_->items()) {
        if (item->data(1).toInt() == kMapLabelStar) {
            if (auto* label = dynamic_cast<QGraphicsSimpleTextItem*>(item)) labels.push_back(label);
        } else if (item->data(1).toInt() == 1 || item->data(1).toInt() == 2
            || item->data(1).toInt() == kMapItemOrbit) {
            markers.push_back(item->deviceTransform(transform).mapRect(item->shape().boundingRect()));
            // Keep selected/hovered fleet names clear as well.
            if (item->data(1).toInt() == 2) for (auto* child : item->childItems()) {
                if (child->isVisible()) occupied.push_back(child->deviceTransform(transform).mapRect(child->boundingRect()));
            }
        }
    }
    const auto priority = [this](const QGraphicsSimpleTextItem* label) {
        return selection_.star && label->data(0).toUInt() == *selection_.star
            ? 3 : label->data(2).toInt();
    };
    std::sort(labels.begin(), labels.end(), [&](const auto* a, const auto* b) {
        if (priority(a) != priority(b)) return priority(a) > priority(b);
        return a->data(0).toUInt() < b->data(0).toUInt();
    });
    for (auto* label : labels) {
        label->hide();
        const auto width = label->boundingRect().width();
        const auto height = label->boundingRect().height();
        const auto origin = label->parentItem()->deviceTransform(transform).map(QPointF{});
        if (!viewport.adjusted(-12, -12, 12, 12).contains(origin)) continue;
        const QPointF positions[] = {{13, -height / 2}, {-width - 13, -height / 2},
            {-width / 2, 13}, {-width / 2, -height - 13}};
        for (const auto& position : positions) {
            label->setPos(position);
            const auto area = label->deviceTransform(transform).mapRect(label->boundingRect()).adjusted(-2, -1, 2, 1);
            if (!viewport.contains(area)) continue;
            const auto intersects = [&area](const QRectF& other) { return area.intersects(other); };
            if (std::any_of(markers.begin(), markers.end(), intersects)
                || std::any_of(occupied.begin(), occupied.end(), intersects)) continue;
            label->show();
            occupied.push_back(area);
            break;
        }
    }
}

} // namespace suns
