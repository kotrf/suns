#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/computer_player.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/production.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace suns;

static void cargo_mining_forecast(bool unloading, std::uint32_t quantity)
{
    auto state = make_demo_game();
    state.players.front().technology.researchActive = false;
    auto& world = state.planets.front();
    world.population = unloading ? 500 : 100500;
    world.industry = world.mines = 100;
    world.observedConcentration = MineralCargo{100,100,100};
    world.minerals = {0,100,100};
    world.productionQueue = {{ProductionKind::Factory,kFactoryCost,0,ProductionAutomation::None,quantity}};
    auto& design = state.shipDesigns.front();
    design.hull = ShipHullType::SuperFreighter;
    design.components = {ShipComponentType::FusionDrive}; design.placements.clear();
    auto& transport = state.fleets.front();
    transport.colonists = unloading ? 100000 : 0;
    transport.telemetry = {};
    transport.telemetry = confirmed_fleet_telemetry(state,transport);
    assert(fleet_cargo_capacity(state,transport) >= 1000);
    set_survey_level(state,1,world.star,SurveyLevel::DeepSurvey,state.turn);
    const PlayerOrders orders{1,{TransferCargoOrder{unloading ? CargoTransferEndpoint{0,1} : CargoTransferEndpoint{1,0},
        unloading ? CargoTransferEndpoint{1,0} : CargoTransferEndpoint{0,1},100000,{}}}};
    const auto received = make_player_view(state,1).state;
    const auto beforeMining = projected_mineral_mining(received,received.planets.front());
    const auto planned = planned_production_state(projected_cargo_order_state(received,orders),orders);
    const auto& afterCargo = planned.planets.front();
    assert(afterCargo.population == (unloading ? 100500U : 500U));
    assert(colony_workforce(planned,afterCargo).operatingMines == (unloading ? 100U : 0U));
    assert(std::abs(projected_mineral_mining(planned,afterCargo).ironium - beforeMining.ironium) > 75);
    const auto forecast = forecast_production_queue(planned,afterCargo,afterCargo.productionQueue,128,beforeMining);
    assert(forecast.front().completionTurn);
    auto actual = TurnProcessor{}.process(state,{orders});
    // This year's extraction was earned by the original residents, while
    // production already uses the new residents. Neither case can finish yet.
    assert(actual.planets.front().industry == 100);
    assert(std::abs(actual.planets.front().minerals.ironium - beforeMining.ironium) < 1e-8);
    assert(actual.planets.front().productionWaitingForMinerals == unloading);
    while (!actual.planets.front().productionQueue.empty() && actual.turn <= state.turn + 128)
        actual = TurnProcessor{}.process(actual,{});
    assert(actual.planets.front().industry == 100 + quantity);
    assert(actual.turn == *forecast.front().completionTurn);
    if (unloading) assert(actual.turn == 3); // The reported P2 was incorrectly Turn 2.
    assert(state.planets.front().population == (unloading ? 500U : 100500U));
}

static void cargo_mining_upkeep(std::uint64_t beforePopulation)
{
    auto state = make_demo_game();
    state.players.front().technology.levels.fill(20);
    auto& p = state.planets.front();
    p.population = beforePopulation; p.industry = p.mines = 100;
    p.observedConcentration = MineralCargo{2,2,2}; p.minerals = {};
    p.productionQueue.clear();
    p.engineering.kind = PlanetaryEngineeringKind::OrbitalHabitat;
    p.engineering.work = engineering_work(p.engineering.kind); p.engineering.operational = true;
    p.habitat.population = 100000; p.habitat.service = HabitatService::Normal; p.habitat.reserveYears = 0;
    state.orbitalStations.front().hull = OrbitalStationHullType::Starbase;
    auto& design = state.shipDesigns.front(); design.hull = ShipHullType::SuperFreighter;
    design.components = {ShipComponentType::FusionDrive}; design.placements.clear();
    auto& f = state.fleets.front(); f.colonists = 100000;
    f.telemetry = {}; f.telemetry = confirmed_fleet_telemetry(state,f);
    set_survey_level(state,1,1,SurveyLevel::DeepSurvey,state.turn);
    const PlayerOrders orders{1,{TransferCargoOrder{{0,1},{1,0},100000,{}}}};
    const auto received = make_player_view(state,1).state;
    const auto mining = projected_mineral_mining(received,received.planets.front());
    const auto planned = projected_cargo_order_state(received,orders);
    const auto output = net_colony_output(planned,planned.planets.front(),mining);
    const auto growth = projected_orbital_growth(planned,planned.planets.front(),mining);
    const auto actual = TurnProcessor{}.process(state,{orders});
    assert(actual.planets.front().habitat.service == HabitatService::Failing);
    assert(actual.planets.front().habitat.population == 90000 && growth == 0);
    assert(output == 100 && output == colony_output(actual,actual.planets.front()));
}

int main()
{
    for (const bool unloading : {false,true}) for (const auto quantity : {1U,10U})
        cargo_mining_forecast(unloading,quantity);
    for (const auto population : {0U,500U}) cargo_mining_upkeep(population);
    auto state = make_demo_game();
    auto& planet = state.planets.front();
    planet.population = 25'000;
    planet.industry = planet.mines = 100;
    planet.observedConcentration = MineralCargo{100, 100, 100};
    const auto workers = colony_workforce(state, planet);
    assert(workers.factoryLimit == 25 && workers.mineLimit == 25);
    assert(workers.operatingFactories == 25 && workers.operatingMines == 25);
    assert(colony_output(state, planet) == 25);
    const auto extraction = projected_mineral_mining(state, planet);
    assert(std::abs(extraction.ironium - (1 + 25000.0 / 750000 + 25 * 0.75)) < 1e-9);
    auto extra = planet;
    ++extra.industry; ++extra.mines;
    assert(colony_output(state, extra) == 25);
    assert(projected_mineral_mining(state, extra).ironium == extraction.ironium);

    // A loss of residents idles infrastructure; neither type competes for a
    // shared worker pool, and a later recovery reactivates the same buildings.
    planet.population = 1'000;
    assert(colony_workforce(state, planet).operatingFactories == 1);
    assert(colony_workforce(state, planet).operatingMines == 1);
    planet.population = 999;
    assert(colony_output(state, planet) == 0);
    assert(colony_workforce(state, planet).operatingMines == 0);
    planet.population = 100'000;
    assert(colony_workforce(state, planet).operatingFactories == 100);
    assert(planet.industry == 100 && planet.mines == 100);

    auto& race = state.players.front().race;
    race.factoriesPerTenThousand = 5;
    race.minesPerTenThousand = 25;
    planet.population = 25'000;
    assert(colony_workforce(state, planet).operatingFactories == 12);
    assert(colony_workforce(state, planet).operatingMines == 62);
    planet.population = 12'499;
    assert(colony_workforce(state, planet).factoryLimit == 6);
    assert(colony_workforce(state, planet).mineLimit == 31);

    Player other; other.id = 2;
    other.race.factoriesPerTenThousand = 25;
    other.race.minesPerTenThousand = 5;
    state.players.push_back(other);
    planet.owner = 2; planet.population = 25'000;
    assert(colony_workforce(state, planet).operatingFactories == 62);
    assert(colony_workforce(state, planet).operatingMines == 12);
    planet.owner = 0;
    assert(!colony_output(state, planet) && !colony_workforce(state, planet).operatingMines);

    // Live cargo planning and ordinary turn resolution use the same limits.
    state = make_demo_game();
    auto& colony = state.planets.front();
    colony.population = 25'000; colony.industry = colony.mines = 100;
    colony.observedConcentration = MineralCargo{100, 100, 100};
    auto& fleet = state.fleets.front();
    fleet.design = kColonyShipDesignId; fleet.ships = {{kColonyShipDesignId, 5}};
    fleet.telemetry = {}; fleet.telemetry = confirmed_fleet_telemetry(state, fleet);
    set_survey_level(state, 1, 1, SurveyLevel::DeepSurvey, state.turn);
    const PlayerOrders load{1, {SetFleetColonistsOrder{1, 1, 2500}}};
    const auto planned = projected_cargo_order_state(make_player_view(state, 1).state, load);
    assert(planned.planets.front().population == 22'500);
    assert(colony_workforce(planned, planned.planets.front()).operatingFactories == 22);
    assert(colony_workforce(planned, planned.planets.front()).operatingMines == 22);
    assert(colony_output(planned, planned.planets.front()) == 22);
    assert(colony.population == 25'000 && fleet.colonists == 0);
    const auto mined = projected_mineral_mining(state, colony);
    const auto next = TurnProcessor{}.process(state, {load});
    assert(next.planets.front().industry == 100 && next.planets.front().mines == 100);
    assert(std::abs(next.planets.front().minerals.ironium - colony.minerals.ironium - mined.ironium) < 1e-8);
    assert(next.fleets.front().colonists == 2500);
    assert(next.planets.front().population < colony.population);

    auto researchState = state;
    researchState.players.front().technology.researchAllocationPercent = 100;
    researchState.players.front().technology.researchActive = true;
    assert(resolve_colony_production(researchState, researchState.planets.front(), true).researchPoints == 25);
    colony.productionQueue = {{ProductionKind::Factory, kFactoryCost}};
    (void)resolve_colony_production(state, colony, true);
    assert(colony.industry == 101 && colony_workforce(state, colony).operatingFactories == 25);

    // Orbit residents retain their own output and do not operate surface mines
    // or surface factories. An empty surface has no ordinary mineral extraction.
    colony.productionQueue.clear(); colony.population = 0;
    colony.engineering.kind = PlanetaryEngineeringKind::OrbitalHabitat;
    colony.engineering.work = engineering_work(colony.engineering.kind);
    colony.engineering.operational = true;
    colony.habitat.population = 1'500'000;
    colony.habitat.service = HabitatService::Normal;
    assert(!colony_workforce(state, colony).operatingFactories);
    assert(!colony_workforce(state, colony).operatingMines);
    assert(!projected_mineral_mining(state, colony).ironium);
    assert(colony_output(state, colony) == 3);

    colony.population = std::numeric_limits<std::uint64_t>::max();
    colony.industry = colony.mines = std::numeric_limits<std::uint32_t>::max();
    state.players.front().race.factoriesPerTenThousand = 25;
    state.players.front().race.minesPerTenThousand = 25;
    assert(colony_workforce(state, colony).factoryLimit == colony.industry);
    assert(colony_workforce(state, colony).operatingMines == colony.mines);
    assert(colony_output(state, colony) == std::numeric_limits<std::uint32_t>::max());

    EmpireSetup setup{"Workers", RacePreset::Terran};
    setup.factoriesPerTenThousand = 5; setup.minesPerTenThousand = 25;
    const auto campaign = generate_campaign(GalaxyConfig{}, {setup});
    assert(campaign.players.front().race.factoriesPerTenThousand == 5);
    assert(campaign.players.front().race.minesPerTenThousand == 25);
    assert(make_player_view(campaign, 1).state.players.front().race.minesPerTenThousand == 25);
    for (int bad : {0, 4, 26, 255}) {
        setup.factoriesPerTenThousand = std::uint8_t(bad);
        bool rejected = false;
        try { (void)generate_campaign(GalaxyConfig{}, {setup}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }

    // A forecast must wait for growing workforce even when the first year has
    // zero usable factories, then match the ordinary yearly resolver.
    for (int rate : {5, 25}) {
        auto growing = make_demo_game();
        growing.players.front().race.factoriesPerTenThousand = std::uint8_t(rate);
        auto& world = growing.planets.front();
        world.population = rate == 5 ? 3900 : 390;
        world.industry = 20;
        world.minerals = {1000, 1000, 1000};
        world.productionQueue = {{ProductionKind::Factory, kFactoryCost}};
        const auto forecast = forecast_production_queue(growing, world, world.productionQueue, 20);
        assert(forecast.size() == 1 && forecast.front().completionTurn);
        auto resolved = growing;
        for (int year = 0; year < 20 && resolved.planets.front().industry == 20; ++year)
            resolved = TurnProcessor{}.process(resolved, {});
        assert(resolved.planets.front().industry == 21 && resolved.turn == *forecast.front().completionTurn);
    }
    auto declining = make_demo_game();
    auto& shrinking = declining.planets.front();
    shrinking.population = 5000; shrinking.industry = 100;
    shrinking.habitability = -100;
    shrinking.minerals = {1000, 1000, 1000};
    shrinking.productionQueue = {{ProductionKind::Factory, kFactoryCost}};
    shrinking.productionQueue.front().quantity = 20;
    const auto stranded = forecast_production_queue(declining, shrinking, shrinking.productionQueue, 100);
    assert(!stranded.front().completionTurn && stranded.front().beyondForecastHorizon);
    for (int year = 0; year < 100; ++year) declining = TurnProcessor{}.process(declining, {});
    assert(!declining.planets.front().productionQueue.empty() && declining.planets.front().industry < 120);
    assert(!colony_workforce(declining, declining.planets.front()).operatingFactories);

    // Existing AI development obeys the same received limit, and resumes
    // useful construction when a delivered population increase permits it.
    auto computer = make_demo_game();
    computer.players.front().controller = PlayerController::Computer;
    computer.stars.resize(1); computer.planets.resize(1); computer.fleets.clear();
    auto& home = computer.planets.front();
    home.population = 5000; home.industry = home.mines = 5;
    computer.players.front().surveyKnowledge.clear();
    set_survey_level(computer, 1, 1, SurveyLevel::DeepSurvey, computer.turn);
    const auto builds = [](const ComputerTurnPlan& plan, ProductionKind kind) {
        return std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const auto& order) {
            if (const auto* one = std::get_if<QueueProductionOrder>(&order)) return one->kind == kind;
            if (const auto* batch = std::get_if<QueueProductionBatchOrder>(&order)) return batch->kind == kind;
            return false;
        });
    };
    const auto idlePlan = plan_computer_turn(make_player_view(computer, 1));
    assert(!builds(idlePlan, ProductionKind::Factory) && !builds(idlePlan, ProductionKind::Mine));
    home.population = 6000;
    set_survey_level(computer, 1, 1, SurveyLevel::DeepSurvey, computer.turn);
    const auto growingPlan = plan_computer_turn(make_player_view(computer, 1));
    assert(builds(growingPlan, ProductionKind::Factory));
}
