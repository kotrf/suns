#include "computer_policy.hpp"
#include "suns/combat.hpp"
#include "suns/native_orbital.hpp"
#include "suns/native_orbital_support.hpp"
#include "suns/mining.hpp"
#include "suns/stations.hpp"
#include "suns/communications.hpp"
#include "suns/production.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/equipment.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

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
std::optional<OrbitalStationHullType> needed_housing(const Planet& p,const OrbitalStation& station)
{
    const auto capacity=native_orbital_capacity(station.hull);
    if(p.nativeOrbit.population<capacity*4/5)return {};
    for(const auto hull:{OrbitalStationHullType::NativeCitadel,OrbitalStationHullType::NativeBastion})
        if(native_orbital_capacity(hull)>capacity)return hull;
    return {};
}
std::optional<ShipComponentType> station_equipment(const GameState& s,PlayerId owner,ShipComponentKind kind)
{
    std::optional<ShipComponentType> best;double score=0;
    for(const auto& tech:equipment_technologies()) {
        if(tech.kind!=kind || !component_available_to_player(s,owner,tech.component))continue;
        const auto spec=component_spec(tech.component);
        // Stations cannot close the initial one-unit gap to an attacking fleet.
        // A range-zero melee beam cannot supply their baseline defense.
        const auto value=kind==ShipComponentKind::BeamWeapon
            ? (spec.shieldOnly || !spec.weaponRange?0:spec.weaponPower*(1+0.2*spec.weaponRange)) : spec.shields;
        if(value>score){score=value;best=tech.component;}
    }
    return best;
}
std::optional<StationConstruction> station_improvement(const GameState& s,const Planet& p)
{
    const auto* station=native_orbital_station(s,p);if(!station)return {};
    const auto original=station_loadout(*station);auto fit=original;
    if(const auto housing=needed_housing(p,*station);housing && station_hull_available(s,p.owner,*housing))
        fit.hull=*housing;
    // Preserve all purchased protection/services and fitted equipment. The first
    // stationary defense fit adds at most two ordinary beams and one shield.
    if(!station_has_weapons(*station)) {
        if(const auto beam=station_equipment(s,p.owner,ShipComponentKind::BeamWeapon))for(int i=0;i<2;++i) {
            auto candidate=fit;candidate.components.push_back(*beam);
            if(station_loadout_valid(candidate))fit=std::move(candidate);
        }
        if(!station_shields(*station))if(const auto shield=station_equipment(s,p.owner,ShipComponentKind::Shield)) {
            auto candidate=fit;candidate.components.push_back(*shield);
            if(station_loadout_valid(candidate))fit=std::move(candidate);
        }
    }
    StationConstruction work{fit,station->id};
    if(fit==original || !station_construction_valid(s,p,work))return {};
    return work;
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
    std::vector<const StarSystem*> threatened;
    for (const auto* colony : colonies) {
        const auto* star = find_star(state, colony->star);
        if (star && colony->nativeOrbit.population && colony_threatened(view, star->position)) threatened.push_back(star);
    }
    std::vector<const Fleet*> guards;
    std::size_t miners=0,scouts=0,founders=0;
    for(const auto& f:state.fleets)if(f.owner==view.player && f.outpost==OutpostKind::None) {
        if(mining_rate(state,f)>0)miners+=fleet_ship_count(f);
        else if(fleet_can_found_native_orbit(state,f))founders+=fleet_ship_count(f);
        else if(fleet_has_space_weapons(state,f)) {
            if (!f.colonists) guards.push_back(&f);
        }
        else if(fleet_sensor_range(state,f)>0)scouts+=fleet_ship_count(f);
    }
    auto focus=ResearchField::Energy;
    if(technology_level(state,view.player,ResearchField::Energy)>=4)focus=ResearchField::Construction;
    if(technology_level(state,view.player,ResearchField::Construction)>=6)focus=ResearchField::Propulsion;
    if(technology_level(state,view.player,ResearchField::Propulsion)>=3)focus=ResearchField::Energy;
    // Keep the original startup sequence. Once it is funded, a crowded hull
    // gives Construction a concrete target instead of researching Energy forever.
    if(technology_level(state,view.player,ResearchField::Energy)>=4
        && technology_level(state,view.player,ResearchField::Construction)>=6
        && technology_level(state,view.player,ResearchField::Propulsion)>=3)
    {
        if (!threatened.empty() && technology_level(state,view.player,ResearchField::Weapons)<3)
            focus=ResearchField::Weapons;
        for(const auto* p:colonies)if(const auto next=needed_housing(*p,*native_orbital_station(state,*p));
            next && !station_hull_available(state,view.player,*next))focus=ResearchField::Construction;
    }
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
        if(goals.size()>=kMaximumComputerFleetGoals)return false;
        if(const auto warp=route_warp(state,fleet,{target.position})) {
            plan.orders.orders.push_back(MoveFleetOrder{fleet.id,target.position,*warp,action});
            goals.push_back({fleet.id,kind,target.id,state.turn,review_turn(state,fleet)});
            explain(reason,kind==ComputerFleetGoalKind::Defend
                ? "Dispatch native guard "+std::to_string(fleet.id)+" to orbit "+std::to_string(target.id)
                : "Dispatch native fleet "+std::to_string(fleet.id)+" from received evidence");
            return true;
        }
        return false;
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
    const auto can_spare_miner = [&] {
        return std::any_of(state.fleets.begin(), state.fleets.end(), [&](const auto& fleet) {
            if (fleet.owner != view.player || fleet.task != FleetTask::RemoteMining || fleet.destination
                || fleet.telemetry.observedTurn != state.turn || reserved(fleet.id)) return false;
            const auto* local = colony_at(state, view.player, fleet.position);
            return local && assigned_rate(*local) - mining_rate(state, fleet) >= needed_rate(*local);
        });
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
        } else if(!fleet_has_space_weapons(state,fleet) && fleet_sensor_range(state,fleet)>0) {
            if(const auto* target=safe_target(fleet,true))mission(fleet,*target,{},ComputerFleetGoalKind::Survey);
        }
    }
    std::unordered_set<StarId> covered;
    std::unordered_set<StarId> stationed;
    std::vector<const Fleet*> idleGuards;
    for (const auto* guard : guards) {
        const auto assignment = std::find_if(goals.begin(), goals.end(), [&](const auto& goal) {
            return goal.fleet == guard->id;
        });
        const auto issuedTurn = assignment == goals.end() ? 0 : assignment->issuedTurn;
        const bool stationary = idle(*guard) && confirmed_idle(*guard, issuedTurn) && guard->pendingCommands.empty();
        if (assignment == goals.end()) {
            if (stationary) idleGuards.push_back(guard);
            continue;
        }
        if (assignment->kind != ComputerFleetGoalKind::Defend) continue;
        covered.insert(assignment->target);
        // An incoming assignment reserves its destination. Relieving a local
        // defender also requires a received arrival.
        const auto* target = find_star(state, assignment->target);
        if (stationary && target && same_position(guard->telemetry.position, target->position))
            stationed.insert(target->id);
    }
    std::vector<const Fleet*> available;
    for (const auto* guard : idleGuards) {
        const auto* local = colony_at(state, view.player, guard->telemetry.position);
        if (local && std::any_of(threatened.begin(), threatened.end(), [&](const auto* star) {
                return star->id == local->star;
            }) && stationed.insert(local->star).second) {
            covered.insert(local->star);
        } else if (guard->telemetry.observedTurn == state.turn && guard->damagePercent <= 40) {
            available.push_back(guard);
        }
    }
    for (const auto* original : available) {
        auto guard = *original;
        guard.position = guard.telemetry.position;
        const StarSystem* target = nullptr;
        double nearest = std::numeric_limits<double>::infinity();
        for (const auto* star : threatened) {
            if (covered.contains(star->id)) continue;
            const auto distance = distance_between(guard.position, star->position);
            if (distance < nearest && route_warp(state, guard, {star->position})) {
                nearest = distance;
                target = star;
            }
        }
        if (target && mission(guard, *target, {}, ComputerFleetGoalKind::Defend)) covered.insert(target->id);
    }
    std::unordered_set<FleetId> departing;
    for(const auto& order:plan.orders.orders)if(const auto* move=std::get_if<MoveFleetOrder>(&order))departing.insert(move->fleet);
    const ShipDesign miner{state.nextShipDesignId,view.player,"Orbital Miner Pair",ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5,ShipComponentType::RoboMiniMiner,ShipComponentType::RoboMiniMiner}};
    const ShipDesign lightMiner{state.nextShipDesignId,view.player,"Light Orbital Miner",ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5,ShipComponentType::RoboMidgetMiner}};
    const ShipDesign scout{state.nextShipDesignId,view.player,"Orbital Surveyor",ShipHullType::StarsScout,
        {ShipComponentType::QuickJump5,ShipComponentType::RhinoScanner}};
    const ShipDesign founder{state.nextShipDesignId,view.player,"Orbital Settler",ShipHullType::ColonyShip,
        {ShipComponentType::QuickJump5,ShipComponentType::OrbitalConstructionModule}};
    const auto queue_ship=[&](const Planet& p,const ShipDesign& design) {
        const auto existing=std::find_if(state.shipDesigns.begin(),state.shipDesigns.end(),[&](const auto& d){
            return !d.retired && d.owner==view.player && d.name==design.name && d.components==design.components
                && d.hull==design.hull && (design.placements.empty() || d.placements==design.placements);
        });
        if (existing!=state.shipDesigns.end()) {
            plan.orders.orders.push_back(QueueShipDesignOrder{p.id,existing->id});
        } else {
            // Two yards can select different fits with the same hull, engine and weapon name.
            const auto matching = std::find_if(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const auto& order) {
                const auto* create = std::get_if<CreateShipDesignOrder>(&order);
                return create && create->hull == design.hull && create->components == design.components
                    && create->placements == design.placements;
            });
            auto name = matching == plan.orders.orders.end()
                ? design.name : std::get<CreateShipDesignOrder>(*matching).name;
            if (matching == plan.orders.orders.end()) {
                const auto name_taken = [&](const std::string& candidate) {
                    return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [&](const auto& d) {
                        return !d.retired && d.owner == view.player && d.name == candidate;
                    }) || std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const auto& order) {
                        const auto* create = std::get_if<CreateShipDesignOrder>(&order);
                        return create && create->name == candidate;
                    });
                };
                for (unsigned suffix = 2; name_taken(name); ++suffix)
                    name = design.name.substr(0, 40) + " " + std::to_string(suffix);
                plan.orders.orders.push_back(CreateShipDesignOrder{name,design.hull,design.components,design.placements});
            }
            plan.orders.orders.push_back(QueueShipDesignOrder{p.id,0,name});
        }
        explain(reason,"Build paid "+design.name+" at colony "+std::to_string(p.id));
    };
    const auto build=[&](const Planet& p,const ShipDesign& design,MineralCargo reserve=MineralCargo{}) {
        if(!shipyard_can_build(state,p,design) || !ship_design_valid(design)
            || !ship_design_available_to_player(state,view.player,design))return false;
        const auto quote=native_orbital_support_quote(state,p);auto stock=p.minerals;
        stock.ironium-=quote.mineralUnitsPaid;stock.boranium-=quote.mineralUnitsPaid;stock.germanium-=quote.mineralUnitsPaid;
        if(!quote.output)return false;
        // Keep a paid exploration reserve and cover a conservative current-rate
        // deficit while this fit is built. No future turns or hidden deposits.
        const auto years=std::ceil(double(ship_design_cost(design))/std::max(1.0,double(quote.output)*(100-allocation)/100));
        const auto residents=p.nativeOrbit.population+std::max<std::int64_t>(0,projected_native_population_change(state,p));
        const auto bill=double((residents+kNativeSupportPopulationPerUnit-1)/kNativeSupportPopulationPerUnit);
        const auto concentration=p.observedConcentration.value_or(MineralCargo{25,25,25});
        const auto* at=find_star(state,p.star);double producing=0;
        for(const auto& f:state.fleets)if(f.owner==view.player && f.task==FleetTask::RemoteMining && !f.destination
            && at && same_position(f.position,at->position))producing+=mining_rate(state,f);
        const auto runway=[&](double c){return std::max(0.0,bill-producing*c/100)*std::max(0.0,years-1);};
        auto cost=ship_design_mineral_cost(design);
        cost.ironium+=reserve.ironium+runway(concentration.ironium);
        cost.boranium+=reserve.boranium+runway(concentration.boranium);
        cost.germanium+=reserve.germanium+runway(concentration.germanium);
        if(!mineral_cargo_sufficient(stock,cost))return false;
        queue_ship(p,design);return true;
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
            // A self-sustaining full habitat can fund its scout from mining.
            // Otherwise protect exploration before growth absorbs that income.
            const auto* habitat=native_orbital_station(state,*p);
            const auto fullBill=double((native_orbital_capacity(habitat->hull)+kNativeSupportPopulationPerUnit-1)/kNativeSupportPopulationPerUnit);
            const auto& replacement = shipyard_can_build(state, *p, miner) ? miner : lightMiner;
            const bool canFundLater=(assigned_rate(*p)+ship_design_remote_mining_rate(replacement))*support_concentration(*p)/100>fullBill;
            const auto exploration=scouts || canFundLater ? MineralCargo{} : ship_design_mineral_cost(scout);
            if(build(*p,miner,exploration) || build(*p,lightMiner,exploration)){++miners;continue;}
        }
        if(!scouts && build(*p,scout)){++scouts;continue;}
        if(!founders && !shortage && !pendingMiner) {
            // Candidate ownership/safety is known independently of a future hull.
            const bool target=std::any_of(state.planets.begin(),state.planets.end(),[&](const auto& t){
                const auto owner=known_planet_owner(state,view.player,t.id);
                const auto dose=native_orbital_radiation_status(state,view.player,t.star,1000,native_orbital_founding_loadout());
                return owner && !*owner && dose && !dose->annualLoss && !occupied(t.star,ComputerFleetGoalKind::Colonize);
            });
            if(target) {
                // A new orbital colony needs a real miner before its support stock runs out.
                if (can_spare_miner()) {
                    if (build(*p, founder)) ++founders;
                } else if (build(*p, miner) || build(*p, lightMiner)) {
                    ++miners;
                }
                continue; // Save for the first settler before discretionary refits.
            }
        }
        if(shortage || pendingMiner || miners<2 || !scouts)continue;
        const auto work=station_improvement(state,*p);
        std::optional<ShipDesign> guard;
        if (!work && !covered.contains(p->star) && std::find(threatened.begin(), threatened.end(), star) != threatened.end())
            guard=guard_design(state,*p);
        if (work || guard) {
            const auto quote=native_orbital_support_quote(state,*p);
            if(quote.service!=HabitatService::Normal || !quote.output)continue;
            const auto workUnits=work ? station_build_cost(work->loadout) : ship_design_cost(*guard);
            const auto years=std::ceil(double(workUnits)
                /std::max(1.0,double(quote.output)*(100-allocation)/100));
            const auto* current=native_orbital_station(state,*p);
            const auto bill=double((native_orbital_capacity(current->hull)+kNativeSupportPopulationPerUnit-1)
                /kNativeSupportPopulationPerUnit);
            const auto completionBill=work ? double((native_orbital_capacity(work->loadout.hull)
                +kNativeSupportPopulationPerUnit-1)/kNativeSupportPopulationPerUnit) : bill;
            double producing=0;
            // Telemetry can predate last year's dispatch. Retained assignments
            // are HQ intent even when the received fleet still appears to mine here.
            for(const auto& f:state.fleets)if(f.owner==view.player && f.task==FleetTask::RemoteMining
                && !f.destination && f.pendingCommands.empty() && !departing.contains(f.id)
                && std::none_of(goals.begin(),goals.end(),[&](const auto& g){return g.fleet==f.id && g.target!=p->star;})
                && same_position(f.position,star->position))producing+=mining_rate(state,f);
            const auto concentration=p->observedConcentration.value_or(MineralCargo{});
            // The existing hull may fill during construction. Kits refill up
            // to one old-hull bill/year, only until the finite reserve is full.
            const auto remainingYears=std::max(0.0,years-1);
            const auto missingKits=std::max(0.0,kNativeSupportReserveBills*bill-quote.reserveAfter);
            const auto fillingYears=std::min(remainingYears,std::floor(missingKits/bill));
            auto cost=work ? station_mineral_cost(work->loadout) : ship_design_mineral_cost(*guard);
            const auto reserve=[&](double c) {
                const auto deficit=[&](double n){return (bill-producing*c/100)*n+std::min(missingKits,bill*n);};
                // Peak cumulative shortfall occurs at the horizon or either
                // side of the kit-fill boundary. No simulated future turns.
                return quote.mineralUnitsPaid+completionBill+std::max({0.0,deficit(remainingYears),
                    deficit(fillingYears),deficit(std::min(remainingYears,fillingYears+1))});
            };
            cost.ironium+=reserve(concentration.ironium);cost.boranium+=reserve(concentration.boranium);
            cost.germanium+=reserve(concentration.germanium);
            if(!mineral_cargo_sufficient(p->minerals,cost))continue;
            if (work) {
                plan.orders.orders.push_back(QueueStationOrder{p->id,*work});
                explain(reason,"Refit paid native station at colony "+std::to_string(p->id));
            } else queue_ship(*p,*guard);
        }
    }
}
}
