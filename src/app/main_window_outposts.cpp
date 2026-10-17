#include "main_window.hpp"
#include "suns/deep_space.hpp"
#include "suns/communications.hpp"
#include "suns/strategic_operations.hpp"
#include <QAction>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace suns {
void MainWindow::installOutposts()
{
    if (findChild<QDockWidget*>("outpostsDock")) return;
    auto* dock=new QDockWidget("Deep-space infrastructure",this); dock->setObjectName("outpostsDock");
    auto* body=new QWidget(dock); auto* layout=new QVBoxLayout(body);
    auto* hint=new QLabel("Build a ship with one Fuel Depot Kit (Construction 4 / Energy 3) or Deep-space Relay Kit "
        "(Construction 4 / Electronics 5) in Ship Designer, then fly it to deep space and deploy. Deployment is permanent; "
        "one ship, no colonists, 5 ly from stars and 1 ly from other outposts. Upkeep: 1 mg/year. "
        "Depots dispense finite fuel; tankers replenish both kinds. A powered relay adds 300 ly only after deployment. "
        "Stocks and condition below are received telemetry; remote outcomes can change before arrival.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* coordinates=new QFormLayout;
    const auto coordinate=[&](const char* name) {
        auto* spin=new QDoubleSpinBox(body); spin->setObjectName(name);
        spin->setRange(-1e6,1e6); spin->setDecimals(2); spin->setSuffix(" ly"); return spin;
    };
    auto* x=coordinate("outpostBuildX"); auto* y=coordinate("outpostBuildY");
    coordinates->addRow("Construction X",x); coordinates->addRow("Construction Y",y); layout->addLayout(coordinates);
    auto* buildRow=new QHBoxLayout;
    auto* deploy=new QPushButton("Deploy selected ship here",body); deploy->setObjectName("deployOutpostHere");
    auto* flyBuild=new QPushButton("Fly to coordinates and deploy",body); flyBuild->setObjectName("buildOutpostAtCoordinates");
    buildRow->addWidget(deploy); buildRow->addWidget(flyBuild); layout->addLayout(buildRow);
    auto* tree=new QTreeWidget(body); tree->setObjectName("outpostContacts"); tree->setRootIsDecorated(false);
    tree->setHeaderLabels({"Installation","Kind","Position, ly","Fuel / capacity, mg","Damage","Telemetry","Service"});
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents); layout->addWidget(tree);
    auto* reserve=new QDoubleSpinBox(body); reserve->setObjectName("outpostTankerReserve");
    reserve->setRange(0,1e12); reserve->setDecimals(2); reserve->setValue(100); reserve->setSuffix(" mg");
    auto* supplyForm=new QFormLayout; supplyForm->addRow("Keep tanker fuel",reserve); layout->addLayout(supplyForm);
    auto* buttons=new QHBoxLayout;
    auto* locate=new QPushButton("Show",body); locate->setObjectName("locateOutpost");
    auto* fly=new QPushButton("Fly to outpost",body); fly->setObjectName("approachOutpost");
    auto* refuel=new QPushButton("Fly and refuel",body); refuel->setObjectName("refuelAtOutpost");
    auto* supply=new QPushButton("Deliver fuel",body); supply->setObjectName("supplyOutpost");
    for (auto* button:{locate,fly,refuel,supply}) buttons->addWidget(button);
    layout->addLayout(buttons); dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea,dock); dock->hide();
    auto* overlay=new QAction("Deep-space infrastructure overlay",this); overlay->setObjectName("outpostsOverlay");
    overlay->setCheckable(true); overlay->setChecked(QSettings{}.value("map/outposts",true).toBool());
    if (auto* menu=menuBar()->findChild<QMenu*>("sunsViewMenu")) { menu->addAction(dock->toggleViewAction()); menu->addAction(overlay); }
    else { menuBar()->addAction(dock->toggleViewAction()); menuBar()->addAction(overlay); }
    connect(overlay,&QAction::toggled,this,[this](bool checked) { QSettings{}.setValue("map/outposts",checked); rebuildScene(); });
    const auto selected=[this,tree]() -> std::optional<Fleet> {
        if (!tree->currentItem()) return std::nullopt;
        const auto id=tree->currentItem()->data(0,Qt::UserRole).toUInt();
        const auto sites=known_outposts(state_,pendingOrders_.player);
        for (const auto& f:sites) if (f.id==id) return f;
        return std::nullopt;
    };
    const auto update=[this,selected,deploy,flyBuild,locate,fly,refuel,supply] {
        const auto site=selected(); const auto f=selectedFleetPlanningView();
        const bool mobile=f && f->outpost==OutpostKind::None && f->damagePercent<100;
        const bool builder=mobile && fleet_construction_kit(state_,*f)!=OutpostKind::None && !f->colonists;
        flyBuild->setEnabled(builder);
        deploy->setEnabled(builder && !f->destination && !f->targetFleet && f->waypointQueue.empty()
            && !f->repeatOrders && outpost_deployment_error(state_,*f,f->position).empty());
        locate->setEnabled(bool(site)); fly->setEnabled(site && mobile && site->id!=f->id);
        refuel->setEnabled(fly->isEnabled() && site->outpost==OutpostKind::FuelDepot);
        supply->setEnabled(fly->isEnabled());
    };
    connect(tree,&QTreeWidget::itemSelectionChanged,this,update);
    connect(deploy,&QPushButton::clicked,this,[this] {
        const auto f=selectedFleetPlanningView(); if (!f || f->destination || f->targetFleet || !f->waypointQueue.empty() || f->repeatOrders) return;
        const auto error=outpost_deployment_error(state_,*f,f->position);
        if (!error.empty()) { statusBar()->showMessage(QString::fromStdString(error),5000); return; }
        appendPendingOrder(SetFleetTaskOrder{f->id,FleetTask::DeployOutpost},
            QString("Permanently deploy %1 at %2, %3").arg(QString::fromStdString(f->name)).arg(f->position.x).arg(f->position.y));
        rebuildScene();
    });
    connect(flyBuild,&QPushButton::clicked,this,[this,x,y] {
        const auto f=selectedFleetPlanningView(); if (!f) return;
        const Position position{x->value(),y->value()}; const auto error=outpost_deployment_error(state_,*f,position);
        if (!error.empty()) { statusBar()->showMessage(QString::fromStdString(error),5000); return; }
        replacePendingFleetMove(f->id,position,selectedFleetSuggestedWarpForRouteProgram(),{FleetArrivalActionKind::DeployOutpost},
            QString("Fly to %1, %2 and permanently deploy outpost").arg(position.x).arg(position.y));
        rebuildScene();
    });
    const auto approach=[this,selected,reserve](FleetArrivalActionKind kind) {
        const auto f=selectedFleetPlanningView(); const auto site=selected();
        if (!f || !site || f->outpost!=OutpostKind::None || f->id==site->id) return;
        FleetArrivalAction action{kind}; if (kind==FleetArrivalActionKind::SupplyOutpost) action.reserveFuel=reserve->value();
        replacePendingFleetMove(f->id,site->position,selectedFleetSuggestedWarpForRouteProgram(),action,
            QString("%1 at received outpost #%2").arg(kind==FleetArrivalActionKind::SupplyOutpost?"Deliver fuel":kind==FleetArrivalActionKind::Refuel?"Refuel":"Arrive").arg(site->id));
        rebuildScene();
    };
    connect(fly,&QPushButton::clicked,this,[approach] { approach(FleetArrivalActionKind::None); });
    connect(refuel,&QPushButton::clicked,this,[approach] { approach(FleetArrivalActionKind::Refuel); });
    connect(supply,&QPushButton::clicked,this,[approach] { approach(FleetArrivalActionKind::SupplyOutpost); });
    connect(locate,&QPushButton::clicked,this,[this,selected] { if (const auto site=selected()) view_->centerOn(site->position.x,site->position.y); });
    connect(this,&MainWindow::routeProgramContextChanged,this,[this,update] { refreshOutposts(); update(); });
    refreshOutposts(); update();
}
void MainWindow::refreshOutposts()
{
    auto* tree=findChild<QTreeWidget*>("outpostContacts"); if (!tree) return;
    const auto selected=tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toUInt():0;
    const QSignalBlocker block(tree); tree->clear();
    for (const auto& f:known_outposts(state_,pendingOrders_.player)) {
        auto* row=new QTreeWidgetItem(tree); row->setData(0,Qt::UserRole,f.id);
        row->setText(0,QString("%1 (#%2)").arg(QString::fromStdString(f.name)).arg(f.id));
        row->setText(1,QString::fromStdString(outpost_kind_name(f.outpost)));
        row->setText(2,QString("%1, %2").arg(f.position.x,0,'f',1).arg(f.position.y,0,'f',1));
        row->setText(3,QString("%1 / %2").arg(f.fuel,0,'f',1).arg(fleet_fuel_capacity(state_,f),0,'f',1));
        row->setText(4,QString("%1%").arg(f.damagePercent,0,'f',0));
        row->setText(5,QString("Year %1; age %2").arg(qulonglong(f.telemetry.observedTurn)).arg(qulonglong(fleet_telemetry_age(state_,f))));
        row->setText(6,!outpost_online(f)?"Offline at observation":f.outpost==OutpostKind::CommunicationsRelay
            && !fleet_transmits(f,f.telemetry.observedTurn)?"Powered; radio silent at observation":"Powered at observation");
        for (int c=0;c<7;++c) row->setToolTip(c,"Last received snapshot. Fuel changes, destruction and network interruptions require a delivered report. Powered sites consume 1 mg per year; tanks can be replenished by a Supply outpost arrival order.");
        if (f.id==selected) tree->setCurrentItem(row);
    }
}
} // namespace suns
