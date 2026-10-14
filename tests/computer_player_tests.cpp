#include "suns/computer_player.hpp"
#include "suns/communications.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <type_traits>

using namespace suns;

namespace {

template<class Action> void rejects(Action action)
{
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

std::vector<EmpireSetup> mixed_setup()
{
    std::vector<EmpireSetup> empires{{"Local", RacePreset::Terran},
        {"Computer", RacePreset::Cryophile}, {"Remote", RacePreset::Radiotroph}};
    empires[1].controller = PlayerController::Computer;
    return empires;
}

void same_plan(const ComputerTurnPlan& a, const ComputerTurnPlan& b)
{
    assert(a.orders.player == b.orders.player && a.orders.orders.size() == b.orders.orders.size());
    assert(a.nextState == b.nextState);
    for (std::size_t i = 0; i < a.orders.orders.size(); ++i) {
        assert(a.orders.orders[i].index() == b.orders.orders[i].index());
        if (const auto* allocation = std::get_if<SetResearchAllocationOrder>(&a.orders.orders[i]))
            assert(allocation->percent == std::get<SetResearchAllocationOrder>(b.orders.orders[i]).percent);
        else std::visit([&](const auto& order) {
            using T = std::decay_t<decltype(order)>;
            const auto& other = std::get<T>(b.orders.orders[i]);
            if constexpr (std::is_same_v<T, SetResearchPlanOrder>)
                assert(order.focus == other.focus && order.queuedFocuses == other.queuedFocuses && order.active == other.active);
            else if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                assert(order.fleet == other.fleet && same_position(order.destination, other.destination)
                    && order.warp == other.warp && order.arrivalAction.kind == other.arrivalAction.kind
                    && order.arrivalAction.reservePopulation == other.arrivalAction.reservePopulation
                    && order.queuedWaypoints.size() == other.queuedWaypoints.size());
                for (std::size_t n = 0; n < order.queuedWaypoints.size(); ++n)
                    assert(same_position(order.queuedWaypoints[n].destination, other.queuedWaypoints[n].destination)
                        && order.queuedWaypoints[n].warp == other.queuedWaypoints[n].warp
                        && order.queuedWaypoints[n].arrivalAction.kind == other.queuedWaypoints[n].arrivalAction.kind);
            } else if constexpr (std::is_same_v<T, QueueShipDesignOrder>)
                assert(order.colony == other.colony && order.design == other.design && order.pendingDesignName == other.pendingDesignName);
            else if constexpr (std::is_same_v<T, CreateShipDesignOrder>)
                assert(order.name == other.name && order.hull == other.hull && order.components == other.components);
            else if constexpr (std::is_same_v<T, QueueProductionBatchOrder>)
                assert(order.colony == other.colony && order.kind == other.kind && order.quantity == other.quantity && order.automation == other.automation);
            else if constexpr (std::is_same_v<T, QueueProductionOrder>)
                assert(order.colony == other.colony && order.kind == other.kind);
            else if constexpr (std::is_same_v<T, SetFleetColonistsOrder>)
                assert(order.colony == other.colony && order.fleet == other.fleet && order.colonists == other.colonists);
            else if constexpr (std::is_same_v<T, MergeFleetsOrder>)
                assert(order.destination == other.destination && order.source == other.source);
            else if constexpr (std::is_same_v<T, SetFleetTaskOrder>)
                assert(order.fleet == other.fleet && order.task == other.task);
            else if constexpr (std::is_same_v<T, QueueStationOrder>)
                assert(order.colony == other.colony && order.construction.refitStation == other.construction.refitStation
                    && order.construction.loadout.hull == other.construction.loadout.hull
                    && order.construction.loadout.components == other.construction.loadout.components);
            else assert(false && "Unexpected computer order in replay comparison");
        }, a.orders.orders[i]);
    }
}

void integration_and_fairness()
{
    auto setup = mixed_setup();
    auto state = generate_campaign(GalaxyConfig{}, setup);
    setup[1].controller = PlayerController::Human;
    const auto humans = generate_campaign(GalaxyConfig{}, setup);
    for (std::size_t i = 0; i < state.planets.size(); ++i) {
        const auto& a = state.planets[i]; const auto& b = humans.planets[i];
        assert(a.population == b.population && a.industry == b.industry && a.mines == b.mines);
        assert(a.minerals.ironium == b.minerals.ironium && a.minerals.boranium == b.minerals.boranium
            && a.minerals.germanium == b.minerals.germanium);
    }
    for (std::size_t i = 0; i < state.fleets.size(); ++i) {
        assert(state.fleets[i].fuel == humans.fleets[i].fuel && state.fleets[i].design == humans.fleets[i].design);
        assert(state.shipDesigns[i].components == humans.shipDesigns[i].components);
        assert(state.players[i].technology.levels == humans.players[i].technology.levels);
    }
    setup[1].controller = static_cast<PlayerController>(2);
    rejects([&] { (void)generate_campaign(GalaxyConfig{}, setup); });

    assert((missing_human_submissions(state, {}) == std::vector<PlayerId>{1, 3}));
    assert((missing_human_submissions(state, {{1, {}}}) == std::vector<PlayerId>{3}));
    assert(missing_human_submissions(state, {{3, {}}, {1, {}}}).empty());
    rejects([&] { (void)missing_human_submissions(state, {{1, {}}, {1, {}}}); });
    rejects([&] { (void)missing_human_submissions(state, {{1, {}}, {2, {}}}); });
    rejects([&] { (void)missing_human_submissions(state, {{99, {}}}); });
    rejects([&] { (void)resolve_controlled_campaign_turn(state, {{1, {}}}); });
    assert(!state.players[1].computer.initialized && state.players[1].computer.decisions.empty());
    // The existing exhaustive resolver still demands an explicit AI envelope.
    rejects([&] { (void)resolve_campaign_turn(state, {{1, {}}, {3, {}}}); });

    const auto view = make_player_view(state, 2);
    const auto plan = plan_computer_turn(view);
    assert(plan.orders.player == 2 && plan.orders.orders.size() >= 2);
    assert(std::get<SetResearchAllocationOrder>(plan.orders.orders[0]).percent == 20);
    assert(plan.nextState.initialized && plan.nextState.decisions.back().turn == 1);
    rejects([&] { (void)plan_computer_turn(make_player_view(state, 1)); });
    rejects([&] { (void)plan_computer_turn(PlayerView{2, state}); });

    auto hidden = state;
    hidden.players[0].technology.levels.fill(20);
    hidden.players[0].pendingSurveyReports.clear();
    hidden.fleets[0].destination = Position{999, 999};
    hidden.fleets[0].colonists = 500000;
    hidden.fleets[0].fuel = 0;
    for (auto& planet : hidden.planets) if (planet.owner != 2) {
        planet.population = 9999999;
        planet.minerals = {9999, 9999, 9999};
        planet.productionQueue.push_back({ProductionKind::Factory, 6, 0});
    }
    same_plan(plan, plan_computer_turn(make_player_view(hidden, 2)));
    for (auto& fleet : hidden.fleets) if (fleet.owner == 2) {
        // Physical changes and undelivered packets cannot alter confirmed knowledge.
        fleet.position = {999, 999}; fleet.fuel = 0;
        fleet.pendingCommands.push_back({1, 99, {{1000, 1000}, 1}});
        fleet.telemetryInTransit.push_back({99, {2, {999, 999}}});
    }
    same_plan(plan, plan_computer_turn(make_player_view(hidden, 2)));

    const auto automatic = resolve_controlled_campaign_turn(state,
        {{3, {SetResearchAllocationOrder{75}}}, {1, {}}});
    const auto manual = resolve_campaign_turn(state,
        {{1, {}}, plan.orders, {3, {SetResearchAllocationOrder{75}}}});
    assert(automatic.state.turn == 2);
    for (const auto& player : automatic.state.players) {
        const auto* other = find_player(manual.state, player.id);
        assert(player.technology.levels == other->technology.levels
            && player.technology.progress == other->technology.progress
            && player.technology.researchAllocationPercent == other->technology.researchAllocationPercent);
    }
    for (std::size_t i = 0; i < automatic.state.planets.size(); ++i) {
        const auto& a = automatic.state.planets[i]; const auto& b = manual.state.planets[i];
        assert(a.population == b.population && a.industry == b.industry && a.mines == b.mines);
        assert(a.minerals.ironium == b.minerals.ironium && a.minerals.boranium == b.minerals.boranium
            && a.minerals.germanium == b.minerals.germanium);
    }
    assert(find_player(automatic.state, 2)->computer == plan.nextState);
    assert(computer_state_valid(*find_player(automatic.state, 2), automatic.state.turn));
    const auto continuation = plan_computer_turn(make_player_view(automatic.state, 2));
    assert(continuation.nextState.fleetGoals == plan.nextState.fleetGoals);
    assert(std::none_of(continuation.orders.orders.begin(), continuation.orders.orders.end(),
        [](const auto& order) { return std::holds_alternative<MoveFleetOrder>(order); }));
    auto reordered = state;
    std::reverse(reordered.players.begin(), reordered.players.end());
    const auto replay = resolve_controlled_campaign_turn(reordered,
        {{1, {}}, {3, {SetResearchAllocationOrder{75}}}});
    assert(find_player(replay.state, 2)->computer == find_player(automatic.state, 2)->computer);
    assert(find_player(replay.state, 2)->technology.progress == find_player(automatic.state, 2)->technology.progress);
}

void retain_settings_and_bounded_replays()
{
    auto state = generate_campaign(GalaxyConfig{}, mixed_setup());
    state.players[1].technology.researchAllocationPercent = 55;
    state.players[1].technology.focus = ResearchField::Propulsion;
    state.players[1].technology.queuedFocuses = {ResearchField::Biology};
    const auto retained = plan_computer_turn(make_player_view(state, 2));
    assert(std::none_of(retained.orders.orders.begin(), retained.orders.orders.end(), [](const auto& order) {
        return std::holds_alternative<SetResearchPlanOrder>(order) || std::holds_alternative<SetResearchAllocationOrder>(order);
    }));
    state.players[1].technology.researchActive = false;
    const auto resume = plan_computer_turn(make_player_view(state, 2));
    const auto& research = std::get<SetResearchPlanOrder>(resume.orders.orders.front());
    assert(research.active && research.focus == ResearchField::Propulsion
        && research.queuedFocuses == std::vector<ResearchField>{ResearchField::Biology});

    for (const auto seed : {17ULL, 1024ULL, 20260817ULL}) {
        auto setup = mixed_setup(); setup[2].controller = PlayerController::Computer;
        auto config = GalaxyConfig{}; config.seed = seed;
        auto current = generate_campaign(config, setup);
        for (int turn = 0; turn < 36; ++turn) {
            auto a = resolve_controlled_campaign_turn(current, {{1, {}}}).state;
            const auto b = resolve_controlled_campaign_turn(current, {{1, {}}}).state;
            for (PlayerId id : {2u, 3u}) {
                const auto* computer = find_player(a, id);
                assert(computer->computer == find_player(b, id)->computer);
                assert(computer->technology.progress == find_player(b, id)->technology.progress);
                assert(computer->computer.decisions.size() == std::min<std::size_t>(turn + 1, 32));
                assert(computer->computer.decisions.back().reason.size() <= kMaximumComputerDecisionReasonBytes);
                assert(computer_state_valid(*computer, a.turn));
            }
            current = std::move(a);
        }
    }
    auto computers = mixed_setup();
    for (auto& setup : computers) setup.controller = PlayerController::Computer;
    auto allComputers = generate_campaign(GalaxyConfig{}, computers);
    assert(resolve_controlled_campaign_turn(allComputers, {}).state.turn == 2);
}

GameState expansion_fixture()
{
    std::vector<EmpireSetup> setup{{"Human", RacePreset::Terran}, {"Computer", RacePreset::Terran}};
    setup[1].controller = PlayerController::Computer;
    auto config = GalaxyConfig{}; config.starCount = 8;
    auto state = generate_campaign(config, setup);
    state.wormholeRules.spawnChancePerTurn = 0;
    PlanetEnvironment environment;
    for (const auto& planet : state.planets) if (planet.owner == 2) environment = planet.environment;
    int unowned = 1;
    for (auto& planet : state.planets) {
        auto* star = const_cast<StarSystem*>(find_star(state, planet.star));
        star->position = planet.owner == 2 ? Position{0, 0}
            : planet.owner == 1 ? Position{-1000, 0} : Position{40.0 * unowned++, 0};
        star->variability = {};
        planet.environment = environment;
        planet.naturalEnvironment.reset();
    }
    for (auto& player : state.players) { player.surveyKnowledge.clear(); player.surveyedStars.clear(); }
    for (auto& fleet : state.fleets) {
        const auto home = std::find_if(state.planets.begin(), state.planets.end(), [&](const auto& p) { return p.owner == fleet.owner; });
        fleet.position = find_star(state, home->star)->position;
        fleet.telemetry.position = fleet.position;
    }
    refresh_sensor_intel(state);
    return state;
}

void known_targets_and_conservative_fuel()
{
    auto state = expansion_fixture();
    auto target = std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 0; });
    const auto unknown = target->star;
    ++target;
    target->owner = 1;
    set_survey_level(state, 2, target->star, SurveyLevel::OrbitalSurvey, 1);
    ++target;
    target->environment = {100, 100, 100};
    set_survey_level(state, 2, target->star, SurveyLevel::OrbitalSurvey, 1);
    ++target;
    const auto known = target->star;
    set_survey_level(state, 2, known, SurveyLevel::OrbitalSurvey, 1);
    assert(known_planet_habitability(state, 2, target->id).value() >= 20);
    const auto design = std::find_if(state.shipDesigns.begin(), state.shipDesigns.end(), [](const auto& d) {
        return d.owner == 2 && ship_design_can_colonize(d);
    });
    Fleet colonizer;
    colonizer.id = state.nextFleetId++; colonizer.owner = 2; colonizer.design = design->id;
    colonizer.position = {0, 0}; colonizer.colonists = 500;
    colonizer.fuel = ship_design_fuel_capacity(*design);
    colonizer.telemetry.observedTurn = 1; colonizer.telemetry.position = colonizer.position;
    colonizer.telemetry.colonists = colonizer.colonists; colonizer.telemetry.fuel = colonizer.fuel;
    state.fleets.push_back(colonizer);
    const auto plan = plan_computer_turn(make_player_view(state, 2));
    const auto colonyGoal = std::find_if(plan.nextState.fleetGoals.begin(), plan.nextState.fleetGoals.end(),
        [&](const auto& g) { return g.fleet == colonizer.id; });
    assert(colonyGoal != plan.nextState.fleetGoals.end() && colonyGoal->kind == ComputerFleetGoalKind::Colonize);
    assert(colonyGoal->target == known && colonyGoal->target != unknown);
    const auto result = resolve_controlled_campaign_turn(state, {{1, {}}});
    assert(result.state.turn == 2);

    // With no known depot, a fuel-starved scout must hold rather than launch a
    // speculative route based on an authoritative future refill.
    state = expansion_fixture();
    state.orbitalStations.clear();
    for (auto& fleet : state.fleets) if (fleet.owner == 2) { fleet.fuel = 0; fleet.telemetry.fuel = 0; }
    const auto stranded = plan_computer_turn(make_player_view(state, 2));
    assert(std::none_of(stranded.orders.orders.begin(), stranded.orders.orders.end(),
        [](const auto& order) { return std::holds_alternative<MoveFleetOrder>(order); }));

    // The planned round trip must complete through shared movement without
    // a fuel stall. Disable further goals after issuing the route.
    state = expansion_fixture();
    auto view = make_player_view(state, 2);
    auto& scout = *std::find_if(view.state.fleets.begin(), view.state.fleets.end(), [](const auto& f) { return f.owner == 2; });
    const auto fleetId = scout.id;
    view.state.orbitalStations.clear();
    view.state.orbitalStations.push_back({99, 2, std::find_if(view.state.planets.begin(), view.state.planets.end(),
        [](const auto& p) { return p.owner == 2; })->id, "Refuel", OrbitalStationHullType::OrbitalDock,
        {OrbitalStationModule::RefuelingDepot}});
    const auto conservative = plan_computer_turn(view);
    const auto move = std::find_if(conservative.orders.orders.begin(), conservative.orders.orders.end(),
        [](const auto& order) { return std::holds_alternative<MoveFleetOrder>(order); });
    assert(move != conservative.orders.orders.end());
    const auto& route = std::get<MoveFleetOrder>(*move);
    assert(!route.queuedWaypoints.empty() && route.queuedWaypoints.back().arrivalAction.kind == FleetArrivalActionKind::Refuel);
    auto travel = resolve_campaign_turn(state, {{1, {}}, {2, {*move}}}).state;
    for (int year = 0; year < 80; ++year) {
        const auto fleet = std::find_if(travel.fleets.begin(), travel.fleets.end(), [&](const auto& f) { return f.id == fleetId; });
        assert(fleet != travel.fleets.end() && !fleet->fuelStalled && fleet->damagePercent == 0);
        if (!fleet->destination && fleet->waypointQueue.empty()) {
            assert(same_position(fleet->position, Position{0, 0}));
            assert(fleet->fuel == fleet_fuel_capacity(travel, *fleet));
            return;
        }
        travel = resolve_campaign_turn(travel, {{1, {}}, {2, {}}}).state;
    }
    assert(false && "Fuel-budgeted survey did not return");
}

void delayed_commands_and_projection()
{
    auto state = expansion_fixture();
    for (auto& star : state.stars) if (find_planet_at_star(state, star.id)->owner == 0) star.position.x += 600;
    auto& scout = *std::find_if(state.fleets.begin(), state.fleets.end(), [](const auto& f) { return f.owner == 2; });
    scout.position = {600, 0}; scout.telemetry.position = scout.position;
    const auto id = scout.id;
    assert(communication_delay_turns(state, 2, scout.position) > 0);
    const auto initial = plan_computer_turn(make_player_view(state, 2));
    assert(!initial.nextState.fleetGoals.empty());
    const auto goal = initial.nextState.fleetGoals.front();
    auto first = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
    const auto fleet = std::find_if(first.fleets.begin(), first.fleets.end(), [&](const auto& f) { return f.id == id; });
    assert(fleet->pendingCommands.size() == 1);
    bool delivered = false;
    for (int year = 0; year < 20; ++year) {
        const auto plan = plan_computer_turn(make_player_view(first, 2));
        assert(std::none_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const auto& order) {
            const auto* move = std::get_if<MoveFleetOrder>(&order); return move && move->fleet == id;
        }));
        const auto remembered = std::find_if(plan.nextState.fleetGoals.begin(), plan.nextState.fleetGoals.end(),
            [&](const auto& g) { return g.fleet == id; });
        assert(remembered != plan.nextState.fleetGoals.end() && remembered->issuedTurn == goal.issuedTurn);
        first = resolve_controlled_campaign_turn(first, {{1, {}}}).state;
        const auto observed = std::find_if(first.fleets.begin(), first.fleets.end(), [&](const auto& f) { return f.id == id; });
        assert(observed->pendingCommands.size() <= 1 && !observed->fuelStalled);
        delivered = delivered || observed->pendingCommands.empty();
    }
    assert(delivered);

    auto projected = make_player_view(first, 2);
    auto& contact = *std::find_if(projected.state.fleets.begin(), projected.state.fleets.end(), [&](const auto& f) { return f.id == id; });
    const auto* target = find_star(projected.state, goal.target);
    contact.position = target->position;
    contact.destination = target->position;
    contact.telemetry.position = {target->position.x - 10, target->position.y};
    contact.telemetry.destination = target->position;
    contact.telemetry.observedTurn = projected.state.turn - 1;
    const auto pending = plan_computer_turn(projected);
    assert(std::any_of(pending.nextState.fleetGoals.begin(), pending.nextState.fleetGoals.end(),
        [&](const auto& g) { return g.fleet == id && g.issuedTurn == goal.issuedTurn; }));
}

void autonomous_fixed_seed_campaigns()
{
    for (const auto seed : {17ULL, 1024ULL, 20260817ULL}) {
        auto setup = mixed_setup(); setup[2].controller = PlayerController::Computer;
        auto config = GalaxyConfig{}; config.seed = seed;
        auto state = generate_campaign(config, setup);
        state.wormholeRules.spawnChancePerTurn = 0;
        for (int year = 0; year < 120; ++year) {
            for (PlayerId id : {2u, 3u}) {
                const auto view = make_player_view(state, id);
                const auto plan = plan_computer_turn(view);
                auto shuffled = view;
                std::reverse(shuffled.state.stars.begin(), shuffled.state.stars.end());
                std::reverse(shuffled.state.planets.begin(), shuffled.state.planets.end());
                std::reverse(shuffled.state.fleets.begin(), shuffled.state.fleets.end());
                std::reverse(shuffled.state.shipDesigns.begin(), shuffled.state.shipDesigns.end());
                same_plan(plan, plan_computer_turn(shuffled));
                for (const auto& order : plan.orders.orders) if (const auto* move = std::get_if<MoveFleetOrder>(&order)) {
                    const auto* fleet = &*std::find_if(view.state.fleets.begin(), view.state.fleets.end(),
                        [&](const auto& f) { return f.id == move->fleet; });
                    assert(fleet->owner == id && fleet_warp_valid(view.state, *fleet, move->warp));
                    assert(fleet_overdrive_damage_rate(view.state, *fleet, move->warp) == 0);
                }
            }
            state = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
            for (const auto& fleet : state.fleets) if (fleet.owner != 1) {
                assert(!fleet.fuelStalled && fleet.damagePercent >= 0 && fleet.damagePercent <= 100);
                assert(fleet.pendingCommands.size() <= 1);
            }
        }
        for (PlayerId id : {2u, 3u}) {
            const auto* player = find_player(state, id);
            std::size_t colonies = 0, surveys = 0; std::uint32_t factories = 0, mines = 0, science = 0;
            for (const auto& planet : state.planets) if (planet.owner == id) {
                ++colonies; factories += planet.industry; mines += planet.mines;
                assert(planet.productionQueue.size() <= 1);
            }
            for (const auto& known : player->surveyKnowledge) if (known.level >= SurveyLevel::OrbitalSurvey) ++surveys;
            for (const auto level : player->technology.levels) science += level;
            std::cout << "seed=" << seed << " empire=" << id << " colonies=" << colonies
                << " surveys=" << surveys << " factories=" << factories << " mines=" << mines
                << " science=" << science << '\n';
            assert(colonies >= 2 && surveys >= 4 && factories >= 8 && mines >= 2 && science >= 5);
            assert(computer_state_valid(*player, state.turn));
        }
    }
}

void legal_civilian_designs_and_recovery()
{
    for (int flags = 0; flags < 4; ++flags) {
        std::vector<EmpireSetup> setup{{"Human", RacePreset::Terran}, {"Computer", RacePreset::Terran}};
        setup[1].controller = PlayerController::Computer;
        setup[1].improvedFuelEfficiency = (flags & 1) != 0;
        setup[1].noRamScoopEngines = (flags & 2) != 0;
        auto state = generate_campaign(GalaxyConfig{}, setup);
        state.players[1].technology.levels.fill(8);
        for (auto& colony : state.planets) if (colony.owner == 2) {
            colony.mines = 2; colony.minerals = {1000, 1000, 1000};
        }
        std::erase_if(state.fleets, [](const auto& fleet) { return fleet.owner == 2; });
        const auto view = make_player_view(state, 2);
        const auto plan = plan_computer_turn(view);
        const auto designs = planned_ship_design_state(view.state, plan.orders);
        bool created = false, queued = false;
        for (const auto& order : plan.orders.orders) {
            if (const auto* design = std::get_if<CreateShipDesignOrder>(&order)) {
                created = true;
                const auto match = std::find_if(designs.shipDesigns.begin(), designs.shipDesigns.end(),
                    [&](const auto& d) { return d.owner == 2 && d.name == design->name; });
                assert(match != designs.shipDesigns.end() && ship_design_valid(*match));
                assert(ship_design_available_to_player(view.state, 2, *match));
                for (const auto component : match->components) assert(component_available_to_player(view.state, 2, component));
            }
            if (const auto* ship = std::get_if<QueueShipDesignOrder>(&order)) {
                queued = true;
                assert(ship->design == 0 && !ship->pendingDesignName.empty());
                assert(std::any_of(designs.shipDesigns.begin(), designs.shipDesigns.end(),
                    [&](const auto& d) { return d.owner == 2 && d.name == ship->pendingDesignName; }));
            }
        }
        assert(created && queued);
        const auto resolved = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
        const auto colony = std::find_if(resolved.planets.begin(), resolved.planets.end(), [](const auto& p) { return p.owner == 2; });
        assert(!colony->productionQueue.empty() && colony->productionQueue.front().shipDesign != 0);
    }

    auto state = expansion_fixture();
    state.turn = 5;
    auto& player = state.players[1];
    player.computer.initialized = true;
    player.computer.decisions = {{1, 1, "Issued a survey route"}};
    auto& scout = *std::find_if(state.fleets.begin(), state.fleets.end(), [](const auto& f) { return f.owner == 2; });
    auto* design = const_cast<ShipDesign*>(find_ship_design(state, scout.design));
    design->hull = ShipHullType::StarsScout;
    design->components = {ShipComponentType::QuickJump5, ShipComponentType::RhinoScanner};
    design->placements.clear(); normalize_ship_design_placement(*design);
    assert(ship_design_valid(*design));
    const auto target = std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 0; });
    player.computer.fleetGoals = {{scout.id, ComputerFleetGoalKind::Survey, target->star, 1, 3}};
    scout.position = {20, 0}; scout.destination = find_star(state, target->star)->position;
    scout.fuel = 0; scout.warp = 5;
    scout.telemetry.observedTurn = 5; scout.telemetry.position = scout.position;
    scout.telemetry.destination = scout.destination; scout.telemetry.fuel = 0; scout.telemetry.warp = 5;
    const auto recovery = plan_computer_turn(make_player_view(state, 2));
    const auto route = std::find_if(recovery.orders.orders.begin(), recovery.orders.orders.end(), [](const auto& order) {
        return std::holds_alternative<MoveFleetOrder>(order);
    });
    assert(route != recovery.orders.orders.end() && std::get<MoveFleetOrder>(*route).warp < 5);
    auto slow = scout; slow.warp = std::get<MoveFleetOrder>(*route).warp;
    assert(fleet_fuel_consumption_for_distance(state, slow, warp_distance(slow.warp)) == 0);
    assert(recovery.nextState.fleetGoals.front().issuedTurn == 5);
    const auto recovered = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
    const auto fleet = std::find_if(recovered.fleets.begin(), recovered.fleets.end(), [&](const auto& f) { return f.id == scout.id; });
    assert(!fleet->fuelStalled && fleet->position.x > scout.position.x);
    const auto waiting = plan_computer_turn(make_player_view(recovered, 2));
    assert(std::none_of(waiting.orders.orders.begin(), waiting.orders.orders.end(), [&](const auto& order) {
        const auto* move = std::get_if<MoveFleetOrder>(&order); return move && move->fleet == scout.id;
    }));
}

} // namespace

int main()
{
    integration_and_fairness();
    retain_settings_and_bounded_replays();
    known_targets_and_conservative_fuel();
    delayed_commands_and_projection();
    autonomous_fixed_seed_campaigns();
    legal_civilian_designs_and_recovery();
}
