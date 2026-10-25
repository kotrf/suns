#include "main_window.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/communications.hpp"
#include "suns/deep_space.hpp"
#include "suns/mobile_base.hpp"
#include "suns/player_knowledge.hpp"
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <limits>

namespace suns {
void MainWindow::installOrbitalBases()
{
    if (findChild<QDockWidget*>("orbitalBasesDock")) return;
    auto* dock=new QDockWidget("Independent orbital shipyards",this); dock->setObjectName("orbitalBasesDock");
    auto* body=new QWidget(dock); auto* layout=new QVBoxLayout(body);
    auto* hint=new QLabel("Design an Expedition Base with four engines and an Orbital Shipyard Kit (Construction 14, Energy/Biology 8). "
        "Fit at most one Bulk / Composite / Advanced Radiation Screen (20 / 40 / 60 protection), plus optional cargo, relay, dock or workshops. "
        "Build and load workers/minerals through ordinary colony controls. Fly here, then send Deploy. "
        "Anchoring is permanent; six assembly stages cost I/B/G 20/10/20 each and need 5,000 workers. "
        "25,000 workers yield 40 construction/year; no population growth or surface mining. "
        "Occupied upkeep is 1 mg fuel and I/B/G 1/1/1 yearly; three emergency years, then 10% losses. "
        "Radiation losses apply independently. Commands and reports use ordinary communication delays.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto combo=[&](const char* name) { auto* c=new QComboBox(body); c->setObjectName(name); layout->addWidget(c); return c; };
    auto* bases=combo("orbitalBaseSelection"); auto* planets=combo("orbitalBasePlanet");
    auto* status=new QLabel(body); status->setObjectName("orbitalBaseStatus"); status->setWordWrap(true); layout->addWidget(status);
    auto button=[&](const char* name,const char* text) { auto* b=new QPushButton(text,body); b->setObjectName(name); layout->addWidget(b); return b; };
    auto* approach=button("orbitalBaseApproach","Fly constructor to selected planet");
    auto* deploy=button("orbitalBaseDeploy","Deploy here permanently");
    auto* pause=button("orbitalBasePause","Pause construction"); auto* resume=button("orbitalBaseResume","Resume construction");
    auto* designs=combo("orbitalBaseDesign");
    auto* build=button("orbitalBaseBuild","Queue selected ship design");
    auto* refit=button("orbitalBaseRefit","Queue full paid base refit to selected design");
    auto* queue=new QListWidget(body); queue->setObjectName("orbitalBaseQueue"); layout->addWidget(queue);
    auto* cancel=button("orbitalBaseCancel","Cancel selected build (spent work lost)");
    auto* clients=combo("orbitalBaseTransport"); auto* form=new QFormLayout; layout->addLayout(form);
    auto amount=[&](const char* name,const char* label) {
        auto* spin=new QDoubleSpinBox(body); spin->setObjectName(name); spin->setRange(0,1e9); spin->setDecimals(2); form->addRow(label,spin); return spin;
    };
    auto* people=new QSpinBox(body); people->setObjectName("orbitalBasePeople"); people->setRange(0,100000000); form->addRow("People to transfer",people);
    auto* iron=amount("orbitalBaseIronium","Ironium, kt"); auto* bor=amount("orbitalBaseBoranium","Boranium, kt");
    auto* ger=amount("orbitalBaseGermanium","Germanium, kt"); auto* fuel=amount("orbitalBaseFuel","Fuel, mg");
    auto* keep=amount("orbitalBaseKeepFuel","Keep donor fuel, mg"); keep->setValue(100);
    auto* deliver=button("orbitalBaseDeliver","Transport → base: deliver people / minerals / fuel");
    auto* evacuate=button("orbitalBaseEvacuate","Base → transport: evacuate / collect");
    auto* scroll=new QScrollArea(dock); scroll->setWidgetResizable(true); scroll->setWidget(body); dock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea,dock); dock->hide(); menuBar()->addAction(dock->toggleViewAction());
    const auto knownWorld=[this] { return make_player_view(state_,pendingOrders_.player).state; };
    const auto update=[=,this] {
        auto known=knownWorld(); const auto* f=find_service_fleet(known,bases->currentData().toUInt());
        for (auto* button : {approach,deploy,pause,resume,build,refit,cancel,deliver,evacuate}) button->setEnabled(f!=nullptr);
        queue->clear(); if (!f) { status->setText("No received constructor or orbital base. Build one in Ship Designer."); return; }
        const auto& b=f->orbitalBase;
        const auto background=f->outpost==OutpostKind::OrbitalShipyard ? b.background : [&] {
            const auto* p=find_planet(known,planets->currentData().toUInt()); const auto* s=p?find_star(known,p->star):nullptr;
            return s?orbital_background(*s):std::uint8_t(0);
        }();
        auto preview=*f; preview.orbitalBase.background=background;
        status->setText(QString("Received year %1 (age %2): %3\nWorkers %4; shared hold %5 / %6 kt; fuel %7 mg; I/B/G %8 / %9 / %10 kt.\n"
            "Assembly %11 / 6; life-support reserve %12 years. Radiation forecast: background %13 − screen %14 = exposure %15; %16 people lost/year. "
            "No future deliveries assumed. Full refits pay the complete selected design price; existing protection works until completion.")
            .arg(qulonglong(f->telemetry.observedTurn)).arg(qulonglong(state_.turn>f->telemetry.observedTurn?state_.turn-f->telemetry.observedTurn:0))
            .arg(QString::fromStdString(orbital_base_status(known,*f))).arg(qulonglong(f->colonists))
            .arg(fleet_cargo_used(known,*f),0,'f',1).arg(fleet_cargo_capacity(known,*f),0,'f',1).arg(f->fuel,0,'f',1)
            .arg(f->minerals.ironium,0,'f',1).arg(f->minerals.boranium,0,'f',1).arg(f->minerals.germanium,0,'f',1)
            .arg(b.assemblyYears).arg(b.reserveYears).arg(background).arg(orbital_screen(known,*f))
            .arg(std::max(0,int(background)-orbital_screen(known,*f))).arg(qulonglong(orbital_radiation_losses(known,preview))));
        // Local drafts are shown as intent, without inventing delivery or stocks.
        auto intent=*f;
        for (const auto& actual:state_.fleets) if(actual.id==f->id && actual.owner==pendingOrders_.player)
            for (const auto& command:actual.pendingCommands) if(command.orbitalBase)
                apply_orbital_base_command(known,intent,*command.orbitalBase);
        for (const auto& order:pendingOrders_.orders) if (const auto* o=std::get_if<OrbitalBaseOrder>(&order); o && o->fleet==f->id)
            apply_orbital_base_command(known,intent,o->command);
        for (const auto& item:intent.orbitalBase.queue) {
            const auto* d=find_ship_design(known,item.design); if (!d) continue;
            const auto m=ship_design_mineral_cost(*d);
            auto* row=new QListWidgetItem(QString("%1%2 — work %3 / %4; I/B/G %5/%6/%7 (received queue + sent/local intent)")
                .arg(item.refit?"Refit: ":"").arg(QString::fromStdString(d->name)).arg(item.work).arg(ship_design_cost(*d))
                .arg(m.ironium).arg(m.boranium).arg(m.germanium),queue);
            row->setData(Qt::UserRole,qulonglong(item.id));
        }
        const bool anchored=f->outpost==OutpostKind::OrbitalShipyard;
        approach->setEnabled(!anchored); deploy->setEnabled(!anchored && planets->count());
        build->setEnabled(anchored && designs->count()); refit->setEnabled(anchored && designs->count());
        pause->setEnabled(anchored); resume->setEnabled(anchored);
        const auto* client=find_service_fleet(known,clients->currentData().toUInt());
        const bool local=anchored && client && same_position(client->position,f->position);
        deliver->setEnabled(local); evacuate->setEnabled(local);
    };
    const auto refresh=[=,this] {
        const auto known=knownWorld();
        auto fill=[&](QComboBox* c,auto add) { const auto selected=c->currentData(); const QSignalBlocker block(c); c->clear(); add(); const auto i=c->findData(selected); if(i>=0)c->setCurrentIndex(i); };
        fill(bases,[&] { for(const auto& f:known.fleets) if(f.owner==pendingOrders_.player
            && (f.outpost==OutpostKind::OrbitalShipyard || fleet_construction_kit(known,f)==OutpostKind::OrbitalShipyard))
            bases->addItem(QString("#%1 %2").arg(f.id).arg(QString::fromStdString(f.name)),f.id); });
        fill(planets,[&] { for(const auto& p:known.planets) if((!p.owner || p.owner==pendingOrders_.player)
            && survey_level(known,pendingOrders_.player,p.star)>=SurveyLevel::OrbitalSurvey) {
            const auto* star=find_star(known,p.star); if(star) planets->addItem(QString("%1 — orbit radiation %2").arg(QString::fromStdString(p.name)).arg(orbital_background(*star)),p.id);
        }});
        fill(designs,[&] { for(const auto& d:known.shipDesigns) if(d.owner==pendingOrders_.player && ship_design_available_to_player(known,d.owner,d)) designs->addItem(QString::fromStdString(d.name),d.id); });
        fill(clients,[&] { for(const auto& f:known.fleets) if(f.owner==pendingOrders_.player && f.outpost==OutpostKind::None
            && f.id!=bases->currentData().toUInt()) clients->addItem(QString("#%1 %2").arg(f.id).arg(QString::fromStdString(f.name)),f.id); });
        update();
    };
    auto send=[=,this](OrbitalBaseCommand c) {
        if (!bases->currentData().toUInt() || !orbital_base_command_valid(c)) return;
        if(c.action==OrbitalBaseAction::QueueShip || c.action==OrbitalBaseAction::Refit) {
            const auto known=knownWorld();const auto* base=find_service_fleet(known,bases->currentData().toUInt());
            auto latest=std::max<std::uint64_t>(state_.turn<<32,base?base->orbitalBase.lastItemId:0);
            for(const auto& actual:state_.fleets)if(base && actual.id==base->id && actual.owner==pendingOrders_.player)
                for(const auto& cmd:actual.pendingCommands)if(cmd.orbitalBase)latest=std::max(latest,cmd.orbitalBase->item);
            for(const auto& o:pendingOrders_.orders)if(const auto* cmd=std::get_if<OrbitalBaseOrder>(&o);cmd && base && cmd->fleet==base->id)latest=std::max(latest,cmd->command.item);
            if(latest==std::numeric_limits<std::uint64_t>::max())return;
            c.item=latest+1;
        }
        appendPendingOrder(OrbitalBaseOrder{bases->currentData().toUInt(),c},"Send orbital shipyard command"); rebuildScene(); };
    connect(deploy,&QPushButton::clicked,this,[=] { send({OrbitalBaseAction::Deploy,planets->currentData().toUInt()}); });
    connect(approach,&QPushButton::clicked,this,[=,this] {
        const auto known=knownWorld(); const auto* p=find_planet(known,planets->currentData().toUInt()); const auto* s=p?find_star(known,p->star):nullptr;
        if(s) { appendPendingOrder(MoveFleetOrder{bases->currentData().toUInt(),s->position,5},"Fly orbital constructor"); rebuildScene(); }
    });
    connect(pause,&QPushButton::clicked,this,[=] { send({OrbitalBaseAction::SetActive,0,0,0,false}); });
    connect(resume,&QPushButton::clicked,this,[=] { send({OrbitalBaseAction::SetActive}); });
    connect(build,&QPushButton::clicked,this,[=] { send({OrbitalBaseAction::QueueShip,0,designs->currentData().toUInt()}); });
    connect(refit,&QPushButton::clicked,this,[=] { send({OrbitalBaseAction::Refit,0,designs->currentData().toUInt()}); });
    connect(cancel,&QPushButton::clicked,this,[=] { if(queue->currentRow()>=0)send({OrbitalBaseAction::Cancel,0,0,queue->currentItem()->data(Qt::UserRole).toULongLong()}); });
    auto transfer=[=,this](bool toBase) {
        const auto a=bases->currentData().toUInt(),b=clients->currentData().toUInt(); if(!a || !b)return;
        const auto from=toBase?b:a,to=toBase?a:b;
        const MineralCargo m{iron->value(),bor->value(),ger->value()};
        if(people->value() || mineral_cargo_mass(m)>0) appendPendingOrder(TransferCargoOrder{{0,from},{0,to},std::uint64_t(people->value()),m},"Transfer orbital base cargo / workers");
        if(fuel->value()>0)appendPendingOrder(TransferFleetFuelOrder{from,to,fuel->value(),keep->value()},"Transfer finite orbital base fuel");
        rebuildScene();
    };
    connect(deliver,&QPushButton::clicked,this,[=] { transfer(true); }); connect(evacuate,&QPushButton::clicked,this,[=] { transfer(false); });
    connect(bases,&QComboBox::currentIndexChanged,this,[=](int){refresh();});
    connect(planets,&QComboBox::currentIndexChanged,this,[=](int){update();}); connect(clients,&QComboBox::currentIndexChanged,this,[=](int){update();});
    connect(this,&MainWindow::routeProgramContextChanged,this,refresh); refresh();
}
} // namespace suns
