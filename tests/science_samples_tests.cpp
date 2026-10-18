#include "suns/science_samples.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/computer_player.hpp"
#include "suns/combat.hpp"
#include "suns/player_knowledge.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace suns;
namespace {
GameState fixture()
{
    auto s = make_demo_game(); s.turn = 10; s.wormholeRules.spawnChancePerTurn = 0;
    s.players[0].technology.researchActive = false; s.players[0].technology.levels.fill(10);
    s.players[0].technology.progress.fill(0);
    s.shipDesigns[0].hull = ShipHullType::SmallFreighter;
    s.shipDesigns[0].components = {ShipComponentType::FuelMizer, ShipComponentType::MoleScanner};
    s.shipDesigns[0].placements.clear();
    s.fleets.resize(1); auto& f = s.fleets[0]; f.ships = {{f.design, 1}};
    f.destination.reset(); f.task = FleetTask::None; f.colonists = 0; f.minerals = {};
    f.telemetry = {}; f.telemetryInTransit.clear(); s.nextFleetId = f.id + 1;
    s.unknownObjects = {{7, f.position, UnknownObjectKind::AncientProbe}}; s.nextUnknownObjectId = 8;
    assert(fleet_cargo_capacity(s, f) >= 10);
    return s;
}
void studied(GameState& s)
{
    const auto& x = s.unknownObjects[0];
    s.players[0].objectKnowledge = {{x.id, x.position, 0, ObjectKnowledgeLevel::Studied, x.kind, s.turn, s.turn}};
}
GameState next(const GameState& s, std::vector<Order> orders = {})
{ return TurnProcessor{}.process(s, {{1, std::move(orders)}}); }
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
void study_recover_return_and_consume_once()
{
    auto s = fixture();
    s = next(s, {SetFleetTaskOrder{1, FleetTask::RecoverSample}});
    assert(s.fleets[0].scienceSamples.empty()); // Received observations are a real prerequisite.
    s = next(s, {SetFleetTaskOrder{1, FleetTask::FieldResearch}});
    assert(sample_observation_received(s, 1, 7));
    s = next(s, {SetFleetTaskOrder{1, FleetTask::RecoverSample}});
    assert(s.unknownObjects[0].sampleRecovered && s.fleets[0].scienceSamples.size() == 1);
    const auto loadedMass = fleet_gross_mass(s, s.fleets[0]);
    auto empty = s.fleets[0]; empty.scienceSamples.clear();
    assert(near(loadedMass - fleet_gross_mass(s, empty), 5));
    s = next(s, {SetFleetTaskOrder{1, FleetTask::RecoverSample}});
    assert(s.fleets[0].scienceSamples.size() == 1);
    MoveFleetOrder unload{1, s.fleets[0].position, 1, {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::All}};
    s = next(s, {unload});
    assert(s.fleets[0].scienceSamples.empty() && s.planets[0].scienceSamples.size() == 1);
    const auto index = std::size_t(ResearchField::Electronics);
    const auto before = s.players[0].technology.progress[index];
    s = next(s, {ProcessScienceSampleOrder{1, 7, false}, ProcessScienceSampleOrder{1, 7, false}});
    assert(s.planets[0].scienceSamples.empty());
    assert(s.players[0].technology.progress[index] == before + 64);
    s = next(s, {SetFleetTaskOrder{1, FleetTask::RecoverSample}});
    assert(s.fleets[0].scienceSamples.empty()); // Consumption does not regenerate physical stock.
}
void hold_capacity_atomic_transfers_and_ordered_previews()
{
    auto s = fixture(); studied(s); auto& f = s.fleets[0]; f.task = FleetTask::RecoverSample;
    f.minerals.ironium = fleet_cargo_capacity(s, f) - 4;
    const auto blocked = TurnProcessor{}.process_with_events(s, {}); s = blocked.state;
    assert(!s.unknownObjects[0].sampleRecovered && s.fleets[0].task == FleetTask::None);
    assert(std::any_of(blocked.events.begin(), blocked.events.end(), [](const auto& e) { return e.kind == GameEventKind::SampleHoldFull; }));
    s.fleets[0].minerals = {};
    s = next(s, {SetFleetTaskOrder{1, FleetTask::RecoverSample}}); assert(s.fleets[0].scienceSamples.size() == 1);
    assert(!transfer_science_sample(s, 2, {{0, 1}, {1, 0}, 7}));
    assert(!transfer_science_sample(s, 1, {{0, 1}, {1, 1}, 7}));
    assert(transfer_science_sample(s, 1, {{0, 1}, {1, 0}, 7}));
    assert(!transfer_science_sample(s, 1, {{0, 1}, {1, 0}, 7}));
    s.fleets[0].minerals.ironium = fleet_cargo_capacity(s, s.fleets[0]) - 4;
    assert(!transfer_science_sample(s, 1, {{1, 0}, {0, 1}, 7}));
    s.fleets[0].minerals = {};
    PlayerOrders orders{1, {ProcessScienceSampleOrder{1, 7, false}, TransferScienceSampleOrder{{1, 0}, {0, 1}, 7}}};
    const auto preview = projected_cargo_order_state(s, orders);
    const auto actual = next(s, orders.orders);
    assert(preview.fleets[0].scienceSamples.empty() && preview.planets[0].scienceSamples.empty());
    assert(actual.fleets[0].scienceSamples.empty() && actual.planets[0].scienceSamples.empty());
    assert(s.planets[0].scienceSamples.size() == 1); // Preview does not mutate truth or spend a year.
    auto far = s; far.fleets[0].position = {900, 900};
    assert(!transfer_science_sample(far, 1, {{1, 0}, {0, 1}, 7}));
}
void retained_artifact_requires_observation_and_correct_research()
{
    auto s = fixture(); s.unknownObjects[0].sampleRecovered = true;
    s.planets[0].scienceSamples = {{7, UnknownObjectKind::AncientProbe, false}};
    assert(!process_science_sample(s, 1, {1, 7, true}));
    studied(s); assert(process_science_sample(s, 1, {1, 7, true}));
    assert(!process_science_sample(s, 1, {1, 7, true}));
    auto& t = s.players[0].technology; t.researchActive = true; t.researchAllocationPercent = 50;
    t.focus = ResearchField::Electronics;
    assert(retained_sample_research(s, s.planets[0]) == 4);
    auto control = s; control.planets[0].scienceSamples[0].retained = false;
    const auto boosted = next(s), baseline = next(control);
    const auto field = std::size_t(t.focus);
    assert(boosted.players[0].technology.progress[field] == baseline.players[0].technology.progress[field] + 4);
    t.focus = ResearchField::Energy; assert(retained_sample_research(s, s.planets[0]) == 0);
    t.focus = ResearchField::Electronics; t.researchAllocationPercent = 0;
    assert(retained_sample_research(s, s.planets[0]) == 0);
    s.players.push_back({2, "Captor"}); s.planets[0].owner = 2;
    assert(!process_science_sample(s, 1, {1, 7, false}));
    assert(retained_sample_research(s, s.planets[0]) == 0);
    assert(process_science_sample(s, 2, {1, 7, false})); // A new owner may choose destructive analysis.
}
void split_and_merge_conserve_whole_specimens_and_cargo()
{
    auto s = fixture(); auto& f = s.fleets[0]; f.ships = {{f.design, 2}};
    const auto oneHold = fleet_cargo_capacity(s, f) / 2;
    const auto count = std::size_t(std::floor(oneHold / 5)) + 1;
    assert(count <= kMaximumScienceSamples);
    s.unknownObjects.clear();
    for (std::size_t i = 0; i < count; ++i) {
        const auto id = UnknownObjectId(i + 1); f.scienceSamples.push_back({id, UnknownObjectKind::AncientProbe, false});
        s.unknownObjects.push_back({id, f.position, UnknownObjectKind::AncientProbe, true});
    }
    s.nextUnknownObjectId = count + 1;
    f.minerals.ironium = fleet_cargo_capacity(s, f) - science_sample_mass(f.scienceSamples) - 1;
    const auto mass = fleet_cargo_used(s, f);
    const auto split = next(s, {SplitFleetOrder{1, {{f.design, 1}}}});
    assert(split.fleets.size() == 2 && !split.fleets[0].scienceSamples.empty() && !split.fleets[1].scienceSamples.empty());
    assert(split.fleets[0].scienceSamples.size() + split.fleets[1].scienceSamples.size() == count);
    assert(near(fleet_cargo_used(split, split.fleets[0]) + fleet_cargo_used(split, split.fleets[1]), mass));
    for (const auto& part : split.fleets) assert(fleet_cargo_used(split, part) <= fleet_cargo_capacity(split, part) + 1e-6);
    const auto merged = next(split, {MergeFleetsOrder{1, 2}});
    assert(merged.fleets.size() == 1 && merged.fleets[0].scienceSamples.size() == count);
    assert(near(fleet_cargo_used(merged, merged.fleets[0]), mass));
    // Indivisible population rounding must not overfill an exactly full hold.
    s = fixture(); s.fleets[0].ships = {{s.fleets[0].design, 2}};
    s.unknownObjects[0].sampleRecovered = true;
    s.fleets[0].scienceSamples = {{7, UnknownObjectKind::AncientProbe, false}};
    const auto ordinary = fleet_cargo_capacity(s, s.fleets[0]) - kScienceSampleMass;
    s.fleets[0].minerals = {1, 0, 0};
    s.fleets[0].colonists = std::uint64_t(std::round((ordinary - 1) * kColonistsPerCargoUnit));
    const auto fullMass = fleet_cargo_used(s, s.fleets[0]);
    const auto fullSplit = next(s, {SplitFleetOrder{1, {{s.fleets[0].design, 1}}}});
    assert(fullSplit.fleets.size() == 2);
    for (const auto& part : fullSplit.fleets) assert(fleet_cargo_used(fullSplit, part) <= fleet_cargo_capacity(fullSplit, part) + 1e-6);
    assert(near(fleet_cargo_used(fullSplit, fullSplit.fleets[0]) + fleet_cargo_used(fullSplit, fullSplit.fleets[1]), fullMass));
}
void hidden_recovery_waits_for_immutable_telemetry()
{
    auto s = fixture(); studied(s); auto& f = s.fleets[0]; f.position = {900, 0};
    s.unknownObjects[0].position = f.position; f.telemetry.position = f.position;
    f.telemetry.observedTurn = s.turn; f.telemetry.ships = f.ships;
    f.task = FleetTask::RecoverSample; recover_science_samples(s, f, s.turn);
    assert(f.scienceSamples.size() == 1);
    assert(fleet_player_view(s, f).scienceSamples.empty());
    const auto before = make_player_view(s, 1).state;
    assert(before.unknownObjects.empty() && before.fleets[0].scienceSamples.empty());
    publish_fleet_telemetry(s, s.turn);
    assert(!f.telemetryInTransit.empty());
    const auto due = f.telemetryInTransit.back().deliveryTurn;
    assert(due > s.turn);
    f.scienceSamples.clear(); // Losing the cargo cannot rewrite a dispatched manifest.
    s.turn = due; deliver_due_fleet_telemetry(s);
    assert(fleet_player_view(s, f).scienceSamples.size() == 1);
    f.task = FleetTask::RecoverSample;
    recover_science_samples(s, f, s.turn); assert(f.scienceSamples.empty());
}
void merging_remote_fleets_does_not_reveal_hidden_cargo()
{
    auto s = fixture(); auto& f = s.fleets[0]; f.position = {900, 0};
    f.scienceSamples = {{7, UnknownObjectKind::AncientProbe, false}};
    s.unknownObjects[0].sampleRecovered = true;
    f.telemetry.position = f.position; f.telemetry.observedTurn = s.turn;
    f.telemetry.ships = f.ships; f.telemetry.fuel = f.fuel;
    auto other = f; other.id = 2; other.scienceSamples.clear(); s.fleets.push_back(other); s.nextFleetId = 3;
    const auto merged = next(s, {MergeFleetsOrder{2, 1}});
    assert(merged.fleets.size() == 1 && merged.fleets[0].scienceSamples.size() == 1);
    assert(make_player_view(merged, 1).state.fleets[0].scienceSamples.empty());
}
void computer_uses_received_study_and_returns_physical_cargo()
{
    auto s = fixture(); studied(s); auto& f = s.fleets[0]; f.task = FleetTask::FieldResearch;
    f.telemetry.task = f.task; f.telemetry.observedTurn = s.turn; f.telemetry.position = f.position;
    f.telemetry.ships = f.ships; f.telemetry.fuel = f.fuel;
    s.players[0].controller = PlayerController::Computer;
    s.players[0].computer.initialized = true;
    s.players[0].computer.decisions = {{s.turn - 1, 0, "Prior expedition"}};
    s.players[0].computer.fleetGoals = {{1, ComputerFleetGoalKind::InvestigateObject, 7, s.turn - 1, s.turn}};
    auto plan = plan_computer_turn(make_player_view(s, 1));
    const auto recovery = std::find_if(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& order) {
        const auto* move = std::get_if<MoveFleetOrder>(&order);
        return move && move->arrivalAction.kind == FleetArrivalActionKind::RecoverSample;
    });
    assert(recovery != plan.orders.orders.end());
    auto hidden = s; hidden.unknownObjects[0].sampleRecovered = true;
    const auto same = plan_computer_turn(make_player_view(hidden, 1));
    assert(same.nextState == plan.nextState); // Hidden stock does not influence the decision.
    s.players[0].computer = plan.nextState; s = TurnProcessor{}.process(s, {plan.orders});
    assert(s.fleets[0].scienceSamples.size() == 1);
    plan = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& order) {
        const auto* move = std::get_if<MoveFleetOrder>(&order);
        return move && move->arrivalAction.kind == FleetArrivalActionKind::UnloadAll && move->arrivalAction.cargo == FleetCargoKind::All;
    }));
    s.players[0].computer = plan.nextState; s = TurnProcessor{}.process(s, {plan.orders});
    assert(s.fleets[0].scienceSamples.empty() && s.planets[0].scienceSamples.size() == 1);
    plan = plan_computer_turn(make_player_view(s, 1));
    assert(std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [](const auto& order) {
        const auto* process = std::get_if<ProcessScienceSampleOrder>(&order);
        return process && process->sample == 7 && !process->retain;
    }));
}
void colonization_and_conquest_preserve_stored_specimens()
{
    auto s = fixture(); s.unknownObjects[0].sampleRecovered = true;
    s.shipDesigns[1].components.push_back(ShipComponentType::CargoPod); s.shipDesigns[1].placements.clear();
    auto& f = s.fleets[0]; f.design = kColonyShipDesignId; f.ships = {{kColonyShipDesignId, 1}};
    f.position = s.stars[1].position; f.colonists = 1000;
    f.scienceSamples = {{7, UnknownObjectKind::AncientProbe, false}};
    assert(fleet_cargo_used(s, f) <= fleet_cargo_capacity(s, f));
    auto founded = next(s, {ColonizePlanetOrder{1, s.planets[1].id}});
    assert(founded.fleets.empty() && founded.planets[1].owner == 1);
    assert(founded.planets[1].scienceSamples == f.scienceSamples);
    s.players.push_back({2, "Defender"}); s.planets[1].owner = 2; s.planets[1].population = 1;
    s.planets[1].scienceSamples = {{7, UnknownObjectKind::AncientProbe, true}};
    s.fleets[0].scienceSamples.clear();
    const auto captured = next(s, {TransferCargoOrder{{0, 1}, {s.planets[1].id, 0}, 1000, {}}});
    assert(captured.planets[1].owner == 1 && captured.planets[1].scienceSamples.size() == 1);
    assert(!captured.planets[1].scienceSamples[0].retained);
}
void combat_capacity_loss_keeps_whole_samples_without_overflow()
{
    GameState s; s.turn = 10; s.galaxySeed = 1234; s.wormholeRules.spawnChancePerTurn = 0;
    s.players = {{1, "Attacker"}, {2, "Carrier"}};
    s.shipDesigns = {{10, 1, "Gunboat", ShipHullType::Destroyer, {ShipComponentType::QuickJump5, ShipComponentType::Laser}},
        {20, 2, "Carrier", ShipHullType::SmallFreighter, {ShipComponentType::QuickJump5, ShipComponentType::Tritanium}}};
    Fleet attacker; attacker.id = 1; attacker.owner = 1; attacker.design = 10; attacker.ships = {{10, 1}};
    Fleet carrier; carrier.id = 2; carrier.owner = 2; carrier.design = 20; carrier.ships = {{20, 1}};
    const auto perHold = fleet_cargo_capacity(s, carrier);
    const auto initialShips = std::uint32_t(std::ceil((64 * kScienceSampleMass + 1) / perHold));
    carrier.ships = {{20, initialShips}}; carrier.fuel = fleet_fuel_capacity(s, carrier);
    for (UnknownObjectId id = 1; id <= 64; ++id) {
        carrier.scienceSamples.push_back({id, UnknownObjectKind::AncientProbe, false});
        s.unknownObjects.push_back({id, {}, UnknownObjectKind::AncientProbe, true});
    }
    carrier.minerals.ironium = fleet_cargo_capacity(s, carrier) - science_sample_mass(carrier.scienceSamples) - 1;
    assert(carrier.minerals.ironium >= 0);
    s.nextUnknownObjectId = 65; s.fleets = {attacker, carrier};
    resolve_space_battles(s, 11);
    const auto survived = std::find_if(s.fleets.begin(), s.fleets.end(), [](const auto& f) { return f.id == 2; });
    assert(survived != s.fleets.end() && fleet_ship_count(*survived) < initialShips && fleet_ship_count(*survived) > 0);
    assert(survived->scienceSamples.size() < 64);
    assert(fleet_cargo_used(s, *survived) <= fleet_cargo_capacity(s, *survived) + 1e-6);
    for (const auto& object : s.unknownObjects) assert(object.sampleRecovered);
}
}
int main()
{
    study_recover_return_and_consume_once();
    hold_capacity_atomic_transfers_and_ordered_previews();
    retained_artifact_requires_observation_and_correct_research();
    split_and_merge_conserve_whole_specimens_and_cargo();
    hidden_recovery_waits_for_immutable_telemetry();
    merging_remote_fleets_does_not_reveal_hidden_cargo();
    computer_uses_received_study_and_returns_physical_cargo();
    colonization_and_conquest_preserve_stored_specimens();
    combat_capacity_loss_keeps_whole_samples_without_overflow();
    std::cout << "Science samples: physical recovery, orders, research, conservation and delayed knowledge passed\n";
}
