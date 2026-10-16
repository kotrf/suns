#include "main_window.hpp"
#include "suns/interstellar_medium.hpp"

#include <QAction>
#include <QDockWidget>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace suns {
namespace {
QString mediumDescription(const MediumObservation& x,std::uint64_t turn)
{
    return QString("Cloud C%1 — %2\nObserved year %3; received %4; age %5 years.\n"
        "Reported centre: %6, %7; radii %8 × %9 ly.\nPeak density: %10–%11. %12\n"
        "Density fades smoothly towards the boundary. Conventional fuel use rises by up to 30%; "
        "ram-scoop collection rises by up to 75%, consumption falls by up to 10%. "
        "Active sensors lose up to 20% local range plus up to 30% along the sight line. "
        "Passive reception and radio propagation retain their rules. Free flight and engine safe Warp remain available.")
        .arg(x.id).arg(x.level==MediumKnowledgeLevel::Characterized?"Characterized":"Detected")
        .arg(qulonglong(x.observedTurn)).arg(qulonglong(x.receivedTurn))
        .arg(qulonglong(turn>x.observedTurn?turn-x.observedTurn:0))
        .arg(x.center.x,0,'f',1).arg(x.center.y,0,'f',1).arg(x.radiusX,0,'f',1).arg(x.radiusY,0,'f',1)
        .arg(x.densityMinimum,0,'f',2).arg(x.densityMaximum,0,'f',2)
        .arg(x.level==MediumKnowledgeLevel::Characterized?"Measured profile.":
            "Approximate footprint: centre in a 20 ly cell, rounded radii. Fly inside the core with active scanners to characterize.");
}
}
void MainWindow::installInterstellarMedium()
{
    if (findChild<QDockWidget*>("interstellarMediumDock")) return;
    auto* dock=new QDockWidget("Interstellar medium",this); dock->setObjectName("interstellarMediumDock");
    auto* body=new QWidget(dock); auto* layout=new QVBoxLayout(body);
    auto* hint=new QLabel("Received cloud surveys only. Active scanners detect broad regions; entering a cloud's core characterizes its profile. "
        "Routes use received profiles with an uncertainty range for unsurveyed density. Older campaigns retain uniform space.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* tree=new QTreeWidget(body); tree->setObjectName("interstellarCloudContacts"); tree->setRootIsDecorated(false);
    tree->setHeaderLabels({"Cloud","Knowledge","Observed / received","Peak density","Reported radii, ly"});
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents); layout->addWidget(tree);
    auto* locate=new QPushButton("Show area",body); locate->setObjectName("locateInterstellarCloud"); locate->setEnabled(false);
    layout->addWidget(locate); dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea,dock); dock->hide();
    auto* menu=menuBar()->findChild<QMenu*>("sunsViewMenu");
    auto* overlay=new QAction("Interstellar medium overlay",this); overlay->setObjectName("interstellarMediumOverlay");
    overlay->setCheckable(true); overlay->setChecked(QSettings{}.value("map/interstellarMedium",true).toBool());
    if (menu) { menu->addAction(dock->toggleViewAction()); menu->addAction(overlay); }
    else { menuBar()->addAction(dock->toggleViewAction()); menuBar()->addAction(overlay); }
    connect(overlay,&QAction::toggled,this,[this](bool checked) { QSettings{}.setValue("map/interstellarMedium",checked); rebuildScene(); });
    connect(tree,&QTreeWidget::itemSelectionChanged,this,[tree,locate] { locate->setEnabled(tree->currentItem()); });
    connect(locate,&QPushButton::clicked,this,[this,tree] { if (auto* row=tree->currentItem())
        view_->centerOn(row->data(0,Qt::UserRole+1).toDouble(),row->data(0,Qt::UserRole+2).toDouble()); });
    connect(this,&MainWindow::routeProgramContextChanged,this,[this,tree,locate] { refreshInterstellarMedium(); locate->setEnabled(tree->currentItem()); });
    refreshInterstellarMedium();
}
void MainWindow::refreshInterstellarMedium()
{
    auto* tree=findChild<QTreeWidget*>("interstellarCloudContacts"); if (!tree) return;
    const auto selected=tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toUInt():0;
    const QSignalBlocker block(tree); tree->clear();
    if (const auto* p=find_player(state_,pendingOrders_.player)) for (const auto& x:p->mediumKnowledge) {
        auto* row=new QTreeWidgetItem(tree); row->setData(0,Qt::UserRole,x.id);
        row->setData(0,Qt::UserRole+1,x.center.x); row->setData(0,Qt::UserRole+2,x.center.y);
        row->setText(0,QString("C%1").arg(x.id));
        row->setText(1,x.level==MediumKnowledgeLevel::Characterized?"Characterized":"Detected");
        row->setText(2,QString("%1 / %2 (age %3)").arg(qulonglong(x.observedTurn)).arg(qulonglong(x.receivedTurn))
            .arg(qulonglong(state_.turn>x.observedTurn?state_.turn-x.observedTurn:0)));
        row->setText(3,QString("%1–%2").arg(x.densityMinimum,0,'f',2).arg(x.densityMaximum,0,'f',2));
        row->setText(4,QString("%1 × %2").arg(x.radiusX,0,'f',0).arg(x.radiusY,0,'f',0));
        for (int column=0;column<5;++column) row->setToolTip(column,mediumDescription(x,state_.turn));
        if (x.id==selected) tree->setCurrentItem(row);
    }
}
void MainWindow::renderKnownInterstellarMedium()
{
    if (const auto* overlay=findChild<QAction*>("interstellarMediumOverlay"); overlay && !overlay->isChecked()) return;
    const auto* p=find_player(state_,pendingOrders_.player); if (!p) return;
    for (const auto& x:p->mediumKnowledge) {
        for (int layer=8;layer>=1;--layer) {
            const double scale=layer/8.0;
            const QColor color(x.level==MediumKnowledgeLevel::Characterized?80:116,116,160,7);
            auto* area=scene_->addEllipse(x.center.x-x.radiusX*scale,x.center.y-x.radiusY*scale,
                2*x.radiusX*scale,2*x.radiusY*scale,Qt::NoPen,QBrush(color));
            area->setData(0,x.id); area->setData(1,7); area->setZValue(-10);
            area->setAcceptedMouseButtons(Qt::NoButton); area->setToolTip(mediumDescription(x,state_.turn));
        }
        auto* edge=scene_->addEllipse(x.center.x-x.radiusX,x.center.y-x.radiusY,2*x.radiusX,2*x.radiusY,
            QPen(QColor(112,145,179,100),0,x.level==MediumKnowledgeLevel::Characterized?Qt::SolidLine:Qt::DashLine));
        edge->setData(0,x.id); edge->setData(1,7); edge->setZValue(-9); edge->setAcceptedMouseButtons(Qt::NoButton);
        edge->setToolTip(mediumDescription(x,state_.turn));
    }
}
} // namespace suns
