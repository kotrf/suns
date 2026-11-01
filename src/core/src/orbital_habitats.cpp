#include "suns/orbital_habitats.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include <algorithm>

namespace suns {
bool habitat_completed(const Planet& p)
{
    return p.engineering.kind == PlanetaryEngineeringKind::OrbitalHabitat
        && p.engineering.work == engineering_work(p.engineering.kind);
}
std::uint64_t total_colony_population(const Planet& p) { return p.population + p.habitat.population; }
std::int64_t projected_orbital_growth(const Planet& p)
{
    const auto n = p.habitat.population;
    if (!p.owner || !habitat_completed(p) || !p.engineering.operational
        || p.habitat.service != HabitatService::Normal || !n || n >= kOrbitalPopulationCapacity) return 0;
    return std::int64_t(std::max<std::uint64_t>(1,
        n * 5 * (kOrbitalPopulationCapacity - n) / (100 * kOrbitalPopulationCapacity)));
}
std::string habitat_service_name(HabitatService service)
{
    switch (service) {
    case HabitatService::Empty: return "Empty / mothballed";
    case HabitatService::Normal: return "Supported and paid";
    case HabitatService::Reserve: return "Emergency reserve (no growth or orbital output)";
    case HabitatService::Failing: return "Life support exhausted (10% population lost per year)";
    }
    return "Unknown";
}
std::uint32_t service_orbital_habitat(const GameState& state, Planet& p, bool supported)
{
    if (!habitat_completed(p)) return 0;
    auto& h = p.habitat;
    const auto year = state.turn + 1;
    if (h.servicedTurn == year) return h.productionPaid;
    h.servicedTurn = year; h.productionPaid = 0; h.populationLost = 0;
    if (!h.population) { h.service = HabitatService::Empty; p.engineering.operational=false; return 0; }
    const auto potential = std::uint64_t(colony_workforce(state, p).operatingFactories)
        + total_colony_population(p) / kPopulationPerProductionUnit;
    const auto affordable = [&] { return p.minerals.ironium >= 1 && p.minerals.boranium >= 1 && p.minerals.germanium >= 1; };
    const auto pay = [&] {
        --p.minerals.ironium; --p.minerals.boranium; --p.minerals.germanium; h.productionPaid += 2;
    };
    if (supported && engineering_operation_available(state, p) && potential >= 2 && affordable()) {
        pay(); h.service = HabitatService::Normal; p.engineering.operational = true;
        if (h.reserveYears < kHabitatReserveYears && potential >= 4 && affordable()) { pay(); ++h.reserveYears; }
    } else {
        p.engineering.operational = false;
        if (h.reserveYears) { --h.reserveYears; h.service = HabitatService::Reserve; }
        else {
            h.service = HabitatService::Failing;
            h.populationLost = (h.population + 9) / 10;
            h.population -= h.populationLost;
        }
    }
    return h.productionPaid;
}
namespace {
Planet after_ordinary_mining(const GameState& state,const Planet& planet,
    std::optional<MineralCargo> firstTurnMining)
{
    // One-year UI estimates start at the received planning boundary. Ordinary
    // extraction precedes upkeep in the real turn; arrivals remain unpredicted.
    // Queue forecasts already perform this phase and call the service resolver
    // directly, so they must not use this helper and mine a second time.
    auto forecast=planet;
    const auto mined=firstTurnMining.value_or(projected_mineral_mining(state,forecast));
    forecast.minerals.ironium+=mined.ironium;
    forecast.minerals.boranium+=mined.boranium;
    forecast.minerals.germanium+=mined.germanium;
    return forecast;
}
}
std::int64_t projected_orbital_growth(const GameState& state,const Planet& planet,
    std::optional<MineralCargo> firstTurnMining)
{
    auto forecast=after_ordinary_mining(state,planet,firstTurnMining);
    (void)service_orbital_habitat(state,forecast,engineering_station_available(state,forecast));
    return projected_orbital_growth(forecast);
}
std::uint32_t net_colony_output(const GameState& state,const Planet& planet,
    std::optional<MineralCargo> firstTurnMining)
{
    auto forecast=after_ordinary_mining(state,planet,firstTurnMining);
    const auto paid=service_orbital_habitat(state,forecast,engineering_station_available(state,forecast));
    const auto gross=colony_output(state, forecast);
    return gross-std::min(gross,paid);
}
void merge_received_habitat_station(std::optional<PlanetSurveySnapshot>& incoming,const std::optional<PlanetSurveySnapshot>& previous)
{
    if(!incoming || !incoming->habitat || !previous || !previous->habitat) return;
    auto& next=*incoming->habitat;const auto& old=*previous->habitat;
    if(old.stationEvidenceTime>next.stationEvidenceTime) {
        next.station=old.station;next.stationSupported=old.stationSupported;next.stationEvidenceTime=old.stationEvidenceTime;
    }
}
void apply_received_station_battle(Player& player,const PendingPlayerReport& report)
{
    if(!report.battle) return;
    const auto time=3*report.observedTurn+1;
    for(const auto& unit:report.battle->units) if(unit.owner==player.id && unit.station) {
        for(auto& record:player.surveyKnowledge) if(record.observedOwner==player.id && record.planetSnapshot && record.planetSnapshot->habitat) {
            auto& h=*record.planetSnapshot->habitat;
            if(!h.station || h.station->id!=unit.station || time<=h.stationEvidenceTime) continue;
            // Merge only delivered battle evidence. Never inspect today's host
            // station or change the population/queue observation date from an older survey.
            h.stationEvidenceTime=time;
            if(!unit.survivingShips){h.station.reset();h.stationSupported=false;}
            else h.station->damagePercent=std::clamp(100*(1-unit.remainingArmor/station_armor(*h.station)),0.0,99.999999);
        }
    }
}
void publish_habitat_reports(GameState& state, const GameState& previous)
{
    for (const auto& p : state.planets) {
        if (!p.owner || p.engineering.kind != PlanetaryEngineeringKind::OrbitalHabitat) continue;
        const auto* old = find_planet(previous, p.id);
        if(!p.habitat.population && (!old || !old->habitat.population)) continue;
        const auto* star = find_star(state, p.star);
        if (!star) continue;
        if (!old || old->habitat.service != p.habitat.service || old->habitat.reserveYears != p.habitat.reserveYears
            || p.habitat.populationLost) {
            // Engineering messages already provide delayed History and milestone plumbing.
            queue_player_report(state, p.owner, p.habitat.service == HabitatService::Normal
                ? PlayerReportKind::EngineeringResumed : PlayerReportKind::EngineeringPaused,
                star->position, state.turn + 1, p.star, p.id, 0, 0, ProductionKind::OrbitalHabitat,
                p.habitat.service == HabitatService::Normal?100:std::uint32_t(p.habitat.service == HabitatService::Failing ? EngineeringBlock::HabitatFailure
                    : p.habitat.service == HabitatService::Reserve ? EngineeringBlock::HabitatReserve : EngineeringBlock::Paused),ResearchField::Biology,p.habitat.reserveYears,{},p.habitat.populationLost);
        }
    }
}
}
