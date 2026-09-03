#include "main_window.hpp"

#include "suns/communications.hpp"

#include <QGraphicsScene>
#include <QGroupBox>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace suns {
namespace {

QProgressBar* makeFleetGauge(const char* objectName, const QString& tooltip, QWidget* parent)
{
    auto* bar = new QProgressBar(parent);
    bar->setObjectName(objectName);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(true);
    bar->setToolTip(tooltip);
    bar->setFormat("No fleet selected");
    bar->setEnabled(false);
    return bar;
}

int percentOf(double used, double capacity)
{
    if (capacity <= 0.000001) return 0;
    return std::clamp(static_cast<int>(std::lround(100.0 * used / capacity)), 0, 100);
}

} // namespace

void MainWindow::installFleetReadabilityPolish()
{
    // View is a frequently used interface menu; keep Help last in the familiar
    // desktop ordering even though Help is created by the later planet-polish
    // module and View by the panel-layout module.
    auto* viewMenu = menuBar()->findChild<QMenu*>("sunsViewMenu");
    auto* helpMenu = menuBar()->findChild<QMenu*>("sunsHelpMenu");
    if (viewMenu && helpMenu) {
        menuBar()->removeAction(viewMenu->menuAction());
        menuBar()->insertMenu(helpMenu->menuAction(), viewMenu);
    }

    QProgressBar* fuelBar = nullptr;
    QProgressBar* cargoBar = nullptr;
    if (auto* fleetGroup = findChild<QGroupBox*>("fleetGroup")) {
        if (auto* layout = qobject_cast<QVBoxLayout*>(fleetGroup->layout())) {
            fuelBar = makeFleetGauge(
                "fleetFuelBar",
                "Fuel aboard versus the fitted ship design's tank capacity.",
                fleetGroup);
            cargoBar = makeFleetGauge(
                "fleetCargoBar",
                "Shared cargo-hold usage. Colonists and Ironium/Boranium/Germanium all occupy this same capacity.",
                fleetGroup);
            layout->insertWidget(1, fuelBar);
            layout->insertWidget(2, cargoBar);
        }
    }

    // Finish the dark theme for controls that use platform popup/palette paths.
    // In particular QComboBox popups were inheriting a light list background,
    // while their item text still came from the dark application palette.
    setStyleSheet(styleSheet() + R"(
        QScrollBar:horizontal {
            height: 10px;
            margin: 0;
            background: #0c131c;
        }
        QScrollBar::handle:horizontal {
            min-width: 28px;
            border-radius: 4px;
            background: #344b61;
        }
        QScrollBar::handle:horizontal:hover {
            background: #496b89;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        QProgressBar {
            min-height: 20px;
            text-align: center;
            color: #edf5fc;
            background: #0a1119;
            border: 1px solid #2c4257;
            border-radius: 3px;
        }
        QProgressBar::chunk {
            border-radius: 2px;
            background: #438cb9;
        }
        QProgressBar:disabled {
            color: #91a2b3;
            border-color: #263645;
            background: #0d141d;
        }
        QProgressBar#ironiumConcentration::chunk { background: #3f93d1; }
        QProgressBar#boraniumConcentration::chunk { background: #4ea96d; }
        QProgressBar#germaniumConcentration::chunk { background: #b99532; }
        QProgressBar#fleetFuelBar::chunk { background: #3e9dc2; }
        QProgressBar#fleetCargoBar::chunk { background: #b77d42; }
        QProgressBar#planetPopulationBar::chunk { background: #56a875; }
        QProgressBar#planetTemperatureBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4b9bd5, stop:0.5 #62b879, stop:1 #d66c55);
        }
        QProgressBar#planetTemperatureBar, QProgressBar#planetGravityBar,
        QProgressBar#planetRadiationBar {
            min-height: 9px;
            max-height: 9px;
        }
        QProgressBar#planetGravityBar::chunk { background: #8b79c8; }
        QProgressBar#planetRadiationBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #65ad72, stop:0.55 #d2b94f, stop:1 #d35c50);
        }

        QDialog, QMessageBox {
            background: #0d141d;
            color: #dce7f2;
        }
        QDialog QLabel, QDialog QCheckBox, QDialog QRadioButton,
        QMessageBox QLabel {
            color: #dce7f2;
            background: transparent;
        }
        QDialog QLabel:disabled, QDialog QCheckBox:disabled,
        QDialog QRadioButton:disabled {
            color: #8da0b3;
        }
        QListView, QListWidget, QTreeView, QTableView, QTextBrowser,
        QComboBox QAbstractItemView {
            color: #e7f0f8;
            background: #101925;
            alternate-background-color: #14202c;
            border: 1px solid #355068;
            outline: 0;
            selection-color: #ffffff;
            selection-background-color: #315f88;
        }
        QAbstractItemView {
            gridline-color: #2b4155;
        }
        QHeaderView {
            color: #f1f6fb;
            background: #111a25;
        }
        QHeaderView::section {
            padding: 4px 6px;
            color: #f1f6fb;
            background: #1c2c3b;
            border: 0;
            border-right: 1px solid #344d63;
            border-bottom: 1px solid #4c6d87;
            font-weight: 700;
        }
        QTableCornerButton::section {
            background: #1c2c3b;
            border: 0;
            border-right: 1px solid #344d63;
            border-bottom: 1px solid #4c6d87;
        }
        QListView::item, QListWidget::item, QTreeView::item, QTableView::item {
            min-height: 20px;
            padding: 1px 4px;
        }
        QComboBox QAbstractItemView::item {
            min-height: 24px;
            padding: 3px 6px;
        }
        QComboBox QAbstractItemView::item:hover {
            color: #ffffff;
            background: #27455f;
        }
        QComboBox QAbstractItemView::item:selected {
            color: #ffffff;
            background: #315f88;
        }
        QComboBox:disabled, QLineEdit:disabled,
        QSpinBox:disabled, QDoubleSpinBox:disabled {
            color: #8598ab;
            background: #101720;
            border-color: #223342;
        }
    )");

    if (!fuelBar || !cargoBar) return;

    const auto refresh = [this, fuelBar, cargoBar] {
        if (shuttingDown_) return;
        const auto visibleFleetStorage = selectedFleetPlanningView();
        const auto* fleet = visibleFleetStorage ? &*visibleFleetStorage : nullptr;
        if (!fleet) {
            for (auto* bar : {fuelBar, cargoBar}) {
                bar->setValue(0);
                bar->setEnabled(false);
                bar->setFormat("No fleet selected");
            }
            return;
        }

        const double fuelCapacity = fleet_fuel_capacity(state_, *fleet);
        fuelBar->setEnabled(fuelCapacity > 0.000001);
        fuelBar->setValue(percentOf(fleet->fuel, fuelCapacity));
        if (fuelCapacity > 0.000001) {
            fuelBar->setFormat(
                QString("Fuel %1 / %2 • %3%")
                    .arg(fleet->fuel, 0, 'f', 1)
                    .arg(fuelCapacity, 0, 'f', 1)
                    .arg(percentOf(fleet->fuel, fuelCapacity)));
        } else {
            fuelBar->setFormat("Fuel — no tank capacity");
        }
        fuelBar->setToolTip(
            QString("Fuel aboard: %1 / %2\nFleet gross mass: %3 kt")
                .arg(fleet->fuel, 0, 'f', 1)
                .arg(fuelCapacity, 0, 'f', 1)
                .arg(fleet_gross_mass(state_, *fleet), 0, 'f', 1));

        const double cargoCapacity = fleet_cargo_capacity(state_, *fleet);
        const double cargoUsed = fleet_cargo_used(state_, *fleet);
        cargoBar->setEnabled(cargoCapacity > 0.000001);
        cargoBar->setValue(percentOf(cargoUsed, cargoCapacity));
        if (cargoCapacity > 0.000001) {
            QString format = QString("Cargo %1 / %2 • %3%")
                    .arg(cargoUsed, 0, 'f', 1)
                    .arg(cargoCapacity, 0, 'f', 1)
                    .arg(percentOf(cargoUsed, cargoCapacity));
            if (fleet->colonists > 0) {
                format += QString(" • %1 colonists").arg(static_cast<qulonglong>(fleet->colonists));
            }
            cargoBar->setFormat(format);
        } else {
            cargoBar->setFormat("Cargo hold — no cargo capacity");
        }
        cargoBar->setToolTip(
            QString("Shared hold: %1 / %2 cargo units\nColonists: %3\nMinerals: I %4 / B %5 / G %6")
                .arg(cargoUsed, 0, 'f', 1)
                .arg(cargoCapacity, 0, 'f', 1)
                .arg(static_cast<qulonglong>(fleet->colonists))
                .arg(fleet->minerals.ironium, 0, 'f', 1)
                .arg(fleet->minerals.boranium, 0, 'f', 1)
                .arg(fleet->minerals.germanium, 0, 'f', 1));
    };

    auto* refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    refreshTimer->setInterval(0);
    connect(refreshTimer, &QTimer::timeout, this, refresh);
    connect(scene_, &QGraphicsScene::changed, this, [this, refreshTimer](const QList<QRectF>&) {
        if (!shuttingDown_) refreshTimer->start();
    });
    connect(scene_, &QGraphicsScene::selectionChanged, this, [this, refreshTimer] {
        if (!shuttingDown_) refreshTimer->start();
    });
    refresh();
}

} // namespace suns
