#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QMainWindow>
#include <QString>

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
    void queueScoutMove();
    void queueColonyShipMove();
    void queueBuildColonyShip();
    void queueColonize();
    void endTurn();

    [[nodiscard]] const StarSystem* selectedStar() const;
    [[nodiscard]] const Planet* selectedPlanet() const;
    [[nodiscard]] const Fleet* playerScout() const;
    [[nodiscard]] const Fleet* playerColonyShip() const;
    [[nodiscard]] const Fleet* colonyShipAtSelectedStar() const;

    void replacePendingOrder(Order order, const QString& description);

    GameState state_;
    TurnProcessor processor_;
    PlayerOrders pendingOrders_{1, {}};
    std::optional<StarId> selectedStarId_;
    QString pendingDescription_;

    QGraphicsScene* scene_{};
    QLabel* selectionLabel_{};
    QLabel* empireLabel_{};
    QLabel* ordersLabel_{};
    QPushButton* scoutMoveButton_{};
    QPushButton* colonyMoveButton_{};
    QPushButton* buildButton_{};
    QPushButton* colonizeButton_{};
    QPushButton* endTurnButton_{};
};

} // namespace suns
