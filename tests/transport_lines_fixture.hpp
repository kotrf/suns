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
