#include "main_window.hpp"

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
    detailsButton->setText("Details & logistics");
    detailsButton->setCheckable(true);
    detailsButton->setArrowType(Qt::RightArrow);
    detailsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    detailsButton->setToolTip("Ship composition, dockside loading, rename, merge, split and colonize");
    actions->addWidget(detailsButton, 1);
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
            min-height: 20px; padding: 2px 5px;
        }
        QWidget#fleetOperationsPanel QComboBox, QWidget#fleetOperationsPanel QSpinBox {
            min-height: 20px; padding: 1px 4px;
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

} // namespace suns
