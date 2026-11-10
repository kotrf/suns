// Genuine v118 fixture, generated using the unmodified save writer, headers and
// native start fixture at main 24994bc577905a6f74c77ea031dc353f109036a4.
// The old paid starter is deliberately unshielded, and has advanced to turn 2.
// Rebuild this generator against that revision, never the current v119 writer.
#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include <cstdio>
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    auto state=suns::test::empty_native_orbital_start();
    const auto receipt=suns::initialize_native_orbital_start(state,1,state.planets.front().id,suns::native_orbital_start_budget());
    if(!receipt) return 3;
    const auto turn=suns::TurnProcessor{}.process_with_events(state,{});
    suns::SaveGameData save; save.state=turn.state; save.strategicMessages=turn.events;
    save.mode=suns::SessionMode::Host; save.pendingOrders={1,{}}; save.campaignId=118; save.turnToken=17;save.playerTokens={{1,17}};
    QString error;
    if(!suns::write_save_game_file(QString::fromLocal8Bit(argv[1]),save,error)) {std::fprintf(stderr,"%s\n",qPrintable(error));return 4;}
    std::puts("Generated genuine v118 fixture from pre-radiation writer.");
}
