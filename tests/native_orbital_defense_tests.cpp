#include "native_orbital_defense_fixture.hpp"
#include "suns/native_orbital_support.hpp"
#include "suns/production.hpp"
#include "suns/combat.hpp"
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
void paid_station_weapons_can_engage_the_initial_attack_range()
{
    for(const std::uint8_t weapons:{0,7,9}) {
        auto state=test::native_orbital_defense_fixture();const auto home=test::native_defense_home(state).id;
        state.players.front().technology.levels[std::size_t(ResearchField::Weapons)]=weapons;
        const auto plan=plan_computer_turn(make_player_view(state,1));const auto* order=test::native_station_order(plan,home);assert(order);
        const auto expected=order->construction.loadout;const auto stationId=order->construction.refitStation;
        const auto price=station_mineral_cost(expected);
        auto control=state;auto plain=plan.orders;
        std::erase_if(plain.orders,[](const auto& o){return std::holds_alternative<QueueStationOrder>(o);});
        bool complete=false;
        for(unsigned year=0;year<20 && !complete;++year) {
            state=TurnProcessor{}.process(state,year?std::vector<PlayerOrders>{}:std::vector{plan.orders});
            control=TurnProcessor{}.process(control,year?std::vector<PlayerOrders>{}:std::vector{plain});
            const auto& colony=*find_planet(state,home);const auto* station=native_orbital_station(state,colony);
            assert(station && station->id==stationId && colony.nativeOrbit.population);
            complete=colony.productionQueue.empty() && station_loadout(*station)==expected;
        }
        assert(complete);
        const auto& colony=*find_planet(state,home);const auto& other=*find_planet(control,home);
        assert(std::abs(other.minerals.ironium-colony.minerals.ironium-price.ironium)<1e-6);
        assert(std::abs(other.minerals.boranium-colony.minerals.boranium-price.boranium)<1e-6);
        assert(std::abs(other.minerals.germanium-colony.minerals.germanium-price.germanium)<1e-6);
        // Isolate the paid station: miners and scouts must not draw the enemy
        // into melee or supply damage that hides an unusable station weapon.
        state.fleets.clear();Player attacker{2,"Laser attacker"};attacker.race=race_preset(RacePreset::Terran);
        attacker.technology.levels.fill(7);state.players.push_back(attacker);
        ShipDesign design{state.nextShipDesignId++,2,"Laser destroyer",ShipHullType::Destroyer,
            {ShipComponentType::QuickJump5,ShipComponentType::Laser}};
        normalize_ship_design_placement(design);assert(ship_design_valid(design));state.shipDesigns.push_back(design);
        Fleet enemy;enemy.id=state.nextFleetId++;enemy.owner=2;enemy.design=design.id;enemy.ships={{design.id,1}};
        enemy.name="Laser attacker";enemy.position=find_star(state,colony.star)->position;enemy.fuel=fleet_fuel_capacity(state,enemy);
        state.fleets.push_back(enemy);
        const auto result=TurnProcessor{}.process_with_events(state,{});
        const auto report=std::find_if(result.events.begin(),result.events.end(),[](const auto& e){
            return e.kind==GameEventKind::SpaceBattle && e.recipient==1 && e.battle;
        });
        assert(report!=result.events.end());const auto& battle=*report->battle;
        const auto unit=std::find_if(battle.units.begin(),battle.units.end(),[&](const auto& u){return u.station==stationId;});
        assert(unit!=battle.units.end());const auto index=std::size_t(unit-battle.units.begin());
        assert(std::any_of(battle.shots.begin(),battle.shots.end(),[&](const auto& shot){return shot.attacker==index && shot.armorDamage>0;}));
        assert(battle.rounds<kSpaceBattleRoundLimit && unit->survivingShips==1);
        assert(std::none_of(result.state.fleets.begin(),result.state.fleets.end(),[&](const auto& f){return f.id==enemy.id;}));
        if(weapons==7)assert(std::count(expected.components.begin(),expected.components.end(),ShipComponentType::YakimoraLightPhaser)==2);
    }
}

GameState native_guard_fixture()
{
    auto state = test::native_orbital_defense_fixture(25'000);
    auto& colony = test::native_defense_home(state);
    auto fit = station_loadout(*native_orbital_station(state, colony));
    fit.components.push_back(ShipComponentType::Laser);
    assert(complete_station_construction(state, colony, {fit, colony.nativeOrbit.station}));
    set_survey_level(state, 1, colony.star, SurveyLevel::DeepSurvey, state.turn);
    return state;
}

ReconContact nearby_contact(const GameState& state)
{
    const auto& colony = *std::find_if(state.planets.begin(), state.planets.end(), [](const auto& p) { return p.owner == 1; });
    const auto orbit = find_star(state, colony.star)->position;
    return {900, 0, {orbit.x + 65, orbit.y}, 2, ContactQuality::Probable, ContactSensor::Active,
        state.turn, state.turn, state.turn, 0, colony.id, 0, 100, 160};
}

const QueueShipDesignOrder* ship_order(const ComputerTurnPlan& plan, PlanetId colony)
{
    for (const auto& order : plan.orders.orders)
        if (const auto* ship = std::get_if<QueueShipDesignOrder>(&order); ship && ship->colony == colony) return ship;
    return nullptr;
}

void guard_requires_delivered_threat_and_survival_budget()
{
    auto state = native_guard_fixture();
    auto& player = state.players.front();
    auto& colony = test::native_defense_home(state);
    const auto plan = [&] { return plan_computer_turn(make_player_view(state, 1)); };
    assert(!ship_order(plan(), colony.id));

    auto pending = nearby_contact(state);
    ++pending.deliveryTurn;
    player.pendingEnemyContacts.push_back(pending);
    assert(!ship_order(plan(), colony.id));

    auto contact = nearby_contact(state);
    contact.observedTurn = contact.lastSeenTurn = state.turn - 7;
    player.enemyContacts = {contact};
    assert(!ship_order(plan(), colony.id));

    contact = nearby_contact(state);
    contact.quality = ContactQuality::Identified;
    contact.hulls = {ShipHullType::StarsScout};
    player.enemyContacts = {contact};
    assert(!ship_order(plan(), colony.id));

    player.enemyContacts = {nearby_contact(state)};
    player.technology.levels[std::size_t(ResearchField::Weapons)] = 0;
    const auto funded = plan();
    assert(funded.nextState.researchGoal && funded.nextState.researchGoal->field == ResearchField::Weapons);
    const auto* build = ship_order(funded, colony.id);
    assert(build && !build->pendingDesignName.empty());
    const auto blueprint = std::find_if(funded.orders.orders.begin(), funded.orders.orders.end(), [&](const auto& order) {
        const auto* design = std::get_if<CreateShipDesignOrder>(&order);
        return design && design->name == build->pendingDesignName;
    });
    assert(blueprint != funded.orders.orders.end());
    const auto& order = std::get<CreateShipDesignOrder>(*blueprint);
    ShipDesign design{1, 1, order.name, order.hull, order.components};
    design.placements = order.placements;
    const auto cost = ship_design_mineral_cost(design);
    assert(ship_design_valid(design) && shipyard_can_build(state, colony, design));

    const auto upkeep = native_orbital_support_quote(state, colony).mineralUnitsPaid;
    colony.minerals = {cost.ironium + upkeep, cost.boranium + upkeep, cost.germanium + upkeep};
    set_survey_level(state, 1, colony.star, SurveyLevel::DeepSurvey, state.turn);
    assert(!ship_order(plan(), colony.id));

    colony.minerals = {10000, 10000, 10000};
    colony.nativeOrbit.servicedTurn = state.turn - 1;
    set_survey_level(state, 1, colony.star, SurveyLevel::DeepSurvey, state.turn);
    assert(!ship_order(plan(), colony.id));
}

void paid_guard_build_dispatch_and_battle(std::uint8_t weapons)
{
    auto state = native_guard_fixture();
    state.players.front().technology.levels[std::size_t(ResearchField::Weapons)] = weapons;
    const auto& colony = test::native_defense_home(state);
    const auto home = colony.id;
    const auto star = colony.star;
    const auto position = find_star(state, star)->position;
    state.players.front().enemyContacts = {nearby_contact(state)};
    auto control = state;
    FleetId guardId = 0;
    for (unsigned year = 0; year < 8 && !guardId; ++year) {
        const auto plan = plan_computer_turn(make_player_view(state, 1));
        if (!year) assert(ship_order(plan, home));
        auto unarmedBudget = plan.orders;
        std::erase_if(unarmedBudget.orders, [](const auto& order) {
            return std::holds_alternative<QueueShipDesignOrder>(order);
        });
        state = resolve_controlled_campaign_turn(state, {}).state;
        control = TurnProcessor{}.process(control, {unarmedBudget});
        const auto& habitat = *find_planet(state, home);
        assert(habitat.nativeOrbit.service == HabitatService::Normal && !habitat.nativeOrbit.populationLost);
        for (const auto& fleet : state.fleets) {
            if (!fleet_has_space_weapons(state, fleet)) continue;
            assert(!guardId && same_position(fleet.position, position));
            guardId = fleet.id;
            const auto cost = ship_design_mineral_cost(*find_ship_design(state, fleet.design));
            const auto& other = *find_planet(control, home);
            assert(std::abs(other.minerals.ironium - habitat.minerals.ironium - cost.ironium) < 1e-6);
            assert(std::abs(other.minerals.boranium - habitat.minerals.boranium - cost.boranium) < 1e-6);
            assert(std::abs(other.minerals.germanium - habitat.minerals.germanium - cost.germanium) < 1e-6);
        }
    }
    assert(guardId);
    state.players.front().enemyContacts = {nearby_contact(state)};
    assert(!ship_order(plan_computer_turn(make_player_view(state, 1)), home));

    // A previously built guard is available nearby while the station is under threat.
    auto& guard = *std::find_if(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == guardId; });
    guard.position = {position.x + 8, position.y};
    guard.telemetry = {};
    guard.telemetry = confirmed_fleet_telemetry(state, guard);
    const auto dispatch = plan_computer_turn(make_player_view(state, 1));
    assert(!ship_order(dispatch, home));
    const auto move = std::find_if(dispatch.orders.orders.begin(), dispatch.orders.orders.end(), [&](const auto& order) {
        const auto* route = std::get_if<MoveFleetOrder>(&order);
        return route && route->fleet == guardId;
    });
    assert(move != dispatch.orders.orders.end());
    const auto& route = std::get<MoveFleetOrder>(*move);
    assert(same_position(route.destination, position) && route.arrivalAction.kind == FleetArrivalActionKind::None);
    assert(std::any_of(dispatch.nextState.fleetGoals.begin(), dispatch.nextState.fleetGoals.end(), [&](const auto& goal) {
        return goal.fleet == guardId && goal.kind == ComputerFleetGoalKind::Defend && goal.target == star;
    }));
    state = resolve_controlled_campaign_turn(state, {}).state;
    const auto afterDispatch = plan_computer_turn(make_player_view(state, 1));
    assert(!ship_order(afterDispatch, home));
    const auto arrived = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const auto& f) { return f.id == guardId; });
    assert(arrived != state.fleets.end() && same_position(arrived->position, position) && !arrived->destination);
    assert(arrived->battlePlan.preferredTarget == BattleTarget::ArmedShips);

    // Test the paid fit against a raider beyond the station's weapons.
    const Position battlePosition{position.x + 8, position.y};
    arrived->position = battlePosition;
    arrived->telemetry = {};
    arrived->telemetry = confirmed_fleet_telemetry(state, *arrived);
    Player attacker{2, "Raider"};
    attacker.race = race_preset(RacePreset::Terran);
    state.players.push_back(attacker);
    ShipDesign design{state.nextShipDesignId++, 2, "Raider", ShipHullType::Destroyer,
        {ShipComponentType::QuickJump5, ShipComponentType::Laser}};
    normalize_ship_design_placement(design);
    state.shipDesigns.push_back(design);
    Fleet raider;
    raider.id = state.nextFleetId++;
    raider.owner = 2;
    raider.design = design.id;
    raider.ships = {{design.id, 1}};
    raider.name = "Raider";
    raider.position = battlePosition;
    raider.fuel = fleet_fuel_capacity(state, raider);
    state.fleets.push_back(raider);
    const auto result = resolve_controlled_campaign_turn(state, {{2, {}}});
    assert(find_planet(result.state, home)->nativeOrbit.population > 0);
    const auto report = std::find_if(result.events.begin(), result.events.end(), [&](const auto& event) {
        return event.kind == GameEventKind::SpaceBattle && event.recipient == 1 && event.battle;
    });
    assert(report != result.events.end());
    const auto& battle = *report->battle;
    assert(same_position(battle.position, battlePosition));
    assert(std::any_of(battle.shots.begin(), battle.shots.end(), [&](const auto& shot) {
        return battle.units[shot.attacker].fleet == guardId && shot.armorDamage > 0;
    }));
    assert(std::none_of(result.state.fleets.begin(), result.state.fleets.end(), [&](const auto& f) { return f.id == raider.id; }));
}

void reserve_guards_cover_neighboring_orbits()
{
    auto state = native_guard_fixture();
    state.players.front().enemyContacts = {nearby_contact(state)};
    const auto& home = test::native_defense_home(state);
    const auto homePosition = find_star(state, home.star)->position;
    auto& neighbor = *std::find_if(state.planets.begin(), state.planets.end(), [](const auto& colony) { return !colony.owner; });
    auto& star = *std::find_if(state.stars.begin(), state.stars.end(), [&](const auto& candidate) { return candidate.id == neighbor.star; });
    const Position neighborPosition{homePosition.x + 20, homePosition.y};
    star.position = neighborPosition;
    star.stellarClass = StarClass::Orange;
    star.variability = {};
    neighbor.owner = 1;
    const auto stationId = state.nextOrbitalStationId;
    assert(complete_station_construction(state, neighbor, {station_loadout(*native_orbital_station(state, home)), 0}));
    neighbor.nativeOrbit = home.nativeOrbit;
    neighbor.nativeOrbit.station = stationId;
    neighbor.minerals = home.minerals;
    neighbor.observedConcentration = home.observedConcentration;
    set_survey_level(state, 1, neighbor.star, SurveyLevel::DeepSurvey, state.turn);
    auto miner = state.fleets.front();
    miner.id = state.nextFleetId++;
    miner.position = neighborPosition;
    miner.telemetry = {};
    miner.telemetry = confirmed_fleet_telemetry(state, miner);
    state.fleets.push_back(miner);

    ShipDesign design{state.nextShipDesignId++, 1, "Orbit guard", ShipHullType::Frigate,
        {ShipComponentType::QuickJump5, ShipComponentType::Laser}};
    normalize_ship_design_placement(design);
    assert(ship_design_valid(design));
    state.shipDesigns.push_back(design);
    Fleet guard;
    guard.owner = 1;
    guard.design = design.id;
    guard.ships = {{design.id, 1}};
    guard.name = design.name;
    guard.position = homePosition;
    guard.fuel = fleet_fuel_capacity(state, guard);
    const auto firstGuard = state.nextFleetId;
    for (unsigned i = 0; i < 2; ++i) {
        guard.id = state.nextFleetId++;
        guard.telemetry = {};
        guard.telemetry = confirmed_fleet_telemetry(state, guard);
        state.fleets.push_back(guard);
    }

    // Give the later FleetId the acknowledged defense goal in the second case.
    // The earlier reserve should be available even when sorted before that defender.
    auto& computer = state.players.front().computer;
    for (const bool designated : {false, true}) {
        if (designated) {
            computer.initialized = true;
            computer.decisions = {{state.turn - 1, 1, "Assign defense"}};
            computer.fleetGoals = {{firstGuard + 1, ComputerFleetGoalKind::Defend, home.star, state.turn - 1, state.turn + 2}};
        }
        const auto expectedReserve = designated ? firstGuard : firstGuard + 1;
        const auto plan = plan_computer_turn(make_player_view(state, 1));
        assert(!ship_order(plan, neighbor.id));
        const auto result = resolve_controlled_campaign_turn(state, {});
        unsigned onOrbits = 0;
        for (const auto& fleet : result.state.fleets) {
            if (fleet.id != firstGuard && fleet.id != firstGuard + 1) continue;
            assert(same_position(fleet.position, fleet.id == expectedReserve ? neighborPosition : homePosition));
            assert(!fleet.destination);
            ++onOrbits;
        }
        assert(onOrbits == 2);
    }

    // A guard still flying to the first orbit cannot relieve the fleet already there.
    auto& incoming = state.fleets.back();
    incoming.position.y += 50;
    incoming.destination = homePosition;
    incoming.telemetry = {};
    incoming.telemetry = confirmed_fleet_telemetry(state, incoming);
    const auto beforeArrival = plan_computer_turn(make_player_view(state, 1));
    assert(ship_order(beforeArrival, neighbor.id));
    assert(std::none_of(beforeArrival.orders.orders.begin(), beforeArrival.orders.orders.end(), [&](const auto& order) {
        const auto* route = std::get_if<MoveFleetOrder>(&order);
        return route && route->fleet == firstGuard;
    }));
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
    paid_station_weapons_can_engage_the_initial_attack_range();
    guard_requires_delivered_threat_and_survival_budget();
    reserve_guards_cover_neighboring_orbits();
    paid_guard_build_dispatch_and_battle(6);
    paid_guard_build_dispatch_and_battle(7);
    std::cout<<"Native station and guard policies passed\n";
}
