#include "main_window.hpp"
#include "race_file.hpp"
#include "race_setup_widget.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace suns {
void MainWindow::openRaceDesigner()
{
    QDialog dialog(this);
    dialog.setObjectName("raceDesignerDialog");
    dialog.setWindowTitle("Race designer — prepare a player race");
    auto* layout = new QVBoxLayout(&dialog);
    auto* help = new QLabel("Create your race, save it as a .sunsrace file, and send that file to the host. "
        "The host loads each player's race in New campaign. Your current game is unaffected.", &dialog);
    help->setWordWrap(true); layout->addWidget(help);
    RaceSetupWidget editor(&dialog); layout->addWidget(&editor);
    QLabel status(&dialog); status.setObjectName("raceFileStatus"); status.setWordWrap(true);
    layout->addWidget(&status);
    QString currentPath;
    QDialogButtonBox buttons(QDialogButtonBox::Close, &dialog);
    auto* load = buttons.addButton("Load race…", QDialogButtonBox::ActionRole);
    load->setObjectName("raceLoadButton");
    auto* save = buttons.addButton("Save race…", QDialogButtonBox::ActionRole);
    save->setObjectName("raceSaveButton");
    connect(load, &QPushButton::clicked, &dialog, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, "Load player race", currentPath,
            "Suns! races (*.sunsrace);;All files (*)");
        if (path.isEmpty()) return;
        EmpireSetup setup; QString error;
        if (!read_race_file(path, setup, error)) { QMessageBox::warning(&dialog, "Load race failed", error); return; }
        editor.setSetup(setup); currentPath = path;
        status.setText("Loaded: " + QDir::toNativeSeparators(path));
    });
    connect(save, &QPushButton::clicked, &dialog, [&] {
        const auto setup = editor.setup();
        if (const auto error = race_setup_error(setup); !error.isEmpty()) {
            QMessageBox::warning(&dialog, "Race settings", error); return;
        }
        auto path = QFileDialog::getSaveFileName(&dialog, "Save player race",
            currentPath.isEmpty() ? "race.sunsrace" : currentPath, "Suns! races (*.sunsrace)");
        if (path.isEmpty()) return;
        if (QFileInfo(path).suffix().isEmpty()) path += ".sunsrace";
        QString error;
        if (!write_race_file(path, setup, error)) { QMessageBox::warning(&dialog, "Save race failed", error); return; }
        currentPath = path;
        status.setText("Saved: " + QDir::toNativeSeparators(path) + "\nSend this race file to the campaign host.");
    });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(&buttons);
    dialog.resize(740, 520);
    dialog.exec();
}
} // namespace suns
