#include "suns/computer_player.hpp"
#include "suns/planetary_recovery.hpp"
#include "suns/communications.hpp"
#include "suns/terraforming.hpp"
#include "suns/production.hpp"
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
    return !fleet.destination && fleet.waypointQueue.empty() && !fleet.repeatOrders
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

double leg_fuel(const GameState& state, const Fleet& fleet, double distance)
{
    // The host rounds fuel separately for each year's movement, including the
    // short final leg. Billing the whole route once would under-budget Stars engines.
    const auto step = warp_distance(fleet.warp);
    const auto years = std::floor(distance / step);
    return years * fleet_fuel_consumption_for_distance(state, fleet, step)
        + fleet_fuel_consumption_for_distance(state, fleet, std::max(0.0, distance - years * step));
}

std::optional<std::uint8_t> route_warp(
    const GameState& state, Fleet fleet, const std::vector<Position>& destinations)
{
    if (fleet.damagePercent >= 100.0) return std::nullopt;
    auto cruise = fleet_max_warp(state, fleet);
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(state, stack.design))
            cruise = std::min(cruise, ship_design_cruise_warp(*design));
    for (int warp = cruise; warp >= 1; --warp) {
        fleet.warp = static_cast<std::uint8_t>(warp);
        if (!fleet_warp_valid(state, fleet, fleet.warp)
            || fleet_overdrive_damage_rate(state, fleet, fleet.warp) > 0) continue;
        double needed = 0;
        auto position = fleet.position;
        for (const auto destination : destinations) {
            needed += leg_fuel(state, fleet, distance_between(position, destination));
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
    for (const auto& fleet : state.fleets) if (fleet.owner == view.player) fleets.push_back(&fleet);
    std::sort(colonies.begin(), colonies.end(), [](auto a, auto b) { return a->id < b->id; });
    std::sort(stars.begin(), stars.end(), [](auto a, auto b) { return a->id < b->id; });
    std::sort(fleets.begin(), fleets.end(), [](auto a, auto b) { return a->id < b->id; });

    std::erase_if(goals, [&](const auto& goal) {
        if (goal.kind > ComputerFleetGoalKind::Resupply) return false;
        const auto fleet = std::find_if(fleets.begin(), fleets.end(), [&](auto f) { return f->id == goal.fleet; });
        if (fleet != fleets.end() && military_fleet(state, **fleet)) return false;
        if (fleet == fleets.end()) return true; // Only received disappearance, never host losses.
        if (!confirmed_idle(**fleet, goal.issuedTurn)) return false;
        if (goal.kind == ComputerFleetGoalKind::Survey)
            return survey_level(state, view.player, goal.target) >= SurveyLevel::OrbitalSurvey;
        const auto* target = find_star(state, goal.target);
        return target && same_position((*fleet)->telemetry.position, target->position);
    });

    for (const auto* original : fleets) {
        if (military_fleet(state, *original)) continue;
        auto fleet = *original;
        const auto existing = std::find_if(goals.begin(), goals.end(), [&](const auto& g) { return g.fleet == fleet.id; });
        if (existing != goals.end()) {
            if (state.turn < existing->reviewTurn || fleet.telemetry.observedTurn <= existing->issuedTurn) continue;
            // Fresh telemetry may show a failed route (changed ownership,
            // interception), or insufficient fuel. Retire only an acknowledged
            // idle programme, or replace an acknowledged route with a slower one.
            if (confirmed_idle(fleet, existing->issuedTurn)) {
                goals.erase(existing);
            } else if (fleet.destination && same_position(fleet.telemetry.position, fleet.position)) {
                std::vector<Position> remaining{*fleet.destination};
                for (const auto& waypoint : fleet.waypointQueue) remaining.push_back(waypoint.destination);
                const auto warp = route_warp(state, fleet, remaining);
                if (warp && *warp < fleet.warp) {
                    auto waypoints = fleet.waypointQueue;
                    for (auto& waypoint : waypoints) waypoint.warp = *warp;
                    plan.orders.orders.push_back(MoveFleetOrder{fleet.id, *fleet.destination, *warp,
                        fleet.arrivalAction.value_or(FleetArrivalAction{}), std::move(waypoints)});
                    existing->issuedTurn = state.turn;
                    existing->reviewTurn = review_turn(state, fleet);
                    explain(reason, "Conserve fuel for fleet " + std::to_string(fleet.id));
                }
                continue;
            } else continue;
        }
        if (!idle(fleet) || fleet.damagePercent >= 100 || goals.size() >= kMaximumComputerFleetGoals) continue;
        // Use the confirmed position and fuel for a stationary fleet, not a
        // speculative arrival reconstructed from a stale moving snapshot.
        if (fleet.telemetry.observedTurn == 0 || !confirmed_idle(fleet, 0)) continue;
        fleet.position = fleet.telemetry.position;
        const auto* local = colony_at(state, view.player, fleet.position);
        if (local && colony_has_orbital_service(state, local->id, view.player, OrbitalStationModule::RefuelingDepot))
            fleet.fuel = fleet_fuel_capacity(state, fleet);

        const bool colonizer = fleet_can_colonize(state, fleet);
        if (colonizer && !fleet_radiation_safe(state, fleet)) continue;
        if (colonizer && fleet.colonists < kMinimumSettlers && local) {
            auto& population = std::find_if(availablePopulation.begin(), availablePopulation.end(),
                [&](const auto& entry) { return entry.first == local->id; })->second;
            const auto capacity = static_cast<std::uint64_t>(std::floor(std::max(0.0,
                fleet_cargo_capacity(state, fleet) - mineral_cargo_mass(fleet.minerals)) * kColonistsPerCargoUnit));
            const auto settlers = std::min({capacity, population / 4, std::uint64_t{2'500}});
            if (settlers >= kMinimumSettlers && settlers > fleet.colonists) {
                plan.orders.orders.push_back(SetFleetColonistsOrder{local->id, fleet.id, settlers});
                population -= settlers - fleet.colonists;
                fleet.colonists = settlers;
                explain(reason, "Load settlers into fleet " + std::to_string(fleet.id));
            }
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
                if ((survey_level(state, view.player, star->id) >= SurveyLevel::OrbitalSurvey && !refreshEnemy)
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
        if (target) {
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
        return survey_level(state, view.player, star->id) < SurveyLevel::OrbitalSurvey;
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
        const auto minesWanted = std::max(2u, colony->industry / 2);
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
        if (goal.fleet <= previousFleet || goal.kind > ComputerFleetGoalKind::Invade
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
    plan_military(view, plan, reason);
    plan_expansion(view, plan, reason);
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
