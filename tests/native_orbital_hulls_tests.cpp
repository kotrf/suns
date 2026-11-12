#include "native_orbital_fixture.hpp"
#include "suns/stations.hpp"
#include "suns/production.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/player_knowledge.hpp"
#include <cassert>
using namespace suns;
namespace {
GameState funded()
{
    auto state=test::empty_native_orbital_start();
    auto budget=native_orbital_start_budget();
    budget.minerals.ironium+=900; budget.minerals.boranium+=900; budget.minerals.germanium+=900;
    assert(initialize_native_orbital_start(state,1,state.planets.front().id,budget));
    state.players.front().technology.researchActive=false;
    state.players.front().technology.researchAllocationPercent=0;
    return state;
}
StationLoadout fit(OrbitalStationHullType hull)
{ return {hull,{ShipComponentType::BulkRadiationScreen},true,true}; }
void access_and_structural_classes_are_independent_of_enum_order()
{
    auto state=funded();
    for (auto hull:{OrbitalStationHullType::NativeCitadel,OrbitalStationHullType::NativeBastion}) {
        const auto& spec=station_hull_spec(hull);
        assert(spec.nativeOnly && station_loadout_valid(fit(hull)));
        auto& level=state.players.front().technology.levels[std::size_t(ResearchField::Construction)];
        level=spec.construction-1; assert(!station_loadout_available(state,1,fit(hull)));
        const auto old=state.planets.front();
        assert(!queue_station_construction(state,1,{old.id,{fit(hull),old.nativeOrbit.station}}));
        assert(state.planets.front().productionQueue.empty() && state.planets.front().minerals.ironium==old.minerals.ironium);
        level=spec.construction; assert(station_loadout_available(state,1,fit(hull)));
        assert(state.orbitalStations.front().hull==OrbitalStationHullType::OrbitalDock);
        auto other=make_demo_game(); other.players.front().technology.levels.fill(26);
        assert(!station_hull_available(other,1,hull) && !station_loadout_available(other,1,StationLoadout{hull}));
        assert(!queue_station_construction(other,1,{other.planets.front().id,{StationLoadout{hull},1}}));
        assert(spec.armor > station_hull_spec(spec.tier).armor && spec.weapons > station_hull_spec(spec.tier).weapons);
        assert(spec.cost > station_hull_spec(spec.tier).cost);
    }
    assert(native_orbital_capacity(OrbitalStationHullType::NativeCitadel)==500'000);
    assert(native_orbital_capacity(OrbitalStationHullType::NativeBastion)==2'000'000);
    auto gate=fit(OrbitalStationHullType::NativeCitadel); gate.components.push_back(ShipComponentType::JumpGate);
    assert(station_loadout_valid(gate));
    gate.components.back()=ShipComponentType::InfinityStargate;
    assert(!station_loadout_valid(gate)); // Citadel is a Starbase class, despite its appended enum value.
    gate.hull=OrbitalStationHullType::NativeBastion; assert(station_loadout_valid(gate));
    assert(!station_loadout_available(state,1,gate)); // The other racial trait's gate is not granted.
    assert(!station_loadout_valid(StationLoadout{OrbitalStationHullType(unsigned(kLastOrbitalStationHull)+1)}));
}
void upgrades_pay_full_cost_and_activate_housing_only_at_completion()
{
    for(auto hull:{OrbitalStationHullType::NativeCitadel,OrbitalStationHullType::NativeBastion}) {
        auto state=funded(); state.players.front().technology.levels[std::size_t(ResearchField::Construction)]=station_hull_spec(hull).construction;
        const auto home=state.planets.front().id, id=state.planets.front().nativeOrbit.station;
        const auto target=fit(hull); const auto price=station_mineral_cost(target); const auto cost=station_build_cost(target);
        state.planets.front().nativeOrbit.population=100'000;
        set_survey_level(state,1,state.planets.front().star,SurveyLevel::DeepSurvey,state.turn);
        const PlayerOrders order{1,{QueueStationOrder{home,{target,id}}}};
        unsigned work=0, paid=0, years=0;
        while(state.orbitalStations.front().hull!=hull && years<30) {
            const auto before=state.planets.front().minerals;
            const auto pop=state.planets.front().nativeOrbit.population;
            state=TurnProcessor{}.process(state,years ? std::vector<PlayerOrders>{} : std::vector<PlayerOrders>{order}); ++years;
            const auto& p=state.planets.front(); work+=p.nativeOrbit.productionAvailable; paid+=p.nativeOrbit.mineralUnitsPaid;
            const bool done=state.orbitalStations.front().hull==hull;
            assert(state.orbitalStations.front().id==id && state.nextOrbitalStationId==2);
            assert(p.nativeOrbit.population==pop+p.nativeOrbit.populationGrown && !p.population);
            assert(p.minerals.ironium==before.ironium+4-p.nativeOrbit.mineralUnitsPaid-(done?price.ironium:0));
            assert(p.minerals.boranium==before.boranium+2-p.nativeOrbit.mineralUnitsPaid-(done?price.boranium:0));
            assert(p.minerals.germanium==before.germanium+1-p.nativeOrbit.mineralUnitsPaid-(done?price.germanium:0));
            if(!done) {
                assert(p.productionQueue.size()==1 && p.productionQueue.front().remainingCost==cost-work);
                assert(native_orbital_capacity(state.orbitalStations.front().hull)==100'000 && p.nativeOrbit.population==100'000);
            }
        }
        assert(years>1 && years<30 && state.planets.front().productionQueue.empty());
        assert(work>=cost && state.planets.front().nativeOrbit.reserveUnits>0 && paid>0);
        assert(state.planets.front().nativeOrbit.population>100'000);
        assert(station_radiation_screen(state.orbitalStations.front())==20);
        assert(known_orbital_station(state,1,home)->hull==hull);
        assert(engineering_station_available(state,state.planets.front()));
    }
}
void smaller_housing_waits_for_evacuations_without_losing_paid_work()
{
    auto state=funded(); state.players.front().technology.levels[std::size_t(ResearchField::Construction)]=14;
    const auto id=state.planets.front().nativeOrbit.station;
    assert(complete_station_construction(state,state.planets.front(),{fit(OrbitalStationHullType::NativeBastion),id}));
    auto& planet=state.planets.front(); planet.nativeOrbit.population=500'001; planet.nativeOrbit.reserveUnits=3;
    const auto before=planet.nativeOrbit;
    const StationConstruction smaller{fit(OrbitalStationHullType::NativeCitadel),id};
    assert(station_refit_over_capacity(state,planet,smaller));
    assert(!queue_station_construction(state,1,{planet.id,smaller}));
    assert(!complete_station_construction(state,planet,smaller) && planet.nativeOrbit==before);
    // A valid queued shrink may outgrow its target while earlier work runs.
    planet.nativeOrbit.population=490'000;
    assert(queue_station_construction(state,1,{planet.id,smaller}));
    planet.productionQueue.front().remainingCost=17;
    planet.nativeOrbit.population=500'001;
    auto resolution=resolve_colony_production(state,planet,true);
    assert(resolution.completions.empty() && planet.productionQueue.front().remainingCost==17);
    assert(resolution.queueOrigins==std::vector<std::size_t>{0});
    planet.nativeOrbit.population=499'000; // Core boundary after a separate accepted evacuation.
    ++state.turn; resolution=resolve_colony_production(state,planet,true);
    assert(resolution.completions.size()==1 && planet.productionQueue.empty());
    const auto paid=planet.nativeOrbit;
    assert(complete_station_construction(state,planet,smaller));
    assert(planet.nativeOrbit==paid && state.orbitalStations.front().id==id);
}
void research_unlocks_choices_without_issuing_an_unpaid_station()
{
    auto state=funded(); state.players.front().technology.levels[std::size_t(ResearchField::Construction)]=5;
    const PlayerOrders research{1,{SetResearchPlanOrder{ResearchField::Construction,{},true},SetResearchAllocationOrder{100}}};
    const auto cost=research_level_cost(state,1,ResearchField::Construction,6);
    state.players.front().technology.progress[std::size_t(ResearchField::Construction)]=cost-1;
    state=TurnProcessor{}.process(state,{research});
    assert(technology_level(state,1,ResearchField::Construction)>=6);
    assert(station_hull_available(state,1,OrbitalStationHullType::NativeCitadel));
    assert(state.orbitalStations.size()==1 && state.orbitalStations.front().hull==OrbitalStationHullType::OrbitalDock);
}
}
int main()
{
    access_and_structural_classes_are_independent_of_enum_order();
    upgrades_pay_full_cost_and_activate_housing_only_at_completion();
    smaller_housing_waits_for_evacuations_without_losing_paid_work();
    research_unlocks_choices_without_issuing_an_unpaid_station();
}
