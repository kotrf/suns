#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include <optional>

class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace suns {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void rebuildScene();
    void updateControls();
    void queueScoutMove();
    void queueColonyShipMove();
    void queueProduction(ProductionKind kind);
    void queueColonize();
    void endTurn();
    void newGalaxy();

    [[nodiscard]] const StarSystem* selectedStar() const;
    [[nodiscard]] const Planet* selectedPlanet() const;
    [[nodiscard]] const Fleet* playerScout() const;
    [[nodiscard]] const Fleet* playerColonyShip() const;
    [[nodiscard]] const Fleet* colonyShipAtSelectedStar() const;

    void appendPendingOrder(Order order, const QString& description);

    GalaxyConfig galaxyConfig_;
    GameState state_;
    TurnProcessor processor_;
    PlayerOrders pendingOrders_{1, {}};
    std::optional<StarId> selectedStarId_;
    QStringList pendingDescriptions_;

    QGraphicsScene* scene_{};
    QGraphicsView* view_{};
    QLabel* galaxyLabel_{};
    QLabel* selectionLabel_{};
    QLabel* empireLabel_{};
    QLabel* ordersLabel_{};
    QLineEdit* seedEdit_{};
    QSpinBox* starCountSpin_{};
    QPushButton* newGalaxyButton_{};
    QPushButton* scoutMoveButton_{};
    QPushButton* colonyMoveButton_{};
    QPushButton* buildColonyButton_{};
    QPushButton* buildFactoryButton_{};
    QPushButton* colonizeButton_{};
    QPushButton* endTurnButton_{};
};

} // namespace suns
