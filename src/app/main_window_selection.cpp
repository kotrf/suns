#include "main_window.hpp"
#include "suns/communications.hpp"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <optional>

namespace suns {

namespace {

constexpr int kMapItemStar = 1;
constexpr int kMapItemFleet = 2;

} // namespace

void MainWindow::rememberMapSelection(int kind, std::uint32_t id)
{
    if ((kind != kMapItemStar && kind != kMapItemFleet) || id == 0) return;
    if (currentDistanceSelectionKind_ == kind && currentDistanceSelectionId_ == id) return;
    previousDistanceSelectionKind_ = currentDistanceSelectionKind_;
    previousDistanceSelectionId_ = currentDistanceSelectionId_;
    currentDistanceSelectionKind_ = kind;
    currentDistanceSelectionId_ = id;
}

QString MainWindow::selectedObjectDistanceSummary() const
{
    struct ObjectView {
        QString name;
        Position position;
    };
    const auto resolve = [this](int kind, std::uint32_t id) -> std::optional<ObjectView> {
        if (kind == kMapItemStar) {
            if (const auto* star = find_star(state_, static_cast<StarId>(id))) {
                return ObjectView{QString::fromStdString(star->name), star->position};
            }
        } else if (kind == kMapItemFleet) {
            const auto fleet = std::find_if(state_.fleets.begin(), state_.fleets.end(), [id](const Fleet& candidate) {
                return candidate.id == static_cast<FleetId>(id);
            });
            if (fleet != state_.fleets.end()) {
                const auto visible = fleet_player_view(state_, *fleet);
                return ObjectView{QString::fromStdString(visible.name), visible.position};
            }
        }
        return std::nullopt;
    };

    const auto current = resolve(currentDistanceSelectionKind_, currentDistanceSelectionId_);
    const auto previous = resolve(previousDistanceSelectionKind_, previousDistanceSelectionId_);
    if (!current) return "Distance: select an object";
    if (!previous) return QString("Distance: select another object after %1").arg(current->name);
    return QString("Distance: %1 ↔ %2 • %3 ly")
        .arg(previous->name, current->name)
        .arg(distance_between(previous->position, current->position), 0, 'f', 1);
}

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
        const auto id = static_cast<std::uint32_t>(item->data(0).toUInt());
        if (routeProgramMapTargetPickActive_) {
            if (kind == kMapItemFleet) {
                const auto target = std::find_if(
                    state_.fleets.begin(), state_.fleets.end(), [id](const Fleet& fleet) {
                        return fleet.id == static_cast<FleetId>(id);
                    });
                const auto source = selectedFleet();
                if (target == state_.fleets.end() || target->owner != pendingOrders_.player
                    || (source && target->id == source->id)) {
                    statusBar()->showMessage(
                        "Choose another friendly fleet, or press Esc to cancel", 3000);
                    return;
                }
            } else if (kind == kMapItemStar) {
                selectedStarId_ = static_cast<StarId>(id);
            } else {
                return;
            }

            rememberMapSelection(kind, id);
            cancelRouteProgramMapTargetPick();
            emit routeProgramMapTargetPicked(kind, id);

            // Restore the source-fleet highlight after the target item caused
            // QGraphicsScene's ordinary selection to move to itself.
            if (!mapSelectionRebuildPending_) {
                mapSelectionRebuildPending_ = true;
                QTimer::singleShot(0, this, [this] {
                    mapSelectionRebuildPending_ = false;
                    if (!shuttingDown_) rebuildScene();
                });
            }
            return;
        }
        if (kind == kMapItemStar) {
            selectedStarId_ = static_cast<StarId>(id);
        } else if (kind == kMapItemFleet) {
            selectedFleetId_ = static_cast<FleetId>(id);
        } else {
            return;
        }
        rememberMapSelection(kind, id);
        emit routeProgramContextChanged();

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
