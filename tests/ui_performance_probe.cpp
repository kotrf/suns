// Manual diagnostic, deliberately outside CTest: report timings, never assert a
// machine-dependent latency threshold. Run with QT_QPA_PLATFORM=offscreen too.
#include "main_window.hpp"
#include "route_program_dock.hpp"
#include "save_game.hpp"
#include "suns/computer_player.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QComboBox>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iterator>
#include <iostream>
#include <stdexcept>

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
    static bool load(MainWindow& w,const QString& path) {return w.loadGameFromPath(path);}
    static bool save(MainWindow& w,const QString& path) {return w.saveGameToPath(path);}
    static void research(MainWindow& w) {w.openResearchDialog();}
    static bool selectNextStar(MainWindow& w)
    {
        if (w.state_.stars.empty()) return false;
        const auto current=std::find_if(w.state_.stars.begin(),w.state_.stars.end(),
            [&](const auto& star){return w.selection_.star == star.id;});
        auto next=current==w.state_.stars.end() ? w.state_.stars.begin() : std::next(current);
        if(next==w.state_.stars.end()) next=w.state_.stars.begin();
        if(!w.selectWorkspaceObject(1,next->id)) return false;
        w.queueMapSelectionRebuild();return true;
    }
    static bool selectColony(MainWindow& w)
    {
        const auto colony=std::find_if(w.state_.planets.begin(),w.state_.planets.end(),
            [&](const auto& p){return p.owner==w.pendingOrders_.player;});
        if(colony==w.state_.planets.end() || !w.selectWorkspaceObject(1,colony->star)) return false;
        w.queueMapSelectionRebuild();return true;
    }
    static bool selectNextFleet(MainWindow& w)
    {
        const auto available=w.availableOwnedFleetsForRouteProgram();
        const auto next=std::find_if(available.begin(),available.end(),[&](auto id){return w.selection_.fleet!=id;});
        return next!=available.end() && w.selectFleetForRouteProgram(*next);
    }
    static std::uint64_t turn(const MainWindow& w) {return w.state_.turn;}
    static std::size_t fleets(const MainWindow& w) {return w.state_.fleets.size();}
    static std::size_t systems(const MainWindow& w) {return w.state_.stars.size();}
    static std::size_t orders(const MainWindow& w) {return w.pendingOrders_.orders.size();}
};
}

int main(int argc,char** argv)
{
    QApplication app(argc,argv);using namespace suns;
    QCommandLineParser parser;parser.addHelpOption();
    parser.addOption({"systems","Number of generated systems (2–1000)","count","288"});
    parser.addOption({"idle-ms","Idle event-loop measurement duration","milliseconds","2000"});
    parser.addOption({"turns","Generated campaign turns to resolve (0–200)","count","0"});
    parser.addOption({"save","Read an existing save through a temporary copy; never advance or overwrite it","path"});
    parser.process(app);bool validCount{},validDuration{},validTurns{};
    const auto count=parser.value("systems").toUInt(&validCount);
    const auto duration=parser.value("idle-ms").toInt(&validDuration);
    const auto turns=parser.value("turns").toUInt(&validTurns);
    if(!validDuration || duration<100 || (!parser.isSet("save")
        && (!validCount || count<2 || count>kMaximumGalaxySystems || !validTurns || turns>200))) return 2;
    if(parser.isSet("save") && (parser.isSet("turns") || parser.isSet("systems"))) {
        std::cerr<<"--save cannot be combined with generation options\n";return 2;
    }
    QTemporaryDir dir;if(!dir.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.filePath("settings"));
    QString inputCopy;
    if(parser.isSet("save")) {
        SaveGameData checked;QString error;
        if(!read_save_game_file(parser.value("save"),checked,error)) {
            std::cerr<<error.toStdString()<<'\n';return 1;
        }
        inputCopy=dir.filePath("input.suns");
        if(!QFile::copy(parser.value("save"),inputCopy)) {
            std::cerr<<"Cannot copy input save to the diagnostic temporary directory\n";return 1;
        }
    }
    MainWindow w;
    // Same installed workspace as main.cpp, including hidden panel callbacks.
    w.installDeferredMapSelectionHandler();attachRouteProgramDock(w);
    w.installUiPolish();w.installMapDisplayModes();w.installPlanetPolish();w.installProductionQueue();
    w.installFleetReadabilityPolish();w.installFleetPortraitPolish();w.installMiningInfrastructure();
    w.installCommunicationStatus();w.installTurnMessages();w.installResearch();w.installEmpireHistory();
    w.installWormholes();w.installUnknownObjects();w.installScienceSamples();w.installExoticMaterials();
    w.installSurveyProbes();w.installVisitors();w.installInterstellarMedium();w.installOutposts();
    w.installMobileBases();w.installOrbitalBases();w.installTransportLines();w.installPlanetaryEngineering();
    w.installPanelLayoutFixes();w.installStrategicOperations();
    w.show();
    const auto now=[] {return std::chrono::steady_clock::now();};
    const auto elapsed=[&](auto start) {return std::chrono::duration<double,std::milli>(now()-start).count();};
    const auto drain=[] {
        // Include deferred zero-delay readers/scene notifications in wall time.
        // Report the 20 ms minimum explicitly, never use it as a pass threshold.
        QEventLoop loop;QTimer::singleShot(20,&loop,&QEventLoop::quit);loop.exec();
    };
    const auto waitForecasts=[&](const char* name) {
#ifdef SUNS_DETAILED_UI_PROBE
        const auto start=now();auto previous=now();double gap=0;
        while(w.routeForecastsPending() && elapsed(start)<120000) {
            drain();gap=std::max(gap,elapsed(previous));previous=now();
        }
        std::cout<<"forecast_after="<<name<<" ready="<<!w.routeForecastsPending()
            <<" wait_ms="<<elapsed(start)<<" longest_wait_heartbeat_gap_ms="<<gap<<'\n'<<std::flush;
        if(w.routeForecastsPending())
            throw std::runtime_error("Route forecast timeout; remaining action/idle timings skipped");
#else
        (void)name;
#endif
    };
    const auto measure=[&](const char* name,const auto& action) {
        const auto start=now();const bool performed=action();const auto sync=elapsed(start);
        drain();
        std::cout<<"action="<<name<<" performed="<<performed<<" sync_ms="<<sync
            <<" queued_wall_ms="<<elapsed(start)<<" drain_floor_ms=20\n"<<std::flush;
        waitForecasts(name);
        return performed;
    };
    std::size_t routeCharacters=0;
    const auto readRoute=[&] {
        if(!w.selectedFleetForRouteProgram() || w.selectedFleetRouteProgramRows().empty()) return false;
#ifdef SUNS_DETAILED_UI_PROBE
        routeCharacters=w.selectedFleetRouteProgramForecast().size();
#else
        routeCharacters=w.selectedFleetRouteProgramEstimate().size();
#endif
        return true;
    };
    try {
    if(parser.isSet("save")) {
        if(!measure("load",[&]{return MainWindowTestAccess::load(w,inputCopy);})) return 1;
    } else {
        auto config=galaxy_preset(GalaxySize::Medium,GalaxyDensity::Normal,20261027);config.starCount=count;
        MainWindowTestAccess::setup(w,config,turns);
    }
    const auto startRedraw=now();MainWindowTestAccess::redraw(w);const auto redraw=elapsed(startRedraw);drain();
    waitForecasts("redraw");
    // Give initial layout/paint callbacks the same settling time in both modes;
    // a historical background wait must not make only its idle sample quiet.
    QEventLoop settle;QTimer::singleShot(500,&settle,&QEventLoop::quit);settle.exec();
    std::cout<<"layout_settle_floor_ms=500\n"<<std::flush;
    const auto actualSystems=MainWindowTestAccess::systems(w);
    std::cout<<"source="<<(parser.isSet("save")?"save":"generated")<<" systems="<<actualSystems
        <<" turn="<<MainWindowTestAccess::turn(w)<<" fleets="<<MainWindowTestAccess::fleets(w)
        <<" redraw_ms="<<redraw<<'\n'<<std::flush;

    QEventLoop idle;QTimer heartbeat;heartbeat.setInterval(20);
    auto last=now();double longestGap=0;
    QObject::connect(&heartbeat,&QTimer::timeout,&idle,[&] {longestGap=std::max(longestGap,elapsed(last));last=now();});
    heartbeat.start();QTimer::singleShot(duration,&idle,&QEventLoop::quit);
    auto start=now();const auto cpuStart=std::clock();idle.exec();heartbeat.stop();
    const auto idleWall=elapsed(start),idleCpu=1000.0*(std::clock()-cpuStart)/CLOCKS_PER_SEC;
    std::cout<<"idle_wall_ms="<<idleWall<<" idle_cpu_ms="<<idleCpu
        <<" longest_heartbeat_gap_ms="<<longestGap<<'\n'<<std::flush;

    bool colony=MainWindowTestAccess::selectColony(w);drain();waitForecasts("select_colony_original_fleet");
    const auto addFactory=[&] {
        if(!colony) return false;
        auto* kind=w.findChild<QComboBox*>("productionBatchKind");
        auto* quantity=w.findChild<QSpinBox*>("productionBatchQuantity");
        auto* add=w.findChild<QPushButton*>("productionBatchAdd");
        if(!kind || !quantity || !add) return false;
        kind->setCurrentIndex(kind->findData(int(ProductionKind::Factory)));quantity->setValue(1);
        if(!add->isEnabled()) return false;
        const auto beforeFactory=MainWindowTestAccess::orders(w);
        add->click();return MainWindowTestAccess::orders(w)==beforeFactory+1;
    };
    std::cout<<"factory_original_fleet="<<w.selectedFleetForRouteProgram()<<'\n'<<std::flush;
    measure("route_estimate_original_fleet",readRoute);
    measure("factory_add_original_fleet",addFactory);
    measure("star_select",[&]{return MainWindowTestAccess::selectNextStar(w);});
    measure("fleet_select",[&]{return MainWindowTestAccess::selectNextFleet(w);});
    colony=MainWindowTestAccess::selectColony(w);drain();waitForecasts("select_colony_changed_fleet");
    measure("factory_add",addFactory);
    measure("research_open",[&]{MainWindowTestAccess::research(w);return true;});
    measure("research_allocation",[&] {
        auto* allocation=w.findChild<QSpinBox*>("researchAllocationSpin");
        if(!allocation) return false;
        const auto previous=allocation->value();allocation->setValue(previous>=95?previous-5:previous+5);
        return allocation->value()!=previous;
    });
    if(auto* research=w.findChild<QDialog*>("researchDialog")) research->hide();drain();
    measure("route_estimate_first_read",readRoute);
    measure("route_estimate_repeated_read",readRoute);
    std::cout<<"route_text_characters="<<routeCharacters<<" pending_orders="<<MainWindowTestAccess::orders(w)<<'\n';

    const auto path=dir.filePath("probe.suns");
    if(!measure("save_temp",[&]{return MainWindowTestAccess::save(w,path);})) return 1;
    SaveGameData loaded;QString error;
    if(!measure("readback",[&]{return read_save_game_file(path,loaded,error);})
        || loaded.state.stars.size()!=actualSystems || loaded.state.turn!=MainWindowTestAccess::turn(w)
        || loaded.pendingOrders.orders.size()!=MainWindowTestAccess::orders(w)) {
        std::cerr<<error.toStdString()<<'\n';return 1;
    }
    std::cout<<"save_reload=ok original_save_unchanged="<<parser.isSet("save")<<'\n';
    } catch(const std::exception& error) {
        // Returning unwinds the window and temporary directory safely.
        std::cerr<<error.what()<<'\n';return 1;
    }
}
