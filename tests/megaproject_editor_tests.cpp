#include "main_window.hpp"
#include "save_game.hpp"
#include "megaproject_fixture.hpp"
#include "suns/communications.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <cassert>
using namespace suns;using E=PlanetaryEngineeringKind;using A=EngineeringAction;
namespace {
QByteArray bytes(const QString& path){QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
void raw(const QString& path,const QByteArray& data){QFile f(path);assert(f.open(QIODevice::WriteOnly) && f.write(data)==data.size());}
SaveGameData host(GameState state)
{
    SaveGameData h;h.state=std::move(state);h.mode=SessionMode::Host;h.campaignId=56;h.turnToken=17;
    h.playerTokens={{1,17}};h.pendingOrders={1,{}};return h;
}
void physical_received_inflight_and_private_shelters_remain_separate()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::RadicalTransformation);
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)}}});
    s=TurnProcessor{}.process(s,{});set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    const auto received=s.players.front().surveyKnowledge;
    s=TurnProcessor{}.process(s,{});
    PendingSurveyReport report;report.star=1;report.level=SurveyLevel::DeepSurvey;report.observedOwner=1;
    report.observedTurn=s.turn;report.deliveryTurn=s.turn+4;report.planetSnapshot=capture_planet_survey(s,1,1,report.level,s.turn);
    assert(report.planetSnapshot->engineering.transformation.years==3);
    s=TurnProcessor{}.process(s,{});s.players.front().surveyKnowledge=received;s.players.front().pendingSurveyReports={report};
    assert(s.planets.front().engineering.transformation.years==4);
    auto other=s.players.front();other.id=2;other.name="Other";other.surveyKnowledge.clear();other.pendingSurveyReports.clear();
    s.players.push_back(other);s.planets[1].owner=2;s.planets[1].population=100000;
    set_survey_level(s,2,1,SurveyLevel::OrbitalSurvey,s.turn);
    const auto foreign=std::find_if(s.players.back().surveyKnowledge.begin(),s.players.back().surveyKnowledge.end(),[](const auto& r){return r.star==1;});
    assert(foreign!=s.players.back().surveyKnowledge.end() && foreign->planetSnapshot->engineering.transformation.shelterCapacity==0);
    auto h=host(s);h.playerTokens[2]=19;h.pendingOrders={1,{test::mega_order(1,E::RadicalTransformation,A::Pause)}};h.pendingDescriptions={"Pause transition"};
    h.inbox={{2,{test::mega_order(s.planets[1].id,E::WormholeExperiment,A::StartOrResume)}}};
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto a=dir.filePath("a.sunsturn"),b=dir.filePath("b.sunsturn"),save=dir.filePath("host.suns");
    assert(write_save_game_file(a,make_player_turn(h,1),error));
    h.state.planets.front().engineering.transformation.target.radiation=0;h.state.planets.front().engineering.transformation.paused=true;
    h.state.planets.front().environment.radiation=95;h.state.orbitalStations.clear();refresh_planetary_engineering(h.state);
    assert(write_save_game_file(b,make_player_turn(h,1),error) && bytes(a)==bytes(b));
    assert(write_save_game_file(save,h,error) && read_save_game_file(save,loaded,error));
    assert(loaded.state.planets.front().engineering.transformation.years==4 && loaded.state.planets.front().engineering.transformation.target.radiation==0);
    assert(loaded.state.players.front().surveyKnowledge.front().planetSnapshot->engineering.transformation.years==2);
    assert(loaded.state.players.front().pendingSurveyReports.front().planetSnapshot->engineering.transformation.years==3);
    assert(std::get<SetPlanetaryEngineeringOrder>(loaded.pendingOrders.orders.front()).action==A::Pause);
    assert(std::get<SetPlanetaryEngineeringOrder>(loaded.inbox.front().orders.front()).kind==E::WormholeExperiment);
    h.state.turn=report.deliveryTurn;(void)deliver_due_survey_reports(h.state);
    assert(make_player_view(h.state,1).state.planets.front().engineering.transformation.years==3);
    // A malformed foreign record cannot smuggle the private housing manifest.
    auto bad=h;auto& knowledge=bad.state.players.back().surveyKnowledge;
    const auto k=std::find_if(knowledge.begin(),knowledge.end(),[](const auto& r){return r.star==1;});
    k->planetSnapshot->engineering.transformation.shelterCapacity=1000000;
    assert(!write_save_game_file(save,bad,error));
}
void versioned_orders_and_paused_completed_experiments_roundtrip()
{
    QTemporaryDir dir;QString error;const auto path=dir.filePath("orders.sunsorders"),save=dir.filePath("world.suns");TurnOrderFileData decoded;
    TurnOrderFileData packet{56,1,17,{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation),test::mega_order(1,E::WormholeExperiment,A::ActivateExperiment)}},{"Commit","Activate"}};
    assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,decoded,error));
    assert(std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.back()).action==A::ActivateExperiment);
    auto old=bytes(path);assert(quint8(old[7])==61);old[7]=char(57);raw(path,old);assert(!read_turn_order_file(path,decoded,error));
    auto invalid=packet;std::get<SetPlanetaryEngineeringOrder>(invalid.orders.orders.front()).kind=E::WormholeExperiment;
    assert(!write_turn_order_file(path,invalid,error));
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::RadicalTransformation);
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)}}});
    assert(apply_planetary_engineering_order(s,s.planets.front(),test::mega_order(1,E::RadicalTransformation,A::Pause)));
    auto h=host(s);SaveGameData loaded;assert(write_save_game_file(save,h,error) && read_save_game_file(save,loaded,error));
    assert(loaded.state.planets.front().engineering.transformation.paused && loaded.state.planets.front().engineering.transformation.shelterCapacity==s.planets.front().population);
    old=bytes(save);assert(quint8(old[7])==135);old[7]=char(131);raw(save,old);assert(!read_save_game_file(save,loaded,error));
    h.state.planets.front().engineering.transformation.years=30;assert(!write_save_game_file(save,h,error));
    assert(apply_planetary_engineering_order(s,s.planets.front(),test::mega_order(1,E::RadicalTransformation,A::StartOrResume)));
    while(surface_sheltered(s.planets.front()))s=TurnProcessor{}.process(s,{});
    assert(write_save_game_file(save,host(s),error) && read_save_game_file(save,loaded,error));
    assert(loaded.state.planets.front().engineering.transformation.phase==TransformationPhase::Complete);
    auto experiment=test::megaproject_fixture();test::prepare_megaproject(experiment,E::WormholeExperiment);
    experiment=TurnProcessor{}.process(experiment,{{1,{test::mega_order(1,E::WormholeExperiment,A::ActivateExperiment)}}});
    assert(write_save_game_file(save,host(experiment),error) && read_save_game_file(save,loaded,error));
    assert(loaded.state.wormholes.size()==1 && loaded.state.planets.front().engineering.experiment.entrance==experiment.planets.front().engineering.experiment.entrance);
    auto player=make_player_turn(host(experiment),1);assert(write_save_game_file(save,player,error) && read_save_game_file(save,loaded,error));
    assert(loaded.state.wormholes.empty());
    const auto hiddenExit=experiment.wormholes.front().endpoints.back().id;
    assert(std::none_of(loaded.state.players.front().wormholeKnowledge.begin(),loaded.state.players.front().wormholeKnowledge.end(),[&](const auto& k){return k.endpoint==hiddenExit || k.linkedEndpoint==hiddenExit;}));
    // Unchanged legacy kinds retain their v131/v57 layouts and target bytes.
    auto legacy=test::atmospheric_fixture();test::finish_atmosphere(legacy,0);
    assert(write_save_game_file(save,host(legacy),error));old=bytes(save);old[7]=char(131);raw(save,old);
    assert(read_save_game_file(save,loaded,error) && loaded.state.planets.front().engineering.targetRadiation==0);
    packet.orders.orders={test::atmospheric_order(1,A::SetOutput,5,0)};packet.descriptions={"Old target"};
    assert(write_turn_order_file(path,packet,error));old=bytes(path);old[7]=char(57);raw(path,old);
    assert(read_turn_order_file(path,decoded,error) && std::get<SetPlanetaryEngineeringOrder>(decoded.orders.orders.front()).targetRadiation==0);
}
void answer_confirmation(QMessageBox::StandardButton answer)
{
    QTimer::singleShot(0,[answer]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());assert(box && box->button(answer));box->button(answer)->click();});
}
}
namespace suns {
struct MainWindowTestAccess {
    static void cargo_distinguishes_invasion_from_peaceful_shelter_unloading()
    {
        auto transition = test::megaproject_fixture();
        test::prepare_megaproject(transition, E::RadicalTransformation);
        transition.planets.front().population = 1'000;
        transition = TurnProcessor{}.process(transition,
            {{1, {test::mega_order(1, E::RadicalTransformation, A::CommitTransformation)}}});
        transition.planets.front().population = 600;
        transition.orbitalStations.clear();

        QTemporaryDir dir;
        assert(dir.isValid());
        QString error;
        for (const PlayerId player : {1, 2}) {
            auto state = transition;
            const bool invasion = player == 2;
            if (invasion) {
                state.players.push_back({2, "Attacker"});
                state.planets[1].owner = player;
                state.planets[1].homeworld = true;
                state.planets[1].population = 25'000;
                state.planets[1].radar = PlanetaryRadarLevel::Full;
                state.stars[1].position = {10, 0};
            }
            const auto design = state.nextShipDesignId++;
            state.shipDesigns.push_back({design, player, "Transport", ShipHullType::LargeFreighter,
                {ShipComponentType::QuickJump5, ShipComponentType::QuickJump5}});
            Fleet transport;
            transport.id = state.nextFleetId++;
            transport.owner = player;
            transport.name = "Transport";
            transport.design = design;
            transport.ships = {{design, 1}};
            transport.position = state.stars.front().position;
            transport.colonists = 100'000;
            assert(fleet_cargo_used(state, transport) <= fleet_cargo_capacity(state, transport));
            assert(fleet_has_instant_link(state, transport));
            transport.telemetry = confirmed_fleet_telemetry(state, transport);
            state.fleets.push_back(transport);
            set_survey_level(state, player, 1, SurveyLevel::OrbitalSurvey, state.turn);

            auto envelope = host(state);
            if (invasion) envelope.playerTokens[2] = 19;
            auto packet = make_player_turn(envelope, player);
            packet.selectedStar = 1;
            packet.selectedFleet = transport.id;
            const auto turnPath = dir.filePath("cargo.sunsturn");
            assert(write_save_game_file(turnPath, packet, error));
            MainWindow window;
            window.installUiPolish();
            assert(window.loadGameFromPath(turnPath));
            const auto& received = window.state_.planets.front();
            assert(surface_sheltered(received));
            assert(received.engineering.transformation.shelterCapacity == (invasion ? 0 : 1'000));
            const int expected = invasion ? 100'000 : 400;

            QTimer::singleShot(0, [&] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                assert(dialog);
                auto* source = dialog->findChild<QComboBox*>("cargoSourceCombo");
                auto* destination = dialog->findChild<QComboBox*>("cargoDestinationCombo");
                auto* people = dialog->findChild<QSpinBox*>("cargoColonistSpin");
                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                assert(source && destination && people && buttons);
                source->setCurrentIndex(source->findData(1)); // The transport is the second endpoint.
                destination->setCurrentIndex(destination->findData(0));
                assert(people->maximum() == expected);
                people->setValue(expected);
                auto* accept = buttons->button(QDialogButtonBox::Ok);
                assert(accept->text() == (invasion ? "Queue invasion" : "Apply transfer"));
                assert(accept->isEnabled());
                accept->click();
            });
            assert(window.selectedFleetCargoLocation());
            window.openCargoManifestDialog();
            assert(window.pendingOrders_.orders.size() == 1);
            const auto& transfer = std::get<TransferCargoOrder>(window.pendingOrders_.orders.front());
            assert(transfer.source.fleet == transport.id && transfer.destination.planet == 1);
            assert(transfer.colonists == std::uint64_t(expected));

            const auto ordersPath = dir.filePath("cargo.sunsorders");
            TurnOrderFileData decoded;
            assert(write_turn_order_file(ordersPath,
                {envelope.campaignId, state.turn, envelope.playerTokens.at(player),
                    window.pendingOrders_, window.pendingDescriptions_}, error));
            assert(read_turn_order_file(ordersPath, decoded, error));
            const auto resolved = TurnProcessor{}.process(state, {decoded.orders});
            assert(resolved.planets.front().owner == player);
            assert(resolved.planets.front().population == 1'000);
            assert(resolved.fleets.size() == 1);
            const auto aboard = resolved.fleets.front().colonists;
            if (invasion) assert(aboard > 0 && aboard <= 99'000);
            else assert(aboard == 99'600);
        }
    }

    static void capture(MainWindow& w,const QString& suffix={})
    {
        const auto path=qEnvironmentVariable("SUNS_MEGAPROJECT_SCREENSHOT");if(path.isEmpty())return;
        auto* dock=w.findChild<QDockWidget*>("planetaryEngineeringDock");dock->setFloating(true);dock->resize(1000,1050);dock->show();
        QApplication::processEvents();assert(dock->grab().save(path+suffix));
    }
    static void load(MainWindow& w,GameState state)
    {set_survey_level(state,1,1,SurveyLevel::DeepSurvey,state.turn);w.state_=std::move(state);w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.star=1;w.rebuildScene();QApplication::processEvents();}
    static void explicit_ui_orders_pass_through_pbem_and_hidden_updates_wait_for_reports(MainWindow& w)
    {
        auto fresh=test::megaproject_fixture();load(w,fresh);
        auto* kind=w.findChild<QComboBox*>("engineeringKind");auto* start=w.findChild<QPushButton*>("engineeringStart");
        auto* commit=w.findChild<QPushButton*>("engineeringCommit");auto* activate=w.findChild<QPushButton*>("engineeringExperiment");
        auto* status=w.findChild<QLabel*>("engineeringStatus");auto* effect=w.findChild<QLabel*>("engineeringEffect");
        assert(kind && commit && activate && start && status && effect);
        kind->setCurrentIndex(kind->findData(int(E::RadicalTransformation)));assert(start->isEnabled() && !commit->isEnabled());
        start->click();assert(w.pendingOrders_.orders.size()==1 && std::get<SetPlanetaryEngineeringOrder>(w.pendingOrders_.orders.front()).action==A::StartOrResume);
        assert(!surface_sheltered(w.state_.planets.front()) && !commit->isEnabled());
        auto prepared=test::megaproject_fixture();test::prepare_megaproject(prepared,E::RadicalTransformation);load(w,prepared);
        assert(commit->isEnabled());answer_confirmation(QMessageBox::No);commit->click();assert(w.pendingOrders_.orders.empty());
        answer_confirmation(QMessageBox::Yes);commit->click();assert(w.pendingOrders_.orders.size()==1);
        assert(std::get<SetPlanetaryEngineeringOrder>(w.pendingOrders_.orders.front()).action==A::CommitTransformation);
        assert(!surface_sheltered(w.state_.planets.front()) && !w.findChild<QPushButton*>("engineeringDismantle")->isEnabled());
        QTemporaryDir dir;QString error;TurnOrderFileData decoded;const auto path=dir.filePath("actual.sunsorders");
        assert(write_turn_order_file(path,{56,w.state_.turn,17,w.pendingOrders_,w.pendingDescriptions_},error) && read_turn_order_file(path,decoded,error));
        auto actual=TurnProcessor{}.process(w.state_,{decoded.orders});load(w,actual);
        assert(surface_sheltered(actual.planets.front()) && actual.planets.front().engineering.transformation.years==1);
        assert(status->text().contains("1 / 30") && w.findChild<QLabel*>("planetTerraformingPotential")->text().contains("Residents sheltered"));
        capture(w,".radical.png");
        const auto before=status->text(),preview=effect->text();
        w.state_.planets.front().engineering.transformation.target.radiation=0;w.state_.planets.front().environment.radiation=95;
        w.state_.orbitalStations.clear();refresh_planetary_engineering(w.state_);w.rebuildScene();QApplication::processEvents();
        assert(status->text()==before && effect->text()==preview);
        ++w.state_.turn;set_survey_level(w.state_,1,1,SurveyLevel::DeepSurvey,w.state_.turn);w.rebuildScene();QApplication::processEvents();
        assert(status->text()!=before && effect->text()!=preview);
        auto foreign=actual;auto other=foreign.players.front();other.id=2;other.name="Captor";foreign.players.push_back(other);
        foreign.planets.front().owner=2;foreign.orbitalStations.front().owner=2;load(w,foreign);
        assert(!commit->isEnabled() && !status->text().contains("Shelter capacity"));
        auto experiment=test::megaproject_fixture();test::prepare_megaproject(experiment,E::WormholeExperiment);load(w,experiment);
        assert(activate->isEnabled() && w.state_.wormholes.empty());
        answer_confirmation(QMessageBox::Yes);activate->click();assert(w.state_.wormholes.empty());
        assert(write_turn_order_file(path,{56,w.state_.turn,17,w.pendingOrders_,w.pendingDescriptions_},error) && read_turn_order_file(path,decoded,error));
        actual=TurnProcessor{}.process(w.state_,{decoded.orders});load(w,actual);
        assert(actual.wormholes.size()==1 && !activate->isEnabled() && status->text().contains("spent"));
        capture(w);
    }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    physical_received_inflight_and_private_shelters_remain_separate();
    versioned_orders_and_paused_completed_experiments_roundtrip();
    MainWindowTestAccess::cargo_distinguishes_invasion_from_peaceful_shelter_unloading();
    MainWindow w;w.installUiPolish();w.installPlanetaryEngineering();
    w.findChild<QDockWidget*>("planetaryEngineeringDock")->show();
    MainWindowTestAccess::explicit_ui_orders_pass_through_pbem_and_hidden_updates_wait_for_reports(w);
}
