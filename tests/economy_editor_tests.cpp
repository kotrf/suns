#include "main_window.hpp"
#include "race_file.hpp"
#include "race_setup_widget.hpp"
#include "galaxy_setup_widget.hpp"
#include "save_game.hpp"
#include "simple_production_estimate.hpp"
#include "suns/communications.hpp"
#include "suns/production.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDataStream>
#include <QDir>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cassert>

using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w) {
        w.state_=make_demo_game();
        auto& p=w.state_.planets.front(); p.industry=4; p.mines=3;
        p.advancedFactories=1; p.advancedMines=1;
        auto& t=w.state_.players.front().technology;
        t.focus=ResearchField::Energy; t.queuedFocuses={ResearchField::Propulsion}; t.researchActive=true;
        w.state_.players.front().race.researchCosts[1]=ResearchCost::Cheap;
        set_survey_level(w.state_,1,1,SurveyLevel::DeepSurvey,w.state_.turn);
        w.pendingOrders_={1,{}}; w.pendingDescriptions_.clear(); w.selection_.star=1;
        w.rebuildScene(); w.refreshProductionQueue(); w.openResearchDialog();
    }
    static const GameState& state(const MainWindow& w) { return w.state_; }
    static const PlayerOrders& orders(const MainWindow& w) { return w.pendingOrders_; }
    static void advance(MainWindow& w) { w.endTurn(); }
};
}
static QByteArray bytes(const QString& path) {
    QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll();
}
static void put(const QString& path,const QByteArray& b) {
    QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(b)==b.size());
}
static void save_and_received_evidence(const QString& path)
{
    SaveGameData host; host.campaignId=1; host.turnToken=22;
    host.playerTokens={{1,11},{2,22}}; host.mode=SessionMode::Host;
    EmpireSetup a{"First"}, b{"Second"}; a.researchCosts[0]=ResearchCost::Cheap;
    GalaxyConfig config; config.slowResearch=true;
    host.state=generate_campaign(config,{a,b}); host.galaxyConfig=config; host.pendingOrders={2,{}};
    auto& p=host.state.planets.front(); assert(p.owner==1);
    p.industry=7; p.mines=5; p.advancedFactories=3; p.advancedMines=2;
    set_survey_level(host.state,1,p.star,SurveyLevel::DeepSurvey,host.state.turn);
    // Undelivered physical upgrades must not leak into the received snapshot.
    p.advancedFactories=5;
    auto publicTurn=make_player_turn(host,1);
    assert(publicTurn.state.planets.front().advancedFactories==3);
    QString error; assert(write_save_game_file(path,publicTurn,error)); const auto original=bytes(path);
    p.advancedFactories=6; host.state.players[1].race.researchCosts.fill(ResearchCost::Expensive);
    assert(write_save_game_file(path,make_player_turn(host,1),error)); assert(bytes(path)==original);
    SaveGameData parsed; assert(read_save_game_file(path,parsed,error));
    assert(parsed.state.planets.front().advancedFactories==3 && parsed.state.planets.front().advancedMines==2);
    assert(parsed.state.slowResearch && parsed.galaxyConfig.slowResearch);
    assert(parsed.state.players.front().race.researchCosts[0]==ResearchCost::Cheap);
    assert(write_save_game_file(path,host,error) && read_save_game_file(path,parsed,error));
    assert(parsed.state.planets.front().advancedFactories==6);
    auto before=TurnProcessor{}.process(host.state,{}), after=TurnProcessor{}.process(parsed.state,{});
    assert(before.players.front().technology.levels==after.players.front().technology.levels);
    assert(before.players.front().technology.progress==after.players.front().technology.progress);
    assert(colony_output(before,before.planets.front())==colony_output(after,after.planets.front()));

    TurnOrderFileData packet{host.campaignId,host.state.turn,11,
        {1,{QueueProductionBatchOrder{p.id,ProductionKind::AdvancedFactory,2},
            QueueProductionBatchOrder{p.id,ProductionKind::UpgradeMine,1}}},{"Advanced factories","Upgrade mine"}};
    assert(write_turn_order_file(path+".orders",packet,error));
    TurnOrderFileData read; assert(read_turn_order_file(path+".orders",read,error));
    assert(validate_turn_submission(host,read).isEmpty());
    const auto actual=TurnProcessor{}.process(host.state,{read.orders});
    assert(actual.planets.front().industry==9 && actual.planets.front().advancedFactories==8);
    assert(actual.planets.front().mines==5 && actual.planets.front().advancedMines==3);

    assert(write_save_game_file(path,host,error)); const auto valid=bytes(path);
    QByteArray marker; {QDataStream out(&marker,QIODevice::WriteOnly);out<<quint32(0x45434f4e);}
    const auto offset=valid.lastIndexOf(marker); assert(offset>0);
    for(int which=0;which<4;++which) {
        auto bad=valid;
        if(which==0) bad.chop(1);
        else if(which==1) bad[offset+4]=char(2);
        else {QDataStream out(&bad,QIODevice::ReadWrite);assert(out.device()->seek(offset+(which==2?13:5)));out<<quint32(0xffffffff);}
        put(path,bad); assert(!read_save_game_file(path,parsed,error));
    }
}
static void genuine_v110_migration(const QString& path)
{
    SaveGameData loaded; QString error;
    assert(read_save_game_file(QString::fromUtf8(SUNS_TEST_FIXTURE_DIR "/economy-v110.suns"),loaded,error));
    const auto& p=loaded.state.planets.front(); const auto& player=loaded.state.players.front();
    assert(p.population==25000 && p.industry==7 && p.mines==3);
    assert(p.advancedFactories==0 && p.advancedMines==0 && !loaded.state.slowResearch);
    assert(player.technology.levels[0]==2 && player.technology.levels[3]==1 && player.technology.progress[5]==17);
    assert(p.productionQueue[0].remainingCost==6 && p.productionQueue[0].quantity==2); // Four resources previously paid.
    assert(p.productionQueue[1].remainingCost==10 && p.productionQueue[1].automation==ProductionAutomation::Annual);
    assert(player.productionTemplates[0].items[0].remainingCost==10);
    assert(production_template_valid(player.productionTemplates[0]));
    assert(player.surveyKnowledge[0].planetSnapshot->habitat->queue[0].remainingCost==6);
    assert(player.history.front().productionOutput==6); // Historical, not recalculated to 1004.
    assert(colony_output(loaded.state,p)==32); // 25 population resources + 7 factories.
    assert(write_save_game_file(path,loaded,error)); SaveGameData roundTrip;
    assert(read_save_game_file(path,roundTrip,error));
    const auto direct=TurnProcessor{}.process(loaded.state,{loaded.pendingOrders});
    const auto reread=TurnProcessor{}.process(roundTrip.state,{roundTrip.pendingOrders});
    assert(direct.planets.front().industry==reread.planets.front().industry);
    assert(direct.planets.front().productionQueue==reread.planets.front().productionQueue);
    assert(direct.players.front().technology.progress==reread.players.front().technology.progress);
}
static void editors_and_simple_estimates(const QString& racePath)
{
    EmpireSetup setup{"Research settings"}; setup.researchCosts={ResearchCost::Cheap,ResearchCost::Normal,ResearchCost::Expensive};
    RaceSetupWidget race; race.setSetup(setup); assert(race.setup().researchCosts==setup.researchCosts);
    QString error; assert(write_race_file(racePath,setup,error)); EmpireSetup read;
    assert(read_race_file(racePath,read,error) && read.researchCosts==setup.researchCosts);
    GalaxyConfig config; GalaxySetupWidget galaxy(config);
    auto* slow=galaxy.findChild<QCheckBox*>("slowResearchCheck"); assert(slow); slow->setChecked(true);
    assert(galaxy.config(1).slowResearch);

    MainWindow window; window.installUiPolish(); window.installProductionQueue(); window.installFleetReadabilityPolish(); MainWindowTestAccess::setup(window);
    auto* kind=window.findChild<QComboBox*>("productionBatchKind");
    auto* count=window.findChild<QSpinBox*>("productionBatchQuantity");
    auto* add=window.findChild<QPushButton*>("productionBatchAdd"); assert(kind && count && add);
    kind->setCurrentIndex(kind->findData(int(ProductionKind::UpgradeFactory)));
    assert(count->maximum()==3 && add->isEnabled()); count->setValue(3); add->click();
    assert(!add->isEnabled());
    const auto& order=std::get<QueueProductionBatchOrder>(MainWindowTestAccess::orders(window).orders.back());
    assert(order.kind==ProductionKind::UpgradeFactory && order.quantity==3);
    auto* plan=window.findChild<QTreeWidget*>("researchPlanTree");
    assert(plan && plan->topLevelItemCount()==2);
    assert(plan->topLevelItem(0)->text(3)=="0 / 50 RP");
    assert(plan->topLevelItem(1)->text(3)=="0 / 30 RP"); // +10 from the preceding breakthrough, then half cost.
    const auto qa=qEnvironmentVariable("SUNS_ECONOMY_QA_DIR");
    if (!qa.isEmpty()) {
        QDir().mkpath(qa); window.resize(1400,900); window.show(); QApplication::processEvents();
        assert(window.findChild<QDialog*>("researchDialog")->grab().save(qa+"/research.png"));
        assert(window.findChild<QDockWidget*>("productionDock")->grab().save(qa+"/production.png"));
        race.resize(720,820); race.show(); QApplication::processEvents();
        assert(race.grab().save(qa+"/race.png"));
    }
    MainWindowTestAccess::advance(window);
    assert(MainWindowTestAccess::state(window).planets.front().advancedFactories==4);

    auto state=make_demo_game(); auto& p=state.planets.front(); p.industry=2; p.advancedFactories=1;
    p.productionQueue={{ProductionKind::UpgradeFactory,10,0,ProductionAutomation::None,3},
        {ProductionKind::AdvancedMine,10},{ProductionKind::UpgradeFactory,10}};
    const auto estimate=simple_production_estimates(state,p,p.productionQueue,20,{100,100,100},true);
    assert(estimate[0].remainingWork==10);
    assert(estimate[0].effectiveQuantity==1 && estimate[0].skippedQuantity==2);
    assert(estimate[0].completionTurn==state.turn+1 && estimate[1].completionTurn==state.turn+1);
    assert(estimate[2].status==SimpleProductionStatus::Skipped);
    state.players.front().technology.researchActive=false;
    (void)resolve_colony_production(state,p,true);
    assert(p.advancedFactories==2 && p.mines==1 && p.advancedMines==1 && p.productionQueue.empty());
}
int main(int argc,char** argv) {
    QApplication app(argc,argv); QTemporaryDir dir; assert(dir.isValid());
    QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,dir.path());
    genuine_v110_migration(dir.filePath("legacy-roundtrip.suns"));
    save_and_received_evidence(dir.filePath("economy.suns"));
    editors_and_simple_estimates(dir.filePath("race.sunsrace"));
}
