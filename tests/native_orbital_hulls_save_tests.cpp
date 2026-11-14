#include "main_window.hpp"
#include "native_orbital_fixture.hpp"
#include "suns/stations.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/production.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>
using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void install(MainWindow& w,GameState state,PlayerOrders orders={1,{}}) {
        w.state_=std::move(state); w.pendingOrders_=std::move(orders);w.pendingDescriptions_.clear();
        for (const auto& order:w.pendingOrders_.orders) { (void)order; w.pendingDescriptions_.push_back("Evacuate resident"); }
        w.selection_.star=w.state_.planets.front().star; w.rebuildScene();
    }
    static void designer(MainWindow& w) { w.openStationDesigner(); }
    static const auto& orders(const MainWindow& w) { return w.pendingOrders_; }
    static void cargo(MainWindow& w,const TransferCargoOrder& order) {
        w.appendPendingOrder(order,"Unload one resident");w.refreshProductionQueue();
    }
    static void queueMiner(MainWindow& w,ShipDesignId design) {
        const auto row=w.shipDesignCombo_->findData(quint32(design));assert(row>=0);
        w.shipDesignCombo_->setCurrentIndex(row);w.queueShipDesign();w.refreshProductionQueue();
    }
    static auto* production(MainWindow& w) { return w.productionQueueTree_; }
    static auto* removeButton(MainWindow& w) { return w.productionRemoveButton_; }
};
}
namespace {
SaveGameData legacy()
{
    SaveGameData h;QString error;
    assert(read_save_game_file(QStringLiteral(SUNS_SOURCE_DIR "/tests/fixtures/native-hulls-v121-host.suns"),h,error));
    return h;
}
void old_files_keep_their_real_paid_generic_hull_and_orders()
{
    QTemporaryDir dir;QString error;SaveGameData loaded;
    for(const auto* kind:{"host","player"}) {
        const auto path=QStringLiteral(SUNS_SOURCE_DIR "/tests/fixtures/native-hulls-v121-")+kind+".suns";
        assert(read_save_game_file(path,loaded,error));
        assert(loaded.state.orbitalStations.front().hull==OrbitalStationHullType::Starbase);
        const auto record=loaded.state.planets.front().nativeOrbit;
        const auto stock=loaded.state.planets.front().minerals;
        assert(record.reserveUnits>0 && record.servicedTurn>1 && record.population>25'000);
        const auto* order=std::get_if<QueueStationOrder>(&loaded.pendingOrders.orders.front());
        assert(order && order->construction.loadout.hull==OrbitalStationHullType::Fortress);
        assert(station_build_cost(order->construction.loadout)==550);
        assert(write_save_game_file(dir.filePath("round.suns"),loaded,error));SaveGameData reopened;
        assert(read_save_game_file(dir.filePath("round.suns"),reopened,error));
        assert(reopened.state.planets.front().nativeOrbit==record && reopened.state.planets.front().minerals.ironium==stock.ironium);
        assert(reopened.state.orbitalStations.front().hull==OrbitalStationHullType::Starbase);
    }
    TurnOrderFileData packet;
    assert(read_turn_order_file(QStringLiteral(SUNS_SOURCE_DIR "/tests/fixtures/native-hulls-v50.sunsorders"),packet,error));
    assert(std::get<QueueStationOrder>(packet.orders.orders.front()).construction.loadout.hull==OrbitalStationHullType::Fortress);
}
void new_fits_survive_all_record_and_order_channels()
{
    QTemporaryDir dir;QString error;
    for (const auto hull:{OrbitalStationHullType::NativeCitadel,OrbitalStationHullType::NativeBastion}) {
        auto h=legacy(); h.state.players.front().technology.levels.fill(26);
        StationLoadout fit{hull};const auto& spec=station_hull_spec(hull);
        for(unsigned i=0;i<spec.weapons;++i)fit.components.push_back(ShipComponentType::Laser);
        for(unsigned i=0;i<spec.defenses;++i)fit.components.push_back(ShipComponentType::MoleSkinShield);
        for(unsigned i=0;i<spec.electronics;++i)fit.components.push_back(ShipComponentType::BattleComputer);
        assert(station_loadout_available(h.state,1,fit));
        const auto id=h.state.orbitalStations.front().id,home=h.state.planets.front().id;
        const auto record=h.state.planets.front().nativeOrbit;
        const StationConstruction refit{fit,id};
        assert(complete_station_construction(h.state,h.state.planets.front(),refit));
        assert(h.state.planets.front().nativeOrbit==record);
        assert(make_player_turn(h,1).state.orbitalStations.front().hull==OrbitalStationHullType::Starbase);
        set_survey_level(h.state,1,h.state.planets.front().star,SurveyLevel::DeepSurvey,h.state.turn);
        h.pendingOrders={1,{QueueStationOrder{home,refit}}};h.pendingDescriptions={"Paid refit"};
        h.state.players.push_back({2,"Other human"});h.playerTokens[2]=5678;
        h.inbox={{2,h.pendingOrders.orders}};
        for(bool player:{false,true}) {
            auto data=player?make_player_turn(h,1):h;data.pendingOrders=h.pendingOrders;data.pendingDescriptions=h.pendingDescriptions;
            const auto path=dir.filePath("current.suns");SaveGameData loaded;
            assert(write_save_game_file(path,data,error));
            const bool read=read_save_game_file(path,loaded,error);
            if(!read)qCritical("Hull %u, player %d: %s",unsigned(hull),player,qPrintable(error));
            assert(read);
            assert(loaded.state.orbitalStations.front().hull==hull && loaded.state.orbitalStations.front().components==fit.components);
            assert(loaded.state.planets.front().nativeOrbit==record);
            assert(std::get<QueueStationOrder>(loaded.pendingOrders.orders.front()).construction==refit);
            if(!player)assert(std::get<QueueStationOrder>(loaded.inbox.front().orders.front()).construction==refit);
        }
        TurnOrderFileData packet{h.campaignId,h.state.turn,h.turnToken,h.pendingOrders,h.pendingDescriptions},decoded;
        const auto path=dir.filePath("current.sunsorders");
        assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,decoded,error));
        assert(std::get<QueueStationOrder>(decoded.orders.orders.front()).construction==refit);
        // A new enum payload cannot masquerade as an old order format.
        QFile file(path);assert(file.open(QIODevice::ReadOnly));auto bytes=file.readAll();file.close();
        assert(quint8(bytes[7])==55);bytes[7]=char(50);
        assert(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size());file.close();
        assert(!read_turn_order_file(path,decoded,error));
    }
}
void a_remote_paid_upgrade_is_known_only_after_delivery_and_reload()
{
    auto h=legacy();auto& s=h.state;
    const auto target=s.planets[1].id;
    auto remote=s.planets.front();remote.id=target;remote.star=s.stars[1].id;remote.homeworld=false;
    remote.nativeOrbit={s.nextOrbitalStationId++,100'000};remote.productionQueue.clear();remote.minerals={2000,2000,2000};
    s.stars[1].position={400,0};s.stars[1].stellarClass=StarClass::Yellow;
    auto station=s.orbitalStations.front();station.id=remote.nativeOrbit.station;station.planet=target;
    station.hull=OrbitalStationHullType::OrbitalDock;s.orbitalStations.push_back(station);s.planets[1]=remote;
    set_survey_level(s,1,remote.star,SurveyLevel::DeepSurvey,s.turn);
    assert(communication_delay_turns(s,1,{400,0})>0);
    const StationLoadout fit{OrbitalStationHullType::NativeCitadel,{ShipComponentType::BulkRadiationScreen},true,true};
    const PlayerOrders orders{1,{QueueStationOrder{target,{fit,station.id}}}};
    for(unsigned year=0;year<15 && find_orbital_station_at_planet(s,target)->hull!=fit.hull;++year)
        s=TurnProcessor{}.process(s,year?std::vector<PlayerOrders>{}:std::vector<PlayerOrders>{orders});
    assert(find_orbital_station_at_planet(s,target)->hull==fit.hull);
    assert(known_orbital_station(s,1,target)->hull==OrbitalStationHullType::OrbitalDock);
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("remote.suns");
    h.pendingOrders={1,{}};h.pendingDescriptions.clear();
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));h=loaded;
    assert(write_save_game_file(path,make_player_turn(h,1),error) && read_save_game_file(path,loaded,error));
    assert(find_orbital_station_at_planet(loaded.state,target)->hull==OrbitalStationHullType::OrbitalDock);
    for(unsigned year=0;year<15 && known_orbital_station(h.state,1,target)->hull!=fit.hull;++year)
        h.state=TurnProcessor{}.process(h.state,{});
    assert(known_orbital_station(h.state,1,target)->hull==fit.hull);
    assert(write_save_game_file(path,make_player_turn(h,1),error) && read_save_game_file(path,loaded,error));
    assert(find_orbital_station_at_planet(loaded.state,target)->hull==fit.hull);
}
void designer_checks_received_housing_cargo_and_racial_research()
{
    MainWindow w;
    const auto choose=[&](OrbitalStationHullType target,bool enabled) {
        bool inspected=false;
        QTimer::singleShot(0,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());assert(dialog);
            auto* hull=dialog->findChild<QComboBox*>("stationHull");assert(hull);
            const auto row=hull->findData(int(target));assert(row>=0);hull->setCurrentIndex(row);
            auto* queue=dialog->findChild<QPushButton*>("queueStationFit");assert(queue && queue->isEnabled()==enabled);
            if(enabled && target==OrbitalStationHullType::NativeBastion
                && qEnvironmentVariableIsSet("SUNS_CAPTURE_NATIVE_HULLS"))
                dialog->grab().save("/tmp/suns-native-hulls-designer.png");
            inspected=true;
            if(enabled)queue->click();else dialog->reject();
        });
        MainWindowTestAccess::designer(w);assert(inspected);
    };
    for(const auto hull:{OrbitalStationHullType::NativeCitadel,OrbitalStationHullType::NativeBastion})
        for(bool unlocked:{false,true}) {
            auto s=legacy().state;s.players.front().technology.levels[std::size_t(ResearchField::Construction)]=station_hull_spec(hull).construction-(unlocked?0:1);
            MainWindowTestAccess::install(w,s);choose(hull,unlocked);
            assert(MainWindowTestAccess::orders(w).orders.size()==(unlocked?1:0));
        }
    auto s=legacy().state;const auto home=s.planets.front().id,id=s.planets.front().nativeOrbit.station;
    const StationLoadout bastion{OrbitalStationHullType::NativeBastion,{ShipComponentType::BulkRadiationScreen},true,true};
    assert(complete_station_construction(s,s.planets.front(),{bastion,id}));
    s.planets.front().nativeOrbit.population=500'001;
    set_survey_level(s,1,s.planets.front().star,SurveyLevel::DeepSurvey,s.turn);
    MainWindowTestAccess::install(w,s);choose(OrbitalStationHullType::NativeCitadel,false);
    const auto design=s.nextShipDesignId++;s.shipDesigns.push_back({design,1,"Evacuation",ShipHullType::SmallFreighter,{ShipComponentType::QuickJump5}});
    Fleet carrier;carrier.id=s.nextFleetId++;carrier.owner=1;carrier.design=design;carrier.ships={{design,1}};
    carrier.position=s.stars.front().position;carrier.fuel=fleet_fuel_capacity(s,carrier);carrier.telemetry=confirmed_fleet_telemetry(s,carrier);
    s.fleets.push_back(carrier);
    const PlayerOrders evacuation{1,{TransferCargoOrder{{home,0,true},{0,carrier.id},1,{}}}};
    MainWindowTestAccess::install(w,s,evacuation);choose(OrbitalStationHullType::NativeCitadel,true);
    assert(MainWindowTestAccess::orders(w).orders.size()==2);
    // A hidden physical population increase must not change authoring admission.
    s.planets.front().nativeOrbit.population=490'000;set_survey_level(s,1,s.planets.front().star,SurveyLevel::DeepSurvey,s.turn);
    s.planets.front().nativeOrbit.population=500'001;
    MainWindowTestAccess::install(w,s);choose(OrbitalStationHullType::NativeCitadel,true);
    auto general=make_demo_game();general.players.front().technology.levels.fill(26);MainWindowTestAccess::install(w,general);
    QTimer::singleShot(0,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());assert(dialog);
        assert(dialog->findChild<QComboBox*>("stationHull")->findData(int(OrbitalStationHullType::NativeCitadel))<0);dialog->reject();
    });MainWindowTestAccess::designer(w);
}
void a_refit_admitted_before_unloading_keeps_its_row_and_cancel_index()
{
    auto state=legacy().state;auto& home=state.planets.front();
    const auto station=home.nativeOrbit.station,planet=home.id;
    const auto miner=state.shipDesigns.front().id;
    const StationLoadout bastion{OrbitalStationHullType::NativeBastion,{ShipComponentType::BulkRadiationScreen},true,true};
    assert(complete_station_construction(state,home,{bastion,station}));
    home.nativeOrbit.population=500'000;home.minerals={2000,2000,2000};
    const auto design=state.nextShipDesignId++;
    state.shipDesigns.push_back({design,1,"Resident carrier",ShipHullType::SmallFreighter,{ShipComponentType::QuickJump5}});
    Fleet carrier;carrier.id=state.nextFleetId++;carrier.owner=1;carrier.design=design;carrier.ships={{design,1}};
    carrier.position=state.stars.front().position;carrier.colonists=1;carrier.fuel=fleet_fuel_capacity(state,carrier);
    carrier.telemetry=confirmed_fleet_telemetry(state,carrier);state.fleets.push_back(carrier);
    set_survey_level(state,1,home.star,SurveyLevel::DeepSurvey,state.turn);
    MainWindow w;w.installUiPolish();w.installProductionQueue();MainWindowTestAccess::install(w,state);
    bool admitted=false;
    QTimer::singleShot(0,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());assert(dialog);
        auto* hull=dialog->findChild<QComboBox*>("stationHull");assert(hull);
        hull->setCurrentIndex(hull->findData(int(OrbitalStationHullType::NativeCitadel)));
        auto* queue=dialog->findChild<QPushButton*>("queueStationFit");assert(queue && queue->isEnabled());
        admitted=true;queue->click();
    });
    MainWindowTestAccess::designer(w);assert(admitted);
    MainWindowTestAccess::cargo(w,{{0,carrier.id},{planet,0,true},1,{}});
    MainWindowTestAccess::queueMiner(w,miner);
    const auto orders=MainWindowTestAccess::orders(w);assert(orders.orders.size()==3);
    const auto control=TurnProcessor{}.process(state,{orders});
    const auto& kept=control.planets.front().productionQueue;
    assert(kept.size()==2 && kept[0].station && kept[0].station->loadout.hull==OrbitalStationHullType::NativeCitadel);
    assert(kept[1].shipDesign==miner && control.orbitalStations.front().hull==OrbitalStationHullType::NativeBastion);
    auto* tree=MainWindowTestAccess::production(w);assert(tree && tree->topLevelItemCount()==2);
    assert(tree->topLevelItem(0)->text(1).contains("Citadel") && tree->topLevelItem(0)->text(2)=="housing");
    assert(tree->topLevelItem(1)->text(1).contains("Orbital Miner"));
    assert(tree->topLevelItem(0)->text(3)==QString::fromUtf8("—") && tree->topLevelItem(1)->text(3)==QString::fromUtf8("—"));
    tree->setCurrentItem(tree->topLevelItem(1));auto* remove=MainWindowTestAccess::removeButton(w);
    assert(remove && remove->isEnabled());remove->click();
    const auto& cancelled=MainWindowTestAccess::orders(w);
    const auto* cancel=std::get_if<CancelProductionOrder>(&cancelled.orders.back());assert(cancel && cancel->index==1);
    const auto result=TurnProcessor{}.process(state,{cancelled});
    assert(result.planets.front().productionQueue.size()==1 && result.planets.front().productionQueue.front().station);
    assert(result.planets.front().productionQueue.front().remainingCost==station_build_cost(std::get<QueueStationOrder>(orders.orders[0]).construction.loadout));
    // Reversing the cargo/refit order must reject that same shrink at admission.
    auto reversed=orders;std::swap(reversed.orders[0],reversed.orders[1]);
    const auto preview=planned_production_state(make_player_view(state,1).state,reversed);
    assert(preview.planets.front().nativeOrbit.population==500'001);
    assert(preview.planets.front().productionQueue.size()==1 && !preview.planets.front().productionQueue.front().station);
}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    old_files_keep_their_real_paid_generic_hull_and_orders();
    new_fits_survive_all_record_and_order_channels();
    a_remote_paid_upgrade_is_known_only_after_delivery_and_reload();
    designer_checks_received_housing_cargo_and_racial_research();
    a_refit_admitted_before_unloading_keeps_its_row_and_cancel_index();
}
