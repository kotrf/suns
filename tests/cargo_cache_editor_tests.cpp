#include "main_window.hpp"
#include "save_game.hpp"
#include "cargo_cache_fixture.hpp"
#include "suns/player_knowledge.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
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
}
