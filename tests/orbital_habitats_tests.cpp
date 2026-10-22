#include "suns/orbital_habitats.hpp"
#include "suns/planetary_engineering.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/production.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/computer_player.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace suns;
using E=PlanetaryEngineeringKind;
using A=EngineeringAction;
namespace {
GameState fixture()
{
    auto s=make_demo_game(); s.turn=10;s.wormholeRules.spawnChancePerTurn=0;
    s.players[0].race=race_preset(RacePreset::Terran);s.players[0].technology.levels.fill(18);
    s.players[0].technology.progress.fill(0);s.players[0].technology.researchActive=false;
    auto& p=s.planets[0];p.population=1000000;p.industry=100;p.minerals={10000,10000,10000};p.productionQueue.clear();
    p.environment={50,50,20};p.naturalEnvironment=p.environment;
    s.orbitalStations[0].hull=OrbitalStationHullType::Starbase;
    for(auto& star:s.stars)star.variability={};
    s.fleets.resize(1);auto& f=s.fleets[0];f.destination.reset();f.task=FleetTask::None;f.position=s.stars[0].position;
    s.shipDesigns[0].hull=ShipHullType::SmallFreighter;s.shipDesigns[0].components={ShipComponentType::FuelMizer,ShipComponentType::MoleScanner};s.shipDesigns[0].placements.clear();
    f.ships={{f.design,10}};f.minerals={};f.colonists=0;f.telemetry=confirmed_fleet_telemetry(s,f);
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    return s;
}
void finish(GameState& s)
{
    auto& p=s.planets[0];
    assert(apply_planetary_engineering_order(s,p,{1,E::OrbitalHabitat,A::StartOrResume,10}));
    for(unsigned year=0;year<60;++year) {
        const auto before=p.engineering.work;
        (void)resolve_colony_production(s,p,true);assert(p.engineering.work==before+80);++s.turn;
    }
    assert(habitat_completed(p) && !p.habitat.population && p.habitat.reserveYears==3);
    refresh_planetary_engineering(s);set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
}
void construction_is_paid_unique_and_persistent()
{
    auto s=fixture();auto& p=s.planets[0];
    for(const auto field:{ResearchField::Energy,ResearchField::Construction,ResearchField::Biology}) {
        auto locked=s;locked.players[0].technology.levels[std::size_t(field)]=field==ResearchField::Energy?15:field==ResearchField::Construction?17:13;
        assert(!planetary_engineering_available(locked,1,E::OrbitalHabitat));
    }
    const auto minerals=p.minerals;
    assert(apply_planetary_engineering_order(s,p,{1,E::OrbitalHabitat,A::StartOrResume,0}));
    assert(p.engineering.strength==kEngineeringStrength); // Housing has no environmental output/off control.
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalMirrors,A::StartOrResume,10}));
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalHabitat,A::StartOrResume,10}));
    (void)resolve_colony_production(s,p,true);++s.turn;
    assert(p.engineering.work==80 && p.engineering.invested.ironium==8);
    assert(apply_planetary_engineering_order(s,p,{1,E::OrbitalHabitat,A::Pause,10}));
    (void)resolve_colony_production(s,p,true);++s.turn;assert(p.engineering.work==80);
    assert(apply_planetary_engineering_order(s,p,{1,E::OrbitalHabitat,A::StartOrResume,10}));
    for(unsigned year=1;year<60;++year){(void)resolve_colony_production(s,p,true);++s.turn;}
    assert(habitat_completed(p) && p.minerals.ironium==minerals.ironium-480 && p.minerals.boranium==minerals.boranium-240);
    assert(p.engineering.invested.germanium==480 && p.habitat.reserveYears==3 && !p.habitat.population);
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalHabitat,A::SetOutput,0}));
}
void maintenance_reserve_growth_and_recovery()
{
    auto s=fixture();finish(s);auto& p=s.planets[0];p.habitat.population=200000;
    const auto before=p.minerals;const auto output=colony_output(p);
    const auto resolved=resolve_colony_production(s,p,true);
    assert(p.habitat.service==HabitatService::Normal && p.habitat.productionPaid==2);
    assert(p.minerals.ironium==before.ironium-1 && resolved.researchPoints==0);
    assert(projected_orbital_growth(p)==9000 && colony_output(p)==output);
    (void)resolve_colony_production(s,p,true);assert(p.minerals.ironium==before.ironium-1);
    p.habitat.population=2000000;assert(projected_orbital_growth(p)==0);
    p.habitat.population=200000;
    for(unsigned year=0;year<3;++year) {
        ++s.turn;(void)resolve_colony_production(s,p,false,false);
        assert(p.habitat.reserveYears==2-year && p.habitat.population==200000);
        assert(!projected_orbital_growth(p) && p.habitat.service==HabitatService::Reserve);
    }
    ++s.turn;(void)resolve_colony_production(s,p,false,false);
    assert(p.habitat.population==180000 && p.habitat.populationLost==20000);
    (void)resolve_colony_production(s,p,false,false);assert(p.habitat.population==180000);
    ++s.turn;(void)resolve_colony_production(s,p,true);
    assert(p.habitat.service==HabitatService::Normal && p.habitat.reserveYears==1 && p.habitat.productionPaid==4);
    assert(!apply_planetary_engineering_order(s,p,{1,E::OrbitalHabitat,A::Dismantle,10}));
    ++s.turn;p.minerals={};(void)resolve_colony_production(s,p,true);
    assert(p.habitat.service==HabitatService::Reserve && p.habitat.reserveYears==0);
    const auto depleted=p.engineering.work;
    s.players[0].technology.levels[std::size_t(ResearchField::Biology)]=13;
    ++s.turn;p.minerals={100,100,100};(void)resolve_colony_production(s,p,true);
    assert(p.habitat.service==HabitatService::Failing && p.engineering.work==depleted);
}
void orbital_output_is_counted_once_and_upkeep_precedes_research()
{
    auto s=fixture();finish(s);auto& p=s.planets[0];p.population=500000;p.industry=0;p.habitat.population=2000000;
    s.players[0].technology.researchActive=true;s.players[0].technology.researchAllocationPercent=100;
    const auto extraction=projected_mineral_mining(s,p);
    const auto before=p.minerals;
    const auto produced=resolve_colony_production(s,p,true);
    assert(colony_output(p)==5 && p.habitat.productionPaid==2 && produced.researchPoints==3);
    assert(p.minerals.ironium==before.ironium-1 && projected_mineral_mining(s,p).ironium==extraction.ironium);
    ++s.turn;p.population=0;p.habitat.population=500000;p.minerals={100,100,100};
    (void)resolve_colony_production(s,p,true);
    assert(p.habitat.service==HabitatService::Reserve && !p.habitat.productionPaid && !colony_output(p));
    assert(p.minerals.ironium==100 && !projected_mineral_mining(s,p).ironium);
}
void cargo_is_atomic_and_routes_can_evacuate_without_station()
{
    auto s=fixture();finish(s);auto& p=s.planets[0];auto& f=s.fleets[0];
    f.telemetry=confirmed_fleet_telemetry(s,f);
    const auto total=p.population;
    const auto moved=projected_cargo_order_state(s,{1,{TransferCargoOrder{{1,0},{1,0,true},200000,{}}}});
    assert(moved.planets[0].population==total-200000 && moved.planets[0].habitat.population==200000);
    p=moved.planets[0];
    auto invalid=projected_cargo_order_state(s,{1,{TransferCargoOrder{{1,0},{1,0,true},2000000,{}}}});
    assert(invalid.planets[0].habitat.population==200000);
    invalid=projected_cargo_order_state(s,{1,{TransferCargoOrder{{1,0,true},{0,f.id},1,{1,0,0}}}});
    assert(!invalid.fleets[0].colonists && invalid.planets[0].habitat.population==200000);
    s.orbitalStations.clear();
    const auto mass=fleet_gross_mass(s,f);
    const auto evacuated=projected_cargo_order_state(s,{1,{TransferCargoOrder{{1,0,true},{0,f.id},2000,{}}}});
    assert(evacuated.planets[0].habitat.population==198000 && evacuated.fleets[0].colonists==2000);
    assert(std::abs(fleet_gross_mass(evacuated,evacuated.fleets[0])-mass-20)<1e-6);
    const auto blocked=projected_cargo_order_state(s,{1,{TransferCargoOrder{{1,0,true},{1,0},2000,{}}}});
    assert(blocked.planets[0].habitat.population==200000);
    p.population=0;p.habitat.population=1000;set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    auto result=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{f.id,f.position,1,{FleetArrivalActionKind::LoadAllAvailable,0,FleetCargoKind::OrbitalColonists}}}}});
    assert(result.fleets[0].colonists==1000 && result.planets[0].habitat.population==0);
    assert(result.planets[0].engineering.work==4800 && colony_output(result.planets[0])==0);
    auto delivered=fixture();finish(delivered);auto& receiver=delivered.planets[0];receiver.habitat.population=kOrbitalPopulationCapacity-50;
    delivered.fleets[0].colonists=100;
    const PlayerOrders unload{1,{MoveFleetOrder{1,delivered.fleets[0].position,1,{FleetArrivalActionKind::UnloadAll,0,FleetCargoKind::OrbitalColonists}}}};
    const auto filled=TurnProcessor{}.process(delivered,{unload});
    assert(filled.planets[0].habitat.population==kOrbitalPopulationCapacity && filled.fleets[0].colonists==50);
    const auto replayed=TurnProcessor{}.process(delivered,{unload});
    assert(replayed.planets[0].habitat==filled.planets[0].habitat && replayed.fleets[0].colonists==filled.fleets[0].colonists);

}
void received_evidence_and_ai_are_not_host_truth()
{
    auto s=fixture();finish(s);auto& p=s.planets[0];p.habitat.population=200000;
    (void)resolve_colony_production(s,p,true);++s.turn;set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    s.players[0].controller=PlayerController::Computer;
    const auto original=plan_computer_turn(make_player_view(s,1));
    const auto received=make_player_view(s,1).state;
    p.habitat.population=1;p.habitat.reserveYears=0;p.habitat.service=HabitatService::Failing;p.minerals={};p.industry=1;
    const auto hidden=make_player_view(s,1).state;
    assert(hidden.planets[0].habitat==received.planets[0].habitat && hidden.planets[0].minerals.ironium==received.planets[0].minerals.ironium);
    assert(hidden.planets[0].industry==received.planets[0].industry);
    assert(plan_computer_turn(make_player_view(s,1)).nextState==original.nextState);
    ++s.turn;
    const auto staleView=make_player_view(s,1).state;
    const auto staleTransfer=projected_cargo_order_state(staleView,{1,{TransferCargoOrder{{1,0},{1,0,true},1000,{}}}});
    assert(staleTransfer.planets[0].habitat.population==staleView.planets[0].habitat.population);
    queue_survey_report(s,1,1,0,s.stars[0].position,s.turn,SurveyLevel::DeepSurvey,true);
    (void)deliver_due_survey_reports(s);
    assert(make_player_view(s,1).state.planets[0].habitat.population==1);
}
void delivered_battle_evidence_survives_late_survey_delivery()
{
    auto s=fixture();finish(s);auto& p=s.planets[0];p.habitat.population=200000;
    (void)resolve_colony_production(s,p,true);++s.turn;set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    ++s.turn;
    queue_survey_report(s,1,1,0,s.stars[0].position,s.turn+1,SurveyLevel::DeepSurvey);
    auto& older=s.players[0].pendingSurveyReports.back();older.deliveryTurn=s.turn+2;
    const auto late=older.deliveryTurn;
    PendingPlayerReport battle;battle.kind=PlayerReportKind::SpaceBattle;battle.observedTurn=s.turn+1;battle.deliveryTurn=battle.observedTurn;
    battle.battle.emplace();battle.battle->observedTurn=battle.observedTurn;
    SpaceBattleUnit unit;unit.owner=1;unit.station=s.orbitalStations[0].id;unit.initialShips=1;unit.survivingShips=0;
    battle.battle->units.push_back(unit);s.players[0].pendingPlayerReports.push_back(battle);
    s.orbitalStations.clear();s.turn=battle.deliveryTurn;(void)deliver_due_player_reports(s);
    auto received=make_player_view(s,1).state;
    assert(received.orbitalStations.empty() && !engineering_station_available(received,received.planets[0]));
    assert(received.planets[0].habitat.population==200000 && projected_orbital_growth(received.planets[0])==0);
    s.turn=late;(void)deliver_due_survey_reports(s);received=make_player_view(s,1).state;
    assert(received.orbitalStations.empty() && !engineering_station_available(received,received.planets[0]));
    // A genuinely later final surface report can acknowledge a rebuilt station.
    auto station=fixture().orbitalStations[0];station.id=++s.nextOrbitalStationId;s.orbitalStations.push_back(station);
    queue_survey_report(s,1,1,0,s.stars[0].position,s.turn,SurveyLevel::DeepSurvey,true);
    (void)deliver_due_survey_reports(s);received=make_player_view(s,1).state;
    assert(received.orbitalStations.size()==1 && engineering_station_available(received,received.planets[0]));
    // A battle from earlier in the same year cannot undo the final-phase rebuild.
    battle.observedTurn=s.turn;battle.battle->units[0].station=station.id;
    apply_received_station_battle(s.players[0],battle);
    assert(make_player_view(s,1).state.orbitalStations.size()==1);
}
void computer_builds_seeds_and_pays_through_ordinary_orders()
{
    auto s=fixture();auto& player=s.players[0];player.controller=PlayerController::Computer;player.race.radiationImmune=true;
    s.planets[0].population=population_capacity(s,s.planets[0],s.turn);
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    const auto plan=plan_computer_turn(make_player_view(s,1));
    assert(std::any_of(plan.orders.orders.begin(),plan.orders.orders.end(),[](const auto& o){
        const auto* project=std::get_if<SetPlanetaryEngineeringOrder>(&o);return project && project->kind==E::OrbitalHabitat;
    }));
    const auto built=TurnProcessor{}.process(s,{plan.orders});
    assert(built.planets[0].engineering.kind==E::OrbitalHabitat && built.planets[0].engineering.work<=80
        && built.planets[0].engineering.invested.ironium>0);
    s=fixture();s.players[0].controller=PlayerController::Computer;finish(s);
    const auto seed=plan_computer_turn(make_player_view(s,1));
    assert(std::any_of(seed.orders.orders.begin(),seed.orders.orders.end(),[](const auto& o){
        const auto* transfer=std::get_if<TransferCargoOrder>(&o);return transfer && transfer->destination.orbital && transfer->colonists==200000;
    }));
    const auto seeded=TurnProcessor{}.process(s,{seed.orders});
    assert(seeded.planets[0].habitat.population==209000 && seeded.planets[0].habitat.productionPaid==2);
    s=fixture();finish(s);s.players[0].controller=PlayerController::Computer;s.unknownObjects.clear();s.players[0].objectKnowledge.clear();
    s.planets[0].habitat.population=200000;s.planets[0].habitat.service=HabitatService::Reserve;s.planets[0].habitat.reserveYears=2;
    s.orbitalStations.clear();s.stars[1].position={50,0};s.planets[1].owner=1;s.planets[1].population=500000;s.planets[1].environment={50,50,20};
    s.fleets[0].fuel=fleet_fuel_capacity(s,s.fleets[0]);s.fleets[0].telemetry=confirmed_fleet_telemetry(s,s.fleets[0]);
    for(const auto& star:s.stars)set_survey_level(s,1,star.id,SurveyLevel::DeepSurvey,s.turn);
    const auto rescue=plan_computer_turn(make_player_view(s,1));
    assert(std::any_of(rescue.orders.orders.begin(),rescue.orders.orders.end(),[](const auto& o){
        const auto* move=std::get_if<MoveFleetOrder>(&o);return move && move->arrivalAction.cargo==FleetCargoKind::OrbitalColonists
            && move->arrivalAction.kind==FleetArrivalActionKind::LoadAllAvailable && !move->queuedWaypoints.empty();
    }));

}
void surface_bombardment_preserves_orbital_colony()
{
    auto s=fixture();finish(s);auto& p=s.planets[0];p.population=100;p.habitat.population=200000;
    s.players.push_back({2,"Bomber"});s.shipDesigns.push_back({90,2,"Surface bomber",ShipHullType::Destroyer,
        {ShipComponentType::FuelMizer,ShipComponentType::LadyFingerBomb}});
    auto& f=s.fleets[0];f.owner=2;f.design=90;f.ships={{90,1}};f.task=FleetTask::Bombardment;
    const auto engineering=p.engineering;
    resolve_bombardments(s,s.turn+1);
    assert(p.population==0 && p.owner==1 && p.habitat.population==200000);
    assert(p.engineering.work==engineering.work && p.engineering.invested.ironium==engineering.invested.ironium);

}
void orbital_only_colony_is_a_real_defender_and_capture_preserves_shell()
{
    auto s=fixture();finish(s);auto& p=s.planets[0];p.population=0;p.habitat.population=200000;p.habitat.service=HabitatService::Normal;
    s.players.push_back({2,"Attacker"});s.players[1].technology.levels.fill(18);
    auto& f=s.fleets[0];f.owner=2;s.shipDesigns[0].owner=2;f.colonists=1;f.ships={{f.design,500}};f.telemetry={};
    s.orbitalStations.clear();
    auto lost=TurnProcessor{}.process(s,{{2,{TransferCargoOrder{{0,f.id},{1,0},1,{}}}}});
    assert(lost.planets[0].owner==1 && lost.planets[0].habitat.population>0);
    bool captured=false;
    for(unsigned seed=1;seed<20 && !captured;++seed) {
        s.galaxySeed=seed;f.colonists=1000000;
        const auto won=TurnProcessor{}.process(s,{{2,{TransferCargoOrder{{0,f.id},{1,0},1000000,{}}}}});
        if(won.planets[0].owner==2) {
            assert(!won.planets[0].habitat.population && won.planets[0].engineering.work==4800 && won.planets[0].engineering.builder==1);
            assert(won.planets[0].engineering.invested.ironium==480);captured=true;
        }
    }
    assert(captured);
}
}
int main()
{
    construction_is_paid_unique_and_persistent();maintenance_reserve_growth_and_recovery();
    orbital_output_is_counted_once_and_upkeep_precedes_research();
    cargo_is_atomic_and_routes_can_evacuate_without_station();received_evidence_and_ai_are_not_host_truth();
    delivered_battle_evidence_survives_late_survey_delivery();
    computer_builds_seeds_and_pays_through_ordinary_orders();
    surface_bombardment_preserves_orbital_colony();
    orbital_only_colony_is_a_real_defender_and_capture_preserves_shell();
}
