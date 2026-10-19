#include "main_window.hpp"
#include "suns/campaign.hpp"
#include "suns/intergalactic_visitors.hpp"
#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPolygonItem>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>

namespace suns {
void MainWindow::startVisitorDemo()
{
    state_=make_visitor_demo(); sessionMode_=SessionMode::Solo; pendingOrders_={1,{}};
    pendingDescriptions_.clear(); inbox_.clear(); campaignMessages_.clear(); currentSavePath_.clear();
    selection_={}; logisticsControlFleetId_.reset(); resetTurnExchangeIdentity();
    galaxyConfig_={0,state_.stars.size(),400,400,15}; scene_->setSceneRect(-255,-255,510,510);
    refreshShipDesignChoices(); rebuildScene(); updateSaveWindowTitle();
    if(auto* dock=findChild<QDockWidget*>("visitorsDock")) dock->show();
    fitGalaxyView();
    QTimer::singleShot(0,this,[this]{ if(!shuttingDown_) fitGalaxyView(); });
    statusBar()->showMessage("Visitor encounter demo — year 60. Choose Intercept and exchange, then End Turn.",8000);
}
void MainWindow::installVisitors()
{
    if(findChild<QDockWidget*>("visitorsDock")) return;
    auto* dock=new QDockWidget("Intergalactic visitors",this); dock->setObjectName("visitorsDock");
    auto* body=new QWidget(dock); auto* layout=new QVBoxLayout(body);
    auto* hint=new QLabel("Rare neutral visitors cross the galaxy and leave. Detect their broadcasts with ordinary active scanners/radars. "
        "Bring 50 kt Germanium in a fleet's hold: the first physical rendezvous receives the Multi Cargo Pod blueprint. "
        "The blueprint arrives at HQ through ordinary communications; building the equipment still costs production/minerals and requires its technology. "
        "Intercept / cancel commands also travel with delay. Forecast coordinates extrapolate a received track, and do not guarantee contact or fuel.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* tree=new QTreeWidget(body); tree->setObjectName("visitorContacts");
    tree->setHeaderLabels({"Visitor","Observed / received (age)","Measured position","Forecast position now","Announced departure","Last known offer"});
    tree->setRootIsDecorated(false); tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree->setStyleSheet("QTreeWidget { background: #0b121b; color: #e6eef7; border: 1px solid #2a4056; } "
        "QHeaderView::section { background: #192838; color: #dbe8f4; padding: 4px; border: 1px solid #304a63; } "
        "QTreeWidget::item:selected { background: #315f88; color: #ffffff; }"); layout->addWidget(tree);
    auto* row=new QHBoxLayout; auto* fleet=new QComboBox(body); fleet->setObjectName("visitorFleet"); row->addWidget(fleet);
    auto* warp=new QSpinBox(body); warp->setObjectName("visitorWarp"); warp->setRange(1,kMaxWarp); warp->setValue(8); row->addWidget(new QLabel("Warp",body)); row->addWidget(warp);
    auto* intercept=new QPushButton("Intercept and exchange",body); intercept->setObjectName("interceptVisitor"); row->addWidget(intercept);
    auto* cancel=new QPushButton("Send cancel / decline",body); cancel->setObjectName("cancelVisitorEncounter"); row->addWidget(cancel);
    auto* locate=new QPushButton("Show forecast area",body); locate->setObjectName("locateVisitor"); row->addWidget(locate); layout->addLayout(row);
    auto* status=new QLabel(body); status->setObjectName("visitorStatus"); status->setWordWrap(true); layout->addWidget(status);
    dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea,dock); dock->hide(); menuBar()->addAction(dock->toggleViewAction());
    auto* demo=new QAction("Start visitor encounter demo…",this); demo->setObjectName("visitorDemoAction"); menuBar()->addAction(demo);
    connect(demo,&QAction::triggered,this,[this]{
        if(QMessageBox::question(this,"Visitor encounter demo","Replace the current game with a prepared encounter? Save your current game first.") == QMessageBox::Yes) startVisitorDemo();
    });
    connect(intercept,&QPushButton::clicked,this,[this,tree,fleet,warp]{
        if(!tree->currentItem() || !fleet->currentData().toUInt()) return;
        appendPendingOrder(EncounterVisitorOrder{fleet->currentData().toUInt(),tree->currentItem()->data(0,Qt::UserRole).toUInt(),std::uint8_t(warp->value())},"Intercept visitor and exchange 50 kt Germanium at physical contact (delayed command)");
        rebuildScene();
    });
    connect(cancel,&QPushButton::clicked,this,[this,fleet,warp]{
        if(!fleet->currentData().toUInt()) return;
        appendPendingOrder(EncounterVisitorOrder{fleet->currentData().toUInt(),0,std::uint8_t(warp->value())},"Cancel visitor encounter on command receipt"); rebuildScene();
    });
    connect(locate,&QPushButton::clicked,this,[this,tree]{
        if(!tree->currentItem()) return;
        if(const auto* x=known_visitor(state_,pendingOrders_.player,tree->currentItem()->data(0,Qt::UserRole).toUInt())) {
            const auto p=visitor_prediction(*x,double(std::min(state_.turn,x->departureTurn))); view_->centerOn(p.x,p.y);
        }
    });
    connect(warp,&QSpinBox::valueChanged,this,[this](int){refreshVisitors();});
    connect(tree,&QTreeWidget::itemSelectionChanged,this,[this]{refreshVisitors();});
    connect(fleet,&QComboBox::currentIndexChanged,this,[this](int){refreshVisitors();});
    connect(this,&MainWindow::routeProgramContextChanged,this,[this]{refreshVisitors();}); refreshVisitors();
}
void MainWindow::refreshVisitors()
{
    auto* tree=findChild<QTreeWidget*>("visitorContacts"); if(!tree || !find_player(state_,pendingOrders_.player)) return;
    const auto view=make_player_view(state_,pendingOrders_.player).state;
    const auto selected=tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toUInt():0;
    const QSignalBlocker guard(tree); tree->clear();
    for(const auto& x:view.players[0].visitorKnowledge) {
        auto* row=new QTreeWidgetItem(tree); row->setData(0,Qt::UserRole,x.id); row->setText(0,QString("Visitor V%1").arg(x.id));
        row->setText(1,QString("%1 / %2 (age %3)").arg(qulonglong(x.observedTurn)).arg(qulonglong(x.receivedTurn)).arg(qulonglong(view.turn-x.observedTurn)));
        row->setText(2,QString("%1, %2").arg(x.position.x,0,'f',1).arg(x.position.y,0,'f',1));
        const auto prediction=visitor_prediction(x,double(std::min(view.turn,x.departureTurn)));
        row->setText(3,view.turn>=x.departureTurn?"Predicted to have left":QString("%1, %2 (forecast)").arg(prediction.x,0,'f',1).arg(prediction.y,0,'f',1));
        row->setText(4,QString::number(qulonglong(x.departureTurn)));
        row->setText(5,x.departed?"Departure observed":x.available?"50 kt Germanium → Multi Cargo Pod blueprint":"Offer withdrawn at observation");
        if(x.id==selected) tree->setCurrentItem(row);
    }
    if(!tree->currentItem() && tree->topLevelItemCount()) tree->setCurrentItem(tree->topLevelItem(0));
    auto* fleet=findChild<QComboBox*>("visitorFleet"); const auto old=fleet->currentData(); const QSignalBlocker fleetGuard(fleet); fleet->clear();
    for(const auto& f:view.fleets) if(f.outpost==OutpostKind::None) fleet->addItem(QString::fromStdString(f.name),f.id);
    if(fleet->findData(old)>=0) fleet->setCurrentIndex(fleet->findData(old));
    const auto f=std::find_if(view.fleets.begin(),view.fleets.end(),[&](const auto& f){return f.id==fleet->currentData().toUInt();});
    const auto* x=tree->currentItem()?known_visitor(view,pendingOrders_.player,tree->currentItem()->data(0,Qt::UserRole).toUInt()):nullptr;
    findChild<QPushButton*>("interceptVisitor")->setEnabled(f!=view.fleets.end() && x && x->available && !x->departed && view.turn<x->departureTurn
        && fleet_warp_valid(view,*f,std::uint8_t(findChild<QSpinBox*>("visitorWarp")->value())));
    findChild<QPushButton*>("cancelVisitorEncounter")->setEnabled(f!=view.fleets.end());
    findChild<QPushButton*>("locateVisitor")->setEnabled(x);
    QString text;
    if(f!=view.fleets.end()) text=QString("Fleet telemetry: year %1, age %2. Received Germanium aboard: %3 kt. Last reported visitor mission: V%4. ")
        .arg(qulonglong(f->telemetry.observedTurn)).arg(qulonglong(view.turn-f->telemetry.observedTurn)).arg(f->minerals.germanium).arg(f->visitor.track.id);
    text+=std::find(view.players[0].acquiredEquipment.begin(),view.players[0].acquiredEquipment.end(),kVisitorGift)!=view.players[0].acquiredEquipment.end()
        ?"Multi Cargo Pod blueprint received: available in Designer subject to normal technology/costs."
        :"No visitor blueprint received yet. Load Germanium through the ordinary Cargo controls before departure.";
    findChild<QLabel*>("visitorStatus")->setText(text);
}
void MainWindow::renderKnownVisitors()
{
    const auto* p=find_player(state_,pendingOrders_.player); if(!p) return;
    for(const auto& x:p->visitorKnowledge) {
        const auto end=visitor_prediction(x,double(x.departureTurn)); const QColor color("#ebc07a");
        auto* line=scene_->addLine(x.position.x,x.position.y,end.x,end.y,QPen(color,0,Qt::DashLine)); line->setAcceptedMouseButtons(Qt::NoButton);
        line->setToolTip("Forecast from a dated visitor broadcast; hidden state is not tracked."); line->setZValue(3);
        if(x.departed || state_.turn>=x.departureTurn) continue;
        const auto forecast=visitor_prediction(x,double(state_.turn));
        auto* marker=scene_->addPolygon(QPolygonF{{0,-7},{8,5},{0,2},{-8,5}},QPen(color),QBrush(QColor("#563b26")));
        marker->setPos(forecast.x,forecast.y); marker->setFlag(QGraphicsItem::ItemIgnoresTransformations); marker->setZValue(9);
        marker->setAcceptedMouseButtons(Qt::NoButton); marker->setData(1,10);
        marker->setToolTip(QString("Intergalactic visitor V%1 — forecast from observation year %2. Announced departure %3.\n50 kt Germanium for a Multi Cargo Pod blueprint; physical timed rendezvous required.")
            .arg(x.id).arg(qulonglong(x.observedTurn)).arg(qulonglong(x.departureTurn)));
    }
}
} // namespace suns
