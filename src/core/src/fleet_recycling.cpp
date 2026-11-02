#include "suns/fleet_recycling.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include <algorithm>
#include <cmath>

namespace suns {
namespace {
double recoveryRate(const GameState& state, const Fleet& fleet, bool colony, std::uint64_t turn)
{
    auto value = state.galaxySeed ^ (std::uint64_t(fleet.id) << 32) ^ fleet.owner
        ^ (turn * 0x9e3779b97f4a7c15ULL);
    const auto mix = [](std::uint64_t n) {
        n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ULL;
        n = (n ^ (n >> 27)) * 0x94d049bb133111ebULL;
        return n ^ (n >> 31);
    };
    value = mix(value);
    // Broad triangular space yield: density rises towards the 50% ceiling.
    // Discrete endpoints include an actual zero-material outcome.
    if (!colony) return kSpaceScrapRecovery * std::sqrt(double(value % 10001) / 10000.0);
    const auto loss = value % 100 < 20 ? 5 + mix(value + 1) % 11 : 0;
    return kColonyScrapRecovery - double(loss) / 100;
}
}
bool scrap_fleet(GameState& state, const Fleet& fleet, std::uint64_t turn)
{
    const auto report = [&](unsigned status, PlanetId planet = 0, MineralCargo minerals = {}) {
        const auto* colony = planet ? find_planet(state, planet) : nullptr;
        queue_player_report(state, fleet.owner, PlayerReportKind::FleetScrapStatus, fleet.position, turn,
            colony ? colony->star : 0, planet, fleet.id, fleet.design, ProductionKind::ColonyShip,
            status, ResearchField::Electronics, 0, minerals);
    };
    // Ordinary cargo minerals survive intact. Living passengers and unique
    // cargo require deliberate unloading, never implicit destruction.
    if (fleet.colonists || !fleet.scienceSamples.empty() || fleet.xenocrystal) { report(3); return false; }
    if (fleet.outpost != OutpostKind::None) { report(4); return false; }
    if (fleet_report_delivery_turn(state, fleet.owner, fleet.position, turn, fleet.id) == kAwaitingTransmission) {
        report(5); return false;
    }
    Planet* colony = nullptr;
    for (auto& candidate : state.planets) if (candidate.owner == fleet.owner && candidate.population) {
        const auto* star = find_star(state, candidate.star);
        if (star && same_position(star->position, fleet.position)) { colony = &candidate; break; }
    }
    auto recovered = fleet.minerals;
    const auto rate = recoveryRate(state, fleet, colony != nullptr, turn);
    for (const auto& stack : fleet_ship_stacks(fleet)) if (const auto* design = find_ship_design(state, stack.design)) {
        const auto cost = ship_design_mineral_cost(*design);
        recovered.ironium += cost.ironium * stack.count * rate;
        recovered.boranium += cost.boranium * stack.count * rate;
        recovered.germanium += cost.germanium * stack.count * rate;
    }
    if (colony && (colony->minerals.ironium + recovered.ironium > 1e15
        || colony->minerals.boranium + recovered.boranium > 1e15
        || colony->minerals.germanium + recovered.germanium > 1e15)) { report(6); return false; }
    // Release the departing transmitter's stored observations in its final
    // normal packet, including the existing merge/battle loss ledgers.
    dispatch_fleet_report_outbox(state, fleet.id, turn);
    if (colony) {
        colony->minerals.ironium += recovered.ironium;
        colony->minerals.boranium += recovered.boranium;
        colony->minerals.germanium += recovered.germanium;
    } else if (recovered.ironium + recovered.boranium + recovered.germanium > 0) {
        SalvageWreck wreck{state.nextWreckId++, fleet.position, turn + 20, {}, recovered};
        state.wrecks.push_back(wreck);
        for (auto& player : state.players) if (player.id == fleet.owner) player.pendingStrategicIntel.push_back({
            wreck.id, StrategicObjectKind::Wreck, wreck.position, 10, 0, turn,
            fleet_report_delivery_turn(state, fleet.owner, fleet.position, turn, fleet.id), double(wreck.expiresTurn), 1});
    }
    report(colony ? 1 : 2, colony ? colony->id : 0, recovered);
    const auto* player = find_player(state, fleet.owner);
    const auto due = player->pendingPlayerReports.back().deliveryTurn;
    if (due > turn) {
        auto contact = fleet;
        contact.pendingCommands.clear();
        state.pendingFleetLossContacts.push_back({std::move(contact), due});
    }
    return true;
}
} // namespace suns
