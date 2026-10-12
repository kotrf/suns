#include "suns/computer_player.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

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
        else {
            const auto& research = std::get<SetResearchPlanOrder>(a.orders.orders[i]);
            const auto& other = std::get<SetResearchPlanOrder>(b.orders.orders[i]);
            assert(research.focus == other.focus && research.queuedFocuses == other.queuedFocuses
                && research.active == other.active);
        }
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
    assert(plan.orders.player == 2 && plan.orders.orders.size() == 1);
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
    assert(continuation.orders.orders.empty()); // Explicit pass, standing programs continue.
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
    assert(plan_computer_turn(make_player_view(state, 2)).orders.orders.empty());
    state.players[1].technology.researchActive = false;
    const auto resume = plan_computer_turn(make_player_view(state, 2));
    assert(resume.orders.orders.size() == 1);
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
                assert(computer->computer.decisions.back().orderCount == (turn == 0 ? 1u : 0u));
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

} // namespace

int main()
{
    integration_and_fairness();
    retain_settings_and_bounded_replays();
}
