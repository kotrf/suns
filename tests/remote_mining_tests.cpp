#include "suns/game_state.hpp"
#include "suns/turn_processor.hpp"
#include "suns/campaign.hpp"
#include "suns/mining.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace {

const suns::Planet& planet(const suns::GameState& state, suns::PlanetId id)
{
    for (const auto& candidate : state.planets) if (candidate.id == id) return candidate;
    assert(false);
    return state.planets.front();
}

const suns::Fleet& fleet(const suns::GameState& state, suns::FleetId id)
{
    for (const auto& candidate : state.fleets) if (candidate.id == id) return candidate;
    assert(false);
    return state.fleets.front();
}

bool close(double a, double b)
{
    return std::abs(a - b) < 0.000001;
}

suns::GameState advance_until_task(
    const suns::TurnProcessor& processor,
    suns::GameState state,
    suns::FleetId id,
    suns::FleetTask task)
{
    for (int turn = 0; turn < 12 && fleet(state, id).task != task; ++turn) {
        state = processor.process(state, {});
    }
    assert(fleet(state, id).task == task);
    return state;
}

void reference_robots_and_access()
{
    using namespace suns;
    assert(mining_technologies().size() == 6);
    auto state = generate_campaign({}, {{"Miners"}});
    auto& levels = state.players.front().technology.levels;
    auto& race = state.players.front().race;
    const auto c = static_cast<std::size_t>(ResearchField::Construction);
    const auto e = static_cast<std::size_t>(ResearchField::Electronics);
    assert(!component_available_to_player(state, 1, ShipComponentType::RemoteMiningModule));
    assert(!component_available_to_player(state, 1, ShipComponentType::RoboMidgetMiner));
    race.advancedRemoteMining = true;
    assert(component_available_to_player(state, 1, ShipComponentType::RoboMidgetMiner));
    race.advancedRemoteMining = false;
    for (const auto& robot : mining_technologies()) {
        const auto spec = component_spec(robot.component);
        assert(spec.kind == ShipComponentKind::Mining && spec.mass == robot.mass);
        assert(spec.buildCost == robot.cost && spec.remoteMiningUnits == robot.rate);
        const auto cost = component_mineral_cost(robot.component);
        assert(cost.ironium == robot.minerals.ironium && cost.boranium == robot.minerals.boranium
            && cost.germanium == robot.minerals.germanium);
        const auto& unlocks = research_unlocks();
        const auto unlock = std::find_if(unlocks.begin(), unlocks.end(), [&](const auto& item) {
            return item.component == robot.component;
        });
        assert(unlock != unlocks.end());
        levels[c] = robot.construction;
        levels[e] = robot.electronics;
        race.advancedRemoteMining = true;
        race.basicRemoteMining = false;
        assert(component_available_to_player(state, 1, robot.component));
        if (robot.construction) {
            --levels[c];
            assert(!component_available_to_player(state, 1, robot.component));
            ++levels[c];
        }
        if (robot.electronics) {
            --levels[e];
            assert(!component_available_to_player(state, 1, robot.component));
            ++levels[e];
        }
        race.advancedRemoteMining = false;
        assert(component_available_to_player(state, 1, robot.component) == !robot.advancedRemoteMining);
        assert(research_unlock_applicable(state, 1, *unlock) == !robot.advancedRemoteMining);
        race.advancedRemoteMining = true;
        race.basicRemoteMining = true;
        assert(component_available_to_player(state, 1, robot.component) == !robot.excludesBasicRemoteMining);
        if (robot.advancedRemoteMining)
            assert(research_unlock_requirement(*unlock).find("Advanced Remote Mining") != std::string::npos);
        if (robot.excludesBasicRemoteMining)
            assert(research_unlock_requirement(*unlock).find("Basic Remote Mining") != std::string::npos);
    }
    assert(component_spec(ShipComponentType::RoboMiniMiner).mass == 240);
    assert(component_spec(ShipComponentType::RoboMiniMiner).remoteMiningUnits == 4);
    assert(component_spec(ShipComponentType::RoboSuperMiner).remoteMiningUnits == 27);
    assert(component_spec(ShipComponentType::RoboUltraMiner).mass == 80);
    assert(component_spec(ShipComponentType::RoboUltraMiner).remoteMiningUnits == 25);
}

void reference_extraction_and_host_validation()
{
    using namespace suns;
    auto state = generate_campaign({}, {{"Miners"}});
    auto& levels = state.players.front().technology.levels;
    const auto c = static_cast<std::size_t>(ResearchField::Construction);
    const auto e = static_cast<std::size_t>(ResearchField::Electronics);
    levels[c] = 15; levels[e] = 8;
    auto& site = state.planets[1];
    site.owner = 0; site.population = 0;
    site.observedConcentration = MineralCargo{100, 50, 8};
    site.minerals = {};
    state.players.front().race.advancedRemoteMining = true;
    ShipDesign old{90, 1, "Old miner", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RemoteMiningModule}};
    const auto oldOutput = projected_remote_mining(state, site, old);
    assert(close(oldOutput.ironium, 1.25) && close(oldOutput.boranium, 0.625)
        && close(oldOutput.germanium, 0.1));
    for (const auto& robot : mining_technologies()) {
        ShipDesign miner{99, 1, robot.name, ShipHullType::MiniMiner,
            {ShipComponentType::QuickJump5, robot.component, robot.component}};
        assert(ship_design_valid(miner));
        const auto yield = projected_remote_mining(state, site, miner);
        assert(close(yield.ironium, robot.rate * 2));
        assert(close(yield.boranium, robot.rate));
        assert(close(yield.germanium, robot.rate * 0.16));
        auto owned = site; owned.owner = 1;
        assert(mineral_cargo_mass(projected_remote_mining(state, owned, miner)) == 0);
        auto mining = state;
        mining.shipDesigns.push_back(miner);
        mining.fleets.clear();
        Fleet fleet;
        fleet.id = 42; fleet.owner = 1; fleet.design = miner.id;
        fleet.position = find_star(state, site.star)->position;
        fleet.ships = {{miner.id, 3}}; fleet.task = FleetTask::RemoteMining;
        mining.fleets.push_back(fleet);
        const auto next = TurnProcessor{}.process(mining, {});
        const auto& surface = planet(next, site.id);
        assert(close(surface.minerals.ironium, yield.ironium * 3));
        assert(close(surface.minerals.boranium, yield.boranium * 3));
        assert(close(surface.minerals.germanium, yield.germanium * 3));
        assert(mineral_cargo_mass(next.fleets.front().minerals) == 0);
        assert(close(next.players.front().history.back().remoteExtraction.ironium, yield.ironium * 3));
    }
    ShipDesign mixed{99, 1, "Mixed robots", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RoboMiniMiner, ShipComponentType::RoboSuperMiner}};
    assert(ship_design_valid(mixed));
    assert(close(projected_remote_mining(state, site, mixed).ironium, 31));
    ShipDesign invalid = mixed; invalid.hull = ShipHullType::StarsScout;
    assert(!ship_design_valid(invalid));
    const PlayerOrders create{1, {CreateShipDesignOrder{mixed.name, mixed.hull, mixed.components}}};
    levels[c] = 11;
    assert(TurnProcessor{}.process(state, {create}).shipDesigns.size() == state.shipDesigns.size());
    levels[c] = 12; levels[e] = 5;
    assert(TurnProcessor{}.process(state, {create}).shipDesigns.size() == state.shipDesigns.size());
    levels[e] = 6;
    assert(TurnProcessor{}.process(state, {create}).shipDesigns.size() == state.shipDesigns.size() + 1);
    const PlayerOrders advanced{1, {CreateShipDesignOrder{"Midget robot", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RoboMidgetMiner}}}};
    state.players.front().race.advancedRemoteMining = false;
    assert(TurnProcessor{}.process(state, {advanced}).shipDesigns.size() == state.shipDesigns.size());
    state.players.front().race.advancedRemoteMining = true;
    assert(TurnProcessor{}.process(state, {advanced}).shipDesigns.size() == state.shipDesigns.size() + 1);
    const PlayerOrders prototype{1, {CreateShipDesignOrder{old.name, old.hull, old.components}}};
    assert(TurnProcessor{}.process(state, {prototype}).shipDesigns.size() == state.shipDesigns.size());
    state.shipDesigns.push_back(old);
    assert(component_available_to_player(state, 1, ShipComponentType::RemoteMiningModule));
    assert(ship_design_available_to_player(state, 1, old));
}

} // namespace

int main()
{
    reference_robots_and_access();
    reference_extraction_and_host_validation();
    auto state = suns::make_demo_game();
    state.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 1;
    state.shipDesigns.push_back({
        3, 1, "Remote Miner", suns::ShipHullType::RemoteMiner,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive,
         suns::ShipComponentType::RemoteMiningModule},
    });
    state.shipDesigns.push_back({
        4, 1, "Ore Hauler", suns::ShipHullType::MediumTransport,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::FusionDrive},
    });

    const auto* alpha = suns::find_star(state, 2);
    assert(alpha != nullptr);
    state.fleets = {
        {2, 1, "Miner 2", suns::FleetRole::Scout, 3, state.stars.front().position, {}, 1, 500.0, 0},
        {3, 1, "Hauler 3", suns::FleetRole::Scout, 4, alpha->position, {}, 1, 300.0, 0},
    };

    const auto expected = suns::projected_remote_mining(state, planet(state, 2), state.shipDesigns[2]);
    const suns::TurnProcessor processor;

    // A persistent work assignment must be the terminal route task.
    suns::PlayerOrders invalidProgram{1, {suns::MoveFleetOrder{
        2,
        alpha->position,
        8,
        {suns::FleetArrivalActionKind::RemoteMining, 1},
        {{{alpha->position.x + 10.0, alpha->position.y}, 8, {}}},
    }}};
    const auto rejectedProgram = processor.process(state, {invalidProgram});
    assert(fleet(rejectedProgram, 2).pendingCommands.empty());
    assert(!fleet(rejectedProgram, 2).destination);

    // Merely arriving in orbit does not start a mining operation.
    const auto first = processor.process(state, {});
    assert(close(suns::mineral_cargo_mass(planet(first, 2).minerals), 0.0));
    assert(fleet(first, 2).task == suns::FleetTask::None);

    suns::MoveFleetOrder miningWaypoint{
        2,
        alpha->position,
        8,
        {suns::FleetArrivalActionKind::RemoteMining, 1},
    };
    suns::PlayerOrders startMining{1, {miningWaypoint}};
    auto started = processor.process(first, {startMining});
    started = advance_until_task(processor, std::move(started), 2, suns::FleetTask::RemoteMining);
    // Arrival assigns the persistent task, but does not grant a full year's
    // output after a turn that may have included travel.
    assert(close(suns::mineral_cargo_mass(planet(started, 2).minerals), 0.0));
    started = processor.process(started, {});
    assert(close(planet(started, 2).minerals.ironium, expected.ironium));
    assert(close(planet(started, 2).minerals.boranium, expected.boranium));
    assert(close(planet(started, 2).minerals.germanium, expected.germanium));
    const auto& remoteHistory = started.players.front().history.back();
    assert(remoteHistory.remoteExtractionRecorded);
    assert(close(remoteHistory.remoteExtraction.ironium, expected.ironium));
    assert(remoteHistory.remoteMineHistory.size() == 1);
    assert(remoteHistory.remoteMineHistory.front().planet == 2);
    assert(close(remoteHistory.remoteMineHistory.front().extraction.boranium, expected.boranium));
    assert(!started.players.front().history.front().remoteExtractionRecorded);
    auto refreshedRemote = started;
    suns::record_empire_turn_statistics(refreshedRemote);
    assert(close(refreshedRemote.players.front().history.back().remoteExtraction.ironium,
        expected.ironium));
    assert(fleet(started, 2).task == suns::FleetTask::RemoteMining);
    assert(close(suns::mineral_cargo_mass(fleet(first, 2).minerals), 0.0));

    // The assigned task remains active without being queued every year.
    const auto continued = processor.process(started, {});
    assert(close(planet(continued, 2).minerals.ironium, expected.ironium * 2.0));
    assert(close(continued.players.front().history.back().remoteExtraction.ironium,
        expected.ironium));

    // A different empire mining the same neutral site sees only its own work.
    auto rivals = started;
    rivals.players.push_back({2, "Rivals", {}});
    rivals.players.back().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 1;
    auto rivalDesign = rivals.shipDesigns[2];
    rivalDesign.id = 5;
    rivalDesign.owner = 2;
    rivals.shipDesigns.push_back(rivalDesign);
    auto rivalFleet = rivals.fleets.front();
    rivalFleet.id = 4;
    rivalFleet.owner = 2;
    rivalFleet.design = 5;
    rivalFleet.task = suns::FleetTask::RemoteMining;
    rivals.fleets.push_back(rivalFleet);
    const auto twoMiners = processor.process(rivals, {});
    assert(close(twoMiners.players[0].history.back().remoteExtraction.ironium, expected.ironium));
    assert(close(twoMiners.players[1].history.back().remoteExtraction.ironium, expected.ironium));
    assert(twoMiners.players[1].history.back().remoteMineHistory.size() == 1);

    // A site colonized before the next mining phase stops producing remote ore.
    auto settled = started;
    settled.planets[1].owner = 1;
    const auto noRemoteMining = processor.process(settled, {});
    assert(close(noRemoteMining.players.front().history.back().remoteExtraction.ironium, 0.0));

    // A transport, not the miner, may take the accumulated surface stock.
    suns::PlayerOrders collect{1, {}};
    const suns::MineralCargo accumulated{
        expected.ironium * 2.0,
        expected.boranium * 2.0,
        expected.germanium * 2.0,
    };
    collect.orders.emplace_back(suns::SetFleetMineralCargoOrder{2, 3, accumulated});
    const auto collected = processor.process(continued, {collect});
    assert(close(fleet(collected, 3).minerals.ironium, accumulated.ironium));
    assert(close(fleet(collected, 3).minerals.boranium, accumulated.boranium));
    assert(close(fleet(collected, 3).minerals.germanium, accumulated.germanium));
    // The miner continues to replenish the surface stock on the collection turn.
    assert(close(planet(collected, 2).minerals.ironium, expected.ironium));
    assert(close(planet(collected, 2).minerals.boranium, expected.boranium));
    assert(close(planet(collected, 2).minerals.germanium, expected.germanium));

    // Loading ore at a neutral site is not a colony delivery. The haul is
    // counted only after the transport reaches and unloads at the homeworld.
    assert(close(collected.players.front().history.back().freightDelivered.ironium, 0.0));
    suns::PlayerOrders returnHome{1, {suns::MoveFleetOrder{
        3, state.stars.front().position, 8,
        {suns::FleetArrivalActionKind::UnloadAll, 1, suns::FleetCargoKind::All}}}};
    auto hauled = processor.process(collected, {returnHome});
    for (int turn = 0; turn < 18
            && close(hauled.players.front().history.back().freightDelivered.ironium, 0.0); ++turn) {
        hauled = processor.process(hauled, {});
    }
    assert(close(hauled.players.front().history.back().freightDelivered.ironium,
        accumulated.ironium));
    assert(close(hauled.players.front().history.back().colonyHistory.front().freightDelivered.boranium,
        accumulated.boranium));

    // The player can stop the task without moving the fleet.
    suns::PlayerOrders stopMining{1, {suns::MoveFleetOrder{
        2,
        alpha->position,
        8,
        {},
        {},
    }}};
    auto stopped = processor.process(collected, {stopMining});
    stopped = advance_until_task(processor, std::move(stopped), 2, suns::FleetTask::None);
    assert(fleet(stopped, 2).task == suns::FleetTask::None);
    const auto surfaceAfterStop = planet(stopped, 2).minerals;
    const auto idle = processor.process(stopped, {});
    assert(close(planet(idle, 2).minerals.ironium, surfaceAfterStop.ironium));
    assert(close(planet(idle, 2).minerals.boranium, surfaceAfterStop.boranium));
    assert(close(planet(idle, 2).minerals.germanium, surfaceAfterStop.germanium));

    // A movement command cancels the task when it reaches the fleet, and the
    // fleet does not silently resume mining after the trip.
    auto restarted = processor.process(idle, {startMining});
    restarted = advance_until_task(processor, std::move(restarted), 2, suns::FleetTask::RemoteMining);
    suns::PlayerOrders depart{1, {suns::MoveFleetOrder{
        2,
        {alpha->position.x + 40.0, alpha->position.y},
        8,
    }}};
    auto departing = processor.process(restarted, {depart});
    for (int turn = 0; turn < 12 && fleet(departing, 2).task == suns::FleetTask::RemoteMining; ++turn) {
        departing = processor.process(departing, {});
    }
    assert(fleet(departing, 2).task == suns::FleetTask::None);
    for (int turn = 0; turn < 12 && fleet(departing, 2).destination; ++turn) {
        departing = processor.process(departing, {});
    }
    assert(!fleet(departing, 2).destination);
    const auto afterArrival = processor.process(departing, {});
    assert(fleet(afterArrival, 2).task == suns::FleetTask::None);

    // The 80 kt apparatus makes a miner substantially more fuel-hungry in transit.
    suns::ShipDesign light{
        5, 1, "Light Scout", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive},
    };
    state.shipDesigns.push_back(light);
    suns::Fleet lightFleet{4, 1, "Light 4", suns::FleetRole::Scout, 5, alpha->position, {}, 8, 300.0, 0};
    auto heavyFleet = state.fleets.front();
    heavyFleet.warp = 8;
    heavyFleet.fuel = 300.0;
    assert(suns::component_spec(suns::ShipComponentType::RemoteMiningModule).mass == 80.0);
    assert(suns::hull_spec(suns::ShipHullType::RemoteMiner).mass == 120.0);
    assert(suns::fleet_fuel_change_for_distance(state, heavyFleet, 64.0)
        > suns::fleet_fuel_change_for_distance(state, lightFleet, 64.0) * 4.0);

    suns::ShipDesign invalidScoutMiner{
        6, 1, "Invalid Scout Miner", suns::ShipHullType::Scout,
        {suns::ShipComponentType::FusionDrive, suns::ShipComponentType::RemoteMiningModule},
    };
    assert(!suns::ship_design_valid(invalidScoutMiner));

    // Construction 1 is an actual gate, not merely a Ship Designer hint.
    auto locked = state;
    locked.players.front().technology.levels[static_cast<std::size_t>(suns::ResearchField::Construction)] = 0;
    auto noMining = processor.process(locked, {startMining});
    for (int turn = 0; turn < 12 && !fleet(noMining, 2).pendingCommands.empty(); ++turn) {
        noMining = processor.process(noMining, {});
    }
    assert(close(suns::mineral_cargo_mass(planet(noMining, 2).minerals), 0.0));
    assert(fleet(noMining, 2).task == suns::FleetTask::None);
}
