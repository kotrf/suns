#include "megaproject_fixture.hpp"
#include "orbital_base_fixture.hpp"
#include "native_orbital_fixture.hpp"
#include "suns/terraforming.hpp"
#include "suns/science_samples.hpp"
#include "suns/exotic_materials.hpp"
#include <cassert>
#include <cmath>
using namespace suns;using E=PlanetaryEngineeringKind;using A=EngineeringAction;
namespace {
bool near(double x,double y){return std::abs(x-y)<1e-6;}
const Fleet* find_fleet(const GameState& s,FleetId id)
{const auto f=std::find_if(s.fleets.begin(),s.fleets.end(),[&](const auto& f){return f.id==id;});return f==s.fleets.end()?nullptr:&*f;}
void actual_paid_preparation_and_thirty_year_irreversible_transition()
{
    auto s=test::megaproject_fixture();const auto original=s.planets.front().environment;
    assert(!apply_planetary_engineering_order(s,s.planets.front(),test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)));
    for(unsigned year=1;year<=120;++year) {
        const auto orders=year==1?std::vector<PlayerOrders>{{1,{test::mega_order(1,E::RadicalTransformation,A::StartOrResume)}}}:std::vector<PlayerOrders>{};
        s=TurnProcessor{}.process(s,orders);const auto& p=s.planets.front();
        assert(p.engineering.work==80*year && p.engineering.transformation.phase==TransformationPhase::Preparation);
        assert(p.environment==original && p.naturalEnvironment==original);
        assert(near(p.engineering.invested.ironium,8*year) && near(p.engineering.invested.boranium,4*year));
    }
    const auto held=s.planets.front().population;const auto beforeCommitStocks=s.planets.front().minerals;
    const auto boundaryMining=projected_mineral_mining(s,s.planets.front());
    const auto concentration=planet_mineral_concentration(s,s.planets.front());
    s.players.front().technology.researchActive=true;s.players.front().technology.researchAllocationPercent=100;
    s.players.front().technology.focus=ResearchField::Weapons;s.players.front().technology.levels[std::size_t(ResearchField::Weapons)]=1;
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation),QueueProductionBatchOrder{1,ProductionKind::Factory}}}});
    assert(surface_sheltered(s.planets.front()) && s.planets.front().engineering.transformation.years==1);
    const auto stocks=s.planets.front().minerals;
    assert(near(stocks.ironium,beforeCommitStocks.ironium+boundaryMining.ironium));
    const auto technology=s.players.front().technology.progress;
    for(unsigned year=2;year<=30;++year) {
        const auto before=s.planets.front();
        assert(colony_output(s,before)==0 && projected_population_growth(s,before,s.turn)==0);
        assert(mineral_cargo_mass(projected_mineral_mining(s,before))==0 && !next_terraforming_environment(s,1,before));
        auto result=TurnProcessor{}.process_with_events(s,{});s=std::move(result.state);const auto& p=s.planets.front();
        assert(p.engineering.transformation.years==year && p.population==held);
        assert(p.minerals.ironium==stocks.ironium && p.minerals.boranium==stocks.boranium && p.minerals.germanium==stocks.germanium);
        assert(s.players.front().technology.progress==technology);
        assert(p.engineering.transformation.target==PlanetEnvironment(50,50,50));
        if(year<30)assert(surface_sheltered(p) && p.naturalEnvironment==original && current_planet_habitability(s,p,s.turn)==-100);
    }
    auto& p=s.planets.front();assert(p.environment==PlanetEnvironment(50,50,50) && p.naturalEnvironment==p.environment);
    assert(!surface_sheltered(p) && p.engineering.transformation.phase==TransformationPhase::Complete);
    assert(planet_mineral_concentration(s,p).ironium==concentration.ironium && colony_output(s,p)>0);
    assert(apply_planetary_engineering_order(s,p,test::mega_order(1,E::RadicalTransformation,A::Dismantle)));
    s.orbitalStations.clear();refresh_planetary_engineering(s);
    assert(p.environment==PlanetEnvironment(50,50,50) && p.naturalEnvironment==p.environment);
    const auto industry=p.industry;s.players.front().technology.researchActive=false;s=TurnProcessor{}.process(s,{});assert(s.planets.front().industry>industry);
    assert(std::any_of(s.players.front().history.begin(),s.players.front().history.end(),[](const auto& h){return std::any_of(h.milestones.begin(),h.milestones.end(),[](const auto& m){return m.kind==HistoryMilestoneKind::EngineeringCompleted;});}));
}
void pause_capture_and_returning_support_keep_target_and_partial_changes()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::RadicalTransformation);
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)}}});
    for(unsigned i=0;i<8;++i)s=TurnProcessor{}.process(s,{});
    auto& p=s.planets.front();const auto target=p.engineering.transformation.target,partial=p.environment;
    assert(!apply_planetary_engineering_order(s,p,test::mega_order(1,E::RadicalTransformation,A::Dismantle)));
    assert(!apply_planetary_engineering_order(s,p,test::mega_order(1,E::RadicalTransformation,A::SetOutput)));
    assert(apply_planetary_engineering_order(s,p,test::mega_order(1,E::RadicalTransformation,A::Pause)));
    s=TurnProcessor{}.process(s,{});assert(s.planets.front().environment==partial && s.planets.front().engineering.transformation.years==9);
    assert(apply_planetary_engineering_order(s,s.planets.front(),test::mega_order(1,E::RadicalTransformation,A::StartOrResume)));
    const auto stations=s.orbitalStations;s.orbitalStations.clear();
    for(unsigned i=0;i<4;++i)s=TurnProcessor{}.process(s,{});
    assert(s.planets.front().environment==partial && s.planets.front().engineering.transformation.years==9);
    auto other=s.players.front();other.id=333;other.name="Captor";other.race.habitableRadiation={0,20};other.technology.levels.fill(0);
    s.players.push_back(other);s.planets.front().owner=333;s.orbitalStations=stations;s.orbitalStations.front().owner=333;
    s=TurnProcessor{}.process(s,{});assert(s.planets.front().environment==partial);
    s.players.back().technology.levels.fill(26);s=TurnProcessor{}.process(s,{});
    assert(s.planets.front().engineering.transformation.years==10 && s.planets.front().engineering.transformation.target==target);
    assert(s.planets.front().engineering.transformation.initiator==1 && s.planets.front().engineering.builder==1);
    while(surface_sheltered(s.planets.front()))s=TurnProcessor{}.process(s,{});
    assert(s.planets.front().environment==target);
}
void an_external_paid_orbital_base_can_restore_committed_work()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::RadicalTransformation);
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)}}});
    s.orbitalStations.clear();const auto reference=orbital_test::world();
    s.shipDesigns.push_back(*find_ship_design(reference,200));s.nextShipDesignId=201;
    auto f=reference.fleets.front();f.position=find_star(s,s.planets.front().star)->position;f.id=s.nextFleetId++;
    const auto id=f.id;s.fleets.push_back(f);
    apply_orbital_base_command(s,s.fleets.back(),{OrbitalBaseAction::Deploy,1});
    assert(s.fleets.back().outpost==OutpostKind::OrbitalShipyard);
    for(unsigned i=1;i<=6;++i) {
        s=TurnProcessor{}.process(s,{});
        assert(s.planets.front().engineering.transformation.years==(i==6?2:1));
        assert(colony_output(s,s.planets.front())==0);
    }
    const auto* base=find_fleet(s,id);assert(base && base->orbitalBase.supported && base->minerals.ironium<f.minerals.ironium);
    s.fleets.clear();s=TurnProcessor{}.process(s,{});assert(s.planets.front().engineering.transformation.years==2);
}
void evacuation_and_return_conserve_residents_and_reject_unpaid_capacity()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::RadicalTransformation);s.planets.front().population=10000;
    const auto reference=orbital_test::world();s.shipDesigns.push_back(*find_ship_design(reference,202));s.nextShipDesignId=203;
    Fleet transport;transport.id=s.nextFleetId++;transport.owner=1;transport.design=202;transport.ships={{202,1}};
    transport.position=find_star(s,s.planets.front().star)->position;s.fleets.push_back(transport);const auto id=transport.id;
    set_survey_level(s,1,1,SurveyLevel::DeepSurvey,s.turn);orbital_test::acknowledge(s);
    const PlayerOrders orders{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation),TransferCargoOrder{{1,0},{0,id},10000,{}}}};
    const auto planned=projected_cargo_order_state(make_player_view(s,1).state,orders);
    assert(planned.planets.front().population==0 && find_fleet(planned,id)->colonists==10000);
    s=TurnProcessor{}.process(s,{orders});assert(s.planets.front().population==0 && find_fleet(s,id)->colonists==10000);
    s=TurnProcessor{}.process(s,{});assert(s.planets.front().engineering.transformation.years==2);
    s=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,id},{1,0},10000,{}}}}});
    assert(s.planets.front().population==10000 && find_fleet(s,id)->colonists==0);
    auto* fleet=const_cast<Fleet*>(find_fleet(s,id));fleet->colonists=1;fleet->minerals={1,0,0};
    s=TurnProcessor{}.process(s,{{1,{TransferCargoOrder{{0,id},{1,0},1,{1,0,0}}}}});
    assert(s.planets.front().population==10000 && find_fleet(s,id)->colonists==1);
    s=TurnProcessor{}.process(s,{{1,{SetFleetColonistsOrder{1,id,10001}}}});
    assert(s.planets.front().population==0 && find_fleet(s,id)->colonists==10001);
    s=TurnProcessor{}.process(s,{{1,{SetFleetColonistsOrder{1,id,0}}}});
    assert(s.planets.front().population==0 && find_fleet(s,id)->colonists==10001);
    const auto point=find_fleet(s,id)->position;
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{id,point,1,{FleetArrivalActionKind::UnloadAll,0,FleetCargoKind::Colonists}}}}});
    assert(s.planets.front().population==10000 && find_fleet(s,id)->colonists==1);
    s=TurnProcessor{}.process(s,{{1,{MoveFleetOrder{id,point,1,{FleetArrivalActionKind::LoadAllAvailable,0,FleetCargoKind::Colonists}}}}});
    assert(s.planets.front().population==0 && find_fleet(s,id)->colonists==10001);
}
void actual_conquest_keeps_the_initiators_target_and_returns_excess_survivors()
{
    auto s=test::megaproject_fixture();auto defender=s.players.front();defender.id=2;defender.name="Defender";
    defender.race.habitableRadiation={0,20};defender.surveyKnowledge.clear();s.players.push_back(defender);
    s.players.front().race.radiationImmune=true;s.planets.front().owner=2;s.orbitalStations.front().owner=2;
    s.planets[1].owner=1;s.planets[1].population=1000000;s.planets[1].homeworld=true;s.planets[1].radar=PlanetaryRadarLevel::Full;
    test::prepare_megaproject(s,E::RadicalTransformation);
    s.planets.front().population=1000;
    s=TurnProcessor{}.process(s,{{2,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)}}});
    const auto originalTarget=s.planets.front().engineering.transformation.target,partial=s.planets.front().environment;
    const auto stations=s.orbitalStations;s.orbitalStations.clear();
    const auto reference=orbital_test::world();s.shipDesigns.push_back(*find_ship_design(reference,202));s.nextShipDesignId=203;
    Fleet attacker;attacker.id=s.nextFleetId++;attacker.owner=1;attacker.design=202;attacker.ships={{202,10}};
    attacker.position=find_star(s,s.planets.front().star)->position;attacker.colonists=1000000;attacker.fuel=100;
    attacker.destination=attacker.position;attacker.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::UnloadAll,0,FleetCargoKind::Colonists};
    const auto id=attacker.id;s.fleets.push_back(attacker);s=TurnProcessor{}.process(s,{});
    const auto& captured=s.planets.front();assert(captured.owner==1 && captured.population<=captured.engineering.transformation.shelterCapacity);
    assert(captured.environment==partial && captured.engineering.transformation.target==originalTarget && originalTarget.radiation==10);
    assert(captured.engineering.transformation.initiator==2 && captured.engineering.transformation.years==1);
    const auto surviving=captured.population+find_fleet(s,id)->colonists;
    assert(surviving>=999000 && surviving<=1000000 && find_fleet(s,id)->colonists>0);
    s.orbitalStations=stations;s.orbitalStations.front().owner=1;s=TurnProcessor{}.process(s,{});
    assert(s.planets.front().engineering.transformation.years==2 && s.planets.front().engineering.transformation.target==originalTarget);
}
void an_explicit_experiment_opens_a_deterministic_unknown_exit_once()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::WormholeExperiment);
    assert(s.wormholes.empty() && near(s.planets.front().engineering.invested.ironium,800));
    const PlayerOrders activation{1,{test::mega_order(1,E::WormholeExperiment,A::ActivateExperiment)}};
    const auto a=TurnProcessor{}.process(s,{activation});const auto b=TurnProcessor{}.process(s,{activation});s=a;
    assert(s.wormholes.size()==1 && s.planets.front().engineering.experiment.entrance==s.wormholes.front().endpoints.front().id);
    assert(a.wormholes.front().endpoints.back().position.x==b.wormholes.front().endpoints.back().position.x);
    assert(a.wormholes.front().collapseTurn==s.turn+kExperimentalWormholeYears);
    assert(!apply_planetary_engineering_order(s,s.planets.front(),test::mega_order(1,E::WormholeExperiment,A::ActivateExperiment)));
    const auto view=make_player_view(s,1).state;assert(view.wormholes.empty());
    const auto endpoint=s.wormholes.front().endpoints.back().id;
    const auto* owner=find_player(view,1);assert(owner);
    assert(std::none_of(owner->wormholeKnowledge.begin(),owner->wormholeKnowledge.end(),[&](const auto& k){return k.endpoint==endpoint || k.linkedEndpoint==endpoint;}));
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::WormholeExperiment,A::Dismantle)}}});assert(s.wormholes.size()==1);
    const auto collapse=s.wormholes.front().collapseTurn;
    while(s.turn<collapse)s=TurnProcessor{}.process(s,{});
    assert(s.wormholes.empty());
}
void admissions_and_payloads_are_strict()
{
    auto s=test::megaproject_fixture();auto native=s;native.players.front().race.primaryTrait=PrimaryRaceTrait::HabitatCivilization;
    assert(!planetary_engineering_available(native,1,E::RadicalTransformation) && planetary_engineering_available(native,1,E::WormholeExperiment));
    auto locked=s;locked.players.front().technology.levels[std::size_t(ResearchField::Biology)]=23;
    assert(!planetary_engineering_available(locked,1,E::RadicalTransformation));
    for(auto kind:{E::RadicalTransformation,E::WormholeExperiment}) {
        auto invalid=test::mega_order(1,kind,A::StartOrResume);invalid.strength=0;
        assert(!engineering_order_shape_valid(invalid) && !apply_planetary_engineering_order(s,s.planets.front(),invalid));
    }
    auto invalid=test::mega_order(1,E::WormholeExperiment,A::CommitTransformation);assert(!engineering_order_shape_valid(invalid));
    invalid=test::mega_order(1,E::RadicalTransformation,A::ActivateExperiment);assert(!engineering_order_shape_valid(invalid));
    test::prepare_megaproject(s,E::RadicalTransformation);
    assert(apply_planetary_engineering_order(s,s.planets.front(),test::mega_order(1,E::RadicalTransformation,A::Dismantle)) && !surface_sheltered(s.planets.front()));
}
void experimental_transit_keeps_risk_and_delayed_exit_discovery()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::WormholeExperiment);
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::WormholeExperiment,A::ActivateExperiment)}}});
    const auto reference=orbital_test::world();s.shipDesigns.push_back(*find_ship_design(reference,202));s.nextShipDesignId=203;
    Fleet f;f.id=s.nextFleetId++;f.owner=1;f.design=202;f.ships={{202,1}};f.position=s.wormholes.front().endpoints.front().position;f.colonists=1000;
    s.fleets.push_back(f);orbital_test::acknowledge(s);const auto entrance=s.wormholes.front().endpoints.front().id;
    const auto exit=s.wormholes.front().endpoints.back();auto lost=s;
    s.wormholeRules.minimumLossChance=0;s.wormholeRules.instabilityLossChance=0;
    assert(!enter_wormhole(s,s.fleets.back(),entrance));
    assert(s.fleets.back().position.x==exit.position.x && s.fleets.back().position.y==exit.position.y);
    assert(std::none_of(s.players.front().wormholeKnowledge.begin(),s.players.front().wormholeKnowledge.end(),[&](const auto& k){return k.linkedEndpoint==exit.id;}));
    auto delivered=s.turn;
    for(const auto& report:s.players.front().pendingWormholeReports)delivered=std::max(delivered,report.deliveryTurn);
    s.turn=delivered;(void)deliver_wormhole_reports(s);
    assert(std::any_of(s.players.front().wormholeKnowledge.begin(),s.players.front().wormholeKnowledge.end(),[&](const auto& k){return k.endpoint==entrance && k.linkedEndpoint==exit.id;}));
    lost.wormholeRules.minimumLossChance=1;lost.wormholeRules.instabilityLossChance=0;
    assert(enter_wormhole(lost,lost.fleets.back(),entrance));
    assert(!lost.wormholeTransits.empty() && lost.wormholeTransits.back().status==WormholeTransitStatus::AwaitingEntryReport);
}
void neutral_transition_claims_cannot_expand_paid_shelters()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::RadicalTransformation);s.planets.front().population=1000;
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)}}});
    s.planets.front().owner=0;s.planets.front().population=0;s.orbitalStations.clear();
    Fleet f;f.id=s.nextFleetId++;f.owner=1;f.design=kColonyShipDesignId;f.ships={{f.design,10}};f.colonists=1001;f.fuel=100;
    assert(fleet_can_colonize(s,f) && fleet_cargo_used(s,f)<=fleet_cargo_capacity(s,f));
    f.position=find_star(s,s.planets.front().star)->position;f.destination=f.position;
    f.arrivalAction=FleetArrivalAction{FleetArrivalActionKind::Colonize};s.fleets.push_back(f);
    auto fits=s;fits.fleets.back().colonists=1000;
    s=TurnProcessor{}.process(s,{});assert(s.planets.front().owner==0 && find_fleet(s,f.id)->colonists==1001);
    fits=TurnProcessor{}.process(fits,{});assert(fits.planets.front().owner==1 && fits.planets.front().population==1000);
    assert(surface_sheltered(fits.planets.front()));
}
void an_orbital_civilization_builds_and_reports_its_local_experimental_entrance()
{
    auto s=test::native_orbital_fixture();s.fleets.clear();s.players.front().technology.levels.fill(26);
    s.orbitalStations.front().hull=OrbitalStationHullType::NativeCitadel;s.planets.front().nativeOrbit.population=250000;
    s.planets.front().minerals={10000,10000,10000};
    for(unsigned n=0;n<100;++n)s=TurnProcessor{}.process(s,n==0?std::vector<PlayerOrders>{{1,{test::mega_order(1,E::WormholeExperiment,A::StartOrResume)}}}:std::vector<PlayerOrders>{});
    assert(s.planets.front().population==0 && s.planets.front().engineering.work==8000);
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::WormholeExperiment,A::ActivateExperiment)}}});
    assert(s.wormholes.size()==1);const auto entrance=s.wormholes.front().endpoints.front().id,exit=s.wormholes.front().endpoints.back().id;
    s=TurnProcessor{}.process(s,{});
    assert(std::any_of(s.players.front().wormholeKnowledge.begin(),s.players.front().wormholeKnowledge.end(),[&](const auto& k){return k.endpoint==entrance;}));
    assert(std::none_of(s.players.front().wormholeKnowledge.begin(),s.players.front().wormholeKnowledge.end(),[&](const auto& k){return k.endpoint==exit || k.linkedEndpoint==exit;}));
}
void received_habitability_facts_take_precedence_over_older_project_inference()
{
    auto s=test::megaproject_fixture();test::prepare_megaproject(s,E::RadicalTransformation);
    s=TurnProcessor{}.process(s,{{1,{test::mega_order(1,E::RadicalTransformation,A::CommitTransformation)}}});
    auto received=make_player_view(s,1).state;received.planets.front().observedHabitability=100;
    assert(player_planet_habitability(received,1,received.planets.front(),received.turn)==100);
    assert(current_planet_habitability(s,s.planets.front(),s.turn)==-100);
}
}
int main()
{
    actual_paid_preparation_and_thirty_year_irreversible_transition();
    pause_capture_and_returning_support_keep_target_and_partial_changes();
    an_external_paid_orbital_base_can_restore_committed_work();
    evacuation_and_return_conserve_residents_and_reject_unpaid_capacity();
    actual_conquest_keeps_the_initiators_target_and_returns_excess_survivors();
    an_explicit_experiment_opens_a_deterministic_unknown_exit_once();
    admissions_and_payloads_are_strict();
    experimental_transit_keeps_risk_and_delayed_exit_discovery();
    neutral_transition_claims_cannot_expand_paid_shelters();
    an_orbital_civilization_builds_and_reports_its_local_experimental_entrance();
    received_habitability_facts_take_precedence_over_older_project_inference();
}
