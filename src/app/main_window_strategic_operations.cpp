#include "main_window.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/communications.hpp"

#include <QComboBox>
#include <QCheckBox>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

namespace suns {
void MainWindow::installStrategicOperations()
{
    auto* root = findChild<QWidget*>("fleetOperationsPanel");
    auto* layout = root ? qobject_cast<QVBoxLayout*>(root->layout()) : nullptr;
    if (!layout || findChild<QGroupBox*>("strategicOperationsGroup")) return;
    auto* group = new QGroupBox("Fleet mission / emissions", root);
    group->setObjectName("strategicOperationsGroup");
    auto* body = new QVBoxLayout(group); body->setContentsMargins(5,5,5,5); body->setSpacing(3);
    auto* row = new QHBoxLayout;
    auto* task = new QComboBox(group); task->setObjectName("strategicTaskCombo");
    for (int i = 0; i <= int(FleetTask::Salvage); ++i)
        task->addItem(QString::fromStdString(fleet_task_name(FleetTask(i))), i);
    auto* assign = new QPushButton("Assign", group); assign->setObjectName("assignStrategicTask");
    row->addWidget(task,1); row->addWidget(assign); body->addLayout(row);
    row = new QHBoxLayout;
    auto* mode = new QComboBox(group); mode->setObjectName("fleetEmissionModeCombo");
    mode->addItems({"Standard", "Passive", "Radio silence"});
    auto* years = new QSpinBox(group); years->setObjectName("radioSilenceYears");
    years->setRange(1,100); years->setValue(5); years->setSuffix(" yr");
    years->setToolTip("Radio silence ends automatically after this many years. Orders wait for the next transmission.");
    auto* setMode = new QPushButton("Set", group); setMode->setObjectName("setFleetEmissionMode");
    row->addWidget(mode,1); row->addWidget(years); row->addWidget(setMode); body->addLayout(row);
    auto* deception = new QCheckBox("Broadcast decoy signatures (jammer)",group);
    deception->setObjectName("fleetDeceptionCheck"); body->addWidget(deception);
    deception->setToolTip("Requires a jammer and Standard mode. Creates an ambiguous false signal area. Tachyon receivers can reject it; broadcasting may also expose your real emission area.");
    auto* summary = new QLabel(group); summary->setObjectName("strategicMissionSummary");
    summary->setWordWrap(true); summary->setTextFormat(Qt::PlainText); body->addWidget(summary);
    task->setToolTip("Stationary mission. Bombing requires bombs and an enemy colony in orbit. Mine laying requires a mine layer; sweeping uses beams. Field research needs a scanner; salvage also needs a cargo hold. Travel replaces the mission.");
    mode->setToolTip("Standard: active scans and jamming. Passive: listen, transmit every third year. Radio silence: listen only until automatic wake-up. Cloaks reduce hull detection; active emissions may still be intercepted.");
    layout->insertWidget(1,group);
    connect(assign,&QPushButton::clicked,this,[this,task] {
        const auto* fleet = selectedFleet(); if (!fleet || fleet->owner != pendingOrders_.player) return;
        const auto t = FleetTask(task->currentData().toInt());
        appendPendingOrder(SetFleetTaskOrder{fleet->id,t},QString("%1: %2").arg(QString::fromStdString(fleet->name),QString::fromStdString(fleet_task_name(t))));
    });
    connect(setMode,&QPushButton::clicked,this,[this,mode,years,deception] {
        const auto* fleet = selectedFleet(); if (!fleet || fleet->owner != pendingOrders_.player) return;
        ElectronicsProgram e{EmissionMode(mode->currentIndex()), mode->currentIndex() == 2 ? state_.turn + years->value() : 0};
        e.deception = mode->currentIndex() == 0 && deception->isChecked();
        appendPendingOrder(SetFleetElectronicsOrder{fleet->id,e},QString("%1: %2").arg(QString::fromStdString(fleet->name),mode->currentText()));
    });
    auto selected = std::make_shared<FleetId>(0);
    auto refresh = [this,task,assign,mode,years,setMode,summary,selected,deception] {
        if (shuttingDown_) return;
        const auto* fleet = selectedFleet();
        const bool owned = fleet && fleet->owner == pendingOrders_.player;
        task->setEnabled(owned); mode->setEnabled(owned); setMode->setEnabled(owned);
        years->setEnabled(owned && mode->currentIndex() == 2);
        if (!owned) { assign->setEnabled(false); deception->setEnabled(false); summary->setText("Select your fleet."); return; }
        const auto known = fleet_player_view(state_, *fleet);
        bool jammer = false;
        for (const auto& stack : fleet_ship_stacks(known)) if (const auto* design = find_ship_design(state_,stack.design))
            for (auto component : design->components) jammer |= component_spec(component).jammingPercent > 0;
        deception->setEnabled(jammer && mode->currentIndex() == 0);
        if (*selected != fleet->id) {
            *selected = fleet->id;
            task->setCurrentIndex(task->findData(int(known.task))); mode->setCurrentIndex(int(known.electronics.mode));
            deception->setChecked(known.electronics.deception);
        }
        const auto requested = FleetTask(task->currentData().toInt());
        const bool available = requested == FleetTask::RemoteMining ? fleet_can_remote_mine(state_, known)
            : strategic_task_available(state_,known,requested);
        assign->setEnabled(available);
        QString text = QString("Mission: %1 • Cloak %2% • %3")
            .arg(QString::fromStdString(fleet_task_name(known.task)))
            .arg(fleet_cloak_fraction(state_,known)*100,0,'f',0)
            .arg(known.electronics.mode == EmissionMode::Standard ? "Active scans" : "Passive listening");
        if (known.electronics.resumeTurn) text += QString(" • wake year %1").arg(qulonglong(known.electronics.resumeTurn));
        if (requested == FleetTask::FieldResearch)
            text += "\nOne study per empire and target: star → Energy 12; deep planet → Biology 12 (artifact site 32); WH within 10 ly → Propulsion 24. Rewards arrive with the report.";
        if (requested == FleetTask::Salvage) text += "\nWithin 10 ly of known wreckage; only unfamiliar technology yields science. The specimen is consumed.";
        if (!available) text += "\nRequires suitable equipment and an idle route. Stop travel first.";
        summary->setText(text);
    };
    auto* timer = new QTimer(this); timer->setSingleShot(true);
    connect(timer,&QTimer::timeout,this,refresh);
    connect(scene_,&QGraphicsScene::changed,this,[this,timer](const QList<QRectF>&) { if (!shuttingDown_) timer->start(0); });
    connect(task,&QComboBox::currentIndexChanged,this,[timer] { timer->start(0); });
    connect(mode,&QComboBox::currentIndexChanged,this,[timer] { timer->start(0); });
    refresh();
}

void MainWindow::renderStrategicObjects()
{
    const auto* player = find_player(state_,pendingOrders_.player); if (!player) return;
    for (const auto& intel : player->strategicIntel) {
        if (intel.type == StrategicObjectKind::Emission && intel.deliveryTurn != state_.turn) continue;
        if (intel.type == StrategicObjectKind::Wreck && intel.quantity <= state_.turn) continue;
        QColor color = intel.type == StrategicObjectKind::Emission ? QColor("#d2a963")
            : intel.type == StrategicObjectKind::Wreck ? QColor("#b38de0")
            : intel.owner == player->id ? QColor("#74c795") : QColor("#d97979");
        QPen pen(color); pen.setCosmetic(true); pen.setStyle(Qt::DashLine); pen.setWidthF(0.8);
        const double radius = std::max(2.0,intel.radius);
        auto* circle = scene_->addEllipse(intel.position.x-radius,intel.position.y-radius,radius*2,radius*2,pen);
        circle->setZValue(-12); circle->setAcceptedMouseButtons(Qt::NoButton);
        circle->setToolTip(intel.type == StrategicObjectKind::Emission ? QString("Unidentified emission area, uncertainty %1 ly; observed year %2").arg(intel.radius).arg(qulonglong(intel.observedTurn))
            : intel.type == StrategicObjectKind::Wreck ? QString("Wreck %1 — recover within 10 ly; expires year %2").arg(intel.id).arg(intel.quantity,0,'f',0)
            : QString("%1 minefield %2 — %3 mines, safe Warp %4; observed year %5").arg(intel.kind == 1 ? "Heavy" : intel.kind == 2 ? "Speed trap" : "Standard")
                .arg(intel.id).arg(intel.quantity,0,'f',0).arg(minefield_safe_warp(intel.kind)).arg(qulonglong(intel.observedTurn)));
    }
}
}
