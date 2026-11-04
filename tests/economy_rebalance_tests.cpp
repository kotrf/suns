#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include "suns/production.hpp"
#include "suns/strategic_operations.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace suns;

static void stars_population_model()
{
    auto state=make_demo_game(); auto& p=state.planets.front();
    assert(p.population==25'000 && population_capacity(state,p,state.turn)==1'000'000);
    assert(state.players.front().race.populationGrowthRate==15);
    const auto growth=[&] { return projected_population_growth(state,p,state.turn); };
    assert(growth()==3750);
    p.population=250'000; assert(growth()==37'500); // No penalty at the boundary.
    p.population=500'000; assert(growth()==33'333);
    p.population=750'000; assert(growth()==12'500);
    p.population=1'000'000; assert(growth()==0);
    p.population=2'000'000; assert(growth()==-80'000);
    p.population=4'000'000; assert(growth()==-480'000);
    p.population=8'000'000; assert(growth()==-960'000); // 12% maximum.
    p.population=10'000; p.habitability=50;
    assert(population_capacity(state,p,state.turn)==500'000 && growth()==750);
    state.players.front().race.populationGrowthRate=20; assert(growth()==1000);
    state.players.front().race.populationGrowthRate=1; assert(growth()==50);
    p.habitability=1; assert(population_capacity(state,p,state.turn)==50'000 && growth()==1);
    p.habitability=0; assert(population_capacity(state,p,state.turn)==50'000 && growth()==0);
    p.habitability=-10; assert(growth()==-100);
    p.population=101; assert(growth()==-1);
    p.population=100; assert(growth()==0);
    p.population=7; assert(growth()==0); // No invented colonists.
    p.population=0; assert(growth()==0);
    p.population=25000; p.habitability=100;
    state.players.front().race.populationGrowthRate=15;
    p.productionQueue.clear();
    auto baseline=p; baseline.industry=baseline.mines=0;
    assert(growth()==projected_population_growth(state,baseline,state.turn));
    const auto next=TurnProcessor{}.process(state,{});
    assert(next.planets.front().population==28'750);
    assert(next.players.front().history.back().population==28'750);
    const auto view=make_player_view(state,1);
    assert(projected_population_growth(view.state,view.state.planets.front(),state.turn)==3750);
    EmpireSetup a{"Slow"},b{"Fast"}; a.populationGrowthRate=1; b.populationGrowthRate=20;
    const auto campaign=generate_campaign(GalaxyConfig{}, {a,b});
    for (const auto& colony:campaign.planets) if(colony.owner) {
        assert(colony.population==25'000);
        assert(projected_population_growth(campaign,colony,campaign.turn)==(colony.owner==1?250:5000));
    }
    a.populationGrowthRate=0; bool rejected=false;
    try { (void)generate_campaign(GalaxyConfig{}, {a}); } catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    // Extreme old counts must yield a bounded negative delta without overflow.
    p.population=std::numeric_limits<std::uint64_t>::max();
    assert(growth()<0);
}

static void research_prices_and_real_overflow()
{
    constexpr std::array<std::uint32_t,27> reference{0,50,80,130,210,340,550,890,1440,2330,
        3770,6100,9870,13850,18040,22440,27050,31870,36900,42140,47590,53250,59120,65200,71490,77990,84700};
    auto state = make_demo_game();
    auto& player = state.players.front();
    player.technology.levels = {};
    for (std::uint8_t n=0;n<=26;++n) {
        assert(research_level_cost(ResearchField::Energy,n)==reference[n]);
        assert(research_level_cost(state,1,ResearchField::Energy,n)==reference[n]);
    }
    player.technology.levels[0]=1; player.technology.levels[3]=2;
    assert(research_level_cost(state,1,ResearchField::Weapons,1)==80);
    player.race.researchCosts[5]=ResearchCost::Cheap;
    assert(research_level_cost(state,1,ResearchField::Weapons,1)==40);
    player.race.researchCosts[5]=ResearchCost::Expensive;
    assert(research_level_cost(state,1,ResearchField::Weapons,2)==192);
    state.slowResearch=true;
    assert(research_level_cost(state,1,ResearchField::Weapons,2)==384);

    state.slowResearch=false; player.race.researchCosts={}; player.technology={};
    player.technology.focus=ResearchField::Energy;
    player.technology.queuedFocuses={ResearchField::Propulsion,ResearchField::Construction};
    player.technology.progress[0]=49; player.technology.progress[1]=57;
    auto& p=state.planets.front(); p.population=4000; p.habitability=0; p.industry=0;
    p.productionQueue.clear();
    const auto result=TurnProcessor{}.process_with_events(state,{});
    const auto& t=result.state.players.front().technology;
    assert(t.levels[0]==1 && t.levels[1]==1 && t.levels[2]==0);
    assert(t.progress[0]==0 && t.progress[1]==0 && t.progress[2]==0);
    assert(t.focus==ResearchField::Construction && t.queuedFocuses.empty());

    player.technology.levels.fill(26); player.technology.progress={};
    player.technology.levels[2]=25; player.technology.focus=ResearchField::Energy;
    player.technology.queuedFocuses={ResearchField::Construction};
    player.technology.progress[2]=research_level_cost(state,1,ResearchField::Construction,26)-1;
    const auto capped=TurnProcessor{}.process(state,{});
    assert(capped.players.front().technology.levels[2]==26);
    for(auto level:capped.players.front().technology.levels) assert(level==26);
    auto legacy=state;
    legacy.players.front().technology.levels={40,0,0,0,0,0};
    legacy.players.front().technology.progress={}; legacy.players.front().technology.queuedFocuses.clear();
    const auto retained=TurnProcessor{}.process(legacy,{});
    assert(retained.players.front().technology.levels[0]==40);
    assert(retained.players.front().technology.progress[1]==4); // Continue in the lowest unfinished field.

}

static void workforce_and_paid_upgrades()
{
    auto state=make_demo_game(); state.players.front().technology.researchActive=false;
    auto& p=state.planets.front(); p.population=10'000; p.industry=20; p.mines=20;
    p.advancedFactories=6; p.advancedMines=15; p.observedConcentration=MineralCargo{100,50,10};
    const auto w=colony_workforce(state,p);
    assert(w.operatingFactories==10 && w.operatingAdvancedFactories==6);
    assert(w.operatingMines==10 && w.operatingAdvancedMines==10);
    assert(colony_output(state,p)==26); // 10 population + 4 basic + 6*2 advanced.
    const auto ore=projected_mineral_mining(state,p);
    assert(std::abs(ore.ironium-(1+10'000.0/750'000+20))<1e-9);
    assert(std::abs(ore.boranium-ore.ironium/2)<1e-9);
    auto basic=p; basic.advancedFactories=basic.advancedMines=0;
    assert(projected_population_growth(state,p,state.turn)==projected_population_growth(state,basic,state.turn));

    p.population=100'000; p.industry=3; p.mines=2; p.advancedFactories=p.advancedMines=0;
    p.productionQueue.clear(); p.minerals={100,100,100};
    assert(apply_production_batch(state,p,{p.id,ProductionKind::UpgradeFactory,2}));
    assert(industry_upgrade_available(p,ProductionKind::UpgradeFactory)==1);
    assert(!apply_production_batch(state,p,{p.id,ProductionKind::UpgradeFactory,2}));
    assert(apply_production_batch(state,p,{p.id,ProductionKind::UpgradeMine,2}));
    const auto output=colony_output(state,p);
    const auto done=resolve_colony_production(state,p,true);
    assert(output==103 && done.completions.size()==4);
    assert(p.industry==3 && p.advancedFactories==2 && p.mines==2 && p.advancedMines==2);
    assert(p.minerals.ironium==90 && p.minerals.boranium==92 && p.minerals.germanium==90);
    assert(colony_output(state,p)==105);
    assert(!production_batch_valid({p.id,ProductionKind::UpgradeFactory,1,ProductionAutomation::Annual}));

    // Basic mines bootstrap without any I/B/G. Full advanced construction and
    // basic construction + upgrade have identical final bills and output.
    p.productionQueue.clear(); p.minerals={}; p.mines=p.advancedMines=0;
    assert(apply_production_batch(state,p,{p.id,ProductionKind::Mine,10}));
    (void)resolve_colony_production(state,p,true);
    assert(p.mines==10 && p.productionQueue.empty() && p.minerals.ironium==0);
    // Repeated units must reset to their own recipe, rather than the legacy
    // default colony-ship cost. Verify the real resolver's resource debit.
    for(auto kind:{ProductionKind::AdvancedFactory,ProductionKind::AdvancedMine,ProductionKind::UpgradeFactory,ProductionKind::UpgradeMine}) {
        auto trial=state; auto& t=trial.planets.front(); t.population=100'000;
        t.industry=t.mines=3; t.advancedFactories=t.advancedMines=0; t.minerals={100,100,100};
        t.productionQueue={{kind,production_cost(kind),0,ProductionAutomation::None,3}};
        trial.players.front().technology.researchActive=true;
        const auto budget=colony_output(trial,t);
        assert(production_item_cost(trial,t.productionQueue.front())==production_cost(kind));
        const auto paid=resolve_colony_production(trial,t,true);
        assert(t.productionQueue.empty() && paid.researchPoints==budget-3*production_cost(kind));
    }
    for(auto pair:{std::pair{ProductionKind::Factory,ProductionKind::AdvancedFactory},
                   std::pair{ProductionKind::Mine,ProductionKind::AdvancedMine}}) {
        const auto upgrade=pair.first==ProductionKind::Factory?ProductionKind::UpgradeFactory:ProductionKind::UpgradeMine;
        assert(production_cost(pair.first)+production_cost(upgrade)==production_cost(pair.second));
        const auto a=production_item_mineral_cost(state,{pair.first});
        const auto b=production_item_mineral_cost(state,{upgrade});
        const auto c=production_item_mineral_cost(state,{pair.second});
        assert(a.ironium+b.ironium==c.ironium && a.boranium+b.boranium==c.boranium && a.germanium+b.germanium==c.germanium);
    }
    p.industry=2; p.advancedFactories=2;
    p.productionQueue={{ProductionKind::UpgradeFactory,10}};
    p.minerals={};
    (void)resolve_colony_production(state,p,true);
    assert(p.productionQueue.empty() && !p.productionWaitingForMinerals);
}

static void allocation_and_scaling()
{
    auto state=make_demo_game(); auto& p=state.planets.front();
    p.population=100'000; p.industry=4; p.minerals={0,0,8};
    p.productionQueue={{ProductionKind::Factory,10,0,ProductionAutomation::None,2}};
    state.players.front().technology.researchAllocationPercent=50;
    assert(colony_output(state,p)==104);
    assert(colony_research_allocation(state,p,104)==52);
    const auto resolution=resolve_colony_production(state,p,true);
    assert(p.industry==6 && p.productionQueue.empty() && p.minerals.germanium==0);
    assert(resolution.researchPoints==84); // 52 allocated + 32 unused; 20 construction.
    p.population=std::numeric_limits<std::uint64_t>::max(); p.industry=p.advancedFactories=std::numeric_limits<std::uint32_t>::max();
    assert(colony_output(state,p)==std::numeric_limits<std::uint32_t>::max());
    p.population=0; assert(colony_output(state,p)==0);
}

static void computer_reserves_colonizer_materials()
{
    auto state=make_demo_game(); auto& player=state.players.front();
    player.controller=PlayerController::Computer; player.technology.levels.fill(4);
    auto& colony=state.planets.front(); colony.industry=50; colony.mines=1000; colony.minerals={0,0,4};
    for (const auto& p:state.planets) set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto plan=plan_computer_turn(make_player_view(state,1));
    assert(plan.nextState.decisions.back().reason.find("Save minerals for")!=std::string::npos);
    for (const auto& order:plan.orders.orders) if(const auto* build=std::get_if<QueueProductionBatchOrder>(&order))
        assert(build->kind!=ProductionKind::Factory && build->kind!=ProductionKind::UpgradeFactory);
}
int main() { stars_population_model(); research_prices_and_real_overflow(); workforce_and_paid_upgrades(); allocation_and_scaling(); computer_reserves_colonizer_materials(); }
