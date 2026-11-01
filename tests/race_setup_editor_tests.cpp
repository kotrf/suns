#include "main_window.hpp"
#include "race_file.hpp"
#include "race_setup_widget.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <cassert>
#include <functional>

namespace suns {
struct MainWindowTestAccess {
    static SaveGameData snapshot(MainWindow& window) { return window.campaignSnapshot(); }
    static bool save(MainWindow& window, const QString& path) { return window.saveGameToPath(path); }
    static bool load(MainWindow& window, const QString& path) { return window.loadGameFromPath(path); }
    static void restart(MainWindow& window) { window.newGalaxy(); }
};
}
using namespace suns;

static RaceSetupWidget* editorIn(QDialog* dialog)
{
    auto* editor = dynamic_cast<RaceSetupWidget*>(dialog->findChild<QWidget*>("raceSetupWidget"));
    assert(editor); return editor;
}
static void chooseFile(QString path, std::function<void()> after = {})
{
    QTimer::singleShot(0, [path, after] {
        auto* file = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()); assert(file);
        file->selectFile(path);
        assert(QMetaObject::invokeMethod(file, "accept", Qt::DirectConnection));
        if (after) QTimer::singleShot(0, after);
    });
}
static QByteArray bytesAt(const QString& path)
{
    QFile file(path); assert(file.open(QIODevice::ReadOnly)); return file.readAll();
}

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QTemporaryDir directory; assert(directory.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
    QString error;
    const auto firstPath = directory.filePath("first.sunsrace");
    const auto secondPath = directory.filePath("second.sunsrace");
    const auto rowPath = directory.filePath("exported.sunsrace");
    const auto badPath = directory.filePath("bad.sunsrace");
    { QFile file(badPath); assert(file.open(QIODevice::WriteOnly)); file.write("{broken"); }

    EmpireSetup second{"Cold navigators", RacePreset::Cryophile};
    second.primaryTrait = PrimaryRaceTrait::StargateSpecialist;
    second.hullAccess = HullAccess::InnerStrength;
    second.basicRemoteMining = true;
    second.factoriesPerTenThousand = 25; second.minesPerTenThousand = 5;
    assert(write_race_file(secondPath, second, error));

    MainWindow window;
    window.installUiPolish(); window.installFleetReadabilityPolish();
    window.resize(1366, 900); window.show(); app.processEvents();
    auto* raceAction = window.findChild<QAction*>("openRaceDesignerAction"); assert(raceAction);
    const auto beforePath = directory.filePath("before.suns");
    const auto afterPath = directory.filePath("after.suns");
    assert(write_save_game_file(beforePath, MainWindowTestAccess::snapshot(window), error));

    // A real player creates and saves a race without generating or mutating a game.
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "raceDesignerDialog");
        auto* editor = editorIn(dialog);
        dialog->findChild<QLineEdit*>("raceEmpireName")->setText("Warm explorers");
        dialog->findChild<QComboBox*>("raceEnvironment")->setCurrentIndex(2);
        assert(dialog->findChild<QLabel*>("raceEnvironmentSummary")->text().contains("immune"));
        dialog->findChild<QCheckBox*>("raceFuelEfficiency")->setChecked(true);
        dialog->findChild<QCheckBox*>("raceNoRamScoops")->setChecked(true);
        auto* hulls = dialog->findChild<QComboBox*>("raceHullFamily");
        hulls->setCurrentIndex(int(HullAccess::HyperExpansion));
        auto* settlers = dialog->findChild<QCheckBox*>("raceSettlerEngines");
        assert(settlers->isChecked() && !settlers->isEnabled());
        hulls->setCurrentIndex(int(HullAccess::SuperStealth)); assert(settlers->isEnabled());
        dialog->findChild<QComboBox*>("raceRemoteMining")->setCurrentIndex(1);
        dialog->findChild<QSpinBox*>("raceFactoryOperation")->setValue(7);
        dialog->findChild<QSpinBox*>("raceMineOperation")->setValue(23);
        assert(editor->setup().race == RacePreset::Radiotroph);
        if (app.arguments().contains("--capture")) dialog->grab().save("/tmp/suns-race-designer.png");
        // Missing suffix is supplied by the UI.
        chooseFile(directory.filePath("first"));
        dialog->findChild<QPushButton*>("raceSaveButton")->click();
        assert(QFile::exists(firstPath));
        assert(dialog->findChild<QLabel*>("raceFileStatus")->text().contains("Saved:"));
        chooseFile(secondPath); dialog->findChild<QPushButton*>("raceLoadButton")->click();
        assert(editor->setup().name == second.name && editor->setup().factoriesPerTenThousand == 25);
        dialog->reject();
    });
    raceAction->trigger();
    assert(write_save_game_file(afterPath, MainWindowTestAccess::snapshot(window), error));
    assert(bytesAt(beforePath) == bytesAt(afterPath));
    EmpireSetup first; assert(read_race_file(firstPath, first, error));
    assert(first.name == "Warm explorers" && first.race == RacePreset::Radiotroph);
    assert(first.improvedFuelEfficiency && first.noRamScoopEngines && first.settlerEngineAccess);
    assert(first.hullAccess == HullAccess::SuperStealth && first.advancedRemoteMining);
    assert(first.factoriesPerTenThousand == 7 && first.minesPerTenThousand == 23);

    // Each file goes into its selected host slot, and imported players become
    // Human. Cancelling/failing an import or editor leaves that row intact.
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "newCampaignDialog");
        auto* raceSlots = dialog->findChild<QComboBox*>("campaignRaceSlot");
        auto* table = dialog->findChild<QTableWidget*>("campaignEmpiresTable");
        assert(raceSlots && table && raceSlots->count() == 2);
        chooseFile(firstPath); dialog->findChild<QPushButton*>("campaignLoadRaceButton")->click();
        assert(table->item(0, 0)->text() == "Warm explorers");
        raceSlots->setCurrentIndex(1);
        assert(qobject_cast<QComboBox*>(table->cellWidget(1, 7))->currentIndex() == 1);
        chooseFile(secondPath); dialog->findChild<QPushButton*>("campaignLoadRaceButton")->click();
        assert(qobject_cast<QComboBox*>(table->cellWidget(1, 7))->currentIndex() == 0);
        assert(table->item(1, 0)->text() == "Cold navigators");
        QTimer::singleShot(0, [] {
            auto* file = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()); assert(file); file->reject();
        });
        dialog->findChild<QPushButton*>("campaignLoadRaceButton")->click();
        assert(table->item(1, 0)->text() == "Cold navigators");
        chooseFile(badPath, [] {
            auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); assert(warning); warning->accept();
        });
        dialog->findChild<QPushButton*>("campaignLoadRaceButton")->click();
        assert(table->item(1, 0)->text() == "Cold navigators");
        QTimer::singleShot(0, [] {
            auto* edit = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            assert(edit && edit->objectName() == "campaignRaceEditorDialog");
            edit->findChild<QLineEdit*>("raceEmpireName")->setText("Discard this"); edit->reject();
        });
        dialog->findChild<QPushButton*>("campaignEditRaceButton")->click();
        assert(table->item(1, 0)->text() == "Cold navigators");
        QTimer::singleShot(0, [] {
            auto* edit = qobject_cast<QDialog*>(QApplication::activeModalWidget()); assert(edit);
            edit->findChild<QSpinBox*>("raceMineOperation")->setValue(9);
            edit->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        dialog->findChild<QPushButton*>("campaignEditRaceButton")->click();
        assert(qobject_cast<QSpinBox*>(table->cellWidget(1, 10))->value() == 9);
        chooseFile(rowPath); dialog->findChild<QPushButton*>("campaignSaveRaceButton")->click();
        if (app.arguments().contains("--capture")) dialog->grab().save("/tmp/suns-campaign-race-files.png");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    window.findChild<QAction*>("newCampaignAction")->trigger();
    EmpireSetup exported; assert(read_race_file(rowPath, exported, error));
    assert(exported.name == second.name && exported.minesPerTenThousand == 9);
    const auto host = MainWindowTestAccess::snapshot(window);
    assert(host.mode == SessionMode::Host && host.state.players.size() == 2 && host.playerTokens.size() == 2);
    const auto& a = host.state.players[0]; const auto& b = host.state.players[1];
    assert(a.controller == PlayerController::Human && b.controller == PlayerController::Human);
    assert(a.name == first.name && a.race.radiationImmune && a.race.hullAccess == HullAccess::SuperStealth);
    assert(a.race.improvedFuelEfficiency && a.race.noRamScoopEngines && a.race.advancedRemoteMining);
    assert(a.race.factoriesPerTenThousand == 7 && a.race.minesPerTenThousand == 23);
    assert(b.name == second.name && b.race.primaryTrait == PrimaryRaceTrait::StargateSpecialist);
    assert(b.race.basicRemoteMining && b.race.factoriesPerTenThousand == 25 && b.race.minesPerTenThousand == 9);
    const auto turn = make_player_turn(host, 2);
    assert(turn.mode == SessionMode::PlayerTurn && turn.state.players.size() == 1);
    assert(turn.state.players.front().name == second.name && turn.state.players.front().race.minesPerTenThousand == 9);
    const auto hostPath = directory.filePath("host.suns");
    assert(MainWindowTestAccess::save(window, hostPath) && MainWindowTestAccess::load(window, hostPath));
    MainWindowTestAccess::restart(window);
    const auto restarted = MainWindowTestAccess::snapshot(window);
    assert(restarted.state.players[0].race.factoriesPerTenThousand == 7);
    assert(restarted.state.players[1].race.primaryTrait == PrimaryRaceTrait::StargateSpecialist);
    assert(restarted.state.players[1].race.minesPerTenThousand == 9);
    assert(restarted.state.players[1].controller == PlayerController::Human);
}
