#include "main_window.hpp"
#include "route_program_dock.hpp"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    suns::MainWindow window;
    window.installDeferredMapSelectionHandler();
    suns::attachRouteProgramDock(window);
    window.installUiPolish();

    window.show();

    if (app.arguments().contains("--smoke-test")) {
        QTimer::singleShot(0, &window, [&window] {
            window.showMaximized();
            if (auto* scene = window.routeProgramScene()) {
                for (auto* item : scene->items()) {
                    if (item->data(1).toInt() == 1) {
                        item->setSelected(true);
                        break;
                    }
                }
            }
        });
        // Let deferred selection redraw and the route-program refresh timer run
        // before exercising the real close/shutdown path.
        QTimer::singleShot(350, &window, [&window] { window.close(); });
        QTimer::singleShot(3000, &app, &QApplication::quit);
    }

    return app.exec();
}
