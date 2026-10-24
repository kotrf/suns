#include "suns/interstellar_medium.hpp"
#include "suns/campaign.hpp"
#include "suns/computer_player.hpp"
#include "suns/orbital_habitats.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/turn_processor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace suns;
bool near(double a,double b,double epsilon=1e-8) { return std::abs(a-b)<epsilon; }
GameState medium_fixture()
{
    auto s=make_demo_game(); s.turn=10; s.wormholeRules.spawnChancePerTurn=0;
    s.interstellarMediumEnabled=true; s.interstellarClouds={{1,{411,7},60,40,0.8}};
    s.fleets[0].position={411,7};
    for (auto& p:s.planets) p.radar=PlanetaryRadarLevel::None;
    for (auto& p:s.players) p.technology.researchActive=false;
    return s;
}
void generation_and_projection()
{
    auto a=generate_game({501,120,1000,800,25}),b=generate_game({501,120,1000,800,25});
    assert(a.interstellarMediumEnabled && a.interstellarClouds==b.interstellarClouds && a.interstellarClouds.size()==1);
    const auto clouds=a.interstellarClouds; generate_interstellar_medium(a); assert(a.interstellarClouds==clouds);
    a.interstellarClouds.clear(); a.interstellarMediumEnabled=false; generate_interstellar_medium(a);
    assert(a.interstellarClouds==clouds && a.stars[0].position==b.stars[0].position);
    auto view=make_player_view(a,1).state;
    assert(view.interstellarClouds.empty() && view.mediumObserver==1 && view.interstellarMediumEnabled);
    assert(interstellar_density(view,clouds[0].center)==0);
    assert(!generate_game({1,8,400,400,15}).interstellarMediumEnabled);
}
void integration_is_bounded_and_resolves_cloud_crossings()
{
    auto s=medium_fixture(); s.interstellarClouds={{1,{0,0},50,20,0.8}};
    assert(interstellar_density(s,{-100,0})==0 && interstellar_density(s,{100,0})==0);
    assert(near(interstellar_density(s,{0,0}),0.8));
    const auto total=medium_segment_density(s,{-100,0},{100,0});
    assert(near(total,0.8*16*50/15/200));
    assert(near(total,medium_segment_density(s,{100,0},{-100,0})));
    assert(near(total,(medium_segment_density(s,{-100,0},{13,0})*113+medium_segment_density(s,{13,0},{100,0})*87)/200));
    assert(near(medium_segment_density(s,{0,0},{0,0}),0.8));
    s.interstellarClouds={{1,{0,0},50,20,1},{2,{10,2},40,25,0.9},{3,{-20,-3},30,60,0.8},{4,{5,4},70,40,0.7}};
    const auto full=medium_segment_density(s,{-150,-12},{150,12});
    const auto split=(medium_segment_density(s,{-150,-12},{0,0})+medium_segment_density(s,{0,0},{150,12}))*0.5;
    assert(full>0 && full<=1 && near(full,split));
    assert(interstellar_density(s,{0,0})<=1);
}
void fuel_and_movement_use_the_same_segment()
{
    auto s=generate_game({10,32,400,400,20}); auto& f=s.fleets[0];
    f.position={1000,1000}; f.destination=Position{1064,1000}; f.warp=8; f.fuel=50;
    s.interstellarClouds={{1,{1032,1000},20,20,1}};
    auto clear=s; clear.interstellarClouds.clear();
    const auto base=fleet_fuel_consumption_for_segment(clear,clear.fleets[0],*f.destination);
    const auto cost=fleet_fuel_consumption_for_segment(s,f,*f.destination);
    const auto change=fleet_fuel_change_for_segment(s,f,*f.destination);
    const auto bounds=fleet_segment_fuel_bounds(s,f,64);
    assert(cost>base && cost>=bounds.consumedMinimum && cost<=bounds.consumedMaximum && cost<=50);
    const auto result=TurnProcessor{}.process_with_events(s,{});
    const auto& moved=result.state.fleets[0];
    assert(moved.position==Position(1064,1000) && near(moved.fuel,50-change));
    const auto record=std::find_if(result.movementFuel.begin(),result.movementFuel.end(),[&](const auto& x) { return x.fleet==f.id; });
    assert(record!=result.movementFuel.end() && near(record->consumed,cost));
    assert(near(record->consumedMinimum,bounds.consumedMinimum) && near(record->consumedMaximum,bounds.consumedMaximum));
    auto projected=s; project_interstellar_medium(projected,1);
    assert(near(fleet_fuel_consumption_for_segment(projected,f,*f.destination),base));
    projected.players[0].mediumKnowledge={{1,{1032,1000},20,20,1,1,MediumKnowledgeLevel::Characterized,9,10}};
    assert(near(fleet_fuel_consumption_for_segment(projected,f,*f.destination),cost));
    s.interstellarClouds[0].density=0.2;
    assert(near(fleet_fuel_consumption_for_segment(projected,f,*f.destination),cost));
}
void fitted_engines_and_fuel_reserves()
{
    auto s=medium_fixture(); auto& f=s.fleets[0]; f.position={1000,0}; f.fuel=3;
    s.interstellarClouds={{1,{1040,0},80,100,1}};
    for (auto engine:{ShipComponentType::QuickJump5,ShipComponentType::FuelMizer,ShipComponentType::FusionDrive,ShipComponentType::RamScoopDrive}) {
        s.shipDesigns[0].components[0]=engine;
        for (auto warp:{4,8}) {
            f.warp=warp; double previous=0;
            for (int step=1;step<=200;++step) {
                const Position end{1000+step*0.5,0};
                const auto cost=fleet_fuel_consumption_for_segment(s,f,end);
                const auto change=fleet_fuel_change_for_segment(s,f,end);
                const auto bounds=fleet_segment_fuel_bounds(s,f,step*0.5);
                assert(cost+1e-8>=previous && cost>=bounds.consumedMinimum-1e-8 && cost<=bounds.consumedMaximum+1e-8);
                assert(change>=bounds.changeMinimum-1e-8 && change<=bounds.changeMaximum+1e-8); previous=cost;
            }
            const auto paid=fleet_fuel_affordable_segment_distance(s,f,{1100,0},100);
            assert(fleet_fuel_consumption_for_segment(s,f,{1000+paid,0})<=f.fuel+1e-6);
        }
    }
    s.shipDesigns[0].components[0]=ShipComponentType::RamScoopDrive; f.warp=4;
    auto clear=s; clear.interstellarClouds.clear();
    assert(fleet_fuel_change_for_segment(s,f,{1080,0})<fleet_fuel_change_for_segment(clear,f,{1080,0}));
    s.shipDesigns.push_back({33,1,"Conventional escort",ShipHullType::Scout,{ShipComponentType::QuickJump5}});
    f.ships={{f.design,1},{33,1}}; f.warp=8; f.fuel=0;
    assert(fleet_fuel_consumption_for_segment(s,f,{1080,0})>0);
    assert(fleet_fuel_affordable_segment_distance(s,f,{1080,0},80)<1e-6);
}
void active_sensors_are_attenuated_without_changing_passive_reception()
{
    auto s=medium_fixture(); auto detector=s.fleets[0]; detector.position={1000,0};
    Fleet target=detector; target.id=99; target.owner=2;
    const auto nominal=fleet_sensor_range(s,detector); target.position={1000+nominal*0.9,0};
    s.interstellarClouds={{1,{1000,0},1000,1000,1}};
    auto clear=s; clear.interstellarClouds.clear();
    assert(strategic_sensor_range(s,detector)<strategic_sensor_range(clear,detector));
    assert(detector_detects_fleet(clear,detector,target) && !detector_detects_fleet(s,detector,target));
    detector.electronics.mode=EmissionMode::Passive; target.position={1000+nominal*0.05,0};
    assert(detector_detects_fleet(s,detector,target)==detector_detects_fleet(clear,detector,target));
    s.fleets[0]=detector; s.fleets[0].electronics.mode=EmissionMode::Standard;
    observe_reconnaissance(s,s.turn);
    const auto status=std::find_if(s.players[0].pendingSensorStatus.begin(),s.players[0].pendingSensorStatus.end(),
        [&](const auto& x) { return x.fleet==detector.id; });
    assert(status!=s.players[0].pendingSensorStatus.end());
    assert(status->activeRange<nominal && near(status->passiveRange,nominal*0.25) && near(status->emissionRange,nominal*2));
}
void surveys_are_delayed_immutable_and_do_not_repeat_briefings()
{
    auto s=medium_fixture(); observe_interstellar_medium(s,10);
    assert(s.players[0].mediumKnowledge.empty());
    const auto found=std::find_if(s.players[0].pendingPlayerReports.begin(),s.players[0].pendingPlayerReports.end(),[](const auto& r) { return bool(r.mediumObservation); });
    assert(found!=s.players[0].pendingPlayerReports.end()); const auto packet=*found;
    assert(packet.deliveryTurn>10 && packet.mediumObservation->level==MediumKnowledgeLevel::Characterized);
    assert(packet.mediumObservation->densityMinimum==0.8 && packet.mediumObservation->receivedTurn==0);
    s.interstellarClouds[0].center={900,900}; s.interstellarClouds[0].density=0.2; s.fleets.clear();
    s.turn=packet.deliveryTurn;
    const auto delivered=deliver_due_player_reports(s);
    const auto event=std::find_if(delivered.begin(),delivered.end(),[](const auto& e) { return bool(e.mediumObservation); });
    assert(event!=delivered.end() && event->position==Position(411,7));
    assert(s.players[0].mediumKnowledge[0].densityMaximum==0.8 && s.players[0].mediumKnowledge[0].receivedTurn==s.turn);
    auto repeat=packet; repeat.observedTurn=repeat.mediumObservation->observedTurn=s.turn; repeat.deliveryTurn=s.turn+1;
    s.players[0].pendingPlayerReports={repeat}; ++s.turn; assert(deliver_due_player_reports(s).empty());
    assert(s.players[0].mediumKnowledge[0].observedTurn==s.turn-1);
    auto old=repeat; old.mediumObservation->level=MediumKnowledgeLevel::Detected; old.mediumObservation->densityMinimum=0;
    old.mediumObservation->densityMaximum=1; old.observedTurn=old.mediumObservation->observedTurn=10; old.deliveryTurn=++s.turn;
    s.players[0].pendingPlayerReports={old}; assert(deliver_due_player_reports(s).empty());
    assert(s.players[0].mediumKnowledge[0].level==MediumKnowledgeLevel::Characterized);
}
void distant_and_passive_observers()
{
    auto s=medium_fixture(); s.fleets[0].position={490,7}; observe_interstellar_medium(s,10);
    assert(s.players[0].pendingPlayerReports.back().mediumObservation->level==MediumKnowledgeLevel::Detected);
    const auto& x=*s.players[0].pendingPlayerReports.back().mediumObservation;
    assert(x.densityMinimum==0 && x.densityMaximum==1 && x.center!=s.interstellarClouds[0].center);
    s.players[0].pendingPlayerReports.clear(); s.fleets[0].electronics.mode=EmissionMode::Passive;
    observe_interstellar_medium(s,10); assert(s.players[0].pendingPlayerReports.empty());
}
void safe_warp_and_integrated_hull_risk()
{
    auto s=medium_fixture(); auto& f=s.fleets[0];
    s.shipDesigns[0].components[0]=ShipComponentType::FusionDrive;
    f.position={1000,0}; f.warp=8; f.fuel=200; f.destination=Position{1064,0};
    s.interstellarClouds={{1,{1032,0},16,20,1}};
    assert(fleet_overdrive_damage_rate(s,f,8)==0); // both endpoints clear
    const auto expected=12*medium_segment_density(s,f.position,*f.destination);
    assert(expected>0 && near(fleet_segment_damage(s,f,*f.destination),expected));
    const auto result=TurnProcessor{}.process_with_events(s,{});
    assert(result.state.fleets[0].position==*f.destination);
    assert(near(result.state.fleets[0].damagePercent,expected));
    auto half=f; half.position={1032,0};
    assert(near(fleet_segment_damage(s,f,{1032,0})+fleet_segment_damage(s,half,*f.destination),expected));
    assert(near(fleet_medium_damage_rate(s,f,8,0.25),3));
    assert(near(fleet_medium_damage_rate(s,f,8,1),12));
    s.shipDesigns[0].components[0]=ShipComponentType::RamScoopDrive;
    assert(near(fleet_medium_damage_rate(s,f,10,0),18));
    assert(near(fleet_medium_damage_rate(s,f,10,0.25),13.5));
    assert(near(fleet_medium_damage_rate(s,f,10,1),0));
    // A mixed fleet must respect its conventional escort, then handle
    // opposing scoop/conventional curves when their tolerance crosses.
    auto escort=s.shipDesigns[0]; escort.id=33; escort.name="Conventional escort";
    escort.components[0]=ShipComponentType::FusionDrive; s.shipDesigns.push_back(escort);
    f.ships={{f.design,1},{escort.id,1}};
    f.warp=9; const Position end{1081,0};
    const auto mixed=fleet_segment_damage(s,f,end);
    s.shipDesigns[0].components[0]=ShipComponentType::FusionDrive;
    assert(near(mixed,fleet_segment_damage(s,f,end)));
    s.shipDesigns[0].components[0]=ShipComponentType::RamScoopDrive; f.warp=10;
    s.shipDesigns.back().components[0]=ShipComponentType::HighWarpDrive;
    const auto full=fleet_segment_damage(s,f,{1100,0});
    half=f; half.position={1035,0};
    assert(near(full,fleet_segment_damage(s,f,{1035,0})+fleet_segment_damage(s,half,{1100,0}),1e-5));
    s.shipDesigns[0].components[0]=ShipComponentType::FusionDrive;
    f.ships.clear(); f.warp=8; f.damagePercent=99.9;
    const auto stopped=TurnProcessor{}.process_with_events(s,{}).state.fleets[0];
    assert(stopped.damagePercent==100 && stopped.position.x>1016 && stopped.position.x<1064);
    // Hull limit also clips fuel consumption at the actually travelled point.
    assert(near(stopped.fuel,200-fleet_fuel_change_for_segment(s,f,stopped.position),1e-6));
    s.interstellarMediumEnabled=false; f.damagePercent=0;
    assert(fleet_segment_damage(s,f,*f.destination)==0);
}
void received_route_alternatives()
{
    auto s=medium_fixture(); auto f=s.fleets[0]; f.position={0,0}; f.fuel=200; f.warp=8;
    s.shipDesigns[0].components[0]=ShipComponentType::FusionDrive;
    const Position target{64,0};
    s.interstellarClouds={{1,{32,0},12,10,1}};
    assert(medium_route_alternatives(s,f,target,8).size()==1); // hidden geometry
    s.players[0].mediumKnowledge={{1,{32,0},12,10,1,1,MediumKnowledgeLevel::Characterized,9,10}};
    const auto paths=medium_route_alternatives(s,f,target,8);
    assert(paths.size()==3 && paths[0].damage>0);
    assert(paths[1].damage<1e-8 && paths[2].damage<1e-8 && paths[1].distance>64);
    assert(paths[1].destinations.back()==target && paths[1].destinations.size()==3);
    s.interstellarClouds={{2,{500,500},100,100,0.5}};
    const auto hidden=medium_route_alternatives(s,f,target,8);
    assert(paths.size()==hidden.size());
    for (std::size_t i=0;i<paths.size();++i) {
        assert(paths[i].destinations==hidden[i].destinations);
        assert(near(paths[i].fuel,hidden[i].fuel) && near(paths[i].damage,hidden[i].damage));
    }
}
void computer_uses_received_bypasses()
{
    auto s=make_demo_game(); s.turn=10; s.players[0].controller=PlayerController::Computer;
    s.interstellarMediumEnabled=true; s.wormholeRules.spawnChancePerTurn=0;
    s.stars.resize(2); s.planets.resize(2); s.stars[1].position={64,0};
    s.shipDesigns[0].components[0]=ShipComponentType::FusionDrive;
    s.fleets[0].fuel=200; s.fleets[0].telemetry.fuel=200;
    s.fleets[0].telemetry.observedTurn=s.turn;
    s.players[0].surveyedStars={1}; s.players[0].surveyKnowledge.clear();
    s.players[0].mediumKnowledge={{1,{32,0},12,10,1,1,MediumKnowledgeLevel::Characterized,9,10}};
    s.interstellarClouds={{1,{32,0},12,10,1}};
    const auto plan=plan_computer_turn(make_player_view(s,1));
    const auto move=std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o) {
        const auto* m=std::get_if<MoveFleetOrder>(&o); return m && m->fleet==1;
    });
    assert(move!=plan.orders.orders.end()); const auto& route=std::get<MoveFleetOrder>(*move);
    assert(route.warp==8 && !same_position(route.destination,s.stars[1].position));
    assert(route.queuedWaypoints.size()>=3 && route.queuedWaypoints.back().arrivalAction.kind==FleetArrivalActionKind::Refuel);
    auto f=s.fleets[0]; f.warp=route.warp;
    assert(fleet_segment_damage(s,f,route.destination)<1e-8);
    // Planning already assumes automatic service at a received friendly depot.
    // Bypass materialization must use that same fuel, even with an empty tank.
    s.fleets[0].fuel=0; s.fleets[0].telemetry.fuel=0;
    const auto empty=plan_computer_turn(make_player_view(s,1));
    const auto emptyMove=std::find_if(empty.orders.orders.begin(),empty.orders.orders.end(),[](const auto& o) {
        const auto* m=std::get_if<MoveFleetOrder>(&o); return m && m->fleet==1;
    });
    assert(emptyMove!=empty.orders.orders.end());
    const auto& funded=std::get<MoveFleetOrder>(*emptyMove);
    assert(funded.destination==route.destination && funded.queuedWaypoints.size()==route.queuedWaypoints.size());
    s.fleets[0].fuel=200; s.fleets[0].telemetry.fuel=200;
    // Newly received clouds must also revise an acknowledged in-flight
    // programme when a bypass is safe at the same Warp (no speed reduction).
    auto active=s; ++active.turn;
    active.players[0].computer=plan.nextState;
    active.players[0].computer.fleetGoals[0].reviewTurn=active.turn;
    auto& flying=active.fleets[0];
    flying.destination=active.stars[1].position; flying.warp=8;
    flying.waypointQueue={{{0,0},8,{FleetArrivalActionKind::Refuel}}};
    flying.telemetry.destination=flying.destination; flying.telemetry.warp=8;
    flying.telemetry.waypointQueue=flying.waypointQueue; flying.telemetry.observedTurn=active.turn;
    const auto replanned=plan_computer_turn(make_player_view(active,1));
    const auto revised=std::find_if(replanned.orders.orders.begin(),replanned.orders.orders.end(),[](const auto& o) {
        const auto* m=std::get_if<MoveFleetOrder>(&o); return m && m->fleet==1;
    });
    assert(revised!=replanned.orders.orders.end());
    const auto& bypass=std::get<MoveFleetOrder>(*revised);
    assert(bypass.warp==8 && bypass.destination!=*flying.destination && bypass.queuedWaypoints.size()>=3);
    s.interstellarClouds.clear();
    const auto changed=plan_computer_turn(make_player_view(s,1));
    const auto again=std::find_if(changed.orders.orders.begin(),changed.orders.orders.end(),[](const auto& o) {
        const auto* m=std::get_if<MoveFleetOrder>(&o); return m && m->fleet==1;
    });
    assert(again!=changed.orders.orders.end()); const auto& same=std::get<MoveFleetOrder>(*again);
    assert(same.destination==route.destination && same.queuedWaypoints.size()==route.queuedWaypoints.size());
}
GameState arrival_load_fixture(double fuel)
{
    auto s=make_demo_game(); s.turn=10; s.wormholeRules.spawnChancePerTurn=0;
    auto& player=s.players[0]; player.controller=PlayerController::Computer;
    player.race=race_preset(RacePreset::Terran); player.technology.levels.fill(18);
    player.technology.researchActive=false; player.objectKnowledge.clear();
    s.unknownObjects.clear(); s.stars.resize(2); s.planets.resize(2); s.orbitalStations.clear();
    s.stars[1].position={64,0};
    for (auto& star:s.stars) star.variability={};
    for (auto& p:s.planets) {
        p.owner=1; p.population=500000; p.environment={50,50,20}; p.naturalEnvironment=p.environment;
        p.industry=100; p.minerals={10000,10000,10000}; p.productionQueue.clear();
    }
    auto& home=s.planets[0];
    home.engineering.kind=PlanetaryEngineeringKind::OrbitalHabitat;
    home.engineering.work=engineering_work(home.engineering.kind); home.engineering.strength=10;
    home.habitat.population=200000; home.habitat.service=HabitatService::Reserve; home.habitat.reserveYears=2;
    auto& design=s.shipDesigns[0]; design.hull=ShipHullType::SmallFreighter;
    design.components={ShipComponentType::FusionDrive,ShipComponentType::MoleScanner}; design.placements.clear();
    assert(ship_design_available_to_player(s,1,design));
    auto& f=s.fleets[0]; f.warp=8; f.ships={{f.design,10}}; f.fuel=fuel; f.colonists=0; f.minerals={};
    f.telemetry={}; f.telemetry=confirmed_fleet_telemetry(s,f);
    assert(near(fleet_cargo_capacity(s,f),700) && near(f.telemetry.fuel,fuel));
    for (const auto& star:s.stars) set_survey_level(s,1,star.id,SurveyLevel::DeepSurvey,s.turn);
    s.interstellarMediumEnabled=true; s.interstellarClouds={{1,{32,0},12,10,1}};
    player.mediumKnowledge={{1,{32,0},12,10,1,1,MediumKnowledgeLevel::Characterized,9,10}};
    return s;
}
GameState active_mineral_shuttle_fixture(double fuel)
{
    auto s=arrival_load_fixture(fuel); s.planets[0].habitat.population=0;
    auto& f=s.fleets[0]; f.destination=f.position;
    f.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::LoadAllAvailable,0,FleetCargoKind::Ironium};
    f.waypointQueue={{{64,0},8,{FleetArrivalActionKind::UnloadAll,0,FleetCargoKind::Ironium}}};
    f.telemetry={}; f.telemetry=confirmed_fleet_telemetry(s,f);
    auto& computer=s.players[0].computer; computer.initialized=true;
    computer.decisions={{9,1,"Issued mineral shuttle"}};
    computer.fleetGoals={{1,ComputerFleetGoalKind::Resupply,2,9,10}};
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    return s;
}
MoveFleetOrder planned_move(const ComputerTurnPlan& plan)
{
    const auto it=std::find_if(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o) {
        const auto* move=std::get_if<MoveFleetOrder>(&o); return move && move->fleet==1;
    });
    assert(it!=plan.orders.orders.end()); return std::get<MoveFleetOrder>(*it);
}
void arrival_loads_are_funded_through_actual_delivery()
{
    for (const auto cargo:{FleetCargoKind::OrbitalColonists,FleetCargoKind::Ironium})
        for (const double fuel:{20.0,780.0,1200.0}) {
            if (fuel==20 && cargo==FleetCargoKind::OrbitalColonists) continue;
            auto s=cargo==FleetCargoKind::Ironium ? active_mineral_shuttle_fixture(fuel) : arrival_load_fixture(fuel);
            const auto plan=plan_computer_turn(make_player_view(s,1));
            const auto route=planned_move(plan);
            if (fuel==20) {
                assert(route.clearRoute && route.queuedWaypoints.empty());
                const auto held=TurnProcessor{}.process(s,{plan.orders});
                assert(same_position(held.fleets[0].position,{0,0}) && near(held.fleets[0].minerals.ironium,0));
                assert(!held.fleets[0].fuelStalled && near(held.fleets[0].fuel,fuel));
                continue;
            }
            assert(!route.clearRoute && route.arrivalAction.kind==FleetArrivalActionKind::LoadAllAvailable);
            assert(route.arrivalAction.cargo==cargo && !route.queuedWaypoints.empty());
            if (fuel==780) assert(route.queuedWaypoints.back().warp<8);
            else assert(route.queuedWaypoints.size()>1); // load -> actual bypass -> unload
            auto state=s; bool sawLoad=false,delivered=false;
            for (int year=0;year<8 && !delivered;++year) {
                state=TurnProcessor{}.process(state,year==0 ? std::vector<PlayerOrders>{plan.orders} : std::vector<PlayerOrders>{});
                const auto& f=state.fleets[0];
                sawLoad=sawLoad || (cargo==FleetCargoKind::OrbitalColonists ? f.colonists==70000 : near(f.minerals.ironium,700));
                assert(!f.fuelStalled && f.damagePercent==0 && f.fuel>=0 && f.fuel<=fuel);
                const bool empty=cargo==FleetCargoKind::OrbitalColonists ? f.colonists==0 : near(f.minerals.ironium,0);
                delivered=sawLoad && same_position(f.position,{64,0}) && empty;
            }
            assert(sawLoad && delivered && state.fleets[0].fuel>0);
        }
}
void initial_depot_service_preserves_active_loading()
{
    enum class Case { Missing, Hidden, ShipyardOnly, Received, StalePosition, LaterWaypoint };
    for (const auto kind:{Case::Missing,Case::Hidden,Case::ShipyardOnly,Case::Received,Case::StalePosition,Case::LaterWaypoint}) {
        auto s=active_mineral_shuttle_fixture(20);
        if (kind!=Case::Missing) {
            auto depot=make_demo_game().orbitalStations[0];
            depot.modules=kind==Case::ShipyardOnly ? std::vector<OrbitalStationModule>{OrbitalStationModule::Shipyard}
                : std::vector<OrbitalStationModule>{OrbitalStationModule::RefuelingDepot};
            if (kind==Case::LaterWaypoint) depot.planet=2;
            s.orbitalStations.push_back(depot);
            if (kind!=Case::Hidden) set_survey_level(s,1,depot.planet,SurveyLevel::DeepSurvey,s.turn);
        }
        if (kind==Case::StalePosition) {
            s.fleets[0].telemetry.observedTurn=9;
            s.players[0].computer.decisions={{8,1,"Old loading programme"}};
            s.players[0].computer.fleetGoals[0].issuedTurn=8;
        }
        const auto view=make_player_view(s,1);
        const auto plan=plan_computer_turn(view); const auto route=planned_move(plan);
        assert(near(s.fleets[0].fuel,20)); // Planning never fills the physical tank.
        if (kind!=Case::Received) {
            assert(route.clearRoute); // No free credit from missing/hidden services or stale motion.
            continue;
        }
        assert(!route.clearRoute && route.warp==8 && route.queuedWaypoints.size()>1);
        assert(route.arrivalAction.kind==FleetArrivalActionKind::LoadAllAvailable && route.arrivalAction.cargo==FleetCargoKind::Ironium);
        assert(route.queuedWaypoints.back().arrivalAction.kind==FleetArrivalActionKind::UnloadAll);
        // Changing only hidden station truth cannot change the received plan.
        auto hidden=s; hidden.orbitalStations.clear();
        const auto same=planned_move(plan_computer_turn(make_player_view(hidden,1)));
        assert(!same.clearRoute && same.warp==route.warp && same.destination==route.destination
            && same.queuedWaypoints.size()==route.queuedWaypoints.size());
        const auto deliver=[&](GameState state,bool revised) {
            bool loaded=false;
            for (int year=0;year<8;++year) {
                state=TurnProcessor{}.process(state,revised && year==0?std::vector<PlayerOrders>{plan.orders}:std::vector<PlayerOrders>{});
                const auto& f=state.fleets[0]; loaded=loaded || near(f.minerals.ironium,700);
                assert(!f.fuelStalled && f.fuel>=0 && f.fuel<=fleet_fuel_capacity(state,f));
                if (revised) assert(f.damagePercent==0);
                if (loaded && same_position(f.position,{64,0}) && near(f.minerals.ironium,0)) {
                    assert(f.fuel>0); return;
                }
            }
            assert(false && "Automatically refuelled shuttle must deliver its cargo");
        };
        deliver(s,false); // The original onboard programme is physically funded.
        deliver(s,true);  // The AI preserves loading and safely bypasses the cloud.
    }
}
int main()
{
    initial_depot_service_preserves_active_loading();
    arrival_loads_are_funded_through_actual_delivery();
    safe_warp_and_integrated_hull_risk(); received_route_alternatives(); computer_uses_received_bypasses();
    generation_and_projection(); integration_is_bounded_and_resolves_cloud_crossings();
    fuel_and_movement_use_the_same_segment(); fitted_engines_and_fuel_reserves();
    active_sensors_are_attenuated_without_changing_passive_reception();
    surveys_are_delayed_immutable_and_do_not_repeat_briefings(); distant_and_passive_observers();
    std::cout<<"interstellar medium tests passed\n";
}
