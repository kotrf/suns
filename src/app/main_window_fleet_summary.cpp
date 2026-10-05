#include "main_window.hpp"
#include "suns/communications.hpp"
#include "suns/wormholes.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace suns {
namespace {

constexpr int kNumericSortRole = Qt::UserRole + 1;

class FleetSummaryItem final : public QTreeWidgetItem {
public:
    explicit FleetSummaryItem(QTreeWidget* tree) : QTreeWidgetItem(tree) {}

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const auto column = treeWidget()->sortColumn();
        const auto value = data(column, kNumericSortRole);
        if (value.isValid()) return value.toDouble() < other.data(column, kNumericSortRole).toDouble();
        return QString::localeAwareCompare(text(column), other.text(column)) < 0;
    }
};

QString amount(double value, double capacity)
{
    return QString("%1 / %2").arg(value, 0, 'f', 1).arg(capacity, 0, 'f', 1);
}

QString composition(const GameState& state, const Fleet& fleet)
{
    QStringList ships;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        ships << QString("%1× %2").arg(stack.count)
            .arg(design ? QString::fromStdString(design->name) : QString("Design %1").arg(stack.design));
    }
    return ships.join("\n");
}

} // namespace

QString MainWindow::fleetLocationName(Position position) const
{
    const auto star = std::find_if(state_.stars.begin(), state_.stars.end(), [&](const auto& candidate) {
        return same_position(candidate.position, position);
    });
    if (star != state_.stars.end()) return QString("Orbit: %1").arg(QString::fromStdString(star->name));
    return QString("Space (%1, %2)").arg(position.x, 0, 'f', 1).arg(position.y, 0, 'f', 1);
}

void MainWindow::openFleetSummaryDialog()
{
    if (!fleetSummaryDialog_) {
        auto* dialog = new QDialog(this);
        fleetSummaryDialog_ = dialog;
        dialog->setObjectName("fleetSummaryDialog");
        dialog->setWindowTitle("Fleet summary — my fleets");
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->resize(1080, 460);
        auto* layout = new QVBoxLayout(dialog);
        auto* search = new QLineEdit(dialog);
        search->setObjectName("fleetSummarySearch");
        search->setPlaceholderText("Find a fleet, system or route…");
        search->setClearButtonEnabled(true);
        layout->addWidget(search);

        auto* table = new QTreeWidget(dialog);
        table->setObjectName("fleetSummaryTable");
        table->setHeaderLabels({"Fleet", "Location", "Ships", "Fuel", "Cargo (kt)", "Route / task", "Comms"});
        table->setRootIsDecorated(false);
        table->setAlternatingRowColors(true);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setSortingEnabled(true);
        table->sortByColumn(0, Qt::AscendingOrder);
        table->header()->setStretchLastSection(true);
        for (int column = 0; column < 7; ++column)
            table->setColumnWidth(column, std::array{170, 180, 55, 115, 115, 230, 160}[column]);
        layout->addWidget(table, 1);

        auto* totals = new QLabel(dialog);
        totals->setObjectName("fleetSummaryTotals");
        totals->setTextFormat(Qt::PlainText);
        totals->setWordWrap(true);
        layout->addWidget(totals);
        auto* hint = new QLabel("Double-click a fleet or press Enter to select it and center the map.", dialog);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        auto* show = buttons->addButton("Show on map", QDialogButtonBox::ActionRole);
        show->setObjectName("fleetSummaryShowButton");
        show->setEnabled(false);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
        connect(table, &QTreeWidget::currentItemChanged, dialog, [show](QTreeWidgetItem* item) {
            show->setEnabled(item != nullptr);
        });
        const auto activate = [this, table] {
            const auto* item = table->currentItem();
            if (!item) return;
            const auto id = static_cast<FleetId>(item->data(0, Qt::UserRole).toUInt());
            cancelRouteProgramMapTargetPick();
            if (!selectFleetForRouteProgram(id)) return;
            const auto* fleet = selectedFleet();
            if (!fleet) return;
            const auto visible = fleet_player_view(state_, *fleet);
            selection_.star.reset();
            for (const auto& star : state_.stars) {
                if (!same_position(star.position, visible.position)) continue;
                selection_.star = star.id;
                break;
            }
            // Resolve the ID anew: the game state can change while this
            // non-modal window remains open across turns and save loads.
            rebuildScene();
            view_->centerOn(visible.position.x, visible.position.y);
            if (auto* dock = findChild<QDockWidget*>("fleetDock")) {
                dock->show();
                dock->raise();
            }
        };
        connect(show, &QPushButton::clicked, dialog, activate);
        connect(table, &QTreeWidget::itemActivated, dialog, [activate](QTreeWidgetItem*, int) { activate(); });
        connect(search, &QLineEdit::textChanged, dialog, [this] { refreshFleetSummary(); });
        connect(this, &MainWindow::routeProgramContextChanged, dialog, [this] {
            if (fleetSummaryDialog_ && fleetSummaryDialog_->isVisible()) refreshFleetSummary();
        });
    }
    fleetSummaryDialog_->show();
    refreshFleetSummary();
    fleetSummaryDialog_->raise();
    fleetSummaryDialog_->activateWindow();
}

void MainWindow::refreshFleetSummary()
{
    if (!fleetSummaryDialog_) return;
    auto* table = fleetSummaryDialog_->findChild<QTreeWidget*>("fleetSummaryTable");
    auto* totals = fleetSummaryDialog_->findChild<QLabel*>("fleetSummaryTotals");
    const auto query = fleetSummaryDialog_->findChild<QLineEdit*>("fleetSummarySearch")->text().trimmed();
    const auto selected = table->currentItem()
        ? table->currentItem()->data(0, Qt::UserRole).toUInt() : selectedFleetForRouteProgram();
    const auto scroll = table->verticalScrollBar()->value();
    const QSignalBlocker blocker(table);
    table->setSortingEnabled(false);
    table->clear();

    auto contacts = state_.fleets;
    const auto missing = wormhole_missing_contacts(state_, pendingOrders_.player);
    contacts.insert(contacts.end(), missing.begin(), missing.end());
    std::sort(contacts.begin(), contacts.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    FleetId previous{};
    std::uint64_t ships{};
    std::size_t fleets{};
    for (const auto& source : contacts) {
        if (source.owner != pendingOrders_.player || source.id == previous) continue;
        previous = source.id;
        auto fleet = fleet_player_view(state_, source);
        const bool awaitingWormholeReport = std::any_of(state_.wormholeTransits.begin(), state_.wormholeTransits.end(), [&](const auto& transit) {
            return transit.lastContact.id == source.id && transit.lastContact.owner == pendingOrders_.player
                && transit.status != WormholeTransitStatus::PresumedLost
                && transit.status != WormholeTransitStatus::EmergenceConfirmed;
        });
        QString route;
        bool queuedRoute = false;
        for (const auto& order : pendingOrders_.orders) {
            if (const auto* rename = std::get_if<RenameFleetOrder>(&order); rename && rename->fleet == fleet.id)
                fleet.name = rename->name;
            if (const auto* move = std::get_if<MoveFleetOrder>(&order); move && move->fleet == fleet.id) {
                queuedRoute = true;
                const bool clear = move->clearRoute || (same_position(fleet.position, move->destination)
                    && move->targetFleet == 0 && move->queuedWaypoints.empty()
                    && move->arrivalAction.kind == FleetArrivalActionKind::None);
                fleet.destination = clear ? std::nullopt : std::optional<Position>{move->destination};
                fleet.targetFleet = move->targetFleet;
                fleet.warp = move->warp;
                fleet.waypointQueue = move->queuedWaypoints;
                fleet.arrivalAction = move->arrivalAction;
                if (clear) fleet.task = FleetTask::None;
            }
        }
        if (awaitingWormholeReport) route = "WH transit — last contact";
        else if (fleet.destination) {
            route = fleet.targetFleet != 0 ? QString("Fleet %1").arg(fleet.targetFleet)
                : fleetLocationName(*fleet.destination).remove("Orbit: ");
            route += QString(" • W%1").arg(fleet.warp);
            if (fleet.targetFleet == 0) route += QString(" • ETA %1").arg(fleet_eta(fleet));
            if (!fleet.waypointQueue.empty()) route += QString(" • +%1 legs").arg(fleet.waypointQueue.size());
        } else route = fleet.task == FleetTask::RemoteMining ? "Remote mining" : "Idle";
        if (queuedRoute) route.prepend("Queued: ");
        const auto age = fleet_telemetry_age(state_, fleet);
        const auto comms = awaitingWormholeReport ? QString("Last contact • %1 yr old").arg(age)
            : QString("%1 • %2").arg(communication_delay_turns(state_, fleet.owner, fleet.position) == 0 && age == 0 ? "LIVE" : "Delayed",
                age == 0 ? QString("current") : QString("%1 yr old").arg(age));
        const auto count = fleet_ship_count(fleet);
        ++fleets;
        ships += count;
        const QStringList columns{QString("%1 (#%2)").arg(QString::fromStdString(fleet.name)).arg(fleet.id),
            awaitingWormholeReport ? QString("Last contact: %1").arg(fleetLocationName(fleet.position)) : fleetLocationName(fleet.position),
            QString::number(count), amount(fleet.fuel, fleet_fuel_capacity(state_, fleet)),
            amount(fleet_cargo_used(state_, fleet), fleet_cargo_capacity(state_, fleet)), route, comms};
        if (!query.isEmpty() && !columns.join(" ").contains(query, Qt::CaseInsensitive)) continue;
        auto* row = new FleetSummaryItem(table);
        for (int column = 0; column < columns.size(); ++column) {
            row->setText(column, columns[column]);
            row->setToolTip(column, columns[column]);
        }
        row->setData(0, Qt::UserRole, static_cast<quint32>(fleet.id));
        row->setData(2, kNumericSortRole, count);
        row->setData(3, kNumericSortRole, fleet.fuel);
        row->setData(4, kNumericSortRole, fleet_cargo_used(state_, fleet));
        row->setData(6, kNumericSortRole, static_cast<qulonglong>(age));
        row->setToolTip(2, composition(state_, fleet));
        if (fleet.id == selected) table->setCurrentItem(row);
    }
    table->setSortingEnabled(true);
    table->verticalScrollBar()->setValue(scroll);
    fleetSummaryDialog_->findChild<QPushButton*>("fleetSummaryShowButton")->setEnabled(table->currentItem() != nullptr);
    totals->setText(QString("%1 of %2 fleets shown • %3 ships in all fleets • Turn %4")
        .arg(table->topLevelItemCount()).arg(fleets).arg(ships).arg(state_.turn));
}

} // namespace suns
