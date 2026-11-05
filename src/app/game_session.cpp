#include "game_session.hpp"
#include "suns/communications.hpp"
#include <algorithm>
#include <QRandomGenerator>

namespace suns {
namespace {
std::uint64_t exchangeId()
{
    std::uint64_t value{};
    while (!value) value = QRandomGenerator::global()->generate64();
    return value;
}
}
GameSession::GameSession() : QObject(nullptr)
{
    data_.state = generate_campaign(data_.galaxyConfig, {{"Terrans", RacePreset::Terran}});
    resetExchangeIdentity();
}
void GameSession::resetExchangeIdentity()
{
    data_.campaignId = exchangeId();
    rotateTurnToken();
}
void GameSession::rotateTurnToken()
{
    data_.turnToken = exchangeId();
    data_.playerTokens.clear();
    if (data_.sessionMode == SessionMode::Host) {
        for (const auto& player : data_.state.players)
            if (player.controller == PlayerController::Human) data_.playerTokens[player.id] = exchangeId();
        data_.turnToken = data_.playerTokens.at(data_.pendingOrders.player);
    }
}
SaveGameData GameSession::snapshot() const
{
    SaveGameData save;
    save.campaignId = data_.campaignId;
    save.turnToken = data_.turnToken;
    save.galaxyConfig = data_.galaxyConfig;
    save.state = data_.state;
    save.pendingOrders = data_.pendingOrders;
    save.pendingDescriptions = data_.pendingDescriptions;
    save.mode = data_.sessionMode;
    save.playerTokens = data_.playerTokens;
    save.inbox = data_.inbox;
    save.strategicMessages = data_.sessionMode == SessionMode::Host ? data_.campaignMessages : data_.turnMessages;
    return save;
}
const GameState& GameSession::receivedState() const
{
    if (!data_.receivedPlayerState || data_.receivedPlayerRevision != data_.planningRevision
        || data_.receivedPlayerId != data_.pendingOrders.player || data_.receivedPlayerState->turn != data_.state.turn) {
        data_.receivedPlayerState = make_player_view(data_.state, data_.pendingOrders.player).state;
        data_.receivedPlayerRevision = data_.planningRevision;
        data_.receivedPlayerId = data_.pendingOrders.player;
    }
    return *data_.receivedPlayerState;
}
const GameState& GameSession::plannedState() const
{
    if (!data_.fleetPlanningState || data_.fleetPlanningRevision != data_.planningRevision) {
        auto received = receivedState();
        for (auto& fleet : received.fleets) {
            if (fleet.owner != data_.pendingOrders.player) continue;
            const auto original = std::find_if(data_.state.fleets.begin(), data_.state.fleets.end(),
                [&](const auto& candidate) { return candidate.id == fleet.id; });
            if (original != data_.state.fleets.end() && original->owner == data_.pendingOrders.player)
                fleet.pendingCommands = original->pendingCommands; // Sent owner intent, never live physical manifests.
        }
        data_.fleetPlanningState = projected_cargo_order_state(received, data_.pendingOrders);
        for (auto& fleet : data_.fleetPlanningState->fleets) {
            if (fleet.owner != data_.pendingOrders.player || fleet_telemetry_age(*data_.fleetPlanningState, fleet) != 0
                || !fleet_has_instant_link(*data_.fleetPlanningState, fleet)) continue;
            fleet.telemetry = {};
            fleet.telemetry = confirmed_fleet_telemetry(*data_.fleetPlanningState, fleet);
        }
        data_.fleetPlanningRevision = data_.planningRevision;
    }
    return *data_.fleetPlanningState;
}
bool GameSession::select(WorkspaceSelectionContext value, QObject* origin)
{
    if (value.star && value.star != data_.selection.star && !find_star(data_.state, *value.star)) return false;
    if (value.fleet && value.fleet != data_.selection.fleet) {
        const auto& planned = plannedState();
        const auto missing = missing_fleet_contacts(data_.state, data_.pendingOrders.player);
        if (std::none_of(planned.fleets.begin(), planned.fleets.end(), [&](const auto& f) { return f.id == *value.fleet; })
            && std::none_of(missing.begin(), missing.end(), [&](const auto& f) { return f.id == *value.fleet; })) return false;
    }
    const auto remember = [&](int kind, std::uint32_t id) {
        if (kind == data_.currentDistanceSelectionKind && id == data_.currentDistanceSelectionId) return;
        data_.previousDistanceSelectionKind = data_.currentDistanceSelectionKind;
        data_.previousDistanceSelectionId = data_.currentDistanceSelectionId;
        data_.currentDistanceSelectionKind = kind; data_.currentDistanceSelectionId = id;
    };
    if (value.fleet && value.fleet != data_.selection.fleet) remember(2, *value.fleet);
    else if (value.star && value.star != data_.selection.star) remember(1, *value.star);
    data_.selection = value;
    publishSelection(origin);
    return true;
}
void GameSession::publishPlanning(Change change, QObject* origin)
{
    if (documentUpdateDepth_) return;
    if (publishedRevision_ != data_.planningRevision) {
        publishedRevision_ = data_.planningRevision;
        emit changed(change, origin);
    }
    publishSelection(origin);
    publishMetadata(origin);
}
void GameSession::publishSelection(QObject* origin)
{
    if (documentUpdateDepth_) return;
    const std::array<std::uint64_t, 4> history{std::uint64_t(data_.currentDistanceSelectionKind), data_.currentDistanceSelectionId,
        std::uint64_t(data_.previousDistanceSelectionKind), data_.previousDistanceSelectionId};
    if (publishedSelection_ == data_.selection && publishedSelectionHistory_ == history) return;
    publishedSelection_ = data_.selection;
    publishedSelectionHistory_ = history;
    emit changed(Change::Selection, origin);
}
void GameSession::publishMetadata(QObject* origin)
{
    if (documentUpdateDepth_) return;
    // Small document/header fields only, not a hash/copy of the campaign.
    auto stamp = QString("%1/%2/%3/%4/%5/%6/%7/%8/")
        .arg(qulonglong(data_.campaignId)).arg(qulonglong(data_.turnToken))
        .arg(qulonglong(data_.state.turn)).arg(int(data_.sessionMode))
        .arg(data_.pendingOrders.player).arg(qulonglong(data_.inbox.size()))
        .arg(qulonglong(data_.playerTokens.size())).arg(data_.currentSavePath);
    for (const auto& orders : data_.inbox) stamp += QString("%1:%2;").arg(orders.player).arg(qulonglong(orders.orders.size()));
    if (stamp == publishedMetadata_) return;
    publishedMetadata_ = std::move(stamp);
    emit changed(Change::Metadata, origin);
}
void GameSession::publishMessages(QObject* origin)
{
    if (!documentUpdateDepth_) emit changed(Change::Messages, origin);
}
GameSession::DocumentUpdate GameSession::beginDocumentUpdate(QObject* origin)
{
    ++documentUpdateDepth_;
    return DocumentUpdate(this, origin);
}
void GameSession::DocumentUpdate::commit()
{
    auto* session = std::exchange(session_, nullptr);
    if (session && --session->documentUpdateDepth_ == 0) session->replaceDocument(origin_);
}
void GameSession::replaceDocument(QObject* origin)
{
    ++documentGeneration_; // Even reopening the same file invalidates its old editor drafts.
    ++data_.planningRevision; // Receivers may immediately query the replacement's received view.
    publishPlanning(Change::Document, origin);
}
} // namespace suns
