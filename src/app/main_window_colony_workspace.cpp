#include "suns/orbital_habitats.hpp"
#include "main_window.hpp"
#include "mineral_stock_panel.hpp"
#include "station_details_dialog.hpp"
#include "suns/campaign.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include "suns/planetary_recovery.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QTreeWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>

namespace suns {
namespace {

void fitMineralRows(QTreeWidget* table)
{
    table->ensurePolished();
    table->header()->ensurePolished();
    int height = 2 * table->frameWidth();
    if (!table->header()->isHidden()) height += table->header()->sizeHint().height();
    for (int row = 0; row < table->topLevelItemCount(); ++row)
        height += table->sizeHintForRow(row);
    if (table->minimumHeight() != height || table->maximumHeight() != height)
        table->setFixedHeight(height);
}

class MineralTable final : public QTreeWidget {
public:
    explicit MineralTable(QWidget* parent) : QTreeWidget(parent), fitTimer_(this)
    {
        fitTimer_.setSingleShot(true);
        QObject::connect(&fitTimer_, &QTimer::timeout, this, [this] {
            doItemsLayout();
            fitMineralRows(this);
        });
    }
protected:
    bool event(QEvent* event) override
    {
        const bool result = QTreeWidget::event(event);
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Resize:
        case QEvent::FontChange:
        case QEvent::StyleChange:
            // Wait for font/style propagation and layout, without rereading
            // game state or repeatedly polling a hidden panel.
            fitTimer_.start(0);
            break;
        default: break;
        }
        return result;
    }
private:
    QTimer fitTimer_;
};

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
    if (auto* group = detailsDock->findChild<QGroupBox*>("planetGroup")) {
        auto* header = new QWidget(group);
        header->setObjectName("systemPlanetHeader");
        auto* headerLayout = new QGridLayout(header);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(0);
        auto* station = new QToolButton(header);
        station->setObjectName("systemStationButton");
        station->setToolButtonStyle(Qt::ToolButtonIconOnly);
        station->setIconSize(QSize(24, 24));
        station->setFixedSize(28, 28);
        station->setAutoRaise(true);
        station->setAccessibleName("Orbital station details");
        station->hide();
        connect(station, &QToolButton::clicked, this, &MainWindow::openStationDetails);
        if (auto* column = qobject_cast<QVBoxLayout*>(group->layout())) {
            if (auto* portrait = group->findChild<QLabel*>("planetPortrait")) {
                column->removeWidget(portrait);
                headerLayout->addWidget(portrait, 0, 0, Qt::AlignHCenter);
            }
            headerLayout->addWidget(station, 0, 0, Qt::AlignTop | Qt::AlignRight);
            column->insertWidget(0, header);
        }
    }
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
    auto* table = new MineralTable(minerals);
    table->setObjectName("colonyMineralsTable");
    table->setHeaderLabels({"Mineral", "Stock kt", "/ year", "Conc."});
    table->setRootIsDecorated(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    fitMineralRows(table);
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
    refreshColonyWorkspace();
    refreshProductionQueue();
}

void MainWindow::refreshColonyWorkspace()
{
    if (shuttingDown_) return;
    auto* status = findChild<QLabel*>("colonyStatusSummary");
    auto* table = findChild<QTreeWidget*>("colonyMineralsTable");
    if (!status || !table) return;
    const auto& received = receivedPlayerState();
    const auto& planned = fleetPlanningState();
    const auto* star = selectedStar();
    const auto* observed = star ? find_planet_at_star(received, star->id) : nullptr;
    if (auto* button = findChild<QToolButton*>("systemStationButton")) {
        const auto* station = observed ? find_orbital_station_at_planet(received, observed->id) : nullptr;
        button->setVisible(station != nullptr);
        if (station) {
            button->setIcon(stationIcon(station->hull));
            button->setText(QString::fromUtf8(station_hull_spec(station->hull).name) + "…");
            button->setToolTip(QString("%1 — %2\nOpen received station information and fitting")
                .arg(QString::fromStdString(station->name), QString::fromUtf8(station_hull_spec(station->hull).name)));
        }
    }
    const auto* planet = observed && observed->owner == pendingOrders_.player
        ? find_planet(planned, observed->id) : observed;
    const bool own = star && planet && planet->owner == pendingOrders_.player
        && is_surveyed(received, pendingOrders_.player, star->id);
    if (!own) status->setToolTip({});
    QStringList lines;
    if (!star) lines << "<b>No system selected</b>" << "Select a system on the map.";
    else {
        lines << QString("<b>%1</b>").arg(QString::fromStdString(star->name).toHtmlEscaped());
        const auto age = system_intel_age(received, pendingOrders_.player, star->id);
        lines << (!age ? QString("Intel: never scanned") : *age == 0 ? QString("Intel: current")
            : QString("Intel: %1 year%2 old").arg(*age).arg(*age == 1 ? "" : "s"));
        if (planet) {
            const auto owner = known_planet_owner(received, pendingOrders_.player, planet->id);
            const auto habitability = known_planet_habitability(received, pendingOrders_.player, planet->id);
            const auto ownership = own ? QString("Your colony") : !owner ? QString("Owner unknown")
                : *owner == 0 ? QString("Uncolonized") : QString("Empire %1").arg(*owner);
            lines << ownership + (habitability ? QString(" • Hab %1%2%").arg(
                survey_level(received, pendingOrders_.player, star->id) == SurveyLevel::BasicScan ? "~" : "").arg(*habitability) : "");
            if (own) {
                lines << QString("Population: %1 / %2%3").arg(planet->population)
                    .arg(population_capacity(planned, *planet, received.turn))
                    .arg(planet->population != observed->population ? " (after cargo)" : "");
                if(habitat_completed(*planet)) lines << QString("Orbit: %1 / %2; reserve %3 years").arg(qulonglong(planet->habitat.population)).arg(qulonglong(kOrbitalPopulationCapacity)).arg(planet->habitat.reserveYears);
                lines << QString("Planetary radar: %1 (%2 ly)")
                    .arg(planet->radar == PlanetaryRadarLevel::Full ? "Full" :
                         planet->radar == PlanetaryRadarLevel::Local ? "Local" : "None")
                    .arg(planetary_radar_range(planet->radar), 0, 'f', 0);
                const auto workforce = colony_workforce(planned, *planet);
                lines << QString("Factories: %1 active / %2 built (max %3)")
                    .arg(workforce.operatingFactories).arg(planet->industry).arg(workforce.factoryLimit);
                lines << QString("Mines: %1 active / %2 built (max %3)")
                    .arg(workforce.operatingMines).arg(planet->mines).arg(workforce.mineLimit);
                const auto* empire = find_player(received, pendingOrders_.player);
                status->setToolTip(QString("Population operates factories and mines independently. Per 10,000 surface colonists: %1 factories and %2 mines. Excess buildings remain idle.")
                    .arg(empire ? empire->race.factoriesPerTenThousand : kDefaultWorkforceRate)
                    .arg(empire ? empire->race.minesPerTenThousand : kDefaultWorkforceRate));
                const auto& damage=planet->warDamage;
                if(planet->aftermathKnownLocally && damage.lastBombardmentTurn) {
                    lines << QString("War ruins: %1 factories / %2 mines • last bombardment year %3")
                        .arg(qulonglong(reconstruction_remaining(*planet,ProductionKind::ReconstructFactory)))
                        .arg(qulonglong(reconstruction_remaining(*planet,ProductionKind::ReconstructMine))).arg(qulonglong(damage.lastBombardmentTurn));
                    lines << QString("Reconstructed: %1 factories / %2 mines • use Reconstruct factories/mines in Production")
                        .arg(qulonglong(damage.factoriesReconstructed)).arg(qulonglong(damage.minesReconstructed));
                }
                if(planet->aftermathKnownLocally && (damage.contamination || damage.lastCleanupTurn))
                    lines << QString("Contamination: %1/100 • Hab penalty %2 points • decay 2/year • %3 years to clear. Cleanup: up to 10 for 6 resources + B/G 1/1 kt.")
                        .arg(damage.contamination).arg(contamination_habitability_penalty(received,pendingOrders_.player,*planet))
                        .arg((damage.contamination+1)/2);
                lines << QString("Resources: %1 / year • Orbit: %2").arg(net_colony_output(planned,*planet,
                    projected_mineral_mining(received, *observed)))
                    .arg(find_orbital_station_at_planet(received, planet->id) ? "station" : "no station");
            }
            if(!own || !planet->aftermathKnownLocally) if(const auto a=known_planet_aftermath(received,pendingOrders_.player,planet->id)) {
                lines << QString("Last-known aftermath — observed year %1, received %2; age %3 years%4")
                    .arg(qulonglong(a->observedTurn)).arg(qulonglong(a->receivedTurn))
                    .arg(qulonglong(received.turn-a->observedTurn)).arg(a->exact ? " (former owner snapshot)" : " (orbital estimate)");
                lines << QString("Ruined factories %1–%2 • mines %3–%4 • contamination %5–%6/100")
                    .arg(qulonglong(a->factoriesMinimum)).arg(qulonglong(a->factoriesMaximum))
                    .arg(qulonglong(a->minesMinimum)).arg(qulonglong(a->minesMaximum))
                    .arg(a->contaminationMinimum).arg(a->contaminationMaximum);
            }
        }
    }
    status->setText(lines.join("<br>"));
    const auto minerals = planet ? mineral_display_data(received, planned, pendingOrders_.player, planet->id) : MineralDisplayData{};
    const auto values = [](const MineralCargo& m) { return std::array{m.ironium, m.boranium, m.germanium}; };
    const auto stock = values(minerals.stock.value_or(MineralCargo{}));
    const auto extraction = values(minerals.mining.value_or(MineralCargo{}));
    const auto conc = values(minerals.concentration.value_or(MineralCargo{}));
    const auto age = minerals.observedTurn ? received.turn - std::min(received.turn, *minerals.observedTurn) : 0;
    const auto ageText = !minerals.observedTurn ? QString("No received surface-stock report") : age == 0 ? QString("Current surface-stock report")
        : QString("Surface stock: %1 years old (year %2)").arg(qulonglong(age)).arg(qulonglong(*minerals.observedTurn));
    for (int row = 0; row < 3; ++row) {
        auto* item = table->topLevelItem(row);
        item->setText(1, minerals.stock ? QString::number(stock[row], 'f', 1) : "—");
        item->setText(2, minerals.mining ? QString::number(extraction[row], 'f', 1) : "—");
        item->setText(3, minerals.concentration ? QString::number(conc[row], 'f', 0) + "%" : "?");
        for (int column = 0; column < 4; ++column)
            item->setToolTip(column, QString("%1 • Stock: %2 kt • Current-year mining before cargo: %3 kt • Concentration: %4 • %5")
                .arg(item->text(0), item->text(1), item->text(2), item->text(3), ageText));
    }
    fitMineralRows(table);
}

} // namespace suns
