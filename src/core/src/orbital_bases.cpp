#include "suns/orbital_bases.hpp"
#include "suns/communications.hpp"
#include "suns/deep_space.hpp"
#include "suns/mobile_base.hpp"
#include "suns/stations.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/campaign.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace suns {
namespace {
bool pay(Fleet& base, MineralCargo bill)
{
    if (!mineral_cargo_sufficient(base.minerals, bill)) return false;
    base.minerals.ironium -= bill.ironium;
    base.minerals.boranium -= bill.boranium;
    base.minerals.germanium -= bill.germanium;
    return true;
}
bool kit_design(const ShipDesign& d)
{
    return d.hull == ShipHullType::ExpeditionBase && ship_design_valid(d)
        && std::count(d.components.begin(), d.components.end(), ShipComponentType::OrbitalShipyardKit) == 1;
}
}
const Fleet* orbital_base_at(const GameState& s, PlanetId planet)
{
    for (const auto& f : s.fleets)
        if (f.outpost == OutpostKind::OrbitalShipyard && f.orbitalBase.planet == planet) return &f;
    return nullptr;
}
std::uint8_t orbital_background(const StarSystem& star)
{
    // A fixed system property, independent of surface terraforming/magnetospheres.
    constexpr int background[]{90, 75, 60, 45, 30, 20};
    const auto kind = std::min(std::size_t(star.stellarClass), std::size(background)-1);
    return std::min(100, background[kind]);
}
std::uint8_t orbital_screen(const GameState& s, const Fleet& f)
{
    std::uint8_t protection = 0;
    for (auto stack : fleet_ship_stacks(f)) if (const auto* d = find_ship_design(s, stack.design))
        for (auto c : d->components) {
            if (c == ShipComponentType::BulkRadiationScreen) protection = std::max<int>(protection, 20);
            if (c == ShipComponentType::CompositeRadiationScreen) protection = std::max<int>(protection, 40);
            if (c == ShipComponentType::AdvancedRadiationScreen) protection = std::max<int>(protection, 60);
        }
    return protection;
}
std::uint64_t orbital_radiation_losses(const GameState& s, const Fleet& f)
{
    const auto* owner = find_player(s, f.owner);
    if (!owner || owner->race.radiationImmune) return 0;
    const auto exposure = std::max(0, int(f.orbitalBase.background) - orbital_screen(s, f));
    const auto excess = std::max(0, exposure - int(owner->race.habitableRadiation.maximum));
    if (!excess || !f.colonists) return 0;
    // 0.2% per excess unit/year, capped at 20%; ceil in whole people.
    const auto rate = std::min(excess, 100);
    return (f.colonists / 500) * rate + ((f.colonists % 500) * rate + 499) / 500;
}
std::string orbital_base_status(const GameState& s, const Fleet& f)
{
    if (f.outpost != OutpostKind::OrbitalShipyard) return "Mobile orbital constructor";
    if (!f.colonists) return "Empty: deliver workers";
    if (f.damagePercent >= 100) return "Disabled by damage";
    if (!f.orbitalBase.supported) return "Life support unavailable: emergency reserves / population losses";
    if (!f.orbitalBase.active) return "Construction paused";
    if (f.orbitalBase.assemblyYears < kOrbitalAssemblyYears) return "Assembling: workers and I/B/G 20/10/20 per annual stage";
    if (f.orbitalBase.queue.empty()) return "Operational: queue a ship or refit";
    const auto* d = find_ship_design(s, f.orbitalBase.queue.front().design);
    if (!d || !ship_design_available_to_player(s, f.owner, *d)) return "Waiting for design technology";
    if (f.minerals.ironium<1 || f.minerals.boranium<1 || f.minerals.germanium<1) return "Building / waiting for delivered minerals";
    return "Building";
}
bool orbital_base_command_valid(const OrbitalBaseCommand& c)
{
    if (c.action > OrbitalBaseAction::Refit) return false;
    switch (c.action) {
    case OrbitalBaseAction::Deploy: return c.planet && !c.design && !c.item;
    case OrbitalBaseAction::QueueShip: case OrbitalBaseAction::Refit: return c.design && !c.planet;
    case OrbitalBaseAction::Cancel: return !c.planet && !c.design && c.item;
    case OrbitalBaseAction::SetActive: return !c.planet && !c.design && !c.item;
    }
    return false;
}
bool submit_orbital_base_command(GameState& s, PlayerId owner, FleetId id, const OrbitalBaseCommand& c)
{
    auto it = std::find_if(s.fleets.begin(), s.fleets.end(), [=](const auto& f) { return f.id == id && f.owner == owner; });
    if (it == s.fleets.end() || !orbital_base_command_valid(c)) return false;
    // Only owner-authored intent is accepted here. Hidden deployment conditions
    // are checked on reception, never exposed as an immediate validation oracle.
    if (c.design) {
        const auto* d = find_ship_design(s, c.design);
        if (!d || d->owner != owner) return false;
    }
    record_command_transmission(s, owner, it->position, s.turn);
    const auto delay = communication_delay_turns(s, owner, it->position);
    if (!delay) apply_orbital_base_command(s, *it, c);
    else {
        PendingFleetCommand packet;
        packet.issuedTurn = s.turn; packet.deliveryTurn = s.turn + delay; packet.orbitalBase = c;
        it->pendingCommands.push_back(packet);
    }
    return true;
}
void apply_orbital_base_command(GameState& s, Fleet& f, const OrbitalBaseCommand& c)
{
    if (!orbital_base_command_valid(c)) return;
    if (c.action == OrbitalBaseAction::Deploy) {
        const auto* p = find_planet(s, c.planet);
        const auto* star = p ? find_star(s, p->star) : nullptr;
        const auto stacks = fleet_ship_stacks(f);
        const auto* d = stacks.empty() ? nullptr : find_ship_design(s, stacks.front().design);
        bool ok = star && d && kit_design(*d) && fleet_ship_count(f) == 1
            && f.outpost == OutpostKind::None && !f.destination && !f.targetFleet && f.waypointQueue.empty()
            && !f.repeatOrders && f.damagePercent < 100 && same_position(f.position, star->position)
            && (p->owner == 0 || p->owner == f.owner) && !find_orbital_station_at_planet(s, p->id)
            && !orbital_base_at(s, p->id);
        if (ok) for (const auto& item : p->productionQueue)
            if (item.kind == ProductionKind::OrbitalStation) ok = false;
        if (ok) {
            f.design = d->id;
            f.outpost = OutpostKind::OrbitalShipyard;
            f.orbitalBase = {}; f.orbitalBase.planet = p->id;
            f.orbitalBase.background = orbital_background(*star);
            f.orbitalBase.reserveYears = 3; // Paid life-support hardware/reserve in the kit.
            f.task = FleetTask::None; f.arrivalAction.reset(); f.routeTemplate.clear();
            f.probe = {}; f.visitor = {};
        }
        queue_player_report(s, f.owner, ok ? PlayerReportKind::OutpostDeployed : PlayerReportKind::OutpostDeploymentFailed,
            f.position, s.turn, 0, 0, f.id, 0, ProductionKind::ColonyShip, std::uint32_t(OutpostKind::OrbitalShipyard));
        return;
    }
    if (f.outpost != OutpostKind::OrbitalShipyard) return;
    if (c.action == OrbitalBaseAction::SetActive) { f.orbitalBase.active = c.active; return; }
    if (c.action == OrbitalBaseAction::Cancel) {
        auto& base=f.orbitalBase;
        // A faster newer cancel may overtake its original slow build packet.
        // Keep the receipt even if the job has not arrived yet.
        if (!std::binary_search(base.acceptedItems.begin(),base.acceptedItems.end(),c.item)) {
            if (base.acceptedItems.size()>=100000) return;
            base.acceptedItems.insert(std::lower_bound(base.acceptedItems.begin(),base.acceptedItems.end(),c.item),c.item);
            base.lastItemId=std::max(base.lastItemId,c.item);
        }
        std::erase_if(base.queue, [&](const auto& item) { return item.id == c.item; });
        return;
    }
    const auto* d = find_ship_design(s, c.design);
    if (!d || !ship_design_available_to_player(s, f.owner, *d) || f.orbitalBase.queue.size() >= 128) return;
    if (c.action == OrbitalBaseAction::Refit) {
        if (!kit_design(*d) || std::any_of(f.orbitalBase.queue.begin(), f.orbitalBase.queue.end(), [](auto i) { return i.refit; })) return;
    }
    auto& base=f.orbitalBase;
    if (base.acceptedItems.size()>=100000 || (c.item && std::binary_search(base.acceptedItems.begin(),base.acceptedItems.end(),c.item))) return;
    if (!c.item && base.lastItemId==std::numeric_limits<std::uint64_t>::max()) return;
    const auto id=c.item ? c.item : base.lastItemId+1;
    base.lastItemId=std::max(base.lastItemId,id);
    base.acceptedItems.insert(std::lower_bound(base.acceptedItems.begin(),base.acceptedItems.end(),id),id);
    base.queue.push_back({c.design, 0, c.action == OrbitalBaseAction::Refit,id});
}
void service_orbital_bases(GameState& s, std::uint64_t turn)
{
    std::vector<FleetId> ids;
    for (const auto& f : s.fleets) if (f.outpost == OutpostKind::OrbitalShipyard) ids.push_back(f.id);
    std::sort(ids.begin(), ids.end());
    for (const auto id : ids) {
        // Defer fleet insertion until the base reference is no longer in use.
        auto& f = *std::find_if(s.fleets.begin(), s.fleets.end(), [=](auto& f) { return f.id == id; });
        auto& b = f.orbitalBase;
        if (b.servicedTurn >= turn) continue;
        const bool previouslySupported = b.supported;
        const auto report = [&](std::uint32_t status) {
            queue_player_report(s, f.owner, PlayerReportKind::OrbitalBaseStatus, f.position, turn,
                0, b.planet, f.id, 0, ProductionKind::OrbitalStation, status,
                ResearchField::Electronics, 0, {}, b.populationLost);
        };
        b.servicedTurn = turn; b.populationLost = 0; b.supported = false;
        b.exposure = std::max(0, int(b.background) - orbital_screen(s, f));
        if (!f.colonists) { if (previouslySupported) report(2); continue; }
        if (f.damagePercent < 100 && f.fuel >= 1 && pay(f, {1, 1, 1})) {
            f.fuel -= 1; b.supported = true;
            if (b.reserveYears < 3 && f.fuel >= 1 && pay(f, {1, 1, 1})) { f.fuel -= 1; ++b.reserveYears; }
        } else if (b.reserveYears) --b.reserveYears;
        else { b.populationLost = f.colonists / 10 + (f.colonists % 10 != 0); f.colonists -= b.populationLost; }
        const auto radiation = orbital_radiation_losses(s, f);
        b.populationLost += radiation; f.colonists -= radiation;
        if (b.populationLost || b.supported != previouslySupported) report(b.supported ? 1 : 0);
        if (!b.supported || !b.active || !f.colonists) continue;
        if (b.assemblyYears < kOrbitalAssemblyYears) {
            if (f.colonists >= 5000 && pay(f, {20, 10, 20})) {
                ++b.assemblyYears;
                if (b.assemblyYears == kOrbitalAssemblyYears)
                    queue_player_report(s, f.owner, PlayerReportKind::ProductionCompleted, f.position, turn,
                        0, 0, f.id, f.design, ProductionKind::OrbitalStation, 1);
            }
            continue;
        }
        if (b.queue.empty()) continue;
        auto& item = b.queue.front();
        const auto* design = find_ship_design(s, item.design);
        if (!design || !ship_design_available_to_player(s, f.owner, *design)) continue;
        const auto cost = ship_design_cost(*design);
        const auto budget = std::uint32_t(std::min(f.colonists, kOrbitalWorkforce) * kOrbitalAnnualWork / kOrbitalWorkforce);
        // Materials are charged cumulatively in whole kt as work is done.
        // This permits refits larger than the hold, supplied over many years,
        // and makes cancellation/reload unable to refund or repeat a debit.
        const auto bill = ship_design_mineral_cost(*design);
        const auto due = [cost](double mineral, std::uint32_t work) {
            return cost ? std::ceil(mineral * work / cost) : mineral;
        };
        for (std::uint32_t spent=0; spent<budget && item.work<cost; ++spent) {
            const MineralCargo step{
                due(bill.ironium,item.work+1)-due(bill.ironium,item.work),
                due(bill.boranium,item.work+1)-due(bill.boranium,item.work),
                due(bill.germanium,item.work+1)-due(bill.germanium,item.work)};
            if (!pay(f,step)) break;
            ++item.work;
        }
        if (item.work < cost) continue;
        // A refit may not discard cargo or fuel to fit a smaller receiving design.
        if (item.refit && (ship_design_cargo_capacity(*design) + 1e-6 < fleet_cargo_used(s, f)
            || ship_design_fuel_capacity(*design) + 1e-6 < f.fuel)) continue;
        const auto completed = item; b.queue.erase(b.queue.begin());
        if (completed.refit) { f.design = design->id; f.ships = {{design->id, 1}}; report(3); }
        else {
            Fleet ship;
            ship.id = s.nextFleetId++; ship.owner = f.owner; ship.design = design->id; ship.ships = {{design->id, 1}};
            ship.orbitalShipyardBirth = true;
            ship.name = design->name; ship.position = f.position; ship.fuel = 0;
            queue_player_report(s, f.owner, PlayerReportKind::ProductionCompleted, f.position, turn,
                0, 0, ship.id, design->id, ProductionKind::ColonyShip, 1);
            s.fleets.push_back(std::move(ship)); // No free initial fuel or minerals.
        }
    }
}
} // namespace suns

// A deliberately small honest planner: one expedition, received targets only.
namespace suns {
void plan_orbital_bases(const PlayerView& view, PlayerOrders& orders, ComputerState& memory)
{
    const auto& s=view.state;
    // An empty old queue is not acknowledgement. Only a received durable
    // receipt plus absence of that same job proves completion/cancellation.
    std::erase_if(memory.orbitalBuilds,[&](const auto& intent) {
        const auto* f=find_service_fleet(s,intent.fleet);
        if (!f || f->owner!=view.player || f->outpost!=OutpostKind::OrbitalShipyard) return true;
        const auto& b=f->orbitalBase;
        return std::binary_search(b.acceptedItems.begin(),b.acceptedItems.end(),intent.item)
            && std::none_of(b.queue.begin(),b.queue.end(),[&](auto job){return job.id==intent.item;});
    });
    int plannedProtection=20; // The new AI constructor's Bulk screen.
    for(const auto& f:s.fleets) if(f.owner==view.player && f.outpost==OutpostKind::None
        && fleet_construction_kit(s,f)==OutpostKind::OrbitalShipyard)
        plannedProtection=std::max(plannedProtection,int(orbital_screen(s,f)));
    const Planet* target=nullptr;
    for (const auto& p:s.planets) if (!p.owner && survey_level(s,view.player,p.star)>=SurveyLevel::OrbitalSurvey
        && !orbital_base_at(s,p.id) && !find_orbital_station_at_planet(s,p.id)
        && known_planet_habitability(s,view.player,p.id).value_or(1)<=0) {
        const auto* star=find_star(s,p.star);
        const auto* owner=find_player(s,view.player);
        const bool safe=star && owner && (owner->race.radiationImmune
            || int(orbital_background(*star))-plannedProtection<=owner->race.habitableRadiation.maximum);
        if (safe && (!target || p.id<target->id)) target=&p;
    }
    bool existing=false;
    for (const auto& f:s.fleets) if (f.owner==view.player && fleet_construction_kit(s,f)==OutpostKind::OrbitalShipyard) {
        existing=true;
        if (f.outpost==OutpostKind::OrbitalShipyard) {
            if (memory.orbitalBuilds.size()>=kMaximumComputerFleetGoals
                || std::any_of(memory.orbitalBuilds.begin(),memory.orbitalBuilds.end(),[&](auto i){return i.fleet==f.id;})) continue;
            if (f.orbitalBase.assemblyYears==kOrbitalAssemblyYears && f.orbitalBase.queue.empty()
                && f.orbitalBase.supported && f.colonists>=5000) {
                for (const auto& d:s.shipDesigns) if (d.owner==view.player && d.hull!=ShipHullType::ExpeditionBase
                    && ship_design_available_to_player(s,view.player,d) && !ship_design_can_colonize(d)
                    && mineral_cargo_sufficient(f.minerals,ship_design_mineral_cost(d))) {
                    const auto latest=std::max<std::uint64_t>(f.orbitalBase.lastItemId,s.turn<<32);
                    if (latest==std::numeric_limits<std::uint64_t>::max()) break;
                    const auto item=latest+1;
                    orders.orders.push_back(OrbitalBaseOrder{f.id,{OrbitalBaseAction::QueueShip,0,d.id,item}});
                    memory.orbitalBuilds.push_back({f.id,d.id,item,s.turn});
                    std::sort(memory.orbitalBuilds.begin(),memory.orbitalBuilds.end(),[](auto a,auto b){return a.fleet<b.fleet;});
                    break;
                }
            }
            continue;
        }
        if (f.destination || !f.pendingCommands.empty() || !target) continue;
        const auto* star=find_star(s,target->star); if (!star) continue;
        if (same_position(f.position,star->position)) {
            orders.orders.push_back(OrbitalBaseOrder{f.id,{OrbitalBaseAction::Deploy,target->id}}); continue;
        }
        const MineralCargo supplies{220,120,220};
        if (f.colonists<kOrbitalWorkforce || !mineral_cargo_sufficient(f.minerals,supplies)) {
            if (f.telemetry.observedTurn!=s.turn) continue;
            for (const auto& p:s.planets) if (p.owner==view.player && p.population>kOrbitalWorkforce+1) {
                const auto* home=find_star(s,p.star);
                const MineralCargo missing{std::max(0.0,supplies.ironium-f.minerals.ironium),std::max(0.0,supplies.boranium-f.minerals.boranium),std::max(0.0,supplies.germanium-f.minerals.germanium)};
                const auto people=f.colonists<kOrbitalWorkforce?kOrbitalWorkforce-f.colonists:0;
                if (home && same_position(home->position,f.position) && mineral_cargo_sufficient(p.minerals,missing)
                    && fleet_cargo_used(s,f)+mineral_cargo_mass(missing)+colonist_cargo_mass(people)<=fleet_cargo_capacity(s,f)) {
                    orders.orders.push_back(TransferCargoOrder{{p.id,0},{0,f.id},people,missing}); break;
                }
            }
            continue;
        }
        auto exposure=f; exposure.orbitalBase.background=orbital_background(*star);
        if (!orbital_radiation_losses(s,exposure) && (fleet_fuel_consumption_for_distance(s, f, distance_between(f.position,star->position)) <= std::max(0.0,f.fuel-20)))
            orders.orders.push_back(MoveFleetOrder{f.id,star->position,5});
    }
    if (existing || !target) return;
    ShipDesign design; design.owner=view.player; design.name="Orbital Shipyard Constructor";
    design.hull=ShipHullType::ExpeditionBase;
    design.components={ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,
        ShipComponentType::OrbitalShipyardKit,ShipComponentType::BulkRadiationScreen,ShipComponentType::RelayArray};
    if (!ship_design_available_to_player(s,view.player,design)) return;
    const auto* star=find_star(s,target->star);
    // The initial design's screen subtracts 20; do not send workers to a harmful site.
    const auto* owner=find_player(s,view.player);
    if (!star || !owner || (!owner->race.radiationImmune && int(orbital_background(*star))-20>owner->race.habitableRadiation.maximum)) return;
    for (const auto& p:s.planets) if (p.owner==view.player && p.productionQueue.empty()
        && colony_has_orbital_service(s,p.id,view.player,OrbitalStationModule::Shipyard)
        && mineral_cargo_sufficient(p.minerals,ship_design_mineral_cost(design))) {
        bool queued=false;
        for (const auto& o:orders.orders) {
            if (const auto* q=std::get_if<QueueShipDesignOrder>(&o); q && q->colony==p.id) queued=true;
            if (const auto* q=std::get_if<QueueProductionOrder>(&o); q && q->colony==p.id) queued=true;
        }
        for (const auto& planet:s.planets) for (const auto& item:planet.productionQueue)
            if (const auto* d=find_ship_design(s,item.shipDesign); d && std::find(d->components.begin(),d->components.end(),ShipComponentType::OrbitalShipyardKit)!=d->components.end()) queued=true;
        if (queued) continue;
        const auto it=std::find_if(s.shipDesigns.begin(),s.shipDesigns.end(),[&](const auto& d){return d.owner==view.player && d.name==design.name;});
        if(it!=s.shipDesigns.end()) {
            if(it->hull!=design.hull || it->components!=design.components) return;
            orders.orders.push_back(QueueShipDesignOrder{p.id,it->id});
        } else {
            orders.orders.push_back(CreateShipDesignOrder{design.name,design.hull,design.components});
            orders.orders.push_back(QueueShipDesignOrder{p.id,0,design.name});
        }
        break;
    }
}
} // namespace suns
