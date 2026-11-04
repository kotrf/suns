// Fixture provenance: compile against original 109afcaa11eb935bcc5357b04b8959dae4af3a2e
// headers, core and src/app/save_game.cpp (save v111). Not a current-build target.
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include <cassert>

int main(int argc, char** argv)
{
    assert(argc==2);
    suns::SaveGameData data;
    data.campaignId=301; data.turnToken=12; data.state=suns::make_demo_game();
    auto& p=data.state.planets.front(); p.population=1000000; p.industry=7; p.mines=3; p.advancedFactories=2; p.advancedMines=1;
    p.productionQueue={{suns::ProductionKind::Factory,6,0,suns::ProductionAutomation::None,2},
        {suns::ProductionKind::Factory,10,0,suns::ProductionAutomation::Annual,1},
        {suns::ProductionKind::Mine,5}};
    auto& player=data.state.players.front(); player.technology.levels[0]=2;
    player.technology.levels[3]=1; player.technology.progress[5]=17;
    player.productionTemplates={{"Legacy industry",{{suns::ProductionKind::Factory,10},
        {suns::ProductionKind::Mine,5,0,suns::ProductionAutomation::Annual,2}}}};
    suns::set_survey_level(data.state,1,p.star,suns::SurveyLevel::DeepSurvey,data.state.turn);
    data.pendingOrders={1,{suns::QueueProductionBatchOrder{p.id,suns::ProductionKind::Factory,1}}};
    data.pendingDescriptions={"Legacy factory order"};
    QString error; assert(suns::write_save_game_file(QString::fromLocal8Bit(argv[1]),data,error));
}
