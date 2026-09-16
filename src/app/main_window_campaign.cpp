#include "main_window.hpp"

#include <QDialog>
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
#include <QVBoxLayout>

namespace suns {

void MainWindow::newCampaign()
{
    QDialog dialog(this);
    dialog.setObjectName("newCampaignDialog");
    dialog.setWindowTitle("New campaign — races and players");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    QLineEdit seed(seedEdit_->text(), &dialog);
    QSpinBox stars(&dialog), count(&dialog);
    stars.setRange(8, 64);
    stars.setValue(starCountSpin_->value());
    count.setRange(1, 8);
    count.setValue(2);
    form->addRow("Galaxy seed", &seed);
    form->addRow("Systems", &stars);
    form->addRow("Human players", &count);
    layout->addLayout(form);
    auto* explanation = new QLabel(
        "Player 1 hosts and plays locally. Other players open their .sunsturn files, plan orders, "
        "then send .sunsorders back. All players submit before the host resolves the turn.\n\n"
        "Terrans: temperate worlds. Cryophiles: cold, lower-gravity worlds. "
        "Radiotrophs: narrow hot/heavy worlds, immune to radiation. "
        "Biology unlocks habitats beyond these environmental limits.", &dialog);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    QTableWidget table(8, 2, &dialog);
    table.setHorizontalHeaderLabels({"Empire name", "Race"});
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int row = 0; row < 8; ++row) {
        table.setItem(row, 0, new QTableWidgetItem(QString("Empire %1").arg(row + 1)));
        auto* race = new QComboBox(&table);
        race->addItems({"Terran", "Cryophile", "Radiotroph"});
        race->setCurrentIndex(row % 3);
        table.setCellWidget(row, 1, race);
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
    dialog.resize(680, 600);
    if (dialog.exec() != QDialog::Accepted) return;
    empireSetups_.clear();
    for (int row = 0; row < count.value(); ++row) {
        const auto* race = qobject_cast<QComboBox*>(table.cellWidget(row, 1));
        empireSetups_.push_back({table.item(row, 0)->text().trimmed().toStdString(),
            static_cast<RacePreset>(race->currentIndex())});
    }
    seedEdit_->setText(seed.text());
    starCountSpin_->setValue(stars.value());
    newGalaxy();
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
    const auto directory = QFileDialog::getExistingDirectory(this, "Export player turns to folder");
    if (directory.isEmpty()) return;
    // Persist the exact host boundary and its tokens before distributing turns.
    // This makes closing/reopening the host safe even before any orders return.
    if (currentSavePath_.isEmpty()) { saveGameAs(); if (currentSavePath_.isEmpty()) return; }
    else if (!saveGameToPath(currentSavePath_)) return;
    const auto host = campaignSnapshot();
    for (const auto& player : state_.players) {
        if (player.id == pendingOrders_.player) continue;
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

} // namespace suns
