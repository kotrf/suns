#include "main_window.hpp"
#include "galaxy_setup_widget.hpp"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <tuple>

namespace suns {

void MainWindow::newCampaign()
{
    QDialog dialog(this);
    dialog.setObjectName("newCampaignDialog");
    dialog.setWindowTitle("New campaign — races and players");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    QLineEdit seed(seedEdit_->text(), &dialog);
    QSpinBox count(&dialog);
    count.setObjectName("campaignEmpireCount");
    count.setRange(1, 8);
    count.setValue(2);
    form->addRow("Galaxy seed", &seed);
    form->addRow("Empires", &count);
    layout->addLayout(form);
    GalaxySetupWidget galaxy(galaxyConfig_, &dialog);
    layout->addWidget(&galaxy);
    auto* explanation = new QLabel(
        "Player 1 is human and plays locally. Remote humans exchange .sunsturn / .sunsorders files. "
        "Computer empires submit ordinary orders automatically on this host, using their own received knowledge. "
        "Computer empires explore, settle, develop colonies and conduct military missions using received intelligence.\n\n"
        "Terrans: temperate worlds. Cryophiles: cold, lower-gravity worlds. "
        "Radiotrophs: narrow hot/heavy worlds, immune to radiation. "
        "Biology and the corresponding physical science unlock paid terraforming in the production queue.", &dialog);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    QTableWidget table(8, 8, &dialog);
    table.setObjectName("campaignEmpiresTable");
    table.setHorizontalHeaderLabels({"Empire name", "Race", "Fuel efficiency", "No ram scoops", "Settler engines", "Hull family", "Remote mining", "Control"});
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int row = 0; row < 8; ++row) {
        table.setItem(row, 0, new QTableWidgetItem(QString("Empire %1").arg(row + 1)));
        auto* race = new QComboBox(&table);
        race->addItems({"Terran", "Cryophile", "Radiotroph"});
        race->setCurrentIndex(row % 3);
        table.setCellWidget(row, 1, race);
        for (int column = 2; column < 5; ++column) {
            auto* access = new QCheckBox(&table);
            access->setToolTip(column == 2 ? "Unlock Fuel Mizer and Galaxy Scoop engines; reference engines consume 15% less fuel."
                : column == 3 ? "Disable researchable ram scoops; unlock Interspace-10 at Propulsion 11. Fuel Mizer and Settler's Delight remain allowed."
                : "Unlock Mini-Colony Ship and Settler's Delight. This grants propulsion access, without changing population growth.");
            table.setCellWidget(row, column, access);
        }
        auto* hullAccess = new QComboBox(&table);
        hullAccess->addItems({"Standard", "Inner Strength", "Super Stealth", "War Monger", "Space Demolition", "Hyper Expansion"});
        hullAccess->setToolTip("Choose access to special hulls. Population, economy and combat race effects are independent.");
        table.setCellWidget(row, 5, hullAccess);
        auto* miningAccess = new QComboBox(&table);
        miningAccess->addItems({"Standard", "Advanced (ARM)", "Basic (BRM)"});
        miningAccess->setToolTip("ARM adds Midget Miner and Ultra-Miner. BRM blocks reference miner hulls.");
        table.setCellWidget(row, 6, miningAccess);
        auto* controller = new QComboBox(&table);
        controller->addItems({"Human", "Computer"});
        controller->setCurrentIndex(row == 0 ? 0 : 1);
        controller->setEnabled(row != 0);
        controller->setToolTip(row == 0 ? "The local player is human."
            : "Human: exchange turn files. Computer: automatic ordinary orders on this host.");
        table.setCellWidget(row, 7, controller);
        connect(hullAccess, &QComboBox::currentIndexChanged, &table, [&table, row](int index) {
            if (index == static_cast<int>(HullAccess::HyperExpansion))
                qobject_cast<QCheckBox*>(table.cellWidget(row, 4))->setChecked(true);
        });
        table.setRowHidden(row, row >= count.value());
    }
    connect(&count, &QSpinBox::valueChanged, &dialog, [&](int value) {
        for (int row = 0; row < 8; ++row) table.setRowHidden(row, row >= value);
    });
    layout->addWidget(&table);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        bool ok{};
        seed.text().toULongLong(&ok);
        if (!ok) { QMessageBox::warning(&dialog, "Invalid seed", "Enter an unsigned integer."); return; }
        if (galaxy.config(seed.text().toULongLong()).starCount < static_cast<std::size_t>(count.value())) {
            QMessageBox::warning(&dialog, "Too few systems", "Each player needs a starting system.");
            return;
        }
        for (int row = 0; row < count.value(); ++row) {
            const auto name = table.item(row, 0)->text().trimmed().toUtf8();
            if (name.isEmpty() || name.size() > 80) {
                QMessageBox::warning(&dialog, "Empire name", "Names must contain 1–80 UTF-8 bytes."); return;
            }
        }
        dialog.accept();
    });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(&buttons);
    dialog.resize(1220, 600);
    if (dialog.exec() != QDialog::Accepted) return;
    empireSetups_.clear();
    for (int row = 0; row < count.value(); ++row) {
        const auto* race = qobject_cast<QComboBox*>(table.cellWidget(row, 1));
        empireSetups_.push_back({table.item(row, 0)->text().trimmed().toStdString(),
            static_cast<RacePreset>(race->currentIndex()),
            qobject_cast<QCheckBox*>(table.cellWidget(row, 2))->isChecked(),
            qobject_cast<QCheckBox*>(table.cellWidget(row, 3))->isChecked(),
            qobject_cast<QCheckBox*>(table.cellWidget(row, 4))->isChecked(),
            static_cast<HullAccess>(qobject_cast<QComboBox*>(table.cellWidget(row, 5))->currentIndex()),
            qobject_cast<QComboBox*>(table.cellWidget(row, 6))->currentIndex() == 1,
            qobject_cast<QComboBox*>(table.cellWidget(row, 6))->currentIndex() == 2,
            static_cast<PlayerController>(qobject_cast<QComboBox*>(table.cellWidget(row, 7))->currentIndex())});
    }
    seedEdit_->setText(seed.text());
    newGalaxy(galaxy.config(seed.text().toULongLong()));
}

SaveGameData MainWindow::campaignSnapshot() const
{
    SaveGameData save;
    save.campaignId = campaignId_;
    save.turnToken = turnToken_;
    save.galaxyConfig = galaxyConfig_;
    save.state = state_;
    save.pendingOrders = pendingOrders_;
    save.pendingDescriptions = pendingDescriptions_;
    save.mode = sessionMode_;
    save.playerTokens = playerTokens_;
    save.inbox = inbox_;
    save.strategicMessages = sessionMode_ == SessionMode::Host ? campaignMessages_ : turnMessages_;
    return save;
}

void MainWindow::exportPlayerTurns()
{
    if (sessionMode_ != SessionMode::Host) {
        QMessageBox::information(this, "Player turns", "Create or open a multiplayer host campaign first.");
        return;
    }
    if (std::none_of(state_.players.begin(), state_.players.end(), [this](const Player& player) {
        return player.id != pendingOrders_.player && player.controller == PlayerController::Human;
    })) {
        statusBar()->showMessage("No remote human empires. Computer empires play automatically on this host.", 6000);
        return;
    }
    const auto directory = QFileDialog::getExistingDirectory(this, "Export player turns to folder");
    if (directory.isEmpty()) return;
    // Persist the exact host boundary and its tokens before distributing turns.
    // This makes closing/reopening the host safe even before any orders return.
    if (currentSavePath_.isEmpty()) { saveGameAs(); if (currentSavePath_.isEmpty()) return; }
    else if (!saveGameToPath(currentSavePath_)) return;
    const auto host = campaignSnapshot();
    for (const auto& player : state_.players) {
        if (player.id == pendingOrders_.player || player.controller != PlayerController::Human) continue;
        const auto file = QString("suns-%1-turn-%2-player-%3.sunsturn")
            .arg(QString::number(campaignId_, 16)).arg(qulonglong(state_.turn)).arg(player.id);
        QString error;
        if (!write_save_game_file(QDir(directory).filePath(file), make_player_turn(host, player.id), error)) {
            QMessageBox::warning(this, "Export failed", error);
            return;
        }
    }
    statusBar()->showMessage("Player turns exported. Collect each .sunsorders file with Import Orders.", 7000);
}

void MainWindow::openComputerDecisions()
{
    if (sessionMode_ == SessionMode::PlayerTurn) {
        statusBar()->showMessage("Computer decisions are available on the campaign host.", 5000);
        return;
    }
    if (auto* existing = findChild<QDialog*>("computerDecisionsDialog")) {
        existing->show(); existing->raise(); existing->activateWindow(); return;
    }
    auto* dialog = new QDialog(this, Qt::Window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("computerDecisionsDialog");
    dialog->setWindowTitle("Computer decisions");
    dialog->resize(950, 600);
    auto* layout = new QVBoxLayout(dialog);
    auto* summary = new QLabel("Current fleet goals and the last 32 resolved decisions per computer empire. Exploration, economy, defense, raids, bombardment and invasion use ordinary orders and received intelligence. Distant commands wait for acknowledgement.", dialog);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel("Empire:", dialog));
    auto* filter = new QComboBox(dialog);
    filter->setObjectName("computerDecisionsEmpireFilter");
    filter->setToolTip("Show decisions and fleet goals for one computer empire.");
    filter->addItem("All computer empires", 0);
    filterRow->addWidget(filter);
    filterRow->addStretch();
    layout->addLayout(filterRow);
    auto* table = new QTreeWidget(dialog);
    table->setObjectName("computerDecisionsTable");
    table->setHeaderLabels({"Empire", "Planned turn", "Orders", "Decision"});
    table->setRootIsDecorated(false);
    table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    layout->addWidget(table, 1);
    auto* goals = new QTreeWidget(dialog);
    goals->setObjectName("computerGoalsTable");
    goals->setHeaderLabels({"Empire", "Fleet", "Goal", "System", "Issued turn"});
    goals->setRootIsDecorated(false);
    goals->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    goals->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    layout->addWidget(goals, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    auto* copyReport = buttons->addButton("Copy report", QDialogButtonBox::ActionRole);
    copyReport->setObjectName("copyComputerDecisionsReport");
    copyReport->setToolTip("Copy the visible full decisions and ongoing fleet goals as plain text.");
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    auto refresh = [this, dialog, table, goals, filter,
        previous = std::vector<std::tuple<PlayerId, std::string, ComputerState>>{},
        previousFilter = -1]() mutable {
        if (shuttingDown_ || !dialog->isVisible()) return;
        std::vector<std::tuple<PlayerId, std::string, ComputerState>> current;
        if (sessionMode_ != SessionMode::PlayerTurn)
            for (const auto& player : state_.players)
                if (player.controller == PlayerController::Computer)
                    current.emplace_back(player.id, player.name, player.computer);
        if (current != previous) {
            const auto selectedId = filter->currentData().toInt();
            const QSignalBlocker blocked(filter);
            filter->clear();
            filter->addItem("All computer empires", 0);
            for (const auto& player : state_.players)
                if (player.controller == PlayerController::Computer)
                    filter->addItem(QString::fromStdString(player.name), int(player.id));
            const auto index = filter->findData(selectedId);
            filter->setCurrentIndex(index >= 0 ? index : 0);
        }
        const auto selectedId = filter->currentData().toInt();
        if (current == previous && selectedId == previousFilter) return;
        previous = current;
        previousFilter = selectedId;
        table->clear();
        goals->clear();
        if (sessionMode_ == SessionMode::PlayerTurn) return;
        for (const auto& player : state_.players) {
            if (player.controller != PlayerController::Computer
                || (selectedId != 0 && int(player.id) != selectedId)) continue;
            for (auto record = player.computer.decisions.rbegin(); record != player.computer.decisions.rend(); ++record) {
                auto* item = new QTreeWidgetItem(table, {QString::fromStdString(player.name),
                    QString::number(qulonglong(record->turn)), QString::number(record->orderCount),
                    QString::fromStdString(record->reason)});
                item->setToolTip(3, QString::fromStdString(record->reason));
            }
            for (const auto& goal : player.computer.fleetGoals) {
                const auto* star = find_star(state_, goal.target);
                const char* kind = "Unknown";
                switch (goal.kind) {
                case ComputerFleetGoalKind::Survey: kind = "Survey and return"; break;
                case ComputerFleetGoalKind::Colonize: kind = "Colonize"; break;
                case ComputerFleetGoalKind::Resupply: kind = "Resupply"; break;
                case ComputerFleetGoalKind::Gather: kind = "Gather task group"; break;
                case ComputerFleetGoalKind::Defend: kind = "Defend colony"; break;
                case ComputerFleetGoalKind::Raid: kind = "Raid colony"; break;
                case ComputerFleetGoalKind::Bombard: kind = "Bombard colony"; break;
                case ComputerFleetGoalKind::Invade: kind = "Land assault troops"; break;
                case ComputerFleetGoalKind::InvestigateObject: kind = "Study unknown object"; break;
                }
                new QTreeWidgetItem(goals, {QString::fromStdString(player.name), QString::number(goal.fleet),
                    kind, goal.kind==ComputerFleetGoalKind::InvestigateObject ? QString("Object X%1").arg(goal.target) : star ? QString::fromStdString(star->name) : QString::number(goal.target),
                    QString::number(qulonglong(goal.issuedTurn))});
            }
        }
    };
    auto* timer = new QTimer(dialog);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, dialog, refresh);
    connect(filter, &QComboBox::currentIndexChanged, dialog, refresh);
    connect(copyReport, &QPushButton::clicked, dialog, [this, filter, table, goals] {
        QStringList report;
        report << "Suns! computer report"
               << QString("Campaign turn: %1").arg(qulonglong(state_.turn))
               << QString("Empire filter: %1").arg(filter->currentText())
               << "" << "Decisions";
        if (table->topLevelItemCount() == 0) report << "(none)";
        for (int row = 0; row < table->topLevelItemCount(); ++row) {
            const auto* item = table->topLevelItem(row);
            report << QString("%1 | turn %2 | %3 orders")
                          .arg(item->text(0)).arg(item->text(1)).arg(item->text(2))
                   << item->text(3);
        }
        report << "" << "Fleet goals";
        if (goals->topLevelItemCount() == 0) report << "(none)";
        for (int row = 0; row < goals->topLevelItemCount(); ++row) {
            const auto* item = goals->topLevelItem(row);
            report << QString("%1 | fleet %2 | %3 | system %4 | issued turn %5")
                          .arg(item->text(0)).arg(item->text(1)).arg(item->text(2))
                          .arg(item->text(3)).arg(item->text(4));
        }
        QApplication::clipboard()->setText(report.join('\n') + '\n');
        statusBar()->showMessage("Computer report copied to clipboard.", 4000);
    });
    timer->start();
    dialog->show(); refresh();
}

} // namespace suns
