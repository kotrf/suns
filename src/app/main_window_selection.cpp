#include "main_window.hpp"
#include "map_marker_items.hpp"
#include "suns/wormholes.hpp"
#include "suns/communications.hpp"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMenu>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <optional>

namespace suns {

namespace {

constexpr int kMapItemStar = 1;
constexpr int kMapItemFleet = 2;

} // namespace

QGraphicsItem* MainWindow::mapObjectAtViewportPosition(const QPoint& position) const
{
    if (!view_) return nullptr;
    QGraphicsItem* closest = nullptr;
    auto closestDistance = std::numeric_limits<qreal>::max();
    // QGraphicsView queries item shapes, not their generous painting bounds.
    // When visible discs overlap, prefer the center nearest the pointer. Equal
    // distances retain Qt's stacking order; a core wins over its own ring.
    for (auto* item : view_->items(position)) {
        const auto kind = item->data(1).toInt();
        if ((kind != kMapItemStar && kind != kMapItemFleet && kind != kMapItemOrbit)
            || item->data(0).toUInt() == 0
            || !(item->flags() & QGraphicsItem::ItemIsSelectable)) continue;
        const auto center = view_->viewportTransform().map(
            item->mapToScene(item->boundingRect().center()));
        const auto delta = center - QPointF(position);
        const auto distance = delta.x() * delta.x() + delta.y() * delta.y();
        if (distance < closestDistance
            || (distance == closestDistance && closest
                && closest->data(1).toInt() == kMapItemOrbit && kind == kMapItemStar)) {
            closest = item;
            closestDistance = distance;
        }
    }
    return closest;
}

void MainWindow::showMapFleetPicker(const QVariantList& fleets, const QPoint& position, bool target, bool quick)
{
    if (mapFleetPicker_) mapFleetPicker_->close();
    auto* menu = new QMenu(this);
    mapFleetPicker_ = menu;
    menu->setObjectName("mapFleetPicker");
    menu->addSection(target ? "Choose target fleet" : "Fleets at this location");
    auto contacts = state_.fleets;
    const auto missing = missing_fleet_contacts(state_, pendingOrders_.player);
    contacts.insert(contacts.end(), missing.begin(), missing.end());
    for (const auto& value : fleets) {
        const auto id = static_cast<FleetId>(value.toUInt());
        const auto it = std::find_if(contacts.begin(), contacts.end(),
            [id](const Fleet& fleet) { return fleet.id == id; });
        if (it == contacts.end()) continue;
        const bool own = it->owner == pendingOrders_.player;
        QPixmap swatch(8, 8);
        swatch.fill(own ? QColor("#dce9f4") : QColor("#dd7777"));
        const auto name = QString::fromStdString(it->name);
        auto* action = menu->addAction(QIcon(swatch), own ? name : QString("Empire %1 — %2").arg(it->owner).arg(name));
        action->setData(static_cast<unsigned int>(id));
        action->setCheckable(!target);
        action->setChecked(!target && selection_.fleet && *selection_.fleet == id);
        action->setEnabled(!target || selectedFleetForRouteProgram() != id);
        connect(action, &QAction::triggered, this, [this, id, target, quick, menu] {
            if (target) {
                if (!selectRouteProgramMapTarget(2, id)) return;
                cancelRouteProgramMapTargetPick();
                if (quick) emit routeProgramQuickTargetRequested(2, id);
            } else if (!selectWorkspaceObject(2, id)) return;
            menu->close();
            queueMapSelectionRebuild();
        });
    }
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(view_->viewport()->mapToGlobal(position) + QPoint(5, 8));
}

void MainWindow::queueMapSelectionRebuild()
{
    // Never delete scene items while Qt is still delivering a click/selection
    // event. Multiple selection changes in one event turn share one redraw.
    if (mapSelectionRebuildPending_) return;
    mapSelectionRebuildPending_ = true;
    QTimer::singleShot(0, this, [this] {
        mapSelectionRebuildPending_ = false;
        if (!shuttingDown_) rebuildScene();
    });
}

void MainWindow::rememberMapSelection(int kind, std::uint32_t id)
{
    if ((kind != kMapItemStar && kind != kMapItemFleet) || id == 0) return;
    if (currentDistanceSelectionKind_ == kind && currentDistanceSelectionId_ == id) return;
    previousDistanceSelectionKind_ = currentDistanceSelectionKind_;
    previousDistanceSelectionId_ = currentDistanceSelectionId_;
    currentDistanceSelectionKind_ = kind;
    currentDistanceSelectionId_ = id;
}

bool MainWindow::selectWorkspaceObject(int kind, std::uint32_t id)
{
    if (id == 0) return false;
    if (kind == kMapItemStar) {
        if (!find_star(state_, static_cast<StarId>(id))) return false;
        selection_.star = static_cast<StarId>(id);
    } else if (kind == kMapItemFleet) {
        const auto fleet = std::find_if(state_.fleets.begin(), state_.fleets.end(), [id](const Fleet& candidate) {
            return candidate.id == static_cast<FleetId>(id);
        });
        const auto missing = missing_fleet_contacts(state_, pendingOrders_.player);
        if (fleet == state_.fleets.end() && std::none_of(missing.begin(), missing.end(),
            [=](const auto& contact) { return contact.id == id; })) return false;
        selection_.fleet = static_cast<FleetId>(id);
    } else {
        return false;
    }
    rememberMapSelection(kind, id);
    emit routeProgramContextChanged();
    return true;
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
            if (!selectRouteProgramMapTarget(kind, id)) return;
            cancelRouteProgramMapTargetPick();

            // Restore the source-fleet highlight after the target item caused
            // QGraphicsScene's ordinary selection to move to itself.
            queueMapSelectionRebuild();
            return;
        }
        if (!selectWorkspaceObject(kind, id)) return;
        if (kind == kMapItemStar) emit routeProgramMapTargetPicked(kind, id);

        queueMapSelectionRebuild();
    });
}

} // namespace suns
