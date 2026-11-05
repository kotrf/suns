#include "main_window.hpp"
#include "ship_designer_dialog.hpp"

#include <QGraphicsScene>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <utility>

namespace suns {
void MainWindow::scheduleSessionRefresh(GameSession::Change change)
{
    switch (change) {
    case GameSession::Change::Planning: sessionPlanningRefresh_ = true; break;
    case GameSession::Change::Panels: sessionPanelsRefresh_ = true; break;
    case GameSession::Change::Selection: sessionSelectionRefresh_ = true; break;
    case GameSession::Change::Messages: sessionMessagesRefresh_ = true; break;
    case GameSession::Change::Metadata: sessionMetadataRefresh_ = true; break;
    case GameSession::Change::Document: sessionDocumentRefresh_ = true; break;
    }
    if (sessionRefreshPending_) return;
    sessionRefreshPending_ = true;
    // Signals only announce a completed document/planning boundary. Coalesce
    // peer updates and never destroy a scene while a mouse event is in flight.
    QTimer::singleShot(0, this, [this] {
        sessionRefreshPending_ = false;
        if (shuttingDown_) return;
        const bool document = std::exchange(sessionDocumentRefresh_, false);
        const bool planning = std::exchange(sessionPlanningRefresh_, false);
        const bool panels = std::exchange(sessionPanelsRefresh_, false);
        const bool selection = std::exchange(sessionSelectionRefresh_, false);
        const bool messages = std::exchange(sessionMessagesRefresh_, false);
        const bool metadata = std::exchange(sessionMetadataRefresh_, false);
        if (document) {
            cancelRouteProgramMapTargetPick();
            if (shipDesigner_) shipDesigner_->close();
            historyRangeInitialized_ = false;
            warpControlFleetId_.reset();
            logisticsControlFleetId_.reset();
            emit routeProgramContextChanged(true);
            scene_->setSceneRect(-galaxyConfig_.width / 2.0 - 55.0, -galaxyConfig_.height / 2.0 - 55.0,
                galaxyConfig_.width + 110.0, galaxyConfig_.height + 110.0);
        }
        if (document) {
            const QSignalBlocker seedBlock(seedEdit_);
            const QSignalBlocker countBlock(starCountSpin_);
            seedEdit_->setText(QString::number(qulonglong(galaxyConfig_.seed)));
            starCountSpin_->setValue(int(galaxyConfig_.starCount));
        }
        if (metadata || document) updateSaveWindowTitle();
        if (document || planning) {
            refreshShipDesignChoices();
            rebuildScene(false); // The shared revision was already advanced by its author.
            refreshProductionQueue();
            refreshEmpireHistory();
        } else if (panels) {
            // Production/research edits keep the same physical map/projections
            // in every view, exactly as in the original single-window fast path.
            renderedPlanningRevision_ = planningRevision_;
            updateControls(false);
            refreshProductionQueue();
            emit routeProgramContextChanged();
            // A coalesced panel edit must not swallow an accompanying map
            // selection; its deferred handler still preserves star-only items.
            if (selection) queueMapSelectionRebuild();
        } else if (selection) {
            queueMapSelectionRebuild();
        } else if (metadata) updateControls(false);
        if (document || messages) refreshTurnMessages();
    });
}
} // namespace suns
