#include "race_file.hpp"
#include "save_game.hpp"
#include "suns/computer_player.hpp"
#include "suns/mining.hpp"
#include "suns/native_orbital.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cassert>
#include <cmath>

using namespace suns;
namespace {
EmpireSetup native(PlayerController controller)
{
    EmpireSetup setup{"Orbital civilization"};
    setup.primaryTrait=PrimaryRaceTrait::HabitatCivilization;setup.controller=controller;
    return setup;
}
GameState campaign(PlayerController controller,std::uint64_t seed=17)
{
    GalaxyConfig config;config.seed=seed;return generate_campaign(config,{native(controller)});
}
std::size_t colonies(const GameState& state,PlayerId player=1)
{
    return std::count_if(state.planets.begin(),state.planets.end(),[&](const auto& p){return p.owner==player;});
}
QByteArray packet(const ComputerTurnPlan& plan,std::uint64_t turn)
{
    QTemporaryDir dir;QString error;const auto path=dir.filePath("orders.sunsorders");
    TurnOrderFileData data{50,turn,17,plan.orders,{}};
    for(const auto& order:plan.orders.orders){(void)order;data.descriptions.append("Ordinary order");}
    assert(write_turn_order_file(path,data,error));
    QFile file(path);assert(file.open(QIODevice::ReadOnly));return file.readAll();
}
void paid_start_is_identical_for_both_controllers_and_race_import()
{
    for(const auto controller:{PlayerController::Human,PlayerController::Computer}) {
        QTemporaryDir dir;QString error;const auto path=dir.filePath("orbital.sunsrace");
        const auto setup=native(controller);assert(write_race_file(path,setup,error));
        EmpireSetup imported;assert(read_race_file(path,imported,error));imported.controller=controller;
        auto state=generate_campaign({},{{"Surface"},imported,{"Other surface"}});
        const auto p=std::find_if(state.planets.begin(),state.planets.end(),[](const auto& p){return p.owner==2;});
        assert(p!=state.planets.end());
        assert(p->nativeOrbit.population==25'000 && !p->population && !p->industry && !p->mines);
        assert(p->minerals.ironium==100 && p->minerals.boranium==100 && p->minerals.germanium==100);
        const auto* station=native_orbital_station(state,*p);assert(station && station->hull==OrbitalStationHullType::OrbitalDock);
        assert(station_radiation_screen(*station)==20 && p->radar==PlanetaryRadarLevel::Full);
        assert(std::count_if(state.fleets.begin(),state.fleets.end(),[](const auto& f){return f.owner==2;})==1);
        const auto miner=std::find_if(state.fleets.begin(),state.fleets.end(),[](const auto& f){return f.owner==2;});
        assert(fleet_can_remote_mine(state,*miner) && miner->task==FleetTask::RemoteMining);
        assert(miner->fuel==fleet_fuel_capacity(state,*miner));
        assert(technology_level(state,2,ResearchField::Electronics)==1);
        for(const auto field:{ResearchField::Energy,ResearchField::Propulsion,ResearchField::Construction})
            assert(technology_level(state,2,field)==0);
        assert(colonies(state)==1); // Player 1's ordinary capital is unaffected.
        assert(state.players[1].controller==controller);
        const auto received=make_player_view(state,2);
        assert(received.state.players.front().history.back().population==25'000);
    }
    auto state=campaign(PlayerController::Human);
    // Without an actual miner, neither surface installations nor orbital
    // residents supply ore. This uses a real generated campaign, not a kernel.
    state.fleets.clear();const auto before=state.planets.front().minerals;
    state=TurnProcessor{}.process(state,{});const auto& p=state.planets.front();
    assert(p.minerals.ironium==before.ironium-p.nativeOrbit.mineralUnitsPaid);
    assert(p.minerals.boranium==before.boranium-p.nativeOrbit.mineralUnitsPaid);
    assert(p.minerals.germanium==before.germanium-p.nativeOrbit.mineralUnitsPaid);
}
void human_replaces_a_miner_and_founds_an_orbit_through_pbem_orders()
{
    GalaxyConfig config;config.seed=17;
    auto state=generate_campaign(config,{{"Local host"},native(PlayerController::Human)});
    const auto home=std::find_if(state.planets.begin(),state.planets.end(),[](const auto& p){return p.owner==2;})->id;
    const auto homePlanet=[&]() -> const Planet& {return *find_planet(state,home);};
    const auto minerDesign=state.fleets.back().design;
    state=TurnProcessor{}.process(state,{{2,{SetResearchAllocationOrder{0},QueueShipDesignOrder{home,minerDesign}}}});
    assert(state.fleets.size()==2 && !homePlanet().productionQueue.empty());
    for(unsigned year=0;year<8 && state.fleets.size()<3;++year)state=TurnProcessor{}.process(state,{});
    assert(state.fleets.size()==3 && homePlanet().productionQueue.empty());
    const auto replacement=state.fleets.back().id;
    state=TurnProcessor{}.process(state,{{2,{SetRemoteMiningOrder{replacement,true}}}});
    const auto settler=std::find_if(state.shipDesigns.begin(),state.shipDesigns.end(),[](const auto& d){return d.owner==2 && ship_design_can_found_native_orbit(d);});
    assert(settler!=state.shipDesigns.end());const auto design=settler->id;
    state=TurnProcessor{}.process(state,{{2,{QueueShipDesignOrder{home,design}}}});
    for(unsigned year=0;year<80 && state.fleets.size()<4;++year)state=TurnProcessor{}.process(state,{});
    assert(state.fleets.size()==4 && homePlanet().productionQueue.empty());
    const auto fleet=state.fleets.back().id;
    const StarSystem* target=nullptr;double nearest=1e9;
    for(const auto& star:state.stars)if(!find_planet_at_star(state,star.id)->owner) {
        const auto dose=native_orbital_radiation_status(state,2,star.id,1000,native_orbital_founding_loadout());
        const auto distance=distance_between(state.fleets.back().position,star.position);
        auto loaded=state.fleets.back();loaded.colonists=1000;
        if(dose && !dose->annualLoss && distance<nearest
            && fleet_fuel_consumption_for_distance(state,loaded,distance)<=loaded.fuel){target=&star;nearest=distance;}
    }
    assert(target);const auto destination=target->position;
    SaveGameData host;host.state=state;host.mode=SessionMode::Host;host.campaignId=50;
    host.turnToken=17;host.playerTokens={{1,17},{2,18}};host.pendingOrders={1,{}};
    QTemporaryDir dir;QString error;const auto path=dir.filePath("player.suns");
    assert(write_save_game_file(path,make_player_turn(host,2),error));SaveGameData player;
    assert(read_save_game_file(path,player,error));assert(player.state.unknownObjects.empty());
    TurnOrderFileData data{50,state.turn,18,{2,{
        TransferCargoOrder{{home,0,true},{0,fleet},1000,{}},
        MoveFleetOrder{fleet,destination,5,{FleetArrivalActionKind::Colonize}}}}, {"Load residents","Found orbit"}};
    const auto orders=dir.filePath("found.sunsorders");assert(write_turn_order_file(orders,data,error));
    TurnOrderFileData decoded;assert(read_turn_order_file(orders,decoded,error));
    assert(validate_turn_submission(host,decoded).isEmpty());
    const auto population=homePlanet().nativeOrbit.population;
    state=TurnProcessor{}.process(state,{decoded.orders});
    assert(homePlanet().nativeOrbit.population-homePlanet().nativeOrbit.populationGrown==population-1000);
    for(unsigned year=0;year<30 && colonies(state,2)<2;++year)state=TurnProcessor{}.process(state,{});
    assert(colonies(state,2)==2 && state.orbitalStations.size()==3);
    assert(std::none_of(state.fleets.begin(),state.fleets.end(),[&](const auto& f){return f.id==fleet;}));
    for(const auto& p:state.planets)if(p.owner==2)assert(!p.population && p.nativeOrbit.population && !p.industry && !p.mines);
}
void actual_computer_campaign_survives_reload_and_supplies_its_second_colony(std::uint64_t seed)
{
    GalaxyConfig config;config.seed=seed;
    auto state=generate_campaign(config,{{"Local host"},native(PlayerController::Computer)});
    const auto home=std::find_if(state.planets.begin(),state.planets.end(),[](const auto& p){return p.owner==2;})->id;
    QTemporaryDir dir;QString error;const auto path=dir.filePath("host.suns");
    bool replacement=false,second=false,supplied=false;
    for(unsigned year=0;year<160;++year) {
        replacement |= std::count_if(state.fleets.begin(),state.fleets.end(),[](const auto& f){return f.owner==2;})>=2;
        second |= colonies(state,2)>=2;
        if(second)for(const auto& p:state.planets)if(p.owner==2 && !p.homeworld) {
            const auto* star=find_star(state,p.star);
            supplied |= p.nativeOrbit.service==HabitatService::Normal && std::any_of(state.fleets.begin(),state.fleets.end(),[&](const auto& f){
                return f.owner==2 && f.task==FleetTask::RemoteMining && same_position(f.position,star->position) && fleet_can_remote_mine(state,f);
            });
        }
        assert(computer_state_valid(state.players[1],state.turn));
        if(year==20 || year==60) {
            const auto plan=plan_computer_turn(make_player_view(state,2));
            auto hidden=state;
            auto hiddenHome=std::find_if(hidden.planets.begin(),hidden.planets.end(),[&](const auto& p){return p.id==home;});
            hiddenHome->minerals={10000,1,9999};
            for(auto& p:hidden.planets)if(!p.owner){p.environment={0,0,100};p.minerals={999,999,999};}
            const auto other=plan_computer_turn(make_player_view(hidden,2));
            assert(plan.nextState==other.nextState && packet(plan,state.turn)==packet(other,state.turn));
            SaveGameData host;host.state=state;host.mode=SessionMode::Host;
            host.campaignId=50;host.turnToken=17;host.playerTokens={{1,17}};host.pendingOrders={1,{}};
            assert(write_save_game_file(path,host,error));SaveGameData loaded;assert(read_save_game_file(path,loaded,error));
            const auto resumed=plan_computer_turn(make_player_view(loaded.state,2));
            assert(plan.nextState==resumed.nextState && packet(plan,state.turn)==packet(resumed,state.turn));
            const auto next=resolve_controlled_campaign_turn(state,{{1,{}}});
            const auto continued=resolve_controlled_campaign_turn(loaded.state,{{1,{}}});
            assert(next.state.players[1].computer==continued.state.players[1].computer);
            assert(find_planet(next.state,home)->productionQueue==find_planet(continued.state,home)->productionQueue);
            state=continued.state;
        } else state=resolve_controlled_campaign_turn(state,{{1,{}}}).state;
        if(year>=60 && replacement && second && supplied)break;
    }
    assert(replacement && second && supplied);
}
void sparse_bootstrap_preserves_paid_exploration()
{
    auto state=generate_campaign(galaxy_preset(GalaxySize::Medium,GalaxyDensity::Sparse,2),
        {{"Local host"},native(PlayerController::Computer)});
    const auto home=std::find_if(state.planets.begin(),state.planets.end(),[](const auto& p){return p.owner==2;})->id;
    const auto homePosition=find_star(state,find_planet(state,home)->star)->position;
    const auto view=make_player_view(state,2);
    const auto concentration=planet_mineral_concentration(view.state,*find_planet(view.state,home)).ironium;
    assert(concentration>27 && concentration<28); // Unmodified review seed2: home184, I27.09%.
    QTemporaryDir dir;QString error;const auto path=dir.filePath("sparse-host.suns");
    bool exploring=false;
    for(unsigned year=0;year<12;++year) {
        if(year==3) {
            SaveGameData host;host.state=state;host.mode=SessionMode::Host;host.campaignId=50;
            host.turnToken=17;host.playerTokens={{1,17}};host.pendingOrders={1,{}};
            assert(write_save_game_file(path,host,error));SaveGameData loaded;assert(read_save_game_file(path,loaded,error));
            const auto before=plan_computer_turn(make_player_view(state,2)),after=plan_computer_turn(make_player_view(loaded.state,2));
            assert(before.nextState==after.nextState && packet(before,state.turn)==packet(after,state.turn));
            state=loaded.state;
        }
        state=resolve_controlled_campaign_turn(state,{{1,{}}}).state;
        exploring |= std::any_of(state.fleets.begin(),state.fleets.end(),[&](const auto& f){
            const auto* design=find_ship_design(state,f.design);
            return f.owner==2 && design && design->hull==ShipHullType::StarsScout && !same_position(f.position,homePosition);
        });
        assert(find_planet(state,home)->nativeOrbit.service==HabitatService::Normal);
    }
    assert(exploring); // Before the fix no scout was built even by year161.
}
void stale_remote_queue_does_not_duplicate_a_build()
{
    auto view=make_player_view(campaign(PlayerController::Computer),1);
    view.state.turn=10;
    const auto queueCount=[](const auto& plan){return std::count_if(plan.orders.orders.begin(),plan.orders.orders.end(),
        [](const auto& o){return std::holds_alternative<QueueShipDesignOrder>(o);});};
    assert(queueCount(plan_computer_turn(view))==0);
    view.state.planets.front().nativeOrbit.servicedTurn=view.state.turn;
    assert(queueCount(plan_computer_turn(view))==1);
}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    paid_start_is_identical_for_both_controllers_and_race_import();
    human_replaces_a_miner_and_founds_an_orbit_through_pbem_orders();
    stale_remote_queue_does_not_duplicate_a_build();
    sparse_bootstrap_preserves_paid_exploration();
    for(const auto seed:{17,42,123})actual_computer_campaign_survives_reload_and_supplies_its_second_colony(seed);
}
