#include "suns/computer_player.hpp"
#include "suns/communications.hpp"
#include "suns/equipment.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/stations.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <type_traits>

using namespace suns;
namespace {

GameState fixture(bool known = true)
{
    std::vector<EmpireSetup> setups{{"Human", RacePreset::Terran}, {"Computer", RacePreset::Terran}};
    setups[1].controller = PlayerController::Computer;
    auto config = GalaxyConfig{}; config.starCount = 8;
    auto state = generate_campaign(config, setups);
    state.wormholeRules.spawnChancePerTurn = 0;
    auto& own = *std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 2; });
    const auto ownEnvironment = own.environment;
    int index = 1;
    for (auto& p : state.planets) {
        auto* star = const_cast<StarSystem*>(find_star(state, p.star));
        star->position = p.owner == 2 ? Position{0, 0} : p.owner == 1 ? Position{120, 0} : Position{-100.0 * index++, 0};
        star->variability = {}; p.environment = ownEnvironment; p.naturalEnvironment.reset();
        if (p.owner) { p.industry = 24; p.mines = 12; p.minerals = {1000, 1000, 1000}; }
        if (p.owner == 2) p.population = 2'000'000;
        if (p.owner == 1) p.population = 250;
    }
    for (auto& player : state.players) { player.technology.levels.fill(6); player.surveyKnowledge.clear(); player.surveyedStars.clear(); }
    for (auto& f : state.fleets) {
        const auto home = std::find_if(state.planets.begin(), state.planets.end(), [&](const auto& p) { return p.owner == f.owner; });
        f.position = find_star(state, home->star)->position; f.telemetry.position = f.position;
    }
    std::erase_if(state.fleets, [](const auto& f) { return f.owner == 1; });
    refresh_sensor_intel(state);
    if (known) for (const auto& p : state.planets) set_survey_level(state, 2, p.star, SurveyLevel::OrbitalSurvey, 1);
    return state;
}

std::string fingerprint(const ComputerTurnPlan& plan)
{
    std::ostringstream out; out.precision(17);
    for (const auto& order : plan.orders.orders) {
        out << order.index() << ':';
        std::visit([&](const auto& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, MoveFleetOrder>) {
                out << x.fleet << ',' << x.destination.x << ',' << x.destination.y << ',' << unsigned(x.warp)
                    << ',' << int(x.arrivalAction.kind) << ',' << x.arrivalAction.reservePopulation << ',' << int(x.arrivalAction.cargo);
                for (const auto& w : x.queuedWaypoints) out << ';' << w.destination.x << ',' << w.destination.y << ',' << unsigned(w.warp) << ',' << int(w.arrivalAction.kind);
            } else if constexpr (std::is_same_v<T, CreateShipDesignOrder>) {
                out << x.name << ',' << int(x.hull); for (auto c : x.components) out << ',' << int(c);
                for (const auto& p : x.placements) out << ',' << p.slot << ',' << int(p.component);
            } else if constexpr (std::is_same_v<T, QueueShipDesignOrder>) out << x.colony << ',' << x.design << ',' << x.pendingDesignName;
            else if constexpr (std::is_same_v<T, QueueProductionOrder>) out << x.colony << ',' << int(x.kind);
            else if constexpr (std::is_same_v<T, QueueProductionBatchOrder>) out << x.colony << ',' << int(x.kind) << ',' << x.quantity << ',' << int(x.automation);
            else if constexpr (std::is_same_v<T, QueueStationOrder>) {
                out << x.colony << ',' << x.construction.refitStation << ',' << int(x.construction.loadout.hull);
                for (auto c : x.construction.loadout.components) out << ',' << int(c);
            } else if constexpr (std::is_same_v<T, SetFleetColonistsOrder>) out << x.colony << ',' << x.fleet << ',' << x.colonists;
            else if constexpr (std::is_same_v<T, MergeFleetsOrder>) out << x.destination << ',' << x.source;
            else if constexpr (std::is_same_v<T, SetFleetTaskOrder>) out << x.fleet << ',' << int(x.task);
            else if constexpr (std::is_same_v<T, SetResearchAllocationOrder>) out << unsigned(x.percent);
            else if constexpr (std::is_same_v<T, SetResearchPlanOrder>) { out << int(x.focus) << ',' << x.active; for (auto f : x.queuedFocuses) out << ',' << int(f); }
            else assert(false && "Unexpected military policy order");
        }, order);
        out << '\n';
    }
    return out.str();
}

void same_plan(const ComputerTurnPlan& a, const ComputerTurnPlan& b)
{ assert(a.nextState == b.nextState && fingerprint(a) == fingerprint(b)); }

void fair_intel_and_legal_orders()
{
    auto state = fixture();
    const auto view = make_player_view(state, 2);
    const auto original = plan_computer_turn(view);
    auto hidden = state;
    for (auto& p : hidden.planets) if (p.owner != 2) {
        p.population = 100'000'000; p.industry = 1000; p.mines = 1000;
        p.defenses = 100; p.minerals = {99999, 99999, 99999};
    }
    for (auto& station : hidden.orbitalStations) if (station.owner != 2) {
        station.hull = OrbitalStationHullType::Fortress; station.components = {ShipComponentType::ArmageddonMissile};
    }
    hidden.players[0].technology.levels.fill(40);
    hidden.players[1].pendingEnemyContacts.push_back({999, 1, {0, 0}, 0, ContactQuality::Identified,
        ContactSensor::Active, 1, 1, 100, 0, 0, 0, 9000, 9000, 500});
    same_plan(original, plan_computer_turn(make_player_view(hidden, 2)));
    auto reordered = view;
    std::reverse(reordered.state.planets.begin(), reordered.state.planets.end());
    std::reverse(reordered.state.stars.begin(), reordered.state.stars.end());
    std::reverse(reordered.state.shipDesigns.begin(), reordered.state.shipDesigns.end());
    same_plan(original, plan_computer_turn(reordered));
    const auto next = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
    assert(find_player(next, 2)->computer == original.nextState);
    assert(std::any_of(next.planets.begin(), next.planets.end(), [](const auto& p) { return p.owner == 2 && p.defenses > 0; }));
}

void assault_through_shared_rules()
{
    for (auto seed : {17ULL, 1024ULL, 20260817ULL}) {
        auto state = fixture(); state.galaxySeed = seed;
        const auto enemy = std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 1; })->id;
        bool merge = false, raid = false, bombard = false, invasion = false, battle = false;
        bool programmedBombardment = false;
        bool neutralized = false; // Bombardment can empty a colony before a ground landing is needed.
        for (int year = 0; year < 160; ++year) {
            const auto view = make_player_view(state, 2);
            const auto plan = plan_computer_turn(view);
            auto replay = plan_computer_turn(view); same_plan(plan, replay);
            const auto plannedDesigns = planned_ship_design_state(view.state, plan.orders);
            for (const auto& order : plan.orders.orders) {
                merge |= std::holds_alternative<MergeFleetsOrder>(order);
                if (const auto* route=std::get_if<MoveFleetOrder>(&order))
                    programmedBombardment |= route->arrivalAction.kind==FleetArrivalActionKind::BombardColony;
                if (const auto* design = std::get_if<CreateShipDesignOrder>(&order)) {
                    const auto found = std::find_if(plannedDesigns.shipDesigns.begin(), plannedDesigns.shipDesigns.end(), [&](const auto& d) { return d.owner == 2 && d.name == design->name; });
                    if (found == plannedDesigns.shipDesigns.end()) {
                        ShipDesign proposed{view.state.nextShipDesignId, 2, design->name, design->hull, design->components, design->placements};
                        std::cerr << "Missing design at turn " << state.turn << " " << design->name << " error="
                            << ship_design_validation_error(proposed) << " access=" << ship_design_available_to_player(view.state, 2, proposed) << '\n';
                    }
                    assert(found != plannedDesigns.shipDesigns.end() && ship_design_valid(*found));
                    assert(ship_design_available_to_player(view.state, 2, *found));
                }
            }
            for (const auto& g : plan.nextState.fleetGoals) {
                raid |= g.kind == ComputerFleetGoalKind::Raid;
                bombard |= g.kind == ComputerFleetGoalKind::Bombard;
                invasion |= g.kind == ComputerFleetGoalKind::Invade;
            }
            auto result = resolve_controlled_campaign_turn(state, {{1, {}}});
            for (const auto& event : result.events) battle |= event.recipient == 2 && event.kind == GameEventKind::SpaceBattle;
            state = std::move(result.state);
            for (const auto& fleet : state.fleets) if (fleet.owner == 2) {
                assert(!fleet.fuelStalled && fleet.pendingCommands.size() <= 1);
                assert(fleet_cargo_used(state, fleet) <= fleet_cargo_capacity(state, fleet) + 1e-6);
            }
            assert(computer_state_valid(*find_player(state, 2), state.turn));
            const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const auto& p) { return p.id == enemy; });
            neutralized |= planet->owner == 0;
            if (planet->owner == 2) break;
        }
        const auto planet = std::find_if(state.planets.begin(), state.planets.end(), [&](const auto& p) { return p.id == enemy; });
        std::cout << "military seed=" << seed << " turn=" << state.turn << " owner=" << planet->owner
            << " merge=" << merge << " raid=" << raid << " bombard=" << bombard << " invasion=" << invasion
            << " neutralized=" << neutralized << " battle=" << battle << std::endl;
        // A bombardment can eliminate a small colony, allowing legal subsequent
        // recolonization; it is not a ground invasion and must not be counted as one.
        // Ground invasion itself is covered by ground_invasion_tests.
        assert(merge && raid && bombard && battle && programmedBombardment && planet->owner == 2);
        assert(invasion || neutralized);
    }
}

void uncertain_threats_and_delayed_missions()
{
    auto state = fixture(false);
    auto& own = *std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 2; });
    auto view = make_player_view(state, 2);
    // Probable hull evidence near a colony causes defense, never a raid on an
    // unobserved owner or a precise pursuit of the hidden physical fleet.
    view.state.players.front().enemyContacts.push_back({900, 0, {80, 0}, 30, ContactQuality::Probable,
        ContactSensor::Passive, 1, 1, 1, 0, own.id, 0, 10, 80, 0});
    const auto plan = plan_computer_turn(view);
    assert(std::none_of(plan.nextState.fleetGoals.begin(), plan.nextState.fleetGoals.end(), [](const auto& g) { return g.kind == ComputerFleetGoalKind::Raid; }));

    state = fixture();
    state.turn = 10;
    auto& computer = state.players[1].computer;
    computer.initialized = true; computer.decisions = {{1, 1, "A delayed raid"}};
    auto& f = state.fleets.back();
    const auto designId = state.nextShipDesignId++;
    ShipDesign guard{designId, 2, "Guard fixture", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::Laser, ShipComponentType::Laser}};
    normalize_ship_design_placement(guard); assert(ship_design_valid(guard));
    state.shipDesigns.push_back(guard);
    f.design = designId; f.ships = {{designId, 1}};
    f.telemetry.ships = f.ships;
    f.telemetry.observedTurn = 7; f.telemetry.position = {90, 0};
    f.telemetry.destination = Position{120, 0}; f.telemetry.warp = 5;
    const auto enemy = std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 1; });
    computer.fleetGoals = {{f.id, ComputerFleetGoalKind::Raid, enemy->star, 5, 15}};
    const auto waiting = plan_computer_turn(make_player_view(state, 2));
    assert(waiting.nextState.fleetGoals.front() == computer.fleetGoals.front());
    assert(std::none_of(waiting.orders.orders.begin(), waiting.orders.orders.end(), [&](const auto& order) {
        const auto* move = std::get_if<MoveFleetOrder>(&order); return move && move->fleet == f.id;
    }));
    // An actual command packet must remain unique while it is physically in
    // flight, including newer telemetry from before its delivery.
    state = fixture();
    auto* enemyStar = const_cast<StarSystem*>(find_star(state, std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 1; })->star));
    enemyStar->position = {600, 0};
    state.shipDesigns.push_back(guard);
    state.nextShipDesignId = guard.id + 1;
    auto& remote = state.fleets.back(); remote.design = guard.id; remote.ships = {{guard.id, 1}};
    remote.position = {300, 0}; remote.telemetry.position = remote.position; remote.telemetry.ships = remote.ships;
    remote.fuel = fleet_fuel_capacity(state, remote); remote.telemetry.fuel = remote.fuel;
    const auto remoteId = remote.id;
    const auto initial = plan_computer_turn(make_player_view(state, 2));
    const auto remembered = std::find_if(initial.nextState.fleetGoals.begin(), initial.nextState.fleetGoals.end(), [&](const auto& g) { return g.fleet == remoteId; });
    assert(remembered != initial.nextState.fleetGoals.end() && remembered->kind == ComputerFleetGoalKind::Gather);
    const auto issued = remembered->issuedTurn;
    state = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
    for (int year = 0; year < 8; ++year) {
        const auto plan = plan_computer_turn(make_player_view(state, 2));
        const auto goal = std::find_if(plan.nextState.fleetGoals.begin(), plan.nextState.fleetGoals.end(), [&](const auto& g) { return g.fleet == remoteId; });
        assert(goal != plan.nextState.fleetGoals.end() && goal->issuedTurn == issued);
        assert(std::none_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const auto& order) {
            const auto* move = std::get_if<MoveFleetOrder>(&order); return move && move->fleet == remoteId;
        }));
        state = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
        const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == remoteId; });
        assert(fleet != state.fleets.end() && fleet->pendingCommands.size() <= 1 && !fleet->fuelStalled);
    }
}

void access_and_paid_work()
{
    for (int flags = 0; flags < 4; ++flags) {
        auto state = fixture();
        auto& player = state.players[1];
        player.race.improvedFuelEfficiency = (flags & 1) != 0;
        player.race.noRamScoopEngines = (flags & 2) != 0;
        for (auto& p : state.planets) if (p.owner == 2) p.defenses = 20;
        for (auto& station : state.orbitalStations) if (station.owner == 2) station.components = {ShipComponentType::Laser};
        for(const auto& p:state.planets) if(p.owner==2)set_survey_level(state,2,p.star,SurveyLevel::DeepSurvey,state.turn);
        const auto view = make_player_view(state, 2);
        const auto plan = plan_computer_turn(view);
        const auto planned = planned_ship_design_state(view.state, plan.orders);
        bool military = false;
        for (const auto& order : plan.orders.orders) if (const auto* created = std::get_if<CreateShipDesignOrder>(&order)) {
            const auto design = std::find_if(planned.shipDesigns.begin(), planned.shipDesigns.end(), [&](const auto& d) { return d.name == created->name && d.owner == 2; });
            assert(design != planned.shipDesigns.end() && ship_design_valid(*design));
            assert(ship_design_available_to_player(view.state, 2, *design));
            military |= created->name.starts_with("Guard:");
        }
        assert(military);
        auto& colony = *std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 2; });
        colony.productionQueue = {{ProductionKind::Mine, 1, 0}};
        set_survey_level(state,2,colony.star,SurveyLevel::DeepSurvey,state.turn);
        const auto retained = plan_computer_turn(make_player_view(state, 2));
        for (const auto& order : retained.orders.orders) {
            assert(!std::holds_alternative<CancelProductionOrder>(order));
            assert(!std::holds_alternative<ReorderProductionQueueOrder>(order));
            if (const auto* ship = std::get_if<QueueShipDesignOrder>(&order)) assert(ship->colony != colony.id);
            if (const auto* station = std::get_if<QueueStationOrder>(&order)) assert(station->colony != colony.id);
        }
    }
}

void defend_then_release()
{
    auto state = fixture();
    const auto own = std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 2; });
    ShipDesign guard{state.nextShipDesignId++, 2, "Defense fixture", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::Laser, ShipComponentType::Laser}};
    normalize_ship_design_placement(guard); assert(ship_design_valid(guard));
    state.shipDesigns.push_back(guard);
    auto& fleet = state.fleets.back();
    fleet.design = guard.id; fleet.ships = {{guard.id, 1}}; fleet.telemetry.ships = fleet.ships;
    fleet.fuel = fleet_fuel_capacity(state, fleet); fleet.telemetry.fuel = fleet.fuel;
    const auto fleetId = fleet.id;
    const auto ownStar = own->star;
    state.players[1].enemyContacts.push_back({900, 0, {80, 0}, 30, ContactQuality::Probable,
        ContactSensor::Passive, 1, 1, 1, 0, own->id, 0, 10, 80, 0});
    const auto plan = plan_computer_turn(make_player_view(state, 2));
    assert(std::any_of(plan.nextState.fleetGoals.begin(), plan.nextState.fleetGoals.end(), [&](const auto& g) {
        return g.fleet == fleetId && g.kind == ComputerFleetGoalKind::Defend && g.target == ownStar;
    }));
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const auto& order) {
        const auto* move = std::get_if<MoveFleetOrder>(&order);
        return move && move->fleet == fleetId && same_position(move->destination, find_star(state, ownStar)->position);
    }));
    state = resolve_controlled_campaign_turn(state, {{1, {}}}).state;
    const auto holding = plan_computer_turn(make_player_view(state, 2));
    assert(std::any_of(holding.nextState.fleetGoals.begin(), holding.nextState.fleetGoals.end(), [&](const auto& g) {
        return g.fleet == fleetId && g.kind == ComputerFleetGoalKind::Defend;
    }));
    assert(std::none_of(holding.orders.orders.begin(), holding.orders.orders.end(), [&](const auto& order) {
        const auto* move = std::get_if<MoveFleetOrder>(&order); return move && move->fleet == fleetId;
    }));
    state.players[1].enemyContacts.clear();
    const auto released = plan_computer_turn(make_player_view(state, 2));
    assert(std::none_of(released.nextState.fleetGoals.begin(), released.nextState.fleetGoals.end(), [&](const auto& g) {
        return g.fleet == fleetId && g.kind == ComputerFleetGoalKind::Defend;
    }));
}

void withhold_unsafe_or_unverified_attack()
{
    auto state = fixture();
    std::vector<ShipDesign> designs{
        {state.nextShipDesignId++, 2, "Guard", ShipHullType::Destroyer, {ShipComponentType::QuickJump5, ShipComponentType::Laser, ShipComponentType::Laser}},
        {state.nextShipDesignId++, 2, "Bomb", ShipHullType::MiniBomber, {ShipComponentType::QuickJump5, ShipComponentType::LadyFingerBomb}},
        {state.nextShipDesignId++, 2, "Assault", ShipHullType::MediumFreighter, {ShipComponentType::QuickJump5}},
    };
    for (auto& d : designs) { normalize_ship_design_placement(d); assert(ship_design_valid(d)); state.shipDesigns.push_back(d); }
    auto& group = state.fleets.back();
    group.design = designs[0].id;
    group.ships = {{designs[0].id, 2}, {designs[1].id, 1}, {designs[2].id, 2}};
    group.colonists = 1'000; group.telemetry.colonists = group.colonists;
    group.fuel = fleet_fuel_capacity(state, group); group.telemetry.fuel = group.fuel; group.telemetry.ships = group.ships;
    auto view = make_player_view(state, 2);
    view.state.players.front().enemyContacts.push_back({900, 1, {120, 0}, 0, ContactQuality::Confirmed,
        ContactSensor::Active, 1, 1, 1, 0, 0, 0, 5000, 9000, 0});
    const auto cautious = plan_computer_turn(view);
    assert(std::none_of(cautious.nextState.fleetGoals.begin(), cautious.nextState.fleetGoals.end(),
        [](const auto& g) { return g.kind == ComputerFleetGoalKind::Raid; }));
    for (auto& player : state.players) if (player.id == 2) { player.surveyKnowledge.clear(); player.surveyedStars.clear(); }
    const auto unknown = plan_computer_turn(make_player_view(state, 2));
    assert(std::none_of(unknown.nextState.fleetGoals.begin(), unknown.nextState.fleetGoals.end(),
        [](const auto& g) { return g.kind == ComputerFleetGoalKind::Raid; }));
}
} // namespace
int main()
{
    fair_intel_and_legal_orders();
    uncertain_threats_and_delayed_missions();
    access_and_paid_work();
    defend_then_release();
    withhold_unsafe_or_unverified_attack();
    assault_through_shared_rules();
}
