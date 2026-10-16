#include "suns/planetary_recovery.hpp"
#include "suns/communications.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/campaign.hpp"
#include "suns/strategic_operations.hpp"
#include <algorithm>
#include <cassert>
using namespace suns;
GameState fixture()
{
    GameState s; s.turn=10; s.players={{1,"Observer"},{2,"Defender"}}; s.wormholeRules.spawnChancePerTurn=0;
    s.stars={{1,"HQ",{0,0}},{2,"World",{600,0}}};
    for(auto id:{1U,2U}) {
        Planet p; p.id=id; p.star=id; p.owner=id; p.population=10000; p.homeworld=true;
        p.habitability=100; p.radar=PlanetaryRadarLevel::Full; s.planets.push_back(p);
    }
    s.planets[1].warDamage={13,7,2,1,9,9,37,9};
    s.shipDesigns={{1,1,"Observer",ShipHullType::Destroyer,{ShipComponentType::FuelMizer,ShipComponentType::MoleScanner}}};
    Fleet f; f.id=1; f.owner=1; f.design=1; f.ships={{1,1}}; f.position={600,0}; f.fuel=1000; s.fleets={f};
    set_survey_level(s,1,2,SurveyLevel::GeologicalSurvey,9);
    return s;
}
void immutable_estimates_arrive_after_their_own_communications_delay()
{
    auto s=fixture(); const auto before=known_planet_habitability(s,1,2);
    observe_planetary_aftermath(s,10);
    const auto packet=s.players[0].pendingPlayerReports.front();
    assert(packet.kind==PlayerReportKind::PlanetaryAftermath && packet.aftermath);
    const auto a=*packet.aftermath; assert(!a.exact && a.factoriesMinimum==10 && a.factoriesMaximum==14);
    assert(a.minesMinimum==5 && a.minesMaximum==9 && a.contaminationMinimum==30 && a.contaminationMaximum==39);
    assert(packet.deliveryTurn>10 && !known_planet_aftermath(s,1,2));
    const auto view=make_player_view(s,1).state;
    assert(view.planets[1].warDamage.contamination==0 && view.players[0].pendingPlayerReports.empty());
    assert(!known_planet_aftermath(view,1,2));
    s.planets[1].warDamage.contamination=99;
    assert(known_planet_habitability(s,1,2)==before); // A stale survey cannot leak the new hazard.
    s.planets[1].warDamage.factoriesReconstructed=13;
    s.fleets.clear(); s.turn=packet.deliveryTurn;
    auto events=deliver_due_player_reports(s); assert(events.size()==1 && events.front().aftermath);
    const auto known=known_planet_aftermath(s,1,2); assert(known && known->observedTurn==10 && known->receivedTurn==s.turn);
    assert(known->factoriesMinimum==10 && known->contaminationMaximum==39);
    const auto exported=make_player_view(s,1).state;
    assert(known_planet_aftermath(exported,1,2)==known && exported.planets[1].warDamage.factoriesDestroyed==0);
    // Stale ownership must not turn a foreign projection into local physical truth.
    auto stale=exported; stale.planets[1].owner=1;
    assert(known_planet_aftermath(stale,1,2)==known);
    assert(known_planet_aftermath(make_player_view(stale,1).state,1,2)==known);
}
void passive_remote_and_jammed_sensors_cannot_survey_aftermath()
{
    auto s=fixture(); s.fleets[0].electronics.mode=EmissionMode::Passive;
    observe_planetary_aftermath(s,10); assert(s.players[0].pendingPlayerReports.empty());
    s.fleets[0].electronics.mode=EmissionMode::Standard; s.fleets[0].position={599,0};
    observe_planetary_aftermath(s,10); assert(s.players[0].pendingPlayerReports.empty());
    s.fleets[0].position={600,0}; s.shipDesigns.push_back({2,2,"Jammer",ShipHullType::Destroyer,
        {ShipComponentType::FuelMizer,ShipComponentType::Jammer50,ShipComponentType::Jammer50}});
    Fleet enemy; enemy.id=2; enemy.owner=2; enemy.design=2; enemy.ships={{2,1}}; enemy.position={600,0}; s.fleets.push_back(enemy);
    observe_planetary_aftermath(s,10); assert(s.players[0].pendingPlayerReports.empty());
}
void newer_evidence_wins_and_routine_updates_do_not_spam_messages()
{
    auto s=fixture(); observe_planetary_aftermath(s,10);
    auto old=s.players[0].pendingPlayerReports.front(); s.players[0].pendingPlayerReports.clear();
    s.planets[1].warDamage.contamination=0; observe_planetary_aftermath(s,11);
    auto fresh=s.players[0].pendingPlayerReports.front(); fresh.deliveryTurn=12;
    old.deliveryTurn=13; s.players[0].pendingPlayerReports={old,fresh}; s.turn=12;
    assert(deliver_due_player_reports(s).size()==1); s.turn=13;
    assert(deliver_due_player_reports(s).empty()); assert(known_planet_aftermath(s,1,2)->observedTurn==11);
    auto repeat=fresh; repeat.observedTurn=repeat.aftermath->observedTurn=14; repeat.deliveryTurn=15;
    s.players[0].pendingPlayerReports={repeat}; s.turn=15;
    assert(deliver_due_player_reports(s).empty()); assert(known_planet_aftermath(s,1,2)->observedTurn==14);
}
void bombardment_evidence_can_wait_aboard_a_passive_bomber()
{
    auto s=fixture(); s.shipDesigns[0].components.push_back(ShipComponentType::NeutronBomb);
    s.fleets[0].task=FleetTask::Bombardment; s.fleets[0].electronics.mode=EmissionMode::Passive;
    resolve_bombardments(s,11);
    const auto& report=s.players[0].pendingPlayerReports.front();
    assert(report.aftermath && report.deliveryTurn==kAwaitingTransmission && report.transmissionFleet==1);
    const auto snapshot=*report.aftermath;
    s.turn=12; s.fleets[0].electronics.mode=EmissionMode::Standard; dispatch_fleet_report_outbox(s);
    auto packet=s.players[0].pendingPlayerReports.front(); assert(packet.deliveryTurn!=kAwaitingTransmission);
    s.planets[1].warDamage.contamination=0; s.turn=packet.deliveryTurn;
    const auto events=deliver_due_player_reports(s);
    const auto e=std::find_if(events.begin(),events.end(),[](const auto& e){return e.recipient==1 && e.kind==GameEventKind::Bombardment;});
    assert(e!=events.end() && e->aftermath && e->aftermath->contaminationMaximum==snapshot.contaminationMaximum);
    assert(e->observedTurn==11 && e->aftermath->receivedTurn==s.turn);
}
int main()
{
    immutable_estimates_arrive_after_their_own_communications_delay(); passive_remote_and_jammed_sensors_cannot_survey_aftermath();
    newer_evidence_wins_and_routine_updates_do_not_spam_messages(); bombardment_evidence_can_wait_aboard_a_passive_bomber();
}
