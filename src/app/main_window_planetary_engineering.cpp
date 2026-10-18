#include "main_window.hpp"
#include "suns/campaign.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/production.hpp"
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

namespace suns {
void MainWindow::installPlanetaryEngineering()
{
    if(findChild<QDockWidget*>("planetaryEngineeringDock")) return;
    auto* dock=new QDockWidget("Planetary engineering",this); dock->setObjectName("planetaryEngineeringDock");
    auto* body=new QWidget(dock); auto* layout=new QVBoxLayout(body);
    auto* hint=new QLabel("Orbital mirrors warm a world; orbital shades cool it. Energy 14 and Construction 14 unlock "
        "a 2,400-production project: at most 80 work/year, so at least 30 years. Materials are paid as work happens "
        "(total 240 I / 120 B / 240 G kt). Construction and operation need a populated colony and a friendly "
        "Starbase or Fortress with a shipyard. Queue priority, research allocation and supplies can extend the schedule.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* planets=new QComboBox(body); planets->setObjectName("engineeringPlanet"); layout->addWidget(planets);
    auto* project=new QComboBox(body); project->setObjectName("engineeringKind");
    project->addItem("Orbital mirrors (+temperature)",int(PlanetaryEngineeringKind::OrbitalMirrors));
    project->addItem("Orbital shades (−temperature)",int(PlanetaryEngineeringKind::OrbitalShades));
    layout->addWidget(project);
    auto* output=new QSpinBox(body); output->setObjectName("engineeringOutput"); output->setRange(0,kEngineeringStrength); output->setValue(kEngineeringStrength);
    auto* form=new QFormLayout; form->addRow("Temperature offset (0 = off)",output); layout->addLayout(form);
    auto* progress=new QProgressBar(body); progress->setObjectName("engineeringProgress"); progress->setRange(0,kEngineeringWork); layout->addWidget(progress);
    auto* status=new QLabel(body); status->setObjectName("engineeringStatus"); status->setWordWrap(true); layout->addWidget(status);
    auto* effect=new QLabel(body); effect->setObjectName("engineeringEffect"); effect->setWordWrap(true); layout->addWidget(effect);
    auto* row=new QHBoxLayout;
    const auto button=[&](const char* title,const char* name) { auto* b=new QPushButton(title,body); b->setObjectName(name); row->addWidget(b); return b; };
    auto* start=button("Start / resume","engineeringStart"); auto* pause=button("Pause construction","engineeringPause");
    auto* apply=button("Set output","engineeringApply"); layout->addLayout(row);
    auto* dismantle=new QPushButton("Dismantle project (no refund)",body); dismantle->setObjectName("engineeringDismantle"); layout->addWidget(dismantle);
    auto* note=new QLabel("Pausing or removing the production-queue item retains all work and paid materials. "
        "Capture also preserves the installation. Losing its supporting station suspends construction and its temperature effect. "
        "Ordinary terraforming changes the planet separately. Foreign projects show dated orbital-survey evidence only.",body);
    note->setWordWrap(true); layout->addWidget(note); layout->addStretch();
    auto* scroll=new QScrollArea(dock); scroll->setWidgetResizable(true); scroll->setWidget(body); dock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea,dock); dock->hide(); menuBar()->addAction(dock->toggleViewAction());
    const auto knownWorld=[this] { return find_player(state_,pendingOrders_.player)
        ? planned_production_state(make_player_view(state_,pendingOrders_.player).state,pendingOrders_):GameState{}; };
    const auto update=[=,this] {
        const auto known=knownWorld(); const auto* p=find_planet(known,planets->currentData().toUInt());
        const bool owned=p && p->owner==pendingOrders_.player;
        const bool existing=p && p->engineering.kind!=PlanetaryEngineeringKind::None;
        const bool complete=existing && p->engineering.work==kEngineeringWork;
        project->setEnabled(owned && !existing); output->setEnabled(owned);
        start->setEnabled(owned && !complete && !engineering_scheduled(*p) && planetary_engineering_available(known,p->owner));
        pause->setEnabled(owned && existing && !complete && engineering_scheduled(*p));
        apply->setEnabled(owned && existing); dismantle->setEnabled(owned && existing);
        progress->setValue(p?int(p->engineering.work):0);
        if(!p) { status->setText("Select a known planet. Orbital survey is required for foreign project details."); effect->clear(); return; }
        if(!owned && survey_level(known,pendingOrders_.player,p->star)<SurveyLevel::OrbitalSurvey) {
            status->setText("No received orbital evidence of this planet's engineering projects.");
            effect->setText("Environmental axes and engineering details require an orbital survey."); return;
        }
        const auto& e=p->engineering;
        QString text=existing?QString("%1 — %2 / %3 work (%4%).\n")
            .arg(QString::fromStdString(engineering_name(e.kind))).arg(e.work).arg(kEngineeringWork).arg(e.work*100/kEngineeringWork):"No existing climate project.\n";
        if(owned) {
            text+=QString::fromStdString(engineering_block_name(engineering_block(known,*p)));
            text+=QString("\nPaid: I %1 / B %2 / G %3 kt. Stocks: I %4 / B %5 / G %6 kt.")
                .arg(e.invested.ironium,0,'f',1).arg(e.invested.boranium,0,'f',1).arg(e.invested.germanium,0,'f',1)
                .arg(p->minerals.ironium,0,'f',1).arg(p->minerals.boranium,0,'f',1).arg(p->minerals.germanium,0,'f',1);
            if(!planetary_engineering_available(known,p->owner) && !complete) text+="\nRequires Energy 14 and Construction 14.";
            if(existing && !complete && engineering_scheduled(*p)) {
                const auto forecast=forecast_production_queue(known,*p,p->productionQueue,128);
                const auto item=std::find_if(p->productionQueue.begin(),p->productionQueue.end(),[](const auto& q){return engineering_kind(q.kind);});
                const auto index=std::size_t(item-p->productionQueue.begin());
                text+=forecast[index].completionTurn?QString("\nEstimated completion: year %1, using current supplies and technology.").arg(qulonglong(*forecast[index].completionTurn))
                    :"\nNo completion within the 128-year forecast with current supplies and technology.";
            }
        } else {
            const auto* owner=find_player(known,pendingOrders_.player);
            const auto intel=owner?std::find_if(owner->surveyKnowledge.begin(),owner->surveyKnowledge.end(),[&](const auto& k){return k.star==p->star;}):std::vector<SystemSurveyKnowledge>::const_iterator{};
            text+=owner && intel!=owner->surveyKnowledge.end()?QString("Orbital evidence observed in year %1. %2")
                .arg(qulonglong(intel->observedTurn)).arg(existing?(e.operational?"Operating at observation.":"Inactive / under construction at observation."):"No visible project at observation.")
                :"No received orbital evidence.";
        }
        status->setText(text);
        if(!owned && survey_level(known,pendingOrders_.player,p->star)<SurveyLevel::OrbitalSurvey) {
            effect->setText("Environmental axes and engineering details require an orbital survey."); return;
        }
        auto projected=*p; projected.observedHabitability.reset();
        projected.engineering.kind=existing?e.kind:PlanetaryEngineeringKind(project->currentData().toInt());
        projected.engineering.work=kEngineeringWork; projected.engineering.strength=owned?std::uint8_t(output->value()):e.strength;
        projected.engineering.operational=projected.engineering.strength>0;
        effect->setText(QString("Temperature now: %1 / 100. With completed, supported output: %2 / 100.\n"
            "Hab now: %3%; with that output: %4%. Forecast assumes current stellar conditions and other environment axes.")
            .arg(engineered_environment(*p).temperature).arg(engineered_environment(projected).temperature)
            .arg(player_planet_habitability(known,pendingOrders_.player,*p,known.turn))
            .arg(player_planet_habitability(known,pendingOrders_.player,projected,known.turn)));
    };
    const auto load=[=] {
        const auto known=knownWorld(); const auto* p=find_planet(known,planets->currentData().toUInt());
        const QSignalBlocker a(output),b(project);
        if(p && p->engineering.kind!=PlanetaryEngineeringKind::None) {
            output->setValue(p->engineering.strength); project->setCurrentIndex(project->findData(int(p->engineering.kind)));
        } else output->setValue(kEngineeringStrength);
        update();
    };
    const auto previousMapStar=std::make_shared<StarId>(0);
    const auto refresh=[=,this] {
        const auto old=planets->currentData().toUInt(); const auto known=knownWorld();
        const QSignalBlocker block(planets); planets->clear();
        PlanetId selected=old;
        const auto* mapStar=selectedStar();
        if(mapStar && mapStar->id!=*previousMapStar) if(const auto* p=find_planet_at_star(known,mapStar->id)) selected=p->id;
        *previousMapStar=mapStar?mapStar->id:0;
        for(const auto& p:known.planets) planets->addItem(QString::fromStdString(p.name)+QString(" (#%1)%2").arg(p.id).arg(p.owner==pendingOrders_.player?"":" — received evidence"),p.id);
        if(const int index=planets->findData(selected);index>=0)planets->setCurrentIndex(index);
        load();
    };
    const auto issue=[=,this](EngineeringAction action) {
        const auto known=knownWorld(); const auto* p=find_planet(known,planets->currentData().toUInt());
        if(!p || p->owner!=pendingOrders_.player) return;
        const auto kind=p->engineering.kind!=PlanetaryEngineeringKind::None?p->engineering.kind:PlanetaryEngineeringKind(project->currentData().toInt());
        appendPendingOrder(SetPlanetaryEngineeringOrder{p->id,kind,action,std::uint8_t(output->value())},
            QString("%1: %2").arg(QString::fromStdString(p->name),action==EngineeringAction::StartOrResume?"start / resume climate project":action==EngineeringAction::Pause?"pause climate construction":action==EngineeringAction::SetOutput?"set climate output":"dismantle climate project"));
        rebuildScene();
    };
    connect(planets,&QComboBox::currentIndexChanged,this,[load](int){load();});
    connect(project,&QComboBox::currentIndexChanged,this,[update](int){update();});
    connect(output,&QSpinBox::valueChanged,this,[update](int){update();});
    connect(start,&QPushButton::clicked,this,[issue]{issue(EngineeringAction::StartOrResume);});
    connect(pause,&QPushButton::clicked,this,[issue]{issue(EngineeringAction::Pause);});
    connect(apply,&QPushButton::clicked,this,[issue]{issue(EngineeringAction::SetOutput);});
    connect(dismantle,&QPushButton::clicked,this,[=,this] {
        if(QMessageBox::question(this,"Dismantle climate project","Remove all construction progress and its temperature effect? Paid production and minerals will not be refunded.")!=QMessageBox::Yes)return;
        issue(EngineeringAction::Dismantle);
    });
    connect(this,&MainWindow::routeProgramContextChanged,this,refresh); refresh();
}
} // namespace suns
