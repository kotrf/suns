#pragma once
#include "suns/transport_lines.hpp"
#include "orbital_base_fixture.hpp"
using namespace orbital_test;
inline GameState transport_world(double distance=40) {
    auto s=world(distance);s.fleets.clear();s.nextFleetId=3;
    s.planets[0].minerals={100,100,100};s.planets[1].minerals={1000,0,0};
    for(FleetId i=1;i<=2;++i) {
        Fleet f;f.id=i;f.owner=1;f.name="Freighter";f.design=202;f.ships={{202,1}};
        f.fuel=1200;f.position={0,0};s.fleets.push_back(f);
    }
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);
    set_survey_level(s,1,2,SurveyLevel::GeologicalSurvey,s.turn);acknowledge(s);
    return s;
}
inline TransportLineSpec line_spec() {return {1,1,"Mine shuttle",2,1,5,TransportCargo::Ironium,true};}
inline GameState computer_transport_world(double distance=40) {
    auto s=transport_world(distance);s.players[0].controller=PlayerController::Computer;
    s.planets[0].minerals={};s.planets[0].industry=0;
    s.planets[1].owner=1;s.planets[1].population=100000;s.planets[1].habitability=100;
    s.orbitalStations={{1,1,1,"Home depot",OrbitalStationHullType::OrbitalDock,{OrbitalStationModule::RefuelingDepot}},
        {2,1,2,"Source depot",OrbitalStationHullType::OrbitalDock,{OrbitalStationModule::RefuelingDepot}}};
    s.nextOrbitalStationId=3;
    s.players[0].surveyKnowledge.clear();s.players[0].pendingSurveyReports.clear();
    for(const auto& p:s.planets)queue_survey_report(s,1,p.star,0,{0,0},s.turn,SurveyLevel::GeologicalSurvey);
    (void)deliver_due_survey_reports(s);acknowledge(s);return s;
}
inline GameState stock_target_world() {
    auto s=transport_world();s.fleets.resize(1);s.fleets[0].position={40,0};
    for(auto& p:s.planets){p.observedConcentration=MineralCargo{};p.industry=0;p.mines=0;p.minerals={};}
    s.planets[0].industry=kFactoryCost;s.planets[0].minerals={70,100,100};s.planets[1].minerals.ironium=80;
    s.players[0].surveyKnowledge.clear();s.players[0].pendingSurveyReports.clear();
    for(const auto& p:s.planets)queue_survey_report(s,1,p.star,0,{0,0},s.turn,SurveyLevel::GeologicalSurvey);
    (void)deliver_due_survey_reports(s);acknowledge(s);return s;
}
// A real paid constructor fitted as a deployed depot; its shared hold is finite.
inline GameState installation_transport_world() {
    auto s=transport_world();s.fleets.resize(1);s.fleets[0].position={40,0};
    auto design=s.shipDesigns.back();design.id=203;design.name="Depot constructor";
    design.components.push_back(C::FuelDepotKit);
    s.shipDesigns.push_back(design);s.nextShipDesignId=204;
    Fleet depot;depot.id=3;depot.owner=1;depot.name="Frontier depot";depot.design=203;depot.ships={{203,1}};
    depot.position={20,20};depot.outpost=OutpostKind::FuelDepot;depot.fuel=1000;
    s.fleets.push_back(depot);s.nextFleetId=4;
    for(auto& p:s.planets){p.industry=0;p.mines=0;p.productionQueue.clear();}
    acknowledge(s);return s;
}
inline TransportLineSpec installation_line() {
    auto d=line_spec();d.destination=0;d.destinationFleet=3;d.name="Depot mineral supply";return d;
}
inline GameState multi_transport_world() {
    auto s=computer_transport_world();s.players[0].controller=PlayerController::Human;s.fleets.resize(1);
    auto star=s.stars[1];star.id=3;star.position={80,0};star.name="Third stop";s.stars.push_back(star);
    auto planet=s.planets[0];planet.id=3;planet.star=3;planet.homeworld=false;planet.name="Third recipient";s.planets.push_back(planet);
    for(auto& p:s.planets){p.mines=0;p.industry=0;p.observedConcentration=MineralCargo{};p.productionQueue.clear();p.minerals={0,100,100};}
    s.planets[0].minerals.ironium=180;s.planets[2].minerals.ironium=10;s.fleets[0].position={0,0};
    s.players[0].surveyKnowledge.clear();s.players[0].pendingSurveyReports.clear();
    for(const auto& p:s.planets)queue_survey_report(s,1,p.star,0,{0,0},s.turn,SurveyLevel::GeologicalSurvey);
    (void)deliver_due_survey_reports(s);acknowledge(s);return s;
}
inline TransportLineSpec multi_line() {
    auto d=line_spec();d.source=1;d.destination=2;d.destinationTarget=50;d.deliveries={{3,0,40}};d.name="Three-stop supply";return d;
}
