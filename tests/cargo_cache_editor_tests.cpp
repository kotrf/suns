#include "main_window.hpp"
#include "save_game.hpp"
#include "cargo_cache_fixture.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/transport_lines.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>
using namespace suns;
namespace {
QByteArray bytes(const QString& path){QFile f(path);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
SaveGameData host(GameState state)
{
    SaveGameData h;h.state=std::move(state);h.mode=SessionMode::Host;h.campaignId=54;h.turnToken=17;
    h.playerTokens={{1,17}};h.pendingOrders={1,{}};return h;
}
void frozen_capacity_and_material_snapshots_survive_round_trip()
{
    auto h=host(test::cargo_cache_fixture({1000,0}));auto& cache=h.state.fleets.front();
    cache.minerals={1500,0,0};publish_fleet_telemetry(h.state,h.state.turn);
    assert(cache.outpost==OutpostKind::CargoCache && !cache.telemetryInTransit.empty());
    const auto before=make_player_view(h.state,1);
    assert(before.state.fleets.front().outpost==OutpostKind::None);
    assert(fleet_cargo_capacity(before.state,before.state.fleets.front())==0);
    QTemporaryDir dir;QString error;const auto path=dir.filePath("cache-host.suns");SaveGameData loaded;
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));h=loaded;
    assert(h.state.fleets.front().minerals.ironium==1500 && h.state.fleets.front().outpost==OutpostKind::CargoCache);
    const auto a=dir.filePath("a.sunsturn"),b=dir.filePath("b.sunsturn");
    assert(write_save_game_file(a,make_player_turn(h,1),error));
    auto hidden=h;hidden.state.fleets.front().minerals={1999,0,0};hidden.state.fleets.front().fuel=0;
    hidden.state.fleets.front().damagePercent=50;
    assert(write_save_game_file(b,make_player_turn(hidden,1),error) && bytes(a)==bytes(b));
    const auto& packets=h.state.fleets.front().telemetryInTransit;
    const auto receipt=std::find_if(packets.begin(),packets.end(),[](const auto& p){return p.telemetry.outpost==OutpostKind::CargoCache && p.telemetry.minerals.ironium==1500;});
    assert(receipt!=packets.end());h.state.turn=receipt->deliveryTurn;
    deliver_due_fleet_telemetry(h.state);(void)deliver_due_player_reports(h.state);
    const auto view=make_player_view(h.state,1);
    assert(view.state.fleets.front().outpost==OutpostKind::CargoCache && view.state.fleets.front().minerals.ironium==1500);
    assert(fleet_cargo_capacity(view.state,view.state.fleets.front())==2000);
    assert(write_save_game_file(b,make_player_turn(h,1),error) && read_save_game_file(b,loaded,error));
    assert(loaded.state.fleets.front().outpost==OutpostKind::CargoCache && loaded.state.fleets.front().minerals.ironium==1500);
    h.state.pendingFleetLossContacts.push_back({h.state.fleets.front(),h.state.turn+5});h.state.fleets.erase(h.state.fleets.begin());
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.pendingFleetLossContacts.front().lastContact.outpost==OutpostKind::CargoCache);
    assert(loaded.state.pendingFleetLossContacts.front().lastContact.minerals.ironium==1500);
}
void complete_dated_material_manifest_validation()
{
    auto h=host(test::cargo_cache_fixture({1000,0}));auto& f=h.state.fleets.front();
    f.minerals={1980,0,0};f.xenocrystal=20;publish_fleet_telemetry(h.state,h.state.turn);
    const auto packet=std::find_if(f.telemetryInTransit.begin(),f.telemetryInTransit.end(),[](const auto& p){
        return p.telemetry.outpost==OutpostKind::CargoCache && p.telemetry.minerals.ironium==1980;
    });
    assert(packet!=f.telemetryInTransit.end() && packet->telemetry.xenocrystal==20);
    const auto index=std::size_t(packet-f.telemetryInTransit.begin());
    QTemporaryDir dir;QString error;SaveGameData loaded;const auto path=dir.filePath("dated-cache.suns");
    assert(write_save_game_file(path,h,error) && read_save_game_file(path,loaded,error));
    assert(loaded.state.fleets.front().telemetryInTransit[index].telemetry.xenocrystal==20);
    // The received/in-flight manifest owns its cargo, independently of truth.
    auto invalid=h;invalid.state.fleets.front().minerals={};invalid.state.fleets.front().xenocrystal=0;
    invalid.state.fleets.front().telemetryInTransit[index].telemetry.xenocrystal=21;
    assert(!write_save_game_file(dir.filePath("invalid.suns"),invalid,error));
    // Corrupt only the dated EXOT quantity. Physical cargo still fits, so this
    // needs the final validation after all extension payloads have loaded.
    auto data=bytes(path);const auto tag=data.indexOf(QByteArray::fromHex("45584f54"));assert(tag>=0);
    QByteArray prefix;QDataStream record(&prefix,QIODevice::WriteOnly);
    record<<quint32(h.state.fleets.size())<<quint32(f.id)<<quint32(f.xenocrystal)
        <<quint32(f.telemetry.xenocrystal)<<quint32(f.telemetryInTransit.size());
    for(const auto& p:f.telemetryInTransit)record<<quint32(p.telemetry.xenocrystal);
    const auto position=data.indexOf(prefix,tag+8);assert(position>=tag+8);
    QDataStream patch(&data,QIODevice::ReadWrite);assert(patch.device()->seek(position+20+4*index));patch<<quint32{21};
    QFile corrupt(path);assert(corrupt.open(QIODevice::WriteOnly) && corrupt.write(data)==data.size());corrupt.close();
    assert(!read_save_game_file(path,loaded,error) && error.contains("invalid cargo-cache state"));
}
void appended_ids_and_old_id_wire_layout()
{
    QTemporaryDir dir;QString error;const auto path=dir.filePath("cache.sunsorders");
    TurnOrderFileData packet{54,1,17,{1,{CreateShipDesignOrder{"Cache",ShipHullType::Scout,
        {ShipComponentType::FusionDrive,ShipComponentType::CargoCacheKit}}}},{"Create paid cache constructor"}},decoded;
    assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,decoded,error));
    assert(std::get<CreateShipDesignOrder>(decoded.orders.orders.front()).components.back()==ShipComponentType::CargoCacheKit);
    auto data=bytes(path);QDataStream oldPacket(&data,QIODevice::ReadWrite);assert(oldPacket.device()->seek(4));oldPacket<<quint32{55};
    QFile f(path);assert(f.open(QIODevice::WriteOnly) && f.write(data)==data.size());f.close();
    assert(!read_turn_order_file(path,decoded,error)); // v55 never knew this appended component.
    auto h=host(make_demo_game());const auto save=dir.filePath("old-ids.suns");
    assert(write_save_game_file(save,h,error));data=bytes(save);
    QDataStream oldSave(&data,QIODevice::ReadWrite);assert(oldSave.device()->seek(4));oldSave<<quint32{128};
    QFile legacy(save);assert(legacy.open(QIODevice::WriteOnly) && legacy.write(data)==data.size());legacy.close();
    SaveGameData loaded;assert(read_save_game_file(save,loaded,error));
    // Explicit byte-layout check with old identities, not a historical fixture.
    assert(loaded.state.fleets.front().design==h.state.fleets.front().design);
}
}
namespace suns {
struct MainWindowTestAccess {
    static void transfer(MainWindow& w,const char* source,const char* destination,double amount,double maximum,bool accept,bool scheduled=false)
    {
        QTimer::singleShot(0,&w,[&] {
            auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget());assert(d);
            const auto choose=[&](const char* object,const char* value) {
                auto* c=d->findChild<QComboBox*>(object);const auto i=c->findText(value,Qt::MatchContains);
                if(i<0) {
                    QStringList names;for(int n=0;n<c->count();++n)names<<c->itemText(n);
                    qFatal("Missing Cargo endpoint %s in %s",value,qPrintable(names.join("; ")));
                }
                c->setCurrentIndex(i);
            };
            choose("cargoSourceCombo",source);choose("cargoDestinationCombo",destination);
            auto* spin=d->findChild<QDoubleSpinBox*>("cargoMineralSpin0");assert(spin->maximum()==maximum);
            spin->setValue(amount);assert(spin->value()==std::min(amount,maximum));
            auto* buttons=d->findChild<QDialogButtonBox*>();assert(buttons->button(QDialogButtonBox::Ok)->isEnabled()==accept);
            const auto base=scheduled || QString(source).contains("base") || QString(destination).contains("base");
            assert(d->findChild<QLabel*>("cargoSourceBeforeLabel")->text().contains(base?"scheduled base":"Available now"));
            buttons->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();
        });
        w.openCargoManifestDialog();
    }
    static GameState resolve(MainWindow& w)
    {
        QTemporaryDir dir;QString error;const auto path=dir.filePath("cache-service.sunsorders");
        TurnOrderFileData data{54,w.state_.turn,17,w.pendingOrders_,w.pendingDescriptions_},decoded;
        assert(write_turn_order_file(path,data,error) && read_turn_order_file(path,decoded,error));
        return TurnProcessor{}.process(w.state_,{decoded.orders});
    }
    static void cargo_respects_base_service_phase(MainWindow& w)
    {
        for(double current:{0.0,50.0}) {
            w.state_=test::cargo_cache_with_base();auto& cache=w.state_.fleets.front();cache.minerals={current,0,0};
            cache.telemetry={};cache.telemetry=confirmed_fleet_telemetry(w.state_,cache);
            w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.fleet=2;w.selection_.star.reset();w.rebuildScene();
            transfer(w,"Cache supply base","Forward cache",100,100,true);assert(w.pendingOrders_.orders.size()==1);
            transfer(w,"Forward cache","Mineral carrier",100,current,current>0);
            const auto actual=resolve(w);
            assert(test::cache_fleet(actual,1).minerals.ironium==100 && test::cache_fleet(actual,2).minerals.ironium==current);
            assert(test::cache_fleet(actual,3).minerals.ironium==0);
            w.state_=actual;w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.rebuildScene();
            transfer(w,"Forward cache","Mineral carrier",100,100,true);
            const auto next=resolve(w);
            assert(test::cache_fleet(next,1).minerals.ironium==0 && test::cache_fleet(next,2).minerals.ironium==current+100);
        }
        w.state_=test::cargo_cache_with_base();w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.fleet=2;w.rebuildScene();
        transfer(w,"Cache supply base","Forward cache",100,100,true);
        transfer(w,"Forward cache","Cache supply base",100,100,true);
        const auto returned=resolve(w);
        assert(test::cache_fleet(returned,1).minerals.ironium==0 && test::cache_fleet(returned,3).minerals.ironium==100);
    }
    static void logistics_excludes_caches(MainWindow& w)
    {
        w.state_=test::cargo_cache_fixture();auto& s=w.state_;
        auto design=*find_ship_design(s,100);design.id=103;design.components.back()=ShipComponentType::FuelDepotKit;
        design.placements.clear();normalize_ship_design_placement(design);s.shipDesigns.push_back(design);s.nextShipDesignId=104;
        auto depot=s.fleets.front();depot.id=4;depot.design=103;depot.ships={{103,1}};depot.name="Ordinary depot";
        depot.position={80,-50};depot.outpost=OutpostKind::FuelDepot;depot.telemetry={};depot.telemetry=confirmed_fleet_telemetry(s,depot);
        s.fleets.push_back(depot);s.nextFleetId=5;
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.rebuildScene();
        if(!w.findChild<QComboBox*>("transportLineDestination"))w.installTransportLines();
        auto* destination=w.findChild<QComboBox*>("transportLineDestination");auto* extra=w.findChild<QComboBox*>("transportLineExtraDestination");
        const auto cache=QVariant::fromValue(-qlonglong(1)),fuel=QVariant::fromValue(-qlonglong(4));
        assert(destination && extra && destination->findData(cache)<0 && extra->findData(cache)<0);
        assert(destination->findData(fuel)>=0 && extra->findData(fuel)>=0);
        destination->addItem("Cargo cache",cache);destination->setCurrentIndex(destination->count()-1);
        w.findChild<QLineEdit*>("transportLineName")->setText("Unsupported cache route");
        w.findChild<QPushButton*>("transportLineSave")->click();
        assert(w.pendingOrders_.orders.empty() && w.findChild<QLabel*>("transportLineStatus")->text().contains("manual Cargo"));
        TransportLineSpec line{1,1,"Forged cache line",s.planets.front().id,0,5,TransportCargo::Ironium,true};line.destinationFleet=1;
        w.pendingOrders_={1,{SetTransportLineOrder{line},AssignTransportLineOrder{1,2}}};w.pendingDescriptions_={"Define","Assign"};
        const auto rejected=resolve(w);assert(rejected.players.front().transportLines.empty());
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();
        TransportLine legacy{line,find_star(s,s.planets.front().star)->position,s.fleets.front().position,{{2,s.turn}},{}};
        s.players.front().transportLines={legacy};s.players.front().lastTransportLineId=1;
        const auto programme=transport_line_program(legacy);
        assert(submit_fleet_route_command(s,1,2,programme.destination,programme.warp,programme.arrivalAction,
            programme.queuedWaypoints,programme.repeatOrders));
        auto& ship=s.fleets[1];ship.telemetry={};ship.telemetry=confirmed_fleet_telemetry(s,ship);
        w.rebuildScene();w.routeProgramContextChanged(true);
        auto* lines=w.findChild<QComboBox*>("transportLineSelection");const auto selected=lines->findData(1u);assert(selected>=0);lines->setCurrentIndex(selected);
        auto* assign=w.findChild<QPushButton*>("transportLineAssign");assert(!assign->isEnabled());assign->click();assert(w.pendingOrders_.orders.empty());
        auto* pause=w.findChild<QPushButton*>("transportLinePause");assert(pause->isEnabled());pause->click();
        assert(w.pendingOrders_.orders.size()==1 && !std::get<SetTransportLineOrder>(w.pendingOrders_.orders.front()).value.active);
        pause->click();assert(w.pendingOrders_.orders.size()==1); // Unsupported resume does not emit a command.
        const auto stopped=resolve(w);
        assert(!stopped.players.front().transportLines.front().spec.active && !test::cache_fleet(stopped,2).repeatOrders);
    }
    static void merge_preserves_transfer_phase_after_reload(MainWindow& w)
    {
        w.state_=test::cargo_cache_with_base();auto& carrier=w.state_.fleets[1];carrier.minerals={200,0,0};
        carrier.telemetry={};carrier.telemetry=confirmed_fleet_telemetry(w.state_,carrier);
        w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();w.selection_.fleet=2;w.selection_.star.reset();w.rebuildScene();
        transfer(w,"Mineral carrier","Forward cache",100,200,true);
        QTimer::singleShot(0,&w,[&] {
            auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget());assert(d && d->windowTitle()=="Merge fleets");
            auto* source=d->findChild<QComboBox*>();const auto base=source->findData(3u);assert(base>=0);source->setCurrentIndex(base);
            d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        w.openMergeFleetsDialog();assert(w.pendingOrders_.orders.size()==2);
        const auto& merge=std::get<MergeFleetsOrder>(w.pendingOrders_.orders.back());assert(merge.destination==2 && merge.source==3);
        // The phase book is derived again from the original ordered intent;
        // it is not persisted as physical state or recovered from final hulls.
        auto save=host(w.state_);save.pendingOrders=w.pendingOrders_;save.pendingDescriptions=w.pendingDescriptions_;
        QTemporaryDir dir;QString error;const auto path=dir.filePath("merge-with-cargo.suns");
        assert(write_save_game_file(path,save,error) && w.loadGameFromPath(path));
        w.selection_.fleet=2;w.selection_.star.reset();w.rebuildScene();
        const auto mergedName=QString::fromStdString(test::cache_fleet(w.fleetPlanningState(),2).name).toUtf8();
        transfer(w,"Forward cache",mergedName.constData(),200,100,true,true);
        const auto actual=resolve(w);
        assert(actual.fleets.size()==2 && test::cache_fleet(actual,1).minerals.ironium==0);
        assert(test::cache_fleet(actual,2).minerals.ironium==300);
    }
    static void cargo_exchange_and_received_panel(MainWindow& w)
    {
        w.state_=test::cargo_cache_fixture();w.pendingOrders_={1,{}};w.pendingDescriptions_.clear();
        w.selection_.fleet=2;w.selection_.star.reset();w.rebuildScene();
        auto* table=w.findChild<QTreeWidget*>("outpostContacts");assert(table && table->topLevelItemCount()==1);
        assert(table->topLevelItem(0)->text(1)=="Cargo cache" && table->topLevelItem(0)->text(7).contains("2000"));
        const auto dialogTransfer=[&](bool returning) {
            QTimer::singleShot(0,&w,[&] {
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());assert(dialog);
                auto* from=dialog->findChild<QComboBox*>("cargoSourceCombo");auto* to=dialog->findChild<QComboBox*>("cargoDestinationCombo");assert(from && to);
                const auto select=[](QComboBox* combo,const char* name){const auto index=combo->findText(name,Qt::MatchContains);assert(index>=0);combo->setCurrentIndex(index);};
                select(from,returning?"Forward cache":"Mineral carrier");select(to,returning?"Mineral carrier":"Forward cache");
                auto* people=dialog->findChild<QSpinBox*>("cargoColonistSpin");
                assert(people && !people->isEnabled() && !people->minimum() && !people->maximum());
                assert(!dialog->findChild<QSlider*>("cargoColonistSlider")->isEnabled());
                assert(!dialog->findChild<QSlider*>("cargoDestinationColonistSlider")->isEnabled());
                dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin0")->setValue(returning?100:1000);
                if(!returning) {
                    dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin1")->setValue(600);
                    dialog->findChild<QDoubleSpinBox*>("cargoMineralSpin2")->setValue(400);
                }
                auto* accept=dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);assert(accept->isEnabled());accept->click();
            });
            w.openCargoManifestDialog();
        };
        dialogTransfer(false);dialogTransfer(true);assert(w.pendingOrders_.orders.size()==2);
        QTemporaryDir dir;QString error;const auto path=dir.filePath("ui-cache.sunsorders");
        TurnOrderFileData packet{54,w.state_.turn,17,w.pendingOrders_,w.pendingDescriptions_},decoded;
        assert(write_turn_order_file(path,packet,error) && read_turn_order_file(path,decoded,error));
        const auto actual=TurnProcessor{}.process(w.state_,{decoded.orders});
        assert(test::cache_fleet(actual,1).minerals.ironium==900 && test::cache_fleet(actual,1).minerals.boranium==600);
        assert(test::cache_fleet(actual,1).minerals.germanium==400 && !test::cache_fleet(actual,1).colonists);
        assert(test::cache_fleet(actual,2).minerals.ironium==300 && test::cache_fleet(actual,2).colonists==1000);
        const auto snapshot=table->topLevelItem(0)->text(7);
        w.state_.fleets.front().minerals={1999,0,0};w.state_.fleets.front().fuel=0;w.state_.fleets.front().damagePercent=50;
        w.rebuildScene();assert(table->topLevelItem(0)->text(7)==snapshot);
    }
};
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    frozen_capacity_and_material_snapshots_survive_round_trip();complete_dated_material_manifest_validation();appended_ids_and_old_id_wire_layout();
    MainWindow w;w.installUiPolish();w.installFleetReadabilityPolish();w.installOutposts();
    MainWindowTestAccess::cargo_exchange_and_received_panel(w);
    MainWindowTestAccess::cargo_respects_base_service_phase(w);
    MainWindowTestAccess::merge_preserves_transfer_phase_after_reload(w);
    MainWindowTestAccess::logistics_excludes_caches(w);
}
