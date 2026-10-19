#include "main_window.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/production.hpp"
#include <QComboBox>
#include <QDockWidget>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace suns {
namespace {
qulonglong key(CargoTransferEndpoint e) { return (qulonglong(e.planet) << 32) | e.fleet; }
CargoTransferEndpoint endpoint(qulonglong k) { return {PlanetId(k >> 32), FleetId(k)}; }
GameState plan(const GameState& s, const PlayerOrders& orders)
{ return planned_production_state(projected_cargo_order_state(make_player_view(s, orders.player).state, orders), orders); }
}
void MainWindow::installExoticMaterials()
{
    if (findChild<QDockWidget*>("exoticMaterialsDock")) return;
    auto* dock = new QDockWidget("Rare materials — xenocrystal", this); dock->setObjectName("exoticMaterialsDock");
    auto* body = new QWidget(dock); auto* layout = new QVBoxLayout(body);
    auto* hint = new QLabel("DeepSurvey reveals rare finite xenocrystal deposits, including on uninhabitable worlds. "
        "Remote Mining with robots, Construction 8 and Electronics 8 extracts one whole unit per mining ship/year, "
        "up to 4 per planet/year, into a physical surface cache. One unit occupies 1 kt of the shared hold. "
        "Received stocks may be old; local transfers require current reports. Mobile-base exchanges occur after production. "
        "Remote voyages use ordinary arrival orders.", body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* tree = new QTreeWidget(body); tree->setObjectName("exoticInventory"); tree->setRootIsDecorated(false);
    tree->setHeaderLabels({"Location", "Known deposit", "Received stock", "Extraction/year", "Evidence", "Laboratory"});
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents); layout->addWidget(tree);
    auto* expedition = new QHBoxLayout;
    auto* fleet = new QComboBox(body); fleet->setObjectName("exoticExpeditionFleet"); expedition->addWidget(fleet);
    auto* colony = new QComboBox(body); colony->setObjectName("exoticReturnColony"); expedition->addWidget(colony);
    const auto button = [&](QHBoxLayout* row, const char* name, const char* label) {
        auto* b = new QPushButton(label, body); b->setObjectName(name); row->addWidget(b); return b;
    };
    auto* survey = button(expedition, "surveyExoticSite", "Fly / deep survey");
    auto* mine = button(expedition, "mineExoticSite", "Fly / remote mine");
    auto* haul = button(expedition, "haulExoticStock", "Collect and deliver"); layout->addLayout(expedition);
    auto* transferRow = new QHBoxLayout;
    auto* from = new QComboBox(body); from->setObjectName("exoticTransferSource"); transferRow->addWidget(from);
    auto* to = new QComboBox(body); to->setObjectName("exoticTransferDestination"); transferRow->addWidget(to);
    auto* amount = new QSpinBox(body); amount->setObjectName("exoticTransferAmount"); amount->setRange(1, kMaximumXenocrystal); amount->setSuffix(" units / kt"); transferRow->addWidget(amount);
    auto* transfer = button(transferRow, "transferXenocrystal", "Queue local transfer"); layout->addLayout(transferRow);
    auto* projects = new QHBoxLayout;
    auto* start = button(projects, "startResonanceLab", "Build / resume laboratory");
    auto* pause = button(projects, "pauseResonanceLab", "Pause construction");
    auto* active = button(projects, "activateResonanceLab", "Toggle operation");
    auto* dismantle = button(projects, "dismantleResonanceLab", "Dismantle"); layout->addLayout(projects);
    auto* details = new QLabel(body); details->setObjectName("exoticProjectDetails"); details->setWordWrap(true); layout->addWidget(details);
    dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide();
    if (auto* menu = menuBar()->findChild<QMenu*>("sunsViewMenu")) menu->addAction(dock->toggleViewAction());
    else menuBar()->addAction(dock->toggleViewAction());
    const auto update = [this] { refreshExoticMaterialControls(); };
    connect(tree, &QTreeWidget::itemSelectionChanged, this, update);
    for (auto* combo : {fleet, colony, from, to}) connect(combo, &QComboBox::currentIndexChanged, this, [update](int) { update(); });
    connect(amount, &QSpinBox::valueChanged, this, [update](int) { update(); });
    connect(transfer, &QPushButton::clicked, this, [=, this] {
        auto known = plan(state_, pendingOrders_);
        const TransferXenocrystalOrder order{endpoint(from->currentData().toULongLong()), endpoint(to->currentData().toULongLong()), std::uint32_t(amount->value())};
        if (!preview_xenocrystal_transfer(known, pendingOrders_.player, order, &pendingOrders_)) return;
        appendPendingOrder(order, "Transfer xenocrystals"); rebuildScene();
    });
    const auto voyage = [=, this](int kind) {
        const auto* row = tree->currentItem(); if (!row) return;
        const auto place = endpoint(row->data(0, Qt::UserRole).toULongLong());
        const auto known = plan(state_, pendingOrders_); const auto* p = find_planet(known, place.planet);
        const auto f = std::find_if(known.fleets.begin(), known.fleets.end(), [&](const auto& f) { return f.id == fleet->currentData().toUInt(); });
        if (!p || f == known.fleets.end()) return;
        const auto* star = find_star(known, p->star); if (!star) return;
        MoveFleetOrder order{f->id, star->position, selectedFleetSuggestedWarpForRouteProgram()};
        order.warp = std::min(order.warp, fleet_max_warp(known, *f));
        if (kind == 1) order.arrivalAction.kind = FleetArrivalActionKind::RemoteMining;
        if (kind == 2) {
            const auto* home = find_planet(known, colony->currentData().toUInt());
            const auto* homeStar = home ? find_star(known, home->star) : nullptr; if (!homeStar) return;
            order.arrivalAction = {FleetArrivalActionKind::LoadAllAvailable, 1, FleetCargoKind::Xenocrystal};
            FleetWaypoint unload; unload.destination = homeStar->position; unload.warp = order.warp;
            unload.arrivalAction = {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Xenocrystal};
            order.queuedWaypoints.push_back(unload);
        }
        appendPendingOrder(order, kind == 0 ? "Deep survey xenocrystal prospect" : kind == 1 ? "Remote mine xenocrystals" : "Collect and deliver xenocrystals");
        rebuildScene();
    };
    connect(survey, &QPushButton::clicked, this, [voyage] { voyage(0); });
    connect(mine, &QPushButton::clicked, this, [voyage] { voyage(1); });
    connect(haul, &QPushButton::clicked, this, [voyage] { voyage(2); });
    const auto project = [=, this](ResonanceAction action) {
        const auto* row = tree->currentItem(); if (!row) return;
        const auto id = endpoint(row->data(0, Qt::UserRole).toULongLong()).planet;
        const auto known = plan(state_, pendingOrders_); const auto* p = find_planet(known, id);
        if (!p || p->owner != pendingOrders_.player) return;
        if (action == ResonanceAction::Dismantle && QMessageBox::question(this, "Dismantle resonance laboratory",
            "Destroy the laboratory and all paid work, minerals and xenocrystals without a refund?") != QMessageBox::Yes) return;
        appendPendingOrder(SetResonanceLaboratoryOrder{id, action, !p->resonanceLab.active}, "Control resonance laboratory"); rebuildScene();
    };
    connect(start, &QPushButton::clicked, this, [project] { project(ResonanceAction::StartOrResume); });
    connect(pause, &QPushButton::clicked, this, [project] { project(ResonanceAction::Pause); });
    connect(active, &QPushButton::clicked, this, [project] { project(ResonanceAction::SetActive); });
    connect(dismantle, &QPushButton::clicked, this, [project] { project(ResonanceAction::Dismantle); });
    connect(this, &MainWindow::routeProgramContextChanged, this, [this] { refreshExoticMaterials(); });
    refreshExoticMaterials();
}
void MainWindow::refreshExoticMaterials()
{
    auto* tree = findChild<QTreeWidget*>("exoticInventory"); if (!tree || !find_player(state_, pendingOrders_.player)) return;
    const auto selected = tree->currentItem() ? tree->currentItem()->data(0, Qt::UserRole).toULongLong() : 0;
    const QSignalBlocker block(tree); tree->clear();
    const auto known = plan(state_, pendingOrders_);
    for (const auto& p : known.planets) {
        const auto observation = known_xenocrystal(known, pendingOrders_.player, p.id);
        auto* row = new QTreeWidgetItem(tree); row->setData(0, Qt::UserRole, QVariant::fromValue(key({p.id, 0})));
        row->setText(0, QString::fromStdString(p.name));
        row->setText(1, observation && observation->depositSurveyed
            ? QString("%1 / %2 units").arg(p.xenocrystal.remaining).arg(p.xenocrystal.original) : "Requires DeepSurvey");
        row->setText(2, p.owner && p.owner != pendingOrders_.player ? "Private colony stock"
            : observation ? QString("%1 units").arg(p.xenocrystal.surface) : "Unknown");
        row->setText(3, observation ? QString("%1 received").arg(p.xenocrystal.extractedLastYear) : "Unknown");
        row->setText(4, observation ? QString("Deposit year %1; stock year %2").arg(qulonglong(observation->depositObservedTurn)).arg(qulonglong(observation->observedTurn)) : "No received rare-material report");
        if (p.owner == pendingOrders_.player) row->setText(5, QString::fromStdString(resonance_block(known, p)));
        if (selected == key({p.id, 0})) tree->setCurrentItem(row);
    }
    for (const auto& f : known.fleets) if (f.owner == pendingOrders_.player) {
        auto* row = new QTreeWidgetItem(tree); row->setData(0, Qt::UserRole, QVariant::fromValue(key({0, f.id})));
        row->setText(0, QString::fromStdString(f.name)); row->setText(2, QString("%1 units / kt").arg(f.xenocrystal));
        row->setText(4, QString("Telemetry year %1").arg(qulonglong(f.telemetry.observedTurn)));
        if (selected == key({0, f.id})) tree->setCurrentItem(row);
    }
    const auto fill = [&](const char* name, auto add) {
        auto* combo = findChild<QComboBox*>(name); const auto old = combo->currentData(); const QSignalBlocker blocker(combo); combo->clear(); add(combo);
        const auto index = combo->findData(old); if (index >= 0) combo->setCurrentIndex(index);
    };
    fill("exoticExpeditionFleet", [&](auto* c) { for (const auto& f : known.fleets) if (f.owner == pendingOrders_.player) c->addItem(QString::fromStdString(f.name), f.id); });
    fill("exoticReturnColony", [&](auto* c) { for (const auto& p : known.planets) if (p.owner == pendingOrders_.player && p.population) c->addItem(QString::fromStdString(p.name), p.id); });
    for (const char* name : {"exoticTransferSource", "exoticTransferDestination"}) fill(name, [&](auto* c) {
        for (const auto& p : known.planets) if ((!p.owner || p.owner == pendingOrders_.player) && known_xenocrystal(known, pendingOrders_.player, p.id))
            c->addItem(QString("%1 — surface: %2 units").arg(QString::fromStdString(p.name)).arg(p.xenocrystal.surface), QVariant::fromValue(key({p.id, 0})));
        for (const auto& f : known.fleets) if (f.owner == pendingOrders_.player)
            c->addItem(QString("%1 — fleet: %2 units").arg(QString::fromStdString(f.name)).arg(f.xenocrystal), QVariant::fromValue(key({0, f.id})));
    });
    refreshExoticMaterialControls();
}
void MainWindow::refreshExoticMaterialControls()
{
    auto* tree = findChild<QTreeWidget*>("exoticInventory"); if (!tree) return;
    auto known = plan(state_, pendingOrders_);
    auto* from = findChild<QComboBox*>("exoticTransferSource"); auto* to = findChild<QComboBox*>("exoticTransferDestination");
    findChild<QPushButton*>("transferXenocrystal")->setEnabled(preview_xenocrystal_transfer(known, pendingOrders_.player,
        {endpoint(from->currentData().toULongLong()), endpoint(to->currentData().toULongLong()), std::uint32_t(findChild<QSpinBox*>("exoticTransferAmount")->value())}, &pendingOrders_));
    known = plan(state_, pendingOrders_);
    const auto place = tree->currentItem() ? endpoint(tree->currentItem()->data(0, Qt::UserRole).toULongLong()) : CargoTransferEndpoint{};
    const auto* p = find_planet(known, place.planet);
    const auto f = std::find_if(known.fleets.begin(), known.fleets.end(), [&](const auto& f) { return f.id == findChild<QComboBox*>("exoticExpeditionFleet")->currentData().toUInt(); });
    const auto observation = p ? known_xenocrystal(known, pendingOrders_.player, p->id) : std::nullopt;
    findChild<QPushButton*>("surveyExoticSite")->setEnabled(p && f != known.fleets.end() && fleet_has_scanner(known, *f));
    findChild<QPushButton*>("mineExoticSite")->setEnabled(p && !p->owner && observation && observation->depositSurveyed
        && observation->site.remaining && f != known.fleets.end() && xenocrystal_mining_rate(known, *f));
    findChild<QPushButton*>("haulExoticStock")->setEnabled(p && (!p->owner || p->owner == pendingOrders_.player) && observation
        && observation->site.surface && f != known.fleets.end() && fleet_cargo_capacity(known, *f) - fleet_cargo_used(known, *f) >= 1
        && findChild<QComboBox*>("exoticReturnColony")->count());
    const bool own = p && p->owner == pendingOrders_.player && p->population;
    findChild<QPushButton*>("startResonanceLab")->setEnabled(own && resonance_technology_available(known, p->owner)
        && p->resonanceLab.work < kResonanceWork && !resonance_scheduled(*p));
    findChild<QPushButton*>("pauseResonanceLab")->setEnabled(own && p->resonanceLab.started && resonance_scheduled(*p));
    findChild<QPushButton*>("activateResonanceLab")->setEnabled(own && p->resonanceLab.work == kResonanceWork);
    findChild<QPushButton*>("dismantleResonanceLab")->setEnabled(own && p->resonanceLab.started);
    QString details = "Resonance laboratory: retained artifacts supply 32 RP/year in their native research fields outside the empire focus, "
        "through ordinary delayed scientific reports. Requires each artifact's delivered study, active research and a nonzero allocation. "
        "Energy 16 / Electronics 16 / Construction 14; friendly Starbase/Fortress + shipyard. "
        "600 work, at most 60/year; 120 I / 60 B / 120 G. Eight delivered xenocrystals are consumed once, on the first paid work. "
        "Pause or queue cancellation keeps paid work; dismantle gives no refund.";
    if (own) {
        details += QString("\n%1 — %2/600 work; crystals paid %3/8; minerals paid %4 I / %5 B / %6 G. %7")
            .arg(QString::fromStdString(p->name)).arg(p->resonanceLab.work).arg(p->resonanceLab.crystalsPaid)
            .arg(p->resonanceLab.invested.ironium).arg(p->resonanceLab.invested.boranium).arg(p->resonanceLab.invested.germanium)
            .arg(QString::fromStdString(resonance_block(known, *p)));
        if (resonance_scheduled(*p)) {
            const auto forecast = forecast_production_queue(known, *p, p->productionQueue, 128);
            for (std::size_t i = 0; i < p->productionQueue.size(); ++i) if (p->productionQueue[i].kind == ProductionKind::ResonanceLaboratory)
                details += forecast[i].completionTurn ? QString(" Forecast completion: year %1.").arg(qulonglong(*forecast[i].completionTurn)) : " Completion not forecast with received supplies.";
        }
    }
    findChild<QLabel*>("exoticProjectDetails")->setText(details);
}
} // namespace suns
