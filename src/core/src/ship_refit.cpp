#include "suns/ship_refit.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cmath>
namespace suns {
std::optional<ShipRefitQuote> ship_refit_quote(const ShipDesign& from,const ShipDesign& to) {
    if(from.hull!=to.hull || from.id==to.id || !ship_design_valid(to) || to.retired)return {};
    ShipRefitQuote q;auto remaining=from.components;
    for(auto c:to.components) {
        auto i=std::find(remaining.begin(),remaining.end(),c);
        if(i!=remaining.end())remaining.erase(i);else q.installed.push_back(c);
    }
    q.removed=remaining;
    for(auto c:q.removed){auto m=component_mineral_cost(c);q.recovered.ironium+=m.ironium*.1;q.recovered.boranium+=m.boranium*.1;q.recovered.germanium+=m.germanium*.1;}
    for(auto c:q.installed){auto m=component_mineral_cost(c);q.installedMinerals.ironium+=m.ironium;q.installedMinerals.boranium+=m.boranium;q.installedMinerals.germanium+=m.germanium;q.componentWork+=component_spec(c).buildCost;}
    q.netMinerals={q.installedMinerals.ironium-q.recovered.ironium,q.installedMinerals.boranium-q.recovered.boranium,q.installedMinerals.germanium-q.recovered.germanium};
    q.work=q.componentWork+(q.componentWork+1)/2;
    return q;
}
bool ship_refit_available(const GameState& s,const Planet& p,const ShipRefitJob& job) {
    const auto* a=find_ship_design(s,job.source);const auto* b=find_ship_design(s,job.target);
    const auto f=std::find_if(s.fleets.begin(),s.fleets.end(),[&](const auto& f){return f.id==job.fleet;});
    const auto* star=find_star(s,p.star);
    if(!p.owner || !star || !a || !b || a->owner!=p.owner || b->owner!=p.owner
        || !ship_refit_quote(*a,*b) || !ship_design_available_to_player(s,p.owner,*b)
        || f==s.fleets.end() || f->owner!=p.owner || !same_position(f->position,star->position)
        || f->destination || !f->pendingCommands.empty() || f->outpost!=OutpostKind::None || f->orbitalBase.planet
        || !fleet_ship_count(*f,job.source)
        || !colony_has_orbital_service(s,p.id,p.owner,OrbitalStationModule::Shipyard))return false;
    auto next=*f;next.ships=fleet_ship_stacks(*f);
    auto i=std::find_if(next.ships.begin(),next.ships.end(),[&](const auto& x){return x.design==job.source;});--i->count;
    next.ships.push_back({job.target,1});normalize_fleet_composition(next);
    return fleet_cargo_used(s,*f)<=fleet_cargo_capacity(s,next)+1e-6 && f->fuel<=fleet_fuel_capacity(s,next)+1e-6;
}
bool queue_ship_refit(GameState& s,PlayerId owner,const RefitShipOrder& order) {
    auto p=std::find_if(s.planets.begin(),s.planets.end(),[&](const auto& p){return p.id==order.colony && p.owner==owner;});
    auto target=order.target;
    if(!order.pendingTargetName.empty()) {
        const auto d=std::find_if(s.shipDesigns.begin(),s.shipDesigns.end(),[&](const auto& d){return !d.retired && d.owner==owner && d.name==order.pendingTargetName;});
        if(d==s.shipDesigns.end())return false;target=d->id;
    }
    ShipRefitJob job{order.fleet,order.source,target};
    if(p==s.planets.end() || !ship_refit_available(s,*p,job))return false;
    for(const auto& world:s.planets)for(const auto& item:world.productionQueue)
        if(item.refit && item.refit->fleet==order.fleet)return false;
    const auto q=ship_refit_quote(*find_ship_design(s,job.source),*find_ship_design(s,job.target));
    ProductionItem item{ProductionKind::ShipRefit,q->work,job.target};item.refit=job;p->productionQueue.push_back(item);return true;
}
bool complete_ship_refit(GameState& s,Planet& p,const ShipRefitJob& job) {
    if(!ship_refit_available(s,p,job))return false;
    auto f=std::find_if(s.fleets.begin(),s.fleets.end(),[&](const auto& f){return f.id==job.fleet;});
    f->ships=fleet_ship_stacks(*f);auto i=std::find_if(f->ships.begin(),f->ships.end(),[&](const auto& x){return x.design==job.source;});
    --i->count;f->ships.push_back({job.target,1});normalize_fleet_composition(*f);
    if(const auto* d=fleet_design(s,*f))f->role=ship_design_can_colonize(*d)?FleetRole::ColonyShip:FleetRole::Scout;
    return true;
}
bool delete_ship_design(GameState& s,PlayerId owner,ShipDesignId id) {
    auto d=std::find_if(s.shipDesigns.begin(),s.shipDesigns.end(),[&](const auto& d){return d.id==id && d.owner==owner && !d.retired;});
    if(d==s.shipDesigns.end())return false;
    for(const auto& f:s.fleets)if(fleet_ship_count(f,id))return false;
    d->retired=true; // Keep immutable fitting data for dated reports/history.
    const auto references=[&](const auto& i){return i.shipDesign==id || (i.kind==ProductionKind::ColonyShip && !i.shipDesign && id==kColonyShipDesignId) || (i.refit && (i.refit->source==id || i.refit->target==id));};
    for(auto& p:s.planets)std::erase_if(p.productionQueue,references);
    for(auto& player:s.players) {
        for(auto& t:player.productionTemplates)std::erase_if(t.items,references);
        std::erase_if(player.productionTemplates,[](const auto& t){return t.items.empty();});
    }
    for(auto& f:s.fleets)std::erase_if(f.orbitalBase.queue,[&](const auto& i){return i.design==id;});
    return true;
}
}
