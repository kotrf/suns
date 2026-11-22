#include "main_window.hpp"
#include "suns/mobile_base.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace suns {

namespace {

const QStringList attackPolicies{"Attack on contact", "Return fire only"};
const QStringList battleTargets{"Any target", "Armed ships", "Cargo ships", "Orbital stations"};
const QStringList battleTactics{"Approach", "Keep distance", "Escort transports", "Withdraw"};

QString battlePlanSummary(const FleetBattlePlan& plan)
{
    auto text = QString("%1 · %2 · %3")
        .arg(attackPolicies.at(plan.attackOnContact ? 0 : 1), battleTactics.at(int(plan.tactic)),
            battleTargets.at(int(plan.preferredTarget)));
    if (plan.preferredTarget != BattleTarget::Any && !plan.allowOtherTargets) text += " only";
    return text;
}

const SetFleetBattlePlanOrder* pendingBattlePlan(const PlayerOrders& orders, FleetId fleet)
{
    for (auto it = orders.orders.rbegin(); it != orders.orders.rend(); ++it)
        if (const auto* plan = std::get_if<SetFleetBattlePlanOrder>(&*it); plan && plan->fleet == fleet) return plan;
    return nullptr;
}

QString battlePlanStatus(const Fleet* received, const SetFleetBattlePlanOrder* pending)
{
    QString text;
    if (!received) text = "A new detachment inherits its source fleet’s plan.";
    else if (!received->telemetry.observedTurn) text = "Awaiting the first onboard plan report.";
    else text = QString("Reported in year %1 — %2").arg(qulonglong(received->telemetry.observedTurn))
        .arg(battlePlanSummary(received->battlePlan));
    if (pending) text += QString("\nQueued this year — %1").arg(battlePlanSummary(pending->plan));
    return text;
}

} // namespace

void MainWindow::installFleetOperationsPanel()
{
    auto* fleetDock = findChild<QDockWidget*>("fleetDock");
    auto* routeDock = findChild<QDockWidget*>("fleetRouteProgramDock");
    auto* fleetScroll = findChild<QScrollArea*>("fleetScrollArea");
    auto* routeScroll = findChild<QScrollArea*>("routeProgramScrollArea");
    if (!fleetDock || !routeDock || !fleetScroll || !routeScroll) return;

    auto* routePanel = routeScroll->takeWidget();
    auto* details = fleetScroll->takeWidget();
    auto* root = new QWidget(fleetDock);
    routePanel->setParent(root);
    details->setParent(root);
    details->setObjectName("fleetDetailsPanel");
    // QScrollArea::setWidget enabled this on the old scroll content. Once
    // nested in the route panel it must inherit the surrounding dark surface.
    details->setAutoFillBackground(false);
    root->setObjectName("fleetOperationsPanel");
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Fleet selection and telemetry stay outside the scrolling editor. Even on
    // a small screen, choosing a waypoint never hides fuel or cargo usage.
    auto* header = new QWidget(root);
    header->setObjectName("fleetOperationsHeader");
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(3);
    auto* source = findChild<QComboBox*>("routeSourceFleetCombo");
    for (auto* form : routePanel->findChildren<QFormLayout*>()) {
        if (form->indexOf(source) < 0) continue;
        const auto row = form->takeRow(source);
        if (row.labelItem) {
            delete row.labelItem->widget();
            delete row.labelItem;
        }
        delete row.fieldItem;
        break;
    }
    auto* sourceRow = new QHBoxLayout;
    sourceRow->setSpacing(4);
    sourceRow->addWidget(new QLabel("Fleet", header));
    sourceRow->addWidget(source, 1);
    source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    source->setMinimumContentsLength(12);
    if (auto* help = findChild<QToolButton*>("routeProgramHelpButton")) sourceRow->addWidget(help);
    headerLayout->addLayout(sourceRow);
    auto* fleetContext = new QLabel(selectedSystemFleetHeading(), header);
    fleetContext->setObjectName("fleetSystemContext");
    fleetContext->setTextFormat(Qt::PlainText);
    fleetContext->setWordWrap(true);
    headerLayout->addWidget(fleetContext);
    if (auto* heading = findChild<QLabel*>("routeProgramHeading")) heading->hide();

    auto* telemetryRow = new QHBoxLayout;
    telemetryRow->setSpacing(5);
    if (auto* portrait = findChild<QLabel*>("fleetPortrait")) telemetryRow->addWidget(portrait);
    auto* gauges = new QVBoxLayout;
    gauges->setSpacing(2);
    for (const auto* name : {"fleetFuelBar", "fleetCargoBar", "fleetDamageBar"}) {
        if (auto* gauge = findChild<QProgressBar*>(name)) gauges->addWidget(gauge);
    }
    telemetryRow->addLayout(gauges, 1);
    headerLayout->addLayout(telemetryRow);
    if (auto* comms = findChild<QLabel*>("fleetCommunicationSummary")) headerLayout->addWidget(comms);
    auto* actions = new QHBoxLayout;
    actions->setSpacing(4);
    if (auto* transfer = findChild<QPushButton*>("fleetTransferCargoButton")) actions->addWidget(transfer, 1);
    auto* detailsButton = new QToolButton(header);
    detailsButton->setObjectName("fleetDetailsButton");
    detailsButton->setText("Details / logistics");
    detailsButton->setCheckable(true);
    detailsButton->setArrowType(Qt::RightArrow);
    detailsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    detailsButton->setToolTip("Ship composition, dockside loading, rename, merge, split and colonize");
    actions->addWidget(detailsButton, 1);
    auto* battleButton = new QPushButton("Battle plan…", header);
    battleButton->setObjectName("fleetBattlePlanButton");
    actions->addWidget(battleButton);
    connect(battleButton, &QPushButton::clicked, this, &MainWindow::openFleetBattlePlan);
    const auto refreshBattleButton = [this, battleButton] {
        const auto* fleet = selectedFleet();
        battleButton->setEnabled(fleet != nullptr);
        battleButton->setToolTip(fleet ? battlePlanStatus(find_service_fleet(receivedPlayerState(), fleet->id),
            pendingBattlePlan(pendingOrders_, fleet->id)) : QString("Select one of your fleets"));
    };
    connect(this, &MainWindow::routeProgramContextChanged, battleButton, refreshBattleButton);
    refreshBattleButton();
    headerLayout->addLayout(actions);
    layout->addWidget(header);

    auto* editorScroll = new QScrollArea(root);
    editorScroll->setObjectName("fleetOrdersScrollArea");
    editorScroll->setWidgetResizable(true);
    editorScroll->setFrameShape(QFrame::NoFrame);
    editorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* routeLayout = qobject_cast<QVBoxLayout*>(routePanel->layout());
    routeLayout->insertWidget(routeLayout->count() - 1, details);
    details->hide();
    connect(detailsButton, &QToolButton::toggled, root, [details, detailsButton, editorScroll](bool expanded) {
        details->setVisible(expanded);
        detailsButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        if (expanded) editorScroll->ensureWidgetVisible(details);
    });
    editorScroll->setWidget(routePanel);
    layout->addWidget(editorScroll, 1);
    root->setStyleSheet(R"(
        QWidget#fleetOperationsPanel QProgressBar { min-height: 18px; max-height: 18px; }
        QWidget#fleetOperationsPanel QPushButton, QWidget#fleetOperationsPanel QToolButton {
            min-height: 18px; padding: 1px 5px;
        }
        QWidget#fleetOperationsPanel QComboBox, QWidget#fleetOperationsPanel QSpinBox {
            min-height: 18px; padding: 1px 4px;
        }
    )");
    fleetDock->setWindowTitle("Fleet — Orders & Logistics");
    connect(fleetDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible) cancelRouteProgramMapTargetPick();
    });
    fleetDock->setWidget(root);
    fleetDock->setMinimumWidth(360);
    fleetDock->setMaximumWidth(620);
    // The right fleet area uses the full window height beside bottom reports.
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    removeDockWidget(routeDock);
    delete routeDock;
    addDockWidget(Qt::RightDockWidgetArea, fleetDock);
    resizeDocks({fleetDock}, {400}, Qt::Horizontal);
}

void MainWindow::openFleetBattlePlan()
{
    const auto* fleet = selectedFleet();
    if (!fleet) return;
    const auto id = fleet->id;
    const auto name = QString::fromStdString(fleet->name);
    const auto* received = find_service_fleet(receivedPlayerState(), id);
    const auto* pending = pendingBattlePlan(pendingOrders_, id);
    const auto plan = pending ? pending->plan : received ? received->battlePlan : fleet->battlePlan;

    QDialog dialog(this);
    dialog.setObjectName("fleetBattlePlanDialog");
    dialog.setWindowTitle(QString("Battle plan — %1").arg(name));
    auto* layout = new QVBoxLayout(&dialog);
    auto* status = new QLabel(battlePlanStatus(received, pending), &dialog);
    status->setObjectName("fleetBattlePlanStatus");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto* form = new QFormLayout;
    auto* attack = new QComboBox(&dialog);
    attack->setObjectName("battleAttackCombo");
    attack->addItems(attackPolicies);
    attack->setCurrentIndex(plan.attackOnContact ? 0 : 1);
    attack->setToolTip("An armed fleet can start an engagement with an eligible target. Return fire only lets the fleet react when attacked.");
    form->addRow("On contact", attack);
    auto* target = new QComboBox(&dialog);
    target->setObjectName("battleTargetCombo");
    target->addItems(battleTargets);
    target->setCurrentIndex(int(plan.preferredTarget));
    target->setToolTip("Cargo ships include hulls and fittings with cargo capacity. Armed ships carry beams or torpedoes.");
    form->addRow("Preferred target", target);
    auto* fallback = new QComboBox(&dialog);
    fallback->setObjectName("battleTargetFallbackCombo");
    fallback->addItems({"Use other targets", "Engage preferred targets only"});
    fallback->setCurrentIndex(plan.allowOtherTargets ? 0 : 1);
    fallback->setEnabled(plan.preferredTarget != BattleTarget::Any);
    connect(target, &QComboBox::currentIndexChanged, fallback,
        [fallback](int index) { fallback->setEnabled(index != int(BattleTarget::Any)); });
    form->addRow("Other targets", fallback);
    auto* tactic = new QComboBox(&dialog);
    tactic->setObjectName("battleTacticCombo");
    tactic->addItems(battleTactics);
    tactic->setCurrentIndex(int(plan.tactic));
    tactic->setToolTip("Approach to weapon range, keep enemies at long range, screen nearby friendly cargo ships, or fly out of the combat area.");
    form->addRow("In battle", tactic);
    layout->addLayout(form);
    auto* delivery = new QLabel("The new plan takes effect when its order reaches the fleet.", &dialog);
    delivery->setWordWrap(true);
    layout->addWidget(delivery);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Queue plan");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    const FleetBattlePlan edited{attack->currentIndex() == 0, BattleTarget(target->currentIndex()),
        !fallback->isEnabled() || fallback->currentIndex() == 0, BattleTactic(tactic->currentIndex())};
    appendPendingOrder(SetFleetBattlePlanOrder{id, edited},
        QString("%1 — %2").arg(name, battlePlanSummary(edited)));
}

} // namespace suns
