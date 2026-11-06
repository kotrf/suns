// Synthetic fixture, compiled with unchanged b7ac910 (save v113).
#include "save_game.hpp"
#include "suns/intergalactic_visitors.hpp"
#include <cassert>
int main(int argc,char** argv) {
    assert(argc==2); suns::SaveGameData d;
    d.campaignId=46113; d.turnToken=11346;
    d.state=suns::make_visitor_demo(suns::VisitorProfile::Shieldwright); d.pendingOrders={1,{}};
    auto& s=d.state; auto& f=s.fleets.front();
    suns::apply_visitor_command(s,f,{s.players[0].visitorKnowledge[0],8});
    f.telemetry.visitor=f.visitor;
    suns::PendingFleetCommand c; c.issuedTurn=s.turn; c.deliveryTurn=s.turn+2; c.visitor=f.visitor; f.pendingCommands.push_back(c);
    auto x=s.players[0].visitorKnowledge[0]; x.receivedTurn=0;
    s.players[0].pendingVisitorReports.push_back({x,suns::VisitorReportKind::Sighted,s.turn,s.turn+1,0});
    suns::GameEvent e; e.id=46113; e.recipient=1; e.kind=suns::GameEventKind::VisitorSighted;
    e.turn=e.observedTurn=s.turn; e.quantity=1; e.visitorProfile=suns::VisitorProfile::Shieldwright; d.strategicMessages={e};
    QString error; assert(suns::write_save_game_file(QString::fromLocal8Bit(argv[1]),d,error));
    suns::SaveGameData read; assert(suns::read_save_game_file(QString::fromLocal8Bit(argv[1]),read,error));
}
