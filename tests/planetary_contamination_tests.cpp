#include "suns/planetary_recovery.hpp"
#include "suns/production.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include <algorithm>
#include <cassert>
using namespace suns;
using K=ProductionKind;
GameState fixture()
{
    auto s=make_demo_game(); s.wormholeRules.spawnChancePerTurn=0; s.fleets.clear();
    for(auto& player:s.players) player.technology.researchActive=false;
    auto& p=s.planets.front(); p.productionQueue.clear(); p.habitability=100; p.population=100000;
    p.industry=6; p.mines=0; p.minerals={50,50,50}; p.warDamage.lastBombardmentTurn=1;
    return s;
}
void only_explicit_radiological_weapons_create_bounded_scars()
{
    auto s=fixture(); s.players.push_back({2,"Defender"}); auto& p=s.planets.front(); p.owner=2; p.defenses=0;
    s.orbitalStations.clear();
    s.shipDesigns.push_back({900,1,"Bomber",ShipHullType::Destroyer,
        {ShipComponentType::FuelMizer,ShipComponentType::LadyFingerBomb}});
    Fleet f; f.id=900; f.owner=1; f.position=find_star(s,p.star)->position;
    f.ships={{900,1}}; f.design=900; f.task=FleetTask::Bombardment; s.fleets={f};
    const auto environment=p.environment; const auto natural=p.naturalEnvironment;
    resolve_bombardments(s,2); assert(p.warDamage.contamination==0);
    s.shipDesigns.back().components.back()=ShipComponentType::NeutronBomb;
    resolve_bombardments(s,2); assert(p.warDamage.contamination==4);
    s.shipDesigns.back().components.back()=ShipComponentType::EnrichedNeutronBomb;
    resolve_bombardments(s,2); assert(p.warDamage.contamination==12);
    s.fleets[0].ships[0].count=100; resolve_bombardments(s,3);
    assert(p.warDamage.contamination==100 && p.warDamage.lastBombardmentTurn==3);
    assert(p.environment.radiation==environment.radiation && p.environment.gravity==environment.gravity);
    assert(p.naturalEnvironment.has_value()==natural.has_value());
    if(natural) assert(p.naturalEnvironment->radiation==natural->radiation);
    const auto packet=s.players[0].pendingPlayerReports.back();
    assert(packet.aftermath && !packet.aftermath->exact && packet.aftermath->contaminationMaximum==100);
}
void hazard_is_temporary_separate_and_respects_immunity()
{
    auto s=fixture(); auto& p=s.planets[0]; const auto clean=current_planet_habitability(s,p,s.turn);
    const auto cleanCapacity=population_capacity(s,p,s.turn); const auto cleanGrowth=projected_population_growth(s,p,s.turn);
    p.warDamage.contamination=100;
    assert(current_planet_habitability(s,p,s.turn)==clean-20);
    assert(population_capacity(s,p,s.turn)<cleanCapacity && projected_population_growth(s,p,s.turn)<cleanGrowth);
    s.players[0].race.radiationImmune=true;
    assert(current_planet_habitability(s,p,s.turn)==clean);
    p.owner=0; const auto evolved=TurnProcessor{}.process(s,{});
    assert(evolved.planets[0].warDamage.contamination==98);
    assert(s.planets[0].warDamage.contamination==100);
    for(int year=0;year<50;++year) decay_planet_contamination(p);
    assert(p.warDamage.contamination==0);
}
void cleanup_spends_production_and_minerals_and_retires_obsolete_work()
{
    auto s=fixture(); auto& p=s.planets[0]; p.warDamage.contamination=15;
    p.population=10000; p.industry=1; p.minerals={};
    assert(apply_production_batch(s,p,{p.id,K::CleanupContamination,2}));
    assert(!apply_production_batch(s,p,{p.id,K::CleanupContamination,1}));
    assert(!production_batch_valid({p.id,K::CleanupContamination,1,ProductionAutomation::Annual}));
    assert(!production_template_valid({"Cleanup exploit",{{K::CleanupContamination,6}}}));
    for(int i=0;i<6;++i) { auto r=resolve_colony_production(s,p,false); assert(r.completions.empty()); ++s.turn; }
    assert(p.productionWaitingForMinerals && p.productionQueue.front().remainingCost==0 && p.warDamage.contamination==15);
    p.minerals={0,1,1}; const auto r=resolve_colony_production(s,p,false);
    assert(r.completions.size()==1 && p.warDamage.contamination==5 && p.warDamage.lastCleanupTurn==s.turn+1);
    assert(p.minerals.boranium==0 && p.minerals.germanium==0);
    decay_planet_contamination(p); decay_planet_contamination(p); decay_planet_contamination(p);
    p.minerals={0,9,9}; const auto skipped=resolve_colony_production(s,p,false);
    assert(skipped.completions.empty() && p.productionQueue.empty() && p.minerals.boranium==9);
}
void forecasts_match_real_cleanup_decay_and_delivered_history()
{
    auto s=fixture(); auto& p=s.planets[0]; p.warDamage.contamination=23;
    assert(apply_production_batch(s,p,{p.id,K::CleanupContamination,2}));
    const auto forecast=forecast_production_queue(s,p,p.productionQueue);
    assert(forecast[0].completionTurn);
    bool marker=false; std::uint64_t done{};
    for(int i=0;i<15 && !s.planets[0].productionQueue.empty();++i) {
        auto result=TurnProcessor{}.process_with_events(s,{}); s=std::move(result.state);
        for(const auto& event:result.events) if(event.kind==GameEventKind::ProductionCompleted && event.productionKind==K::CleanupContamination) {
            done=event.turn; assert(event.aftermath && event.aftermath->exact && event.aftermath->receivedTurn==event.turn);
        }
        for(const auto& m:s.players[0].history.back().milestones) marker|=m.kind==HistoryMilestoneKind::ContaminationCleanup;
    }
    assert(done==forecast[0].completionTurn && marker && s.planets[0].warDamage.contamination==0);
    auto decays=fixture(); auto& q=decays.planets[0]; q.warDamage.contamination=2;
    assert(apply_production_batch(decays,q,{q.id,K::CleanupContamination,1}));
    const auto unnecessary=forecast_production_queue(decays,q,q.productionQueue);
    assert(!unnecessary[0].completionTurn && unnecessary[0].retiredTurn==decays.turn+1 && !unnecessary[0].beyondForecastHorizon);
    const auto before=q.minerals; const auto mined=projected_mineral_mining(decays,q); const auto after=TurnProcessor{}.process(decays,{});
    assert(after.planets[0].productionQueue.empty() && after.planets[0].minerals.boranium==before.boranium+mined.boranium);
    auto recolonized=fixture(); recolonized.planets[1].owner=0; recolonized.planets[1].warDamage.contamination=42;
    recolonized.planets[1].warDamage.lastBombardmentTurn=1;
    const auto aged=TurnProcessor{}.process(recolonized,{});
    assert(aged.planets[1].warDamage.contamination==40);
}
void computer_uses_the_same_paid_orders()
{
    auto s=fixture(); s.players[0].controller=PlayerController::Computer; s.planets[0].warDamage.contamination=40;
    const auto plan=plan_computer_turn(make_player_view(s,1));
    assert(std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){
        const auto* q=std::get_if<QueueProductionBatchOrder>(&o); return q && q->kind==K::CleanupContamination;
    }));
    auto next=TurnProcessor{}.process(s,{plan.orders});
    for(int year=0;year<12 && !next.planets[0].warDamage.lastCleanupTurn;++year) next=TurnProcessor{}.process(next,{});
    assert(next.planets[0].warDamage.lastCleanupTurn && next.planets[0].warDamage.contamination<38);
}
void capture_and_recolonization_preserve_the_hazard()
{
    auto s=make_demo_game(); s.wormholeRules.spawnChancePerTurn=0;
    for(auto& player:s.players) player.technology.researchActive=false;
    auto& p=s.planets[1]; p.owner=0; p.population=0; p.warDamage.contamination=30; p.warDamage.lastBombardmentTurn=1;
    auto& f=s.fleets.front(); f.design=kColonyShipDesignId; f.ships={{kColonyShipDesignId,1}};
    f.position=s.stars[1].position; f.colonists=100;
    const auto colony=TurnProcessor{}.process(s,{{1,{ColonizePlanetOrder{f.id,p.id}}}});
    assert(colony.planets[1].owner==1 && colony.planets[1].warDamage.contamination==28);
    s.players.push_back({2,"Defender"}); p.owner=2; p.population=1;
    const auto invaded=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,f.id},{p.id,0},100,{}}}}});
    assert(invaded.planets[1].owner==1 && invaded.planets[1].warDamage.contamination==28);
}
int main()
{
    only_explicit_radiological_weapons_create_bounded_scars(); hazard_is_temporary_separate_and_respects_immunity();
    cleanup_spends_production_and_minerals_and_retires_obsolete_work();
    forecasts_match_real_cleanup_decay_and_delivered_history(); computer_uses_the_same_paid_orders();
    capture_and_recolonization_preserve_the_hazard();
}
