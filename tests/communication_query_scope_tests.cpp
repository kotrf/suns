#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "save_game.hpp"

#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <cassert>
#include <future>
#include <stdexcept>

namespace {
using namespace suns;

GameState fixture()
{
    auto state = make_demo_game();
    state.stars[0].position = {0, 0};
    state.stars[1].position = {250, 0};
    state.stars[2].position = {900, 900};
    auto& capital = state.planets[0];
    capital.owner = 1;
    capital.homeworld = true;
    capital.population = 1000;
    capital.radar = PlanetaryRadarLevel::Full;
    auto& remote = state.planets[1];
    remote.owner = 1;
    remote.homeworld = false;
    remote.population = 1000;
    remote.radar = PlanetaryRadarLevel::Local;
    remote.defenses = 77;
    if (!find_player(state, 2)) state.players.push_back({2, "Hidden"});
    state.fleets.resize(1);
    auto relay = state.fleets.front();
    relay.id = state.nextFleetId++;
    relay.position = {170, 0};
    relay.electronics = {};
    relay.telemetry = {};
    state.fleets.push_back(relay);
    auto hidden = relay;
    hidden.id = state.nextFleetId++;
    hidden.owner = 2;
    hidden.position = {10000, 10000};
    const auto* own = find_ship_design(state, hidden.design);
    auto design = *own;
    design.id = state.nextShipDesignId++;
    design.owner = 2;
    hidden.design = design.id;
    hidden.ships = {{design.id, 1}};
    state.shipDesigns.push_back(design);
    state.fleets.push_back(hidden);
    return state;
}

void sameRoute(const CommunicationRoute& a, const CommunicationRoute& b)
{
    assert(a.channel == b.channel && a.transmitter == b.transmitter && a.receiver == b.receiver);
    assert(a.propagationTurns == b.propagationTurns && a.receiverDelay == b.receiverDelay);
}

using Routes = std::array<CommunicationRoute, 24>;
Routes routes(const GameState& state)
{
    Routes result;
    std::size_t index = 0;
    for (const auto player : {PlayerId(1), PlayerId(2)})
        for (const auto turn : {std::uint64_t(0), state.turn, state.turn + 4})
            for (const auto position : {Position{0, 0}, Position{170, 0}, Position{250, 0}, Position{500, 0}}) {
                result[index++] = communication_route(state, player, position, turn);
                assert(communication_delay_turns(state, player, position, turn)
                    == result[index - 1].propagationTurns + result[index - 1].receiverDelay);
            }
    return result;
}

void sameRoutes(const Routes& a, const Routes& b)
{
    for (std::size_t i = 0; i < a.size(); ++i) sameRoute(a[i], b[i]);
}

QByteArray viewBytes(const GameState& state, bool cached)
{
    // The disabled outer scope propagates through make_player_view's normal
    // inner scope, giving a true uncached reference for the entire export.
    const CommunicationQueryScope outer(state, cached);
    SaveGameData data;
    data.mode = SessionMode::PlayerTurn;
    data.campaignId = 1;
    data.turnToken = 1;
    data.state = make_player_view(state, 1).state;
    data.pendingOrders = {1, {}};
    data.galaxyConfig.starCount = data.state.stars.size();
    QTemporaryDir temporary;
    assert(temporary.isValid());
    const auto path = temporary.filePath("view.sunsturn");
    QString error;
    assert(write_save_game_file(path, data, error));
    QFile file(path);
    assert(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

void routes_keep_identity_keys_and_snapshot_boundaries()
{
    auto state = fixture();
    auto other = state;
    other.fleets[1].electronics = {EmissionMode::RadioSilence, state.turn + 3};
    const auto baseline = routes(state);
    const auto otherBaseline = routes(other);
    assert(baseline[2].propagationTurns == 0 && otherBaseline[2].propagationTurns > 0);
    {
        const CommunicationQueryScope outer(state);
        sameRoutes(routes(state), baseline);
        sameRoutes(routes(state), baseline);
        // An unscoped derived snapshot must bypass the active host's graph.
        sameRoutes(routes(other), otherBaseline);
        {
            const CommunicationQueryScope nested(other);
            sameRoutes(routes(other), otherBaseline);
            sameRoutes(routes(state), baseline);
        }
        sameRoutes(routes(state), baseline);
        try {
            const CommunicationQueryScope unwound(other);
            sameRoutes(routes(other), otherBaseline);
            throw std::runtime_error("scope unwind");
        } catch (const std::runtime_error&) {}
        sameRoutes(routes(state), baseline);
    }
    // Same object, player and year, but new physical evidence: no cache from
    // the previous lexical lifetime may survive this transition.
    state.fleets[1].electronics = other.fleets[1].electronics;
    const auto changed = routes(state);
    assert(changed[2].propagationTurns > 0);
    {
        const CommunicationQueryScope refreshed(state);
        sameRoutes(routes(state), changed);
    }
    sameRoutes(routes(state), changed);
}

void views_match_uncached_and_keep_hidden_evidence_private()
{
    auto state = fixture();
    const auto baseline = viewBytes(state, false);
    assert(viewBytes(state, true) == baseline);
    auto hidden = state;
    hidden.fleets.back().name = "Undelivered rename";
    hidden.fleets.back().fuel *= 0.5;
    hidden.fleets.back().damagePercent = 27;
    hidden.planets.back().habitability = 42;
    assert(viewBytes(hidden, false) == baseline);
    assert(viewBytes(hidden, true) == baseline);
    state.planets.front().radar = PlanetaryRadarLevel::None;
    assert(viewBytes(state, true) == viewBytes(state, false));
    state.planets.front().radar = PlanetaryRadarLevel::Full;
    assert(viewBytes(state, true) == baseline);
}

void concurrent_snapshots_never_share_query_frames()
{
    auto first = fixture();
    auto second = first;
    second.fleets[1].electronics = {EmissionMode::RadioSilence, first.turn + 3};
    const auto a = routes(first);
    const auto b = routes(second);
    assert(a[2].propagationTurns != b[2].propagationTurns);
    const auto worker = [](const GameState& state, const Routes& expected) {
        for (int i = 0; i < 8; ++i) {
            const CommunicationQueryScope scope(state);
            sameRoutes(routes(state), expected);
            assert(viewBytes(state, true) == viewBytes(state, false));
        }
    };
    auto left = std::async(std::launch::async, [&] { worker(first, a); });
    auto right = std::async(std::launch::async, [&] { worker(second, b); });
    left.get();
    right.get();
}
} // namespace

int main()
{
    routes_keep_identity_keys_and_snapshot_boundaries();
    views_match_uncached_and_keep_hidden_evidence_private();
    concurrent_snapshots_never_share_query_frames();
}
