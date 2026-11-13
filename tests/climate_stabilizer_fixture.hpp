#pragma once
#include "suns/campaign.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/production.hpp"
#include "suns/player_knowledge.hpp"
#include <stdexcept>
namespace suns::test {
inline GameState climate_fixture()
{
    auto s=make_demo_game();s.wormholeRules.spawnChancePerTurn=0;s.fleets.clear();
    for(auto& star:s.stars)star.variability={};
    auto& owner=s.players.front();owner.race=race_preset(RacePreset::Terran);
    owner.race.legacyBiologyAdaptation=false;
    owner.race.habitableTemperature={30,70};owner.race.habitableGravity={30,70};owner.race.habitableRadiation={30,70};
    owner.technology.levels.fill(20);owner.technology.researchActive=false;owner.technology.researchAllocationPercent=0;
    auto& p=s.planets.front();p.population=1000000;p.industry=1000;p.mines=0;
    p.environment={40,50,50};p.naturalEnvironment=p.environment;p.observedHabitability.reset();
    p.engineering={};p.minerals={10000,10000,10000};p.productionQueue.clear();
    s.orbitalStations.front().hull=OrbitalStationHullType::Starbase;
    return s;
}
inline void finish_climate(GameState& s,std::uint8_t target=48)
{
    auto& p=s.planets.front();
    const bool accepted=apply_planetary_engineering_order(s,p,{p.id,PlanetaryEngineeringKind::ClimateStabilizer,EngineeringAction::StartOrResume,10,target});
    if(!accepted)throw std::logic_error("Climate fixture could not start");
    for(unsigned year=0;year<60;++year){(void)resolve_colony_production(s,p,true);++s.turn;}
    refresh_planetary_engineering(s);
}
}
