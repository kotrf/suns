#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QApplication>
#include <QMenuBar>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    window.installDeferredMapSelectionHandler();
    suns::attachRouteProgramDock(window);

    auto* cargoAction = window.menuBar()->addAction("Cargo Manifest…");
    QObject::connect(cargoAction, &QAction::triggered, &window, [&window] {
        window.openCargoManifestDialog();
    });

    window.show();
    return app.exec();
}
