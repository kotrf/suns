#include "main_window.hpp"
#include "station_details_dialog.hpp"
#include "native_orbital_ui.hpp"
#include "suns/communications.hpp"
#include "suns/campaign.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include "suns/mobile_base.hpp"
#include "suns/orbital_bases.hpp"
#include "suns/exotic_materials.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QDockWidget>
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
namespace {
QString gateLimits(const StargateSpec& gate)
{
    return QString("%1 ly / %2 kt")
        .arg(gate.rangeLy == 0 ? QString("Unlimited") : QString::number(gate.rangeLy, 'f', 0))
        .arg(gate.maximumMassKt == 0 ? QString("Unlimited") : QString::number(gate.maximumMassKt, 'f', 0));
}
}
void MainWindow::openStationDetails()
{
    const auto received = make_player_view(state_, pendingOrders_.player).state;
    const auto* star = selectedStar();
    const auto* planet = star ? find_planet_at_star(received, star->id) : nullptr;
    const auto* station = planet ? find_orbital_station_at_planet(received, planet->id) : nullptr;
    if (!station) return;
    const auto dose = native_orbital_radiation_status(received, *planet);
    const auto support = native_orbital_support_text(received, *planet);
    StationDetailsDialog dialog(*station, received.turn, this,
        (dose ? native_orbital_radiation_text(*dose) : QString{}) + (support.isEmpty() ? QString{} : "\n" + support));
    dialog.exec();
}

void MainWindow::openStationDesigner()
{
    const auto* selected=selectedPlanet();
    if(!selected || selected->owner!=pendingOrders_.player) return;
    const PlanetId planetId=selected->id;
    const auto known=projected_cargo_order_state(make_player_view(state_,pendingOrders_.player).state,pendingOrders_);
    const auto* receivedPlanet = find_planet(known, planetId);
    const bool native = native_orbital_race(known, pendingOrders_.player);
    if (orbital_base_at(known,planetId)) {
        installOrbitalBases();
        if (auto* panel=findChild<QDockWidget*>("orbitalBasesDock")) { panel->show(); panel->raise(); }
        return;
    }
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
    std::vector<const StationHullSpec*> choices;
    for (const auto& spec:station_hulls()) if (!spec.nativeOnly || native || spec.hull==work.loadout.hull) choices.push_back(&spec);
    std::stable_sort(choices.begin(),choices.end(),[](const auto* a,const auto* b){return a->construction<b->construction;});
    for(const auto* spec:choices) {
        hull->addItem(QString("%1 — Construction %2").arg(spec->name).arg(spec->construction),int(spec->hull));
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
    defense->setVisible(!native); // Surface defenses do not shelter station residents.
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
        const StationConstruction proposed{fit,work.refitStation};
        const bool housingBlocked=receivedPlanet && station_refit_over_capacity(known,*receivedPlanet,proposed);
        preview->setText(QString("Armor %1 • Shields %2 • Slots: %3 weapon / %4 defense / %5 electronics\nCost %6 production • I %7 / B %8 / G %9 kt\n%10")
            .arg(station_armor(draft),0,'f',0).arg(station_shields(draft),0,'f',0).arg(spec.weapons).arg(spec.defenses).arg(spec.electronics)
            .arg(station_build_cost(fit)).arg(minerals.ironium).arg(minerals.boranium).arg(minerals.germanium)
            .arg(existingQueued ? "A station project is already in the production queue; cancel it before replacing it."
                : available ? "Minerals are charged on completion. Existing fitted ships/stations keep their equipment."
                            : "This hull or equipment is not available at the empire's current technology."));
        if (const auto* gate = station_stargate(draft))
            preview->setText(preview->text() + QString("\nGate: %1. Both endpoints impose their own limits. %2")
                .arg(gateLimits(*gate), find_player(known,pendingOrders_.player)->race.primaryTrait == PrimaryRaceTrait::StargateSpecialist
                    ? "Minerals and colonists are allowed; loaded mass counts."
                    : "Generalist: no minerals or colonists aboard."));
        if (receivedPlanet) if (const auto dose = native_orbital_radiation_status(known,
            pendingOrders_.player, receivedPlanet->star, receivedPlanet->nativeOrbit.population, fit))
            preview->setText(preview->text() + "\n" + native_orbital_radiation_text(*dose)
                + "\nThis protection takes effect when construction or refitting completes.");
        if (native && receivedPlanet) {
            const auto capacity=native_orbital_capacity(fit.hull);
            preview->setText(preview->text()+QString("\nResidents after accepted cargo: %1 / %2 housing.")
                .arg(qulonglong(receivedPlanet->nativeOrbit.population)).arg(qulonglong(capacity)));
            preview->setText(preview->text()+(housingBlocked
                ? QString(" Evacuate at least %1 residents before choosing this hull.").arg(qulonglong(receivedPlanet->nativeOrbit.population-capacity))
                : QString(" Paid refits preserve residents and support kits; new housing activates on completion.")));
        }
        queue->setEnabled(available && !existingQueued && !housingBlocked);
    };
    std::function<void(const StationLoadout&)> populate;
    populate=[&](const StationLoadout& fit) {
        populating=true; table->setRowCount(0);
        const auto& spec=station_hull_spec(fit.hull);
        const auto category=[](ShipComponentType c) { const auto k=component_spec(c).kind;
            return k==ShipComponentKind::BeamWeapon || k==ShipComponentKind::Torpedo ? 0
                : k==ShipComponentKind::Armor || k==ShipComponentKind::Shield || radiation_screen_protection(c) ? 1 : 2; };
        const int capacities[]{spec.weapons,spec.defenses,spec.electronics};
        const QString names[]{"Weapon","Defense","Electronics"};
        for(int group=0;group<3;++group) {
            std::vector<ShipComponentType> fitted;
            for(auto c:fit.components) if(category(c)==group) fitted.push_back(c);
            for(int slot=0;slot<capacities[group];++slot) {
                const int index=table->rowCount(); table->insertRow(index);
                table->setItem(index,0,new QTableWidgetItem(QString("%1 %2").arg(names[group]).arg(slot+1)));
                auto* choice=new QComboBox(table); choice->addItem("Empty",-1);
                for(unsigned i=0;i<=unsigned(kLastShipComponent);++i) {
                    const auto c=ShipComponentType(i);
                    const auto* gate = stargate_spec(c);
                    const bool available = gate ? stargate_component_available(known,pendingOrders_.player,c)
                            && station_hull_at_least(fit.hull,gate->minimumHull)
                        : component_available_to_player(known,pendingOrders_.player,c);
                    if(station_component_supported(c) && (native || !radiation_screen_protection(c)) && category(c)==group
                        && (available || (slot<int(fitted.size()) && fitted[slot]==c))) {
                        const auto info=component_spec(c);
                        choice->addItem(gate ? QString::fromUtf8(gate->name) : QString::fromStdString(info.name),int(c));
                        choice->setItemData(choice->count()-1,QString("Armor %1 • Shields %2 • Weapon power %3, range %4 • Production %5")
                            .arg(info.armor).arg(info.shields).arg(info.weaponPower).arg(info.weaponRange).arg(info.buildCost),Qt::ToolTipRole);
                        if (const auto protection = radiation_screen_protection(c))
                            choice->setItemData(choice->count()-1, QString("Radiation protection %1. One screen per station; occupies a defense slot. Cost %2 production.")
                                .arg(protection).arg(info.buildCost), Qt::ToolTipRole);
                        if (gate) choice->setItemData(choice->count()-1,
                            QString("%1 • Energy %2 / Electronics %3 / Construction %4. %5\nBoth owned endpoints must support the distance and loaded gross fleet mass.")
                                .arg(gateLimits(*gate)).arg(gate->energy).arg(gate->electronics).arg(gate->construction)
                                .arg(gate->specialistOnly ? "Stargate Specialist only." : "All empires."), Qt::ToolTipRole);
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
    if(!station_loadout_available(known,pendingOrders_.player,work.loadout) || existingQueued
        || !receivedPlanet || !station_construction_valid(known,*receivedPlanet,work)) return;
    const auto description=QString("%1 %2 at %3 (%4 production)")
        .arg(work.refitStation ? "Refit" : "Build").arg(station_hull_spec(work.loadout.hull).name)
        .arg(QString::fromStdString(selected->name)).arg(station_build_cost(work.loadout));
    appendPendingOrder(QueueStationOrder{planetId,work},description);
    refreshProductionQueue();
}
void MainWindow::openGateTransit()
{
    const auto* fleet = selectedFleet();
    const auto player = pendingOrders_.player;
    if (!fleet || fleet->owner != player) {
        QMessageBox::information(this, "Stargate transit", "Select one of your fleets first.");
        return;
    }
    const auto known = projected_cargo_order_state(make_player_view(state_, player).state, pendingOrders_);
    const auto* knownFleet = find_service_fleet(known, fleet->id);
    if (!knownFleet) return;

    struct GateChoice {
        GateEndpoint endpoint;
        QString name, limits;
    };
    std::vector<GateChoice> gates;
    for (const auto& planet : known.planets) {
        const auto* star = find_star(known, planet.star);
        if (!star || !stargate_station_available(known, player, planet.id)) continue;
        const auto* gate = station_stargate(*find_orbital_station_at_planet(known, planet.id));
        gates.push_back({{planet.id, 0, star->position}, QString::fromStdString(planet.name), gateLimits(*gate)});
    }
    for (const auto& base : known.fleets) {
        if (base.owner != player || base.id == fleet->id || !mobile_base_gate(known, base)) continue;
        gates.push_back({{0, base.id, base.position},
            QString("%1 (#%2) — Expedition Stargate").arg(QString::fromStdString(base.name)).arg(base.id),
            QString("%1 ly / %2 kt; %3 mg from this base per jump. Received position (%4, %5) ly.")
                .arg(kExpeditionGateRangeLy).arg(kExpeditionGateMassKt).arg(kExpeditionGateFuelMg)
                .arg(base.position.x, 0, 'f', 1).arg(base.position.y, 0, 'f', 1)});
    }
    std::vector<const GateChoice*> sources, destinations;
    const bool alreadyQueued = std::any_of(pendingOrders_.orders.begin(), pendingOrders_.orders.end(), [&](const auto& order) {
        if (const auto* orbital = std::get_if<GateFleetOrder>(&order)) return orbital->fleet == fleet->id;
        if (const auto* mobile = std::get_if<MobileGateOrder>(&order)) return mobile->fleet == fleet->id;
        return false;
    });
    const bool delayed = communication_delay_turns(known, player, knownFleet->position) > 0;
    const bool specialist = find_player(known, player)->race.primaryTrait == PrimaryRaceTrait::StargateSpecialist;

    QDialog dialog(this);
    dialog.setObjectName("stargateTransitDialog");
    dialog.setWindowTitle("Stargate transit");
    auto* layout = new QVBoxLayout(&dialog);
    auto* summary = new QLabel(&dialog);
    summary->setObjectName("stargateTransitSummary");
    summary->setWordWrap(true);
    layout->addWidget(summary);
    auto* sourcePicker = new QComboBox(&dialog);
    sourcePicker->setObjectName("stargateSources");
    for (const auto& gate : gates) {
        if (!same_position(knownFleet->position, gate.endpoint.position)) continue;
        sources.push_back(&gate);
        sourcePicker->addItem(gate.name);
        sourcePicker->setItemData(sourcePicker->count() - 1, gate.limits, Qt::ToolTipRole);
    }
    auto* targetPicker = new QComboBox(&dialog);
    targetPicker->setObjectName("stargateDestinations");
    auto* form = new QFormLayout;
    form->addRow("Source gate", sourcePicker);
    if (sources.empty()) sourcePicker->setEnabled(false);
    form->addRow("Destination gate", targetPicker);
    layout->addLayout(form);
    auto* resonant = new QCheckBox("Resonant pulse: spend 8 source xenocrystals for twice the finite range", &dialog);
    resonant->setObjectName("resonantGateTransit");
    resonant->setEnabled(resonant_gate_technology_available(known, player));
    auto* resonanceDetails = new QLabel(&dialog);
    resonanceDetails->setObjectName("gateResonanceDetails");
    resonanceDetails->setWordWrap(true);
    layout->addWidget(resonant);
    layout->addWidget(resonanceDetails);
    auto* status = new QLabel(&dialog);
    status->setObjectName("gateTransitStatus");
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    const auto refresh = [&] {
        const auto* source = sources.empty() ? nullptr : sources[sourcePicker->currentIndex()];
        const bool fromOrbit = source && source->endpoint.colony;
        resonant->setVisible(fromOrbit);
        resonanceDetails->setVisible(fromOrbit);
        const bool pulse = fromOrbit && resonant->isChecked();
        const auto observation = fromOrbit ? known_xenocrystal(known, player, source->endpoint.colony) : std::nullopt;
        if (fromOrbit) {
            const auto* colony = find_planet(known, source->endpoint.colony);
            resonanceDetails->setText(QString(
                "Resonance connects orbital gates. It requires Energy 18 / Electronics 18 / Construction 16. "
                "Source xenocrystals: %1; stock telemetry: %2.")
                .arg(observation ? QString("%1 units received; %2 units planned").arg(observation->site.surface).arg(colony->xenocrystal.surface) : "unknown")
                .arg(observation ? QString("year %1 (age %2)").arg(qulonglong(observation->observedTurn))
                    .arg(qulonglong(known.turn - observation->observedTurn)) : "none"));
        }
        summary->setText(QString("%1 — loaded gross mass %2 kt. %3\n%4")
            .arg(QString::fromStdString(fleet->name)).arg(fleet_gross_mass(known, *knownFleet), 0, 'f', 1)
            .arg(source ? source->limits : "No confirmed gate at this position")
            .arg(specialist ? "Stargate Specialist: cargo and colonists may travel aboard."
                : "Generalist: unload minerals, xenocrystals and colonists first."));
        targetPicker->clear();
        destinations.clear();
        QString mobileError;
        if (source && !alreadyQueued) for (const auto& target : gates) {
            if (&target == source) continue;
            if (source->endpoint.base || target.endpoint.base) {
                if (pulse || same_position(source->endpoint.position, target.endpoint.position)) continue;
                const auto error = mobile_gate_transit_error(known, *knownFleet, {source->endpoint, target.endpoint});
                if (!error.empty()) {
                    if (mobileError.isEmpty()) mobileError = QString::fromStdString(error);
                    continue;
                }
            } else {
                if (pulse && (!observation || observation->observedTurn != known.turn || !observation->stockPhase
                    || knownFleet->telemetry.observedTurn != known.turn)) continue;
                if (!stargate_transfer_error(known, player, fleet->id, source->endpoint.colony, target.endpoint.colony, pulse, delayed).empty()) continue;
                if (delayed) {
                    auto preview = known;
                    if (!preview_stargate_transfer(preview, player, {fleet->id, source->endpoint.colony, target.endpoint.colony, pulse})) continue;
                }
            }
            destinations.push_back(&target);
            targetPicker->addItem(target.name, uint(target.endpoint.colony ? target.endpoint.colony : target.endpoint.base));
            targetPicker->setItemData(targetPicker->count() - 1, target.limits, Qt::ToolTipRole);
        }
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!destinations.empty());
        if (alreadyQueued) status->setText("A transit is already queued for this fleet.");
        else if (destinations.empty()) status->setText(!mobileError.isEmpty() ? mobileError
            : pulse ? "No confirmed resonant destination. Check the source stock, range, mass and telemetry dates."
            : "No confirmed destination satisfies the gate rules.");
    };
    connect(targetPicker, &QComboBox::currentIndexChanged, &dialog, [&](int index) {
        if (index < 0) return;
        const auto& source = sources[sourcePicker->currentIndex()]->endpoint;
        const auto& target = destinations[index]->endpoint;
        if (source.base || target.base) status->setText(
            "Both gates must remain at their listed positions. Each mobile base spends 10 mg above its reserve. "
            "The fleet attempts the jump once after the command arrives, movement ends and battles resolve.");
        else status->setText(delayed ? "The fleet receives an orbital gate step and retries at the source until transit is possible."
            : "The host rechecks both orbital gates and any payment when resolving the turn.");
    });
    connect(sourcePicker, &QComboBox::currentIndexChanged, &dialog, refresh);
    connect(resonant, &QCheckBox::toggled, &dialog, refresh);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    refresh();
    if (dialog.exec() != QDialog::Accepted || targetPicker->currentIndex() < 0) return;
    const auto& source = sources[sourcePicker->currentIndex()]->endpoint;
    const auto& target = destinations[targetPicker->currentIndex()]->endpoint;
    if (source.base || target.base) {
        appendPendingOrder(MobileGateOrder{fleet->id, {source, target}}, QString("Transit fleet %1 to %2 through Expedition Stargate")
            .arg(QString::fromStdString(fleet->name), targetPicker->currentText()));
        return;
    }
    const GateFleetOrder order{fleet->id, source.colony, target.colony, resonant->isChecked()};
    if (delayed) {
        FleetArrivalAction action{FleetArrivalActionKind::TransitStargate};
        action.gateSource = source.colony;
        action.gateDestination = target.colony;
        action.gateResonant = order.resonant;
        appendPendingOrder(MoveFleetOrder{fleet->id, source.position, knownFleet->warp, action},
            QString("Deliver gate programme to %1; wait and retry at source").arg(QString::fromStdString(fleet->name)));
        return;
    }
    appendPendingOrder(order, QString("Transit fleet %1 through Jump Gate to %2%3")
        .arg(QString::fromStdString(fleet->name), targetPicker->currentText(),
            order.resonant ? " (8 xenocrystals; resonant range)" : ""));
}

} // namespace suns
