#include "suns/game_state.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/turn_processor.hpp"

#include <cassert>
#include <cmath>
#include <algorithm>

namespace {

bool close(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

} // namespace

int main()
{
    auto state = suns::make_demo_game();
    assert(state.players.size() == 1);
    const auto& initialHistory = state.players.front().history;
    assert(initialHistory.size() == 1);
    assert(initialHistory.front().turn == 1);
    assert(initialHistory.front().colonies == 1);
    assert(initialHistory.front().population == 1'000'000);
    assert(initialHistory.front().factories == 4);
    assert(initialHistory.front().fleets == 1);
    assert(initialHistory.front().ships == 1);
    assert(initialHistory.front().fleetMass > 0.0);
    assert(close(initialHistory.front().minerals.ironium, 100.0));
    assert(initialHistory.front().colonyHistory.size() == 1);
    assert(initialHistory.front().colonyHistory.front().planet == state.planets.front().id);
    assert(initialHistory.front().colonyHistory.front().population == 1'000'000);
    assert(close(initialHistory.front().extraction.ironium, 0.0));
    assert(!initialHistory.front().extractionRecorded);
    assert(!initialHistory.front().freightRecorded);

    const suns::TurnProcessor processor;
    const auto first = processor.process(state, {});
    const auto replay = processor.process(state, {});
    assert(first.turn == 2);
    assert(first.players.front().history.size() == 2);
    assert(first.players.front().history.back().turn == 2);
    assert(first.players.front().history.back().population > 1'000'000);
    assert(first.players.front().history.back().population
        == replay.players.front().history.back().population);
    assert(close(
        first.players.front().history.back().minerals.germanium,
        replay.players.front().history.back().minerals.germanium));
    const auto expectedMining = suns::projected_mineral_mining(state, state.planets.front());
    const auto& mined = first.players.front().history.back();
    assert(mined.extractionRecorded);
    assert(close(mined.extraction.ironium, expectedMining.ironium));
    assert(close(mined.extraction.boranium, expectedMining.boranium));
    assert(close(mined.colonyHistory.front().extraction.germanium, expectedMining.germanium));
    assert(close(mined.extraction.ironium, replay.players.front().history.back().extraction.ironium));

    // Re-recording one planning boundary replaces its snapshot rather than
    // manufacturing a duplicate sample.
    auto corrected = first;
    corrected.planets.front().population += 50;
    suns::record_empire_turn_statistics(corrected);
    assert(corrected.players.front().history.size() == 2);
    assert(corrected.players.front().history.back().population
        == first.players.front().history.back().population + 50);
    assert(corrected.players.front().history.back().colonyHistory.front().population
        == first.players.front().history.back().colonyHistory.front().population + 50);
    assert(corrected.players.front().history.back().extractionRecorded);
    assert(close(corrected.players.front().history.back().extraction.ironium, mined.extraction.ironium));
    assert(corrected.players.front().history.back().freightRecorded);

    // A player's history is built only from assets they own. Authoritative
    // enemy truth and neutral surface stockpiles never leak into the record.
    auto hidden = state;
    hidden.players.push_back({2, "Visitors", {}});
    hidden.planets[1].owner = 2;
    hidden.planets[1].population = 999999;
    hidden.planets[1].minerals = {888.0, 777.0, 666.0};
    hidden.fleets.push_back({
        50, 2, "Hidden fleet", suns::FleetRole::Scout, suns::kScoutDesignId,
        {20.0, 20.0}, std::nullopt, 1, 0.0, 0,
    });
    const auto playerOne = suns::empire_turn_statistics(hidden, 1);
    assert(playerOne.population == initialHistory.front().population);
    assert(close(playerOne.minerals.ironium, initialHistory.front().minerals.ironium));
    assert(playerOne.fleets == initialHistory.front().fleets);
    assert(playerOne.colonyHistory.size() == 1);
    assert(playerOne.colonyHistory.front().planet != hidden.planets[1].id);

    // Record mining under the empire which mined it even when a world changes
    // hands before the next boundary; the winner gets no historical credit.
    auto conquered = hidden;
    conquered.turn = 2;
    conquered.planets.front().owner = 2;
    suns::record_empire_turn_statistics(conquered, {{state.planets.front().id, 1, {3.0, 2.0, 1.0}}}, true);
    assert(close(conquered.players[0].history.back().extraction.ironium, 3.0));
    assert(close(conquered.players[1].history.back().extraction.ironium, 0.0));
    assert(close(conquered.players[1].history.back().colonyHistory.front().extraction.ironium, 0.0));

    // Delivered cargo is credited to the receiving player's colony, not to
    // loads or ship-to-ship handling. A waypoint unload is counted once.
    auto freightState = state;
    freightState.fleets.push_back({2, 1, "Freighter", suns::FleetRole::ColonyShip,
        suns::kColonyShipDesignId, freightState.stars[0].position, std::nullopt,
        8, 100.0, 10000});
    freightState.fleets.back().minerals = {2.0, 3.0, 4.0};
    suns::PlayerOrders delivery{1, {suns::TransferCargoOrder{
        {0, 2}, {freightState.planets.front().id, 0}, 5000, {1.0, 2.0, 3.0}}}};
    const auto delivered = processor.process(freightState, {delivery});
    const auto& deliveredHistory = delivered.players.front().history.back();
    assert(deliveredHistory.freightRecorded);
    assert(close(deliveredHistory.freightDelivered.ironium, 1.0));
    assert(close(deliveredHistory.freightDelivered.boranium, 2.0));
    assert(deliveredHistory.colonistsDelivered == 5000);
    assert(close(deliveredHistory.colonyHistory.front().freightDelivered.germanium, 3.0));
    const auto deliveredEvents = processor.process_with_events(freightState, {delivery});
    const auto cargoMessage = std::find_if(deliveredEvents.events.begin(), deliveredEvents.events.end(), [](const auto& event) {
        return event.kind == suns::GameEventKind::FreightDelivered;
    });
    assert(cargoMessage != deliveredEvents.events.end());
    assert(cargoMessage->planet == freightState.planets.front().id);
    assert(close(cargoMessage->deliveredMinerals.ironium, 1.0));
    assert(cargoMessage->deliveredColonists == 5000);
    assert(cargoMessage->id == processor.process_with_events(freightState, {delivery}).events.back().id);
    assert(close(delivered.players.front().history.front().freightDelivered.ironium, 0.0));

    auto waypoint = delivered;
    auto& transport = waypoint.fleets.back();
    transport.destination = waypoint.stars.front().position;
    transport.arrivalAction = suns::FleetArrivalAction{
        suns::FleetArrivalActionKind::UnloadAll, 1, suns::FleetCargoKind::All};
    const auto unloaded = processor.process(waypoint, {});
    const auto& unloadHistory = unloaded.players.front().history.back();
    assert(close(unloadHistory.freightDelivered.ironium, 1.0));
    assert(close(unloadHistory.freightDelivered.boranium, 1.0));
    assert(unloadHistory.colonistsDelivered == 5000);
    const auto unloadEvents = processor.process_with_events(waypoint, {});
    assert(std::count_if(unloadEvents.events.begin(), unloadEvents.events.end(), [](const auto& event) {
        return event.kind == suns::GameEventKind::FreightDelivered;
    }) == 1);
    auto refreshedFreight = unloaded;
    suns::record_empire_turn_statistics(refreshedFreight);
    assert(refreshedFreight.players.front().history.back().freightRecorded);
    assert(close(refreshedFreight.players.front().history.back().freightDelivered.ironium, 1.0));

    // A colony changing owner later cannot retroactively award this haul to
    // its new owner; a former colony has no invented per-colony entry.
    auto lostFreight = freightState;
    lostFreight.players.push_back({2, "Visitors", {}});
    lostFreight.turn = 2;
    lostFreight.planets.front().owner = 2;
    suns::record_empire_turn_statistics(lostFreight, {}, true,
        {{state.planets.front().id, 1, {7.0, 0.0, 0.0}, 900}});
    assert(close(lostFreight.players[0].history.back().freightDelivered.ironium, 7.0));
    assert(lostFreight.players[0].history.back().colonistsDelivered == 900);
    assert(close(lostFreight.players[1].history.back().freightDelivered.ironium, 0.0));

    // Only delivered player-visible reports become compact history markers.
    auto briefing = state;
    briefing.players.push_back({2, "Visitors", {}});
    suns::record_empire_turn_statistics(briefing);
    briefing.players.front().technology.progress[
        static_cast<std::size_t>(suns::ResearchField::Electronics)] = 17;
    const auto* homeStar = suns::find_star(briefing, briefing.planets.front().star);
    assert(homeStar);
    suns::queue_player_report(briefing, 1, suns::PlayerReportKind::ColonyFounded,
        homeStar->position, 2, homeStar->id, briefing.planets.front().id);
    const auto report = processor.process_with_events(briefing, {});
    const auto rerun = processor.process_with_events(briefing, {});
    const auto& milestones = report.state.players[0].history.back().milestones;
    assert(milestones.size() == 2);
    const auto colony = std::find_if(milestones.begin(), milestones.end(), [](const auto& marker) {
        return marker.kind == suns::HistoryMilestoneKind::ColonyFounded;
    });
    const auto research = std::find_if(milestones.begin(), milestones.end(), [](const auto& marker) {
        return marker.kind == suns::HistoryMilestoneKind::ResearchCompleted;
    });
    assert(colony != milestones.end() && colony->planet == briefing.planets.front().id);
    assert(research != milestones.end());
    assert(research->researchField == suns::ResearchField::Electronics);
    assert(research->technologyLevel == 1);
    assert(std::any_of(rerun.state.players[0].history.back().milestones.begin(),
        rerun.state.players[0].history.back().milestones.end(), [&](const auto& marker) {
            return marker.eventId == research->eventId;
        }));
    assert(report.state.players[1].history.back().milestones.empty());
    auto fuelReport = state;
    suns::queue_player_report(fuelReport, 1, suns::PlayerReportKind::FleetStalledForFuel,
        homeStar->position, 2, 0, 0, fuelReport.fleets.front().id);
    const auto stalled = processor.process_with_events(fuelReport, {});
    const auto& fuelMarkers = stalled.state.players.front().history.back().milestones;
    assert(std::any_of(fuelMarkers.begin(), fuelMarkers.end(), [](const auto& marker) {
        return marker.kind == suns::HistoryMilestoneKind::FleetStalledForFuel && marker.fleet == 1;
    }));
    auto refreshed = report.state;
    suns::record_empire_turn_statistics(refreshed);
    assert(refreshed.players[0].history.back().milestones.size() == 2);

    // Each year records only current ownership, preserving past observations
    // when a colony is lost and providing a gap instead of a fictitious zero.
    auto changingOwner = state;
    changingOwner.planets[1].owner = 1;
    changingOwner.planets[1].population = 420;
    changingOwner.turn = 2;
    suns::record_empire_turn_statistics(changingOwner);
    assert(changingOwner.players.front().history.back().colonyHistory.size() == 2);
    changingOwner.planets[1].owner = 2;
    changingOwner.turn = 3;
    suns::record_empire_turn_statistics(changingOwner);
    assert(changingOwner.players.front().history.back().colonyHistory.size() == 1);
    assert(changingOwner.players.front().history[1].colonyHistory[1].population == 420);

    return 0;
}
