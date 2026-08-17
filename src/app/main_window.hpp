#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include <optional>

class QCheckBox;
class QComboBox;
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
    void refreshShipDesignChoices();
    void openShipDesigner();
    void queueFleetMove();
    void queueFleetLoadAll();
    void queueShipDesign();
    void queueProduction(ProductionKind kind);
    void queueColonists();
    void queueRefuel();
    void queueColonize();
    void endTurn();
    void newGalaxy();

    [[nodiscard]] const StarSystem* selectedStar() const;
    [[nodiscard]] const Planet* selectedPlanet() const;
    [[nodiscard]] const Fleet* selectedFleet() const;
    [[nodiscard]] const Fleet* selectedColonyShipAtSelectedStar() const;
    [[nodiscard]] const Planet* selectedFriendlyColonyForFleet() const;

    void appendPendingOrder(Order order, const QString& description);
    void replacePendingFleetMove(
        FleetId fleet,
        Position destination,
        std::uint8_t warp,
        FleetArrivalAction arrivalAction,
        const QString& description);

    GalaxyConfig galaxyConfig_;
    GameState state_;
    TurnProcessor processor_;
    PlayerOrders pendingOrders_{1, {}};
    std::optional<StarId> selectedStarId_;
    std::optional<FleetId> selectedFleetId_{1};
    std::optional<FleetId> warpControlFleetId_;
    std::optional<FleetId> logisticsControlFleetId_;
    QStringList pendingDescriptions_;
    bool showSensorRanges_{true};

    QGraphicsScene* scene_{};
    QGraphicsView* view_{};
    QLabel* galaxyLabel_{};
    QLabel* selectionLabel_{};
    QLabel* fleetLabel_{};
    QLabel* empireLabel_{};
    QLabel* ordersLabel_{};
    QLineEdit* seedEdit_{};
    QSpinBox* starCountSpin_{};
    QSpinBox* warpSpin_{};
    QSpinBox* colonistLoadSpin_{};
    QSpinBox* arrivalReserveSpin_{};
    QComboBox* shipDesignCombo_{};
    QCheckBox* sensorRangesCheck_{};
    QPushButton* newGalaxyButton_{};
    QPushButton* fleetMoveButton_{};
    QPushButton* fleetLoadAllButton_{};
    QPushButton* designShipButton_{};
    QPushButton* buildShipButton_{};
    QPushButton* buildFactoryButton_{};
    QPushButton* loadColonistsButton_{};
    QPushButton* refuelButton_{};
    QPushButton* colonizeButton_{};
    QPushButton* endTurnButton_{};
};

} // namespace suns
