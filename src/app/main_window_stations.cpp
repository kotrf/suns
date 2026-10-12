#include "main_window.hpp"
#include "suns/campaign.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>
#include <functional>

namespace suns {
void MainWindow::openStationDesigner()
{
    const auto* selected=selectedPlanet();
    if(!selected || selected->owner!=pendingOrders_.player) return;
    const PlanetId planetId=selected->id;
    const auto known=make_player_view(state_,pendingOrders_.player).state;
    const auto* station=known_orbital_station(known,pendingOrders_.player,planetId);
    const auto plannedQueue=plannedProductionQueue(*selected);
    const bool existingQueued=std::any_of(plannedQueue.begin(),plannedQueue.end(),
        [](const auto& i) { return i.kind==ProductionKind::OrbitalStation; });
    StationConstruction work{station ? station_loadout(*station) : StationLoadout{},station ? station->id : 0};
    for(const auto& item:plannedQueue) if(item.station) work=*item.station;
    QDialog dialog(this); dialog.setObjectName("stationDesignerDialog");
    dialog.setWindowTitle(QString("Orbital station — %1").arg(QString::fromStdString(selected->name)));
    dialog.resize(800,620);
    dialog.setStyleSheet("QDialog#stationDesignerDialog { background: #0b121a; }"
        "QTableWidget { background: #101d29; color: #d6e5f2; gridline-color: #34495a; }"
        "QTableWidget::item { background: #142433; }"
        "QHeaderView::section { background: #1b2c3b; color: #d6e5f2; padding: 4px; }"
        "QCheckBox:disabled, QLabel:disabled { color: #8599aa; }");
    auto* layout=new QVBoxLayout(&dialog);
    auto* status=new QLabel(&dialog); status->setObjectName("stationCurrentStatus"); status->setWordWrap(true);
    status->setText(station ? QString("Station #%1 — %2. Armor %3, shields %4, damage %5%. Refit pays the full fitted build cost and repairs the hull; the existing station operates until completion.")
        .arg(station->id).arg(QString::fromStdString(station->name)).arg(station_armor(*station),0,'f',0)
        .arg(station_shields(*station),0,'f',0).arg(station->damagePercent,0,'f',1)
        : "No confirmed orbital station. Construction requires colony production and surface minerals.");
    layout->addWidget(status);
    auto* row=new QHBoxLayout;
    auto* hull=new QComboBox(&dialog); hull->setObjectName("stationHull");
    for(const auto h:{OrbitalStationHullType::OrbitalDock,OrbitalStationHullType::Starbase,OrbitalStationHullType::Fortress}) {
        const auto& spec=station_hull_spec(h);
        hull->addItem(QString("%1 — Construction %2").arg(spec.name).arg(spec.construction),int(h));
    }
    hull->setCurrentIndex(hull->findData(int(work.loadout.hull)));
    auto* yard=new QCheckBox("Shipyard / fleet repair",&dialog); yard->setObjectName("stationShipyard"); yard->setChecked(work.loadout.shipyard);
    auto* fuel=new QCheckBox("Refueling depot",&dialog); fuel->setObjectName("stationRefueling"); fuel->setChecked(work.loadout.refueling);
    row->addWidget(hull,1); row->addWidget(yard); row->addWidget(fuel); layout->addLayout(row);
    auto* table=new QTableWidget(&dialog); table->setObjectName("stationFittingTable"); table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Slot","Equipment"}); table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents); table->verticalHeader()->hide();
    table->setSelectionMode(QAbstractItemView::NoSelection); layout->addWidget(table,1);
    auto* preview=new QLabel(&dialog); preview->setObjectName("stationFitPreview"); preview->setWordWrap(true); layout->addWidget(preview);
    auto* defense=new QLabel(&dialog); defense->setWordWrap(true);
    defense->setText(QString("Colony defenses: %1 / %2. Population protection %3%; installation protection %4%. Build defenses in the Production batch selector (Energy 1; each costs 5 production, I 2 / B 1 / G 1).")
        .arg(selected->defenses).arg(kMaximumColonyDefenses).arg(colony_defense_coverage(*selected)*100,0,'f',1)
        .arg(colony_defense_coverage(*selected)*50,0,'f',1)); layout->addWidget(defense);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    auto* queue=buttons->button(QDialogButtonBox::Ok); queue->setObjectName("queueStationFit"); queue->setText(work.refitStation ? "Queue refit" : "Queue construction");
    layout->addWidget(buttons);
    bool populating=false;
    const auto capture=[&] {
        StationLoadout fit{OrbitalStationHullType(hull->currentData().toInt()),{},yard->isChecked(),fuel->isChecked()};
        for(int i=0;i<table->rowCount();++i) if(const auto* c=qobject_cast<QComboBox*>(table->cellWidget(i,1));c && c->currentData().toInt()>=0)
            fit.components.push_back(ShipComponentType(c->currentData().toInt()));
        return fit;
    };
    const auto refresh=[&] {
        if(populating) return;
        const auto fit=capture();
        OrbitalStation draft; draft.hull=fit.hull; draft.components=fit.components;
        const auto minerals=station_mineral_cost(fit); const auto& spec=station_hull_spec(fit.hull);
        const bool available=station_loadout_available(known,pendingOrders_.player,fit);
        preview->setText(QString("Armor %1 • Shields %2 • Slots: %3 weapon / %4 defense / %5 electronics\nCost %6 production • I %7 / B %8 / G %9 kt\n%10")
            .arg(station_armor(draft),0,'f',0).arg(station_shields(draft),0,'f',0).arg(spec.weapons).arg(spec.defenses).arg(spec.electronics)
            .arg(station_build_cost(fit)).arg(minerals.ironium).arg(minerals.boranium).arg(minerals.germanium)
            .arg(existingQueued ? "A station project is already in the production queue; cancel it before replacing it."
                : available ? "Minerals are charged on completion. Existing fitted ships/stations keep their equipment."
                            : "This hull or equipment is not available at the empire's current technology."));
        queue->setEnabled(available && !existingQueued);
    };
    std::function<void(const StationLoadout&)> populate;
    populate=[&](const StationLoadout& fit) {
        populating=true; table->setRowCount(0);
        const auto& spec=station_hull_spec(fit.hull);
        const auto category=[](ShipComponentType c) { const auto k=component_spec(c).kind;
            return k==ShipComponentKind::BeamWeapon || k==ShipComponentKind::Torpedo ? 0
                : k==ShipComponentKind::Armor || k==ShipComponentKind::Shield ? 1 : 2; };
        const int capacities[]{spec.weapons,spec.defenses,spec.electronics};
        const QString names[]{"Weapon","Armor / shield","Electronics"};
        for(int group=0;group<3;++group) {
            std::vector<ShipComponentType> fitted;
            for(auto c:fit.components) if(category(c)==group) fitted.push_back(c);
            for(int slot=0;slot<capacities[group];++slot) {
                const int index=table->rowCount(); table->insertRow(index);
                table->setItem(index,0,new QTableWidgetItem(QString("%1 %2").arg(names[group]).arg(slot+1)));
                auto* choice=new QComboBox(table); choice->addItem("Empty",-1);
                for(unsigned i=0;i<=unsigned(ShipComponentType::Superlatanium);++i) {
                    const auto c=ShipComponentType(i);
                    if(station_component_supported(c) && category(c)==group
                        && (component_available_to_player(known,pendingOrders_.player,c)
                            || (slot<int(fitted.size()) && fitted[slot]==c))) {
                        const auto info=component_spec(c); choice->addItem(QString::fromStdString(info.name),int(c));
                        choice->setItemData(choice->count()-1,QString("Armor %1 • Shields %2 • Weapon power %3, range %4 • Production %5")
                            .arg(info.armor).arg(info.shields).arg(info.weaponPower).arg(info.weaponRange).arg(info.buildCost),Qt::ToolTipRole);
                    }
                }
                if(slot<int(fitted.size())) choice->setCurrentIndex(choice->findData(int(fitted[slot])));
                table->setCellWidget(index,1,choice);
                connect(choice,&QComboBox::currentIndexChanged,&dialog,[&](int) { refresh(); });
            }
        }
        populating=false; refresh();
    };
    populate(work.loadout);
    connect(hull,&QComboBox::currentIndexChanged,&dialog,[&](int) {
        auto fit=capture();
        if(!station_loadout_valid(fit)) {
            const QSignalBlocker blocker(hull); hull->setCurrentIndex(hull->findData(int(work.loadout.hull)));
            preview->setText("Remove excess equipment before changing to a smaller station hull."); return;
        }
        work.loadout=fit; populate(fit);
    });
    connect(yard,&QCheckBox::toggled,&dialog,[&](bool) { refresh(); });
    connect(fuel,&QCheckBox::toggled,&dialog,[&](bool) { refresh(); });
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted) return;
    work.loadout=capture();
    if(!station_loadout_available(known,pendingOrders_.player,work.loadout) || existingQueued) return;
    const auto description=QString("%1 %2 at %3 (%4 production)")
        .arg(work.refitStation ? "Refit" : "Build").arg(station_hull_spec(work.loadout.hull).name)
        .arg(QString::fromStdString(selected->name)).arg(station_build_cost(work.loadout));
    appendPendingOrder(QueueStationOrder{planetId,work},description);
    refreshProductionQueue();
}
} // namespace suns
