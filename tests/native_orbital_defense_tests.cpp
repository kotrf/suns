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
void last_years_miner_assignment_survives_delayed_telemetry()
{
    auto state=test::native_orbital_defense_fixture(20'000);auto& p=test::native_defense_home(state);
    const auto home=p.id;const auto homeStar=p.star;
    state.players.front().technology.levels[std::size_t(ResearchField::Energy)]=0;
    auto& miner=state.fleets.front();miner.ships.front().count=1;miner.telemetry={};
    miner.telemetry=confirmed_fleet_telemetry(state,miner);const auto moved=miner.id;
    const auto rate=ship_design_remote_mining_rate(*find_ship_design(state,miner.design));
    auto second=miner;second.id=state.nextFleetId++;state.fleets.push_back(second);
    const auto c=200.0/rate;p.observedConcentration=MineralCargo{c,c,c};p.radar=PlanetaryRadarLevel::None;
    // No scanner bridges the departing miner's first subspace report to HQ.
    for(auto& f:state.fleets)if(fleet_sensor_range(state,f)>0) {
        f.electronics={EmissionMode::RadioSilence,100};f.telemetry={};f.telemetry=confirmed_fleet_telemetry(state,f);
    }
    auto& target=*std::find_if(state.planets.begin(),state.planets.end(),[](const auto& world){return !world.owner;});
    target.owner=1;target.nativeOrbit.population=1000;target.minerals={10000,10000,10000};
    target.nativeOrbit.service=HabitatService::Normal;target.nativeOrbit.servicedTurn=state.turn;
    target.observedConcentration=p.observedConcentration;target.radar=PlanetaryRadarLevel::None;
    assert(complete_station_construction(state,target,{StationLoadout{},0}));
    target.nativeOrbit.station=find_orbital_station_at_planet(state,target.id)->id;
    auto* star=const_cast<StarSystem*>(find_star(state,target.star));const auto at=find_star(state,homeStar)->position;
    star->position={at.x+300,at.y};star->stellarClass=StarClass::Orange;star->variability={};
    set_survey_level(state,1,target.star,SurveyLevel::DeepSurvey,state.turn);
    p.nativeOrbit.servicedTurn=state.turn-1; // Do not start a project in the dispatch year.
    set_survey_level(state,1,homeStar,SurveyLevel::DeepSurvey,state.turn);
    const auto destination=star->position;state=resolve_controlled_campaign_turn(state,{}).state;
    // Deliver the colony closing independently of the travelling fleet packet.
    set_survey_level(state,1,homeStar,SurveyLevel::DeepSurvey,state.turn);
    auto view=make_player_view(state,1);assert(state.turn==11);
    const auto f=std::find_if(view.state.fleets.begin(),view.state.fleets.end(),[&](const auto& f){return f.id==moved;});
    assert(f!=view.state.fleets.end() && f->telemetry.observedTurn==10 && f->task==FleetTask::RemoteMining);
    assert(!f->destination && f->pendingCommands.empty());
    assert(std::any_of(view.state.players.front().computer.fleetGoals.begin(),view.state.players.front().computer.fleetGoals.end(),
        [&](const auto& g){return g.fleet==moved && g.kind==ComputerFleetGoalKind::Resupply && g.target!=homeStar;}));
    auto& received=*std::find_if(view.state.planets.begin(),view.state.planets.end(),[&](const auto& p){return p.id==home;});
    assert(received.nativeOrbit.servicedTurn==11);
    const auto rich=plan_computer_turn(view);const auto* work=test::native_station_order(rich,home);assert(work);
    const auto price=station_mineral_cost(work->construction.loadout);
    const auto quote=native_orbital_support_quote(view.state,received);
    const auto years=std::ceil(double(station_build_cost(work->construction.loadout))/(quote.output*0.8));assert(years>1);
    const auto margin=quote.mineralUnitsPaid+4+2*(years-1)-0.5;
    received.minerals={price.ironium+margin,price.boranium+margin,price.germanium+margin};
    auto acknowledged=view;
    auto& moving=*std::find_if(acknowledged.state.fleets.begin(),acknowledged.state.fleets.end(),[&](const auto& f){return f.id==moved;});
    moving.destination=destination;moving.task=FleetTask::None;
    assert(!test::native_station_order(plan_computer_turn(acknowledged),home));
    assert(!test::native_station_order(plan_computer_turn(view),home));
    received.minerals.ironium+=1;received.minerals.boranium+=1;received.minerals.germanium+=1;
    assert(test::native_station_order(plan_computer_turn(view),home));
}
void station_budget_covers_finite_emergency_kit_replenishment()
{
    auto state=test::native_orbital_defense_fixture(500'000);auto& p=test::native_defense_home(state);const auto home=p.id;
    state.players.front().technology.levels[std::size_t(ResearchField::Construction)]=14;
    auto fit=station_loadout(*native_orbital_station(state,p));fit.hull=OrbitalStationHullType::NativeCitadel;
    assert(complete_station_construction(state,p,{fit,p.nativeOrbit.station}));p.nativeOrbit.reserveUnits=0;
    const auto& miner=state.fleets.front();const auto rate=ship_design_remote_mining_rate(*find_ship_design(state,miner.design))*fleet_ship_count(miner);
    const auto c=2025.0/rate;p.observedConcentration=MineralCargo{c,c,c};
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto rich=plan_computer_turn(make_player_view(state,1));const auto* order=test::native_station_order(rich,home);assert(order);
    assert(order->construction.loadout.hull==OrbitalStationHullType::NativeBastion);
    const auto price=station_mineral_cost(order->construction.loadout);
    const auto quote=native_orbital_support_quote(state,p);assert(quote.mineralUnitsPaid==40 && quote.reserveAfter==20);
    p.minerals={price.ironium+120,price.boranium+120,price.germanium+120};
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto underfunded=plan_computer_turn(make_player_view(state,1));
    const auto finish=[&](GameState s,const PlayerOrders& orders) {
        for(unsigned year=0;year<10;++year) {
            s=TurnProcessor{}.process(s,year?std::vector<PlayerOrders>{}:std::vector{orders});
            const auto& colony=*find_planet(s,home);assert(colony.nativeOrbit.service==HabitatService::Normal && !colony.nativeOrbit.populationLost);
            if(native_orbital_station(s,colony)->hull==OrbitalStationHullType::NativeBastion)return s;
        }
        assert(false);return s;
    };
    const auto unsafe=finish(state,rich.orders);
    assert(find_planet(unsafe,home)->minerals.ironium<80); // Paid refit leaves less than one full Bastion bill.
    assert(!test::native_station_order(underfunded,home));
    p.minerals.ironium+=40;p.minerals.boranium+=40;p.minerals.germanium+=40;
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    const auto funded=plan_computer_turn(make_player_view(state,1));assert(test::native_station_order(funded,home));
    const auto safe=finish(state,funded.orders);const auto& colony=*find_planet(safe,home);
    assert(colony.minerals.ironium>=80 && colony.minerals.boranium>=80 && colony.minerals.germanium>=80);
    assert(colony.nativeOrbit.reserveUnits==60);
    p.nativeOrbit.reserveUnits=60;p.minerals={price.ironium+100,price.boranium+100,price.germanium+100};
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);
    assert(test::native_station_order(plan_computer_turn(make_player_view(state,1)),home)); // No perpetual kit charge.
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
    last_years_miner_assignment_survives_delayed_telemetry();
    station_budget_covers_finite_emergency_kit_replenishment();
    std::cout<<"Native station policy passed\n";
}
