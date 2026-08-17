#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QMainWindow>

#include <optional>

class QGraphicsScene;
class QLabel;
class QPushButton;

namespace suns {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void rebuildScene();
    void updateControls();
    void queueMoveToSelectedStar();
    void endTurn();

    [[nodiscard]] const StarSystem* selectedStar() const;
    [[nodiscard]] const Fleet* playerFleet() const;

    GameState state_;
    TurnProcessor processor_;
    PlayerOrders pendingOrders_{1, {}};
    std::optional<StarId> selectedStarId_;

    QGraphicsScene* scene_{};
    QLabel* selectionLabel_{};
    QLabel* ordersLabel_{};
    QPushButton* moveButton_{};
    QPushButton* endTurnButton_{};
};

} // namespace suns
