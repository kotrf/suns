#include "main_window.hpp"
#include "suns/transport_lines.hpp"
#include "suns/communications.hpp"
#include "suns/mobile_base.hpp"
#include "suns/deep_space.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <limits>

namespace suns {
void MainWindow::installTransportLines() {
    if(findChild<QDockWidget*>("transportLinesDock"))return;
    auto* dock=new QDockWidget("Logistics — transport lines",this);dock->setObjectName("transportLinesDock");
    auto* body=new QWidget(dock);auto* layout=new QVBoxLayout(body);auto* form=new QFormLayout;
    auto* hint=new QLabel("Define a mineral shuttle, assign freighters, then submit the turn. Each receives an ordinary repeating "
        "Load All → Unload route through delayed communications. Edits restart at the source when received; pause/delete/release send a Stop. "
        "A zero departure minimum allows empty cycles; a positive minimum waits and tops up locally. Normal shared holds, fuel, refuelling, capture and combat apply. No future deliveries are assumed. Stationary installations accept mineral deliveries within their shared hold; surplus waits aboard. Fuel delivery remains a separate route action.",body);
    hint->setWordWrap(true);layout->addWidget(hint);layout->addLayout(form);
    auto combo=[&](const char* id,const char* text){auto* c=new QComboBox(body);c->setObjectName(id);form->addRow(text,c);return c;};
    auto button=[&](const char* id,const char* text){auto* b=new QPushButton(text,body);b->setObjectName(id);layout->addWidget(b);return b;};
    auto* lines=combo("transportLineSelection","Line (HQ intent)");
    auto* name=new QLineEdit(body);name->setObjectName("transportLineName");name->setMaxLength(64);form->addRow("Name",name);
    auto* source=combo("transportLineSource","Load at");auto* destination=combo("transportLineDestination","Unload at");
    auto* cargo=combo("transportLineCargo","Mineral");cargo->addItems({"Ironium","Boranium","Germanium"});
    auto* warp=new QSpinBox(body);warp->setObjectName("transportLineWarp");warp->setRange(1,10);warp->setValue(5);form->addRow("Warp",warp);
    auto* reserve=new QSpinBox(body);reserve->setObjectName("transportLineReserve");reserve->setRange(0,1000000000);form->addRow("Leave at source, kt",reserve);
    auto* minimum=new QSpinBox(body);minimum->setObjectName("transportLineMinimum");minimum->setRange(0,1000000000);form->addRow("Depart with at least, kt (0 = no wait)",minimum);
    auto* target=new QSpinBox(body);target->setObjectName("transportLineTarget");target->setRange(0,1000000000);form->addRow("Destination stock target, kt (0 = unload all)",target);
    target->setToolTip("Top up the owned destination to this stock on arrival. Keep surplus aboard and retry annually as stocks are used; return after the selected cargo is empty. Target changes and Stop use normal command delay.");
    auto* save=button("transportLineSave","Create / update line and dispatch to its roster");
    auto* pause=button("transportLinePause","Pause / resume line (delayed fleet commands)");
    auto* remove=button("transportLineDelete","Delete line and send Stop to its roster");
    auto* fleets=combo("transportLineFleet","Received cargo fleet");
    auto* assign=button("transportLineAssign","Assign selected fleet and send programme");
    auto* release=button("transportLineRelease","Release selected fleet and send Stop");
    auto* status=new QLabel(body);status->setObjectName("transportLineStatus");status->setWordWrap(true);layout->addWidget(status);
    auto* metrics=new QLabel(body);metrics->setObjectName("transportLineMetrics");metrics->setWordWrap(true);layout->addWidget(metrics);
    auto* receipts=new QListWidget(body);receipts->setObjectName("transportLineReceipts");layout->addWidget(receipts);
    auto* roster=new QListWidget(body);roster->setObjectName("transportLineRoster");layout->addWidget(roster);
    auto* overlay=new QCheckBox("Show registered transport lines on map",body);overlay->setChecked(true);layout->addWidget(overlay);
    auto* scroll=new QScrollArea(dock);scroll->setWidgetResizable(true);scroll->setWidget(body);dock->setWidget(scroll);addDockWidget(Qt::RightDockWidgetArea,dock);dock->hide();menuBar()->addAction(dock->toggleViewAction());
    const auto received=[this]{return make_player_view(state_,pendingOrders_.player);};
    const auto intent=[this,received] {
        auto view=received();
        for(const auto& order:pendingOrders_.orders) {
            if(const auto fleet=transport_manual_override(order))release_transport_assignment(view.state,view.player,*fleet);
            if(const auto* o=std::get_if<SetTransportLineOrder>(&order))(void)set_transport_line(view.state,view.player,*o);
            else if(const auto* o=std::get_if<AssignTransportLineOrder>(&order))(void)assign_transport_line(view.state,view.player,*o);
        }
        return view.state.players.front();
    };
    const auto selected=[=]()->std::optional<TransportLine> {
        const auto p=intent();for(const auto& line:p.transportLines)if(line.spec.id==lines->currentData().toUInt())return line;
        return {};
    };
    const auto refresh=[=,this] {
        const auto view=received();const auto p=intent();const auto id=lines->currentData().toUInt();
        {const QSignalBlocker block(lines);lines->clear();lines->addItem("New line",0u);
            for(const auto& l:p.transportLines)lines->addItem(QString("#%1 %2%3").arg(l.spec.id).arg(QString::fromStdString(l.spec.name),l.spec.active?"":" (paused)"),l.spec.id);
            const auto at=lines->findData(id);lines->setCurrentIndex(at<0?0:at);}
        const auto line=selected();
        auto fill=[&](QComboBox* c,auto populate){const auto old=c->currentData();const QSignalBlocker block(c);c->clear();populate();const auto i=c->findData(old);if(i>=0)c->setCurrentIndex(i);};
        fill(source,[&]{for(const auto& planet:view.state.planets)if((planet.owner==view.player || (!planet.owner && survey_level(view.state,view.player,planet.star)>=SurveyLevel::GeologicalSurvey)))source->addItem(QString::fromStdString(planet.name),planet.id);});
        fill(destination,[&]{
            for(const auto& planet:view.state.planets)if(planet.owner==view.player)destination->addItem(QString::fromStdString(planet.name),planet.id);
            for(const auto& f:view.state.fleets)if(f.owner==view.player && f.outpost!=OutpostKind::None)
                destination->addItem(QString("%1 #%2 — %3").arg(QString::fromStdString(outpost_kind_name(f.outpost))).arg(f.id)
                    .arg(QString::fromStdString(f.name)),QVariant::fromValue(-qlonglong(f.id)));
        });
        fill(fleets,[&]{for(const auto& f:view.state.fleets)if(f.owner==view.player && f.outpost==OutpostKind::None && fleet_cargo_capacity(view.state,f)>0)fleets->addItem(QString("#%1 %2").arg(f.id).arg(QString::fromStdString(f.name)),f.id);
            if(line)for(const auto& a:line->assignments)if(fleets->findData(a.fleet)<0)fleets->addItem(QString("Roster #%1 — no received contact").arg(a.fleet),a.fleet);
        });
        roster->clear();pause->setEnabled(bool(line));remove->setEnabled(bool(line));assign->setEnabled(bool(line)&&fleets->count());release->setEnabled(bool(line)&&fleets->count());
        metrics->clear();receipts->clear();
        if(!line){status->setText("New owner-local line. Definitions and roster changes below include local draft intent; fleet status always uses received telemetry.");return;}
        name->setText(QString::fromStdString(line->spec.name));warp->setValue(line->spec.warp);cargo->setCurrentIndex(int(line->spec.cargo));
        reserve->setValue(line->spec.sourceReserve);minimum->setValue(line->spec.departureMinimum);
        target->setValue(line->spec.destinationTarget);
        auto endpoint=[&](QComboBox* c,PlanetId id){auto i=c->findData(id);if(i<0){c->addItem(QString("Last defined planet #%1 (currently unavailable)").arg(id),id);i=c->count()-1;}c->setCurrentIndex(i);};
        endpoint(source,line->spec.source);
        if(line->spec.destinationFleet) {
            const auto key=QVariant::fromValue(-qlonglong(line->spec.destinationFleet));auto i=destination->findData(key);
            if(i<0){destination->addItem(QString("Installation #%1 (no received contact)").arg(line->spec.destinationFleet),key);i=destination->count()-1;}
            destination->setCurrentIndex(i);
        } else endpoint(destination,line->spec.destination);
        status->setText(QString("HQ definition v%1: %2. Roster is received + local intent, not acknowledgement.\n%3\n"
            "Statistics below use delivered line receipts; older programmes need an explicit edit/reassignment for attribution.")
            .arg(qulonglong(line->spec.revision)).arg(line->spec.active?"active":"Stop requested")
            .arg(QString::fromStdString(transport_line_error(view,line->spec))));
        const auto m=transport_line_metrics(view,*line);
        const auto years=m.endTurn>=m.startTurn?m.endTurn-m.startTurn+1:0;
        QString text=QString("Received deliveries, receipt years %1–%2: %3 unloads; I/B/G %4 / %5 / %6 kt.\n"
            "Received mean: %7 kt/year. Reports still in transit are absent; no receipt does not prove no deliveries.\n"
            "Calculated flight-only round trip at Warp %8: at least %9 years; loading, waiting, service and fuel limits add time.\n")
            .arg(qulonglong(m.startTurn)).arg(qulonglong(m.endTurn)).arg(qulonglong(m.deliveries))
            .arg(m.delivered.ironium).arg(m.delivered.boranium).arg(m.delivered.germanium)
            .arg(years?QString::number(mineral_cargo_mass(m.delivered)/years,'f',1):"unavailable")
            .arg(line->spec.warp).arg(m.nominalRoundTripYears);
        if(line->spec.destinationTarget)text+=QString("HQ destination target: %1 kt. Surplus waits aboard; future consumption is not promised. Target limits this unloader, not all colony supplies.\n").arg(line->spec.destinationTarget);
        if(m.contacts)text+=QString("Received programme snapshots: %1 contacts; %2 en-route programmes, %3 loading/waiting; shared hold %4/%5 kt (%6%). "
            "Telemetry years %7–%8 (ages %9–%10). This is reported hold occupancy, not time utilization.\n")
            .arg(qulonglong(m.contacts)).arg(qulonglong(m.moving)).arg(qulonglong(m.waiting)).arg(m.cargoUsed).arg(m.cargoCapacity)
            .arg(m.cargoCapacity?100*m.cargoUsed/m.cargoCapacity:0,0,'f',1)
            .arg(qulonglong(m.oldestTelemetry)).arg(qulonglong(m.newestTelemetry))
            .arg(qulonglong(view.state.turn-m.newestTelemetry)).arg(qulonglong(view.state.turn-m.oldestTelemetry));
        else text+="Reported hold occupancy unavailable: no post-dispatch matching programme telemetry.\n";
        text+=QString("Unconfirmed/paused/missing roster entries: %1.\n").arg(qulonglong(m.unconfirmed));
        const auto& evidence=view.state.players.front();
        for(const auto endpoint:{line->spec.source,line->spec.destination}) {
            const auto* planet=find_planet(view.state,endpoint);if(!planet)continue;
            const auto k=std::find_if(evidence.surveyKnowledge.begin(),evidence.surveyKnowledge.end(),[&](const auto& k){return k.star==planet->star;});
            if(k!=evidence.surveyKnowledge.end())text+=QString("%1 evidence: year %2, age %3; %4\n")
                .arg(endpoint==line->spec.source?"Source":"Destination").arg(qulonglong(k->observedTurn)).arg(qulonglong(view.state.turn-k->observedTurn))
                .arg(planet->owner==view.player?QString("received I/B/G %1 / %2 / %3 kt").arg(planet->minerals.ironium).arg(planet->minerals.boranium).arg(planet->minerals.germanium)
                    :"no owned surface-stock telemetry; cache/ownership is checked physically on arrival");
        }
        if(line->spec.destinationFleet) {
            const auto* f=find_service_fleet(view.state,line->spec.destinationFleet);
            if(f)text+=QString("Installation #%1: received year %2, I/B/G %3 / %4 / %5 kt; shared hold %6/%7 kt. Arrival checks actual ownership and free capacity.\n")
                .arg(f->id).arg(qulonglong(f->telemetry.observedTurn)).arg(f->minerals.ironium).arg(f->minerals.boranium).arg(f->minerals.germanium)
                .arg(fleet_cargo_used(view.state,*f)).arg(fleet_cargo_capacity(view.state,*f));
        }
        if(evidence.transportReceiptsTruncatedThrough)
            text+=QString("Receipt storage limit omits observations through year %1; displayed volumes are a retained lower bound.\n").arg(qulonglong(evidence.transportReceiptsTruncatedThrough));
        metrics->setText(text);
        for(auto i=evidence.transportReceipts.rbegin();i!=evidence.transportReceipts.rend() && receipts->count()<8;++i)
            if(i->line==line->spec.id && i->receivedTurn>=m.startTurn)receipts->addItem(QString("Observed %1; received %2: fleet #%3 → %4, programme v%5, I/B/G %6 / %7 / %8 kt")
                .arg(qulonglong(i->observedTurn)).arg(qulonglong(i->receivedTurn)).arg(i->fleet).arg(i->destinationFleet?QString("installation #%1").arg(i->destinationFleet):QString("planet #%1").arg(i->planet)).arg(qulonglong(i->revision))
                .arg(i->minerals.ironium).arg(i->minerals.boranium).arg(i->minerals.germanium));
        for(const auto& a:line->assignments) {
            const auto* f=find_service_fleet(view.state,a.fleet);
            QString text=QString("Fleet #%1: %2").arg(a.fleet).arg(QString::fromStdString(transport_assignment_status(view,*line,a)));
            if(f)text+=QString("\nTelemetry year %1, age %2; last target (%3, %4); fuel %5; hold %6/%7 kt; I/B/G %8/%9/%10")
                .arg(qulonglong(f->telemetry.observedTurn)).arg(qulonglong(fleet_telemetry_age(view.state,*f)))
                .arg(f->telemetry.destination?f->telemetry.destination->x:f->telemetry.position.x).arg(f->telemetry.destination?f->telemetry.destination->y:f->telemetry.position.y)
                .arg(f->fuel).arg(fleet_cargo_used(view.state,*f)).arg(fleet_cargo_capacity(view.state,*f))
                .arg(f->minerals.ironium).arg(f->minerals.boranium).arg(f->minerals.germanium);
            roster->addItem(text);
        }
    };
    const auto send=[this,refresh](Order order,const QString& text){appendPendingOrder(std::move(order),text);rebuildScene();refresh();};
    connect(lines,&QComboBox::currentIndexChanged,this,[=](int){refresh();});
    connect(save,&QPushButton::clicked,this,[=,this]{
        const auto old=selected();const auto p=intent();TransportLineSpec d=old?old->spec:TransportLineSpec{};
        if(old && d.revision==std::numeric_limits<std::uint64_t>::max())return;
        if(!old && p.lastTransportLineId==std::numeric_limits<TransportLineId>::max())return;
        if(!old && p.transportLines.size()>=kMaximumTransportLines){status->setText("Maximum 128 lines per empire");return;}
        d.id=old?d.id:p.lastTransportLineId+1;d.revision=old?d.revision+1:1;
        d.name=name->text().trimmed().toStdString();d.source=source->currentData().toUInt();
        const auto endpoint=destination->currentData().toLongLong();
        d.destination=endpoint>0?PlanetId(endpoint):0;d.destinationFleet=endpoint<0?FleetId(-endpoint):0;
        d.warp=warp->value();d.cargo=TransportCargo(cargo->currentIndex());
        d.sourceReserve=reserve->value();d.departureMinimum=minimum->value();
        d.destinationTarget=target->value();
        const auto error=transport_line_error(received(),d);if(!error.empty()){status->setText(QString::fromStdString(error));return;}
        send(SetTransportLineOrder{d},"Define transport line "+QString::fromStdString(d.name));lines->setCurrentIndex(lines->findData(d.id));
    });
    connect(pause,&QPushButton::clicked,this,[=]{if(auto l=selected();l && l->spec.revision<std::numeric_limits<std::uint64_t>::max()) {auto d=l->spec;++d.revision;d.active=!d.active;send(SetTransportLineOrder{d},"Pause / resume transport line");}});
    connect(remove,&QPushButton::clicked,this,[=]{if(auto l=selected();l && l->spec.revision<std::numeric_limits<std::uint64_t>::max()){auto d=l->spec;++d.revision;send(SetTransportLineOrder{d,true},"Delete transport line and send Stop");}});
    connect(assign,&QPushButton::clicked,this,[=]{if(const auto l=selected())send(AssignTransportLineOrder{l->spec.id,fleets->currentData().toUInt()},"Assign transport to line");});
    connect(release,&QPushButton::clicked,this,[=]{if(const auto l=selected())send(AssignTransportLineOrder{l->spec.id,fleets->currentData().toUInt(),true},"Release transport and send Stop");});
    connect(overlay,&QCheckBox::toggled,this,[this](bool on){setProperty("hideTransportLines",!on);rebuildScene();});
    connect(this,&MainWindow::routeProgramContextChanged,this,[=]{if(dock->isVisible())refresh();});
    connect(dock,&QDockWidget::visibilityChanged,this,[=](bool visible){if(visible)refresh();});refresh();
}
}
