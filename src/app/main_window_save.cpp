#include "main_window.hpp"
#include "save_game.hpp"

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>

namespace suns {

void MainWindow::saveGame()
{
    if (currentSavePath_.isEmpty()) {
        saveGameAs();
        return;
    }
    saveGameToPath(currentSavePath_);
}

void MainWindow::saveGameAs()
{
    QString suggested = currentSavePath_;
    if (suggested.isEmpty()) {
        suggested = QString("suns-turn-%1.suns").arg(static_cast<qulonglong>(state_.turn));
    }

    auto path = QFileDialog::getSaveFileName(
        this,
        "Save Suns! Game",
        suggested,
        "Suns! save games (*.suns);;All files (*)");
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".suns";
    saveGameToPath(path);
}

void MainWindow::openGame()
{
    const auto path = QFileDialog::getOpenFileName(
        this,
        "Open Suns! Game",
        currentSavePath_.isEmpty() ? QString{} : QFileInfo(currentSavePath_).absolutePath(),
        "Suns! save games (*.suns);;All files (*)");
    if (path.isEmpty()) return;
    loadGameFromPath(path);
}

bool MainWindow::saveGameToPath(const QString& path)
{
    SaveGameData save;
    save.galaxyConfig = galaxyConfig_;
    save.state = state_;
    save.pendingOrders = pendingOrders_;
    save.pendingDescriptions = pendingDescriptions_;
    save.selectedStar = selectedStarId_;
    save.selectedFleet = selectedFleetId_;
    save.showSensorRanges = showSensorRanges_;

    QString error;
    if (!write_save_game_file(path, save, error)) {
        QMessageBox::warning(this, "Save Game Failed", error);
        statusBar()->showMessage("Save failed", 3000);
        return false;
    }

    currentSavePath_ = QFileInfo(path).absoluteFilePath();
    updateSaveWindowTitle();
    statusBar()->showMessage(
        QString("Saved turn %1 to %2")
            .arg(static_cast<qulonglong>(state_.turn))
            .arg(QFileInfo(currentSavePath_).fileName()),
        3000);
    return true;
}

bool MainWindow::loadGameFromPath(const QString& path)
{
    SaveGameData loaded;
    QString error;
    if (!read_save_game_file(path, loaded, error)) {
        QMessageBox::warning(this, "Open Game Failed", error);
        statusBar()->showMessage("Open game failed", 3000);
        return false;
    }

    galaxyConfig_ = loaded.galaxyConfig;
    state_ = std::move(loaded.state);
    pendingOrders_ = std::move(loaded.pendingOrders);
    pendingDescriptions_ = std::move(loaded.pendingDescriptions);
    selectedStarId_ = loaded.selectedStar;
    selectedFleetId_ = loaded.selectedFleet;
    showSensorRanges_ = loaded.showSensorRanges;
    currentSavePath_ = QFileInfo(path).absoluteFilePath();

    warpControlFleetId_.reset();
    logisticsControlFleetId_.reset();

    seedEdit_->setText(QString::number(static_cast<qulonglong>(galaxyConfig_.seed)));
    starCountSpin_->setValue(static_cast<int>(galaxyConfig_.starCount));
    {
        const QSignalBlocker blocker(sensorRangesCheck_);
        sensorRangesCheck_->setChecked(showSensorRanges_);
    }

    scene_->setSceneRect(
        -galaxyConfig_.width / 2.0 - 55.0,
        -galaxyConfig_.height / 2.0 - 55.0,
        galaxyConfig_.width + 110.0,
        galaxyConfig_.height + 110.0);

    refreshShipDesignChoices();
    rebuildScene();
    fitGalaxyView();
    updateSaveWindowTitle();

    statusBar()->showMessage(
        QString("Opened %1 — turn %2, %3 pending order%4")
            .arg(QFileInfo(currentSavePath_).fileName())
            .arg(static_cast<qulonglong>(state_.turn))
            .arg(static_cast<qulonglong>(pendingOrders_.orders.size()))
            .arg(pendingOrders_.orders.size() == 1 ? "" : "s"),
        4000);
    return true;
}

void MainWindow::updateSaveWindowTitle()
{
    if (currentSavePath_.isEmpty()) {
        setWindowTitle(QString("Suns! — Turn %1").arg(static_cast<qulonglong>(state_.turn)));
        return;
    }

    setWindowTitle(QString("Suns! — %1 — Turn %2")
        .arg(QFileInfo(currentSavePath_).fileName())
        .arg(static_cast<qulonglong>(state_.turn)));
}

} // namespace suns
