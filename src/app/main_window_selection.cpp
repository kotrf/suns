#include "main_window.hpp"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QTimer>

namespace suns {

namespace {

constexpr int kMapItemStar = 1;
constexpr int kMapItemFleet = 2;

} // namespace

void MainWindow::installDeferredMapSelectionHandler()
{
    if (!scene_) return;

    // The constructor historically rebuilt the scene synchronously from
    // selectionChanged. Disconnect that callback before installing the safe
    // version below. At this point selectionChanged is the only scene signal
    // connected to MainWindow.
    scene_->disconnect(this);

    connect(scene_, &QGraphicsScene::selectionChanged, this, [this] {
        if (shuttingDown_) return;

        const auto selected = scene_->selectedItems();
        if (selected.isEmpty()) return;

        const auto* item = selected.front();
        const auto kind = item->data(1).toInt();
        if (kind == kMapItemStar) {
            selectedStarId_ = static_cast<StarId>(item->data(0).toUInt());
        } else if (kind == kMapItemFleet) {
            selectedFleetId_ = static_cast<FleetId>(item->data(0).toUInt());
        } else {
            return;
        }

        // Never clear/delete QGraphicsItems while Qt is still delivering the
        // selectionChanged event that references them. Multiple changes in the
        // same event-loop turn collapse into one redraw.
        if (mapSelectionRebuildPending_) return;
        mapSelectionRebuildPending_ = true;
        QTimer::singleShot(0, this, [this] {
            mapSelectionRebuildPending_ = false;
            if (shuttingDown_) return;
            rebuildScene();
        });
    });
}

} // namespace suns
