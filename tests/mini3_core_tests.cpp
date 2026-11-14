#include "suns/campaign.hpp"
#include "suns/ship_refit.hpp"
#include "suns/production.hpp"
#include "suns/exotic_materials.hpp"
#include "suns/stations.hpp"
#include "suns/mobile_base.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <string>
using namespace suns;
namespace {
GameState fixture() {
    auto s=make_demo_game();s.wormholeRules.spawnChancePerTurn=0;
    s.players.front().technology.levels.fill(26);s.players.front().technology.researchAllocationPercent=0;
    auto& p=s.planets.front();p.population=1000000;p.industry=1000;p.minerals={10000,10000,10000};p.productionQueue.clear();
    auto from=s.shipDesigns.front();from.id=s.nextShipDesignId++;from.name="Refit source";
    from.components={ShipComponentType::FusionDrive,ShipComponentType::LongRangeScanner};
    from.placements=autoplace_ship_components(from.hull,from.components);assert(ship_design_valid(from));
    auto to=from;to.id=s.nextShipDesignId++;to.name="Refit target";to.components.back()=ShipComponentType::PenetratingScanner;
    to.placements=autoplace_ship_components(to.hull,to.components);assert(ship_design_valid(to));s.shipDesigns={from,to};
    auto f=s.fleets.front();f.design=from.id;f.ships={{from.id,2}};f.position=s.stars.front().position;
    f.destination.reset();f.pendingCommands.clear();f.fuel=10;f.damagePercent=5;
    f.telemetry.ships=f.ships;f.telemetry.position=f.position;f.telemetry.fuel=f.fuel;f.telemetry.observedTurn=s.turn;
    s.fleets={f};set_survey_level(s,1,p.star,SurveyLevel::DeepSurvey,s.turn);return s;
}
void quote_and_host() {
    auto s=fixture();const auto source=s.shipDesigns[0].id,target=s.shipDesigns[1].id,fleet=s.fleets[0].id;
    const auto q=ship_refit_quote(s.shipDesigns[0],s.shipDesigns[1]);assert(q);
    assert(q->removed==std::vector{ShipComponentType::LongRangeScanner});
    assert(q->installed==std::vector{ShipComponentType::PenetratingScanner});
    assert(q->componentWork==component_spec(ShipComponentType::PenetratingScanner).buildCost);
    assert(std::abs(q->recovered.ironium-component_mineral_cost(ShipComponentType::LongRangeScanner).ironium*.1)<1e-8);
    assert(q->work==q->componentWork+(q->componentWork+1)/2);
    const auto control=TurnProcessor{}.process(s,{});
    const auto fuel=control.fleets[0].fuel;
    const PlayerOrders o{1,{RefitShipOrder{s.planets[0].id,fleet,source,target}}};
    const auto preview=planned_production_state(s,o);assert(preview.planets[0].productionQueue.size()==1);
    assert(fleet_ship_count(s.fleets[0],target)==0);
    s=TurnProcessor{}.process(s,{o});assert(fleet_ship_count(s.fleets[0],source)==1 && fleet_ship_count(s.fleets[0],target)==1);
    assert(s.planets[0].productionQueue.empty() && s.fleets[0].fuel==fuel);
    assert(std::abs(control.planets[0].minerals.ironium-s.planets[0].minerals.ironium-q->netMinerals.ironium)<1e-6);
    assert(std::abs(control.planets[0].minerals.boranium-s.planets[0].minerals.boranium-q->netMinerals.boranium)<1e-6);
    assert(std::abs(control.planets[0].minerals.germanium-s.planets[0].minerals.germanium-q->netMinerals.germanium)<1e-6);
    assert(!delete_ship_design(s,1,source) && !delete_ship_design(s,1,target));
}
void pending_target_is_resolved_by_name() {
    auto s=fixture();auto other=s.players.front();other.id=2;other.name="Other";s.players.push_back(other);
    for(int n=0;n<3;++n){auto foreign=s.shipDesigns.front();foreign.id=s.nextShipDesignId++;foreign.owner=2;foreign.name="Foreign "+std::to_string(n);s.shipDesigns.push_back(foreign);}
    const auto project=s.shipDesigns[1];
    const PlayerOrders orders{1,{CreateShipDesignOrder{"Deferred target",project.hull,project.components,project.placements},RefitShipOrder{s.planets[0].id,s.fleets[0].id,s.shipDesigns[0].id,0,"Deferred target"}}};
    const auto view=make_player_view(s,1).state;
    const auto preview=planned_production_state(view,orders);assert(preview.planets[0].productionQueue.size()==1);
    const auto next=TurnProcessor{}.process(s,{orders});
    const auto target=std::find_if(next.shipDesigns.begin(),next.shipDesigns.end(),[](const auto& d){return d.owner==1 && d.name=="Deferred target";});assert(target!=next.shipDesigns.end());
    assert(target->id!=preview.planets[0].productionQueue.front().shipDesign);
    assert(fleet_ship_count(next.fleets[0],target->id)==1);
}
void blocked_and_delete() {
    auto s=fixture();const auto a=s.shipDesigns[0].id,b=s.shipDesigns[1].id;
    assert(!delete_ship_design(s,1,a));s.fleets[0].destination=Position{9,9};assert(!delete_ship_design(s,1,a));
    assert(!queue_ship_refit(s,1,{s.planets[0].id,s.fleets[0].id,a,b}));s.fleets[0].destination.reset();
    assert(queue_ship_refit(s,1,{s.planets[0].id,s.fleets[0].id,a,b}));
    assert(!queue_ship_refit(s,1,{s.planets[0].id,s.fleets[0].id,a,b}));
    s.players[0].productionTemplates={{"Target",{{ProductionKind::ColonyShip,1,b}}}};
    assert(delete_ship_design(s,1,b));assert(s.shipDesigns[1].retired && s.planets[0].productionQueue.empty());
    assert(s.players[0].productionTemplates.empty());assert(!ship_design_available_to_player(s,1,s.shipDesigns[1]));
    const auto late=TurnProcessor{}.process(s,{{1,{QueueShipDesignOrder{s.planets[0].id,b}}}});
    assert(late.planets[0].productionQueue.empty() && fleet_ship_count(late.fleets[0],b)==0);
    auto empty=make_demo_game();auto before=empty;before.planets[0].owner=0;
    publish_exotic_material_reports(empty,before);
    for(const auto& p:empty.players)for(const auto& report:p.pendingPlayerReports)assert(report.kind!=PlayerReportKind::ResonanceProjectStatus);
}
void actual_unload() {
    for(auto cargo:{FleetCargoKind::Colonists,FleetCargoKind::Boranium,FleetCargoKind::All}) {
        auto s=make_demo_game();s.wormholeRules.spawnChancePerTurn=0;
        auto& f=s.fleets.back();f.position=s.stars.front().position;f.destination=f.position;
        f.colonists=1000;f.minerals={0,3,0};f.arrivalAction={FleetArrivalActionKind::UnloadAll,1,cargo};
        const auto id=f.id;const auto result=TurnProcessor{}.process(s,{});
        const auto after=std::find_if(result.fleets.begin(),result.fleets.end(),[id](const auto& f){return f.id==id;});assert(after!=result.fleets.end());
        assert(after->colonists==(cargo==FleetCargoKind::Boranium?1000:0));
        assert(after->minerals.boranium==(cargo==FleetCargoKind::Colonists?3:0));
    }
}

void queue_edits_keep_submission_order() {
    using K = ProductionKind;
    const auto s = fixture();
    const auto colony = s.planets.front().id;
    const auto unused = s.shipDesigns.back().id;
    const auto check = [&](std::vector<Order> orders, std::vector<K> expected) {
        auto current = s;
        current.planets.front().productionQueue = {
            {K::ColonyShip, 1000000, unused}, {K::Factory, 1000000}, {K::Mine, 1000000}};
        const PlayerOrders submission{1, std::move(orders)};
        const auto preview = planned_production_state(current, submission);
        const auto host = TurnProcessor{}.process(current, {submission});
        const auto kinds = [](const auto& state) {
            std::vector<K> result;
            for (const auto& item : state.planets.front().productionQueue) result.push_back(item.kind);
            return result;
        };
        assert(kinds(preview) == expected);
        assert(kinds(host) == expected);
    };
    check({CancelProductionOrder{colony, 1}, DeleteShipDesignOrder{unused}}, {K::Mine});
    check({DeleteShipDesignOrder{unused}, CancelProductionOrder{colony, 1}}, {K::Factory});
    check({ReorderProductionQueueOrder{colony, 2, 1}, DeleteShipDesignOrder{unused}}, {K::Mine, K::Factory});
    check({DeleteShipDesignOrder{unused}, ReorderProductionQueueOrder{colony, 1, 0}}, {K::Mine, K::Factory});
    check({CancelProductionOrder{colony, 1}, DeleteShipDesignOrder{unused},
        CancelProductionOrder{colony, 0}}, {});
}

void refit_preserves_fuel_while_waiting_for_minerals() {
    for (const double initialFuel : {0.0, 10.0}) {
        auto s = fixture();
        auto& from = s.shipDesigns[0];
        auto& to = s.shipDesigns[1];
        from.components = {ShipComponentType::FusionDrive, ShipComponentType::FuelTank};
        from.placements = autoplace_ship_components(from.hull, from.components);
        to.components = {ShipComponentType::FusionDrive, ShipComponentType::PenetratingScanner};
        to.placements = autoplace_ship_components(to.hull, to.components);
        assert(ship_design_valid(from) && ship_design_valid(to));
        auto& fleet = s.fleets.front();
        fleet.ships = {{from.id, 1}};
        fleet.fuel = initialFuel;
        fleet.telemetry.ships = fleet.ships;
        fleet.telemetry.fuel = initialFuel;
        auto& colony = s.planets.front();
        colony.minerals = {};
        colony.mines = colony.advancedMines = 0;
        const auto job = RefitShipOrder{colony.id, fleet.id, from.id, to.id};
        const auto source = from.id, target = to.id;
        auto future = fleet;
        future.ships = {{target, 1}};
        const auto capacity = fleet_fuel_capacity(s, future);
        assert(capacity < fleet_fuel_capacity(s, fleet));
        const auto preview = planned_production_state(s, {1, {job}});
        assert(preview.planets.front().productionQueue.size() == 1);
        auto next = TurnProcessor{}.process(s, {{1, {job}}});
        assert(next.planets.front().productionQueue.size() == 1);
        assert(fleet_ship_count(next.fleets.front(), source) == 1);
        assert(next.fleets.front().fuel == initialFuel);
        // Departing pauses the job, but must still receive ordinary dock fuel.
        const auto origin = next.fleets.front().position;
        const auto departing = TurnProcessor{}.process(next, {{1, {MoveFleetOrder{
            next.fleets.front().id, {origin.x + 3, origin.y}, 1}}}});
        assert(!same_position(departing.fleets.front().position, origin));
        assert(departing.fleets.front().fuel > initialFuel);
        assert(departing.planets.front().productionQueue.size() == 1);
        assert(fleet_ship_count(departing.fleets.front(), target) == 0);
        next = TurnProcessor{}.process(next, {});
        assert(next.planets.front().productionQueue.size() == 1);
        assert(next.fleets.front().fuel == initialFuel);
        next.planets.front().minerals = {10000, 10000, 10000};
        next = TurnProcessor{}.process(next, {});
        assert(next.planets.front().productionQueue.empty());
        assert(fleet_ship_count(next.fleets.front(), target) == 1);
        assert(std::abs(next.fleets.front().fuel - capacity) < 1e-6);

        // Actual excess fuel must still block a smaller tank; never discard it.
        s.fleets.front().fuel = capacity + 1;
        s.fleets.front().telemetry.fuel = capacity + 1;
        assert(planned_production_state(s, {1, {job}}).planets.front().productionQueue.empty());
        const auto blocked = TurnProcessor{}.process(s, {{1, {job}}});
        assert(blocked.planets.front().productionQueue.empty());
        assert(fleet_ship_count(blocked.fleets.front(), target) == 0);
        assert(blocked.fleets.front().fuel >= capacity + 1);
    }
}
void generator_does_not_block_refit() {
    auto s=fixture();
    auto& from=s.shipDesigns[0]; auto& to=s.shipDesigns[1];
    from.hull=to.hull=ShipHullType::Destroyer;
    to.components.assign(hull_spec(to.hull).requiredEngines,ShipComponentType::FusionDrive);
    from.components=to.components;from.components.push_back(ShipComponentType::AntimatterGenerator);
    from.placements=autoplace_ship_components(from.hull,from.components);
    to.placements=autoplace_ship_components(to.hull,to.components);
    assert(ship_design_valid(from) && ship_design_valid(to));
    auto& f=s.fleets[0];f.ships={{from.id,1}};f.design=from.id;
    auto future=f;future.ships={{to.id,1}};
    const auto capacity=fleet_fuel_capacity(s,future);
    f.fuel=capacity-10;f.telemetry.ships=f.ships;f.telemetry.fuel=f.fuel;
    s.orbitalStations[0].modules={OrbitalStationModule::Shipyard};
    const RefitShipOrder job{s.planets[0].id,f.id,from.id,to.id};
    assert(planned_production_state(s,{1,{job}}).planets[0].productionQueue.size()==1);
    const auto next=TurnProcessor{}.process(s,{{1,{job}}});
    assert(fleet_ship_count(next.fleets[0],to.id)==1);
    assert(next.fleets[0].fuel==f.fuel);
    // Waiting for production must also preserve the original fuel for years.
    assert(queue_ship_refit(s,1,job));
    s.planets[0].productionQueue[0].remainingCost=1000000;
    auto waiting=TurnProcessor{}.process(s,{});
    waiting=TurnProcessor{}.process(waiting,{});
    assert(waiting.planets[0].productionQueue.size()==1);
    assert(waiting.fleets[0].fuel==f.fuel);
}
void gate_uses_departure_depot() {
    auto s=make_demo_game();s.wormholeRules.spawnChancePerTurn=0;
    s.players[0].technology.levels.fill(26);
    s.planets[1].owner=1;s.planets[1].population=100000;
    auto& source=s.orbitalStations[0];source.hull=OrbitalStationHullType::Starbase;
    source.components={ShipComponentType::JumpGate};
    s.orbitalStations.push_back({s.nextOrbitalStationId++,1,s.planets[1].id,"Target gate",
        OrbitalStationHullType::Starbase,{OrbitalStationModule::Shipyard},{ShipComponentType::JumpGate}});
    auto& f=s.fleets[0];f.destination.reset();f.fuel=10;
    const auto capacity=fleet_fuel_capacity(s,f);
    assert(stargate_transfer_valid(s,1,f.id,1,2));
    const auto next=TurnProcessor{}.process(s,{{1,{GateFleetOrder{f.id,1,2}}}});
    assert(same_position(next.fleets[0].position,s.stars[1].position));
    assert(next.fleets[0].fuel==capacity);
}
void station_loss_preserves_refit() {
    auto s=fixture();
    const QueueStationOrder station{s.planets[0].id,{{OrbitalStationHullType::OrbitalDock,{},false,true},s.orbitalStations[0].id}};
    const auto control=TurnProcessor{}.process(s,{{1,{station}}});
    const RefitShipOrder job{s.planets[0].id,s.fleets[0].id,s.shipDesigns[0].id,s.shipDesigns[1].id};
    const auto next=TurnProcessor{}.process(s,{{1,{station,job}}});
    assert(!orbital_station_has_module(next.orbitalStations[0],OrbitalStationModule::Shipyard));
    assert(next.planets[0].productionQueue.size()==1 && next.planets[0].productionQueue[0].refit);
    assert(next.planets[0].minerals.germanium==control.planets[0].minerals.germanium);
    assert(fleet_ship_count(next.fleets[0],job.source)==2);
}
void mobile_base_preserves_refit_fuel_and_repairs() {
    auto s=fixture();auto& from=s.shipDesigns[0];
    from.components.push_back(ShipComponentType::FuelTank);
    from.placements=autoplace_ship_components(from.hull,from.components);
    assert(ship_design_valid(from));
    s.orbitalStations[0].modules={OrbitalStationModule::Shipyard};
    assert(queue_ship_refit(s,1,{s.planets[0].id,s.fleets[0].id,from.id,s.shipDesigns[1].id}));
    s.planets[0].productionQueue[0].remainingCost=1000000;
    ShipDesign d{s.nextShipDesignId++,1,"Service base",ShipHullType::ExpeditionBase,
        {ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,ShipComponentType::QuickJump5,
         ShipComponentType::ExpeditionDock,ShipComponentType::ExpeditionWorkshop,ShipComponentType::RelayArray}};
    s.shipDesigns.push_back(d);
    Fleet base;base.id=s.nextFleetId++;base.owner=1;base.design=d.id;base.ships={{d.id,1}};
    base.position=s.fleets[0].position;base.fuel=5000;base.minerals={100,100,100};base.mobileBase.clients={s.fleets[0].id};
    s.fleets.push_back(base);
    service_mobile_bases(s,s.turn+1);
    assert(s.fleets[0].fuel==10);
    assert(s.fleets[0].damagePercent<5);
}
void deployed_generator_precedes_upkeep() {
    auto s=fixture();auto& d=s.shipDesigns[0];
    d.components={ShipComponentType::FusionDrive,ShipComponentType::DeepSpaceRelayKit,ShipComponentType::AntimatterGenerator};
    d.placements=autoplace_ship_components(d.hull,d.components);assert(ship_design_valid(d));
    auto& f=s.fleets[0];f.ships={{d.id,1}};f.outpost=OutpostKind::CommunicationsRelay;f.fuel=.5;
    s.orbitalStations[0].modules={OrbitalStationModule::Shipyard};
    const auto expected=f.fuel+fleet_fuel_generation(s,f)-1;
    const auto next=TurnProcessor{}.process(s,{});
    assert(std::abs(next.fleets[0].fuel-expected)<1e-6);
}
void rejected_creation_has_no_named_dependents() {
    auto s=fixture();const auto stale=s;
    auto hidden=s.fleets[0];hidden.id=s.nextFleetId++;hidden.design=s.shipDesigns[1].id;hidden.ships={{hidden.design,1}};hidden.position={2000,2000};
    s.fleets.push_back(hidden);
    const auto old=s.shipDesigns[1];const auto replacement=s.shipDesigns[0];
    const PlayerOrders orders{1,{DeleteShipDesignOrder{old.id},CreateShipDesignOrder{old.name,replacement.hull,replacement.components,replacement.placements},
        QueueShipDesignOrder{s.planets[0].id,0,old.name},RefitShipOrder{s.planets[0].id,s.fleets[0].id,replacement.id,0,old.name}}};
    assert(!planned_production_state(stale,orders).planets[0].productionQueue.empty());
    const auto next=TurnProcessor{}.process(s,{orders});
    std::uint32_t count=0;for(const auto& f:next.fleets)count+=fleet_ship_count(f,old.id);
    assert(count==1);
    assert(fleet_ship_count(next.fleets[0],replacement.id)==2);
    const PlayerOrders bare{1,{QueueShipDesignOrder{s.planets[0].id,0,old.name},RefitShipOrder{s.planets[0].id,s.fleets[0].id,replacement.id,0,old.name}}};
    assert(planned_production_state(stale,bare).planets[0].productionQueue.empty());
}
void restored_shipyard_resumes_same_turn(bool depot) {
    auto s=fixture();auto& from=s.shipDesigns[0];from.components.push_back(ShipComponentType::FuelTank);
    from.placements=autoplace_ship_components(from.hull,from.components);assert(ship_design_valid(from));
    const RefitShipOrder job{s.planets[0].id,s.fleets[0].id,from.id,s.shipDesigns[1].id};
    const QueueStationOrder remove{s.planets[0].id,{{OrbitalStationHullType::OrbitalDock,{},false,depot},s.orbitalStations[0].id}};
    auto next=TurnProcessor{}.process(s,{{1,{remove,job}}});
    assert(next.planets[0].productionQueue.size()==1);
    if(depot)assert(next.fleets[0].fuel==10);
    const QueueStationOrder restore{s.planets[0].id,{{OrbitalStationHullType::OrbitalDock,{},true,depot},s.orbitalStations[0].id}};
    next=TurnProcessor{}.process(next,{{1,{restore,ReorderProductionQueueOrder{s.planets[0].id,1,0}}}});
    assert(next.planets[0].productionQueue.empty());
    assert(fleet_ship_count(next.fleets[0],job.target)==1);
}
}
int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string test = argv[1];
        if (test == "queue-order") queue_edits_keep_submission_order();
        else if (test == "refit-fuel") refit_preserves_fuel_while_waiting_for_minerals();
        else if (test == "generator") generator_does_not_block_refit();
        else if (test == "gate-fuel") gate_uses_departure_depot();
        else if (test == "station-loss") station_loss_preserves_refit();
        else if (test == "mobile-fuel") mobile_base_preserves_refit_fuel_and_repairs();
        else if (test == "relay-fuel") deployed_generator_precedes_upkeep();
        else if (test == "creation-binding") rejected_creation_has_no_named_dependents();
        else if (test == "shipyard-fuel") restored_shipyard_resumes_same_turn(true);
        else if (test == "shipyard-resume") restored_shipyard_resumes_same_turn(false);
        else assert(false);
        return 0;
    }
    quote_and_host();pending_target_is_resolved_by_name();blocked_and_delete();actual_unload();
    queue_edits_keep_submission_order();
    refit_preserves_fuel_while_waiting_for_minerals();
    generator_does_not_block_refit();gate_uses_departure_depot();
    station_loss_preserves_refit();mobile_base_preserves_refit_fuel_and_repairs();deployed_generator_precedes_upkeep();
    rejected_creation_has_no_named_dependents();restored_shipyard_resumes_same_turn(true);restored_shipyard_resumes_same_turn(false);
}
