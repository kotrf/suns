#include "native_orbital_defense_fixture.hpp"
#include "suns/native_orbital_support.hpp"
#include "suns/production.hpp"
#include <cmath>
#include <iostream>
using namespace suns;
namespace {
void paid_housing_and_weapons_preserve_station_and_residents()
{
    auto state=test::native_orbital_defense_fixture();const auto home=test::native_defense_home(state).id;
    const auto original=*native_orbital_station(state,*find_planet(state,home));
    const auto plan=plan_computer_turn(make_player_view(state,1));
    const auto* order=test::native_station_order(plan,home);assert(order);
    assert(order->construction.refitStation==original.id && order->construction.loadout.hull==OrbitalStationHullType::NativeCitadel);
    assert(order->construction.loadout.shipyard && order->construction.loadout.refueling);
    for(const auto c:original.components)assert(std::find(order->construction.loadout.components.begin(),order->construction.loadout.components.end(),c)!=order->construction.loadout.components.end());
    const auto cost=station_mineral_cost(order->construction.loadout);
    assert(station_construction_valid(state,*find_planet(state,home),order->construction));
    auto control=state;auto controlOrders=plan.orders;
    std::erase_if(controlOrders.orders,[](const auto& o){return std::holds_alternative<QueueStationOrder>(o);});
    bool complete=false;
    for(unsigned year=0;year<20 && !complete;++year) {
        state=TurnProcessor{}.process(state,year?std::vector<PlayerOrders>{}:std::vector{plan.orders});
        control=TurnProcessor{}.process(control,year?std::vector<PlayerOrders>{}:std::vector{controlOrders});
        const auto& p=*find_planet(state,home);const auto* station=native_orbital_station(state,p);
        assert(station && station->id==original.id && p.nativeOrbit.population && !p.population);
        assert(!p.nativeOrbit.populationLost && p.nativeOrbit.service==HabitatService::Normal);
        complete=station->hull==OrbitalStationHullType::NativeCitadel;
        if(complete) {
            assert(station_has_weapons(*station) && station_radiation_screen(*station)==station_radiation_screen(original));
            const auto& other=*find_planet(control,home);
            assert(std::abs(other.minerals.ironium-p.minerals.ironium-cost.ironium)<1e-6);
            assert(std::abs(other.minerals.boranium-p.minerals.boranium-cost.boranium)<1e-6);
            assert(std::abs(other.minerals.germanium-p.minerals.germanium-cost.germanium)<1e-6);
        }
    }
    assert(complete);
    assert(!test::native_station_order(plan_computer_turn(make_player_view(state,1)),home));
}
void emergency_economy_and_stale_queues_keep_priority()
{
    for(int scenario=0;scenario<5;++scenario) {
        auto state=test::native_orbital_defense_fixture();auto& p=test::native_defense_home(state);
        if(scenario==0)p.nativeOrbit.servicedTurn=state.turn-1;
        if(scenario==1)p.productionQueue={{ProductionKind::ColonyShip,10,state.shipDesigns.front().id}};
        if(scenario==2){p.minerals={};p.nativeOrbit.reserveUnits=12;}
        if(scenario==3){state.fleets.front().ships.front().count=1;state.fleets.front().telemetry={};state.fleets.front().telemetry=confirmed_fleet_telemetry(state,state.fleets.front());}
        if(scenario==4) {
            const auto funded=plan_computer_turn(make_player_view(state,1));const auto* q=test::native_station_order(funded,p.id);assert(q);
            const auto cost=station_mineral_cost(q->construction.loadout);
            const auto bill=native_orbital_support_quote(state,p).mineralUnitsPaid;
            p.minerals={cost.ironium+bill,cost.boranium+bill,cost.germanium+bill};
            assert(mineral_cargo_sufficient(p.minerals,cost)); // Construction fits; survival reserve does not.
        }
        set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
        const auto plan=plan_computer_turn(make_player_view(state,1));
        assert(!test::native_station_order(plan,p.id));
        if(scenario==3)assert(std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){return std::holds_alternative<QueueShipDesignOrder>(o);}));
    }
}
void housing_drives_research_and_preserves_fitted_protection()
{
    auto state=test::native_orbital_defense_fixture(420'000);auto& p=test::native_defense_home(state);
    auto fit=station_loadout(*native_orbital_station(state,p));fit.hull=OrbitalStationHullType::NativeCitadel;
    assert(complete_station_construction(state,p,{fit,p.nativeOrbit.station}));
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto plan=plan_computer_turn(make_player_view(state,1));
    assert(plan.nextState.researchGoal && plan.nextState.researchGoal->field==ResearchField::Construction);
    if(const auto* q=test::native_station_order(plan,p.id))assert(q->construction.loadout.hull==OrbitalStationHullType::NativeCitadel);
    state.players.front().technology.levels[std::size_t(ResearchField::Construction)]=14;
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto unlocked=plan_computer_turn(make_player_view(state,1));const auto* q=test::native_station_order(unlocked,p.id);
    assert(q && q->construction.loadout.hull==OrbitalStationHullType::NativeBastion);
    assert(q->construction.refitStation==p.nativeOrbit.station);
}
void housing_threshold_and_existing_fit_are_preserved()
{
    auto state=test::native_orbital_defense_fixture(79'999);auto& p=test::native_defense_home(state);
    auto station=std::find_if(state.orbitalStations.begin(),state.orbitalStations.end(),[&](const auto& s){return s.id==p.nativeOrbit.station;});
    std::erase(station->modules,OrbitalStationModule::RefuelingDepot);
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto small=plan_computer_turn(make_player_view(state,1));const auto* first=test::native_station_order(small,p.id);
    assert(first && first->construction.loadout.hull==OrbitalStationHullType::OrbitalDock);
    assert(first->construction.loadout.shipyard && !first->construction.loadout.refueling);
    assert(complete_station_construction(state,p,first->construction));
    const auto fitted=station_loadout(*native_orbital_station(state,p));
    assert(station_has_weapons(*native_orbital_station(state,p)));
    p.nativeOrbit.population=80'000;set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto crowded=plan_computer_turn(make_player_view(state,1));const auto* next=test::native_station_order(crowded,p.id);
    assert(next && next->construction.loadout.hull==OrbitalStationHullType::NativeCitadel);
    assert(next->construction.loadout.components==fitted.components && !next->construction.loadout.refueling);
}
void first_settler_savings_are_not_spent_on_defense()
{
    auto state=test::native_orbital_defense_fixture(25'000);auto& p=test::native_defense_home(state);
    const auto funded=plan_computer_turn(make_player_view(state,1));const auto* arm=test::native_station_order(funded,p.id);assert(arm);
    const auto price=station_mineral_cost(arm->construction.loadout);
    const auto bill=native_orbital_support_quote(state,p).mineralUnitsPaid+4;
    p.minerals={price.ironium+bill,price.boranium+bill,price.germanium+bill};
    const auto founder=std::find_if(state.shipDesigns.begin(),state.shipDesigns.end(),[](const auto& d){return ship_design_can_found_native_orbit(d);});
    assert(founder!=state.shipDesigns.end() && !mineral_cargo_sufficient(p.minerals,ship_design_mineral_cost(*founder)));
    std::erase_if(state.fleets,[&](const auto& f){return fleet_can_found_native_orbit(state,f);});
    auto& target=*std::find_if(state.planets.begin(),state.planets.end(),[](const auto& world){return !world.owner;});
    auto* star=const_cast<StarSystem*>(find_star(state,target.star));star->stellarClass=StarClass::Orange;star->variability={};
    set_survey_level(state,1,target.star,SurveyLevel::DeepSurvey,state.turn);
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto plan=plan_computer_turn(make_player_view(state,1));
    assert(!test::native_station_order(plan,p.id));
}
void received_evidence_and_storage_order_are_deterministic()
{
    auto state=test::native_orbital_defense_fixture();const auto home=test::native_defense_home(state).id;
    const auto before=plan_computer_turn(make_player_view(state,1));const auto* old=test::native_station_order(before,home);assert(old);
    auto& p=test::native_defense_home(state);p.minerals={};p.nativeOrbit.population=1;
    state.orbitalStations.front().hull=OrbitalStationHullType::NativeBastion;state.orbitalStations.front().components.clear();
    const auto hidden=plan_computer_turn(make_player_view(state,1));const auto* after=test::native_station_order(hidden,home);
    assert(after && after->construction==old->construction && hidden.nextState==before.nextState);
    std::reverse(state.planets.begin(),state.planets.end());std::reverse(state.fleets.begin(),state.fleets.end());
    std::reverse(state.shipDesigns.begin(),state.shipDesigns.end());
    const auto shuffled=plan_computer_turn(make_player_view(state,1));const auto* again=test::native_station_order(shuffled,home);
    assert(again && again->construction==old->construction && shuffled.nextState==before.nextState);
}
void departing_miners_do_not_fund_a_station_project()
{
    auto state=test::native_orbital_defense_fixture(25'000);auto& p=test::native_defense_home(state);
    state.players.front().technology.levels[std::size_t(ResearchField::Energy)]=0;
    auto& miner=state.fleets.front();miner.ships.front().count=1;miner.telemetry={};
    miner.telemetry=confirmed_fleet_telemetry(state,miner);
    const auto rate=ship_design_remote_mining_rate(*find_ship_design(state,miner.design));assert(rate>=2);
    const auto moved=miner.id;auto second=miner;second.id=state.nextFleetId++;state.fleets.push_back(second);
    const auto concentration=200.0/rate;p.observedConcentration=MineralCargo{concentration,concentration,concentration};
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    auto plan=plan_computer_turn(make_player_view(state,1));const auto* initial=test::native_station_order(plan,p.id);assert(initial);
    const auto price=station_mineral_cost(initial->construction.loadout);
    const auto bill=native_orbital_support_quote(state,p).mineralUnitsPaid+4;
    p.minerals={price.ironium+bill,price.boranium+bill,price.germanium+bill};
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    assert(test::native_station_order(plan_computer_turn(make_player_view(state,1)),p.id));
    auto& target=*std::find_if(state.planets.begin(),state.planets.end(),[](const auto& world){return !world.owner;});
    target.owner=1;target.nativeOrbit.population=1000;target.minerals={10000,10000,10000};
    target.nativeOrbit.service=HabitatService::Normal;target.nativeOrbit.servicedTurn=state.turn;
    target.observedConcentration=p.observedConcentration;
    assert(complete_station_construction(state,target,{StationLoadout{},0}));
    target.nativeOrbit.station=find_orbital_station_at_planet(state,target.id)->id;
    const auto at=find_star(state,p.star)->position;auto* star=const_cast<StarSystem*>(find_star(state,target.star));
    star->position={at.x+10,at.y+10};star->stellarClass=StarClass::Orange;star->variability={};
    set_survey_level(state,1,target.star,SurveyLevel::DeepSurvey,state.turn);
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    plan=plan_computer_turn(make_player_view(state,1));
    assert(std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[&](const auto& o){
        const auto* move=std::get_if<MoveFleetOrder>(&o);return move && move->fleet==moved && same_position(move->destination,star->position);
    }));
    assert(!test::native_station_order(plan,p.id));
}
}
int main()
{
    paid_housing_and_weapons_preserve_station_and_residents();
    emergency_economy_and_stale_queues_keep_priority();
    housing_drives_research_and_preserves_fitted_protection();
    housing_threshold_and_existing_fit_are_preserved();
    first_settler_savings_are_not_spent_on_defense();
    received_evidence_and_storage_order_are_deterministic();
    departing_miners_do_not_fund_a_station_project();
    std::cout<<"Native station policy passed\n";
}
