#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include "visitor_offer_text.hpp"
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
#include <QMenu>
#include <QPainter>
#include <QPixmap>
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
namespace {
QPixmap visitorPortrait(VisitorProfile profile)
{
    QPixmap image(76,76); image.fill(QColor("#0b121b"));
    QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = profile==VisitorProfile::Freight ? QColor("#ebc07a")
        : profile==VisitorProfile::Prospector ? QColor("#78cdb4") : QColor("#97baf0");
    painter.setPen(QPen(color,1.5)); painter.drawEllipse(QRectF(7,7,62,62));
    painter.setBrush(color.darker(240));
    if(profile==VisitorProfile::Freight)
        painter.drawPolygon(QPolygonF{{38,12},{60,55},{38,46},{16,55}});
    else if(profile==VisitorProfile::Prospector)
        painter.drawPolygon(QPolygonF{{38,13},{60,38},{50,61},{38,48},{26,61},{16,38}});
    else painter.drawPolygon(QPolygonF{{38,13},{61,28},{55,51},{38,63},{21,51},{15,28}});
    painter.setBrush(color); painter.drawEllipse(QRectF(33,32,10,10));
    return image;
}
}
void MainWindow::startVisitorDemo(VisitorProfile profile)
{
    auto documentUpdate = session_->beginDocumentUpdate(this);
    cancelRouteProgramMapTargetPick();
    if (shipDesigner_) shipDesigner_->close();
    state_=make_visitor_demo(profile); sessionMode_=SessionMode::Solo; pendingOrders_={1,{}};
    empireSetups_.clear(); resetTurnMessages(); historyRangeInitialized_ = false;
    warpControlFleetId_.reset();
    currentDistanceSelectionKind_ = previousDistanceSelectionKind_ = 0;
    currentDistanceSelectionId_ = previousDistanceSelectionId_ = 0;
    pendingDescriptions_.clear(); inbox_.clear(); campaignMessages_.clear(); currentSavePath_.clear();
    selection_={}; logisticsControlFleetId_.reset(); resetTurnExchangeIdentity();
    galaxyConfig_={0,state_.stars.size(),400,400,15}; scene_->setSceneRect(-255,-255,510,510);
    documentUpdate.commit();
    emit routeProgramContextChanged(true);
    refreshShipDesignChoices(); refreshEmpireHistory(); rebuildScene(false); updateSaveWindowTitle();
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
        "Bring the advertised minerals in a fleet's hold: the first physical rendezvous receives the offered blueprint. "
        "The blueprint arrives at HQ through ordinary communications; building the equipment still costs production/minerals and requires its technology. "
        "Intercept / cancel commands also travel with delay. Forecast coordinates extrapolate a received track, and do not guarantee contact or fuel.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* offerRow=new QHBoxLayout;
    auto* portrait=new QLabel(body); portrait->setObjectName("visitorPortrait"); portrait->setFixedSize(76,76); offerRow->addWidget(portrait);
    auto* offerText=new QLabel(body); offerText->setObjectName("visitorOffer"); offerText->setTextFormat(Qt::PlainText);
    offerText->setWordWrap(true); offerRow->addWidget(offerText,1); layout->addLayout(offerRow);
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
    auto* demos=menuBar()->addMenu("Visitor encounter demos");
    for(const auto& offer:visitor_offers()) {
        auto* demo=demos->addAction(QString("%1…").arg(offer.name));
        demo->setObjectName(offer.profile==VisitorProfile::Freight?"visitorDemoAction":offer.profile==VisitorProfile::Prospector?"visitorProspectorDemoAction":"visitorShieldwrightDemoAction");
        connect(demo,&QAction::triggered,this,[this,profile=offer.profile]{
            if(QMessageBox::question(this,"Visitor encounter demo","Replace the current game with a prepared encounter? Save your current game first.") == QMessageBox::Yes) startVisitorDemo(profile);
        });
    }
    connect(intercept,&QPushButton::clicked,this,[this,tree,fleet,warp]{
        if(!tree->currentItem() || !fleet->currentData().toUInt()) return;
        const auto id=tree->currentItem()->data(0,Qt::UserRole).toUInt();
        const auto* known=known_visitor(state_,pendingOrders_.player,id); if(!known) return;
        appendPendingOrder(EncounterVisitorOrder{fleet->currentData().toUInt(),id,std::uint8_t(warp->value())},
            QString("Intercept visitor V%1: %2 at physical contact (delayed command)").arg(id).arg(visitor_offer_text(known->profile)));
    });
    connect(cancel,&QPushButton::clicked,this,[this,fleet,warp]{
        if(!fleet->currentData().toUInt()) return;
        appendPendingOrder(EncounterVisitorOrder{fleet->currentData().toUInt(),0,std::uint8_t(warp->value())},"Cancel visitor encounter on command receipt");
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
        auto* row=new QTreeWidgetItem(tree); row->setData(0,Qt::UserRole,x.id); row->setText(0,QString("V%1 — %2").arg(x.id).arg(visitor_offer(x.profile).name));
        row->setText(1,QString("%1 / %2 (age %3)").arg(qulonglong(x.observedTurn)).arg(qulonglong(x.receivedTurn)).arg(qulonglong(view.turn-x.observedTurn)));
        row->setText(2,QString("%1, %2").arg(x.position.x,0,'f',1).arg(x.position.y,0,'f',1));
        const auto prediction=visitor_prediction(x,double(std::min(view.turn,x.departureTurn)));
        row->setText(3,view.turn>=x.departureTurn?"Predicted to have left":QString("%1, %2 (forecast)").arg(prediction.x,0,'f',1).arg(prediction.y,0,'f',1));
        row->setText(4,QString::number(qulonglong(x.departureTurn)));
        row->setText(5,x.departed?"Departure observed":x.available?visitor_offer_text(x.profile):"Offer withdrawn at observation");
        if(x.id==selected) tree->setCurrentItem(row);
    }
    if(!tree->currentItem() && tree->topLevelItemCount()) tree->setCurrentItem(tree->topLevelItem(0));
    auto* fleet=findChild<QComboBox*>("visitorFleet"); const auto old=fleet->currentData(); const QSignalBlocker fleetGuard(fleet); fleet->clear();
    for(const auto& f:view.fleets) if(f.outpost==OutpostKind::None) fleet->addItem(QString::fromStdString(f.name),f.id);
    if(fleet->findData(old)>=0) fleet->setCurrentIndex(fleet->findData(old));
    const auto f=std::find_if(view.fleets.begin(),view.fleets.end(),[&](const auto& f){return f.id==fleet->currentData().toUInt();});
    const auto* x=tree->currentItem()?known_visitor(view,pendingOrders_.player,tree->currentItem()->data(0,Qt::UserRole).toUInt()):nullptr;
    auto* offerText=findChild<QLabel*>("visitorOffer"); auto* portrait=findChild<QLabel*>("visitorPortrait");
    if(x) {
        const auto& offer=visitor_offer(x->profile);
        offerText->setText(QString("%1 — intergalactic visitor V%2\n“%3”\n%4\nOne physical exchange before year %5; normal research and manufacturing still apply.")
            .arg(offer.name).arg(x->id).arg(offer.greeting).arg(visitor_offer_text(x->profile)).arg(qulonglong(x->departureTurn)));
        portrait->setPixmap(visitorPortrait(x->profile)); portrait->setAccessibleName(QString("Intergalactic %1").arg(offer.name));
    } else { offerText->setText("Select a received visitor contact to read its offer."); portrait->clear(); }
    findChild<QPushButton*>("interceptVisitor")->setEnabled(f!=view.fleets.end() && x && x->available && !x->departed && view.turn<x->departureTurn
        && fleet_warp_valid(view,*f,std::uint8_t(findChild<QSpinBox*>("visitorWarp")->value())));
    findChild<QPushButton*>("cancelVisitorEncounter")->setEnabled(f!=view.fleets.end());
    findChild<QPushButton*>("locateVisitor")->setEnabled(x);
    QString text;
    if(f!=view.fleets.end()) text=QString("Fleet telemetry: year %1, age %2. Received minerals aboard: I %3 / B %4 / G %5 kt. Last reported visitor mission: V%6. ")
        .arg(qulonglong(f->telemetry.observedTurn)).arg(qulonglong(view.turn-f->telemetry.observedTurn))
        .arg(f->minerals.ironium).arg(f->minerals.boranium).arg(f->minerals.germanium).arg(f->visitor.track.id);
    QStringList gifts;
    for(const auto& offer:visitor_offers()) if(std::find(view.players[0].acquiredEquipment.begin(),view.players[0].acquiredEquipment.end(),offer.gift)!=view.players[0].acquiredEquipment.end())
        gifts << visitor_gift_text(offer.profile)+" received";
    text+=gifts.empty()?"No visitor blueprint received yet. Load the advertised minerals through ordinary Cargo controls before departure."
        :gifts.join("; ")+". Available in Designer subject to normal technology/costs.";
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
        marker->setToolTip(QString("Intergalactic visitor V%1 — %2. Forecast from observation year %3. Announced departure %4.\n%5; physical timed rendezvous required.")
            .arg(x.id).arg(visitor_offer(x.profile).name).arg(qulonglong(x.observedTurn)).arg(qulonglong(x.departureTurn)).arg(visitor_offer_text(x.profile)));
    }
}
} // namespace suns
