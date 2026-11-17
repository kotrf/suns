#include "rogue_planetoid_fixture.hpp"
#include "suns/computer_player.hpp"
#include "suns/player_knowledge.hpp"
#include "suns/science_samples.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
using namespace suns;
namespace {
void generation_preserves_existing_landmarks_and_adds_one_rare_body()
{
    const auto a=generate_game({70,120,1100,800,25}),b=generate_game({70,120,1100,800,25});
    // Captured with the pre-planetoid executable on main edf665b, seed70.
    const std::array<Position,3> oldPositions{{{81.0799,-142.1504},{-91.7598,-182.704},{205.8364,-56.7984}}};
    const std::array oldKinds{UnknownObjectKind::CompactRemnant,UnknownObjectKind::AncientProbe,UnknownObjectKind::DriftingDerelict};
    assert(a.unknownObjects.size()==4 && b.unknownObjects.size()==4);
    for(std::size_t i=0;i<3;++i) {
        const auto& x=a.unknownObjects[i];
        assert(x.id==i+1 && distance_between(x.position,oldPositions[i])<1e-9);
        assert(x.kind==oldKinds[i]);
    }
    for(std::size_t i=0;i<a.unknownObjects.size();++i) {
        const auto& x=a.unknownObjects[i];const auto& y=b.unknownObjects[i];
        assert(unknown_object_valid(x) && x.id==y.id && x.kind==y.kind && x.position==y.position && x.velocity==y.velocity);
    }
    const auto& body=a.unknownObjects.back();assert(body.kind==UnknownObjectKind::RoguePlanetoid && !body.sampleRecovered);
    assert(std::abs(body.velocity.x)==0.5 && std::abs(body.velocity.y)==0.25);
    for(const auto& star:a.stars)assert(distance_between(star.position,body.position)>=30);
    for(std::size_t i=0;i<3;++i)assert(distance_between(a.unknownObjects[i].position,body.position)>=60);
    const auto noSpawn=[](const GameState& s) {
        return std::none_of(s.unknownObjects.begin(),s.unknownObjects.end(),[](const auto& x){return x.kind==UnknownObjectKind::RoguePlanetoid;});
    };
    assert(noSpawn(generate_game({71,120,1100,800,25})) && noSpawn(generate_game({70,64,1100,800,25})));
    auto legacy=a;legacy.unknownObjects.clear();legacy.nextUnknownObjectId=1;generate_unknown_objects(legacy,false);
    assert(legacy.unknownObjects.size()==2 && noSpawn(legacy));
    generate_unknown_objects(legacy);assert(legacy.unknownObjects.size()==2); // No reseeding an existing campaign.
    const auto view=make_player_view(a,1).state;assert(view.unknownObjects.empty() && view.nextUnknownObjectId==1);
}
void study_and_course_updates_obey_delivery_chronology()
{
    auto s=test::rogue_planetoid_fixture();auto& f=s.fleets.front();f.task=FleetTask::FieldResearch;
    research_unknown_objects(s,f,10);assert(s.players.front().pendingPlayerReports.size()==1);
    auto study=s.players.front().pendingPlayerReports.front();
    assert(study.quantity==32 && study.researchField==ResearchField::Biology && study.objectObservation->kind==UnknownObjectKind::RoguePlanetoid);
    research_unknown_objects(s,f,10);assert(s.players.front().pendingPlayerReports.size()==1 && s.players.front().fieldScience.size()==1);
    study.deliveryTurn=20;s.players.front().pendingPlayerReports.clear();
    advance_unknown_objects(s);advance_unknown_objects(s);observe_unknown_objects(s,12);
    auto fix=s.players.front().pendingPlayerReports.front();fix.deliveryTurn=13;
    s.players.front().pendingPlayerReports={study,fix};s.turn=13;(void)deliver_due_player_reports(s);
    const auto position=known_unknown_object(s,1,7)->position;
    assert(known_unknown_object(s,1,7)->level==ObjectKnowledgeLevel::Classified);
    s.turn=20;(void)deliver_due_player_reports(s);const auto* known=known_unknown_object(s,1,7);
    assert(known->level==ObjectKnowledgeLevel::Studied && known->position==position && known->observedTurn==12);
    assert(known->velocity==Position(0.5,0.25));
    s.unknownObjects.front().position={150,0};observe_unknown_objects(s,21);s.turn=21;(void)deliver_due_player_reports(s);
    known=known_unknown_object(s,1,7);
    assert(known->level==ObjectKnowledgeLevel::Studied && known->kind==UnknownObjectKind::RoguePlanetoid);
    assert(known->observedTurn==21 && known->uncertainty==15 && !known->velocity && object_observation_valid(*known));
    fix.deliveryTurn=22;s.players.front().pendingPlayerReports={fix};s.turn=22;(void)deliver_due_player_reports(s);
    assert(known_unknown_object(s,1,7)->observedTurn==21);
}
void real_rendezvous_and_return_produce_one_physical_biology_sample()
{
    auto s=test::rogue_planetoid_fixture();s.players.front().objectKnowledge={test::drifting_observation(s)};
    const auto fuel=s.fleets.front().fuel;
    s=TurnProcessor{}.process(s,{{1,{SetFleetTaskOrder{1,FleetTask::RecoverSample,7}}}});
    const auto& f=s.fleets.front();
    assert(f.position==s.unknownObjects.front().position && f.position==Position(111.5,0.25) && f.fuel<fuel);
    assert(f.scienceSamples.size()==1 && f.scienceSamples.front().source==7 && f.scienceSamples.front().kind==UnknownObjectKind::RoguePlanetoid);
    assert(s.unknownObjects.front().sampleRecovered && science_sample_mass(f.scienceSamples)==5);
    auto lost=s;lost.fleets.front().scienceSamples.clear();
    lost=TurnProcessor{}.process(lost,{{1,{SetFleetTaskOrder{1,FleetTask::RecoverSample,7}}}});
    assert(lost.fleets.front().scienceSamples.empty() && lost.unknownObjects.front().sampleRecovered);
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{1,{0,0},5,{FleetArrivalActionKind::UnloadAll,1,FleetCargoKind::All}}}}});
    for(unsigned n=0;n<10 && s.fleets.front().destination;++n)s=TurnProcessor{}.process(s,{});
    assert(s.fleets.front().position==Position(0,0) && s.fleets.front().scienceSamples.empty());
    assert(s.planets.front().scienceSamples.size()==1 && s.planets.front().scienceSamples.front().kind==UnknownObjectKind::RoguePlanetoid);
    auto retained=s;assert(process_science_sample(retained,1,{retained.planets.front().id,7,true}));
    auto& tech=retained.players.front().technology;tech.researchActive=true;tech.researchAllocationPercent=20;tech.focus=ResearchField::Biology;
    assert(retained_sample_research(retained,retained.planets.front())==4);
    tech.focus=ResearchField::Construction;assert(retained_sample_research(retained,retained.planets.front())==0);
    assert(process_science_sample(s,1,{s.planets.front().id,7,false}) && s.planets.front().scienceSamples.empty());
    assert(!process_science_sample(s,1,{s.planets.front().id,7,false}));
    const auto& report=s.players.front().pendingPlayerReports.back();
    assert(report.kind==PlayerReportKind::ScientificData && report.quantity==80 && report.researchField==ResearchField::Biology);
}
void computer_uses_only_the_received_planetoid_course()
{
    auto s=test::rogue_planetoid_fixture();s.players.front().controller=PlayerController::Computer;
    s.players.front().objectKnowledge={test::drifting_observation(s,ObjectKnowledgeLevel::Classified)};
    const auto plan=plan_computer_turn(make_player_view(s,1));
    const auto find=[](const ComputerTurnPlan& p) {
        return std::find_if(p.orders.orders.begin(),p.orders.orders.end(),[](const auto& o){
            const auto* m=std::get_if<MoveFleetOrder>(&o);return m && m->arrivalAction.kind==FleetArrivalActionKind::FieldResearch;
        });
    };
    const auto order=find(plan);assert(order!=plan.orders.orders.end());const auto point=std::get<MoveFleetOrder>(*order).destination;
    assert(object_course_contains(s.players.front().objectKnowledge.front(),point) && point!=s.unknownObjects.front().position);
    s.unknownObjects.front().position={999,999};s.unknownObjects.front().velocity={-0.5,0.25};s.unknownObjects.front().sampleRecovered=true;
    const auto hidden=plan_computer_turn(make_player_view(s,1));const auto other=find(hidden);
    assert(other!=hidden.orders.orders.end() && std::get<MoveFleetOrder>(*other).destination==point && plan.nextState==hidden.nextState);
}
}
int main()
{
    generation_preserves_existing_landmarks_and_adds_one_rare_body();
    study_and_course_updates_obey_delivery_chronology();
    real_rendezvous_and_return_produce_one_physical_biology_sample();
    computer_uses_only_the_received_planetoid_course();
}
