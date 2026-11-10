#include "native_orbital_fixture.hpp"
#include "orbital_base_fixture.hpp"
#include "suns/mining.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include "suns/production.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace suns;
namespace {
bool same_minerals(const MineralCargo& a, const MineralCargo& b)
{
    return std::abs(a.ironium-b.ironium) < 1e-8 && std::abs(a.boranium-b.boranium) < 1e-8
        && std::abs(a.germanium-b.germanium) < 1e-8;
}
GameState started()
{
    auto state = test::empty_native_orbital_start();
    assert(initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget()));
    return state;
}
void starter_is_paid_legal_and_observed()
{
    for (const auto controller : {PlayerController::Human, PlayerController::Computer}) {
        auto state = test::empty_native_orbital_start();
        state.players.front().controller = controller;
        const auto levels = state.players.front().technology.levels;
        const auto receipt = initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget());
        assert(receipt && receipt->productionSpent == 193);
        assert(same_minerals(receipt->mineralsSpent, {72, 9, 26}));
        const auto& home = state.planets.front();
        assert(home.owner == 1 && !home.population && !home.industry && !home.mines);
        assert(!home.advancedFactories && !home.advancedMines && !home.habitat.population);
        assert(home.nativeOrbit.population == kInitialHomePopulation && home.nativeOrbit.station == receipt->station);
        assert(same_minerals(home.minerals, {100, 100, 100}));
        assert(home.homeworld && home.radar == PlanetaryRadarLevel::Full);
        assert(colony_has_orbital_service(state, home.id, 1, OrbitalStationModule::Shipyard));
        assert(colony_has_orbital_service(state, home.id, 1, OrbitalStationModule::RefuelingDepot));
        assert(state.orbitalStations.size() == 1 && state.shipDesigns.size() == 1 && state.fleets.size() == 1);
        const auto& miner = state.fleets.front();
        assert(miner.id == receipt->minerFleet && miner.design == receipt->minerDesign && fleet_ship_count(miner) == 1);
        assert(ship_design_valid(state.shipDesigns.front()) && ship_design_available_to_player(state, 1, state.shipDesigns.front()));
        assert(fleet_can_remote_mine(state, miner) && miner.task == FleetTask::RemoteMining);
        assert(miner.fuel == 210 && miner.fuel == fleet_fuel_capacity(state, miner));
        assert(miner.telemetry.task == FleetTask::RemoteMining && miner.telemetry.observedTurn == state.turn);
        assert(state.players.front().technology.levels == levels && state.players.front().controller == controller);
        assert(!state.players.front().race.advancedRemoteMining && !state.players.front().race.basicRemoteMining);
        assert(colony_output(state, home) == 50);
        const auto received = make_player_view(state, 1);
        assert(colony_output(received.state, received.state.planets.front()) == 50);
        const auto& history = state.players.front().history;
        assert(history.size() == 1 && history.front().population == kInitialHomePopulation);
        assert(history.front().productionOutput == 50 && history.front().remoteExtraction.ironium == 0);
        assert(same_minerals(history.front().minerals, {100, 100, 100}));
    }
}
void early_access_is_narrow_and_does_not_unlock_campaigns()
{
    auto state = started();
    assert(ship_hull_available_to_player(state, 1, ShipHullType::MiniMiner));
    assert(component_available_to_player(state, 1, ShipComponentType::RoboMiniMiner));
    assert(!ship_hull_available_to_player(state, 1, ShipHullType::HeavyTransport));
    assert(!component_available_to_player(state, 1, ShipComponentType::RoboMiner));
    assert(!component_available_to_player(state, 1, ShipComponentType::LongHump6));
    for (const auto& unlock : research_unlocks()) {
        if (unlock.hull == ShipHullType::MiniMiner || unlock.component == ShipComponentType::RoboMiniMiner)
            assert(research_unlock_available(state, 1, unlock));
    }
    auto ordinary = state; ordinary.players.front().race.primaryTrait = PrimaryRaceTrait::Generalist;
    assert(!ship_hull_available_to_player(ordinary, 1, ShipHullType::MiniMiner));
    assert(!component_available_to_player(ordinary, 1, ShipComponentType::RoboMiniMiner));
    auto conflicting = state; conflicting.players.front().race.basicRemoteMining = true;
    assert(!ship_hull_available_to_player(conflicting, 1, ShipHullType::MiniMiner));
    assert(!component_available_to_player(conflicting, 1, ShipComponentType::RoboMiniMiner));
    for (const auto controller : {PlayerController::Human, PlayerController::Computer}) {
        EmpireSetup empire{"Orbital civilization"}; empire.controller = controller;
        empire.primaryTrait = PrimaryRaceTrait::HabitatCivilization;
        bool rejected = false;
        try { (void)generate_campaign(GalaxyConfig{}, {empire}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    const auto generalist = generate_campaign(GalaxyConfig{}, {{"Terrans"}});
    assert(colony_output(generalist, generalist.planets.front()) == 29);
}
void ordinary_orders_manufacture_a_paid_replacement()
{
    auto state = started();
    const auto home = state.planets.front().id;
    const auto source = state.fleets.front().id;
    const auto fit = state.shipDesigns.front();
    PlayerOrders orders{1, {CreateShipDesignOrder{"Replacement miner", fit.hull, fit.components},
        QueueShipDesignOrder{home, 0, "Replacement miner"}}};
    for (unsigned year = 1; year <= 4; ++year) {
        const auto previous = state.planets.front().minerals;
        state = TurnProcessor{}.process(state, year == 1 ? std::vector<PlayerOrders>{orders} : std::vector<PlayerOrders>{});
        assert(state.planets.front().nativeOrbit.population == kInitialHomePopulation && !state.planets.front().population);
        assert(state.players.front().history.back().extraction.ironium == 0);
        assert(same_minerals(state.players.front().history.back().remoteExtraction, {4, 2, 1}));
        const auto spent = year == 4 ? ship_design_mineral_cost(fit) : MineralCargo{};
        assert(same_minerals(state.planets.front().minerals,
            {previous.ironium+4-spent.ironium, previous.boranium+2-spent.boranium, previous.germanium+1-spent.germanium}));
        if (year < 4) {
            assert(state.fleets.size() == 1 && state.planets.front().productionQueue.size() == 1);
            assert(state.planets.front().productionQueue.front().remainingCost == 153-50*year);
        }
    }
    assert(state.turn == 5 && state.planets.front().productionQueue.empty() && state.fleets.size() == 2);
    assert(state.shipDesigns.size() == 2 && ship_design_cost(state.shipDesigns.back()) == 153);
    assert(same_minerals(state.planets.front().minerals, {58, 108, 90}));
    const auto& replacement = state.fleets.back();
    assert(replacement.id != source && replacement.design == state.shipDesigns.back().id);
    assert(replacement.fuel == fleet_fuel_capacity(state, replacement));
    assert(replacement.task == FleetTask::None && fleet_can_remote_mine(state, replacement));
    assert(technology_level(state, 1, ResearchField::Energy) == 0);
    const auto replacementId = replacement.id;
    // Losing the original hull immediately stops its physical extraction.
    std::erase_if(state.fleets, [=](const auto& f) { return f.id == source; });
    const auto paused = TurnProcessor{}.process(state, {});
    assert(same_minerals(paused.planets.front().minerals, state.planets.front().minerals));
    state = TurnProcessor{}.process(state, {{1, {SetRemoteMiningOrder{replacementId, true}}}});
    assert(same_minerals(state.players.front().history.back().remoteExtraction, {4, 2, 1}));
    assert(same_minerals(state.planets.front().minerals, {62, 110, 91}));
}
void income_requires_supported_real_residents_and_energy()
{
    auto state = started(); auto& home = state.planets.front();
    home.environment = {0, 100, 20}; assert(colony_output(state, home) == 50);
    home.environment = {100, 0, 20}; assert(colony_output(state, home) == 50);
    // Even malformed surface assets cannot manufacture native resources.
    home.population = 1'000'000; home.industry = 1000; home.mines = 1000;
    assert(colony_output(state, home) == 50);
    state.players.front().technology.levels[std::size_t(ResearchField::Energy)] = 3;
    assert(colony_output(state, home) == 100);
    home.nativeOrbit.population = 24'999; assert(colony_output(state, home) == 99);
    home.nativeOrbit.population = 100'000; assert(colony_output(state, home) == 200);
    home.nativeOrbit.population = std::numeric_limits<std::uint64_t>::max(); assert(colony_output(state, home) == 200);
    home.nativeOrbit.population = 0; assert(colony_output(state, home) == 0);
    home.nativeOrbit.population = 25'000;
    state.orbitalStations.front().damagePercent = 100; assert(colony_output(state, home) == 0);
    state.orbitalStations.clear(); assert(colony_output(state, home) == 0);
    const auto stalled = TurnProcessor{}.process(state, {});
    assert(stalled.players.front().technology.progress == state.players.front().technology.progress);
}
void undersupply_and_invalid_starts_are_atomic()
{
    const auto empty = test::empty_native_orbital_start();
    for (unsigned defect = 0; defect < 13; ++defect) {
        auto state = empty; auto budget = native_orbital_start_budget();
        if (defect == 0) --budget.production;
        if (defect == 1) budget.minerals.ironium = 71;
        if (defect == 2) budget.minerals.boranium = 8;
        if (defect == 3) budget.minerals.germanium = 25;
        if (defect == 4) budget.minerals.ironium = std::numeric_limits<double>::infinity();
        if (defect == 5) budget.population = 0;
        if (defect == 6) budget.population = 100'001;
        if (defect == 7) state.players.front().race.basicRemoteMining = true;
        if (defect == 8) state.players.front().race.radiationImmune = true;
        if (defect == 9) state.players.front().race.environmentBased = false;
        if (defect == 10) state.mediumObserver = 1;
        if (defect == 11) state.turn = 2;
        if (defect == 12) state.nextFleetId = std::numeric_limits<FleetId>::max();
        const auto before = state;
        assert(!initialize_native_orbital_start(state, 1, state.planets.front().id, budget));
        assert(state.planets.front().owner == 0 && !state.planets.front().nativeOrbit.population);
        assert(state.orbitalStations.empty() && state.fleets.empty() && state.shipDesigns.empty());
        assert(state.players.front().history.empty() && state.players.front().surveyKnowledge.empty());
        assert(state.players.front().race.basicRemoteMining == before.players.front().race.basicRemoteMining);
        assert(state.players.front().race.radiationImmune == before.players.front().race.radiationImmune);
        assert(state.nextFleetId == before.nextFleetId && state.nextShipDesignId == before.nextShipDesignId
            && state.nextOrbitalStationId == before.nextOrbitalStationId);
    }
    auto state = started();
    assert(!initialize_native_orbital_start(state, 1, state.planets.front().id, native_orbital_start_budget()));
    assert(!initialize_native_orbital_start(state, 1, state.planets[1].id, native_orbital_start_budget()));
    assert(state.fleets.size() == 1 && state.planets.front().nativeOrbit.population == kInitialHomePopulation);
    assert(same_minerals(state.planets.front().minerals, {100, 100, 100}) && !state.planets[1].owner);
}
void starts_require_actual_nonzero_output()
{
    for (const auto energy : {0U, 1U, 9U}) for (const auto residents : {1U, 4U, 5U, 9U, 10U}) {
        auto state = test::empty_native_orbital_start();
        state.players.front().technology.levels[std::size_t(ResearchField::Energy)] = energy;
        auto budget = native_orbital_start_budget(); budget.population = residents;
        const bool viable = residents*(energy+1) >= 10;
        const auto receipt = initialize_native_orbital_start(state, 1, state.planets.front().id, budget);
        assert(bool(receipt) == viable);
        if (!viable) {
            assert(state.fleets.empty() && state.orbitalStations.empty() && state.shipDesigns.empty());
            assert(state.nextFleetId == 1 && state.nextShipDesignId == 1 && state.nextOrbitalStationId == 1);
            assert(!state.planets.front().owner && state.players.front().surveyKnowledge.empty() && state.players.front().history.empty());
        } else {
            assert(colony_output(state, state.planets.front()) > 0);
            state = TurnProcessor{}.process(state, {{1, {SetResearchPlanOrder{ResearchField::Energy, {}, false},
                QueueShipDesignOrder{state.planets.front().id, receipt->minerDesign}}}});
            assert(state.planets.front().productionQueue.front().remainingCost < 153);
        }
    }
}
void a_deployed_independent_shipyard_blocks_start_atomically()
{
    auto state = orbital_test::world();
    state.planets[1].minerals = {};
    Player native{2, "New orbital empire"};
    native.race = race_preset(RacePreset::Terran);
    native.race.primaryTrait = PrimaryRaceTrait::HabitatCivilization;
    state.players.push_back(native);
    orbital_test::deploy(state);
    const auto home = state.planets[1].id;
    assert(state.turn == 1 && !state.planets[1].owner);
    assert(orbital_base_at(state, home) && !find_orbital_station_at_planet(state, home));

    // The otherwise identical prepared host is valid without the deployed yard.
    auto vacant = state; vacant.fleets.clear();
    assert(initialize_native_orbital_start(vacant, 2, home, native_orbital_start_budget()));
    const auto before = state;
    assert(!initialize_native_orbital_start(state, 2, home, native_orbital_start_budget()));
    assert(!state.planets[1].owner && !state.planets[1].nativeOrbit.station);
    assert(!state.planets[1].nativeOrbit.population && same_minerals(state.planets[1].minerals, {}));
    assert(state.orbitalStations.empty() && state.fleets.size() == before.fleets.size());
    const auto* yard = orbital_base_at(state, home);
    assert(yard && yard->owner == 1 && yard->id == before.fleets.front().id);
    assert(yard->colonists == before.fleets.front().colonists
        && same_minerals(yard->minerals, before.fleets.front().minerals));
    assert(state.shipDesigns.size() == before.shipDesigns.size());
    assert(state.nextFleetId == before.nextFleetId && state.nextShipDesignId == before.nextShipDesignId
        && state.nextOrbitalStationId == before.nextOrbitalStationId);
    assert(state.players.back().history.empty() && state.players.back().surveyKnowledge.empty());
    assert(state.stars[1].stellarClass == before.stars[1].stellarClass);
}
void native_surface_industry_never_spends_work_or_minerals()
{
    auto state = started(); state.players.front().technology.researchActive = false;
    const std::vector<ProductionKind> unsupported{ProductionKind::Factory, ProductionKind::Mine,
        ProductionKind::AdvancedFactory, ProductionKind::AdvancedMine, ProductionKind::UpgradeFactory,
        ProductionKind::UpgradeMine, ProductionKind::ReconstructFactory, ProductionKind::ReconstructMine};
    auto& home = state.planets.front();
    home.industry = home.mines = 10; home.warDamage.factoriesDestroyed = home.warDamage.minesDestroyed = 10;
    for (const auto kind : unsupported) {
        auto trial = state;
        PlayerOrders orders{1, {QueueProductionOrder{home.id, kind},
            QueueProductionBatchOrder{home.id, kind, 2}}};
        const auto planned = planned_production_state(trial, orders);
        assert(planned.planets.front().productionQueue.empty());
        const auto idle = TurnProcessor{}.process(trial, {});
        const auto result = TurnProcessor{}.process(trial, {orders});
        assert(result.planets.front().productionQueue.empty());
        assert(result.planets.front().industry == home.industry && result.planets.front().mines == home.mines);
        assert(same_minerals(result.planets.front().minerals, idle.planets.front().minerals));
        // Loaded/stale, partly built, automated and fully paid rows also retire
        // before consuming another resource or completing a surface installation.
        trial.planets.front().productionQueue = {{kind, 0}, {kind, 1}};
        if (kind == ProductionKind::Factory || kind == ProductionKind::Mine
            || kind == ProductionKind::AdvancedFactory || kind == ProductionKind::AdvancedMine)
            trial.planets.front().productionQueue.push_back({kind, production_cost(kind), 0, ProductionAutomation::Annual, 2});
        const auto resumed = TurnProcessor{}.process(trial, {});
        assert(resumed.planets.front().productionQueue.empty());
        assert(resumed.planets.front().industry == home.industry && resumed.planets.front().mines == home.mines);
        assert(same_minerals(resumed.planets.front().minerals, idle.planets.front().minerals));
    }
    ProductionTemplate surface{"Surface", {{ProductionKind::Factory, production_cost(ProductionKind::Factory)}}};
    auto& player = state.players.front(); player.productionTemplates = {surface};
    assert(production_template_valid(surface));
    assert(!apply_production_template(home, player, surface.name));
    assert(planned_production_state(state, {1, {ApplyProductionTemplateOrder{home.id, surface.name}}})
        .planets.front().productionQueue.empty());
    player.productionTemplates.front().defaultForNewColonies = true;
    apply_default_production_template(state, home); assert(home.productionQueue.empty());
    auto ordinary = state; ordinary.players.front().race.primaryTrait = PrimaryRaceTrait::Generalist;
    ordinary.planets.front().population = 25'000;
    assert(apply_production_template(ordinary.planets.front(), ordinary.players.front(), surface.name));
    const auto built = TurnProcessor{}.process(ordinary, {});
    assert(built.planets.front().industry == home.industry+1);
}
}
int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "occupied-orbit") { a_deployed_independent_shipyard_blocks_start_atomically(); return 0; }
    if (argc == 2 && std::string(argv[1]) == "minimum") { starts_require_actual_nonzero_output(); return 0; }
    if (argc == 2 && std::string(argv[1]) == "industry") { native_surface_industry_never_spends_work_or_minerals(); return 0; }
    starter_is_paid_legal_and_observed();
    early_access_is_narrow_and_does_not_unlock_campaigns();
    ordinary_orders_manufacture_a_paid_replacement();
    income_requires_supported_real_residents_and_energy();
    undersupply_and_invalid_starts_are_atomic();
    starts_require_actual_nonzero_output();
    a_deployed_independent_shipyard_blocks_start_atomically();
    native_surface_industry_never_spends_work_or_minerals();
}
