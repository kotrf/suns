#include "main_window.hpp"
#include "suns/campaign.hpp"
#include "suns/science_samples.hpp"
#include "suns/unknown_objects.hpp"
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
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace suns {
namespace {
qulonglong endpoint_key(CargoTransferEndpoint e) { return (qulonglong(e.planet) << 32) | e.fleet; }
CargoTransferEndpoint endpoint(qulonglong key) { return {PlanetId(key >> 32), FleetId(key)}; }
GameState sample_plan(const GameState& state, const PlayerOrders& orders)
{
    return projected_cargo_order_state(make_player_view(state, orders.player).state, orders);
}
}
void MainWindow::installScienceSamples()
{
    if (findChild<QDockWidget*>("scienceSamplesDock")) return;
    auto* dock = new QDockWidget("Science samples / artifacts", this); dock->setObjectName("scienceSamplesDock");
    auto* body = new QWidget(dock); auto* layout = new QVBoxLayout(body);
    auto* hint = new QLabel("Study a landmark and receive its scientific report, then use Fly and recover specimen in Unknown objects. "
        "There is one physical specimen per landmark for the whole galaxy. Each occupies 5 kt in the shared hold and adds to flight mass. "
        "Return to a friendly colony with On arrival → Unload all cargo, or transfer it locally here. Remote manifests are dated telemetry.", body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* tree = new QTreeWidget(body); tree->setObjectName("scienceSampleInventory");
    tree->setHeaderLabels({"Specimen", "Received location", "Mass", "Use"}); tree->setRootIsDecorated(false);
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents); layout->addWidget(tree);
    auto* destination = new QComboBox(body); destination->setObjectName("scienceSampleDestination"); layout->addWidget(destination);
    auto* row = new QHBoxLayout;
    auto* transfer = new QPushButton("Transfer specimen", body); transfer->setObjectName("transferScienceSample"); row->addWidget(transfer);
    auto* retain = new QPushButton("Retain / activate artifact", body); retain->setObjectName("retainScienceSample"); row->addWidget(retain);
    auto* analyze = new QPushButton("Analyze (consumes specimen)", body); analyze->setObjectName("analyzeScienceSample"); row->addWidget(analyze);
    layout->addLayout(row);
    auto* details = new QLabel(body); details->setObjectName("scienceSampleDetails"); details->setWordWrap(true); layout->addWidget(details);
    dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide();
    if (auto* menu = menuBar()->findChild<QMenu*>("sunsViewMenu")) menu->addAction(dock->toggleViewAction());
    else menuBar()->addAction(dock->toggleViewAction());
    const auto update = [=, this] {
        transfer->setEnabled(false); retain->setEnabled(false); analyze->setEnabled(false); details->clear();
        const auto* selected = tree->currentItem();
        if (!selected) return;
        auto known = sample_plan(state_, pendingOrders_);
        const auto source = endpoint(selected->data(1, Qt::UserRole).toULongLong());
        const auto id = selected->data(0, Qt::UserRole).toUInt();
        transfer->setEnabled(transfer_science_sample(known, pendingOrders_.player,
            {source, endpoint(destination->currentData().toULongLong()), id}));
        const auto kind = UnknownObjectKind(selected->data(2, Qt::UserRole).toUInt());
        if (source.planet) {
            auto preview = sample_plan(state_, pendingOrders_);
            analyze->setEnabled(process_science_sample(preview, pendingOrders_.player, {source.planet, id, false}));
            preview = sample_plan(state_, pendingOrders_);
            retain->setEnabled(process_science_sample(preview, pendingOrders_.player, {source.planet, id, true}));
        }
        details->setText(QString("Analyze: consume this unique specimen for %1 %2 research points, usable after the normal report arrives.\n"
            "Retain: +4 research points/year at a populated colony while %2 is the active focus with a nonzero research allocation. "
            "Activation and operation require the delivered completed study of object X%3. A captured artifact needs the new owner's observation.\n"
            "Transfers require current local contact, friendly ownership, co-location and 5 kt of free hold. Pending orders are previewed here.")
            .arg(science_sample_research_points(kind)).arg(QString::fromStdString(research_field_name(unknown_object_research_field(kind)))).arg(id));
    };
    connect(tree, &QTreeWidget::itemSelectionChanged, this, [=, this] {
        const QSignalBlocker block(destination); destination->clear();
        if (const auto* selected = tree->currentItem()) {
            const auto source = selected->data(1, Qt::UserRole).toULongLong();
            const auto known = sample_plan(state_, pendingOrders_);
            for (const auto& p : known.planets) if (p.owner == pendingOrders_.player && p.population && source != endpoint_key({p.id, 0}))
                destination->addItem(QString::fromStdString(p.name) + " — colony", QVariant::fromValue(endpoint_key({p.id, 0})));
            for (const auto& f : known.fleets) if (f.owner == pendingOrders_.player && source != endpoint_key({0, f.id}))
                destination->addItem(QString::fromStdString(f.name) + QString(" — fleet #%1").arg(f.id), QVariant::fromValue(endpoint_key({0, f.id})));
        }
        update();
    });
    connect(destination, &QComboBox::currentIndexChanged, this, [update](int) { update(); });
    connect(transfer, &QPushButton::clicked, this, [=, this] {
        if (const auto* selected = tree->currentItem()) {
            appendPendingOrder(TransferScienceSampleOrder{endpoint(selected->data(1, Qt::UserRole).toULongLong()),
                endpoint(destination->currentData().toULongLong()), selected->data(0, Qt::UserRole).toUInt()}, "Transfer scientific specimen");
            rebuildScene();
        }
    });
    const auto process = [=, this](bool keep) {
        const auto* selected = tree->currentItem(); if (!selected) return;
        const auto source = endpoint(selected->data(1, Qt::UserRole).toULongLong()); if (!source.planet) return;
        const auto id = selected->data(0, Qt::UserRole).toUInt();
        if (!keep && QMessageBox::question(this, "Analyze specimen", "Consume this unique specimen for research? Its working artifact will be lost permanently.") != QMessageBox::Yes) return;
        appendPendingOrder(ProcessScienceSampleOrder{source.planet, id, keep}, keep ? "Retain scientific artifact" : "Analyze and consume scientific specimen");
        rebuildScene();
    };
    connect(retain, &QPushButton::clicked, this, [process] { process(true); });
    connect(analyze, &QPushButton::clicked, this, [process] { process(false); });
    connect(this, &MainWindow::routeProgramContextChanged, this, [this, update] { refreshScienceSamples(); update(); });
    refreshScienceSamples(); update();
}
void MainWindow::refreshScienceSamples()
{
    auto* tree = findChild<QTreeWidget*>("scienceSampleInventory"); if (!tree || !find_player(state_, pendingOrders_.player)) return;
    const auto oldId = tree->currentItem() ? tree->currentItem()->data(0, Qt::UserRole).toUInt() : 0;
    const auto oldPlace = tree->currentItem() ? tree->currentItem()->data(1, Qt::UserRole).toULongLong() : 0;
    tree->clear();
    const auto known = sample_plan(state_, pendingOrders_);
    const auto add = [&](const auto& samples, CargoTransferEndpoint place, const QString& name) {
        for (const auto& s : samples) {
            auto* row = new QTreeWidgetItem(tree);
            row->setData(0, Qt::UserRole, s.source); row->setData(1, Qt::UserRole, QVariant::fromValue(endpoint_key(place)));
            row->setData(2, Qt::UserRole, unsigned(s.kind));
            row->setText(0, QString("X%1 — %2").arg(s.source).arg(science_sample_name(s.kind)));
            row->setText(1, name); row->setText(2, "5 kt"); row->setText(3, s.retained ? "Retained artifact" : "Stored specimen");
            if (oldId == s.source && oldPlace == endpoint_key(place)) tree->setCurrentItem(row);
        }
    };
    for (const auto& p : known.planets) if (p.owner == pendingOrders_.player) add(p.scienceSamples, {p.id, 0}, QString::fromStdString(p.name));
    for (const auto& f : known.fleets) if (f.owner == pendingOrders_.player) add(f.scienceSamples, {0, f.id},
        QString("%1 — observed year %2").arg(QString::fromStdString(f.name)).arg(qulonglong(f.telemetry.observedTurn)));
}
bool MainWindow::queueUnknownObjectRecovery(UnknownObjectId id)
{
    const auto* selected = selectedFleet(); if (!selected) return false;
    const auto known = make_player_view(state_, pendingOrders_.player).state;
    const auto* x = known_unknown_object(known, pendingOrders_.player, id);
    const auto f = std::find_if(known.fleets.begin(), known.fleets.end(), [&](const auto& f) { return f.id == selected->id; });
    if (!x || x->level != ObjectKnowledgeLevel::Studied || f == known.fleets.end()
        || !fleet_has_scanner(known, *f) || fleet_cargo_capacity(known, *f) < kScienceSampleMass) return false;
    replacePendingFleetMove(f->id, x->position, selectedFleetSuggestedWarpForRouteProgram(),
        FleetArrivalAction{FleetArrivalActionKind::RecoverSample}, QString("Recover specimen from studied object X%1").arg(id));
    rebuildScene(); return true;
}
} // namespace suns
