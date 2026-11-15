#pragma once
#include "suns/computer_player.hpp"
#include "suns/communications.hpp"
#include "suns/mining.hpp"
#include "suns/native_orbital.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/stations.hpp"
#include <algorithm>
#include <cassert>

namespace suns::test {
inline Planet& native_defense_home(GameState& state)
{
    return *std::find_if(state.planets.begin(),state.planets.end(),[](const auto& p){return p.owner==1;});
}
// A funded mature test input. Normal campaigns still buy every physical asset.
inline GameState native_orbital_defense_fixture(std::uint64_t population=90'000)
{
    EmpireSetup setup{"Orbital defense"};setup.primaryTrait=PrimaryRaceTrait::HabitatCivilization;
    setup.controller=PlayerController::Computer;GalaxyConfig config;config.seed=17;config.starCount=8;
    auto state=generate_campaign(config,{setup});state.turn=10;state.wormholeRules.spawnChancePerTurn=0;
    state.players.front().technology.levels.fill(6);state.players.front().computer={};
    auto& p=native_defense_home(state);p.nativeOrbit.population=population;p.minerals={10000,10000,10000};
    p.nativeOrbit.service=HabitatService::Normal;p.nativeOrbit.servicedTurn=state.turn;p.nativeOrbit.reserveUnits=12;
    p.observedConcentration=MineralCargo{100,100,100};p.productionQueue.clear();
    auto* star=const_cast<StarSystem*>(find_star(state,p.star));star->stellarClass=StarClass::Orange;star->variability={};
    auto base=state.fleets.front();state.fleets.clear();state.nextFleetId=1;
    const auto add=[&](ShipDesignId design,std::uint32_t count,FleetTask task) {
        auto f=base;f.id=state.nextFleetId++;f.design=design;f.ships={{design,count}};f.position=star->position;
        f.destination.reset();f.arrivalAction.reset();f.waypointQueue.clear();f.pendingCommands.clear();
        f.repeatOrders=false;f.targetFleet=0;f.task=task;f.colonists=0;f.minerals={};
        f.fuel=fleet_fuel_capacity(state,f);f.telemetry={};f.telemetry=confirmed_fleet_telemetry(state,f);
        state.fleets.push_back(f);
    };
    add(base.design,40,FleetTask::RemoteMining);
    for(const auto& d:state.shipDesigns)if(d.owner==1 && (d.hull==ShipHullType::StarsScout || ship_design_can_found_native_orbit(d)))
        add(d.id,1,FleetTask::None);
    assert(state.fleets.size()==3);
    set_survey_level(state,1,p.star,SurveyLevel::DeepSurvey,state.turn);return state;
}
inline const QueueStationOrder* native_station_order(const ComputerTurnPlan& plan,PlanetId home)
{
    const QueueStationOrder* result=nullptr;
    for(const auto& o:plan.orders.orders)if(const auto* q=std::get_if<QueueStationOrder>(&o);q && q->colony==home) {
        assert(!result);result=q;
    }
    return result;
}
}
