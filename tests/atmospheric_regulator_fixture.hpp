#pragma once
#include "climate_stabilizer_fixture.hpp"
namespace suns::test {
inline GameState atmospheric_fixture()
{
    auto s=climate_fixture();s.players.front().technology.levels.fill(22);
    s.players.front().technology.levels[std::size_t(ResearchField::Biology)]=20;
    auto& p=s.planets.front();p.environment={50,50,40};p.naturalEnvironment=p.environment;
    return s;
}
inline SetPlanetaryEngineeringOrder atmospheric_order(PlanetId id,EngineeringAction action,std::uint8_t output=10,std::uint8_t target=50)
{ return {id,PlanetaryEngineeringKind::AtmosphericRegulator,action,output,50,target}; }
inline void finish_atmosphere(GameState& s,std::uint8_t target=50)
{
    auto& p=s.planets.front();
    if(!apply_planetary_engineering_order(s,p,atmospheric_order(p.id,EngineeringAction::StartOrResume,10,target)))
        throw std::logic_error("Atmosphere fixture could not start");
    for(unsigned n=0;n<75;++n){(void)resolve_colony_production(s,p,true);++s.turn;}
    refresh_planetary_engineering(s);
}
}
