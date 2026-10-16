#include "main_window.hpp"
#include "suns/unknown_objects.hpp"
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
    QString text=QString("Object X%1 — %2\n%3\nObserved year %4; received %5; age %6 years.\nPosition %7, %8 ±%9 ly.")
        .arg(x.id).arg(x.kind ? unknown_object_name(*x.kind) : "Unclassified physical contact")
        .arg(object_knowledge_name(x.level)).arg(qulonglong(x.observedTurn)).arg(qulonglong(x.receivedTurn))
        .arg(qulonglong(turn>x.observedTurn?turn-x.observedTurn:0))
        .arg(x.position.x,0,'f',1).arg(x.position.y,0,'f',1).arg(x.uncertainty);
    if(x.kind) text+=QString("\nStudy objective: %1 research points in %2, once per empire, after data delivery.")
        .arg(unknown_object_research_points(*x.kind)).arg(QString::fromStdString(research_field_name(unknown_object_research_field(*x.kind))));
    else text+="\nApproach with active scanners to classify; no contents or scientific value identified yet.";
    return text;
}
}
void MainWindow::installUnknownObjects()
{
    if(findChild<QDockWidget*>("unknownObjectsDock")) return;
    auto* dock=new QDockWidget("Unknown objects / expeditions",this); dock->setObjectName("unknownObjectsDock");
    auto* body=new QWidget(dock); auto* layout=new QVBoxLayout(body);
    auto* hint=new QLabel("Received physical contacts only. Select a fleet and a row. Fly to the reported area, or arrive with Field research. Active scanners classify nearby objects; study within 20 ly (limited by sensor range) yields finite data, usable after delivery. Objects remain as landmarks.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* tree=new QTreeWidget(body); tree->setObjectName("unknownObjectContacts");
    tree->setHeaderLabels({"Object","Knowledge","Observed / received","Position ± ly","Science objective"});
    tree->setRootIsDecorated(false); tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents); layout->addWidget(tree);
    auto* buttons=new QHBoxLayout; auto* locate=new QPushButton("Show area",body);
    locate->setObjectName("locateUnknownObject");
    auto* fly=new QPushButton("Fly to object",body); fly->setObjectName("approachUnknownObject");
    auto* study=new QPushButton("Fly and study",body); study->setObjectName("studyUnknownObject");
    buttons->addWidget(locate); buttons->addWidget(fly); buttons->addWidget(study); layout->addLayout(buttons);
    dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea,dock); dock->hide();
    if(auto* menu=menuBar()->findChild<QMenu*>("sunsViewMenu")) menu->addAction(dock->toggleViewAction());
    else menuBar()->addAction(dock->toggleViewAction());
    const auto update=[this,tree,fly,study,locate] {
        const auto* row=tree->currentItem(); const auto* x=row?known_unknown_object(state_,pendingOrders_.player,row->data(0,Qt::UserRole).toUInt()):nullptr;
        const auto* f=selectedFleet(); locate->setEnabled(x); fly->setEnabled(x && f);
        study->setEnabled(x && f && x->level!=ObjectKnowledgeLevel::Studied && fleet_has_scanner(state_,*f));
    };
    connect(tree,&QTreeWidget::itemSelectionChanged,this,update);
    connect(fly,&QPushButton::clicked,this,[this,tree] { if(auto* row=tree->currentItem()) queueUnknownObjectApproach(row->data(0,Qt::UserRole).toUInt(),false); });
    connect(study,&QPushButton::clicked,this,[this,tree] { if(auto* row=tree->currentItem()) queueUnknownObjectApproach(row->data(0,Qt::UserRole).toUInt(),true); });
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
    const auto* fleet=selectedFleet(); const auto* x=known_unknown_object(state_,pendingOrders_.player,id);
    if(!fleet || !x || (study && (x->level==ObjectKnowledgeLevel::Studied || !fleet_has_scanner(state_,*fleet)))) return false;
    FleetArrivalAction action; if(study) action.kind=FleetArrivalActionKind::FieldResearch;
    replacePendingFleetMove(fleet->id,x->position,selectedFleetSuggestedWarpForRouteProgram(),action,
        QString("%1 object X%2 at reported coordinates").arg(study?"Study":"Approach").arg(id));
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
