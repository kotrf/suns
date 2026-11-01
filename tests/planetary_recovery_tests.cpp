#include "suns/production.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;
using K=ProductionKind;
GameState fixture()
{
    GameState s; s.players={{1,"One"},{2,"Two"}}; s.turn=1; s.wormholeRules.spawnChancePerTurn=0;
    for(auto& p:s.players) p.technology.researchActive=false;
    s.stars={{1,"World",{0,0}}};
    Planet p; p.id=1; p.star=1; p.owner=2; p.name="World"; p.population=500000; p.industry=0;
    p.habitability=100; p.minerals={100,100,100}; p.homeworld=true; p.radar=PlanetaryRadarLevel::Full;
    s.planets={p}; return s;
}
void bombardment_records_only_real_installation_losses()
{
    auto s=fixture(); auto& p=s.planets.front(); p.industry=10; p.mines=10; p.radar=PlanetaryRadarLevel::None;
    p.naturalEnvironment=PlanetEnvironment{50,50,15};
    s.shipDesigns={{1,1,"Bomber",ShipHullType::Destroyer,{ShipComponentType::FuelMizer,ShipComponentType::LadyFingerBomb}}};
    Fleet f; f.id=1; f.owner=1; f.design=1; f.ships={{1,3}}; f.task=FleetTask::Bombardment; s.fleets={f};
    resolve_bombardments(s,2);
    assert(p.warDamage.factoriesDestroyed==10-p.industry && p.warDamage.minesDestroyed==10-p.mines);
    assert(p.warDamage.factoriesDestroyed && p.warDamage.minesDestroyed && p.warDamage.lastBombardmentTurn==2);
    assert(p.environment.radiation==15 && p.naturalEnvironment->radiation==15);
    const auto destroyed=p.warDamage.factoriesDestroyed+p.warDamage.minesDestroyed;
    resolve_bombardments(s,3);
    assert(p.warDamage.factoriesDestroyed+p.warDamage.minesDestroyed>=destroyed);
    assert(p.industry+p.warDamage.factoriesDestroyed==10 && p.mines+p.warDamage.minesDestroyed==10);
    assert(colony_output(s, p)<11); // Physical destroyed factories already remove their output.
    const auto hostile=make_player_view(s,1).state;
    assert(hostile.planets.empty());
    set_survey_level(s,1,1,SurveyLevel::GeologicalSurvey,3);
    const auto observed=make_player_view(s,1).state;
    assert(observed.planets.front().warDamage.factoriesDestroyed==0); // No current hidden aftermath export.
}
void reconstruction_is_paid_limited_and_not_a_template_cheat()
{
    auto s=fixture(); auto& p=s.planets.front(); p.warDamage={2,1,0,0,1,0};
    assert(apply_production_batch(s,p,{p.id,K::ReconstructFactory,2}));
    assert(!apply_production_batch(s,p,{p.id,K::ReconstructFactory,1}));
    assert(!apply_production_batch(s,p,{p.id,K::ReconstructMine,2}));
    assert(!production_batch_valid({p.id,K::ReconstructFactory,1,ProductionAutomation::Annual}));
    assert(!production_template_valid({"Repair exploit",{{K::ReconstructFactory,4}}}));
    p.minerals={};
    for(int i=0;i<4;++i) { auto resolved=resolve_colony_production(s,p,false); assert(resolved.completions.empty()); ++s.turn; }
    assert(p.industry==0 && p.warDamage.factoriesReconstructed==0 && p.productionWaitingForMinerals);
    assert(p.productionQueue.front().remainingCost==0 && reconstruction_remaining(p,K::ReconstructFactory)==2);
    p.minerals={2,1,2};
    const auto completed=resolve_colony_production(s,p,false);
    assert(completed.completions.size()==1 && p.industry==1 && p.warDamage.factoriesReconstructed==1);
    assert(p.minerals.ironium==0 && p.minerals.boranium==0 && p.minerals.germanium==0);
    assert(p.warDamage.lastReconstructionTurn==s.turn+1);
    p.productionQueue.clear(); p.warDamage.factoriesReconstructed=2;
    p.productionQueue={{K::ReconstructFactory,4}}; p.minerals={10,10,10};
    const auto skipped=resolve_colony_production(s,p,false);
    assert(skipped.completions.empty() && p.productionQueue.empty() && p.minerals.ironium==10 && p.industry==1);
}
void forecasts_orders_and_recovery_history_use_the_same_production()
{
    auto s=fixture(); auto& p=s.planets.front(); p.warDamage={2,1,0,0,1,0};
    assert(apply_production_batch(s,p,{p.id,K::ReconstructFactory,2}));
    assert(apply_production_batch(s,p,{p.id,K::ReconstructMine,1}));
    const auto forecast=forecast_production_queue(s,p,p.productionQueue);
    assert(forecast[0].completionTurn && forecast[1].completionTurn);
    bool history=false; std::uint64_t factoryDone=0,mineDone=0;
    while(!s.planets.front().productionQueue.empty()) {
        auto result=TurnProcessor{}.process_with_events(s,{}); s=result.state;
        for(const auto& event:result.events) if(event.kind==GameEventKind::ProductionCompleted) {
            assert(!event.fleet);
            if(event.productionKind==K::ReconstructFactory) factoryDone=s.turn;
            if(event.productionKind==K::ReconstructMine) mineDone=s.turn;
        }
        for(const auto& marker:s.players[1].history.back().milestones) history|=marker.kind==HistoryMilestoneKind::Reconstruction;
        assert(s.turn<30);
    }
    assert(factoryDone==forecast[0].completionTurn && mineDone==forecast[1].completionTurn);
    assert(history && s.planets.front().industry==2 && s.planets.front().mines==1);
    assert(reconstruction_remaining(s.planets.front(),K::ReconstructFactory)==0);
    const auto& d=s.planets.front().warDamage; assert(d.factoriesDestroyed==2 && d.factoriesReconstructed==2);
    // Queued new construction creates a new installation, leaving a ruin available for later repair.
    auto normal=fixture(); normal.planets[0].warDamage={1,0,0,0,1,0};
    auto newBuild=normal; newBuild.planets[0].industry=6;
    newBuild=TurnProcessor{}.process(newBuild,{{2,{QueueProductionOrder{1,K::Factory}}}});
    assert(newBuild.planets[0].industry==7 && reconstruction_remaining(newBuild.planets[0],K::ReconstructFactory)==1);
    auto result=TurnProcessor{}.process(normal,{{2,{QueueProductionOrder{1,K::ReconstructFactory}}}});
    assert(!result.planets[0].productionQueue.empty());
    auto wrong=TurnProcessor{}.process(normal,{{1,{QueueProductionBatchOrder{1,K::ReconstructFactory,1}}}});
    assert(wrong.planets[0].productionQueue.empty() && wrong.planets[0].warDamage.factoriesReconstructed==0);
}
void capture_and_recolonization_inherit_the_physical_world()
{
    auto s=make_demo_game(); s.wormholeRules.spawnChancePerTurn=0;
    for(auto& player:s.players) player.technology.researchActive=false;
    auto& p=s.planets[1]; p.owner=0; p.population=0; p.industry=5; p.mines=3;
    p.warDamage={4,2,1,0,1,1}; p.minerals={10,20,30};
    auto& f=s.fleets.front(); f.design=kColonyShipDesignId; f.ships={{kColonyShipDesignId,1}};
    f.position=s.stars[1].position; f.colonists=100;
    const auto colonized=TurnProcessor{}.process(s,{{1,{ColonizePlanetOrder{f.id,p.id}}}});
    assert(colonized.planets[1].owner==1 && colonized.planets[1].industry==5 && colonized.planets[1].mines==3);
    assert(colonized.planets[1].warDamage==p.warDamage && colonized.planets[1].minerals.ironium>=10);
    assert(colonized.planets[1].productionQueue.empty() && colonized.planets[1].radar==PlanetaryRadarLevel::None);
    s.players.push_back({2,"Defenders"}); p.owner=2; p.population=1;
    const auto invaded=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,f.id},{p.id,0},100,{}}}}});
    assert(invaded.planets[1].owner==1 && invaded.planets[1].industry==5 && invaded.planets[1].mines==3);
    assert(invaded.planets[1].warDamage==p.warDamage && invaded.planets[1].productionQueue.empty());
}
void computer_uses_ordinary_reconstruction_orders()
{
    auto s=fixture(); auto& p=s.planets.front(); p.industry=8; p.warDamage={4,2,0,0,1,0};
    s.players[1].controller=PlayerController::Computer;
    const auto plan=plan_computer_turn(make_player_view(s,2));
    const auto repair=std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& order) {
        const auto* build=std::get_if<QueueProductionBatchOrder>(&order);
        return build && build->kind==K::ReconstructFactory;
    });
    assert(repair!=plan.orders.orders.end());
    const auto order=std::get<QueueProductionBatchOrder>(*repair); assert(order.quantity==3 && production_batch_valid(order));
    const auto mined=projected_mineral_mining(s,p);
    const auto resolved=TurnProcessor{}.process(s,{{2,{order}}});
    assert(resolved.planets[0].warDamage.factoriesReconstructed==2 && resolved.planets[0].industry==10);
    assert(std::abs(resolved.planets[0].minerals.ironium-(100+mined.ironium-4))<1e-6);
}
int main()
{
    bombardment_records_only_real_installation_losses(); reconstruction_is_paid_limited_and_not_a_template_cheat();
    forecasts_orders_and_recovery_history_use_the_same_production(); capture_and_recolonization_inherit_the_physical_world();
    computer_uses_ordinary_reconstruction_orders();
}
