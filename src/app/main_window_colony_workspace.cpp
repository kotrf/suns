#include "main_window.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <array>

namespace suns {
namespace {

QDialog* detailsWindow(MainWindow* window, QWidget* content, const QString& title, const char* name)
{
    auto* dialog = new QDialog(window);
    dialog->setObjectName(name);
    dialog->setWindowTitle(title);
    dialog->resize(440, 580);
    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(content, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    return dialog;
}

void openDetails(QDialog* dialog)
{
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void removeFormField(QWidget* field, QWidget* panel)
{
    for (auto* form : panel->findChildren<QFormLayout*>()) {
        if (form->indexOf(field) < 0) continue;
        const auto row = form->takeRow(field);
        if (row.labelItem) {
            delete row.labelItem->widget();
            delete row.labelItem;
        }
        delete row.fieldItem;
        break;
    }
}

} // namespace

void MainWindow::installColonyWorkspace()
{
    if (findChild<QWidget*>("colonyStatusPanel")) return;
    auto* overview = findChild<QDockWidget*>("overviewDock");
    auto* production = findChild<QDockWidget*>("productionDock");
    if (!overview || !production || !productionQueueTree_) return;

    // Keep the existing live planet, environment and empire widgets in a
    // second dock column. Reparenting retains their refresh connections.
    auto* overviewDetails = overview->widget();
    overviewDetails->setParent(nullptr);
    auto* detailsDock = new QDockWidget("System Details", this);
    detailsDock->setObjectName("systemDetailsDock");
    detailsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    detailsDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    detailsDock->setMinimumWidth(240);
    detailsDock->setMaximumWidth(380);
    if (auto* scroll = qobject_cast<QScrollArea*>(overviewDetails)) {
        scroll->setObjectName("systemDetailsScrollArea");
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        if (auto* column = qobject_cast<QVBoxLayout*>(scroll->widget()->layout())) {
            if (auto* planetGroup = scroll->findChild<QGroupBox*>("planetGroup")) {
                column->removeWidget(planetGroup);
                column->insertWidget(0, planetGroup);
            }
            column->addStretch();
        }
    }
    detailsDock->setWidget(overviewDetails);
    addDockWidget(Qt::LeftDockWidgetArea, detailsDock);
    auto* statusPanel = new QWidget(overview);
    statusPanel->setObjectName("colonyStatusPanel");
    statusPanel->setMinimumWidth(300);
    auto* statusLayout = new QVBoxLayout(statusPanel);
    statusLayout->setContentsMargins(5, 5, 5, 5);
    statusLayout->setSpacing(5);
    auto* status = new QLabel(statusPanel);
    status->setObjectName("colonyStatusSummary");
    status->setTextFormat(Qt::RichText);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLayout->addWidget(status);

    auto* minerals = new QGroupBox("Minerals", statusPanel);
    auto* mineralLayout = new QVBoxLayout(minerals);
    mineralLayout->setContentsMargins(4, 5, 4, 4);
    auto* table = new QTreeWidget(minerals);
    table->setObjectName("colonyMineralsTable");
    table->setHeaderLabels({"Mineral", "Stock kt", "/ year", "Conc."});
    table->setRootIsDecorated(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setFixedHeight(92);
    table->header()->setSectionResizeMode(QHeaderView::Stretch);
    table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->setUniformRowHeights(true);
    const std::array colors{QColor("#68a9ff"), QColor("#79cb90"), QColor("#e4c367")};
    const std::array names{"Ironium", "Boranium", "Germanium"};
    for (int row = 0; row < 3; ++row) {
        auto* item = new QTreeWidgetItem(table);
        item->setText(0, names[row]);
        item->setForeground(0, colors[row]);
    }
    mineralLayout->addWidget(table);
    statusLayout->addWidget(minerals);
    statusLayout->addStretch();
    overview->setWidget(statusPanel);
    overview->setWindowTitle("System — Status & Minerals");

    auto* productionDetails = production->widget();
    if (auto* scroll = qobject_cast<QScrollArea*>(productionDetails)) productionDetails = scroll->takeWidget();
    productionDetails->setParent(nullptr);
    auto* productionDialog = detailsWindow(this, productionDetails, "Production details", "productionDetailsDialog");
    auto* root = new QWidget(production);
    root->setObjectName("colonyProductionPanel");
    root->setMinimumWidth(300);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(4);
    auto* queue = findChild<QGroupBox*>("productionQueueGroup");
    queue->setTitle("Production queue");
    queue->setParent(root);
    layout->addWidget(queue, 1);
    productionQueueSummary_->setProperty("compactColonySummary", true);
    productionQueueTree_->setMinimumHeight(96);
    productionQueueTree_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    productionQueueTree_->setHeaderLabels({"#", "Item", "Work", "ETA"});
    productionQueueTree_->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    productionQueueTree_->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    productionQueueTree_->setColumnWidth(2, 55);
    productionQueueTree_->setColumnWidth(3, 95);
    if (auto* oldLayout = qobject_cast<QVBoxLayout*>(productionDetails->layout())) {
        auto* summary = new QLabel(productionDetails);
        summary->setObjectName("productionExtendedSummary");
        summary->setWordWrap(true);
        oldLayout->insertWidget(0, summary);
        productionMineralDetails_->setParent(productionDetails);
        oldLayout->addWidget(productionMineralDetails_);
    }

    auto* builders = new QGroupBox("Add to queue", root);
    auto* builderLayout = new QVBoxLayout(builders);
    builderLayout->setContentsMargins(5, 5, 5, 5);
    builderLayout->setSpacing(3);
    removeFormField(shipDesignCombo_, productionDetails);
    auto* ships = new QHBoxLayout;
    ships->setSpacing(3);
    ships->addWidget(shipDesignCombo_, 1);
    buildShipButton_->setText("Ship");
    buildShipButton_->setProperty("compactColonyBuilder", true);
    buildShipButton_->setToolTip("Queue the selected ship design at this colony");
    ships->addWidget(buildShipButton_);
    builderLayout->addLayout(ships);
    auto* structures = new QHBoxLayout;
    structures->setSpacing(3);
    buildFactoryButton_->setText("Factory");
    buildOrbitalDockButton_->setText("Orbital dock");
    buildOrbitalDockButton_->setProperty("compactColonyBuilder", true);
    structures->addWidget(buildFactoryButton_);
    if (auto* mine = findChild<QPushButton*>("queueMineButton")) {
        mine->setText("Mine");
        structures->addWidget(mine);
    }
    structures->addWidget(buildOrbitalDockButton_);
    builderLayout->addLayout(structures);
    layout->addWidget(builders);
    auto* productionMore = new QPushButton("Production details…", root);
    productionMore->setObjectName("productionDetailsButton");
    connect(productionMore, &QPushButton::clicked, productionDialog, [productionDialog] { openDetails(productionDialog); });
    layout->addWidget(productionMore);
    if (auto* oldGroup = productionDetails->findChild<QGroupBox*>("productionGroup")) oldGroup->setTitle("Mining infrastructure");
    production->setWidget(root);
    production->setWindowTitle("Production");
    // Bottom reports occupy the center; neither the colony desk nor fleet
    // controls surrender their vertical space when a report is opened.
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    connect(this, &MainWindow::routeProgramContextChanged, this, [this] { refreshColonyWorkspace(); });
    auto* timer = new QTimer(statusPanel);
    timer->setInterval(150);
    connect(timer, &QTimer::timeout, statusPanel, [this] { refreshColonyWorkspace(); });
    timer->start();
    refreshColonyWorkspace();
    refreshProductionQueue();
}

void MainWindow::refreshColonyWorkspace()
{
    if (shuttingDown_) return;
    auto* status = findChild<QLabel*>("colonyStatusSummary");
    auto* table = findChild<QTreeWidget*>("colonyMineralsTable");
    if (!status || !table) return;
    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const bool own = star && planet && planet->owner == pendingOrders_.player
        && is_surveyed(state_, pendingOrders_.player, star->id);
    QStringList lines;
    if (!star) lines << "<b>No system selected</b>" << "Select a system on the map.";
    else {
        lines << QString("<b>%1</b>").arg(QString::fromStdString(star->name).toHtmlEscaped());
        const auto age = system_intel_age(state_, pendingOrders_.player, star->id);
        lines << (!age ? QString("Intel: never scanned") : *age == 0 ? QString("Intel: current")
            : QString("Intel: %1 year%2 old").arg(*age).arg(*age == 1 ? "" : "s"));
        if (planet) {
            const auto owner = known_planet_owner(state_, pendingOrders_.player, planet->id);
            const auto habitability = known_planet_habitability(state_, pendingOrders_.player, planet->id);
            const auto ownership = own ? QString("Your colony") : !owner ? QString("Owner unknown")
                : *owner == 0 ? QString("Uncolonized") : QString("Empire %1").arg(*owner);
            lines << ownership + (habitability ? QString(" • Hab %1%2%").arg(
                survey_level(state_, pendingOrders_.player, star->id) == SurveyLevel::BasicScan ? "~" : "").arg(*habitability) : "");
            if (own) {
                lines << QString("Population: %1 / %2").arg(planet->population).arg(population_capacity(state_, *planet, state_.turn));
                lines << QString("Factories: %1 • Mines: %2").arg(planet->industry).arg(planet->mines);
                lines << QString("Resources: %1 / year • Orbit: %2").arg(colony_output(*planet))
                    .arg(find_orbital_station_at_planet(state_, planet->id) ? "station" : "no station");
            }
        }
    }
    status->setText(lines.join("<br>"));
    const auto mining = own ? projected_mineral_mining(state_, *planet) : MineralCargo{};
    const bool geology = planet && planet_geology_known(state_, pendingOrders_.player, planet->id);
    const auto concentration = geology ? planet_mineral_concentration(state_, *planet) : MineralCargo{};
    const auto values = [](const MineralCargo& minerals) { return std::array{minerals.ironium, minerals.boranium, minerals.germanium}; };
    const auto stock = values(own ? planet->minerals : MineralCargo{});
    const auto extraction = values(mining);
    const auto conc = values(concentration);
    for (int row = 0; row < 3; ++row) {
        auto* item = table->topLevelItem(row);
        item->setText(1, own ? QString::number(stock[row], 'f', 1) : "—");
        item->setText(2, own ? QString::number(extraction[row], 'f', 1) : "—");
        item->setText(3, geology ? QString::number(conc[row], 'f', 0) + "%" : "?");
        for (int column = 0; column < 4; ++column)
            item->setToolTip(column, QString("%1 • Stock: %2 kt • Mining: %3 kt/year • Concentration: %4")
                .arg(item->text(0), item->text(1), item->text(2), item->text(3)));
    }
}

} // namespace suns
