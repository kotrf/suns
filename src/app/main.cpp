#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    suns::attachRouteProgramDock(window);
    window.show();
    return app.exec();
}
