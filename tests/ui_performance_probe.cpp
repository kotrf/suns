// Manual diagnostic, deliberately outside CTest: report timings, never assert a
// machine-dependent latency threshold. Run with QT_QPA_PLATFORM=offscreen too.
#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/computer_player.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w, GalaxyConfig config, unsigned turns)
    {
        std::vector<EmpireSetup> setup;
        for (int i=0;i<8;++i) {
            setup.push_back({"Empire "+std::to_string(i+1),RacePreset::Terran});
            if (i) setup.back().controller = PlayerController::Computer;
        }
        w.state_=generate_campaign(config,setup);w.galaxyConfig_=config;
        for (unsigned i=0;i<turns;++i)
            w.state_=resolve_controlled_campaign_turn(w.state_,{{1,{}}}).state;
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();
        w.selection_.star=w.state_.planets.front().star;w.selection_.fleet=w.state_.fleets.front().id;
    }
    static void redraw(MainWindow& w) {w.rebuildScene();}
    static bool save(MainWindow& w,const QString& path) {return w.saveGameToPath(path);}
    static std::uint64_t turn(const MainWindow& w) { return w.state_.turn; }
    static std::size_t fleets(const MainWindow& w) { return w.state_.fleets.size(); }
};
}

int main(int argc,char** argv)
{
    QApplication app(argc,argv);using namespace suns;
    QCommandLineParser parser;parser.addHelpOption();
    parser.addOption({"systems","Number of systems (2–1000)","count","288"});
    parser.addOption({"idle-ms","Idle event-loop measurement duration","milliseconds","2000"});
    parser.addOption({"turns","Campaign turns to resolve before measuring (0–200)","count","0"});
    parser.process(app);bool validCount{},validDuration{},validTurns{};
    const auto count=parser.value("systems").toUInt(&validCount);
    const auto duration=parser.value("idle-ms").toInt(&validDuration);
    const auto turns=parser.value("turns").toUInt(&validTurns);
    if (!validCount || count<2 || count>kMaximumGalaxySystems || !validDuration || duration<100
        || !validTurns || turns>200) return 2;
    MainWindow w;
    // Same installed workspace as main.cpp, including hidden panel timers.
    w.installDeferredMapSelectionHandler();attachRouteProgramDock(w);
    w.installUiPolish();w.installMapDisplayModes();w.installPlanetPolish();w.installProductionQueue();
    w.installFleetReadabilityPolish();w.installFleetPortraitPolish();w.installMiningInfrastructure();
    w.installCommunicationStatus();w.installTurnMessages();w.installResearch();w.installEmpireHistory();
    w.installWormholes();w.installUnknownObjects();w.installScienceSamples();w.installExoticMaterials();
    w.installSurveyProbes();w.installVisitors();w.installInterstellarMedium();w.installOutposts();
    w.installMobileBases();w.installOrbitalBases();w.installPlanetaryEngineering();
    w.installPanelLayoutFixes();w.installStrategicOperations();
    auto config=galaxy_preset(GalaxySize::Medium,GalaxyDensity::Normal,20261027);config.starCount=count;
    MainWindowTestAccess::setup(w,config,turns);
    const auto now=[] {return std::chrono::steady_clock::now();};
    const auto elapsed=[&](auto start) {return std::chrono::duration<double,std::milli>(now()-start).count();};
    auto start=now();MainWindowTestAccess::redraw(w);const auto redraw=elapsed(start);
    for (int i=0;i<3;++i) app.processEvents();
    QEventLoop idle;QTimer heartbeat;heartbeat.setInterval(20);
    auto last=now();double longestGap=0;
    QObject::connect(&heartbeat,&QTimer::timeout,&idle,[&] {longestGap=std::max(longestGap,elapsed(last));last=now();});
    heartbeat.start();QTimer::singleShot(duration,&idle,&QEventLoop::quit);
    start=now();const auto cpuStart=std::clock();idle.exec();
    const auto idleWall=elapsed(start),idleCpu=1000.0*(std::clock()-cpuStart)/CLOCKS_PER_SEC;
    QTemporaryDir dir;if(!dir.isValid()) return 1;
    const auto path=dir.filePath("probe.suns");start=now();
    if(!MainWindowTestAccess::save(w,path)) return 1;
    const auto save=elapsed(start);SaveGameData loaded;QString error;
    if(!read_save_game_file(path,loaded,error) || loaded.state.stars.size()!=count) {
        std::cerr<<error.toStdString()<<'\n';return 1;
    }
    std::cout<<"systems="<<count<<" turn="<<MainWindowTestAccess::turn(w)
        <<" fleets="<<MainWindowTestAccess::fleets(w)<<" redraw_ms="<<redraw<<" idle_wall_ms="<<idleWall
        <<" idle_cpu_ms="<<idleCpu<<" longest_heartbeat_gap_ms="<<longestGap
        <<" save_ms="<<save<<" save_reload=ok\n";
}
