#include "main_window.hpp"
#include "suns/unknown_objects.hpp"
#include "suns/science_samples.hpp"
#include "suns/communications.hpp"
#include "suns/strategic_operations.hpp"
#include <QAction>
#include <QDockWidget>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace suns {
namespace {
QString description(const ObjectObservation& x,std::uint64_t turn)
{
    QString text=QString("Object X%1 — %2\n%3\nPosition observed year %4; received %5; age %6 years.\nLast reported position %7, %8 ±%9 ly.")
        .arg(x.id).arg(x.kind ? unknown_object_name(*x.kind) : "Unclassified physical contact")
        .arg(object_knowledge_name(x.level)).arg(qulonglong(x.observedTurn)).arg(qulonglong(x.receivedTurn))
        .arg(qulonglong(turn>x.observedTurn?turn-x.observedTurn:0))
        .arg(x.position.x,0,'f',1).arg(x.position.y,0,'f',1).arg(x.uncertainty);
    if(x.kind) text+=QString("\nStudy objective: %1 research points in %2, once per empire, after data delivery.")
        .arg(unknown_object_research_points(*x.kind)).arg(QString::fromStdString(research_field_name(unknown_object_research_field(*x.kind))));
    else text+="\nApproach with active scanners to classify; no contents or scientific value identified yet.";
    if(x.kind==UnknownObjectKind::DriftingDerelict) {
        text+=x.velocity ? QString("\nReported course: %1, %2 ly/year. Routes use an approximate intercept from this dated course.")
            .arg(x.velocity->x,0,'f',2).arg(x.velocity->y,0,'f',2)
            : "\nLatest fix is coarse; approach to obtain a fresh course.";
        if(x.level==ObjectKnowledgeLevel::Studied)
            text+="\nOne physical 5 kt architectural core exists galaxy-wide. Recover with active scanners and free hold; a ship can close the final locally detected distance using normal movement and fuel. Deliver to a populated colony: analyze for 80 Construction points or retain for +4 Construction research/year. Availability is confirmed at the site.";
    }
    return text;
}
}
void MainWindow::installUnknownObjects()
{
    if(findChild<QDockWidget*>("unknownObjectsDock")) return;
    auto* dock=new QDockWidget("Unknown objects / expeditions",this); dock->setObjectName("unknownObjectsDock");
    auto* body=new QWidget(dock); auto* layout=new QVBoxLayout(body);
    auto* hint=new QLabel("Received physical contacts only. Select a fleet and a row. Fly to the reported area, or arrive with Field research. Active scanners classify nearby objects; study within 20 ly (limited by sensor range) yields finite data after delivery. Moving contacts retain dated positions and courses; an estimated intercept can miss after delays. After a study report arrives, a scanner fleet with 5 kt of free hold can recover the site's one physical specimen and bring it home. Recovery can follow a locally detected moving target using normal movement and fuel. A delivered Compact remnant study also unlocks Adaptive magnetosphere at Energy 18 / Construction 18 without consuming a specimen.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* tree=new QTreeWidget(body); tree->setObjectName("unknownObjectContacts");
    tree->setHeaderLabels({"Object","Knowledge","Position observed / received","Last reported position ± ly","Science objective"});
    tree->setRootIsDecorated(false); tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents); layout->addWidget(tree);
    auto* buttons=new QHBoxLayout; auto* locate=new QPushButton("Show area",body);
    locate->setObjectName("locateUnknownObject");
    auto* fly=new QPushButton("Fly to object",body); fly->setObjectName("approachUnknownObject");
    auto* study=new QPushButton("Fly and study",body); study->setObjectName("studyUnknownObject");
    auto* recover = new QPushButton("Fly and recover specimen", body); recover->setObjectName("recoverScienceSample");
    buttons->addWidget(locate); buttons->addWidget(fly); buttons->addWidget(study); buttons->addWidget(recover); layout->addLayout(buttons);
    dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea,dock); dock->hide();
    if(auto* menu=menuBar()->findChild<QMenu*>("sunsViewMenu")) menu->addAction(dock->toggleViewAction());
    else menuBar()->addAction(dock->toggleViewAction());
    const auto update=[this,tree,fly,study,locate,recover] {
        const auto* row=tree->currentItem(); const auto* x=row?known_unknown_object(state_,pendingOrders_.player,row->data(0,Qt::UserRole).toUInt()):nullptr;
        const auto f=selectedFleetPlanningView();const bool mobile=f && f->outpost==OutpostKind::None;
        locate->setEnabled(x); fly->setEnabled(x && mobile);
        fly->setText(x && x->velocity ? "Fly to estimated intercept" : "Fly to reported area");
        recover->setEnabled(x && mobile && x->level==ObjectKnowledgeLevel::Studied && fleet_has_scanner(state_,*f)
            && fleet_cargo_used(state_,*f)+kScienceSampleMass<=fleet_cargo_capacity(state_,*f)+1e-6);
        recover->setToolTip("Requires a delivered study, active scanners and 5 kt of free hold. A moving target can be acquired locally within 20 ly, limited by actual scanner conditions; normal movement and fuel apply. Availability is unknown until arrival.");
        study->setEnabled(x && mobile && x->level!=ObjectKnowledgeLevel::Studied && fleet_has_scanner(state_,*f)
            && x->uncertainty<=object_study_radius(state_,*f));
        study->setToolTip("Needs active scanners when work runs. The fitted study radius must cover the reported uncertainty; classify the area first or use a stronger scanner. Maximum study radius: 20 ly.");
    };
    connect(tree,&QTreeWidget::itemSelectionChanged,this,update);
    connect(fly,&QPushButton::clicked,this,[this,tree] { if(auto* row=tree->currentItem()) queueUnknownObjectApproach(row->data(0,Qt::UserRole).toUInt(),false); });
    connect(study,&QPushButton::clicked,this,[this,tree] { if(auto* row=tree->currentItem()) queueUnknownObjectApproach(row->data(0,Qt::UserRole).toUInt(),true); });
    connect(recover,&QPushButton::clicked,this,[this,tree] { if(auto* row=tree->currentItem()) queueUnknownObjectRecovery(row->data(0,Qt::UserRole).toUInt()); });
    connect(locate,&QPushButton::clicked,this,[this,tree] {
        if(auto* row=tree->currentItem()) if(const auto* x=known_unknown_object(state_,pendingOrders_.player,row->data(0,Qt::UserRole).toUInt()))
            view_->centerOn(x->position.x,x->position.y);
    });
    connect(this,&MainWindow::routeProgramContextChanged,this,[this,update] { refreshUnknownObjects(); update(); });
    refreshUnknownObjects(); update();
}
void MainWindow::refreshUnknownObjects()
{
    auto* tree=findChild<QTreeWidget*>("unknownObjectContacts"); if(!tree) return;
    const auto selected=tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toUInt():0;
    const QSignalBlocker block(tree); tree->clear();
    if(const auto* player=find_player(state_,pendingOrders_.player)) for(const auto& x:player->objectKnowledge) {
        auto* row=new QTreeWidgetItem(tree); row->setData(0,Qt::UserRole,x.id);
        row->setText(0,QString("X%1 — %2").arg(x.id).arg(x.kind?unknown_object_name(*x.kind):"Unknown"));
        row->setText(1,object_knowledge_name(x.level));
        row->setText(2,QString("%1 / %2 (age %3)").arg(qulonglong(x.observedTurn)).arg(qulonglong(x.receivedTurn))
            .arg(qulonglong(state_.turn>x.observedTurn?state_.turn-x.observedTurn:0)));
        row->setText(3,QString("%1, %2 ±%3").arg(x.position.x,0,'f',1).arg(x.position.y,0,'f',1).arg(x.uncertainty));
        row->setText(4,x.level==ObjectKnowledgeLevel::Studied ? "Data delivered; objective complete" : x.kind
            ? QString("%1 %2 points").arg(unknown_object_research_points(*x.kind))
                .arg(QString::fromStdString(research_field_name(unknown_object_research_field(*x.kind)))) : "Unidentified — approach to classify");
        for(int c=0;c<5;++c) row->setToolTip(c,description(x,state_.turn));
        if(x.id==selected) tree->setCurrentItem(row);
    }
}
bool MainWindow::queueUnknownObjectApproach(UnknownObjectId id,bool study)
{
    const auto fleet=selectedFleetPlanningView(); const auto* x=known_unknown_object(state_,pendingOrders_.player,id);
    if(!fleet || fleet->outpost!=OutpostKind::None || !x || (study && (x->level==ObjectKnowledgeLevel::Studied || !fleet_has_scanner(state_,*fleet)
        || x->uncertainty>object_study_radius(state_,*fleet)))) return false;
    FleetArrivalAction action; if(study) action.kind=FleetArrivalActionKind::FieldResearch;
    const auto warp=selectedFleetSuggestedWarpForRouteProgram();
    const auto point=object_approach_position(*x,state_.turn,fleet->position,warp);
    replacePendingFleetMove(fleet->id,point,warp,action,
        QString("%1 object X%2 at %3").arg(study?"Study":"Approach").arg(id).arg(x->velocity?"estimated intercept":"reported coordinates"));
    rebuildScene(); return true;
}
void MainWindow::renderKnownUnknownObjects()
{
    const auto* player=find_player(state_,pendingOrders_.player); if(!player) return;
    for(const auto& x:player->objectKnowledge) {
        const QColor color(x.level==ObjectKnowledgeLevel::Studied?"#8dcc9e":x.kind?"#84c4e0":"#d2ad61");
        if(x.uncertainty) {
            auto* area=scene_->addEllipse(x.position.x-x.uncertainty,x.position.y-x.uncertainty,
                2*x.uncertainty,2*x.uncertainty,QPen(color,0,Qt::DashLine));
            area->setAcceptedMouseButtons(Qt::NoButton); area->setToolTip(description(x,state_.turn)); area->setZValue(4);
        }
        auto* marker=scene_->addRect(-4,-4,8,8,QPen(color),Qt::NoBrush); marker->setRotation(45);
        marker->setPos(x.position.x,x.position.y); marker->setData(0,x.id); marker->setData(1,6);
        marker->setFlag(QGraphicsItem::ItemIgnoresTransformations); marker->setAcceptedMouseButtons(Qt::NoButton);
        marker->setToolTip(description(x,state_.turn)); marker->setZValue(8);
        auto* label=new QGraphicsSimpleTextItem(QString("X%1").arg(x.id),marker); label->setRotation(-45);
        label->setPos(6,-9); label->setBrush(color); label->setAcceptedMouseButtons(Qt::NoButton); label->setToolTip(description(x,state_.turn));
    }
}
} // namespace suns
