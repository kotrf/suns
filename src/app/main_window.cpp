#include "main_window.hpp"

#include "suns/game_state.hpp"

#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QStatusBar>

namespace suns {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Suns!");
    resize(1100, 720);

    const auto state = make_demo_game();
    auto* scene = new QGraphicsScene(this);
    scene->setSceneRect(-400, -300, 800, 600);

    for (const auto& star : state.stars) {
        constexpr double radius = 5.0;
        scene->addEllipse(
            star.position.x - radius,
            star.position.y - radius,
            radius * 2.0,
            radius * 2.0);
        auto* label = scene->addText(QString::fromStdString(star.name));
        label->setPos(star.position.x + 8.0, star.position.y - 12.0);
    }

    auto* view = new QGraphicsView(scene, this);
    view->setRenderHint(QPainter::Antialiasing);
    setCentralWidget(view);
    statusBar()->showMessage(QString("Turn %1 — bootstrap galaxy").arg(state.turn));
}

} // namespace suns
