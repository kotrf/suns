#pragma once
#include "atmospheric_regulator_fixture.hpp"
#include "suns/megaprojects.hpp"
#include "suns/wormholes.hpp"

namespace suns::test {
inline GameState megaproject_fixture()
{
    auto s=atmospheric_fixture();s.players.front().technology.levels.fill(26);
    s.players.front().technology.levels[std::size_t(ResearchField::Biology)]=24;
    s.planets.front().homeworld=true;s.planets.front().radar=PlanetaryRadarLevel::Full;
    for(std::size_t i=1;i<s.planets.size();++i)if(s.planets[i].owner==1){s.planets[i].owner=0;s.planets[i].population=0;}
    return s;
}
inline SetPlanetaryEngineeringOrder mega_order(PlanetId planet,PlanetaryEngineeringKind kind,EngineeringAction action)
{ return {planet,kind,action,10,50,50}; }
inline void prepare_megaproject(GameState& s,PlanetaryEngineeringKind kind)
{
    auto& p=s.planets.front();
    if(!apply_planetary_engineering_order(s,p,mega_order(p.id,kind,EngineeringAction::StartOrResume)))
        throw std::logic_error("Could not prepare megaproject");
    for(unsigned n=0;n<engineering_work(kind)/kEngineeringWorkPerYear;++n){(void)resolve_colony_production(s,p,true);++s.turn;}
    refresh_planetary_engineering(s);
    if(p.engineering.work!=engineering_work(kind))throw std::logic_error("Incomplete test megaproject");
}
} // namespace suns::test
