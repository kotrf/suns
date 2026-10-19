#include "main_window.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/stations.hpp"
#include "suns/survey_probes.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace suns {
namespace {
ProbeProgram draft(MainWindow& w)
{
    ProbeProgram p;
    p.center = {w.findChild<QDoubleSpinBox*>("probeCenterX")->value(), w.findChild<QDoubleSpinBox*>("probeCenterY")->value()};
    p.radius = w.findChild<QDoubleSpinBox*>("probeRadius")->value();
    p.depth = SurveyLevel(w.findChild<QComboBox*>("probeDepth")->currentData().toUInt());
    p.flight = ProbeFlightMode(w.findChild<QComboBox*>("probeFlight")->currentData().toUInt());
    p.maxWarp = std::uint8_t(w.findChild<QSpinBox*>("probeWarp")->value());
    p.fuelReserve = w.findChild<QDoubleSpinBox*>("probeReserve")->value();
    p.maxSystems = w.findChild<QSpinBox*>("probeSystems")->value(); p.maxTurns = w.findChild<QSpinBox*>("probeYears")->value();
    p.returnHome = w.findChild<QCheckBox*>("probeReturn")->isChecked(); return p;
}
}
void MainWindow::installSurveyProbes()
{
    if (findChild<QDockWidget*>("surveyProbesDock")) return;
    auto* dock = new QDockWidget("Autonomous survey probes", this); dock->setObjectName("surveyProbesDock");
    auto* body = new QWidget(dock); auto* layout = new QVBoxLayout(body);
    auto* hint = new QLabel("Construction 16 + Electronics 18; Survey Probe hull and Survey Autopilot. "
        "Choose a circular region and desired survey. Each orbital survey stage takes a normal annual observation. "
        "Programme, stop, resume and knowledge uploads travel with ordinary communication delay. "
        "The probe keeps working while disconnected. Configure starts a new mission with the receipt position as home; "
        "resume keeps its progress and limits. Knowledge uploads do not resume a paused mission.", body);
    hint->setWordWrap(true); layout->addWidget(hint);
    layout->addWidget(new QLabel("Programme draft (load received settings or edit, then send):", body));
    auto* form = new QFormLayout;
    auto* fleet = new QComboBox(body); fleet->setObjectName("probeFleet"); form->addRow("Received probe fleet", fleet);
    auto* colony = new QComboBox(body); colony->setObjectName("probeColony"); form->addRow("Build at colony", colony);
    const auto real = [&](const char* name, const char* label, double min, double max, double value) {
        auto* control = new QDoubleSpinBox(body); control->setObjectName(name); control->setRange(min, max); control->setValue(value); form->addRow(label, control); return control;
    };
    real("probeCenterX", "Region center X", -1e6, 1e6, 0); real("probeCenterY", "Region center Y", -1e6, 1e6, 0);
    real("probeRadius", "Region radius (ly)", 1, 1e6, 300);
    auto* depth = new QComboBox(body); depth->setObjectName("probeDepth");
    depth->addItem("Orbital survey", unsigned(SurveyLevel::OrbitalSurvey)); depth->addItem("Geological survey", unsigned(SurveyLevel::GeologicalSurvey));
    depth->addItem("Deep survey", unsigned(SurveyLevel::DeepSurvey)); depth->setCurrentIndex(2); form->addRow("Desired survey", depth);
    auto* flight = new QComboBox(body); flight->setObjectName("probeFlight");
    flight->addItem("Safe cruise (no overdrive)", unsigned(ProbeFlightMode::SafeCruise)); flight->addItem("Economical (least fuel)", unsigned(ProbeFlightMode::Economical)); form->addRow("Flight mode", flight);
    const auto integer = [&](const char* name, const char* label, int max, int value) {
        auto* control = new QSpinBox(body); control->setObjectName(name); control->setRange(1, max); control->setValue(value); form->addRow(label, control);
    };
    integer("probeWarp", "Maximum Warp", kMaxWarp, 6); real("probeReserve", "Extra fuel reserve", 0, 1e9, 20);
    integer("probeSystems", "Completed targets limit", 1000, 12); integer("probeYears", "Mission years limit", 10000, 100);
    auto* home = new QCheckBox("Return to launch position on completion / fuel limit", body); home->setObjectName("probeReturn"); home->setChecked(true); form->addRow(home);
    layout->addLayout(form);
    auto* row = new QHBoxLayout;
    const auto button = [&](const char* name, const char* label) { auto* b = new QPushButton(label, body); b->setObjectName(name); row->addWidget(b); return b; };
    auto* build = button("buildSurveyProbe", "Build basic probe"); auto* load = button("loadProbeProgram", "Load received programme");
    auto* send = button("sendProbeProgram", "Send new programme"); auto* stop = button("stopProbeProgram", "Send stop");
    auto* resume = button("resumeProbeProgram", "Send resume"); auto* upload = button("uploadProbeKnowledge", "Upload received surveys"); layout->addLayout(row);
    auto* status = new QLabel(body); status->setObjectName("probeMissionStatus"); status->setWordWrap(true); layout->addWidget(status);
    auto* reports = new QTreeWidget(body); reports->setObjectName("probeReceivedSurveys"); reports->setHeaderLabels({"Received system", "Survey level", "Observation year"}); layout->addWidget(reports);
    dock->setWidget(body); addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide(); menuBar()->addAction(dock->toggleViewAction());
    const auto command = [this, fleet](ProbeAction action) {
        const auto id = fleet->currentData().toUInt(); if (!id) return;
        const auto programme = draft(*this); if (!probe_program_valid(programme)) return;
        const auto label = action == ProbeAction::Configure ? "Configure" : action == ProbeAction::Stop ? "Stop"
            : action == ProbeAction::Resume ? "Resume" : "Upload HQ surveys";
        appendPendingOrder(SetProbeProgramOrder{id, action, programme}, QString("%1 probe #%2 (delayed command)").arg(label).arg(id)); rebuildScene();
    };
    connect(send, &QPushButton::clicked, this, [command] { command(ProbeAction::Configure); });
    connect(stop, &QPushButton::clicked, this, [command] { command(ProbeAction::Stop); });
    connect(resume, &QPushButton::clicked, this, [command] { command(ProbeAction::Resume); });
    connect(upload, &QPushButton::clicked, this, [command] { command(ProbeAction::UpdateKnowledge); });
    connect(load, &QPushButton::clicked, this, [this, fleet] {
        const auto view = make_player_view(state_, pendingOrders_.player).state;
        const auto f = std::find_if(view.fleets.begin(), view.fleets.end(), [&](const auto& f) { return f.id == fleet->currentData().toUInt(); });
        if (f == view.fleets.end()) return;
        auto p = f->probe.program; if (!f->probe.configured) p.center = f->position;
        findChild<QDoubleSpinBox*>("probeCenterX")->setValue(p.center.x); findChild<QDoubleSpinBox*>("probeCenterY")->setValue(p.center.y);
        findChild<QDoubleSpinBox*>("probeRadius")->setValue(p.radius); findChild<QDoubleSpinBox*>("probeReserve")->setValue(p.fuelReserve);
        findChild<QComboBox*>("probeDepth")->setCurrentIndex(findChild<QComboBox*>("probeDepth")->findData(unsigned(p.depth)));
        findChild<QComboBox*>("probeFlight")->setCurrentIndex(int(p.flight)); findChild<QSpinBox*>("probeWarp")->setValue(p.maxWarp);
        findChild<QSpinBox*>("probeSystems")->setValue(p.maxSystems); findChild<QSpinBox*>("probeYears")->setValue(p.maxTurns); findChild<QCheckBox*>("probeReturn")->setChecked(p.returnHome);
    });
    connect(build, &QPushButton::clicked, this, [this, colony] {
        const auto view = make_player_view(state_, pendingOrders_.player).state;
        auto design = basic_survey_probe_design(pendingOrders_.player, view.nextShipDesignId);
        if (!colony->currentData().toUInt() || !ship_design_available_to_player(view, pendingOrders_.player, design)) return;
        auto existing = std::find_if(view.shipDesigns.begin(), view.shipDesigns.end(), [&](const auto& d) { return d.name == design.name; });
        for (int suffix = 2; existing != view.shipDesigns.end(); ++suffix) {
            Fleet candidate; candidate.owner = pendingOrders_.player; candidate.design = existing->id;
            if (probe_capable(view, candidate) && ship_design_available_to_player(view, pendingOrders_.player, *existing)) break;
            design.name = "Autonomous Survey Probe " + std::to_string(suffix);
            existing = std::find_if(view.shipDesigns.begin(), view.shipDesigns.end(), [&](const auto& d) { return d.name == design.name; });
        }
        if (existing != view.shipDesigns.end() && existing->hull == ShipHullType::SurveyProbe) {
            appendPendingOrder(QueueShipDesignOrder{colony->currentData().toUInt(), existing->id}, "Build survey probe");
        } else if (existing == view.shipDesigns.end()) {
            const bool created = std::any_of(pendingOrders_.orders.begin(), pendingOrders_.orders.end(), [&](const auto& o) {
                const auto* create = std::get_if<CreateShipDesignOrder>(&o); return create && create->name == design.name;
            });
            if (!created) appendPendingOrder(CreateShipDesignOrder{design.name, design.hull, design.components}, "Create basic survey probe design");
            appendPendingOrder(QueueShipDesignOrder{colony->currentData().toUInt(), 0, design.name}, "Build survey probe");
        }
        rebuildScene();
    });
    connect(fleet, &QComboBox::currentIndexChanged, this, [this](int) { refreshSurveyProbes(); });
    connect(this, &MainWindow::routeProgramContextChanged, this, [this] { refreshSurveyProbes(); }); refreshSurveyProbes();
}
void MainWindow::refreshSurveyProbes()
{
    auto* combo = findChild<QComboBox*>("probeFleet"); if (!combo || !find_player(state_, pendingOrders_.player)) return;
    const auto view = make_player_view(state_, pendingOrders_.player).state;
    const QSignalBlocker guard(combo); const auto old = combo->currentData(); combo->clear();
    for (const auto& f : view.fleets) if (f.probe.configured || probe_capable(view, f)) combo->addItem(QString::fromStdString(f.name), f.id);
    if (combo->findData(old) >= 0) combo->setCurrentIndex(combo->findData(old));
    auto* colony = findChild<QComboBox*>("probeColony"); const QSignalBlocker colonyGuard(colony); const auto oldColony = colony->currentData(); colony->clear();
    for (const auto& p : view.planets) if (p.owner == pendingOrders_.player && colony_has_orbital_service(view, p.id, p.owner, OrbitalStationModule::Shipyard)) colony->addItem(QString::fromStdString(p.name), p.id);
    if (colony->findData(oldColony) >= 0) colony->setCurrentIndex(colony->findData(oldColony));
    const auto f = std::find_if(view.fleets.begin(), view.fleets.end(), [&](const auto& f) { return f.id == combo->currentData().toUInt(); });
    const bool exists = f != view.fleets.end(), capable = exists && probe_capable(view, *f);
    findChild<QPushButton*>("buildSurveyProbe")->setEnabled(colony->count() && ship_design_available_to_player(view, pendingOrders_.player, basic_survey_probe_design(pendingOrders_.player, view.nextShipDesignId)));
    findChild<QPushButton*>("sendProbeProgram")->setEnabled(capable);
    findChild<QPushButton*>("loadProbeProgram")->setEnabled(exists);
    for (const auto* name : {"stopProbeProgram", "resumeProbeProgram", "uploadProbeKnowledge"}) findChild<QPushButton*>(name)->setEnabled(exists && f->probe.configured && (capable || QString(name) == "stopProbeProgram"));
    auto* status = findChild<QLabel*>("probeMissionStatus");
    if (exists) {
        QString target = f->probe.target ? QString("#%1").arg(f->probe.target) : "None";
        if (const auto* star = find_star(view, f->probe.target)) target = QString::fromStdString(star->name);
        status->setText(QString("Received telemetry: year %1 (age %2). Last reported target: %3. State: %4. Completed targets: %5.\n"
            "Position estimate: (%6, %7). Future autonomous choices are not predicted.\nSafe fuel planning includes each annual leg, extra reserve and return to launch position when enabled.")
            .arg(qulonglong(f->telemetry.observedTurn)).arg(qulonglong(view.turn - std::min(view.turn, f->telemetry.observedTurn)))
            .arg(target).arg(QString::fromStdString(probe_status_name(f->probe.status))).arg(f->probe.completed.size()).arg(f->position.x).arg(f->position.y));
    } else status->setText("Build a probe, then choose it after its manifest arrives. Reports below are delivered HQ knowledge; onboard notebooks are private until ordinary reports arrive.");
    auto* reports = findChild<QTreeWidget*>("probeReceivedSurveys"); reports->clear();
    for (const auto& k : view.players[0].surveyKnowledge) if (k.level >= SurveyLevel::OrbitalSurvey) {
        const auto* star = find_star(view, k.star); auto* item = new QTreeWidgetItem(reports);
        item->setText(0, star ? QString::fromStdString(star->name) : QString::number(k.star));
        item->setText(1, k.level == SurveyLevel::DeepSurvey ? "Deep survey"
            : k.level == SurveyLevel::GeologicalSurvey ? "Geological survey" : "Orbital survey");
        item->setText(2, QString::number(qulonglong(k.observedTurn)));
    }
}
}
