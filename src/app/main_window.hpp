#pragma once

#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <optional>
#include <set>
#include <vector>

class QCloseEvent;
class QDialog;
class QEvent;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QListWidget;
class QObject;
class QPushButton;
class QProgressBar;
class QSpinBox;
class QDockWidget;
class QTreeWidget;
class QWidget;
class QTextBrowser;

namespace suns {

class ShipDesignerDialog;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Route-program dock API. Kept outside the main map implementation so the
    // increasingly rich fleet-program UI can evolve independently.
    [[nodiscard]] FleetId selectedFleetForRouteProgram() const;
    [[nodiscard]] std::uint8_t selectedFleetMaxWarpForRouteProgram() const;
    [[nodiscard]] std::uint8_t selectedFleetSuggestedWarpForRouteProgram() const;
    [[nodiscard]] bool selectedFleetRepeatOrdersForRouteProgram() const;
    [[nodiscard]] QString selectedFleetRouteProgramSummary() const;
    [[nodiscard]] std::vector<Position> selectedFleetRouteProgramPolyline() const;
    [[nodiscard]] std::vector<FleetId> availableFleetTargetsForRouteProgram() const;
    [[nodiscard]] std::vector<FleetId> availableOwnedFleetsForRouteProgram() const;
    [[nodiscard]] QString fleetTargetNameForRouteProgram(FleetId fleet) const;
    bool selectFleetForRouteProgram(FleetId fleet);
    [[nodiscard]] QGraphicsScene* routeProgramScene() const { return scene_; }
    bool appendSelectedStarWaypoint(std::uint8_t warp, FleetArrivalAction arrivalAction);
    bool appendFleetTargetWaypoint(
        FleetId targetFleet, std::uint8_t warp, FleetArrivalAction arrivalAction);
    bool setSelectedFleetRepeatOrdersForRouteProgram(bool enabled);
    bool clearSelectedFleetRouteProgram();

    // Dockside cargo editor entrypoint. Implemented separately from the map UI
    // so logistics can evolve without further inflating main_window.cpp.
    void openCargoManifestDialog();
    void openMergeFleetsDialog();
    void openSplitFleetDialog();

    // Replace the constructor's legacy synchronous scene-selection callback
    // with a deferred rebuild. Rebuilding QGraphicsScene while Qt is still
    // delivering selectionChanged may destroy the item currently being clicked.
    void installDeferredMapSelectionHandler();

    // Responsive Full-HD layout, visual theme and map navigation controls.
    // Installed after all docks have been attached.
    void installUiPolish();

    // Frequently switched map-reading modes: physical stellar colour,
    // habitability heatmap and colony population marker size.
    void installMapDisplayModes();

    // Procedural orbital portraits, compact planet geology presentation and
    // reference material moved out of the always-visible command console.
    void installPlanetPolish();

    // Harden side-panel resizing after all command/dock content is installed.
    // Adds recovery UI and enables horizontal scrolling when narrow panels
    // cannot display a technical line without clipping it.
    void installPanelLayoutFixes();
    void resetPanelLayout();

    // Finish the information-dashboard pass: fleet fuel/cargo gauges, distinct
    // mineral colours, popup/dialog contrast and conventional menu ordering.
    void installFleetReadabilityPolish();

    // Add a compact, recognisable fleet portrait derived from the actual ship
    // design rather than a rigid role label. Custom hull/component fits therefore
    // receive correspondingly different silhouettes.
    void installFleetPortraitPolish();

    // Add buildable mines and expose the marginal extraction benefit of the
    // next mine on the currently selected colony.
    void installMiningInfrastructure();

    // Show delayed telemetry, estimated position and commands physically in flight.
    void installCommunicationStatus();

    // Player briefing backed by typed, fog-of-war-safe core events.
    void installTurnMessages();

    // Empire research allocation and ordered technology plan.
    void installResearch();

    // Ordered colony build list with completion forecasts and move controls.
    void installProductionQueue();

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
    void queueColonize();
    void endTurn();
    void newGalaxy();
    [[nodiscard]] bool installSaveMenuBootstrap();
    void saveGame();
    void saveGameAs();
    void openGame();
    bool saveGameToPath(const QString& path);
    bool loadGameFromPath(const QString& path);
    void exportTurnOrders();
    void importTurnOrders();
    void resetTurnExchangeIdentity();
    void rotateTurnExchangeToken();
    void updateSaveWindowTitle();
    void zoomMap(double factor);
    void fitGalaxyView();
    void applyMapDisplayMode();
    void refreshPlanetPolish();
    void appendTurnMessages(const std::vector<GameEvent>& events);
    void resetTurnMessages();
    void refreshTurnMessages();
    void openResearchDialog();
    void refreshResearchPanel();
    void queueResearchPlan();
    void queueResearchAllocation(int percent);
    void addResearchPlanItem();
    void moveSelectedResearchPlanItem(int direction);
    void removeSelectedResearchPlanItem();
    void refreshProductionQueue();
    void moveSelectedProductionItem(int direction);
    [[nodiscard]] bool confirmFleetColonization(
        const Fleet& fleet, const Planet& planet, bool scheduledRoute);

    [[nodiscard]] const StarSystem* selectedStar() const;
    [[nodiscard]] const Planet* selectedPlanet() const;
    [[nodiscard]] const Fleet* selectedFleet() const;
    [[nodiscard]] std::optional<Fleet> selectedFleetPlanningView() const;
    [[nodiscard]] const Fleet* selectedColonyShipAtSelectedStar() const;
    [[nodiscard]] const Planet* selectedFriendlyColonyForFleet() const;
    [[nodiscard]] QString selectedPlanetPanelSummary() const;
    [[nodiscard]] QString selectedFleetPanelSummary() const;
    bool appendRouteWaypoint(
        Position destination,
        FleetId targetFleet,
        std::uint8_t warp,
        FleetArrivalAction arrivalAction);

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
    std::uint64_t campaignId_{};
    std::uint64_t turnToken_{};
    std::optional<StarId> selectedStarId_;
    std::optional<FleetId> selectedFleetId_{1};
    std::optional<FleetId> warpControlFleetId_;
    std::optional<FleetId> logisticsControlFleetId_;
    QStringList pendingDescriptions_;
    QString currentSavePath_;
    int mapDisplayMode_{};
    bool showSensorRanges_{true};
    bool mapSelectionRebuildPending_{};
    bool mapDisplayApplyPending_{};
    bool planetPolishRefreshPending_{};
    bool shuttingDown_{};
    bool saveMenuBootstrap_{installSaveMenuBootstrap()};
    std::vector<GameEvent> turnMessages_;
    std::set<std::uint64_t> readTurnMessageIds_;
    std::set<QString> hiddenTurnMessageClasses_;
    QPointer<ShipDesignerDialog> shipDesigner_;

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
    QPushButton* buildOrbitalDockButton_{};
    QPushButton* loadColonistsButton_{};
    QPushButton* colonizeButton_{};
    QPushButton* endTurnButton_{};
    QDockWidget* turnMessagesDock_{};
    QListWidget* turnMessagesList_{};
    QLabel* turnMessagesSummary_{};
    QComboBox* turnMessageAgeFilter_{};
    QComboBox* turnMessageTypeFilter_{};
    QComboBox* turnMessagePlanetFilter_{};
    QTextBrowser* turnMessageBody_{};
    QPushButton* turnMessageHideSimilarButton_{};
    QPushButton* turnMessageShowOnMapButton_{};
    QPushButton* turnMessageRestoreHiddenButton_{};
    QWidget* planetEnvironmentPanel_{};
    QProgressBar* planetTemperatureBar_{};
    QProgressBar* planetGravityBar_{};
    QProgressBar* planetRadiationBar_{};
    QDialog* researchDialog_{};
    QLabel* researchSummary_{};
    QLabel* researchUnlock_{};
    QProgressBar* researchProgress_{};
    QTreeWidget* researchPlanTree_{};
    QComboBox* researchAddCombo_{};
    QPushButton* researchAddButton_{};
    QPushButton* researchMoveUpButton_{};
    QPushButton* researchMoveDownButton_{};
    QPushButton* researchRemoveButton_{};
    QSpinBox* researchAllocationSpin_{};
    QTreeWidget* productionQueueTree_{};
    QLabel* productionQueueSummary_{};
    QPushButton* productionMoveUpButton_{};
    QPushButton* productionMoveDownButton_{};
};

} // namespace suns
