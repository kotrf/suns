#include "computer_policy.hpp"
#include "suns/native_orbital.hpp"
#include "suns/native_orbital_support.hpp"
#include "suns/mining.hpp"
#include "suns/stations.hpp"
#include "suns/communications.hpp"
#include "suns/production.hpp"
#include "suns/player_knowledge.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace suns::computer_detail {
namespace {
double mining_rate(const GameState& state,const Fleet& fleet)
{
    double result=0;
    for(const auto& stack:fleet_ship_stacks(fleet)) if(const auto* d=find_ship_design(state,stack.design))
        result+=stack.count*ship_design_remote_mining_rate(*d);
    return result;
}
bool fresh_queue(const GameState& state,const Planet& colony)
{
    // A closing economic observation includes this boundary's support stamp.
    // Older packets cannot acknowledge a production order sent this year.
    return state.turn==1 || colony.nativeOrbit.servicedTurn==state.turn;
}
double support_concentration(const Planet& p)
{
    const auto c=p.observedConcentration.value_or(MineralCargo{25,25,25});
    return std::max(1.0,std::min({c.ironium,c.boranium,c.germanium}));
}
}

void plan_native_civilization(const PlayerView& view,ComputerTurnPlan& plan,std::string& reason)
{
    const auto& state=view.state;
    const auto* player=find_player(state,view.player);
    if(!player)return;
    auto& goals=plan.nextState.fleetGoals;
    std::vector<const Planet*> colonies;
    for(const auto& p:state.planets)if(p.owner==view.player && native_orbital_station(state,p))colonies.push_back(&p);
    std::erase_if(goals,[&](const auto& goal){
        const auto f=std::find_if(state.fleets.begin(),state.fleets.end(),[&](const auto& f){return f.id==goal.fleet;});
        if(f==state.fleets.end())return true;
        const auto* at=find_star(state,goal.target);
        if(goal.kind==ComputerFleetGoalKind::Colonize) {
            const auto* p=at?find_planet_at_star(state,at->id):nullptr;
            const auto owner=p?known_planet_owner(state,view.player,p->id):std::nullopt;
            if(owner && *owner==view.player)return true;
        }
        if(goal.kind==ComputerFleetGoalKind::Resupply && at && f->task==FleetTask::RemoteMining
            && same_position(f->position,at->position))return true;
        return state.turn>=goal.reviewTurn && confirmed_idle(*f,goal.issuedTurn);
    });
    const auto reserved=[&](FleetId id){return std::any_of(goals.begin(),goals.end(),[&](const auto& g){return g.fleet==id;});};
    const auto occupied=[&](StarId id,ComputerFleetGoalKind kind){return std::any_of(goals.begin(),goals.end(),[&](const auto& g){return g.target==id && g.kind==kind;});};
    std::size_t miners=0,scouts=0,founders=0;
    for(const auto& f:state.fleets)if(f.owner==view.player && f.outpost==OutpostKind::None) {
        if(mining_rate(state,f)>0)miners+=fleet_ship_count(f);
        else if(fleet_can_found_native_orbit(state,f))founders+=fleet_ship_count(f);
        else if(fleet_sensor_range(state,f)>0)scouts+=fleet_ship_count(f);
    }
    auto focus=ResearchField::Energy;
    if(technology_level(state,view.player,ResearchField::Energy)>=4)focus=ResearchField::Construction;
    if(technology_level(state,view.player,ResearchField::Construction)>=6)focus=ResearchField::Propulsion;
    if(technology_level(state,view.player,ResearchField::Propulsion)>=3)focus=ResearchField::Energy;
    const std::uint8_t allocation=miners<2?0:20;
    if(player->technology.researchAllocationPercent!=allocation)plan.orders.orders.push_back(SetResearchAllocationOrder{allocation});
    if(!player->technology.researchActive || player->technology.focus!=focus)
        plan.orders.orders.push_back(SetResearchPlanOrder{focus,{},true});
    plan.nextState.researchGoal=ComputerResearchGoal{focus,std::uint8_t(std::min(26,int(technology_level(state,view.player,focus))+1))};

    const auto safe_target=[&](const Fleet& fleet,bool survey)->const StarSystem* {
        const StarSystem* best=nullptr;double distance=std::numeric_limits<double>::infinity();
        for(const auto& star:state.stars) {
            if(survey) {
                if(survey_level(state,view.player,star.id)>=SurveyLevel::OrbitalSurvey
                    || occupied(star.id,ComputerFleetGoalKind::Survey))continue;
            } else {
                const auto* p=find_planet_at_star(state,star.id);if(!p)continue;
                const auto owner=known_planet_owner(state,view.player,p->id);
                const auto dose=native_orbital_radiation_status(state,view.player,star.id,1000,native_orbital_founding_loadout());
                if(!owner || *owner || !dose || dose->annualLoss || occupied(star.id,ComputerFleetGoalKind::Colonize))continue;
            }
            const auto d=distance_between(fleet.position,star.position);
            if(d<distance && route_warp(state,fleet,{star.position})) {best=&star;distance=d;}
        }
        return best;
    };
    const auto mission=[&](const Fleet& fleet,const StarSystem& target,FleetArrivalAction action,ComputerFleetGoalKind kind) {
        if(goals.size()>=kMaximumComputerFleetGoals)return;
        if(const auto warp=route_warp(state,fleet,{target.position})) {
            plan.orders.orders.push_back(MoveFleetOrder{fleet.id,target.position,*warp,action});
            goals.push_back({fleet.id,kind,target.id,state.turn,review_turn(state,fleet)});
            explain(reason,"Dispatch native fleet "+std::to_string(fleet.id)+" from received evidence");
        }
    };
    // Assign real miners to received inhabited orbits. Keep established mining
    // running; a newly founded colony receives an idle/new ship, not free output.
    const auto assigned_rate=[&](const Planet& p) {
        const auto* at=find_star(state,p.star);double rate=0;
        if(!at)return rate;
        for(const auto& f:state.fleets)if(f.owner==view.player) {
            const auto g=std::find_if(goals.begin(),goals.end(),[&](const auto& g){return g.fleet==f.id && g.kind==ComputerFleetGoalKind::Resupply;});
            if(g!=goals.end()) {if(g->target==p.star)rate+=mining_rate(state,f);}
            else if(same_position(f.position,at->position))rate+=mining_rate(state,f);
        }
        return rate;
    };
    const auto needed_rate=[&](const Planet& p) {
        return (std::ceil(double(p.nativeOrbit.population)/25000.0)+0.25)*100/support_concentration(p);
    };
    for(const auto& original:state.fleets) {
        if(original.owner!=view.player || original.outpost!=OutpostKind::None || reserved(original.id))continue;
        const bool mining=original.task==FleetTask::RemoteMining && original.telemetry.task==FleetTask::RemoteMining
            && original.telemetry.observedTurn==state.turn && !original.destination;
        if(!mining && (!idle(original) || !confirmed_idle(original,0)))continue;
        auto fleet=original;fleet.position=fleet.telemetry.position;budget_initial_depot_service(state,fleet);
        if(mining_rate(state,fleet)>0) {
            const auto* local=colony_at(state,view.player,fleet.position);
            if(mining && (!local || assigned_rate(*local)-mining_rate(state,fleet)<needed_rate(*local)))continue;
            const Planet* target=nullptr;double best=std::numeric_limits<double>::infinity();
            for(const auto* p:colonies) {
                const auto* star=find_star(state,p->star);if(!star)continue;
                if(mining && p==local)continue;
                const auto supplied=assigned_rate(*p);
                if(mining && supplied>=needed_rate(*p))continue;
                const auto score=supplied*support_concentration(*p)/100.0
                    /std::max(1.0,std::ceil(double(p->nativeOrbit.population)/25000.0))
                    +distance_between(fleet.position,star->position)/10000.0;
                if(score<best && route_warp(state,fleet,{star->position})){target=p;best=score;}
            }
            if(target) {
                const auto* at=find_star(state,target->star);
                if(same_position(fleet.position,at->position)) {
                    plan.orders.orders.push_back(SetRemoteMiningOrder{fleet.id,true});
                    goals.push_back({fleet.id,ComputerFleetGoalKind::Resupply,target->star,state.turn,review_turn(state,fleet)});
                } else mission(fleet,*at,{FleetArrivalActionKind::RemoteMining},ComputerFleetGoalKind::Resupply);
            }
        } else if(fleet_can_found_native_orbit(state,fleet)) {
            const auto* local=colony_at(state,view.player,fleet.position);
            if(fleet.colonists<1000 && local && local->nativeOrbit.population>6000) {
                const auto amount=1000-fleet.colonists;
                if(fleet_cargo_used(state,fleet)+colonist_cargo_mass(amount)<=fleet_cargo_capacity(state,fleet)+1e-6) {
                    plan.orders.orders.push_back(TransferCargoOrder{{local->id,0,true},{0,fleet.id},amount,{}});
                    fleet.colonists+=amount;
                }
            }
            if(fleet.colonists)if(const auto* target=safe_target(fleet,false))
                mission(fleet,*target,{FleetArrivalActionKind::Colonize},ComputerFleetGoalKind::Colonize);
        } else if(fleet_sensor_range(state,fleet)>0) {
            if(const auto* target=safe_target(fleet,true))mission(fleet,*target,{},ComputerFleetGoalKind::Survey);
        }
    }
    const ShipDesign miner{state.nextShipDesignId,view.player,"Orbital Miner Pair",ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5,ShipComponentType::RoboMiniMiner,ShipComponentType::RoboMiniMiner}};
    const ShipDesign scout{state.nextShipDesignId,view.player,"Orbital Surveyor",ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5,ShipComponentType::RhinoScanner}};
    const ShipDesign founder{state.nextShipDesignId,view.player,"Orbital Settler",ShipHullType::ColonyShip,
        {ShipComponentType::QuickJump5,ShipComponentType::OrbitalConstructionModule}};
    const auto build=[&](const Planet& p,const ShipDesign& design) {
        if(!ship_design_valid(design) || !ship_design_available_to_player(state,view.player,design))return false;
        const auto quote=native_orbital_support_quote(state,p);auto stock=p.minerals;
        stock.ironium-=quote.mineralUnitsPaid;stock.boranium-=quote.mineralUnitsPaid;stock.germanium-=quote.mineralUnitsPaid;
        if(!mineral_cargo_sufficient(stock,ship_design_mineral_cost(design)))return false;
        const auto existing=std::find_if(state.shipDesigns.begin(),state.shipDesigns.end(),[&](const auto& d){return d.owner==view.player && d.name==design.name && d.components==design.components && d.hull==design.hull;});
        if(existing!=state.shipDesigns.end())plan.orders.orders.push_back(QueueShipDesignOrder{p.id,existing->id});
        else {
            if(std::none_of(plan.orders.orders.begin(),plan.orders.orders.end(),[&](const auto& o){const auto* c=std::get_if<CreateShipDesignOrder>(&o);return c && c->name==design.name;}))
                plan.orders.orders.push_back(CreateShipDesignOrder{design.name,design.hull,design.components});
            plan.orders.orders.push_back(QueueShipDesignOrder{p.id,0,design.name});
        }
        explain(reason,"Build paid "+design.name+" at colony "+std::to_string(p.id));return true;
    };
    for(const auto* p:colonies) {
        // Without a current received queue, wait for evidence rather than
        // duplicating an unacknowledged remote construction command.
        if(!fresh_queue(state,*p) || !p->productionQueue.empty() || !p->nativeOrbit.population
            || !colony_has_orbital_service(state,p->id,view.player,OrbitalStationModule::Shipyard))continue;
        const auto* star=find_star(state,p->star);if(!star)continue;
        const bool shortage=std::any_of(colonies.begin(),colonies.end(),[&](const auto* c){return assigned_rate(*c)<needed_rate(*c);});
        const bool pendingMiner=std::any_of(colonies.begin(),colonies.end(),[&](const auto* c){
            return std::any_of(c->productionQueue.begin(),c->productionQueue.end(),[&](const auto& item){
                const auto* design=find_ship_design(state,item.shipDesign);
                return design && ship_design_remote_mining_rate(*design)>0;
            });
        });
        if(!pendingMiner && (miners<2 || shortage)) {
            if(build(*p,miner)){++miners;continue;}
        }
        if(!scouts && build(*p,scout)){++scouts;continue;}
        if(!founders && !shortage && !pendingMiner) {
            // Candidate ownership/safety is known independently of a future hull.
            const bool target=std::any_of(state.planets.begin(),state.planets.end(),[&](const auto& t){
                const auto owner=known_planet_owner(state,view.player,t.id);
                const auto dose=native_orbital_radiation_status(state,view.player,t.star,1000,native_orbital_founding_loadout());
                return owner && !*owner && dose && !dose->annualLoss && !occupied(t.star,ComputerFleetGoalKind::Colonize);
            });
            if(target && build(*p,founder)){++founders;continue;}
        }
    }
}
}
