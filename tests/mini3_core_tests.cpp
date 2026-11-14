#include "suns/campaign.hpp"
#include "suns/ship_refit.hpp"
#include "suns/production.hpp"
#include "suns/exotic_materials.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
using namespace suns;
namespace {
GameState fixture() {
    auto s=make_demo_game();s.wormholeRules.spawnChancePerTurn=0;
    s.players.front().technology.levels.fill(26);s.players.front().technology.researchAllocationPercent=0;
    auto& p=s.planets.front();p.population=1000000;p.industry=1000;p.minerals={10000,10000,10000};p.productionQueue.clear();
    auto from=s.shipDesigns.front();from.id=s.nextShipDesignId++;from.name="Refit source";
    from.components={ShipComponentType::FusionDrive,ShipComponentType::LongRangeScanner};
    from.placements=autoplace_ship_components(from.hull,from.components);assert(ship_design_valid(from));
    auto to=from;to.id=s.nextShipDesignId++;to.name="Refit target";to.components.back()=ShipComponentType::PenetratingScanner;
    to.placements=autoplace_ship_components(to.hull,to.components);assert(ship_design_valid(to));s.shipDesigns={from,to};
    auto f=s.fleets.front();f.design=from.id;f.ships={{from.id,2}};f.position=s.stars.front().position;
    f.destination.reset();f.pendingCommands.clear();f.fuel=10;f.damagePercent=5;
    f.telemetry.ships=f.ships;f.telemetry.position=f.position;f.telemetry.fuel=f.fuel;f.telemetry.observedTurn=s.turn;
    s.fleets={f};set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);return s;
}
void quote_and_host() {
    auto s=fixture();const auto source=s.shipDesigns[0].id,target=s.shipDesigns[1].id,fleet=s.fleets[0].id;
    const auto q=ship_refit_quote(s.shipDesigns[0],s.shipDesigns[1]);assert(q);
    assert(q->removed==std::vector{ShipComponentType::LongRangeScanner});
    assert(q->installed==std::vector{ShipComponentType::PenetratingScanner});
    assert(q->componentWork==component_spec(ShipComponentType::PenetratingScanner).buildCost);
    assert(std::abs(q->recovered.ironium-component_mineral_cost(ShipComponentType::LongRangeScanner).ironium*.1)<1e-8);
    assert(q->work==q->componentWork+(q->componentWork+1)/2);
    const auto control=TurnProcessor{}.process(s,{});
    const auto fuel=control.fleets[0].fuel;
    const PlayerOrders o{1,{RefitShipOrder{s.planets[0].id,fleet,source,target}}};
    const auto preview=planned_production_state(s,o);assert(preview.planets[0].productionQueue.size()==1);
    assert(fleet_ship_count(s.fleets[0],target)==0);
    s=TurnProcessor{}.process(s,{o});assert(fleet_ship_count(s.fleets[0],source)==1 && fleet_ship_count(s.fleets[0],target)==1);
    assert(s.planets[0].productionQueue.empty() && s.fleets[0].fuel==fuel);
    assert(std::abs(control.planets[0].minerals.ironium-s.planets[0].minerals.ironium-q->netMinerals.ironium)<1e-6);
    assert(std::abs(control.planets[0].minerals.boranium-s.planets[0].minerals.boranium-q->netMinerals.boranium)<1e-6);
    assert(std::abs(control.planets[0].minerals.germanium-s.planets[0].minerals.germanium-q->netMinerals.germanium)<1e-6);
    assert(!delete_ship_design(s,1,source) && !delete_ship_design(s,1,target));
}
void pending_target_is_resolved_by_name() {
    auto s=fixture();auto other=s.players.front();other.id=2;other.name="Other";s.players.push_back(other);
    for(int n=0;n<3;++n){auto foreign=s.shipDesigns.front();foreign.id=s.nextShipDesignId++;foreign.owner=2;foreign.name="Foreign "+std::to_string(n);s.shipDesigns.push_back(foreign);}
    const auto project=s.shipDesigns[1];
    const PlayerOrders orders{1,{CreateShipDesignOrder{"Deferred target",project.hull,project.components,project.placements},RefitShipOrder{s.planets[0].id,s.fleets[0].id,s.shipDesigns[0].id,0,"Deferred target"}}};
    const auto view=make_player_view(s,1).state;
    const auto preview=planned_production_state(view,orders);assert(preview.planets[0].productionQueue.size()==1);
    const auto next=TurnProcessor{}.process(s,{orders});
    const auto target=std::find_if(next.shipDesigns.begin(),next.shipDesigns.end(),[](const auto& d){return d.owner==1 && d.name=="Deferred target";});assert(target!=next.shipDesigns.end());
    assert(target->id!=preview.planets[0].productionQueue.front().shipDesign);
    assert(fleet_ship_count(next.fleets[0],target->id)==1);
}
void blocked_and_delete() {
    auto s=fixture();const auto a=s.shipDesigns[0].id,b=s.shipDesigns[1].id;
    assert(!delete_ship_design(s,1,a));s.fleets[0].destination=Position{9,9};assert(!delete_ship_design(s,1,a));
    assert(!queue_ship_refit(s,1,{s.planets[0].id,s.fleets[0].id,a,b}));s.fleets[0].destination.reset();
    assert(queue_ship_refit(s,1,{s.planets[0].id,s.fleets[0].id,a,b}));
    assert(!queue_ship_refit(s,1,{s.planets[0].id,s.fleets[0].id,a,b}));
    s.players[0].productionTemplates={{"Target",{{ProductionKind::ColonyShip,1,b}}}};
    assert(delete_ship_design(s,1,b));assert(s.shipDesigns[1].retired && s.planets[0].productionQueue.empty());
    assert(s.players[0].productionTemplates.empty());assert(!ship_design_available_to_player(s,1,s.shipDesigns[1]));
    const auto late=TurnProcessor{}.process(s,{{1,{QueueShipDesignOrder{s.planets[0].id,b}}}});
    assert(late.planets[0].productionQueue.empty() && fleet_ship_count(late.fleets[0],b)==0);
    auto empty=make_demo_game();auto before=empty;before.planets[0].owner=0;
    publish_exotic_material_reports(empty,before);
    for(const auto& p:empty.players)for(const auto& report:p.pendingPlayerReports)assert(report.kind!=PlayerReportKind::ResonanceProjectStatus);
}
void actual_unload() {
    for(auto cargo:{FleetCargoKind::Colonists,FleetCargoKind::Boranium,FleetCargoKind::All}) {
        auto s=make_demo_game();s.wormholeRules.spawnChancePerTurn=0;
        auto& f=s.fleets.back();f.position=s.stars.front().position;f.destination=f.position;
        f.colonists=1000;f.minerals={0,3,0};f.arrivalAction={FleetArrivalActionKind::UnloadAll,1,cargo};
        const auto id=f.id;const auto result=TurnProcessor{}.process(s,{});
        const auto after=std::find_if(result.fleets.begin(),result.fleets.end(),[id](const auto& f){return f.id==id;});assert(after!=result.fleets.end());
        assert(after->colonists==(cargo==FleetCargoKind::Boranium?1000:0));
        assert(after->minerals.boranium==(cargo==FleetCargoKind::Colonists?3:0));
    }
}
}
int main(){quote_and_host();pending_target_is_resolved_by_name();blocked_and_delete();actual_unload();}
