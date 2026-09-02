#include "suns/game_event.hpp"
#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>

namespace {

using namespace suns;

const GameEvent* find_event(const std::vector<GameEvent>& events, GameEventKind kind)
{
    const auto event = std::find_if(events.begin(), events.end(), [&](const GameEvent& candidate) {
        return candidate.kind == kind;
    });
    return event == events.end() ? nullptr : &*event;
}

GameState colonization_fixture(std::uint16_t reward)
{
    auto state = make_demo_game();
    set_survey_level(state, 1, 2, SurveyLevel::OrbitalSurvey, state.turn);
    state.planets[1].precursorArtifacts = {true, false, 0, reward};
    state.players.front().technology.focus = ResearchField::Electronics;
    state.players.front().technology.researchActive = false;

    const auto* target = find_star(state, 2);
    assert(target);
    auto& colonizer = state.fleets.front();
    colonizer.position = target->position;
    colonizer.design = kColonyShipDesignId;
    colonizer.ships = {{kColonyShipDesignId, 1}};
    colonizer.role = FleetRole::ColonyShip;
    colonizer.colonists = 500;
    return state;
}

void assignment_is_stable_and_rare()
{
    constexpr std::uint64_t seed = 0x123456789abcdef0ULL;
    std::uint32_t sites = 0;
    for (PlanetId planet = 2; planet <= 10001; ++planet) {
        const auto first = generated_precursor_artifact_site(seed, planet);
        const auto replay = generated_precursor_artifact_site(seed, planet);
        assert(first.present == replay.present);
        assert(first.researchPoints == replay.researchPoints);
        assert(first.researchPoints >= 8 && first.researchPoints <= 14);
        if (first.present) ++sites;
    }
    // The precise occurrence rate is a balance decision; this only guards the
    // intended "rare, but real" category.
    assert(sites > 0 && sites < 1000);
}

void successful_colonization_claims_site_and_advances_paused_research()
{
    auto state = colonization_fixture(12);
    PlayerOrders orders{1, {ColonizePlanetOrder{1, 2}}};
    const auto result = TurnProcessor{}.process_with_events(state, {orders});

    const auto& planet = result.state.planets[1];
    assert(planet.owner == 1);
    assert(planet.precursorArtifacts.present);
    assert(planet.precursorArtifacts.claimed);
    assert(planet.precursorArtifacts.discoveredBy == 1);
    assert(result.state.players.front().technology.progress[3] == 12);

    const auto* discovery = find_event(
        result.events, GameEventKind::PrecursorArtifactsDiscovered);
    assert(discovery);
    assert(discovery->recipient == 1);
    assert(discovery->planet == 2);
    assert(discovery->quantity == 12);
    assert(discovery->researchField == ResearchField::Electronics);
}

void failed_and_repeat_colonization_cannot_consume_or_duplicate_site()
{
    auto failed = colonization_fixture(10);
    failed.fleets.front().colonists = 0;
    const auto failedResult = TurnProcessor{}.process_with_events(
        failed, {{1, {ColonizePlanetOrder{1, 2}}}});
    assert(!failedResult.state.planets[1].precursorArtifacts.claimed);
    assert(find_event(
        failedResult.events, GameEventKind::PrecursorArtifactsDiscovered) == nullptr);

    auto first = colonization_fixture(10);
    auto claimed = TurnProcessor{}.process_with_events(
        first, {{1, {ColonizePlanetOrder{1, 2}}}}).state;
    assert(claimed.players.front().technology.progress[3] == 10);

    auto& planet = claimed.planets[1];
    planet.owner = 0;
    planet.population = 0;
    const auto* target = find_star(claimed, 2);
    assert(target);
    claimed.fleets.push_back({
        claimed.nextFleetId++, 1, "Second Colonizer", FleetRole::ColonyShip,
        kColonyShipDesignId, target->position, std::nullopt, kColonyShipCruiseWarp,
        100.0, 500,
    });
    const auto fleetId = claimed.fleets.back().id;
    const auto repeated = TurnProcessor{}.process_with_events(
        claimed, {{1, {ColonizePlanetOrder{fleetId, 2}}}});
    assert(repeated.state.planets[1].owner == 1);
    assert(repeated.state.players.front().technology.progress[3] == 10);
    assert(find_event(
        repeated.events, GameEventKind::PrecursorArtifactsDiscovered) == nullptr);
}

void artifact_progress_completes_levels_through_the_normal_technology_state()
{
    auto state = colonization_fixture(10);
    state.players.front().technology.progress[3] = 15;
    const auto result = TurnProcessor{}.process_with_events(
        state, {{1, {ColonizePlanetOrder{1, 2}}}});

    const auto& technology = result.state.players.front().technology;
    assert(technology.levels[3] == 1);
    assert(technology.progress[3] == 7);
    const auto* completion = find_event(
        result.events, GameEventKind::ResearchLevelCompleted);
    assert(completion);
    assert(completion->researchField == ResearchField::Electronics);
    assert(completion->technologyLevel == 1);
}

} // namespace

int main()
{
    assignment_is_stable_and_rare();
    successful_colonization_claims_site_and_advances_paused_research();
    failed_and_repeat_colonization_cannot_consume_or_duplicate_site();
    artifact_progress_completes_levels_through_the_normal_technology_state();
}
