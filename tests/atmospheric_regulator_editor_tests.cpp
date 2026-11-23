#include "main_window.hpp"
#include "save_game.hpp"
#include "atmospheric_regulator_fixture.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <cassert>
using namespace suns;using E=PlanetaryEngineeringKind;using A=EngineeringAction;
namespace {
QByteArray bytes(const QString& path){QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
void raw(const QString& path,const QByteArray& data){QFile f(path);assert(f.open(QIODevice::WriteOnly) && f.write(data)==data.size());}
SaveGameData host()
{
    SaveGameData h;h.state=test::atmospheric_fixture();h.mode=SessionMode::Host;h.campaignId=56;h.turnToken=17;
    h.playerTokens={{1,17}};h.pendingOrders={1,{}};return h;
}
void targets_remain_separate_in_physical_received_inflight_draft_and_inbox_records()
{
    auto h=host();auto& s=h.state;auto& p=s.planets.front();
    assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::StartOrResume,10,47)));
    (void)resolve_colony_production(s,p,true);++s.turn;set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    ++s.turn;assert(apply_planetary_engineering_order(s,p,test::atmospheric_order(1,A::SetOutput,8,62)));
    PendingSurveyReport report;report.star=1;report.level=SurveyLevel::DeepSurvey;report.observedTurn=s.turn;report.deliveryTurn=s.turn+4;
    report.observedOwner=1;report.planetSnapshot=capture_planet_survey(s,1,1,report.level,s.turn);
    assert(report.planetSnapshot && report.planetSnapshot->engineering.targetRadiation==62);s.players.front().pendingSurveyReports.push_back(report);
    h.pendingOrders={1,{test::atmospheric_order(1,A::SetOutput,7,44)}};h.pendingDescriptions={"Set target surface radiation"};
    Player mailbox;mailbox.id=2;mailbox.name="Remote player";mailbox.race=s.players.front().race;mailbox.technology=s.players.front().technology;
    s.players.push_back(mailbox);h.playerTokens[2]=19;s.planets[1].owner=2;s.planets[1].population=100000;
    h.inbox={{2,{test::atmospheric_order(s.planets[1].id,A::StartOrResume,6,39)}}};
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("host.suns"),a=dir.filePath("a.sunsturn"),b=dir.filePath("b.sunsturn");
    assert(write_save_game_file(a,make_player_turn(h,1),error));
    p.engineering.targetRadiation=70;(void)resolve_colony_production(s,p,true);s.orbitalStations.clear();refresh_planetary_engineering(s);
    assert(write_save_game_file(b,make_player_turn(h,1),error) && bytes(a)==bytes(b));
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.planets.front().engineering.targetRadiation==70 && loaded.state.planets.front().engineering.targetTemperature==50);
    assert(loaded.state.players.front().surveyKnowledge.front().planetSnapshot->engineering.targetRadiation==47);
    assert(loaded.state.players.front().pendingSurveyReports.back().planetSnapshot->engineering.targetRadiation==62);
    assert(std::get<SetPlanetaryEngineeringOrder>(loaded.pendingOrders.orders.front()).targetRadiation==44);
    assert(std::get<SetPlanetaryEngineeringOrder>(loaded.inbox.front().orders.front()).targetRadiation==39);
    assert(write_save_game_file(b,make_player_turn(loaded,1),error) && read_save_game_file(b,loaded,error));
    assert(loaded.state.planets.front().engineering.targetRadiation==47);
    h.state.turn=report.deliveryTurn;(void)deliver_due_survey_reports(h.state);
    assert(make_player_view(h.state,1).state.planets.front().engineering.targetRadiation==62);
}
void versioned_kinds_and_unrelated_target_bytes_are_strict()
{
    QTemporaryDir dir;QString error;const auto save=dir.filePath("version.suns"),path=dir.filePath("orders.sunsorders");
    SaveGameData loaded;TurnOrderFileData decoded;
    TurnOrderFileData packet{56,1,17,{1,{test::atmospheric_order(1,A::StartOrResume,10,0),test::atmospheric_order(1,A::SetOutput,0,100)}},{"Start","Off"}};
    assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,decoded,error));
    assert(std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.front()).targetRadiation==0);
    assert(std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.back()).targetRadiation==100);
    auto old=bytes(path);assert(quint8(old[7])==61);old[7]=char(56);raw(path,old);assert(!read_turn_order_file(path,decoded,error));
    auto h=host();assert(apply_planetary_engineering_order(h.state,h.state.planets.front(),test::atmospheric_order(1,A::StartOrResume)));
    assert(write_save_game_file(save,h,error));old=bytes(save);assert(quint8(old[7])==135);old[7]=char(130);raw(save,old);
    assert(!read_save_game_file(save,loaded,error));
    for(unsigned invalid=0;invalid<3;++invalid) {
        auto bad=packet;auto& order=std::get<SetPlanetaryEngineeringOrder>(bad.orders.orders.front());
        if(invalid==0)order.targetRadiation=101;if(invalid==1)order.targetTemperature=30;
        if(invalid==2){order.kind=E::ClimateStabilizer;order.targetRadiation=40;}
        assert(!write_turn_order_file(path,bad,error));
    }
    h.state.planets.front().engineering.targetTemperature=30;assert(!write_save_game_file(save,h,error));
    // The old Climate records keep their temperature byte, without a radiation byte.
    h=host();test::finish_climate(h.state,73);assert(write_save_game_file(save,h,error));old=bytes(save);old[7]=char(130);raw(save,old);
    assert(read_save_game_file(save,loaded,error) && loaded.state.planets.front().engineering.targetTemperature==73);
    assert(loaded.state.planets.front().engineering.targetRadiation==50);
    packet.orders.orders={SetPlanetaryEngineeringOrder{1,E::ClimateStabilizer,A::SetOutput,5,0}};packet.descriptions={"Climate target zero"};
    assert(write_turn_order_file(path,packet,error));old=bytes(path);old[7]=char(56);raw(path,old);
    assert(read_turn_order_file(path,decoded,error) && std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.front()).targetTemperature==0);
    assert(std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.front()).targetRadiation==50);
}
}
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& w,GameState s)
    {
        set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);w.state_=std::move(s);
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.star=1;w.rebuildScene();
    }
    static void controls_author_radiation_and_keep_hidden_changes_frozen(MainWindow& w)
    {
        load(w,test::atmospheric_fixture());
        auto* kind=w.findChild<QComboBox*>("engineeringKind");auto* target=w.findChild<QSpinBox*>("engineeringTargetRadiation");
        auto* temp=w.findChild<QSpinBox*>("engineeringTargetTemperature");auto* output=w.findChild<QSpinBox*>("engineeringOutput");
        auto* start=w.findChild<QPushButton*>("engineeringStart");auto* effect=w.findChild<QLabel*>("engineeringEffect");auto* status=w.findChild<QLabel*>("engineeringStatus");
        assert(kind && target && temp && output && start && effect && target->isHidden());
        kind->setCurrentIndex(kind->findData(int(E::AtmosphericRegulator)));assert(!target->isHidden() && temp->isHidden() && start->isEnabled());
        target->setValue(0);output->setValue(6);assert(effect->text().contains("Target surface radiation: 0") && effect->text().contains("6000 work"));
        start->click();const auto initial=std::get<SetPlanetaryEngineeringOrder>(w.pendingOrders_.orders.front());
        assert(initial.targetRadiation==0 && initial.targetTemperature==50 && initial.kind==E::AtmosphericRegulator);
        target->setValue(58);output->setValue(3);w.findChild<QPushButton*>("engineeringApply")->click();
        QTemporaryDir dir;QString error;const auto path=dir.filePath("actual.sunsorders");TurnOrderFileData decoded;
        assert(write_turn_order_file(path,{56,w.state_.turn,17,w.pendingOrders_,w.pendingDescriptions_},error) && read_turn_order_file(path,decoded,error));
        const auto actual=TurnProcessor{}.process(w.state_,{decoded.orders});
        assert(actual.planets.front().engineering.work==80 && actual.planets.front().engineering.targetRadiation==58);
        assert(actual.planets.front().engineering.strength==3 && actual.planets.front().engineering.targetTemperature==50 && actual.planets.front().environment.radiation==40);
        auto complete=test::atmospheric_fixture();test::finish_atmosphere(complete,48);load(w,complete);
        assert(target->value()==48);const auto before=status->text(),preview=effect->text();
        const auto capture=qEnvironmentVariable("SUNS_ATMOSPHERIC_SCREENSHOT");
        if(!capture.isEmpty()) {
            auto* dock=w.findChild<QDockWidget*>("planetaryEngineeringDock");dock->setFloating(true);dock->resize(960,1000);dock->show();
            QApplication::processEvents();assert(dock->grab().save(capture));
        }
        w.state_.planets.front().engineering.targetRadiation=77;w.state_.planets.front().minerals={};w.state_.orbitalStations.clear();
        refresh_planetary_engineering(w.state_);w.rebuildScene();assert(target->value()==48 && status->text()==before && effect->text()==preview);
        ++w.state_.turn;set_survey_level(w.state_,1,1,SurveyLevel::DeepSurvey,w.state_.turn);w.rebuildScene();
        assert(target->value()==77 && status->text().contains("Starbase"));
        auto foreign=complete;auto other=foreign.players.front();other.id=2;other.name="Other";foreign.players.push_back(other);
        foreign.planets.front().owner=2;foreign.orbitalStations.front().owner=2;load(w,foreign);
        assert(target->value()==48 && !target->isEnabled() && !start->isEnabled());
    }
    static void system_details_radiation_matches_received_engineering_and_terraforming()
    {
        MainWindow w;w.installUiPolish();w.installPlanetaryEngineering();
        w.findChild<QDockWidget*>("planetaryEngineeringDock")->show();
        auto s=test::atmospheric_fixture();s.players.front().race.habitableRadiation={50,70};
        test::finish_atmosphere(s,70);load(w,s);QApplication::processEvents();
        auto* radiation=w.findChild<QProgressBar*>("planetRadiationBar");
        auto* potential=w.findChild<QLabel*>("planetTerraformingPotential");
        auto* effect=w.findChild<QLabel*>("engineeringEffect");
        assert(radiation && potential && effect);
        assert(radiation->value()==50);
        assert(radiation->toolTip().contains("Radiation (higher is more severe): 50 / 100"));
        assert(radiation->toolTip().contains("50–70 (inside)"));
        assert(radiation->toolTip().contains("Natural value: 40 / 100"));
        assert(radiation->toolTip().contains("Potential value: 60 / 100"));
        assert(potential->text().contains("Hab 83% → 100%"));
        assert(effect->text().contains("Radiation now: 50 / 100"));
        const auto tooltip=radiation->toolTip(),hab=potential->text();
        auto& hidden=w.state_.planets.front();hidden.environment.radiation=85;
        hidden.engineering.targetRadiation=0;w.state_.orbitalStations.clear();refresh_planetary_engineering(w.state_);
        w.rebuildScene();QApplication::processEvents();
        assert(radiation->value()==50 && radiation->toolTip()==tooltip && potential->text()==hab);
        ++w.state_.turn;set_survey_level(w.state_,1,1,SurveyLevel::DeepSurvey,w.state_.turn);
        w.rebuildScene();QApplication::processEvents();
        assert(radiation->value()==85 && radiation->toolTip().contains("50–70 (outside)"));
        assert(radiation->toolTip().contains("Natural value: 40 / 100"));
        assert(radiation->toolTip().contains("Potential value: 60 / 100"));
        assert(effect->text().contains("Radiation now: 85 / 100"));
        // A target of zero lowers radiation; ordinary terraforming remains visible separately.
        auto lower=s;assert(apply_planetary_engineering_order(lower,lower.planets.front(),test::atmospheric_order(1,A::SetOutput,10,0)));
        load(w,lower);QApplication::processEvents();
        assert(radiation->value()==30 && radiation->toolTip().contains("Potential value: 45 / 100"));
        // Fixed shields use the same surface axis, while inactive/absent projects expose the baseline.
        auto shield=s;auto& e=shield.planets.front().engineering;
        e.kind=E::ArtificialMagnetosphere;e.work=engineering_work(e.kind);e.invested={240,120,240};e.targetRadiation=50;
        load(w,shield);QApplication::processEvents();
        assert(radiation->value()==30 && radiation->toolTip().contains("Potential value: 45 / 100"));
        auto off=s;assert(apply_planetary_engineering_order(off,off.planets.front(),test::atmospheric_order(1,A::SetOutput,0,70)));
        load(w,off);QApplication::processEvents();
        assert(radiation->value()==40 && radiation->toolTip().contains("Potential value: 55 / 100"));
        auto ordinary=s;ordinary.planets.front().engineering={};ordinary.planets.front().environment.radiation=45;
        load(w,ordinary);QApplication::processEvents();
        assert(radiation->value()==45 && radiation->toolTip().contains("Natural value: 40 / 100"));
        assert(radiation->toolTip().contains("Potential value: 55 / 100"));
        auto foreign=s;auto other=foreign.players.front();other.id=2;other.name="Other";foreign.players.push_back(other);
        foreign.planets.front().owner=2;foreign.orbitalStations.front().owner=2;
        load(w,foreign);QApplication::processEvents();
        assert(radiation->value()==50 && radiation->toolTip().contains("Potential value: 60 / 100"));
    }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    targets_remain_separate_in_physical_received_inflight_draft_and_inbox_records();
    versioned_kinds_and_unrelated_target_bytes_are_strict();
    MainWindow w;w.installPlanetaryEngineering();
    w.findChild<QDockWidget*>("planetaryEngineeringDock")->show();
    MainWindowTestAccess::controls_author_radiation_and_keep_hidden_changes_frozen(w);
    MainWindowTestAccess::system_details_radiation_matches_received_engineering_and_terraforming();
}
