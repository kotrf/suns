#include "suns/strategic_operations.hpp"
#include "suns/communications.hpp"
#include "suns/equipment.hpp"
#include "suns/player_knowledge.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace suns {
namespace {
constexpr double epsilon = 1e-6;

template<class F> void components(const GameState& state, const Fleet& fleet, F visit)
{
    for (const auto& stack : fleet_ship_stacks(fleet))
        if (const auto* design = find_ship_design(state, stack.design))
            for (auto component : design->components) visit(component_spec(component), stack.count);
}

double combined_rating(const GameState& state, const Fleet& fleet, double ShipComponentSpec::*rating)
{
    double best = 0;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        const auto* design = find_ship_design(state, stack.design);
        if (!design) continue;
        double remaining = 1;
        for (auto component : design->components)
            remaining *= 1 - std::clamp(component_spec(component).*rating / 100, 0.0, 1.0);
        best = std::max(best, 1 - remaining);
    }
    return std::min(best, 0.95);
}

std::optional<double> entry_fraction(Position start, Position end, Position center, double radius)
{
    if (distance_between(start, center) <= radius + epsilon) return 0;
    const double dx = end.x - start.x, dy = end.y - start.y;
    const double px = start.x - center.x, py = start.y - center.y;
    const double a = dx*dx + dy*dy;
    if (a <= epsilon*epsilon) return {};
    const double b = 2*(px*dx + py*dy), c = px*px + py*py - radius*radius;
    const double discriminant = b*b - 4*a*c;
    if (discriminant < 0) return {};
    const double t = (-b - std::sqrt(discriminant))/(2*a);
    return t >= -epsilon && t <= 1 + epsilon
        ? std::optional<double>{std::clamp(t, 0.0, 1.0)} : std::nullopt;
}

void stage_intel(GameState& state, Player& player, StrategicIntel intel, Position source)
{
    auto transmit = intel.observedTurn;
    for (const auto& fleet : state.fleets) if (fleet.owner == player.id && same_position(fleet.position,source))
        transmit = std::max(transmit,fleet_available_turn(fleet,intel.observedTurn));
    intel.deliveryTurn = transmit + communication_delay_turns(state, player.id, source);
    const auto similar = [&](const StrategicIntel& old) {
        return old.type == intel.type && old.id == intel.id && old.observedTurn == intel.observedTurn;
    };
    if (std::any_of(player.strategicIntel.begin(), player.strategicIntel.end(), similar)) return;
    auto old = std::find_if(player.pendingStrategicIntel.begin(), player.pendingStrategicIntel.end(), similar);
    if (old == player.pendingStrategicIntel.end()) player.pendingStrategicIntel.push_back(intel);
    else if (intel.deliveryTurn < old->deliveryTurn) *old = intel;
}

bool science_once(GameState& state, Fleet& fleet, std::uint64_t key,
    Position target, StarId star, ResearchField field, std::uint32_t points, std::uint64_t turn)
{
    auto p = std::find_if(state.players.begin(), state.players.end(), [&](const auto& p) { return p.id == fleet.owner; });
    if (p == state.players.end() || std::any_of(p->fieldScience.begin(), p->fieldScience.end(),
        [&](const auto& r) { return r.key == key; })) return false;
    const auto delivery = turn + communication_delay_turns(state, p->id, fleet.position);
    p->fieldScience.push_back({key, delivery});
    queue_player_report(state, p->id, PlayerReportKind::ScientificData, fleet.position,
        turn, star, 0, fleet.id, 0, ProductionKind::Research, points, field, 0, {}, 0, target);
    return true;
}

std::vector<FleetId> sorted_fleets(const GameState& state)
{
    std::vector<FleetId> ids;
    for (const auto& f : state.fleets) ids.push_back(f.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
Fleet* fleet_by_id(GameState& state, FleetId id)
{
    auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == id; });
    return f == state.fleets.end() ? nullptr : &*f;
}
}

double minefield_radius(const Minefield& field) { return std::sqrt(std::max(0.0, field.mines)); }
std::uint8_t minefield_safe_warp(std::uint8_t kind) { return kind == 1 ? 6 : kind == 2 ? 5 : 4; }

double fleet_cloak_fraction(const GameState& state, const Fleet& fleet)
{
    // The most visible stack reveals the group. Identical ships do not multiply cloak strength.
    double cloak = 0.95;
    for (const auto& stack : fleet_ship_stacks(fleet)) {
        double remaining = 1;
        if (const auto* design = find_ship_design(state, stack.design))
            for (auto component : design->components)
                remaining *= 1 - std::clamp(component_spec(component).cloakPercent/100, 0.0, 1.0);
        cloak = std::min(cloak, 1 - remaining);
    }
    return cloak;
}
double fleet_tachyon_fraction(const GameState& state, const Fleet& fleet)
{ return combined_rating(state, fleet, &ShipComponentSpec::tachyonPercent); }

double strategic_sensor_range(const GameState& state, const Fleet& detector)
{
    if (detector.electronics.mode != EmissionMode::Standard) return 0;
    double interference = 0;
    for (const auto& enemy : state.fleets) {
        if (enemy.owner == detector.owner || enemy.electronics.mode != EmissionMode::Standard) continue;
        const double radius = std::max(30.0, fleet_sensor_range(state, enemy));
        if (distance_between(enemy.position, detector.position) <= radius)
            interference = std::max(interference, combined_rating(state, enemy, &ShipComponentSpec::jammingPercent));
    }
    return fleet_sensor_range(state, detector) * (1 - std::min(0.75, interference));
}

double fleet_detection_range(const GameState& state, const Fleet& target, double range, double tachyon, bool passive)
{
    const double cloak = fleet_cloak_fraction(state, target) * (1 - tachyon);
    double reflected = range * (1 - cloak);
    if (passive) {
        // A passive receiver localizes only close hull/engine signatures.
        const double motion = target.destination ? std::min(2.0, 0.5 + target.warp/5.0) : 0.5;
        reflected *= 0.25 * motion;
    }
    return reflected;
}
bool colony_detects_fleet(const GameState& state, const Planet& colony, const Fleet& target)
{
    const auto* star = find_star(state, colony.star);
    return colony.population && star && distance_between(star->position, target.position)
        <= fleet_detection_range(state, target, kColonySensorRange) + epsilon;
}
bool detector_detects_fleet(const GameState& state, const Fleet& detector, const Fleet& target)
{
    const bool passive = detector.electronics.mode != EmissionMode::Standard;
    const double range = passive ? fleet_sensor_range(state, detector) : strategic_sensor_range(state, detector);
    return fleet_has_scanner(state, detector) && distance_between(detector.position, target.position)
        <= fleet_detection_range(state, target, range, fleet_tachyon_fraction(state, detector), passive) + epsilon;
}

std::string fleet_task_name(FleetTask task)
{
    switch (task) {
    case FleetTask::None: return "Idle";
    case FleetTask::RemoteMining: return "Remote mining";
    case FleetTask::Bombardment: return "Bombard enemy colony";
    case FleetTask::LayMines: return "Lay minefields";
    case FleetTask::SweepMines: return "Sweep hostile mines";
    case FleetTask::FieldResearch: return "Field research";
    case FleetTask::Salvage: return "Recover wreckage";
    }
    return "Unknown task";
}
bool strategic_task_available(const GameState& state, const Fleet& fleet, FleetTask task)
{
    if (task == FleetTask::None) return true;
    if (fleet.destination || fleet.targetFleet || !fleet.waypointQueue.empty() || fleet.damagePercent >= 100) return false;
    bool bombs = false, miners = false, beams = false;
    components(state, fleet, [&](const auto& s, auto) {
        bombs |= s.kind == ShipComponentKind::Bomb;
        miners |= s.minesPerYear > 0;
        beams |= s.kind == ShipComponentKind::BeamWeapon && s.weaponPower > 0;
    });
    switch (task) {
    case FleetTask::Bombardment: return bombs;
    case FleetTask::LayMines: return miners;
    case FleetTask::SweepMines: return beams;
    case FleetTask::FieldResearch: return fleet_has_scanner(state, fleet);
    case FleetTask::Salvage: return fleet_has_scanner(state, fleet) && fleet_cargo_capacity(state, fleet) > 0;
    default: return false;
    }
}

void resolve_bombardments(GameState& state, std::uint64_t turn)
{
    for (const auto id : sorted_fleets(state)) {
        auto* f = fleet_by_id(state, id);
        if (!f || f->task != FleetTask::Bombardment || !strategic_task_available(state, *f, f->task)) continue;
        for (auto& planet : state.planets) {
            const auto* star = find_star(state, planet.star);
            if (!planet.owner || planet.owner == f->owner || !star || !same_position(star->position, f->position)) continue;
            const auto defender = planet.owner;
            const auto population = planet.population;
            double survival = 1, minimum = 0, installations = 0, reverting = 0;
            components(state, *f, [&](const auto& s, auto count) {
                if (s.kind != ShipComponentKind::Bomb) return;
                if (s.unterraformingBomb) { reverting += count; return; }
                const double coverage = s.smartBomb ? std::clamp(double(population)/2'500'000, 0.1, 1.0) : 1;
                survival *= std::pow(1 - std::clamp(s.bombPopulationPercent*coverage/100, 0.0, 1.0), count);
                minimum += double(s.bombMinimumKills)*count;
                installations += double(s.bombInstallations)*count;
            });
            const auto killed = std::uint64_t(std::min(double(population), std::max(minimum, std::floor(population*(1-survival)))));
            planet.population -= killed;
            auto removed = std::uint32_t(std::min({installations, double(planet.industry) + planet.mines, double(std::numeric_limits<std::uint32_t>::max())}));
            auto factories = std::min(planet.industry, removed/2 + removed%2);
            const auto mines = std::min(planet.mines,removed-factories);
            factories += std::min(planet.industry-factories,removed-factories-mines);
            removed = factories + mines; planet.industry -= factories; planet.mines -= mines;
            if (reverting > 0 && planet.naturalEnvironment) {
                auto restore = [&](std::uint8_t& current, std::uint8_t natural) {
                    const int step = int(std::min(reverting, 100.0));
                    current = std::uint8_t(current < natural ? std::min(int(natural), current+step)
                        : std::max(int(natural), current-step));
                };
                restore(planet.environment.temperature, planet.naturalEnvironment->temperature);
                restore(planet.environment.gravity, planet.naturalEnvironment->gravity);
                restore(planet.environment.radiation, planet.naturalEnvironment->radiation);
            }
            if (!killed && !removed && !reverting) continue;
            for (auto recipient : {f->owner, defender})
                queue_player_report(state, recipient, PlayerReportKind::Bombardment, star->position,
                    turn, star->id, planet.id, recipient == f->owner ? f->id : 0, 0,
                    ProductionKind::ColonyShip, removed, ResearchField::Weapons, 0, {}, killed);
            if (!planet.population) {
                queue_player_report(state, defender, PlayerReportKind::ColonyLost, star->position, turn, star->id, planet.id);
                planet.owner = 0;
                planet.productionQueue.clear();
                std::erase_if(state.orbitalStations, [&](const auto& s) { return s.planet == planet.id; });
            }
        }
    }
}

void advance_minefields(GameState& state)
{
    for (auto& field : state.minefields) field.mines *= 0.95;
    std::erase_if(state.minefields, [](const auto& f) { return f.mines < 1; });
}

Position minefield_navigation_endpoint(const GameState& state, const Fleet& fleet, Position end)
{
    double earliest = 1;
    for (const auto& field : state.minefields) {
        if (field.owner == fleet.owner || fleet.warp <= minefield_safe_warp(field.kind)) continue;
        const auto entry = entry_fraction(fleet.position,end,field.position,minefield_radius(field));
        if (entry) earliest = std::min(earliest,*entry);
    }
    return {fleet.position.x + earliest*(end.x-fleet.position.x), fleet.position.y + earliest*(end.y-fleet.position.y)};
}

bool apply_minefield_crossing(GameState& state, Fleet& fleet, Position start, Position& end,
    std::span<const Minefield> navigationFields)
{
    Minefield* first = nullptr;
    double fraction = 2;
    const auto fields = navigationFields.empty() ? std::span<const Minefield>{state.minefields} : navigationFields;
    for (const auto& field : fields) {
        if (field.owner == fleet.owner || fleet.warp <= minefield_safe_warp(field.kind)) continue;
        const auto entry = entry_fraction(start, end, field.position, minefield_radius(field));
        if (entry && (*entry < fraction || (*entry == fraction && first && field.id < first->id))) {
            const auto physical = std::find_if(state.minefields.begin(), state.minefields.end(),
                [&](const auto& f) { return f.id == field.id; });
            if (physical != state.minefields.end()) { fraction = *entry; first = &*physical; }
        }
    }
    if (!first) return false;
    end = {start.x + fraction*(end.x-start.x), start.y + fraction*(end.y-start.y)};
    const auto source = end;
    double armor = 0;
    for (const auto& s : fleet_ship_stacks(fleet))
        if (const auto* d = find_ship_design(state, s.design)) armor += std::max(1.0, ship_design_armor(*d))*s.count;
    const double consumed = std::min(first->mines, first->kind == 1 ? 100.0 : 50.0);
    first->mines -= consumed;
    if (first->kind != 2) fleet.damagePercent = std::min(100.0,
        fleet.damagePercent + 100*consumed*(first->kind == 1 ? 12 : 4)/std::max(1.0, armor));
    const auto delivery = state.turn + 1 + communication_delay_turns(state, fleet.owner, source);
    if (fleet.damagePercent >= 100 && delivery > state.turn + 1)
        state.pendingFleetLossContacts.push_back({fleet_player_view(state, fleet), delivery});
    queue_player_report(state, fleet.owner, PlayerReportKind::MineStrike, source, state.turn+1,
        0, 0, fleet.id, 0, ProductionKind::ColonyShip, std::uint32_t(first->kind), ResearchField::Weapons,
        0, {}, std::uint64_t(std::round(fleet.damagePercent)));
    fleet.destination.reset(); fleet.arrivalAction.reset(); fleet.waypointQueue.clear();
    fleet.targetFleet = 0; fleet.repeatOrders = false; fleet.routeTemplate.clear(); fleet.task = FleetTask::None;
    for (auto& p : state.players) if (p.id == fleet.owner)
        stage_intel(state, p, {first->id, StrategicObjectKind::Minefield, first->position,
            minefield_radius(*first), first->owner, state.turn+1, 0, first->mines, first->kind}, source);
    return true;
}

void run_strategic_tasks(GameState& state, std::uint64_t turn)
{
    std::erase_if(state.wrecks, [&](const auto& w) { return w.expiresTurn <= turn; });
    for (const auto id : sorted_fleets(state)) {
        auto* fleet = fleet_by_id(state, id);
        if (!fleet || !strategic_task_available(state, *fleet, fleet->task)) continue;
        if (fleet->task == FleetTask::LayMines) {
            std::array<double,3> rates{};
            components(state, *fleet, [&](const auto& s, auto count) {
                if (s.mineFieldKind < 3) rates[s.mineFieldKind] += s.minesPerYear*count;
            });
            for (std::uint8_t kind = 0; kind < 3; ++kind) if (rates[kind] > 0) {
                auto field = std::find_if(state.minefields.begin(), state.minefields.end(), [&](const auto& f) {
                    return f.owner == fleet->owner && f.kind == kind && same_position(f.position, fleet->position);
                });
                if (field == state.minefields.end()) state.minefields.push_back({state.nextMinefieldId++, fleet->owner,
                    fleet->position, std::min(rates[kind], 1e8), kind});
                else field->mines = std::min(1e8, field->mines + rates[kind]);
                auto created = std::find_if(state.minefields.begin(), state.minefields.end(), [&](const auto& f) {
                    return f.owner == fleet->owner && f.kind == kind && same_position(f.position,fleet->position);
                });
                for (auto& p : state.players) if (p.id == fleet->owner)
                    stage_intel(state,p,{created->id,StrategicObjectKind::Minefield,created->position,
                        minefield_radius(*created),created->owner,turn,0,created->mines,kind},fleet->position);
            }
        } else if (fleet->task == FleetTask::SweepMines) {
            double strength = 0;
            components(state, *fleet, [&](const auto& s, auto count) {
                if (s.kind == ShipComponentKind::BeamWeapon) strength += s.weaponPower*count;
            });
            for (auto& field : state.minefields) if (field.owner != fleet->owner
                && distance_between(field.position, fleet->position) <= minefield_radius(field) + 10)
                field.mines = std::max(0.0, field.mines - strength*(field.kind == 1 ? 0.5 : 2));
        } else if (fleet->task == FleetTask::FieldResearch && fleet->electronics.mode == EmissionMode::Standard) {
            for (const auto& star : state.stars) if (same_position(star.position, fleet->position)) {
                const auto* planet = find_planet_at_star(state, star.id);
                // Explicit, finite objectives: stellar observation and a deep planetary survey.
                science_once(state, *fleet, (1ULL<<32)|star.id, star.position, star.id,
                    ResearchField::Energy, 12, turn);
                if (survey_level(state, fleet->owner, star.id) >= SurveyLevel::GeologicalSurvey)
                    science_once(state, *fleet, (2ULL<<32)|star.id, star.position, star.id,
                        ResearchField::Biology, planet && planet->precursorArtifacts.present ? 32 : 12, turn);
            }
            for (const auto& wh : state.wormholes) for (const auto& end : wh.endpoints)
                if (distance_between(end.position, fleet->position) <= 10)
                    science_once(state, *fleet, (3ULL<<32)|end.id, end.position, 0, ResearchField::Propulsion, 24, turn);
        } else if (fleet->task == FleetTask::Salvage) {
            const auto wreck = std::min_element(state.wrecks.begin(), state.wrecks.end(), [&](const auto& a, const auto& b) {
                const double da = distance_between(a.position, fleet->position), db = distance_between(b.position, fleet->position);
                return da != db ? da < db : a.id < b.id;
            });
            if (wreck == state.wrecks.end() || distance_between(wreck->position, fleet->position) > 10) continue;
            const auto* owner = find_player(state, fleet->owner);
            if (!owner) continue;
            std::size_t field = 0; int novelty = 0;
            for (std::size_t i = 0; i < kResearchFieldCount; ++i) {
                const int gap = int(wreck->technology[i]) - owner->technology.levels[i];
                if (gap > novelty) { novelty = gap; field = i; }
            }
            // Ordinary or obsolete wreckage never becomes a repeatable technology farm.
            if (novelty > 0) science_once(state, *fleet, (4ULL<<32)|wreck->id, wreck->position, 0,
                ResearchField(field), std::uint32_t(std::min(96, novelty*12)), turn);
            state.wrecks.erase(wreck);
        }
    }
    std::erase_if(state.minefields, [](const auto& f) { return f.mines < 1; });
}

std::uint64_t fleet_available_turn(const Fleet& fleet, std::uint64_t turn)
{
    if (fleet.electronics.mode == EmissionMode::RadioSilence) return std::max(turn, fleet.electronics.resumeTurn);
    if (fleet.electronics.mode == EmissionMode::Passive) return turn + (3-turn%3)%3;
    return turn;
}
std::uint32_t communication_jamming_delay(const GameState& state, PlayerId owner, Position position)
{
    double strength = 0;
    for (const auto& enemy : state.fleets)
        if (enemy.owner != owner && enemy.electronics.mode == EmissionMode::Standard
            && distance_between(enemy.position, position) <= std::max(30.0, fleet_sensor_range(state, enemy)))
            strength = std::max(strength, combined_rating(state, enemy, &ShipComponentSpec::jammingPercent));
    return strength > 0 ? std::uint32_t(std::ceil(std::min(2.0, strength*2))) : 0;
}

bool fleet_transmits(const Fleet& fleet, std::uint64_t turn)
{
    return fleet.electronics.mode == EmissionMode::Standard
        || (fleet.electronics.mode == EmissionMode::Passive && turn % 3 == 0)
        || (fleet.electronics.resumeTurn && turn >= fleet.electronics.resumeTurn);
}
void apply_electronics_program(Fleet& fleet, ElectronicsProgram program) { fleet.electronics = program; }
bool submit_electronics_command(GameState& state, PlayerId player, FleetId id, ElectronicsProgram program)
{
    auto* fleet = fleet_by_id(state, id);
    if (!fleet || fleet->owner != player || program.mode > EmissionMode::RadioSilence) return false;
    if (program.deception && (program.mode != EmissionMode::Standard
        || combined_rating(state,*fleet,&ShipComponentSpec::jammingPercent) <= 0)) return false;
    if (program.mode == EmissionMode::RadioSilence && (program.resumeTurn <= state.turn
        || program.resumeTurn > state.turn + 100)) return false;
    if (program.mode != EmissionMode::RadioSilence) program.resumeTurn = 0;
    auto delay = communication_delay_turns(state, player, fleet->position);
    if (!delay && fleet_transmits(*fleet, state.turn)) { apply_electronics_program(*fleet, program); return true; }
    std::uint64_t delivery = state.turn + delay;
    if (fleet->electronics.mode == EmissionMode::RadioSilence) delivery = std::max(delivery, fleet->electronics.resumeTurn);
    else if (fleet->electronics.mode == EmissionMode::Passive) while (delivery % 3) ++delivery;
    fleet->pendingCommands.push_back({state.turn, delivery, {}, {}, program});
    return true;
}

void observe_strategic_objects(GameState& state, std::uint64_t turn)
{
    for (auto& player : state.players) {
        for (const auto& detector : state.fleets) {
            if (detector.owner != player.id || !fleet_has_scanner(state, detector)) continue;
            const double range = strategic_sensor_range(state, detector);
            if (range > 0) for (const auto& old : player.strategicIntel) {
                if (old.type == StrategicObjectKind::Emission || distance_between(old.position,detector.position) > range*0.5) continue;
                const bool exists = old.type == StrategicObjectKind::Minefield
                    ? std::any_of(state.minefields.begin(),state.minefields.end(),[&](const auto& f) { return f.id == old.id; })
                    : std::any_of(state.wrecks.begin(),state.wrecks.end(),[&](const auto& w) { return w.id == old.id; });
                if (!exists) { auto gone = old; gone.quantity = 0; gone.observedTurn = turn; stage_intel(state,player,gone,detector.position); }
            }
            for (const auto& field : state.minefields) if (field.owner == player.id
                || distance_between(field.position, detector.position) <= range*0.5 + minefield_radius(field))
                stage_intel(state, player, {field.id, StrategicObjectKind::Minefield, field.position,
                    minefield_radius(field), field.owner, turn, 0, field.mines, field.kind}, detector.position);
            for (const auto& wreck : state.wrecks) if (distance_between(wreck.position, detector.position) <= range)
                stage_intel(state, player, {wreck.id, StrategicObjectKind::Wreck, wreck.position, 10, 0, turn,
                    0, double(wreck.expiresTurn), 0}, detector.position);
            for (const auto& enemy : state.fleets) {
                if (enemy.owner != player.id && enemy.electronics.deception && fleet_transmits(enemy,turn)
                    && distance_between(enemy.position,detector.position) <= fleet_sensor_range(state,detector)*2
                    && fleet_tachyon_fraction(state,detector) < combined_rating(state,enemy,&ShipComponentSpec::jammingPercent)) {
                    const Position ghost{std::floor((enemy.position.x + 40)/20)*20+10,
                        std::floor((enemy.position.y + 20)/20)*20+10};
                    const auto id = std::uint32_t((std::uint64_t(enemy.id)*2246822519ULL + turn*3266489917ULL + state.galaxySeed) & 0xffffffff);
                    const auto before = player.pendingStrategicIntel.size();
                    stage_intel(state,player,{id,StrategicObjectKind::Emission,ghost,15,0,turn,0,0,0},detector.position);
                    if (player.pendingStrategicIntel.size() > before)
                        queue_player_report(state,player.id,PlayerReportKind::EmissionDetected,detector.position,turn,
                            0,0,0,0,ProductionKind::ColonyShip,15,ResearchField::Electronics,0,{},0,ghost);
                }
                if (enemy.owner == player.id || !fleet_transmits(enemy, turn) || detector_detects_fleet(state, detector, enemy)) continue;
                if (distance_between(enemy.position, detector.position) > fleet_sensor_range(state, detector)*2) continue;
                // Stable quantized area: no fleet ID, owner, exact position or composition leaks.
                const Position area{std::floor(enemy.position.x/20)*20+10, std::floor(enemy.position.y/20)*20+10};
                const auto key = std::uint32_t((std::uint64_t(enemy.id)*2654435761ULL + turn*40503 + state.galaxySeed) & 0xffffffff);
                StrategicIntel signal{key, StrategicObjectKind::Emission, area, 15, 0, turn, 0, 0, 0};
                const auto prior = player.pendingStrategicIntel.size();
                stage_intel(state, player, signal, detector.position);
                if (player.pendingStrategicIntel.size() > prior)
                    queue_player_report(state, player.id, PlayerReportKind::EmissionDetected, detector.position,
                        turn, 0, 0, 0, 0, ProductionKind::ColonyShip, 15, ResearchField::Electronics, 0, {}, 0, area);
            }
        }
    }
}

void deliver_strategic_intel(GameState& state)
{
    for (auto& player : state.players) {
        // Emissions are one planning-year evidence. Historical events survive.
        std::erase_if(player.strategicIntel, [&](const auto& i) {
            return i.type == StrategicObjectKind::Emission && i.deliveryTurn < state.turn;
        });
        for (const auto& packet : player.pendingStrategicIntel) {
            if (packet.deliveryTurn > state.turn) continue;
            auto old = std::find_if(player.strategicIntel.begin(), player.strategicIntel.end(), [&](const auto& i) {
                return i.type == packet.type && i.id == packet.id;
            });
            if (packet.type == StrategicObjectKind::Emission && packet.deliveryTurn < state.turn) continue;
            if (old == player.strategicIntel.end()) player.strategicIntel.push_back(packet);
            else if (old->observedTurn <= packet.observedTurn) *old = packet;
        }
        std::erase_if(player.pendingStrategicIntel, [&](const auto& i) { return i.deliveryTurn <= state.turn; });
        std::erase_if(player.strategicIntel, [](const auto& i) { return i.type != StrategicObjectKind::Emission && i.quantity <= 0; });
    }
}

void apply_scientific_data(GameState& state, Player& player, const PendingPlayerReport& report, std::vector<GameEvent>& events)
{
    const auto index = std::size_t(report.researchField);
    if (index >= kResearchFieldCount) return;
    auto points = report.quantity;
    while (points && player.technology.levels[index] < 255) {
        const auto next = std::uint8_t(player.technology.levels[index]+1);
        const auto cost = research_level_cost(report.researchField, next);
        auto& progress = player.technology.progress[index];
        const auto spent = std::min(points, cost > progress ? cost-progress : 0);
        points -= spent; progress += spent;
        if (progress < cost) break;
        progress = 0; player.technology.levels[index] = next;
        GameEvent event;
        event.id = (std::uint64_t(player.id)<<48) ^ (state.turn<<16) ^ (index<<8) ^ next ^ 0x5343490000000000ULL;
        event.turn = state.turn; event.observedTurn = report.observedTurn;
        event.recipient = player.id; event.kind = GameEventKind::ResearchLevelCompleted;
        event.researchField = report.researchField; event.technologyLevel = next;
        events.push_back(event);
    }
}
}
