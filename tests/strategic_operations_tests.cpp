#include "suns/strategic_operations.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

using namespace suns;
using C = ShipComponentType;

GameState arena(std::vector<C> fit = {C::RhinoScanner}, ShipHullType hull = ShipHullType::Destroyer)
{
    GameState s; s.players = {{1,"One"},{2,"Two"},{3,"Observer"}};
    s.wormholeRules.spawnChancePerTurn = 0;
    fit.insert(fit.begin(),C::FuelMizer);
    s.shipDesigns = {{10,1,"Test",hull,fit}}; s.nextShipDesignId = 11;
    Fleet f; f.id = 1; f.owner = 1; f.design = 10; f.name = "Test"; f.fuel = 1000;
    f.ships = {{10,1}}; f.telemetry.observedTurn = 1; f.telemetry.ships = f.ships;
    s.fleets = {f}; s.nextFleetId = 2;
    return s;
}
const GameEvent* event(const TurnResult& r, GameEventKind kind, PlayerId owner = 1)
{
    auto e = std::find_if(r.events.begin(),r.events.end(),[&](const auto& e) { return e.kind == kind && e.recipient == owner; });
    return e == r.events.end() ? nullptr : &*e;
}

void bombardment_orders_and_targets()
{
    auto s = arena({C::LadyFingerBomb});
    s.stars = {{1,"Target",{0,0}}};
    Planet p; p.id = 1; p.star = 1; p.owner = 2; p.population = 100000; p.industry = 20; p.mines = 20;
    s.planets = {p};
    assert(!event(TurnProcessor{}.process_with_events(s,{}),GameEventKind::Bombardment));
    auto r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::Bombardment}}}});
    assert(event(r,GameEventKind::Bombardment) && event(r,GameEventKind::Bombardment,2));
    assert(!event(r,GameEventKind::Bombardment,3));
    assert(event(r,GameEventKind::Bombardment)->deliveredColonists == 600);
    assert(event(r,GameEventKind::Bombardment)->quantity == 2);
    s.planets[0].owner = 1;
    r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::Bombardment}}}});
    assert(!event(r,GameEventKind::Bombardment));
    s.planets[0].owner = 2; s.fleets[0].position = {1,0};
    r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::Bombardment}}}});
    assert(!event(r,GameEventKind::Bombardment));
    s.fleets[0].position = {0,0}; s.planets[0].population = 100;
    r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::Bombardment}}}});
    assert(r.state.planets[0].owner == 0 && r.state.planets[0].population == 0);
    assert(event(r,GameEventKind::ColonyLost,2));
    s = arena({C::SmartBomb}); s.stars = {{1,"Target",{0,0}}}; s.planets = {p};
    s.fleets[0].task = FleetTask::Bombardment;
    resolve_bombardments(s,2);
    assert(s.planets[0].population < p.population && s.planets[0].industry == p.industry && s.planets[0].mines == p.mines);
}

void mines_crossings_laying_sweeping()
{
    auto s = arena({},ShipHullType::Cruiser);
    s.minefields = {{1,2,{30,0},100,0}}; s.nextMinefieldId = 2;
    auto& f = s.fleets[0]; f.warp = 6;
    Position end{100,0};
    assert(apply_minefield_crossing(s,f,{0,0},end));
    assert(std::abs(end.x-20)<1e-6 && f.damagePercent > 0 && s.minefields[0].mines == 50);
    s = arena(); s.fleets[0].warp = 4; s.minefields = {{1,2,{30,0},100,0}};
    end = {100,0}; assert(!apply_minefield_crossing(s,s.fleets[0],{0,0},end));
    s.fleets[0].warp = 10; s.minefields[0].owner = 1;
    assert(!apply_minefield_crossing(s,s.fleets[0],{0,0},end));
    s.minefields[0].owner = 2; s.minefields[0].kind = 2;
    assert(apply_minefield_crossing(s,s.fleets[0],{0,0},end) && s.fleets[0].damagePercent == 0);
    s = arena({C::MineDispenser50});
    auto r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::LayMines}}}});
    assert(r.state.minefields.size() == 1 && r.state.minefields[0].mines == 50);
    r = TurnProcessor{}.process_with_events(r.state,{});
    assert(r.state.minefields.size() == 1 && r.state.minefields[0].mines == 97.5);
    s = arena({C::Laser}); s.minefields = {{1,2,{0,0},50,0}}; s.nextMinefieldId = 2;
    r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::SweepMines}}}});
    assert(r.state.minefields[0].mines < 47.5);
    s = arena({},ShipHullType::Cruiser); s.minefields = {{1,2,{30,0},100,0}}; s.nextMinefieldId = 2;
    r = TurnProcessor{}.process_with_events(s,{{1,{MoveFleetOrder{1,{100,0},6}}}});
    assert(event(r,GameEventKind::MineStrike));
    assert(!r.state.fleets[0].destination && r.state.fleets[0].position.x < 30);
    // All fleets use the same boundary even after an earlier strike shrinks
    // the field. Physical vector order cannot decide who hits it.
    auto second = s.fleets[0]; second.id = 2;
    s.fleets.push_back(second); s.nextFleetId = 3;
    const std::vector<PlayerOrders> orders{{1,{MoveFleetOrder{1,{100,0},6},MoveFleetOrder{2,{100,0},6}}}};
    const auto forward = TurnProcessor{}.process_with_events(s,orders);
    std::reverse(s.fleets.begin(),s.fleets.end());
    const auto reverse = TurnProcessor{}.process_with_events(s,orders);
    for (FleetId id : {1,2}) {
        const auto find = [id](const auto& result) -> const Fleet& {
            return *std::find_if(result.state.fleets.begin(),result.state.fleets.end(),
                [id](const auto& f) { return f.id == id; });
        };
        assert(event(forward,GameEventKind::MineStrike));
        assert(!find(forward).destination && !find(reverse).destination);
        assert(same_position(find(forward).position,find(reverse).position));
        assert(find(forward).damagePercent == find(reverse).damagePercent);
    }
    assert(forward.state.minefields.empty() && reverse.state.minefields.empty());
    s = arena(); s.minefields = {{1,2,{30,0},100,0}}; s.nextMinefieldId = 2;
    r = TurnProcessor{}.process_with_events(s,{{1,{MoveFleetOrder{1,{100,0},6}}}});
    assert(event(r,GameEventKind::MineStrike) && r.state.fleets.empty()); // Destroyer armor is exhausted.
    s.fleets[0].electronics = {EmissionMode::RadioSilence,5};
    s.fleets[0].destination = Position{100,0}; s.fleets[0].warp = 6; // Route already aboard.
    r = TurnProcessor{}.process_with_events(s,{});
    assert(r.state.fleets.empty() && !event(r,GameEventKind::MineStrike));
    assert(r.state.pendingFleetLossContacts.size() == 1 && r.state.pendingFleetLossContacts[0].deliveryTurn == 5);
    assert(make_player_view(r.state,1).state.fleets.size() == 1);
    // Unseen fields do not leak through a player export.
    const auto view = make_player_view(s,1);
    assert(view.state.minefields.empty() && view.state.players[0].strategicIntel.empty());
}

void stealth_jamming_and_signals()
{
    auto s = arena({C::RhinoScanner});
    s.shipDesigns.push_back({20,2,"Hidden",ShipHullType::Destroyer,{C::FuelMizer,C::UltraStealthCloak}});
    Fleet enemy; enemy.id = 2; enemy.owner = 2; enemy.design = 20; enemy.position = {40,0}; enemy.ships = {{20,1}};
    s.fleets.push_back(enemy); s.nextFleetId = 3;
    assert(!detector_detects_fleet(s,s.fleets[0],s.fleets[1]));
    const auto hidden = make_player_view(s,1);
    assert(hidden.state.fleets.size() == 1);
    observe_strategic_objects(s,2); s.turn = 2; deliver_strategic_intel(s);
    assert(s.players[0].strategicIntel.size() == 1);
    const auto& signal = s.players[0].strategicIntel[0];
    assert(signal.type == StrategicObjectKind::Emission && signal.owner == 0 && signal.radius == 15);
    assert(signal.position.x != enemy.position.x);
    s.turn = 3; deliver_strategic_intel(s); assert(s.players[0].strategicIntel.empty());
    s.shipDesigns[1].components = {C::FuelMizer,C::RhinoScanner,C::Jammer50};
    assert(strategic_sensor_range(s,s.fleets[0]) == 25);
    assert(communication_delay_turns(s,1,{0,0}) == 1);
    s.fleets[1].electronics.mode = EmissionMode::RadioSilence;
    assert(strategic_sensor_range(s,s.fleets[0]) == 50);
    s.fleets[0].electronics.mode = EmissionMode::Passive;
    assert(strategic_sensor_range(s,s.fleets[0]) == 0);
    s.fleets[0].electronics = {}; s.fleets[1].electronics = {EmissionMode::Standard,0,true};
    s.players[0].strategicIntel.clear(); s.players[0].pendingStrategicIntel.clear();
    observe_strategic_objects(s,4); s.turn = 4; deliver_strategic_intel(s);
    assert(s.players[0].strategicIntel.empty()); // Jammer delays the receiver's report.
    s.turn = 5; deliver_strategic_intel(s);
    assert(std::any_of(s.players[0].strategicIntel.begin(),s.players[0].strategicIntel.end(),[](const auto& i) {
        return i.type == StrategicObjectKind::Emission && i.position.x == 90;
    }));
    assert(!submit_electronics_command(s,1,1,{EmissionMode::Standard,0,true})); // no jammer aboard
}

void silence_resumes_and_orders_wait()
{
    auto s = arena();
    auto neighbor = s.fleets[0]; neighbor.id = 2; neighbor.electronics = {EmissionMode::RadioSilence,8};
    s.fleets.push_back(neighbor); s.nextFleetId = 3;
    assert(report_transmission_turn(s,1,{0,0},2) == 2);
    std::reverse(s.fleets.begin(),s.fleets.end());
    assert(report_transmission_turn(s,1,{0,0},2) == 2);
    assert(report_transmission_turn(s,1,{0,0},2,2) == 8);
    s = arena();
    assert(!submit_electronics_command(s,2,1,{EmissionMode::RadioSilence,4}));
    assert(!submit_electronics_command(s,1,1,{EmissionMode::RadioSilence,1}));
    assert(submit_electronics_command(s,1,1,{EmissionMode::RadioSilence,4}));
    assert(!fleet_has_instant_link(s,s.fleets[0]));
    assert(submit_fleet_route_command(s,1,1,{100,0},4,{},{}));
    assert(s.fleets[0].pendingCommands[0].deliveryTurn == 4);
    s = TurnProcessor{}.process(s,{}); assert(!s.fleets[0].destination);
    s = TurnProcessor{}.process(s,{}); assert(!s.fleets[0].destination);
    s = TurnProcessor{}.process(s,{}); assert(s.fleets[0].destination && s.fleets[0].electronics.mode == EmissionMode::Standard);
}

void field_science_is_finite_delayed_and_real()
{
    auto s = arena(); s.stars = {{1,"Target",{0,0}}};
    auto r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::FieldResearch}}}});
    assert(event(r,GameEventKind::ScientificData));
    assert(r.state.players[0].technology.progress[0] == 12 && r.state.players[0].fieldScience.size() == 1);
    auto repeated = TurnProcessor{}.process_with_events(r.state,{});
    assert(!event(repeated,GameEventKind::ScientificData));
    s.players[0].technology.researchActive = false;
    Planet home; home.id = 1; home.star = 2; home.owner = 1; home.population = 1000;
    s.planets = {home}; s.stars.push_back({2,"Home",{600,0}});
    r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::FieldResearch}}}});
    // Command itself must travel out first.
    assert(r.state.players[0].technology.progress[0] == 0);
    while (r.state.turn < 6) r = TurnProcessor{}.process_with_events(r.state,{});
    assert(r.state.players[0].technology.progress[0] == 0 && !r.state.players[0].fieldScience.empty());
    assert(make_player_view(r.state,1).state.players[0].fieldScience.empty());
    while (r.state.turn < 10) r = TurnProcessor{}.process_with_events(r.state,{});
    assert(r.state.players[0].technology.progress[0] == 12);
    s = arena({C::RhinoScanner},ShipHullType::MediumTransport);
    s.wrecks = {{1,{0,0},20,{0,0,0,0,0,5}}}; s.nextWreckId = 2;
    r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::Salvage}}}});
    assert(r.state.wrecks.empty() && event(r,GameEventKind::ScientificData));
    assert(r.state.players[0].technology.levels[5] > 0);
    repeated = TurnProcessor{}.process_with_events(r.state,{});
    assert(!event(repeated,GameEventKind::ScientificData));
    s.wrecks[0].technology = {};
    r = TurnProcessor{}.process_with_events(s,{{1,{SetFleetTaskOrder{1,FleetTask::Salvage}}}});
    assert(r.state.wrecks.empty() && !event(r,GameEventKind::ScientificData));
}

int main()
{
    bombardment_orders_and_targets(); mines_crossings_laying_sweeping(); stealth_jamming_and_signals();
    silence_resumes_and_orders_wait(); field_science_is_finite_delayed_and_real();
}
