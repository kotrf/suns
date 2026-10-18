#include "main_window.hpp"
#include "suns/mobile_base.hpp"
#include "suns/communications.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QStatusBar>
#include <QVBoxLayout>
#include <algorithm>

namespace suns {
void MainWindow::installMobileBases()
{
    if (findChild<QDockWidget*>("mobileBasesDock")) return;
    auto* dock = new QDockWidget("Mobile expedition bases", this); dock->setObjectName("mobileBasesDock");
    auto* body = new QWidget(dock); auto* layout = new QVBoxLayout(body);
    auto* hint = new QLabel("Ship Designer: Expedition Base (Construction 12), four engines, Expedition Dock, "
        "optional Expedition Workshops, tanks, cargo and relay. Build at an orbital shipyard and move with ordinary routes. "
        "A base that moves waits until next year to dock clients; radio and sensors work normally in flight. "
        "Each workshop restores 100 kt of dry structure/year, costing 0.10 I / 0.05 B / 0.05 G per kt. "
        "The persistent client list gives refuel and repair priority. Missing, distant and undamaged clients do not block repair.", body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* bases = new QComboBox(body); bases->setObjectName("mobileBaseSelection"); layout->addWidget(bases);
    auto* stocks = new QLabel(body); stocks->setObjectName("mobileBaseStocks"); stocks->setWordWrap(true); layout->addWidget(stocks);
    auto* enabled = new QCheckBox("Provide external services", body); enabled->setObjectName("mobileBaseEnabled"); layout->addWidget(enabled);
    auto spin = [body](const char* name) {
        auto* s=new QDoubleSpinBox(body); s->setObjectName(name); s->setRange(0,1e12); s->setDecimals(2); return s;
    };
    auto* reserve=spin("mobileBaseReserve"); reserve->setSuffix(" mg");
    auto* form=new QFormLayout; form->addRow("Keep base fuel",reserve); layout->addLayout(form);
    auto* queue=new QListWidget(body); queue->setObjectName("mobileBaseClients"); layout->addWidget(queue);
    auto* clients=new QComboBox(body); clients->setObjectName("mobileBaseClientSelection"); layout->addWidget(clients);
    auto* row=new QHBoxLayout;
    auto button=[body,row](const char* title,const char* name) {
        auto* b=new QPushButton(title,body); b->setObjectName(name); row->addWidget(b); return b;
    };
    auto* add=button("Add client","mobileBaseAddClient"); auto* remove=button("Remove","mobileBaseRemoveClient");
    auto* up=button("Up","mobileBaseClientUp"); auto* down=button("Down","mobileBaseClientDown"); layout->addLayout(row);
    auto* apply=new QPushButton("Send service policy",body); apply->setObjectName("mobileBaseApply"); layout->addWidget(apply);
    auto* forecast=new QLabel(body); forecast->setObjectName("mobileBaseForecast"); forecast->setWordWrap(true); layout->addWidget(forecast);
    auto* fly=new QPushButton("Fly selected fleet to base's received position",body); fly->setObjectName("mobileBaseApproach"); layout->addWidget(fly);
    auto* cargoForm=new QFormLayout;
    auto* iron=spin("mobileBaseTransferIronium"); auto* bor=spin("mobileBaseTransferBoranium"); auto* ger=spin("mobileBaseTransferGermanium");
    cargoForm->addRow("Transfer ironium, kt",iron); cargoForm->addRow("Boranium, kt",bor); cargoForm->addRow("Germanium, kt",ger);
    auto* fuel=spin("mobileBaseTransferFuel"); auto* donorReserve=spin("mobileBaseDonorReserve"); donorReserve->setValue(100);
    cargoForm->addRow("Transfer fuel, mg",fuel); cargoForm->addRow("Keep donor fuel, mg",donorReserve); layout->addLayout(cargoForm);
    auto* transferHint=new QLabel("Transfers use the client selected above. At a received shared position, "
        "send fuel/mineral delivery commands in either direction. Commands travel to the base; remote positions and stocks may change before delivery. Minerals share the ordinary hold with all cargo. "
        "Automatic queued service happens once after movement, delivery and onboard repair. Forecast uses received snapshots; "
        "remote outcomes and later orders may differ.",body); transferHint->setWordWrap(true); layout->addWidget(transferHint);
    auto* transfers=new QHBoxLayout;
    auto* deliver=new QPushButton("Client → base",body); deliver->setObjectName("mobileBaseDeliver"); transfers->addWidget(deliver);
    auto* collect=new QPushButton("Base → client",body); collect->setObjectName("mobileBaseCollect"); transfers->addWidget(collect); layout->addLayout(transfers);
    auto* scroll=new QScrollArea(dock); scroll->setWidgetResizable(true); scroll->setWidget(body);
    dock->setWidget(scroll); addDockWidget(Qt::RightDockWidgetArea,dock); dock->hide(); menuBar()->addAction(dock->toggleViewAction());

    // Rebuild a planning world from received owner evidence before every query.
    const auto knownWorld=[this] {
        GameState known=state_; known.fleets.clear();
        for (const auto& f:state_.fleets) if (f.owner==pendingOrders_.player) known.fleets.push_back(fleet_player_view(state_,f));
        for (const auto& f:missing_fleet_contacts(state_,pendingOrders_.player))
            if (!find_service_fleet(known,f.id)) known.fleets.push_back(fleet_player_view(state_,f));
        return known;
    };
    const auto policy=[enabled,reserve,queue] {
        MobileBaseProgram p; p.enabled=enabled->isChecked(); p.fuelReserve=reserve->value();
        for(int i=0;i<queue->count();++i) p.clients.push_back(queue->item(i)->data(Qt::UserRole).toUInt());
        return p;
    };
    const auto update=[=,this] {
        auto known=knownWorld(); const auto* base=find_service_fleet(known,bases->currentData().toUInt());
        apply->setEnabled(base && mobile_base_dock(known,*base)); fly->setEnabled(base && selectedFleetPlanningView() && selectedFleetPlanningView()->id!=base->id);
        if (!base) { stocks->setText("No received expedition base. Design and build one at an orbital shipyard."); forecast->clear(); deliver->setEnabled(false); collect->setEnabled(false); return; }
        const auto id=base->id; const auto snapshot=*base;
        stocks->setText(QString("At %1, %2 ly. ").arg(snapshot.position.x,0,'f',1).arg(snapshot.position.y,0,'f',1) + QString("Received year %1 (age %2): fuel %3 / %4 mg; I %5, B %6, G %7 kt; damage %8%; workshops %9 kt/year")
            .arg(qulonglong(snapshot.telemetry.observedTurn)).arg(qulonglong(state_.turn>snapshot.telemetry.observedTurn?state_.turn-snapshot.telemetry.observedTurn:0))
            .arg(snapshot.fuel,0,'f',1).arg(fleet_fuel_capacity(known,snapshot),0,'f',1).arg(snapshot.minerals.ironium,0,'f',1)
            .arg(snapshot.minerals.boranium,0,'f',1).arg(snapshot.minerals.germanium,0,'f',1).arg(snapshot.damagePercent,0,'f',1)
            .arg(mobile_base_repair_capacity(known,snapshot),0,'f',1));
        for(auto& f:known.fleets) {
            f.mobileBase.enabled=false;
            if(f.id==id) { f.mobileBase=policy(); f.lastServiceTurn=0; }
        }
        // Show the effect of locally queued movement without inventing a delivered report.
        for(const auto& order:pendingOrders_.orders) {
            if(const auto* move=std::get_if<MoveFleetOrder>(&order); move && move->fleet==id)
                for(auto& f:known.fleets) if(f.id==id) {
                    if(move->clearRoute) { f.destination.reset(); f.targetFleet=0; f.waypointQueue.clear(); f.repeatOrders=false; }
                    else f.lastMovedTurn=state_.turn+1;
                }
            if(const auto* gate=std::get_if<GateFleetOrder>(&order); gate && gate->fleet==id)
                for(auto& f:known.fleets) if(f.id==id) f.lastMovedTurn=state_.turn+1;
        }
        const auto error=mobile_base_service_error(known,*find_service_fleet(known,id),state_.turn+1);
        QString text=error.empty()?"Service-policy preview from received stocks (after command delivery):":QString::fromStdString(error);
        if(error.empty()) {
            if(mobile_base_repair_capacity(known,snapshot)<=0) text+="\nNo workshops fitted; refuel and cargo only.";
            else if(snapshot.minerals.ironium<=0 || snapshot.minerals.boranium<=0 || snapshot.minerals.germanium<=0)
                text+="\nRepair blocked: workshop minerals exhausted.";
            if(snapshot.fuel<=policy().fuelReserve) text+="\nRefuel blocked: no fuel above base reserve.";
        }
        const auto before=known; service_mobile_bases(known,state_.turn+1);
        for(auto client:policy().clients) {
            const auto* a=find_service_fleet(before,client); const auto* b=find_service_fleet(known,client);
            QString detail="missing client";
            if(a && b) {
                if(!same_position(a->position,snapshot.position)) detail="not at base";
                else detail=QString("repair %1 percentage points; fuel +%2 mg").arg(a->damagePercent-b->damagePercent,0,'f',2).arg(b->fuel-a->fuel,0,'f',1);
            }
            text+=QString("\n#%1: %2").arg(client).arg(detail);
        }
        forecast->setText(text);
        const auto* client=find_service_fleet(before,clients->currentData().toUInt());
        const bool transfer=client && same_position(snapshot.position,client->position) && error.empty();
        deliver->setEnabled(transfer); collect->setEnabled(transfer);
    };
    const auto loadPolicy=[=,this] {
        const auto known=knownWorld(); const auto* base=find_service_fleet(known,bases->currentData().toUInt());
        MobileBaseProgram p; if(base) p=base->mobileBase;
        for(const auto& order:pendingOrders_.orders) if(const auto* o=std::get_if<SetMobileBaseOrder>(&order); o && base && o->fleet==base->id) p=o->program;
        { const QSignalBlocker block(enabled); enabled->setChecked(p.enabled); }
        { const QSignalBlocker block(reserve); reserve->setValue(p.fuelReserve); }
        queue->clear();
        for(auto id:p.clients) {
            const auto* client=find_service_fleet(known,id);
            auto* item=new QListWidgetItem(QString("#%1 — %2").arg(id).arg(client?QString::fromStdString(client->name):"missing"),queue);
            item->setData(Qt::UserRole,id);
        }
        update();
    };
    const auto refresh=[=,this] {
        const auto selected=bases->currentData().toUInt(); const auto selectedClient=clients->currentData().toUInt();
        { const QSignalBlocker b(bases); bases->clear();
            const auto known=knownWorld();
            for(const auto& f:known.fleets) if(mobile_base_hulls(known,f)>0) bases->addItem(QString("%1 (#%2)").arg(QString::fromStdString(f.name)).arg(f.id),f.id);
            const int i=bases->findData(selected); if(i>=0) bases->setCurrentIndex(i);
        }
        { const QSignalBlocker b(clients); clients->clear();
            for(const auto& f:knownWorld().fleets) if(f.id!=bases->currentData().toUInt()) clients->addItem(QString("%1 (#%2)").arg(QString::fromStdString(f.name)).arg(f.id),f.id);
            const int i=clients->findData(selectedClient); if(i>=0) clients->setCurrentIndex(i);
        }
        loadPolicy();
    };
    connect(bases,&QComboBox::currentIndexChanged,this,[refresh](int){refresh();});
    connect(clients,&QComboBox::currentIndexChanged,this,[update](int){update();});
    connect(enabled,&QCheckBox::toggled,this,[update]{update();}); connect(reserve,&QDoubleSpinBox::valueChanged,this,[update]{update();});
    connect(add,&QPushButton::clicked,this,[=] {
        const auto id=clients->currentData().toUInt(); if(!id || queue->count()>=128) return;
        for(int i=0;i<queue->count();++i) if(queue->item(i)->data(Qt::UserRole).toUInt()==id) return;
        auto* item=new QListWidgetItem(clients->currentText(),queue); item->setData(Qt::UserRole,id); update();
    });
    connect(remove,&QPushButton::clicked,this,[=]{delete queue->takeItem(queue->currentRow()); update();});
    const auto reorder=[=](int delta){int i=queue->currentRow(); if(i<0 || i+delta<0 || i+delta>=queue->count()) return; auto* item=queue->takeItem(i); queue->insertItem(i+delta,item); queue->setCurrentRow(i+delta); update();};
    connect(up,&QPushButton::clicked,this,[reorder]{reorder(-1);}); connect(down,&QPushButton::clicked,this,[reorder]{reorder(1);});
    connect(apply,&QPushButton::clicked,this,[=,this] {
        const auto id=bases->currentData().toUInt(); if(!id) return;
        appendPendingOrder(SetMobileBaseOrder{id,policy()},QString("Send service policy to mobile base #%1").arg(id)); rebuildScene();
    });
    connect(fly,&QPushButton::clicked,this,[=,this] {
        const auto known=knownWorld(); const auto* base=find_service_fleet(known,bases->currentData().toUInt()); const auto fleet=selectedFleetPlanningView();
        if(base && fleet && fleet->id!=base->id) { replacePendingFleetMove(fleet->id,base->position,selectedFleetSuggestedWarpForRouteProgram(),{},"Approach mobile base's received position"); rebuildScene(); }
    });
    const auto transfer=[=,this](bool toBase) {
        const auto base=bases->currentData().toUInt(), client=clients->currentData().toUInt(); if(!base || !client) return;
        const auto from=toBase?client:base, to=toBase?base:client;
        const MineralCargo minerals{iron->value(),bor->value(),ger->value()};
        if(mineral_cargo_mass(minerals)>0) appendPendingOrder(TransferCargoOrder{{0,from},{0,to},0,minerals},"Transfer mobile-base minerals");
        if(fuel->value()>0) appendPendingOrder(TransferFleetFuelOrder{from,to,fuel->value(),donorReserve->value()},"Transfer mobile-base fuel");
        rebuildScene();
    };
    connect(deliver,&QPushButton::clicked,this,[transfer]{transfer(true);}); connect(collect,&QPushButton::clicked,this,[transfer]{transfer(false);});
    connect(this,&MainWindow::routeProgramContextChanged,this,refresh); refresh();
}
} // namespace suns
