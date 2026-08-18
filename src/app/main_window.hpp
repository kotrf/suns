#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QEvent;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QObject;
class QPushButton;
class QSpinBox;

namespace suns {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Route-program dock API. Kept outside the main map implementation so the
    // increasingly rich fleet-program UI can evolve independently.
    [[nodiscard]] FleetId selectedFleetForRouteProgram() const;
    [[nodiscard]] std::uint8_t selectedFleetMaxWarpForRouteProgram() const;
    [[nodiscard]] std::uint8_t selectedFleetSuggestedWarpForRouteProgram() const;
    [[nodiscard]] QString selectedFleetRouteProgramSummary() const;
    [[nodiscard]] std::vector<Position> selectedFleetRouteProgramPolyline() const;
    [[nodiscard]] QGraphicsScene* routeProgramScene() const { return scene_; }
    bool appendSelectedStarWaypoint(std::uint8_t warp, FleetArrivalAction arrivalAction);
    bool clearSelectedFleetRouteProgram();

    // Dockside cargo editor entrypoint. Implemented separately from the map UI
    // so logistics can evolve without further inflating main_window.cpp.
    void openCargoManifestDialog();

    // Replace the constructor's legacy synchronous scene-selection callback
    // with a deferred rebuild. Rebuilding QGraphicsScene while Qt is still
    // delivering selectionChanged may destroy the item currently being clicked.
    void installDeferredMapSelectionHandler();

    // Responsive Full-HD layout, visual theme and map navigation controls.
    // Installed after all docks have been attached.
    void installUiPolish();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

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
    void zoomMap(double factor);
    void fitGalaxyView();

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
    bool mapSelectionRebuildPending_{};
    bool shuttingDown_{};

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
