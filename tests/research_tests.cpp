#include "suns/game_event.hpp"
#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"
#include "suns/production.hpp"
#include "suns/orbital_habitats.hpp"
#include "remote_receiver_fixture.hpp"

#include <algorithm>
#include <cassert>

namespace {

const suns::GameEvent* research_event(const std::vector<suns::GameEvent>& events)
{
    const auto it = std::find_if(events.begin(), events.end(), [](const suns::GameEvent& event) {
        return event.kind == suns::GameEventKind::ResearchLevelCompleted;
    });
    return it == events.end() ? nullptr : &*it;
}

void verify_research_costs_and_initial_unlocks()
{
    const auto state = suns::make_demo_game();
    assert(suns::research_level_cost(suns::ResearchField::Electronics, 1) == 18);
    assert(suns::research_level_cost(suns::ResearchField::Electronics, 2) == 36);
    assert(suns::research_level_cost(suns::ResearchField::Electronics, 3) == 72);
    assert(!suns::component_available_to_player(
        state, 1, suns::ShipComponentType::CompactLongRangeScanner));
    assert(!suns::component_available_to_player(
        state, 1, suns::ShipComponentType::PenetratingScanner));
    assert(suns::component_available_to_player(
        state, 1, suns::ShipComponentType::LongRangeScanner));
    assert(!suns::component_available_to_player(
        state, 1, suns::ShipComponentType::FieldRepairBay));
    auto researched = state;
    researched.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 2;
    assert(!suns::component_available_to_player(
        researched, 1, suns::ShipComponentType::FieldRepairBay));
    researched.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 3;
    assert(suns::component_available_to_player(
        researched, 1, suns::ShipComponentType::FieldRepairBay));
    const suns::ShipDesign repairScout{42, 1, "Repair Scout", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FieldRepairBay}};
    assert(suns::ship_design_valid(repairScout));
    assert(suns::ship_design_available_to_player(researched, 1, repairScout));
    assert(!suns::ship_design_available_to_player(state, 1, repairScout));
}

void verify_unused_colony_output_advances_focus_queue()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();

    suns::PlayerOrders start{1, {}};
    start.orders.emplace_back(suns::SetResearchPlanOrder{
        suns::ResearchField::Electronics,
        {suns::ResearchField::Propulsion, suns::ResearchField::Construction},
    });

    const auto turn2 = processor.process_with_events(state, {start});
    assert(turn2.state.players.front().technology.progress[3] == 6);
    assert(turn2.state.planets.front().productionQueue.empty());
    assert(research_event(turn2.events) == nullptr);

    const auto turn3 = processor.process_with_events(turn2.state, {});
    assert(turn3.state.players.front().technology.progress[3] == 12);
    assert(research_event(turn3.events) == nullptr);

    const auto turn4 = processor.process_with_events(turn3.state, {});
    const auto& technology = turn4.state.players.front().technology;
    assert(technology.levels[3] == 1);
    assert(technology.progress[3] == 0);
    assert(technology.focus == suns::ResearchField::Propulsion);
    assert(technology.queuedFocuses.size() == 1);
    assert(technology.queuedFocuses.front() == suns::ResearchField::Construction);
    assert(suns::component_available_to_player(
        turn4.state, 1, suns::ShipComponentType::CompactLongRangeScanner));
    assert(!suns::component_available_to_player(
        turn4.state, 1, suns::ShipComponentType::PenetratingScanner));

    const auto* event = research_event(turn4.events);
    assert(event != nullptr);
    assert(event->recipient == 1);
    assert(event->researchField == suns::ResearchField::Electronics);
    assert(event->technologyLevel == 1);

    const auto replay = processor.process_with_events(turn3.state, {});
    const auto* replayEvent = research_event(replay.events);
    assert(replayEvent != nullptr);
    assert(replayEvent->id == event->id);
}

void verify_overflow_follows_preselected_field()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();
    auto& technology = state.players.front().technology;
    technology.focus = suns::ResearchField::Electronics;
    technology.queuedFocuses = {
        suns::ResearchField::Propulsion,
        suns::ResearchField::Construction,
    };
    technology.progress[3] = 17;

    const auto result = processor.process_with_events(state, {});

    const auto& completed = result.state.players.front().technology;
    assert(completed.levels[3] == 1);
    assert(completed.focus == suns::ResearchField::Propulsion);
    assert(completed.progress[1] == 5);
    assert(completed.queuedFocuses.size() == 1);
    assert(completed.queuedFocuses.front() == suns::ResearchField::Construction);

}

void verify_future_plan_can_be_reordered_and_cleared()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();
    state.players.front().technology.queuedFocuses = {
        suns::ResearchField::Propulsion,
        suns::ResearchField::Construction,
    };

    suns::PlayerOrders reorder{1, {}};
    reorder.orders.emplace_back(suns::SetResearchPlanOrder{
        suns::ResearchField::Electronics,
        {suns::ResearchField::Construction, suns::ResearchField::Propulsion},
    });
    const auto reordered = processor.process(state, {reorder});
    assert(reordered.players.front().technology.focus == suns::ResearchField::Electronics);
    assert(reordered.players.front().technology.queuedFocuses.size() == 2);
    assert(reordered.players.front().technology.queuedFocuses[0] == suns::ResearchField::Construction);
    assert(reordered.players.front().technology.queuedFocuses[1] == suns::ResearchField::Propulsion);

    suns::PlayerOrders clear{1, {}};
    clear.orders.emplace_back(suns::SetResearchPlanOrder{
        suns::ResearchField::Electronics,
        {},
    });
    const auto cleared = processor.process(reordered, {clear});
    assert(cleared.players.front().technology.focus == suns::ResearchField::Electronics);
    assert(cleared.players.front().technology.queuedFocuses.empty());
}

void verify_active_research_can_be_moved_removed_and_resumed()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();
    auto& technology = state.players.front().technology;
    technology.progress[static_cast<std::size_t>(suns::ResearchField::Electronics)] = 7;
    technology.queuedFocuses = {
        suns::ResearchField::Propulsion,
        suns::ResearchField::Construction,
    };

    suns::PlayerOrders moveCurrent{1, {}};
    moveCurrent.orders.emplace_back(suns::SetResearchPlanOrder{
        suns::ResearchField::Propulsion,
        {suns::ResearchField::Construction, suns::ResearchField::Electronics},
    });
    auto moved = processor.process(state, {moveCurrent});
    const auto& movedTechnology = moved.players.front().technology;
    assert(movedTechnology.researchActive);
    assert(movedTechnology.focus == suns::ResearchField::Propulsion);
    assert(movedTechnology.progress[static_cast<std::size_t>(suns::ResearchField::Electronics)] == 7);
    assert(movedTechnology.progress[static_cast<std::size_t>(suns::ResearchField::Propulsion)] == 6);
    assert(movedTechnology.queuedFocuses.back() == suns::ResearchField::Electronics);

    suns::PlayerOrders pause{1, {}};
    pause.orders.emplace_back(suns::SetResearchPlanOrder{
        suns::ResearchField::Propulsion,
        {},
        false,
    });
    const auto paused = processor.process(moved, {pause});
    const auto& pausedTechnology = paused.players.front().technology;
    assert(!pausedTechnology.researchActive);
    assert(pausedTechnology.progress[static_cast<std::size_t>(suns::ResearchField::Electronics)] == 7);
    assert(pausedTechnology.progress[static_cast<std::size_t>(suns::ResearchField::Propulsion)] == 6);

    suns::PlayerOrders resume{1, {}};
    resume.orders.emplace_back(suns::SetResearchPlanOrder{
        suns::ResearchField::Electronics,
        {},
        true,
    });
    const auto resumed = processor.process(paused, {resume});
    const auto& resumedTechnology = resumed.players.front().technology;
    assert(resumedTechnology.researchActive);
    assert(resumedTechnology.focus == suns::ResearchField::Electronics);
    assert(resumedTechnology.levels[static_cast<std::size_t>(suns::ResearchField::Electronics)] == 0);
    assert(resumedTechnology.progress[static_cast<std::size_t>(suns::ResearchField::Electronics)] == 13);
}

void verify_component_unlock_is_enforced_for_new_designs()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();

    suns::PlayerOrders locked{1, {}};
    locked.orders.emplace_back(suns::CreateShipDesignOrder{
        "Compact Relay",
        suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::CompactLongRangeScanner},
    });
    auto result = processor.process(state, {locked});
    assert(suns::find_ship_design(result, suns::kFirstCustomShipDesignId) == nullptr);

    state.players.front().technology.levels[3] = 1;
    result = processor.process(state, {locked});
    assert(suns::find_ship_design(result, suns::kFirstCustomShipDesignId) != nullptr);

    suns::PlayerOrders penetrating{1, {}};
    penetrating.orders.emplace_back(suns::CreateShipDesignOrder{
        "Penetrating Surveyor",
        suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::PenetratingScanner},
    });
    auto stillLocked = processor.process(state, {penetrating});
    assert(suns::find_ship_design(stillLocked, suns::kFirstCustomShipDesignId) == nullptr);

    state.players.front().technology.levels[3] = 3;
    const auto unlocked = processor.process(state, {penetrating});
    assert(suns::find_ship_design(unlocked, suns::kFirstCustomShipDesignId) != nullptr);
}

void verify_percentage_is_taken_before_production_and_leftovers_follow()
{
    const suns::TurnProcessor processor;
    auto state = suns::make_demo_game();
    auto& colony = state.planets.front();
    colony.minerals = {100.0, 100.0, 100.0};
    colony.productionQueue = {{suns::ProductionKind::Factory, suns::kFactoryCost, 0}};

    suns::PlayerOrders allocate{1, {}};
    allocate.orders.emplace_back(suns::SetResearchAllocationOrder{50});
    const auto first = processor.process(state, {allocate});
    assert(first.players.front().technology.researchAllocationPercent == 50);
    assert(first.players.front().technology.progress[3] == 3);
    assert(first.planets.front().productionQueue.front().remainingCost == 3);

    const auto second = processor.process(first, {});
    assert(second.players.front().technology.progress[3] == 6);
    assert(second.planets.front().industry == colony.industry + 1);
    assert(second.planets.front().productionQueue.empty());

    const auto third = processor.process(second, {});
    assert(third.players.front().technology.progress[3] == 13);
}

void verify_receiver_reserve_matches_research_forecast()
{
    using namespace suns;
    auto state = make_demo_game();
    auto& colony = state.planets.front();
    colony.radar = PlanetaryRadarLevel::None;
    colony.minerals = {100, 100, 100};
    colony.productionQueue = {{ProductionKind::LocalRadar, production_cost(ProductionKind::LocalRadar)},
        {ProductionKind::Factory, kFactoryCost}};
    auto& technology = state.players.front().technology;
    technology.researchAllocationPercent = 100;
    assert(colony_output(state, colony) == 6);
    assert(colony_research_allocation(state, colony, 6) == 5);
    assert(colony_research_allocation(state, colony, 1) == 0);
    assert(colony_research_allocation(state, colony, 0) == 0);
    auto pendingState = state;
    pendingState.planets.front().productionQueue.clear();
    const PlayerOrders pending{1, {QueueProductionBatchOrder{colony.id, ProductionKind::LocalRadar},
        QueueProductionOrder{colony.id, ProductionKind::Factory}}};
    const auto planned = planned_production_state(pendingState, pending);
    assert(planned.planets.front().productionQueue.size() == 2);
    assert(planned.planets.front().productionQueue.front().kind == ProductionKind::LocalRadar);
    assert(colony_research_allocation(planned, planned.planets.front(), 6) == 5);
    assert(TurnProcessor{}.process(pendingState, {pending}).players.front().technology.progress[3] == 5);
    const auto first = TurnProcessor{}.process(state, {});
    assert(first.players.front().technology.progress[3] == 5);
    assert(first.planets.front().productionQueue.front().remainingCost
        == production_cost(ProductionKind::LocalRadar) - 1);

    technology.researchAllocationPercent = 50;
    assert(colony_research_allocation(state, colony, 6) == 3);
    const auto half = TurnProcessor{}.process(state, {});
    assert(half.players.front().technology.progress[3] == 3);
    assert(half.planets.front().radar == PlanetaryRadarLevel::Local);

    technology.researchAllocationPercent = 100;
    colony.productionQueue.erase(colony.productionQueue.begin());
    assert(colony_research_allocation(state, colony, 6) == 6);
    assert(TurnProcessor{}.process(state, {}).players.front().technology.progress[3] == 6);
    technology.researchActive = false;
    assert(colony_research_allocation(state, colony, 6) == 0);
    assert(TurnProcessor{}.process(state, {}).players.front().technology.progress[3] == 0);
}

void verify_receiver_completion_waits_for_delivered_report()
{
    using namespace suns;
    auto state = test::remote_receiver_fixture();
    const auto guarantee = [](const GameState& host) {
        const auto view = make_player_view(host, 1);
        const auto& colony = *find_planet(view.state, 2);
        return colony_research_allocation(view.state, colony, net_colony_output(view.state, colony));
    };
    assert(guarantee(state) == 5);
    auto next = TurnProcessor{}.process(state, {});
    assert(find_planet(next, 2)->radar == PlanetaryRadarLevel::Local);
    assert(colony_research_allocation(next, *find_planet(next, 2), 6) == 6);
    auto view = make_player_view(next, 1);
    assert(find_planet(view.state, 2)->radar == PlanetaryRadarLevel::None);
    assert(find_planet(view.state, 2)->productionQueue.size() == 2);
    assert(guarantee(next) == 5);
    test::deliver_receiver_report(next);
    view = make_player_view(next, 1);
    assert(find_planet(view.state, 2)->radar == PlanetaryRadarLevel::Local);
    assert(find_planet(view.state, 2)->productionQueue.size() == 1);
    assert(guarantee(next) == 6);
    next.planets[1].radar = PlanetaryRadarLevel::None;
    assert(guarantee(next) == 6);
}

void verify_new_research_changes_scouting_and_fuel_logistics()
{
    auto state = suns::make_demo_game();
    const auto scoop = suns::ShipComponentType::EfficientRamScoopDrive;
    const auto scanner = suns::ShipComponentType::DeepPenetratingScanner;
    assert(!suns::component_available_to_player(state, 1, scoop));
    assert(!suns::component_available_to_player(state, 1, scanner));
    suns::PlayerOrders orders{1, {suns::CreateShipDesignOrder{
        "Long Range Surveyor", suns::ShipHullType::Scout, {scoop, scanner}}}};
    const suns::TurnProcessor processor;
    assert(suns::find_ship_design(processor.process(state, {orders}),
        suns::kFirstCustomShipDesignId) == nullptr);

    state.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Propulsion)] = 3;
    state.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Electronics)] = 4;
    assert(suns::component_available_to_player(state, 1, scoop));
    assert(suns::component_available_to_player(state, 1, scanner));
    const auto unlocked = processor.process(state, {orders});
    const auto* design = suns::find_ship_design(unlocked, suns::kFirstCustomShipDesignId);
    assert(design != nullptr);
    auto fleet = state.fleets.front();
    fleet.design = design->id;
    fleet.ships = {{design->id, 1}};
    fleet.warp = 7;
    auto comparison = fleet;
    comparison.design = suns::kScoutDesignId;
    comparison.ships = {{suns::kScoutDesignId, 1}};
    state.shipDesigns.push_back(*design);
    assert(suns::fleet_fuel_change_for_distance(state, fleet, 49.0) < 0.0);
    assert(suns::fleet_fuel_change_for_distance(state, comparison, 49.0) > 0.0);
    assert(suns::fleet_penetrating_sensor_range(state, fleet) == 145.0);
    assert(suns::fleet_penetrating_sensor_range(state, comparison) == 0.0);
    assert(suns::component_spec(scoop).radiationHazard == 0.0);
    assert(suns::component_spec(scoop).mass > suns::component_spec(suns::ShipComponentType::RamScoopDrive).mass);
}

} // namespace

int main()
{
    verify_research_costs_and_initial_unlocks();
    verify_unused_colony_output_advances_focus_queue();
    verify_overflow_follows_preselected_field();
    verify_future_plan_can_be_reordered_and_cleared();
    verify_active_research_can_be_moved_removed_and_resumed();
    verify_component_unlock_is_enforced_for_new_designs();
    verify_percentage_is_taken_before_production_and_leftovers_follow();
    verify_receiver_reserve_matches_research_forecast();
    verify_receiver_completion_waits_for_delivered_report();
    verify_new_research_changes_scouting_and_fuel_logistics();
    return 0;
}
