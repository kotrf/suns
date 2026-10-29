#include "suns/transport_lines.hpp"
#include "suns/communications.hpp"
#include "suns/mobile_base.hpp"
#include <algorithm>
#include <limits>
#include <cmath>
#include <tuple>

namespace suns {
namespace {
Player* owner(GameState& s, PlayerId id) {
    const auto p=std::find_if(s.players.begin(),s.players.end(),[&](const auto& p){return p.id==id;});
    return p==s.players.end()?nullptr:&*p;
}
void dispatch(GameState& s, PlayerId player, const TransportLine& line, FleetId fleet) {
    const auto p=transport_line_program(line);
    // Failure on a physically absent fleet is not acknowledgement at HQ.
    (void)submit_fleet_route_command(s,player,fleet,p.destination,p.warp,p.arrivalAction,
        p.queuedWaypoints,p.repeatOrders,0,p.clearRoute);
}
}
bool transport_line_spec_valid(const TransportLineSpec& d) {
    return d.id && d.revision && !d.name.empty() && d.name.size()<=64 && d.source && d.destination
        && d.sourceReserve<=1000000000 && d.departureMinimum<=1000000000
        && d.source!=d.destination && d.warp>=1 && d.warp<=kMaxWarp && d.cargo<=TransportCargo::Germanium;
}
std::string transport_line_error(const PlayerView& view,const TransportLineSpec& d) {
    if(!transport_line_spec_valid(d))return "Name, distinct endpoints, mineral and Warp 1–10 are required";
    const auto* from=find_planet(view.state,d.source);const auto* to=find_planet(view.state,d.destination);
    if(!from || !to)return "Both endpoints need received planetary surveys";
    const auto* a=find_star(view.state,from->star);const auto* b=find_star(view.state,to->star);
    if(!a || !b || same_position(a->position,b->position))return "Endpoints need distinct received system coordinates";
    if(!d.active)return {};
    if(from->owner && from->owner!=view.player)return "The received source belongs to another empire";
    if(!from->owner && survey_level(view.state,view.player,from->star)<SurveyLevel::GeologicalSurvey)
        return "An uncolonized mining source needs a delivered Geological Survey";
    if(to->owner!=view.player)return "The received destination must be your colony";
    return {};
}
FleetRouteProgram transport_line_program(const TransportLine& line) {
    FleetRouteProgram p;p.destination=line.sourcePosition;p.warp=line.spec.warp;
    if(!line.spec.active){p.clearRoute=true;return p;}
    const auto cargo=line.spec.cargo==TransportCargo::Ironium?FleetCargoKind::Ironium:
        line.spec.cargo==TransportCargo::Boranium?FleetCargoKind::Boranium:FleetCargoKind::Germanium;
    p.arrivalAction.kind=line.spec.sourceReserve || line.spec.departureMinimum
        ? FleetArrivalActionKind::LoadMinerals : FleetArrivalActionKind::LoadAllAvailable;
    p.arrivalAction.cargo=cargo;p.arrivalAction.mineralReserve=line.spec.sourceReserve;
    p.arrivalAction.mineralMinimum=line.spec.departureMinimum;
    FleetArrivalAction unload;unload.kind=FleetArrivalActionKind::UnloadTransportLine;unload.cargo=cargo;
    unload.transportLine=line.spec.id;unload.transportRevision=line.spec.revision;unload.transportDestination=line.spec.destination;
    p.queuedWaypoints={{line.destinationPosition,line.spec.warp,unload}};p.repeatOrders=true;
    return p;
}
std::optional<MoveFleetOrder> pending_transport_route(const GameState& host,const PlayerOrders& orders,FleetId fleet) {
    if(std::none_of(orders.orders.begin(),orders.orders.end(),[](const auto& o){return std::holds_alternative<SetTransportLineOrder>(o) || std::holds_alternative<AssignTransportLineOrder>(o);}))return {};
    auto view=make_player_view(host,orders.player);std::optional<MoveFleetOrder> result;
    const auto remember=[&](const TransportLine& line,bool stop) {
        auto snapshot=line;if(stop)snapshot.spec.active=false;const auto p=transport_line_program(snapshot);
        result=MoveFleetOrder{fleet,p.destination,p.warp,p.arrivalAction,p.queuedWaypoints,p.repeatOrders,0,p.clearRoute};
    };
    for(const auto& order:orders.orders) {
        if(const auto* o=std::get_if<SetTransportLineOrder>(&order)) {
            const auto& lines=view.state.players.front().transportLines;
            const auto old=std::find_if(lines.begin(),lines.end(),[&](const auto& l){return l.spec.id==o->value.id;});
            const auto previous=old==lines.end()?std::optional<TransportLine>{}:std::optional{*old};
            if(set_transport_line(view.state,orders.player,*o) && previous
                && std::any_of(previous->assignments.begin(),previous->assignments.end(),[&](auto a){return a.fleet==fleet;})) {
                if(o->remove)remember(*previous,true);
                else for(const auto& line:lines)if(line.spec.id==o->value.id)remember(line,false);
            }
        } else if(const auto* o=std::get_if<AssignTransportLineOrder>(&order)) {
            const auto& lines=view.state.players.front().transportLines;
            const auto old=std::find_if(lines.begin(),lines.end(),[&](const auto& l){return l.spec.id==o->line;});
            const auto previous=old==lines.end()?std::optional<TransportLine>{}:std::optional{*old};
            if(assign_transport_line(view.state,orders.player,*o) && o->fleet==fleet && previous)remember(*previous,o->remove);
        } else if(const auto f=transport_manual_override(order)) {
            release_transport_assignment(view.state,orders.player,*f);
            if(*f==fleet) {
                if(const auto* move=std::get_if<MoveFleetOrder>(&order))result=*move;
                else result.reset();
            }
        }
    }
    return result;
}
std::optional<FleetId> transport_manual_override(const Order& order) {
    return std::visit([](const auto& value)->std::optional<FleetId> {
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,MoveFleetOrder> || std::is_same_v<T,SetFleetTaskOrder>
            || std::is_same_v<T,SetRemoteMiningOrder> || std::is_same_v<T,SetProbeProgramOrder>
            || std::is_same_v<T,EncounterVisitorOrder> || std::is_same_v<T,ColonizePlanetOrder>
            || std::is_same_v<T,GateFleetOrder>)return value.fleet;
        return {};
    },order);
}
void release_transport_assignment(GameState& s,PlayerId id,FleetId fleet) {
    if(auto* p=owner(s,id))for(auto& line:p->transportLines)
        std::erase_if(line.assignments,[&](const auto& a){return a.fleet==fleet;});
}
bool set_transport_line(GameState& s,PlayerId id,const SetTransportLineOrder& order) {
    auto* p=owner(s,id);if(!p)return false;
    const auto& d=order.value;
    const auto it=std::find_if(p->transportLines.begin(),p->transportLines.end(),[&](const auto& l){return l.spec.id==d.id;});
    if(it!=p->transportLines.end()) {
        if(it->spec.revision==std::numeric_limits<std::uint64_t>::max() || d.revision!=it->spec.revision+1)return false;
        if(order.remove) {
            auto stop=*it;stop.spec.active=false;
            for(const auto& a:stop.assignments)dispatch(s,id,stop,a.fleet);
            p->transportLines.erase(it);return true;
        }
    } else if(order.remove || !d.id || d.id<=p->lastTransportLineId || d.revision!=1
        || p->transportLines.size()>=kMaximumTransportLines)return false;
    const auto view=make_player_view(s,id);
    if(!transport_line_error(view,d).empty())return false;
    const auto* from=find_planet(view.state,d.source);const auto* to=find_planet(view.state,d.destination);
    const auto* a=find_star(view.state,from->star);const auto* b=find_star(view.state,to->star);
    if(!a || !b)return false;
    if(it==p->transportLines.end()) {
        p->lastTransportLineId=d.id;p->transportLines.push_back({d,a->position,b->position,{}});return true;
    }
    it->spec=d;it->sourcePosition=a->position;it->destinationPosition=b->position;
    for(auto& assignment:it->assignments) {
        assignment.issuedTurn=s.turn;dispatch(s,id,*it,assignment.fleet);
    }
    return true;
}
bool assign_transport_line(GameState& s,PlayerId id,const AssignTransportLineOrder& order) {
    auto* p=owner(s,id);if(!p)return false;
    auto it=std::find_if(p->transportLines.begin(),p->transportLines.end(),[&](const auto& l){return l.spec.id==order.line;});
    if(it==p->transportLines.end() || !order.fleet)return false;
    const auto member=std::find_if(it->assignments.begin(),it->assignments.end(),[&](const auto& a){return a.fleet==order.fleet;});
    if(order.remove) {
        if(member==it->assignments.end())return false;
        auto stop=*it;stop.spec.active=false;dispatch(s,id,stop,order.fleet);
        it->assignments.erase(member);return true;
    }
    if(member!=it->assignments.end())return false; // Replayed assignment does not send another command.
    if(it->assignments.size()>=kMaximumLineAssignments)return false;
    const auto view=make_player_view(s,id);const auto* f=find_service_fleet(view.state,order.fleet);
    if(!f || f->owner!=id || f->outpost!=OutpostKind::None || fleet_cargo_capacity(view.state,*f)<=0)return false;
    release_transport_assignment(s,id,order.fleet);
    it->assignments.push_back({order.fleet,s.turn});
    dispatch(s,id,*it,order.fleet);return true;
}
namespace {
auto receipt_key(const TransportLineReceipt& r) {
    return std::tuple{r.observedTurn,r.line,r.revision,r.fleet,r.planet};
}
bool programme_matches(const TransportLine& line,const FleetTelemetry& t) {
    const auto p=transport_line_program(line);const auto& route=t.routeTemplate;
    if(!t.repeatOrders || route.size()!=2 || !same_position(route[0].destination,p.destination)
        || route[0].warp!=p.warp || route[0].arrivalAction!=p.arrivalAction
        || !same_position(route[1].destination,line.destinationPosition) || route[1].warp!=line.spec.warp)return false;
    if(route[1].arrivalAction==p.queuedWaypoints[0].arrivalAction)return true;
    // Existing v102/v103 programmes keep flying without an undelivered upgrade.
    auto legacy=p.queuedWaypoints[0].arrivalAction;legacy.kind=FleetArrivalActionKind::UnloadAll;
    legacy.transportLine=0;legacy.transportRevision=0;legacy.transportDestination=0;
    return route[1].arrivalAction==legacy;
}
}
std::uint64_t transport_statistics_start(const Player& p,std::uint64_t turn) {
    (void)p;return turn>=kTransportStatisticsYears?turn-kTransportStatisticsYears+1:1;
}
bool transport_receipt_valid(const TransportLineReceipt& r,std::uint64_t turn) {
    const auto& m=r.minerals;
    return r.line && r.revision && r.fleet && r.planet && r.observedTurn && r.observedTurn<=r.receivedTurn
        && r.receivedTurn<=turn && std::isfinite(m.ironium) && std::isfinite(m.boranium) && std::isfinite(m.germanium)
        && m.ironium>=0 && m.boranium>=0 && m.germanium>=0 && mineral_cargo_mass(m)>0
        && (int(m.ironium>0)+int(m.boranium>0)+int(m.germanium>0)==1);
}
void prune_transport_receipts(Player& p,std::uint64_t turn) {
    (void)turn;
    std::erase_if(p.transportReceipts,[&](const auto& r){return r.observedTurn<=p.transportReceiptsTruncatedThrough;});
}
void receive_transport_receipt(Player& p,const PendingPlayerReport& report,std::uint64_t turn) {
    if(report.kind!=PlayerReportKind::TransportLineDelivery || report.deliveryTurn>turn)return;
    TransportLineReceipt r{report.transportLine,report.transportRevision,report.fleet,report.planet,
        report.observedTurn,turn,report.deliveredMinerals};
    if(!transport_receipt_valid(r,turn) || r.line>p.lastTransportLineId || r.observedTurn<=p.transportReceiptsTruncatedThrough)return;
    prune_transport_receipts(p,turn);auto& entries=p.transportReceipts;
    const auto at=std::lower_bound(entries.begin(),entries.end(),r,[](const auto& a,const auto& b){return receipt_key(a)<receipt_key(b);});
    if(at!=entries.end() && receipt_key(*at)==receipt_key(r))return;
    entries.insert(at,r);
    if(entries.size()>kMaximumTransportReceipts) {
        p.transportReceiptsTruncatedThrough=entries.front().observedTurn;
        prune_transport_receipts(p,turn); // Drop the whole oldest year; never advertise a partial year as complete coverage.
    }
}
TransportLineMetrics transport_line_metrics(const PlayerView& view,const TransportLine& line) {
    TransportLineMetrics m;const auto& player=view.state.players.front();
    m.startTurn=transport_statistics_start(player,view.state.turn);m.endTurn=view.state.turn;
    for(const auto& r:player.transportReceipts)if(r.line==line.spec.id && r.receivedTurn>=m.startTurn && r.receivedTurn<=m.endTurn) {
        ++m.deliveries;m.delivered.ironium+=r.minerals.ironium;m.delivered.boranium+=r.minerals.boranium;m.delivered.germanium+=r.minerals.germanium;
    }
    m.nominalRoundTripYears=2*std::ceil(distance_between(line.sourcePosition,line.destinationPosition)/warp_distance(line.spec.warp));
    for(const auto& a:line.assignments) {
        const auto* f=find_service_fleet(view.state,a.fleet);
        if(!f || f->telemetry.observedTurn<=a.issuedTurn || !line.spec.active || !programme_matches(line,f->telemetry)) {++m.unconfirmed;continue;}
        const auto& t=f->telemetry;++m.contacts;
        if(!m.oldestTelemetry || t.observedTurn<m.oldestTelemetry)m.oldestTelemetry=t.observedTurn;
        m.newestTelemetry=std::max(m.newestTelemetry,t.observedTurn);
        if(t.destination && !same_position(t.position,*t.destination))++m.moving;
        else if(t.arrivalAction && t.arrivalAction->kind==FleetArrivalActionKind::LoadMinerals
            && same_position(t.position,line.sourcePosition))++m.waiting;
        m.cargoUsed+=fleet_cargo_used(view.state,*f);m.cargoCapacity+=fleet_cargo_capacity(view.state,*f);
    }
    return m;
}
std::string transport_assignment_status(const PlayerView& view,const TransportLine& line,const TransportAssignment& a) {
    const auto* f=find_service_fleet(view.state,a.fleet);
    if(!f)return "No received fleet contact; loss or command failure is not confirmed";
    const auto& t=f->telemetry;
    if(t.observedTurn<=a.issuedTurn)return "Awaiting telemetry after dispatch";
    if(!line.spec.active)return !t.destination && !t.repeatOrders?"Stop confirmed":"Stop sent; old programme still reported";
    if(!programme_matches(line,t))return "Sent programme not confirmed (or overridden aboard)";
    if(t.damagePercent>=100)return "Reported disabled by damage";
    const auto reason=transport_line_error(view,line.spec);if(!reason.empty())return reason;
    if(t.fuel<=0 && t.destination && !same_position(t.position,*t.destination))
        return "Reported empty fuel tank; drift or fitted fuel-free travel may continue; later refuelling is unconfirmed";
    if(t.arrivalAction && t.arrivalAction->kind==FleetArrivalActionKind::LoadMinerals
        && same_position(t.position,line.sourcePosition)) {
        const auto amount=line.spec.cargo==TransportCargo::Ironium?t.minerals.ironium:
            line.spec.cargo==TransportCargo::Boranium?t.minerals.boranium:t.minerals.germanium;
        if(amount+std::max(0.0,fleet_cargo_capacity(view.state,*f)-fleet_cargo_used(view.state,*f))+1e-6<line.spec.departureMinimum)
            return "Waiting: reported shared hold cannot fit the departure minimum";
        return "Waiting at source for departure minimum; loaded cargo retained; source reserve enforced on site";
    }
    const auto* source=find_planet(view.state,line.spec.source);
    if(source && source->owner==view.player) {
        const auto stock=line.spec.cargo==TransportCargo::Ironium?source->minerals.ironium:
            line.spec.cargo==TransportCargo::Boranium?source->minerals.boranium:source->minerals.germanium;
        if(stock<=line.spec.sourceReserve)return line.spec.departureMinimum
            ?"Received source stock at/below reserve; loading may wait; future supply is unknown"
            :"Programme confirmed; received source stock at/below reserve (empty cycles continue)";
    }
    if(t.routeTemplate[1].arrivalAction.kind==FleetArrivalActionKind::UnloadAll)
        return "Legacy repeating programme confirmed; no line receipts until an explicit edit/reassignment is received";
    return source && !source->owner?"Programme confirmed; mining cache is checked physically on arrival":
        "Repeating programme confirmed; normal fuel, service and combat apply";
}
}
