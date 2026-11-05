#pragma once

#include "save_game.hpp"
#include "suns/campaign.hpp"
#include <QObject>
#include <QStringList>
#include <memory>
#include <set>
#include <utility>

namespace suns {

// Stable IDs only; the selected planet is resolved from its star in this game.
struct WorkspaceSelectionContext {
    std::optional<StarId> star;
    std::optional<FleetId> fleet{1};
    bool operator==(const WorkspaceSelectionContext&) const = default;
};

// Canonical document data. MainWindow is a controller/view, never its owner.
// Mutable access is confined to that controller; reference/tool views consume
// receivedState rather than authoritative host data.
class GameSession final : public QObject {
    Q_OBJECT
public:
    enum class Change { Planning, Panels, Selection, Messages, Metadata, Document };
    Q_ENUM(Change)
    GameSession();
    const GameState& state() const { return data_.state; }
    const GameState& receivedState() const;
    const GameState& plannedState() const;
    bool select(WorkspaceSelectionContext selection, QObject* origin = nullptr);
    const PlayerOrders& pendingOrders() const { return data_.pendingOrders; }
    const WorkspaceSelectionContext& selection() const { return data_.selection; }
    const QStringList& pendingDescriptions() const { return data_.pendingDescriptions; }
    std::uint64_t revision() const { return data_.planningRevision; }
    SaveGameData snapshot() const;

signals:
    void changed(suns::GameSession::Change change, QObject* origin);

private:
    friend class MainWindow;
    class DocumentUpdate {
    public:
        DocumentUpdate(const DocumentUpdate&) = delete;
        DocumentUpdate& operator=(const DocumentUpdate&) = delete;
        DocumentUpdate(DocumentUpdate&& other) noexcept
            : session_(std::exchange(other.session_, nullptr)), origin_(other.origin_) {}
        ~DocumentUpdate() { commit(); }
        void commit();
    private:
        friend class GameSession;
        DocumentUpdate(GameSession* session, QObject* origin) : session_(session), origin_(origin) {}
        GameSession* session_;
        QObject* origin_;
    };
    DocumentUpdate beginDocumentUpdate(QObject* origin);
    unsigned documentUpdateDepth_{};
    struct Data {
        GalaxyConfig galaxyConfig;
        GameState state;
        std::uint64_t planningRevision{};
        PlayerOrders pendingOrders{1, {}};
        SessionMode sessionMode{SessionMode::Solo};
        std::vector<EmpireSetup> empireSetups;
        std::map<PlayerId, std::uint64_t> playerTokens;
        std::vector<PlayerOrders> inbox;
        std::vector<GameEvent> campaignMessages;
        std::uint64_t campaignId{}, turnToken{};
        WorkspaceSelectionContext selection;
        int currentDistanceSelectionKind{}, previousDistanceSelectionKind{};
        std::uint32_t currentDistanceSelectionId{}, previousDistanceSelectionId{};
        QStringList pendingDescriptions;
        QString currentSavePath;
        std::vector<GameEvent> turnMessages;
        std::set<std::uint64_t> readTurnMessageIds;
        // Derived received/accepted views are shared too. Selection never
        // invalidates them; no world/year simulation is performed here.
        mutable std::uint64_t movementPreviewRevision{std::uint64_t(-1)};
        mutable PlayerId movementPreviewPlayer{};
        mutable std::optional<GameState> movementPreview;
        mutable std::uint64_t movementPreviewBuildCount{};
        mutable std::optional<GameState> receivedPlayerState;
        mutable std::uint64_t receivedPlayerRevision{std::uint64_t(-1)};
        mutable PlayerId receivedPlayerId{};
        mutable std::uint64_t fleetPlanningRevision{std::uint64_t(-1)};
        mutable std::optional<GameState> fleetPlanningState;
    } data_;
    void resetExchangeIdentity();
    void rotateTurnToken();
    void publishPlanning(Change, QObject* origin);
    void publishSelection(QObject* origin);
    void publishMetadata(QObject* origin);
    void publishMessages(QObject* origin);
    void replaceDocument(QObject* origin);
    std::uint64_t publishedRevision_{};
    WorkspaceSelectionContext publishedSelection_;
    std::array<std::uint64_t, 4> publishedSelectionHistory_{};
    QString publishedMetadata_;
};
using GameSessionPtr = std::shared_ptr<GameSession>;
} // namespace suns
