#include "suns/orbital_habitats.hpp"
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
        "Starbase or Fortress with a shipyard. Queue priority, research allocation and supplies can extend the schedule. Artificial magnetosphere: Energy/Construction 16, radiation reduction 0–10. Adaptive: Energy/Construction 18 plus a delivered completed Compact remnant study, 4,800 work and I/B/G 480/240/480 kt (at least 60 years), radiation reduction 0–20. One engineering installation per planet; switching requires dismantling.",body);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* planets=new QComboBox(body); planets->setObjectName("engineeringPlanet"); layout->addWidget(planets);
    auto* project=new QComboBox(body); project->setObjectName("engineeringKind");
    project->addItem("Orbital mirrors (+temperature)",int(PlanetaryEngineeringKind::OrbitalMirrors));
    project->addItem("Orbital shades (−temperature)",int(PlanetaryEngineeringKind::OrbitalShades));
    project->addItem("Artificial magnetosphere (−radiation)",int(PlanetaryEngineeringKind::ArtificialMagnetosphere));
    project->addItem("Adaptive magnetosphere (−radiation; field study)",int(PlanetaryEngineeringKind::AdaptiveMagnetosphere));
    project->addItem("Orbital habitat (2 million residents)",int(PlanetaryEngineeringKind::OrbitalHabitat));
    layout->addWidget(project);
    auto* output=new QSpinBox(body); output->setObjectName("engineeringOutput"); output->setRange(0,kEngineeringStrength); output->setValue(kEngineeringStrength);
    auto* form=new QFormLayout; form->addRow("Axis adjustment (0 = off)",output); layout->addLayout(form);
    auto* progress=new QProgressBar(body); progress->setObjectName("engineeringProgress"); progress->setRange(0,kEngineeringWork); layout->addWidget(progress);
    auto* status=new QLabel(body); status->setObjectName("engineeringStatus"); status->setWordWrap(true); layout->addWidget(status);
    auto* effect=new QLabel(body); effect->setObjectName("engineeringEffect"); effect->setWordWrap(true); layout->addWidget(effect);
    auto* row=new QHBoxLayout;
    const auto button=[&](const char* title,const char* name) { auto* b=new QPushButton(title,body); b->setObjectName(name); row->addWidget(b); return b; };
    auto* start=button("Start / resume","engineeringStart"); auto* pause=button("Pause construction","engineeringPause");
    auto* apply=button("Set output","engineeringApply"); layout->addLayout(row);
    auto* dismantle=new QPushButton("Dismantle project (no refund)",body); dismantle->setObjectName("engineeringDismantle"); layout->addWidget(dismantle);
    auto* settlers=new QSpinBox(body); settlers->setObjectName("habitatResettlementCount"); settlers->setRange(1,int(kOrbitalPopulationCapacity)); settlers->setSingleStep(1000); settlers->setValue(200000);
    form->addRow("Residents to move",settlers);
    auto* toOrbit=new QPushButton("Move surface residents to orbit",body); toOrbit->setObjectName("habitatToOrbit"); layout->addWidget(toOrbit);
    auto* toSurface=new QPushButton("Move orbital residents to surface",body); toSurface->setObjectName("habitatToSurface"); layout->addWidget(toSurface);
    auto* note=new QLabel("Pausing or removing the production-queue item retains all work and paid materials. "
        "Capture also preserves the installation. Losing its supporting station suspends construction and its environmental effect. "
        "Ordinary terraforming changes the planet separately. Foreign projects show dated orbital-survey evidence only.",body);
    note->setWordWrap(true); layout->addWidget(note); layout->addStretch();
    auto* scroll=new QScrollArea(dock); scroll->setWidgetResizable(true); scroll->setWidget(body); dock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea,dock); dock->hide(); menuBar()->addAction(dock->toggleViewAction());
    const auto knownWorld=[this] { return find_player(state_,pendingOrders_.player)
        ? planned_production_state(projected_cargo_order_state(make_player_view(state_,pendingOrders_.player).state,pendingOrders_),pendingOrders_):GameState{}; };
    const auto update=[=,this] {
        const auto known=knownWorld(); const auto* p=find_planet(known,planets->currentData().toUInt());
        const auto received = find_player(state_,pendingOrders_.player)
            ? make_player_view(state_,pendingOrders_.player).state : GameState{};
        const auto* beforeCargo = p ? find_planet(received,p->id) : nullptr;
        const auto firstTurnMining = beforeCargo ? projected_mineral_mining(received,*beforeCargo) : MineralCargo{};
        const bool owned=p && p->owner==pendingOrders_.player;
        const bool existing=p && p->engineering.kind!=PlanetaryEngineeringKind::None;
        const auto kind=existing?p->engineering.kind:PlanetaryEngineeringKind(project->currentData().toInt());
        const auto total=engineering_work(kind);
        { const QSignalBlocker block(output); output->setMaximum(engineering_max_strength(kind)); }
        const bool complete=existing && p->engineering.work==total;
        progress->setMaximum(int(total));
        project->setEnabled(owned && !existing); output->setEnabled(owned && kind!=PlanetaryEngineeringKind::OrbitalHabitat);
        start->setEnabled(owned && !complete && !engineering_scheduled(*p) && planetary_engineering_available(known,p->owner,kind));
        pause->setEnabled(owned && existing && !complete && engineering_scheduled(*p));
        apply->setEnabled(owned && existing && kind!=PlanetaryEngineeringKind::OrbitalHabitat); dismantle->setEnabled(owned && existing && !p->habitat.population);
        const auto canResettle=[&](bool toOrbital) {
            if(!owned || !habitat_completed(*p)) return false;
            const PlayerOrders order{pendingOrders_.player,{TransferCargoOrder{{p->id,0,!toOrbital},{p->id,0,toOrbital},std::uint64_t(settlers->value()),{}}}};
            const auto preview=projected_cargo_order_state(known,order); const auto* after=find_planet(preview,p->id);
            return after && after->habitat.population!=p->habitat.population;
        };
        toOrbit->setEnabled(canResettle(true)); toSurface->setEnabled(canResettle(false));
        settlers->setEnabled(owned && kind==PlanetaryEngineeringKind::OrbitalHabitat);
        progress->setValue(p?int(p->engineering.work):0);
        if(!p) { status->setText("Select a known planet. Orbital survey is required for foreign project details."); effect->clear(); return; }
        if(!owned && survey_level(known,pendingOrders_.player,p->star)<SurveyLevel::OrbitalSurvey) {
            status->setText("No received orbital evidence of this planet's engineering projects.");
            effect->setText("Environmental axes and engineering details require an orbital survey."); return;
        }
        const auto& e=p->engineering;
        QString text=existing?QString("%1 — %2 / %3 work (%4%).\n")
            .arg(QString::fromStdString(engineering_name(e.kind))).arg(e.work).arg(total).arg(e.work*100/total):"No existing engineering project.\n";
        if(owned) {
            const auto* owner=find_player(known,pendingOrders_.player);
            if(owner) {
                const auto report=std::find_if(owner->surveyKnowledge.begin(),owner->surveyKnowledge.end(),[&](const auto& k){return k.star==p->star;});
                if(report!=owner->surveyKnowledge.end() && report->planetSnapshot)
                    text+=QString("Received baseline: year %1 (age %2). Pending orders and completion/effect estimates are forecasts.\n")
                        .arg(qulonglong(report->observedTurn)).arg(qulonglong(known.turn-std::min(known.turn,report->observedTurn)));
            }
            text+=QString::fromStdString(engineering_block_name(engineering_block(known,*p)));
            text+=QString("\nPaid: I %1 / B %2 / G %3 kt. Stocks: I %4 / B %5 / G %6 kt.")
                .arg(e.invested.ironium,0,'f',1).arg(e.invested.boranium,0,'f',1).arg(e.invested.germanium,0,'f',1)
                .arg(p->minerals.ironium,0,'f',1).arg(p->minerals.boranium,0,'f',1).arg(p->minerals.germanium,0,'f',1);
            if(!planetary_engineering_available(known,p->owner,kind) && !complete) text+=kind==PlanetaryEngineeringKind::OrbitalHabitat?"\nRequires Construction 18, Energy 16 and Biology 14.":kind==PlanetaryEngineeringKind::AdaptiveMagnetosphere?"\nRequires Energy 18, Construction 18 and delivered completed Compact remnant field research.":kind==PlanetaryEngineeringKind::ArtificialMagnetosphere?"\nRequires Energy 16 and Construction 16.":"\nRequires Energy 14 and Construction 14.";
            if(existing && !complete && engineering_scheduled(*p)) {
                const auto forecast=forecast_production_queue(known,*p,p->productionQueue,128,firstTurnMining);
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
        if(kind==PlanetaryEngineeringKind::OrbitalHabitat && owned) {
            const auto potential=std::uint64_t(colony_workforce(known,*p).operatingFactories)+total_colony_population(*p)/kPopulationPerProductionUnit;
            const auto mined=firstTurnMining;
            const MineralCargo supplied{p->minerals.ironium+mined.ironium,p->minerals.boranium+mined.boranium,p->minerals.germanium+mined.germanium};
            const auto cause=!engineering_station_available(known,*p)?"Supporting Starbase/Fortress shipyard unavailable"
                :!engineering_operation_available(known,*p)?"Operating technology unavailable"
                :potential<2?"Insufficient output for mandatory upkeep"
                :!mineral_cargo_sufficient(supplied,{1,1,1})?"Upkeep minerals unavailable after ordinary mining"
                :"Support and upkeep available from received stocks and forecast ordinary mining; next annual payment/recharge is a forecast";
            text+=QString("\nForecast operating condition: %1.").arg(cause);
            const auto* owner=find_player(known,pendingOrders_.player);
            if(owner)for(const auto& record:owner->surveyKnowledge)if(record.star==p->star && record.planetSnapshot && record.planetSnapshot->habitat)
                text+=QString(" Station evidence: year %1 (also incorporates received battle reports).").arg(qulonglong(record.planetSnapshot->habitat->stationEvidenceTime/3));
            text+=QString("\nSurface: %1 / %2 residents. Orbit: %3 / %4 residents; projected growth +%5/year.\n%6. Emergency reserve: %7 years. Last annual loss: %8 residents.\nOccupied upkeep: 2 production + I/B/G 1/1/1 per year, before research. Recharging one reserve year costs the same extra. Empty structures have no upkeep.\n4800 work, at most 80/year; I/B/G 480/240/480. First reserve of three years is included. Evacuate before dismantling; no automatic migration.")
                .arg(qulonglong(p->population)).arg(qulonglong(population_capacity(known,*p,known.turn)))
                .arg(qulonglong(p->habitat.population)).arg(qulonglong(kOrbitalPopulationCapacity)).arg(qlonglong(projected_orbital_growth(known,*p,firstTurnMining)))
                .arg(QString::fromStdString(habitat_service_name(p->habitat.service))).arg(p->habitat.reserveYears).arg(qulonglong(p->habitat.populationLost));
        }
        status->setText(text);
        if(!owned && survey_level(known,pendingOrders_.player,p->star)<SurveyLevel::OrbitalSurvey) {
            effect->setText("Environmental axes and engineering details require an orbital survey."); return;
        }
        if(kind==PlanetaryEngineeringKind::OrbitalHabitat) {
            effect->setText("Orbital housing leaves surface Hab and geology unchanged. Five-percent base orbital growth slows with crowding. Population contributes ordinary colony output only while supported and paid. Station loss spends finite reserves, then kills 10% per year. Cargo and route policies can evacuate all residents even without the station; local surface/orbit shuttles require support. Estimates use received evidence, not future arrivals."); return;
        }
        auto projected=*p; projected.observedHabitability.reset();
        projected.engineering.kind=existing?e.kind:PlanetaryEngineeringKind(project->currentData().toInt());
        projected.engineering.work=total; projected.engineering.strength=owned?std::uint8_t(output->value()):e.strength;
        projected.engineering.operational=projected.engineering.strength>0;
        effect->setText(QString("Temperature now: %1 / 100. With completed, supported output: %2 / 100.\n"
            "Radiation now: %5 / 100. With that output: %6 / 100.\nHab now: %3%; with that output: %4%. Forecast assumes current stellar conditions and other environment axes.")
            .arg(engineered_environment(*p).temperature).arg(engineered_environment(projected).temperature)
            .arg(player_planet_habitability(known,pendingOrders_.player,*p,known.turn))
            .arg(player_planet_habitability(known,pendingOrders_.player,projected,known.turn))
            .arg(engineered_environment(*p).radiation).arg(engineered_environment(projected).radiation));
    };
    const auto load=[=] {
        const auto known=knownWorld(); const auto* p=find_planet(known,planets->currentData().toUInt());
        const QSignalBlocker a(output),b(project);
        if(p && p->engineering.kind!=PlanetaryEngineeringKind::None) {
            output->setMaximum(engineering_max_strength(p->engineering.kind)); output->setValue(p->engineering.strength); project->setCurrentIndex(project->findData(int(p->engineering.kind)));
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
            QString("%1: %2").arg(QString::fromStdString(p->name),action==EngineeringAction::StartOrResume?"start / resume engineering project":action==EngineeringAction::Pause?"pause engineering construction":action==EngineeringAction::SetOutput?"set engineering output":"dismantle engineering project"));
        rebuildScene();
    };
    const auto resettle=[=,this](bool orbital) {
        const auto id=planets->currentData().toUInt();
        appendPendingOrder(TransferCargoOrder{{id,0,!orbital},{id,0,orbital},std::uint64_t(settlers->value()),{}},orbital?"Move residents to orbital habitat":"Move residents to surface");
        rebuildScene();
    };
    connect(toOrbit,&QPushButton::clicked,this,[=]{resettle(true);});
    connect(toSurface,&QPushButton::clicked,this,[=]{resettle(false);});
    connect(settlers,&QSpinBox::valueChanged,this,[=](int){update();});
    connect(planets,&QComboBox::currentIndexChanged,this,[load](int){load();});
    connect(project,&QComboBox::currentIndexChanged,this,[update](int){update();});
    connect(output,&QSpinBox::valueChanged,this,[update](int){update();});
    connect(start,&QPushButton::clicked,this,[issue]{issue(EngineeringAction::StartOrResume);});
    connect(pause,&QPushButton::clicked,this,[issue]{issue(EngineeringAction::Pause);});
    connect(apply,&QPushButton::clicked,this,[issue]{issue(EngineeringAction::SetOutput);});
    connect(dismantle,&QPushButton::clicked,this,[=,this] {
        if(QMessageBox::question(this,"Dismantle engineering project","Remove all construction progress and its environmental effect? Paid production and minerals will not be refunded.")!=QMessageBox::Yes)return;
        issue(EngineeringAction::Dismantle);
    });
    connect(this,&MainWindow::routeProgramContextChanged,this,refresh); refresh();
}
} // namespace suns
