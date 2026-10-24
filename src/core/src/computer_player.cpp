#include "suns/orbital_habitats.hpp"
#include "suns/computer_player.hpp"
#include "suns/combat.hpp"
#include "suns/mobile_base.hpp"
#include "suns/survey_probes.hpp"
#include "suns/intergalactic_visitors.hpp"
#include "suns/planetary_recovery.hpp"
#include "suns/unknown_objects.hpp"
#include "suns/science_samples.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/interstellar_medium.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/communications.hpp"
#include "suns/terraforming.hpp"
#include "suns/production.hpp"
#include "suns/stations.hpp"
#include "computer_policy.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace suns {
namespace computer_detail {

constexpr std::uint64_t kMinimumSettlers = 100;

bool idle(const Fleet& fleet)
{
    return !fleet.visitor.track.id && !fleet.probe.enabled && !fleet.destination && fleet.waypointQueue.empty() && !fleet.repeatOrders
        && fleet.targetFleet == 0 && fleet.task == FleetTask::None;
}

// A projected dot reaching the target is not an arrival acknowledgement.
bool confirmed_idle(const Fleet& fleet, std::uint64_t issuedTurn)
{
    return fleet.telemetry.observedTurn > issuedTurn && !fleet.telemetry.destination
        && fleet.telemetry.waypointQueue.empty() && !fleet.telemetry.repeatOrders
        && fleet.telemetry.targetFleet == 0 && fleet.telemetry.task == FleetTask::None;
}

const Planet* colony_at(const GameState& state, PlayerId player, Position position)
{
    for (const auto& planet : state.planets) {
        const auto* star = find_star(state, planet.star);
        if (planet.owner == player && star && same_position(position, star->position)) return &planet;
    }
    return nullptr;
}

// Host depot service happens before orders, including active programmes.
// Credit only received service at the confirmed initial position, never a
// projected future arrival. An active programme needs fresh telemetry; an
// acknowledged idle fleet retains its established stationary-depot treatment.
void budget_initial_depot_service(const GameState& received,Fleet& fleet)
{
    if (!fleet.telemetry.observedTurn || !same_position(fleet.position,fleet.telemetry.position)
        || (fleet.telemetry.observedTurn!=received.turn && !confirmed_idle(fleet,0))) return;
    if (const auto* local=colony_at(received,fleet.owner,fleet.position);
        local && colony_has_orbital_service(received,local->id,fleet.owner,OrbitalStationModule::RefuelingDepot))
        fleet.fuel=fleet_fuel_capacity(received,fleet);
}

std::optional<GateFleetOrder> gate_staging_order(
    const GameState& received, const Fleet& fleet, Position target, bool requireRefueling, const PlayerOrders* pending)
{
    if (pending && std::any_of(pending->orders.begin(), pending->orders.end(), [&](const auto& order) {
        const auto* gate = std::get_if<GateFleetOrder>(&order); return gate && gate->fleet == fleet.id;
    })) return {};
    auto state = pending ? projected_cargo_order_state(received, *pending) : received;
    const auto ownFleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == fleet.id; });
    if (ownFleet == state.fleets.end()) return {};
    *ownFleet = fleet; // Includes the ordinary cargo loading planned by this caller.
    const auto* player = find_player(state, fleet.owner);
    if (!player || player->race.primaryTrait != PrimaryRaceTrait::StargateSpecialist) return {};
    const auto* source = colony_at(state, fleet.owner, fleet.position);
    if (!source || !stargate_station_available(state, fleet.owner, source->id)) return {};
    const auto* from = station_stargate(*find_orbital_station_at_planet(state, source->id));
    const auto mass = fleet_gross_mass(state, fleet); // Includes settlers loaded earlier in this plan.
    if (from->maximumMassKt && mass > from->maximumMassKt) return {};
    const Planet* best = nullptr;
    bool bestResonant = false;
    auto bestDistance = distance_between(fleet.position, target);
    const auto minimumGain = std::max(1.0, warp_distance(fleet.warp));
    for (const auto& planet : state.planets) {
        if (requireRefueling && !colony_has_orbital_service(state, planet.id,
                fleet.owner, OrbitalStationModule::RefuelingDepot)) continue;
        const auto* star = find_star(state, planet.star);
        if (!star) continue;
        const bool resonant = !stargate_transfer_valid(state, fleet.owner, fleet.id, source->id, planet.id);
        if (resonant) {
            auto preview = state;
            if (!preview_stargate_transfer(preview, fleet.owner, {fleet.id, source->id, planet.id, true})) continue;
            // A scarce pulse must save a substantial ordinary flight leg.
            if (distance_between(fleet.position, target) - distance_between(star->position, target)
                < std::max(160.0, 2 * minimumGain)) continue;
        }
        const auto* gate = station_stargate(*find_orbital_station_at_planet(state, planet.id));
        if (gate->maximumMassKt && mass > gate->maximumMassKt) continue;
        const auto remaining = distance_between(star->position, target);
        if (remaining + minimumGain < distance_between(fleet.position, target)
            && (!best || remaining < bestDistance || (remaining == bestDistance && planet.id < best->id))) {
            best = &planet; bestDistance = remaining; bestResonant = resonant;
        }
    }
    if (!best) return {};
    return GateFleetOrder{fleet.id, source->id, best->id, bestResonant};
}

double leg_fuel(const GameState& state, const Fleet& fleet, Position destination)
{
    // The host rounds fuel separately for each year's movement, including the
    // short final leg. Billing the whole route once would under-budget Stars engines.
    const auto step = warp_distance(fleet.warp);
    const auto distance=distance_between(fleet.position,destination);
    const auto years=std::floor(distance/step);
    // The conservative uniform-density envelope is identical in every full
    // movement year. Sum it without simulating thousands of candidate years.
    return years*fleet_segment_fuel_bounds(state,fleet,step).consumedMaximum
        +fleet_segment_fuel_bounds(state,fleet,std::max(0.0,distance-years*step)).consumedMaximum;
}

std::optional<MediumRouteAlternative> navigation_leg(const GameState& state,const Fleet& fleet,Position destination)
{
    const auto* observer=find_player(state,fleet.owner);
    if (!state.interstellarMediumEnabled || !observer || observer->mediumKnowledge.empty()) {
        const auto needed=leg_fuel(state,fleet,destination);
        if (fleet_overdrive_damage_rate(state,fleet,fleet.warp)>1e-8 || needed>fleet.fuel+1e-6) return {};
        const auto distance=distance_between(fleet.position,destination);
        return MediumRouteAlternative{{destination},distance,distance/warp_distance(fleet.warp),needed,0};
    }
    std::optional<MediumRouteAlternative> best;
    double bestScore=std::numeric_limits<double>::infinity();
    for (auto candidate:medium_route_alternatives(state,fleet,destination,fleet.warp)) {
        if (candidate.damage>1e-6) continue;
        double needed=0; auto moving=fleet;
        for (const auto end:candidate.destinations) { needed+=leg_fuel(state,moving,end); moving.position=end; }
        if (needed>fleet.fuel+1e-6) continue;
        const double score=candidate.years+0.5*needed/std::max(1.0,fleet_fuel_capacity(state,fleet));
        if (score<bestScore-1e-8) { candidate.fuel=needed; bestScore=score; best=std::move(candidate); }
    }
    return best;
}

std::optional<std::uint8_t> route_warp(
    const GameState& state, Fleet fleet, const std::vector<Position>& destinations)
{
    if (fleet.damagePercent >= 100.0) return std::nullopt;
    budget_initial_depot_service(state,fleet);
    auto cruise = fleet_max_warp(state, fleet);
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(state, stack.design))
            cruise = std::min(cruise, ship_design_cruise_warp(*design));
    const auto startPosition=fleet.position;
    for (int warp = cruise; warp >= 1; --warp) {
        fleet.warp = static_cast<std::uint8_t>(warp);
        if (!fleet_warp_valid(state, fleet, fleet.warp)) continue;
        double needed = 0;
        auto position = startPosition;
        for (const auto destination : destinations) {
            fleet.position=position;
            const auto leg=navigation_leg(state,fleet,destination);
            if (!leg) { needed=std::numeric_limits<double>::infinity(); break; }
            needed += leg->fuel;
            position = destination;
        }
        // Conservative: collected/generated fuel is a reserve, not advance credit.
        if (needed <= fleet.fuel + 1e-6) return fleet.warp;
    }
    return std::nullopt;
}

std::uint64_t review_turn(const GameState& state, const Fleet& fleet)
{
    return state.turn + 2ULL * communication_delay_turns(state, fleet.owner, fleet.position) + 3;
}

void explain(std::string& reason, const std::string& action)
{
    if (reason.size() + action.size() + 2 <= kMaximumComputerDecisionReasonBytes) {
        if (!reason.empty()) reason += "; ";
        reason += action;
    }
}

bool suitable(const GameState& state, PlayerId player, const Planet& planet)
{
    const auto owner = known_planet_owner(state, player, planet.id);
    const auto hab = known_planet_habitability(state, player, planet.id);
    return owner && *owner == 0 && hab && *hab >= 20;
}

std::optional<ShipDesign> civilian_design(const GameState& state, PlayerId player, bool colonizer)
{
    auto evaluation = state;
    const auto temporaryId = state.nextShipDesignId;
    const auto score = [&](ShipDesign design) {
        if (!ship_design_valid(design) || !ship_design_available_to_player(state, player, design))
            return std::numeric_limits<double>::infinity();
        design.id = temporaryId;
        evaluation.shipDesigns.push_back(design);
        Fleet fleet;
        fleet.owner = player; fleet.design = temporaryId; fleet.fuel = ship_design_fuel_capacity(design);
        if (colonizer) fleet.colonists = std::min<std::uint64_t>(2'500,
            static_cast<std::uint64_t>(ship_design_cargo_capacity(design) * kColonistsPerCargoUnit));
        const auto warp = route_warp(evaluation, fleet, colonizer
            ? std::vector<Position>{{250, 0}} : std::vector<Position>{{200, 0}, {0, 0}});
        const auto result = warp && (!colonizer || (fleet.colonists >= kMinimumSettlers && fleet_radiation_safe(evaluation, fleet)))
            ? (colonizer ? 250.0 : 400.0) / warp_distance(*warp) + ship_design_cost(design) / 8.0
                - (colonizer ? 0 : ship_design_sensor_range(design) / 100.0)
            : std::numeric_limits<double>::infinity();
        evaluation.shipDesigns.pop_back();
        return result;
    };
    std::optional<ShipDesign> best;
    double bestScore = std::numeric_limits<double>::infinity();
    for (const auto& design : state.shipDesigns) {
        Fleet example; example.owner = player; example.design = design.id;
        if (design.owner != player || military_fleet(state, example) || ship_design_can_colonize(design) != colonizer
            || (!colonizer && !ship_design_has_scanner(design))) continue;
        const auto candidate = score(design);
        if (candidate < bestScore || (candidate == bestScore && best && design.id < best->id)) {
            best = design; bestScore = candidate;
        }
    }
    if (!best) return std::nullopt;
    std::vector<ShipComponentType> components{ShipComponentType::FuelTank, ShipComponentType::FusionDrive, ShipComponentType::RamScoopDrive,
        ShipComponentType::RadiatingRamScoopDrive};
    for (const auto& unlock : research_unlocks()) if (unlock.component) components.push_back(*unlock.component);
    std::sort(components.begin(), components.end());
    components.erase(std::unique(components.begin(), components.end()), components.end());
    // Improve engines first, then scanner/fuel fitting, using the same access
    // and slot validation as the human designer. Existing hull families stay intact.
    for (const auto kind : {ShipComponentKind::Engine, ShipComponentKind::Scanner, ShipComponentKind::Fuel}) {
        for (const auto component : components) {
            if (component_spec(component).kind != kind || !component_available_to_player(state, player, component)) continue;
            auto candidate = *best;
            bool replaced = false;
            for (auto& fitted : candidate.components) if (component_spec(fitted).kind == kind) {
                fitted = component; replaced = true;
            }
            if (!replaced) {
                if (kind != ShipComponentKind::Fuel) continue;
                candidate.components.push_back(component);
            }
            candidate.placements.clear();
            normalize_ship_design_placement(candidate);
            const auto candidateScore = score(candidate);
            if (candidateScore + 0.01 < bestScore) { best = std::move(candidate); bestScore = candidateScore; }
        }
    }
    const ShipDesign* matching = nullptr;
    for (const auto& existing : state.shipDesigns) if (existing.owner == player && existing.hull == best->hull
        && existing.components == best->components && ship_design_valid(existing)
        && (!matching || existing.id < matching->id)) matching = &existing;
    if (matching) return *matching;
    best->id = 0;
    auto name = std::string(colonizer ? "Colony: " : "Survey: ");
    for (const auto component : best->components) if (component_spec(component).kind == ShipComponentKind::Engine) {
        name += component_spec(component).name; break;
    }
    if (!colonizer) for (const auto component : best->components) if (component_spec(component).kind == ShipComponentKind::Scanner) {
        name += " / " + component_spec(component).name; break;
    }
    if (ship_design_fuel_capacity(*best) > hull_spec(best->hull).baseFuelCapacity) name += " + fuel";
    name.resize(std::min<std::size_t>(name.size(), 40));
    best->name = name;
    for (unsigned suffix = 2; std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(),
        [&](const auto& existing) { return existing.owner == player && existing.name == best->name; }); ++suffix)
        best->name = name + " " + std::to_string(suffix);
    return best;
}

void plan_research(const PlayerView& view, ComputerTurnPlan& plan, std::string& reason)
{
    const auto& player = view.state.players.front();
    const auto& technology = player.technology;
    if (!plan.nextState.initialized && technology.researchAllocationPercent == 0) {
        plan.orders.orders.push_back(SetResearchAllocationOrder{20});
        explain(reason, "Allocate 20% to research");
    }
    if (!technology.researchActive) {
        plan.orders.orders.push_back(SetResearchPlanOrder{
            technology.focus, technology.queuedFocuses, true});
        explain(reason, "Resume research");
    }
    // An existing queue is a standing programme; finish it before choosing a
    // fresh objective. Remember the intended level so overflow RP cannot pin
    // the controller to the same field forever.
    if (!technology.queuedFocuses.empty()) { plan.nextState.researchGoal.reset(); return; }
    auto& goal = plan.nextState.researchGoal;
    if (!goal || goal->field != technology.focus) {
        goal = ComputerResearchGoal{technology.focus, static_cast<std::uint8_t>(
            std::min(255, int(technology.levels[static_cast<std::size_t>(technology.focus)]) + 1))};
        return;
    }
    if (technology.levels[static_cast<std::size_t>(goal->field)] < goal->level) return;
    // Expansion sciences first; ties have a fixed order, independent of storage.
    constexpr std::array fields{ResearchField::Propulsion, ResearchField::Construction,
        ResearchField::Biology, ResearchField::Energy, ResearchField::Electronics, ResearchField::Weapons};
    auto next = fields.front();
    for (const auto field : fields)
        if (technology.levels[static_cast<std::size_t>(field)]
            < technology.levels[static_cast<std::size_t>(next)]) next = field;
    if (player.race.primaryTrait == PrimaryRaceTrait::StargateSpecialist
        && std::count_if(view.state.planets.begin(), view.state.planets.end(),
            [&](const auto& p) { return p.owner == view.player; }) >= 2) {
        // Reach a viable frontier drive first. A functioning network then
        // favours gate sciences instead of racing to top conventional engines.
        const auto level = [&](ResearchField f) { return technology.levels[std::size_t(f)]; };
        if (level(ResearchField::Propulsion) < 3) next = ResearchField::Propulsion;
        else if (level(ResearchField::Biology) < 1) next = ResearchField::Biology;
        else {
            const auto hubs = std::count_if(view.state.planets.begin(), view.state.planets.end(),
                [&](const auto& p) { return stargate_station_available(view.state, view.player, p.id); });
            for (const auto& gate : stargate_technologies()) {
                if (gate.specialistOnly && hubs < 2) break;
                if (level(ResearchField::Construction) < gate.construction) { next = ResearchField::Construction; break; }
                if (level(ResearchField::Energy) < gate.energy) { next = ResearchField::Energy; break; }
                if (level(ResearchField::Electronics) < gate.electronics) { next = ResearchField::Electronics; break; }
            }
        }
    }
    if (hostile_intel(view)) {
        if (technology.levels[static_cast<std::size_t>(ResearchField::Construction)] < 3) next = ResearchField::Construction;
        else if (technology.levels[static_cast<std::size_t>(ResearchField::Weapons)] < 2) next = ResearchField::Weapons;
        else if (technology.levels[static_cast<std::size_t>(ResearchField::Energy)] < 1) next = ResearchField::Energy;
    }
    goal = ComputerResearchGoal{next, static_cast<std::uint8_t>(
        std::min(255, int(technology.levels[static_cast<std::size_t>(next)]) + 1))};
    if (next != technology.focus) {
        plan.orders.orders.push_back(SetResearchPlanOrder{next, {}, true});
        explain(reason, "Research " + research_field_name(next) + " " + std::to_string(goal->level));
    }
}

void plan_exotic_materials(const PlayerView& view, ComputerTurnPlan& plan, std::string& reason)
{
    const auto& s = view.state; auto& goals = plan.nextState.fleetGoals;
    if (!exotic_mining_available(s, view.player)) return;
    std::vector<const Planet*> sites, homes;
    const auto needsCrystals = [&](const Planet& p) {
        return (resonance_technology_available(s, view.player) && !p.resonanceLab.crystalsPaid)
            || (resonant_gate_technology_available(s, view.player) && stargate_station_available(s, view.player, p.id));
    };
    for (const auto& p : s.planets) {
        const auto known = known_xenocrystal(s, view.player, p.id);
        if (!p.owner && known && known->depositSurveyed && (known->site.remaining || known->site.surface)) sites.push_back(&p);
        if (p.owner == view.player && known && known->colony && known->colony->owner == view.player
            && known->colony->population) homes.push_back(&p);
        if (p.owner == view.player && p.population && (p.resonanceLab.started || p.xenocrystal.surface >= kResonanceCrystals)
            && resonance_technology_available(s, view.player) && !engineering_station_available(s, p)
            && std::none_of(p.productionQueue.begin(), p.productionQueue.end(), [](const auto& item) { return item.kind == ProductionKind::OrbitalStation; })) {
            const auto* station = find_orbital_station_at_planet(s, p.id);
            if (!station || station->owner == view.player) {
                StationConstruction support;
                if (station) { support.loadout = station_loadout(*station); support.refitStation = station->id; }
                support.loadout.hull = std::max(support.loadout.hull, OrbitalStationHullType::Starbase);
                support.loadout.shipyard = true;
                if (station_construction_valid(s, p, support) && mineral_cargo_sufficient(p.minerals, station_mineral_cost(support.loadout))) {
                    plan.orders.orders.push_back(QueueStationOrder{p.id, support});
                    explain(reason, "Build orbital support for a resonance laboratory");
                }
            }
        }
        if (p.owner == view.player && p.population && resonance_technology_available(s, view.player)
            && engineering_station_available(s, p) && (p.resonanceLab.started || p.xenocrystal.surface >= kResonanceCrystals)
            && p.resonanceLab.work < kResonanceWork && !resonance_scheduled(p))
            plan.orders.orders.push_back(SetResonanceLaboratoryOrder{p.id});
    }
    for (const auto* p : homes) if (p->xenocrystal.surface && !needsCrystals(*p)
        && std::any_of(homes.begin(), homes.end(), [&](const auto* home) {
            return home->id != p->id && needsCrystals(*home) && home->xenocrystal.surface < kResonanceCrystals;
        })) sites.push_back(p);
    std::erase_if(goals, [&](const auto& g) {
        return g.kind >= ComputerFleetGoalKind::ExoticMining && std::none_of(s.fleets.begin(), s.fleets.end(),
            [&](const auto& f) { return f.id == g.fleet && f.owner == view.player; });
    });
    for (const auto& f : s.fleets) {
        if (f.owner != view.player || f.outpost != OutpostKind::None || mobile_base_hulls(s, f) || military_fleet(s, f)) continue;
        auto goal = std::find_if(goals.begin(), goals.end(), [&](const auto& g) { return g.fleet == f.id; });
        if (goal != goals.end() && goal->kind < ComputerFleetGoalKind::ExoticMining) continue;
        if (!f.telemetry.observedTurn || f.telemetry.destination || f.telemetry.targetFleet) continue;
        if (goal != goals.end() && f.telemetry.observedTurn <= goal->issuedTurn) continue;
        if (goal != goals.end()) {
            const auto* target = find_planet_at_star(s, goal->target);
            if (target && target->owner && (goal->kind == ComputerFleetGoalKind::ExoticMining
                || (target->owner != view.player && !f.xenocrystal))) {
                plan.orders.orders.push_back(SetFleetTaskOrder{f.id, FleetTask::None});
                goals.erase(goal);
                explain(reason, "Retire an exotic mission after received ownership blocks extraction");
                continue;
            }
        }
        if (goal != goals.end() && goal->kind == ComputerFleetGoalKind::ExoticMining) {
            const auto* p = find_planet_at_star(s, goal->target);
            const auto known = p ? known_xenocrystal(s, view.player, p->id) : std::nullopt;
            if (known && !known->site.remaining && known->observedTurn > goal->issuedTurn) {
                plan.orders.orders.push_back(SetFleetTaskOrder{f.id, FleetTask::None});
                goals.erase(goal);
            }
            continue;
        }
        if (f.xenocrystal) {
            const Planet* home = nullptr; std::optional<std::uint8_t> warp; double distance = std::numeric_limits<double>::infinity();
            const auto* origin = goal != goals.end() ? find_planet_at_star(s, goal->target) : nullptr;
            // Prefer received consumers. Already loaded cargo must still reach
            // an own store when demand changes, including returning to origin.
            for (const bool consumersOnly : {true, false}) {
                for (const auto* p : homes) if (const auto* star = find_star(s, p->star)) {
                    if (consumersOnly && ((origin && p->id == origin->id)
                        || !needsCrystals(*p) || p->xenocrystal.surface >= kResonanceCrystals)) continue;
                    const auto candidate = route_warp(s, f, {star->position});
                    const auto d = distance_between(f.position, star->position);
                    if (candidate && (d < distance || (d == distance && home && p->id < home->id))) {
                        home = p; warp = candidate; distance = d;
                    }
                }
                if (home) break;
            }
            if (home) {
                plan.orders.orders.push_back(MoveFleetOrder{f.id, find_star(s, home->star)->position, *warp,
                    {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::Xenocrystal}});
                const ComputerFleetGoal returning{f.id, ComputerFleetGoalKind::ExoticHauling, home->star, s.turn, review_turn(s, f)};
                if (goal == goals.end()) goals.push_back(returning); else *goal = returning;
                explain(reason, "Deliver xenocrystals aboard fleet " + std::to_string(f.id));
            }
            continue;
        }
        if (goal != goals.end()) {
            const auto* p = find_planet_at_star(s, goal->target);
            if (p && p->owner == view.player && confirmed_idle(f, goal->issuedTurn)) { goals.erase(goal); goal = goals.end(); }
            else if (p && !p->owner) {
                const auto known = known_xenocrystal(s, view.player, p->id);
                if (known && known->site.surface && same_position(f.position, find_star(s, p->star)->position)) {
                    plan.orders.orders.push_back(MoveFleetOrder{f.id, f.position, 1,
                        {FleetArrivalActionKind::LoadAllAvailable, 1, FleetCargoKind::Xenocrystal}});
                    goal->issuedTurn = s.turn; goal->reviewTurn = review_turn(s, f);
                } else if (known && !known->site.remaining && !known->site.surface) goals.erase(goal);
                continue;
            } else continue;
        }
        if (!idle(f) || fleet_can_colonize(s, f) || !f.scienceSamples.empty()) continue;
        const bool miner = xenocrystal_mining_rate(s, f) > 0;
        if (!miner && fleet_cargo_capacity(s, f) - fleet_cargo_used(s, f) < 1) continue;
        const Planet* target = nullptr; std::optional<std::uint8_t> warp; double distance = std::numeric_limits<double>::infinity();
        for (const auto* p : sites) {
            const auto known = known_xenocrystal(s, view.player, p->id);
            if (miner ? (p->owner || !known->site.remaining) : !known->site.surface) continue;
            if (std::any_of(goals.begin(), goals.end(), [&](const auto& g) { return g.target == p->star
                && g.kind == (miner ? ComputerFleetGoalKind::ExoticMining : ComputerFleetGoalKind::ExoticHauling); })) continue;
            const auto* star = find_star(s, p->star); if (!star) continue;
            auto loaded = f;
            if (!miner) loaded.xenocrystal += std::uint32_t(std::min(double(known->site.surface), std::floor(fleet_cargo_capacity(s, f) - fleet_cargo_used(s, f))));
            const bool returnable = std::any_of(homes.begin(), homes.end(), [&](const auto* home) {
                return route_warp(s, loaded, {star->position, find_star(s, home->star)->position}).has_value();
            });
            const auto candidate = returnable ? route_warp(s, f, {star->position}) : std::nullopt;
            const auto d = distance_between(f.position, star->position);
            if (candidate && d < distance) { target = p; warp = candidate; distance = d; }
        }
        if (target) {
            plan.orders.orders.push_back(MoveFleetOrder{f.id, find_star(s, target->star)->position, *warp,
                {miner ? FleetArrivalActionKind::RemoteMining : FleetArrivalActionKind::LoadAllAvailable, 1, FleetCargoKind::Xenocrystal}});
            goals.push_back({f.id, miner ? ComputerFleetGoalKind::ExoticMining : ComputerFleetGoalKind::ExoticHauling,
                target->star, s.turn, review_turn(s, f)});
            explain(reason, miner ? "Mine a surveyed xenocrystal deposit" : "Collect a received xenocrystal cache");
        }
    }
    if (sites.empty()) return;
    const bool needsMiner = std::any_of(sites.begin(), sites.end(), [&](const auto* p) {
        return !p->owner && p->xenocrystal.remaining;
    });
    const bool minerExists = !needsMiner || std::any_of(s.fleets.begin(), s.fleets.end(), [&](const auto& f) { return f.owner == view.player && xenocrystal_mining_rate(s, f); });
    const bool haulerExists = std::any_of(s.fleets.begin(), s.fleets.end(), [&](const auto& f) { return f.owner == view.player && !fleet_can_colonize(s, f)
        && !xenocrystal_mining_rate(s, f) && fleet_cargo_capacity(s, f) >= kResonanceCrystals; });
    if (minerExists && haulerExists) return;
    const auto engine = component_available_to_player(s, view.player, ShipComponentType::FuelMizer)
        ? ShipComponentType::FuelMizer : ShipComponentType::QuickJump5;
    ShipDesign design{0, view.player, minerExists ? "AI Xenocrystal Transport" : "AI Xenocrystal Miner",
        minerExists ? ShipHullType::SmallFreighter : ShipHullType::MiniMiner,
        minerExists ? std::vector<ShipComponentType>{engine} : std::vector<ShipComponentType>{engine, ShipComponentType::RoboMiniMiner}};
    for (const auto& existing : s.shipDesigns) if (existing.owner == view.player && existing.name == design.name) { design = existing; break; }
    if (!ship_design_available_to_player(s, view.player, design)) return;
    if (design.id && std::any_of(homes.begin(), homes.end(), [&](const auto* p) {
        return std::any_of(p->productionQueue.begin(), p->productionQueue.end(), [&](const auto& item) { return item.shipDesign == design.id; });
    })) return;
    for (const auto* p : homes) if (p->productionQueue.empty() && colony_has_orbital_service(s, p->id, view.player, OrbitalStationModule::Shipyard)
        && mineral_cargo_sufficient(p->minerals, ship_design_mineral_cost(design))) {
        if (!design.id) plan.orders.orders.push_back(CreateShipDesignOrder{design.name, design.hull, design.components, design.placements});
        plan.orders.orders.push_back(QueueShipDesignOrder{p->id, design.id, design.id ? std::string{} : design.name}); break;
    }
}

void plan_expansion(const PlayerView& view, ComputerTurnPlan& plan, std::string& reason)
{
    const auto& state = view.state;
    auto& goals = plan.nextState.fleetGoals;
    std::vector<const Planet*> colonies;
    std::vector<const StarSystem*> stars;
    std::vector<const Fleet*> fleets;
    std::vector<std::pair<PlanetId, std::uint64_t>> availablePopulation;
    for (const auto& planet : state.planets) if (planet.owner == view.player) colonies.push_back(&planet);
    for (const auto* colony : colonies) availablePopulation.emplace_back(colony->id, colony->population);
    for (const auto& star : state.stars) stars.push_back(&star);
    for (const auto& fleet : state.fleets) if (fleet.owner == view.player && fleet.outpost == OutpostKind::None
        && mobile_base_hulls(state, fleet) == 0) fleets.push_back(&fleet);
    std::sort(colonies.begin(), colonies.end(), [](auto a, auto b) { return a->id < b->id; });
    std::sort(stars.begin(), stars.end(), [](auto a, auto b) { return a->id < b->id; });
    std::sort(fleets.begin(), fleets.end(), [](auto a, auto b) { return a->id < b->id; });
    for (const auto* colony : colonies) for (const auto& sample : colony->scienceSamples)
        if (!sample.retained) {
            const bool retain = resonance_technology_available(state, view.player)
                && (colony->resonanceLab.started || colony->xenocrystal.surface >= kResonanceCrystals
                    || std::any_of(state.planets.begin(), state.planets.end(), [&](const auto& p) {
                        const auto known = known_xenocrystal(state, view.player, p.id);
                        return known && known->depositSurveyed && known->site.original;
                    }))
                && sample_observation_received(state, view.player, sample.source);
            plan.orders.orders.push_back(ProcessScienceSampleOrder{colony->id, sample.source, retain});
            explain(reason, std::string(retain ? "Retain delivered specimen X" : "Analyze delivered specimen X") + std::to_string(sample.source));
        }

    std::erase_if(goals, [&](const auto& goal) {
        if(goal.kind==ComputerFleetGoalKind::InvestigateObject)
            return std::none_of(fleets.begin(),fleets.end(),[&](const auto* f){return f->id==goal.fleet;});
        if (goal.kind > ComputerFleetGoalKind::Resupply) return false;
        const auto fleet = std::find_if(fleets.begin(), fleets.end(), [&](auto f) { return f->id == goal.fleet; });
        if (fleet != fleets.end() && military_fleet(state, **fleet)) return false;
        if (fleet == fleets.end()) return true; // Only received disappearance, never host losses.
        if (!confirmed_idle(**fleet, goal.issuedTurn)) return false;
        if (goal.kind == ComputerFleetGoalKind::Survey)
            return survey_level(state, view.player, goal.target) >= (exotic_mining_available(state, view.player) ? SurveyLevel::DeepSurvey : SurveyLevel::OrbitalSurvey);
        const auto* target = find_star(state, goal.target);
        return target && same_position((*fleet)->telemetry.position, target->position);
    });

    for (const auto* original : fleets) {
        if (military_fleet(state, *original)) continue;
        auto fleet = *original;
        const auto existing = std::find_if(goals.begin(), goals.end(), [&](const auto& g) { return g.fleet == fleet.id; });
        if (existing != goals.end() && existing->kind >= ComputerFleetGoalKind::ExoticMining) continue;
        if(existing!=goals.end() && existing->kind==ComputerFleetGoalKind::InvestigateObject) {
            const auto* object=known_unknown_object(state,view.player,existing->target);
            if (fleet.telemetry.observedTurn > existing->issuedTurn && !fleet.telemetry.destination
                && (fleet.telemetry.task == FleetTask::RecoverSample || fleet.telemetry.task == FleetTask::None)) {
                if (!fleet.scienceSamples.empty()) {
                    const Planet* home = nullptr; std::optional<std::uint8_t> warp;
                    double best = std::numeric_limits<double>::infinity();
                    for (const auto* colony : colonies) if (const auto* star = find_star(state, colony->star)) {
                        const auto candidate = route_warp(state, fleet, {star->position});
                        const auto distance = distance_between(fleet.telemetry.position, star->position);
                        if (candidate && distance < best) { home = colony; warp = candidate; best = distance; }
                    }
                    if (home) {
                        plan.orders.orders.push_back(MoveFleetOrder{fleet.id, find_star(state, home->star)->position, *warp,
                            {FleetArrivalActionKind::UnloadAll, 1, FleetCargoKind::All}});
                        *existing = {fleet.id, ComputerFleetGoalKind::Resupply, home->star, state.turn, review_turn(state, fleet)};
                        explain(reason, "Return physical specimen aboard fleet " + std::to_string(fleet.id));
                        continue;
                    }
                } else if (state.turn >= existing->reviewTurn) {
                    plan.orders.orders.push_back(SetFleetTaskOrder{fleet.id, FleetTask::None});
                    goals.erase(existing); explain(reason, "No specimen recovered by fleet " + std::to_string(fleet.id));
                    continue;
                }
            }
            if(object && object->kind && object->level==ObjectKnowledgeLevel::Studied && !fleet.telemetry.destination
                && fleet.telemetry.task==FleetTask::FieldResearch && fleet.telemetry.observedTurn>existing->issuedTurn) {
                auto loaded = fleet; loaded.scienceSamples.push_back({object->id, object->kind.value(), false});
                const bool canReturn = fleet_cargo_used(state, loaded) <= fleet_cargo_capacity(state, loaded) + 1e-6
                    && std::any_of(colonies.begin(), colonies.end(), [&](const auto* colony) {
                        const auto* star = find_star(state, colony->star);
                        return star && route_warp(state, loaded, {object->position, star->position}).has_value();
                    });
                if (canReturn) {
                    const auto warp = route_warp(state, loaded, {object->position});
                    if (warp) {
                        plan.orders.orders.push_back(MoveFleetOrder{fleet.id, object->position, *warp, {FleetArrivalActionKind::RecoverSample}});
                        existing->issuedTurn = state.turn; existing->reviewTurn = review_turn(state, fleet);
                        explain(reason, "Recover specimen from received study X" + std::to_string(object->id));
                        continue;
                    }
                }
                plan.orders.orders.push_back(SetFleetTaskOrder{fleet.id,FleetTask::None});
                goals.erase(existing); explain(reason,"Finish received object study by fleet "+std::to_string(fleet.id));
                continue;
            }
        }
        if (existing != goals.end()) {
            if (state.turn < existing->reviewTurn || fleet.telemetry.observedTurn <= existing->issuedTurn) continue;
            // Fresh telemetry may show a failed route (changed ownership,
            // interception), or insufficient fuel. Retire only an acknowledged
            // idle programme, or replace an acknowledged route when fuel or
            // newly received medium hazards warrant a slower speed/bypass.
            if (confirmed_idle(fleet, existing->issuedTurn)) {
                goals.erase(existing);
            } else if (fleet.destination && same_position(fleet.telemetry.position, fleet.position)) {
                std::vector<Position> remaining{*fleet.destination};
                for (const auto& waypoint : fleet.waypointQueue) remaining.push_back(waypoint.destination);
                const auto warp = route_warp(state, fleet, remaining);
                auto moving=fleet;
                bool receivedHazard=fleet_segment_damage(state,moving,*fleet.destination)>1e-6;
                moving.position=*fleet.destination;
                for (const auto& waypoint:fleet.waypointQueue) {
                    moving.warp=waypoint.warp;
                    receivedHazard=receivedHazard || fleet_segment_damage(state,moving,waypoint.destination)>1e-6;
                    moving.position=waypoint.destination;
                }
                if (warp && (*warp < fleet.warp || receivedHazard)) {
                    auto waypoints = fleet.waypointQueue;
                    for (auto& waypoint : waypoints) waypoint.warp = *warp;
                    plan.orders.orders.push_back(MoveFleetOrder{fleet.id, *fleet.destination, *warp,
                        fleet.arrivalAction.value_or(FleetArrivalAction{}), std::move(waypoints)});
                    existing->issuedTurn = state.turn;
                    existing->reviewTurn = review_turn(state, fleet);
                    explain(reason, "Revise received route risks/fuel for fleet " + std::to_string(fleet.id));
                }
                continue;
            } else continue;
        }
        if (probe_capable(state, fleet) || !idle(fleet) || fleet.damagePercent >= 100 || goals.size() >= kMaximumComputerFleetGoals) continue;
        // Use the confirmed position and fuel for a stationary fleet, not a
        // speculative arrival reconstructed from a stale moving snapshot.
        if (fleet.telemetry.observedTurn == 0 || !confirmed_idle(fleet, 0)) continue;
        fleet.position = fleet.telemetry.position;
        const auto* local = colony_at(state, view.player, fleet.position);
        budget_initial_depot_service(state,fleet);

        bool visitorAssigned = false;
        const auto* empire = find_player(state, view.player);
        if (!fleet_has_space_weapons(state, fleet) && !fleet_can_colonize(state, fleet)
            && fleet.telemetry.observedTurn == state.turn && fleet_has_instant_link(state, fleet)
            && std::find(empire->acquiredEquipment.begin(), empire->acquiredEquipment.end(), kVisitorGift) == empire->acquiredEquipment.end()) {
            auto contacts = empire->visitorKnowledge;
            std::sort(contacts.begin(), contacts.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
            for (const auto& contact : contacts) if (contact.available && !contact.departed && state.turn < contact.departureTurn) {
                const auto needed = std::max(0.0, kVisitorGermaniumPrice-fleet.minerals.germanium);
                if (fleet_cargo_used(state, fleet)+needed > fleet_cargo_capacity(state, fleet)
                    || (needed && (!local || local->minerals.germanium < needed))) continue;
                auto trial = fleet; trial.minerals.germanium += needed;
                for (int warp = fleet_max_warp(state, trial); warp >= 1; --warp) {
                    trial.warp = std::uint8_t(warp);
                    if (!fleet_warp_valid(state, trial, trial.warp) || fleet_overdrive_damage_rate(state, trial, trial.warp)>0) continue;
                    const auto aim = visitor_intercept_destination(trial, contact, state.turn, trial.warp);
                    if (!aim || fleet_segment_damage(state,trial,*aim)>1e-6) continue;
                    auto returning = trial; returning.position = *aim;
                    if (fleet_segment_damage(state,returning,fleet.position)>1e-6) continue;
                    if (leg_fuel(state, trial, *aim)+leg_fuel(state, returning, fleet.position)>trial.fuel) continue;
                    if (needed) plan.orders.orders.push_back(SetFleetMineralCargoOrder{local->id, fleet.id, trial.minerals});
                    plan.orders.orders.push_back(EncounterVisitorOrder{fleet.id, contact.id, trial.warp});
                    explain(reason, "Deliver paid Germanium to a received visitor trajectory"); visitorAssigned=true; break;
                }
                if (visitorAssigned) break;
            }
        }
        if (visitorAssigned) continue;
        const bool colonizer = fleet_can_colonize(state, fleet);
        if (colonizer && !fleet_radiation_safe(state, fleet)) continue;
        if (colonizer && fleet.colonists < kMinimumSettlers && local) {
            auto& population = std::find_if(availablePopulation.begin(), availablePopulation.end(),
                [&](const auto& entry) { return entry.first == local->id; })->second;
            const auto capacity = static_cast<std::uint64_t>(std::floor(std::max(0.0,
                fleet_cargo_capacity(state, fleet) - mineral_cargo_mass(fleet.minerals)
                    - science_sample_mass(fleet.scienceSamples) - fleet.xenocrystal) * kColonistsPerCargoUnit));
            const auto settlers = std::min({capacity, population / 4, std::uint64_t{2'500}});
            if (settlers >= kMinimumSettlers && settlers > fleet.colonists) {
                plan.orders.orders.push_back(SetFleetColonistsOrder{local->id, fleet.id, settlers});
                population -= settlers - fleet.colonists;
                fleet.colonists = settlers;
                explain(reason, "Load settlers into fleet " + std::to_string(fleet.id));
            }
        }

        // Stage through the received network, then wait for arrival telemetry
        // before issuing the frontier route. Never combine a speculative gate
        // arrival with a same-year movement or cargo operation.
        const StarSystem* frontier = nullptr;
        for (const auto* star : stars) {
            const auto* planet = find_planet_at_star(state, star->id);
            if (colonizer ? !planet || !suitable(state, view.player, *planet)
                : survey_level(state, view.player, star->id) >= SurveyLevel::OrbitalSurvey) continue;
            if (!frontier || distance_between(fleet.position, star->position) < distance_between(fleet.position, frontier->position))
                frontier = star;
        }
        if (frontier) if (const auto gate = gate_staging_order(state, fleet, frontier->position, true, &plan.orders)) {
            const auto colony = std::find_if(colonies.begin(), colonies.end(),
                [&](const auto* p) { return p->id == gate->destination; });
            plan.orders.orders.push_back(*gate);
            goals.push_back({fleet.id, ComputerFleetGoalKind::Resupply, (*colony)->star,
                state.turn, review_turn(state, fleet)});
            explain(reason, "Gate fleet " + std::to_string(fleet.id) + " toward frontier; await telemetry");
            continue;
        }

        const Planet* depot = nullptr;
        double depotDistance = std::numeric_limits<double>::infinity();
        for (const auto* colony : colonies) {
            if (!colony_has_orbital_service(state, colony->id, view.player, OrbitalStationModule::RefuelingDepot)) continue;
            if (colonizer && fleet.colonists < kMinimumSettlers && colony->population / 4 < kMinimumSettlers) continue;
            const auto* star = find_star(state, colony->star);
            if (!star || !route_warp(state, fleet, {star->position})) continue;
            const auto distance = distance_between(fleet.position, star->position);
            if (distance < depotDistance) { depot = colony; depotDistance = distance; }
        }

        const StarSystem* target = nullptr;
        std::optional<std::uint8_t> warp;
        double best = std::numeric_limits<double>::infinity();
        if (colonizer && fleet.colonists >= kMinimumSettlers) {
            for (const auto& planet : state.planets) {
                if (!suitable(state, view.player, planet)
                    || std::any_of(goals.begin(), goals.end(), [&](const auto& goal) {
                        return goal.kind == ComputerFleetGoalKind::Colonize && goal.target == planet.star;
                    })) continue;
                const auto* star = find_star(state, planet.star);
                if (!star) continue;
                const auto candidateWarp = route_warp(state, fleet, {star->position});
                if (!candidateWarp) continue;
                const auto score = distance_between(fleet.position, star->position) / warp_distance(*candidateWarp)
                    + (100 - *known_planet_habitability(state, view.player, planet.id)) / 10.0;
                if (score < best || (score == best && target && star->id < target->id)) {
                    best = score; target = star; warp = candidateWarp;
                }
            }
        } else if (!colonizer && fleet_has_scanner(state, fleet) && depot) {
            const auto* depotStar = find_star(state, depot->star);
            for (const auto* star : stars) {
                const auto* planet = find_planet_at_star(state, star->id);
                const auto owner = planet ? known_planet_owner(state, view.player, planet->id) : std::nullopt;
                const bool refreshEnemy = owner && *owner != 0 && *owner != view.player
                    && system_intel_age(state, view.player, star->id).value_or(0) > 12;
                if ((survey_level(state, view.player, star->id) >= (exotic_mining_available(state, view.player) ? SurveyLevel::DeepSurvey : SurveyLevel::OrbitalSurvey) && !refreshEnemy)
                    || std::any_of(goals.begin(), goals.end(), [&](const auto& goal) {
                        return goal.kind == ComputerFleetGoalKind::Survey && goal.target == star->id;
                    })) continue;
                const auto candidateWarp = route_warp(state, fleet, {star->position, depotStar->position});
                if (!candidateWarp) continue;
                const auto score = (distance_between(fleet.position, star->position)
                    + distance_between(star->position, depotStar->position)) / warp_distance(*candidateWarp);
                if (score < best) { best = score; target = star; warp = candidateWarp; }
            }
        }
        const ObjectObservation* expedition=nullptr;
        if(!colonizer && fleet_has_scanner(state,fleet) && fleet_active_sensors(fleet,state.turn) && depot) {
            const auto* player=find_player(state,view.player); const auto* depotStar=find_star(state,depot->star);
            for(const auto& object:player->objectKnowledge) {
                if(object.level==ObjectKnowledgeLevel::Studied || object.uncertainty>object_study_radius(state,fleet)
                    || std::any_of(goals.begin(),goals.end(),[&](const auto& g) {
                    return g.kind==ComputerFleetGoalKind::InvestigateObject && g.target==object.id;
                })) continue;
                const auto candidate=route_warp(state,fleet,{object.position,depotStar->position}); if(!candidate) continue;
                const auto score=(distance_between(fleet.position,object.position)+distance_between(object.position,depotStar->position))/warp_distance(*candidate);
                if(score<best || (expedition && score==best && object.id<expedition->id)) {
                    best=score; expedition=&object; warp=candidate;
                }
            }
        }
        if(expedition) {
            plan.orders.orders.push_back(MoveFleetOrder{fleet.id,expedition->position,*warp,{FleetArrivalActionKind::FieldResearch}});
            goals.push_back({fleet.id,ComputerFleetGoalKind::InvestigateObject,expedition->id,state.turn,review_turn(state,fleet)});
            explain(reason,"Study received object X"+std::to_string(expedition->id)+" by fleet "+std::to_string(fleet.id));
        } else if (target) {
            MoveFleetOrder move{fleet.id, target->position, *warp};
            if (colonizer) {
                move.arrivalAction.kind = FleetArrivalActionKind::Colonize;
                move.arrivalAction.colonyPlan = colony_development_presets().front();
            }
            else move.queuedWaypoints.push_back({find_star(state, depot->star)->position,
                *warp, {FleetArrivalActionKind::Refuel}});
            plan.orders.orders.push_back(std::move(move));
            goals.push_back({fleet.id, colonizer ? ComputerFleetGoalKind::Colonize : ComputerFleetGoalKind::Survey,
                target->id, state.turn, review_turn(state, fleet)});
            explain(reason, std::string(colonizer ? "Settle " : "Survey ") + std::to_string(target->id)
                + " by fleet " + std::to_string(fleet.id) + " at Warp " + std::to_string(*warp));
        } else if (depot && !same_position(fleet.position, find_star(state, depot->star)->position)) {
            const auto* star = find_star(state, depot->star);
            const auto returnWarp = route_warp(state, fleet, {star->position});
            FleetArrivalAction action{FleetArrivalActionKind::Refuel};
            if (colonizer && fleet.colonists < kMinimumSettlers) {
                action.kind = FleetArrivalActionKind::LoadAllAvailable;
                action.reservePopulation = depot->population * 3 / 4;
            }
            plan.orders.orders.push_back(MoveFleetOrder{fleet.id, star->position, *returnWarp, action});
            goals.push_back({fleet.id, ComputerFleetGoalKind::Resupply, star->id, state.turn, review_turn(state, fleet)});
            explain(reason, "Resupply fleet " + std::to_string(fleet.id) + " at " + std::to_string(star->id));
        }
    }

    std::size_t settlers = 0, scouts = 0, sites = 0;
    for (const auto* fleet : fleets) {
        if (fleet_can_colonize(state, *fleet)) ++settlers;
        else if (!military_fleet(state, *fleet) && fleet_has_scanner(state, *fleet)) ++scouts;
    }
    for (const auto& planet : state.planets) if (suitable(state, view.player, planet)) ++sites;
    for (const auto* colony : colonies) for (const auto& item : colony->productionQueue) {
        if (item.kind != ProductionKind::ColonyShip) continue;
        if (const auto* design = find_ship_design(state, item.shipDesign)) {
            if (ship_design_can_colonize(*design)) settlers += item.quantity;
            else {
                Fleet example; example.owner = view.player; example.design = design->id;
                if (!military_fleet(state, example) && ship_design_has_scanner(*design)) scouts += item.quantity;
            }
        }
    }
    const bool unexplored = std::any_of(stars.begin(), stars.end(), [&](auto star) {
        return survey_level(state, view.player, star->id) < (exotic_mining_available(state, view.player) ? SurveyLevel::DeepSurvey : SurveyLevel::OrbitalSurvey);
    });
    auto colonyDesign = civilian_design(state, view.player, true);
    auto scoutDesign = civilian_design(state, view.player, false);
    const auto scoutsWanted = std::min<std::size_t>(4, colonies.size() + 1);
    std::vector<std::string> createdDesigns;
    for (const auto* colony : colonies) {
        if (!colony->productionQueue.empty()) continue; // Preserve paid work and standing queues.
        if (std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const Order& order) {
            return std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, QueueShipDesignOrder> || std::is_same_v<T, QueueStationOrder>
                    || std::is_same_v<T, QueueProductionBatchOrder>) return value.colony == colony->id;
                return false;
            }, order);
        })) continue;
        const auto canAfford = [&](ProductionKind kind) {
            return mineral_cargo_sufficient(colony->minerals,
                production_item_mineral_cost(state, ProductionItem{kind, production_cost(kind), 0}));
        };
        const auto build = [&](ProductionKind kind, std::uint32_t quantity = 1) {
            plan.orders.orders.push_back(QueueProductionBatchOrder{colony->id, kind, quantity});
            explain(reason, "Build " + std::to_string(quantity) + " "
                + (kind == ProductionKind::CleanupContamination ? "contamination cleanup" : kind == ProductionKind::ReconstructFactory ? "factory reconstruction" : kind == ProductionKind::ReconstructMine ? "mine reconstruction" : kind == ProductionKind::Mine ? "mine" : kind == ProductionKind::Factory ? "factory" : "terraform")
                + " at colony " + std::to_string(colony->id));
        };
        // Fair frontier infrastructure: local receivers are constructed by
        // every newly founded colony through its autonomous queue. Computer
        // empires can then pay for a 150 ly upgrade through the same ordinary
        // production order and mineral constraints as human empires.
        if (!colony->homeworld && colony->radar == PlanetaryRadarLevel::Local
            && colony->industry >= 8 && canAfford(ProductionKind::PlanetaryRadar)) {
            build(ProductionKind::PlanetaryRadar);
            continue;
        }
        if(colony->warDamage.contamination>=10 && !find_player(state,view.player)->race.radiationImmune
            && canAfford(ProductionKind::CleanupContamination)) {
            build(ProductionKind::CleanupContamination,std::min(3U,cleanup_units_available(*colony)));
            continue;
        }
        bool reconstructing=false;
        for(auto kind:{ProductionKind::ReconstructFactory,ProductionKind::ReconstructMine})
            if(const auto remaining=reconstruction_available(*colony,kind); remaining && canAfford(kind)) {
                build(kind,std::uint32_t(std::min<std::uint64_t>(remaining,3))); reconstructing=true; break;
            }
        if(reconstructing) continue;
        const auto exotic = known_xenocrystal(state, view.player, colony->id);
        const bool rareDeposit = exotic_mining_available(state, view.player) && exotic && exotic->depositSurveyed
            && exotic->site.remaining && exotic->colony && exotic->colony->owner == view.player && exotic->colony->population;
        const auto minesWanted = std::max(rareDeposit ? 25u : 2u, colony->industry / 2);
        const bool lowStock = std::min({colony->minerals.ironium, colony->minerals.boranium, colony->minerals.germanium}) < 15;
        if (colony->mines < minesWanted && (colony->mines < 2 || lowStock) && canAfford(ProductionKind::Mine)) {
            build(ProductionKind::Mine); continue;
        }
        const bool yard = colony_has_orbital_service(state, colony->id, view.player, OrbitalStationModule::Shipyard);
        const ShipDesign* ship = nullptr;
        if (yard && colony->population / 4 >= kMinimumSettlers && colonyDesign && settlers < std::min<std::size_t>(2, sites)
            && mineral_cargo_sufficient(colony->minerals, ship_design_mineral_cost(*colonyDesign))) ship = &*colonyDesign;
        else if (yard && scoutDesign && unexplored && scouts < scoutsWanted
            && mineral_cargo_sufficient(colony->minerals, ship_design_mineral_cost(*scoutDesign))) ship = &*scoutDesign;
        if (ship) {
            if (ship->id == 0 && std::find(createdDesigns.begin(), createdDesigns.end(), ship->name) == createdDesigns.end()) {
                plan.orders.orders.push_back(CreateShipDesignOrder{ship->name, ship->hull, ship->components, ship->placements});
                createdDesigns.push_back(ship->name);
            }
            plan.orders.orders.push_back(QueueShipDesignOrder{colony->id, ship->id, ship->id == 0 ? ship->name : std::string{}});
            if (ship_design_can_colonize(*ship)) ++settlers; else ++scouts;
            explain(reason, "Build " + ship->name + " at colony " + std::to_string(colony->id));
        } else if (colony->industry < 24 && canAfford(ProductionKind::Factory)) {
            build(ProductionKind::Factory);
        } else if (colony->mines < minesWanted && canAfford(ProductionKind::Mine)) {
            build(ProductionKind::Mine);
        } else if (!find_orbital_station_at_planet(state, colony->id) && colony->industry >= 4
            && colony->population >= 100'000 && canAfford(ProductionKind::OrbitalStation)) {
            plan.orders.orders.push_back(QueueProductionOrder{colony->id, ProductionKind::OrbitalStation});
            explain(reason, "Build dock at colony " + std::to_string(colony->id));
        } else if (known_planet_habitability(state, view.player, colony->id).value_or(100) < 80
            && next_terraforming_environment(state, view.player, *colony)) build(ProductionKind::Terraforming);
    }
    std::sort(goals.begin(), goals.end(), [](const auto& a, const auto& b) { return a.fleet < b.fleet; });
}

} // namespace computer_detail

using namespace computer_detail;

bool computer_state_valid(const Player& player, std::uint64_t boundaryTurn)
{
    if (player.controller > PlayerController::Computer
        || player.computer.decisions.size() > kMaximumComputerDecisionRecords
        || player.computer.initialized != !player.computer.decisions.empty()
        || player.computer.fleetGoals.size() > kMaximumComputerFleetGoals) return false;
    if (!player.computer.initialized && (!player.computer.fleetGoals.empty() || player.computer.researchGoal)) return false;
    if (player.controller == PlayerController::Human) return !player.computer.initialized;
    std::uint64_t previous = 0;
    for (const auto& record : player.computer.decisions) {
        if (record.turn <= previous || record.turn >= boundaryTurn
            || record.reason.empty() || record.reason.size() > kMaximumComputerDecisionReasonBytes)
            return false;
        previous = record.turn;
    }
    FleetId previousFleet = 0;
    for (const auto& goal : player.computer.fleetGoals) {
        if (goal.fleet <= previousFleet || goal.kind > ComputerFleetGoalKind::ExoticHauling
            || goal.target == 0 || goal.issuedTurn == 0 || goal.issuedTurn >= boundaryTurn
            || goal.reviewTurn < goal.issuedTurn) return false;
        previousFleet = goal.fleet;
    }
    if (player.computer.researchGoal && (player.computer.researchGoal->field > ResearchField::Weapons
        || player.computer.researchGoal->level == 0)) return false;
    return true;
}

ComputerTurnPlan plan_computer_turn(const PlayerView& view)
{
    const auto* player = find_player(view.state, view.player);
    if (!player || player->controller != PlayerController::Computer
        || view.state.players.size() != 1 || view.state.turn == 0
        || player->technology.focus > ResearchField::Weapons
        || !computer_state_valid(*player, view.state.turn))
        throw std::invalid_argument("A computer controller needs its own PlayerView");
    if (!player->computer.decisions.empty()
        && player->computer.decisions.back().turn >= view.state.turn)
        throw std::invalid_argument("Computer already planned this boundary");

    ComputerTurnPlan plan{{view.player, {}}, player->computer};
    std::string reason;
    plan_research(view, plan, reason);
    // Commit only received, fully supplied colonies whose racial habitability
    // improves materially. Construction and operation use ordinary rules.
    for(const auto& p:view.state.planets) {
        if(p.owner!=view.player || !p.population || p.engineering.kind!=PlanetaryEngineeringKind::None
            || !p.productionQueue.empty() || colony_output(p)<kEngineeringWorkPerYear
            || !engineering_station_available(view.state,p)) continue;
        auto best=PlanetaryEngineeringKind::None; std::uint8_t strength=0;
        const int baseline=player_planet_habitability(view.state,view.player,p,view.state.turn);
        int gain=4;
        for(const auto kind:{PlanetaryEngineeringKind::ArtificialMagnetosphere,PlanetaryEngineeringKind::AdaptiveMagnetosphere}) {
            const double work=engineering_work(kind);
            if(!planetary_engineering_available(view.state,view.player,kind)
                || !mineral_cargo_sufficient(p.minerals,{work*0.1,work*0.05,work*0.1})) continue;
            for(std::uint8_t output=1;output<=engineering_max_strength(kind);++output) {
                auto proposed=p; proposed.observedHabitability.reset();
                proposed.engineering.kind=kind; proposed.engineering.work=engineering_work(kind);
                proposed.engineering.strength=output; proposed.engineering.operational=true;
                const auto improvement=player_planet_habitability(view.state,view.player,proposed,view.state.turn)-baseline;
                if(improvement>gain) {best=kind;strength=output;gain=improvement;}
            }
        }
        const auto capacity=population_capacity(view.state,p,view.state.turn);
        if(best==PlanetaryEngineeringKind::None && capacity && p.population>=capacity*4/5
            && planetary_engineering_available(view.state,view.player,PlanetaryEngineeringKind::OrbitalHabitat)
            && mineral_cargo_sufficient(p.minerals,{500,260,500})) {
            best=PlanetaryEngineeringKind::OrbitalHabitat;strength=10;
        }
        if(best!=PlanetaryEngineeringKind::None) {
            plan.orders.orders.push_back(SetPlanetaryEngineeringOrder{p.id,best,EngineeringAction::StartOrResume,strength});
            explain(reason,(best==PlanetaryEngineeringKind::OrbitalHabitat ? "Build received orbital housing at colony " : "Build received radiation protection at colony ")+std::to_string(p.id));
        }
    }
    plan_military(view, plan, reason);
    plan_gate_construction(view, plan, reason);
    // A paid dedicated probe uses ordinary programme and construction orders.
    bool hasProbe = false;
    for (const auto& fleet : view.state.fleets) if (probe_capable(view.state, fleet)) {
        hasProbe = true;
        if (!fleet.probe.configured && idle(fleet) && fleet.telemetry.observedTurn == view.state.turn) {
            ProbeProgram programme; programme.center = fleet.position; programme.radius = 600;
            double nearest = std::numeric_limits<double>::infinity();
            for (const auto& star : view.state.stars) if (survey_level(view.state, view.player, star.id) < SurveyLevel::DeepSurvey)
                nearest = std::min(nearest, distance_between(fleet.position, star.position));
            if (std::isfinite(nearest)) programme.radius = std::min(1e6, std::max(600.0, nearest + 100.0));
            plan.orders.orders.push_back(SetProbeProgramOrder{fleet.id, ProbeAction::Configure, programme});
            explain(reason, "Launch autonomous survey programme from received knowledge");
        }
    }
    if (!hasProbe && ship_hull_available_to_player(view.state, view.player, ShipHullType::SurveyProbe)) {
        auto design = basic_survey_probe_design(view.player, view.state.nextShipDesignId);
        bool queued = false;
        for (const auto& p : view.state.planets) for (const auto& item : p.productionQueue) {
            const auto* d = find_ship_design(view.state, item.shipDesign);
            queued |= item.kind == ProductionKind::ColonyShip && d && d->hull == ShipHullType::SurveyProbe;
        }
        if (!queued && ship_design_available_to_player(view.state, view.player, design)) {
            for (const auto& p : view.state.planets) if (p.owner == view.player && p.productionQueue.empty()
                && colony_has_orbital_service(view.state, p.id, view.player, OrbitalStationModule::Shipyard)
                && p.minerals.ironium >= ship_design_mineral_cost(design).ironium
                && p.minerals.boranium >= ship_design_mineral_cost(design).boranium
                && p.minerals.germanium >= ship_design_mineral_cost(design).germanium) {
                auto existing = std::find_if(view.state.shipDesigns.begin(), view.state.shipDesigns.end(),
                    [&](const auto& d) { return d.owner == view.player && d.name == design.name; });
                for (int suffix = 2; existing != view.state.shipDesigns.end(); ++suffix) {
                    Fleet candidate; candidate.owner = view.player; candidate.design = existing->id;
                    if (probe_capable(view.state, candidate) && ship_design_available_to_player(view.state, view.player, *existing)) break;
                    design.name = "Autonomous Survey Probe " + std::to_string(suffix);
                    existing = std::find_if(view.state.shipDesigns.begin(), view.state.shipDesigns.end(),
                        [&](const auto& d) { return d.owner == view.player && d.name == design.name; });
                }
                if (existing == view.state.shipDesigns.end()) {
                    plan.orders.orders.push_back(CreateShipDesignOrder{design.name, design.hull, design.components});
                    plan.orders.orders.push_back(QueueShipDesignOrder{p.id, 0, design.name});
                } else plan.orders.orders.push_back(QueueShipDesignOrder{p.id, existing->id});
                explain(reason, "Build an autonomous survey probe using colony minerals and shipyard"); break;
            }
        }
    }
    plan_exotic_materials(view, plan, reason);
    plan_expansion(view, plan, reason);
    // Population policy uses only fresh received orbital evidence and ordinary
    // cargo orders. It never requests emergency action from hidden host losses.
    for(const auto& p:view.state.planets) if(p.owner==view.player && habitat_completed(p)) {
        const auto evidence=std::find_if(player->surveyKnowledge.begin(),player->surveyKnowledge.end(),[&](const auto& k){return k.star==p.star;});
        if(evidence==player->surveyKnowledge.end()) continue;
        if(evidence->observedTurn==view.state.turn && !p.habitat.population && p.population>400000 && engineering_station_available(view.state,p)
            && engineering_operation_available(view.state,p)) {
            plan.orders.orders.push_back(TransferCargoOrder{{p.id,0},{p.id,0,true},200000,{}});
            explain(reason,"Seed received completed orbital habitat with physical colonists");
        }
        if(!p.habitat.population || (p.habitat.service!=HabitatService::Reserve && p.habitat.service!=HabitatService::Failing)) continue;
        const auto* at=find_star(view.state,p.star); if(!at)continue;
        for(const auto& f:view.state.fleets) {
            if(f.owner!=view.player || !idle(f) || !same_position(f.position,at->position))continue;
            const bool reserved=std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[&](const auto& o){return std::visit([&](const auto& v){
                if constexpr(requires { v.fleet; }) return v.fleet==f.id;
                else if constexpr(std::is_same_v<std::decay_t<decltype(v)>,TransferCargoOrder>) return v.source.fleet==f.id || v.destination.fleet==f.id;
                else return false;
            },o);});
            if(reserved)continue;
            const auto space=std::max(0.0,fleet_cargo_capacity(view.state,f)-fleet_cargo_used(view.state,f));
            const auto count=std::min(p.habitat.population,std::uint64_t(std::floor(space*kColonistsPerCargoUnit)));
            if(!count)continue;
            const Planet* safe=nullptr; double shortest=std::numeric_limits<double>::infinity();
            for(const auto& destination:view.state.planets) {
                if(destination.owner!=view.player || destination.id==p.id || !destination.population
                    || current_planet_habitability(view.state,destination,view.state.turn)<=0)continue;
                const auto* star=find_star(view.state,destination.star);if(!star)continue;
                const auto d=distance_between(f.position,star->position);auto loaded=f;loaded.colonists+=count;
                if(d<shortest && fleet_fuel_consumption_for_distance(view.state,loaded,d)<=f.fuel) {safe=&destination;shortest=d;}
            }
            if(!safe)continue;
            plan.orders.orders.push_back(MoveFleetOrder{f.id,at->position,f.warp,
                {FleetArrivalActionKind::LoadAllAvailable,0,FleetCargoKind::OrbitalColonists},
                {{find_star(view.state,safe->star)->position,f.warp,{FleetArrivalActionKind::UnloadAll,1,FleetCargoKind::Colonists}}}});
            auto& goals=plan.nextState.fleetGoals;std::erase_if(goals,[&](const auto& g){return g.fleet==f.id;});
            goals.push_back({f.id,ComputerFleetGoalKind::Resupply,safe->star,view.state.turn,review_turn(view.state,f)});
            explain(reason,"Evacuate received failing orbital habitat with an available paid transport");break;
        }
    }
    // Materialize the same received-profile paths used by route_warp as normal
    // waypoints. Final arrival actions stay at their original destinations.
    for (auto& order:plan.orders.orders) if (auto* move=std::get_if<MoveFleetOrder>(&order)) {
        if (!view.state.interstellarMediumEnabled || player->mediumKnowledge.empty()
            || move->clearRoute || move->targetFleet
            || std::any_of(move->queuedWaypoints.begin(),move->queuedWaypoints.end(),[](const auto& p) { return p.targetFleet!=0; })) continue;
        const auto original=std::find_if(view.state.fleets.begin(),view.state.fleets.end(),
            [&](const auto& f) { return f.id==move->fleet; });
        if (original==view.state.fleets.end()) continue;
        auto fleet=*original;
        // Use the same confirmed initial service as Warp selection, including
        // an acknowledged loading programme currently at a received depot.
        budget_initial_depot_service(view.state,fleet);
        for (const auto& pending:plan.orders.orders) {
            if (const auto* load=std::get_if<SetFleetColonistsOrder>(&pending); load && load->fleet==fleet.id)
                fleet.colonists=load->colonists;
            if (const auto* load=std::get_if<SetFleetMineralCargoOrder>(&pending); load && load->fleet==fleet.id)
                fleet.minerals=load->minerals;
        }
        std::vector<FleetWaypoint> input{{move->destination,move->warp,move->arrivalAction}};
        input.insert(input.end(),move->queuedWaypoints.begin(),move->queuedWaypoints.end());
        // Arrival inventories can grow or become unavailable before delivery.
        // After Load All, reserve a full hold for every later leg, including
        // after an unload that could fail or only partially complete. Ballast
        // exists only in this budgeting clone; no physical cargo is created.
        const auto prepared=fleet;
        std::uint8_t maximumWarp=1;
        for (const auto& waypoint:input) maximumWarp=std::max(maximumWarp,waypoint.warp);
        std::vector<FleetWaypoint> output;
        bool valid=false;
        for (int cap=maximumWarp;cap>=1 && !valid;--cap) {
            fleet=prepared; output.clear(); valid=true;
            for (const auto& waypoint:input) {
                fleet.warp=std::min(waypoint.warp,static_cast<std::uint8_t>(cap));
                if (!fleet_warp_valid(view.state,fleet,fleet.warp)) { valid=false; break; }
                const auto path=navigation_leg(view.state,fleet,waypoint.destination);
                if (!path) { valid=false; break; }
                for (std::size_t i=0;i<path->destinations.size();++i)
                    output.push_back({path->destinations[i],fleet.warp,
                        i+1==path->destinations.size()?waypoint.arrivalAction:FleetArrivalAction{}});
                fleet.position=waypoint.destination; fleet.fuel-=path->fuel;
                if (waypoint.arrivalAction.kind==FleetArrivalActionKind::LoadAllAvailable)
                    fleet.minerals.ironium+=std::max(0.0,fleet_cargo_capacity(view.state,fleet)-fleet_cargo_used(view.state,fleet));
            }
        }
        if (!valid) {
            // Keep the existing acknowledgement cycle, but send an ordinary
            // stop instead of approving an unfunded/unsafe fallback route.
            move->destination=prepared.position; move->warp=std::max<std::uint8_t>(1,prepared.warp);
            move->arrivalAction={}; move->queuedWaypoints.clear(); move->repeatOrders=false; move->clearRoute=true;
            explain(reason,"Hold unfunded received route for fleet "+std::to_string(move->fleet));
            continue;
        }
        bool changed=output.size()!=input.size();
        for (std::size_t i=0;!changed && i<input.size();++i)
            changed=output[i].warp!=input[i].warp || !same_position(output[i].destination,input[i].destination);
        if (!changed) continue;
        move->destination=output[0].destination; move->warp=output[0].warp; move->arrivalAction=output[0].arrivalAction;
        move->queuedWaypoints.assign(output.begin()+1,output.end());
        explain(reason,"Fund received cloud route with fleet "+std::to_string(move->fleet));
    }
    std::sort(plan.nextState.fleetGoals.begin(),plan.nextState.fleetGoals.end(),[](const auto& a,const auto& b){return a.fleet<b.fleet;});
    plan.nextState.initialized = true;
    if (reason.empty()) reason = plan.nextState.fleetGoals.empty()
        ? "Retain paid queues and research; no safe new missions available."
        : "Await mission acknowledgements; retain production and research goals.";
    plan.nextState.decisions.push_back({view.state.turn,
        static_cast<std::uint32_t>(plan.orders.orders.size()), std::move(reason)});
    if (plan.nextState.decisions.size() > kMaximumComputerDecisionRecords)
        plan.nextState.decisions.erase(plan.nextState.decisions.begin(),
            plan.nextState.decisions.end() - kMaximumComputerDecisionRecords);
    return plan;
}

std::vector<PlayerId> missing_human_submissions(
    const GameState& state, const std::vector<PlayerOrders>& humanSubmissions)
{
    std::vector<PlayerId> submitted;
    for (const auto& orders : humanSubmissions) {
        const auto* player = find_player(state, orders.player);
        if (!player || player->controller != PlayerController::Human
            || std::find(submitted.begin(), submitted.end(), orders.player) != submitted.end())
            throw std::invalid_argument("Unknown, computer or duplicate human submission");
        submitted.push_back(orders.player);
    }
    std::vector<PlayerId> missing;
    for (const auto& player : state.players) {
        if (player.controller > PlayerController::Computer)
            throw std::invalid_argument("Invalid empire controller");
        if (player.controller == PlayerController::Human
            && std::find(submitted.begin(), submitted.end(), player.id) == submitted.end())
            missing.push_back(player.id);
    }
    std::sort(missing.begin(), missing.end());
    return missing;
}

TurnResult resolve_controlled_campaign_turn(
    const GameState& state, std::vector<PlayerOrders> humanSubmissions)
{
    if (!missing_human_submissions(state, humanSubmissions).empty())
        throw std::invalid_argument("Waiting for every human to submit orders (or an explicit pass)");
    std::vector<std::pair<PlayerId, ComputerState>> plannedStates;
    for (const auto& player : state.players) {
        if (player.controller != PlayerController::Computer) continue;
        auto plan = plan_computer_turn(make_player_view(state, player.id));
        humanSubmissions.push_back(std::move(plan.orders));
        plannedStates.emplace_back(player.id, std::move(plan.nextState));
    }
    auto result = resolve_campaign_turn(state, std::move(humanSubmissions));
    for (auto& player : result.state.players) {
        const auto plan = std::find_if(plannedStates.begin(), plannedStates.end(),
            [&](const auto& entry) { return entry.first == player.id; });
        if (plan != plannedStates.end()) player.computer = std::move(plan->second);
    }
    return result;
}

} // namespace suns
