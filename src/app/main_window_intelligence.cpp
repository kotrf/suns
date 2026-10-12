#include "main_window.hpp"
#include "suns/reconnaissance.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace suns {
void MainWindow::openReconnaissance()
{
    if(auto* existing=findChild<QDialog*>("intelligenceDialog")) { existing->show(); existing->raise(); existing->activateWindow(); return; }
    auto* dialog=new QDialog(this,Qt::Window); dialog->setObjectName("intelligenceDialog"); dialog->setWindowTitle("Reconnaissance / received contacts");
    dialog->resize(1100,550);
    dialog->setStyleSheet("QDialog#intelligenceDialog { background: #0b121a; } QTableWidget { background: #101d29; color: #d6e5f2; gridline-color: #34495a; } QHeaderView::section { background: #1b2c3b; color: #d6e5f2; padding: 4px; }");
    auto* layout=new QVBoxLayout(dialog);
    auto* explanation=new QLabel("Received observations and connected sensors only. Probable contacts have unknown owners; identification reveals hull classes and ship counts, never equipment/cargo/orders. A stale marker is a last reported position. Lost contact does not mean destruction. Anonymous emissions/decoys remain unverified areas in Turn Messages.",dialog);
    explanation->setWordWrap(true); layout->addWidget(explanation);
    auto* table=new QTableWidget(dialog); table->setObjectName("reconnaissanceTable"); table->setColumnCount(10);
    table->setHorizontalHeaderLabels({"Contact","Quality","State","Owner","Seen","Age","Position ± ly","Mass kt","Ships / hulls","Observer"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); table->horizontalHeader()->setSectionResizeMode(8,QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->verticalHeader()->hide(); layout->addWidget(table,1);
    auto* status=new QLabel(dialog); status->setObjectName("reconnaissanceSummary"); status->setWordWrap(true); layout->addWidget(status);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,dialog);
    auto* locate=buttons->addButton("Show last position",QDialogButtonBox::ActionRole); locate->setObjectName("locateReconContact"); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::hide);
    connect(locate,&QPushButton::clicked,dialog,[this,table] {
        if(table->currentRow()<0) return;
        const auto id=table->item(table->currentRow(),0)->data(Qt::UserRole).toUInt();
        const auto contacts=known_enemy_contacts(state_,pendingOrders_.player);
        const auto found=std::find_if(contacts.begin(),contacts.end(),[&](const auto& c){return c.fleet==id;});
        if(found!=contacts.end()) view_->centerOn(found->position.x,found->position.y);
    });
    auto refresh=[this,dialog,table,status,locate] {
        if(shuttingDown_ || !dialog->isVisible()) return;
        const auto id=table->currentRow()>=0?table->item(table->currentRow(),0)->data(Qt::UserRole).toUInt():0;
        const auto contacts=known_enemy_contacts(state_,pendingOrders_.player);
        table->setRowCount(int(contacts.size())); int active=0,stale=0;
        for(int row=0;row<int(contacts.size());++row) {
            const auto& c=contacts[row]; const auto age=state_.turn>c.lastSeenTurn?state_.turn-c.lastSeenTurn:0;
            if(!c.lost) ++active; if(age) ++stale;
            QStringList hulls; for(auto h:c.hulls) hulls<<QString::fromStdString(hull_spec(h).name);
            const QStringList cells{QString::number(c.fleet),contact_quality_name(c.quality),c.lost?"Lost":age?"Stale":"Fresh",
                c.owner?QString("Empire %1").arg(c.owner):"Unknown",QString::number(qulonglong(c.lastSeenTurn)),QString::number(qulonglong(age)),
                QString("%1, %2 ±%3").arg(c.position.x,0,'f',1).arg(c.position.y,0,'f',1).arg(c.uncertainty),
                QString("%1–%2").arg(c.massMinimum,0,'f',0).arg(c.massMaximum,0,'f',0),
                c.quality==ContactQuality::Identified?QString("%1 / %2").arg(c.shipCount).arg(hulls.join(", ")):"Unknown",
                c.sourceFleet?QString("Fleet %1").arg(c.sourceFleet):QString("Colony %1").arg(c.sourceColony)};
            for(int col=0;col<cells.size();++col) {
                auto* item=new QTableWidgetItem(cells[col]);
                item->setForeground(QColor(c.lost||age?"#9daebc":c.quality==ContactQuality::Probable?"#d2a963":"#dd7777"));
                item->setToolTip(QString("Observed %1, received %2. %3 sensor%4. Last position remains historical evidence.")
                    .arg(qulonglong(c.observedTurn)).arg(qulonglong(c.deliveryTurn))
                    .arg(c.sensor==ContactSensor::Active?"Active":"Passive").arg(c.jammed?"; interference reduced quality":""));
                if(col==0) item->setData(Qt::UserRole,c.fleet); table->setItem(row,col,item);
            }
            if(c.fleet==id) table->selectRow(row);
        }
        status->setText(QString("%1 received contacts • %2 retained tracks • %3 dated positions. Probable = area/estimate; confirmed = localized owner; identified = hull classes and count.").arg(contacts.size()).arg(active).arg(stale));
        locate->setEnabled(table->currentRow()>=0);
    };
    auto* timer=new QTimer(dialog); timer->setInterval(500); connect(timer,&QTimer::timeout,dialog,refresh); timer->start();
    connect(table,&QTableWidget::itemSelectionChanged,dialog,[table,locate]{locate->setEnabled(table->currentRow()>=0);});
    dialog->show(); refresh();
}
}
