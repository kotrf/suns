#include "suns/transport_lines.hpp"
#include "suns/communications.hpp"
#include "suns/mobile_base.hpp"
#include <algorithm>
#include <limits>

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
    FleetArrivalAction unload;unload.kind=FleetArrivalActionKind::UnloadAll;unload.cargo=cargo;
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
std::string transport_assignment_status(const PlayerView& view,const TransportLine& line,const TransportAssignment& a) {
    const auto* f=find_service_fleet(view.state,a.fleet);
    if(!f)return "No received fleet contact";
    if(f->telemetry.observedTurn<=a.issuedTurn)return "Awaiting telemetry after dispatch";
    if(!line.spec.active)return !f->destination && !f->repeatOrders?"Stop confirmed":"Stop sent; old programme still reported";
    const auto program=transport_line_program(line);
    const auto& route=f->routeTemplate;
    const bool matches=f->repeatOrders && route.size()==2 && same_position(route[0].destination,program.destination)
        && route[0].warp==program.warp && route[0].arrivalAction==program.arrivalAction
        && same_position(route[1].destination,line.destinationPosition) && route[1].warp==line.spec.warp
        && route[1].arrivalAction==program.queuedWaypoints[0].arrivalAction;
    if(!matches)return "Sent programme not confirmed (or overridden aboard)";
    if(f->damagePercent>=100)return "Reported disabled by damage";
    const auto reason=transport_line_error(view,line.spec);if(!reason.empty())return reason;
    if(f->arrivalAction && f->arrivalAction->kind==FleetArrivalActionKind::LoadMinerals
        && same_position(f->position,line.sourcePosition)) {
        const auto amount=line.spec.cargo==TransportCargo::Ironium?f->minerals.ironium:
            line.spec.cargo==TransportCargo::Boranium?f->minerals.boranium:f->minerals.germanium;
        if(amount+std::max(0.0,fleet_cargo_capacity(view.state,*f)-fleet_cargo_used(view.state,*f))+1e-6<line.spec.departureMinimum)
            return "Waiting: reported shared hold cannot fit the departure minimum";
        return "Waiting at source for departure minimum; loaded cargo retained; source reserve enforced on site";
    }
    const auto* source=find_planet(view.state,line.spec.source);
    if(source && source->owner==view.player) {
        const auto stock=line.spec.cargo==TransportCargo::Ironium?source->minerals.ironium:
            line.spec.cargo==TransportCargo::Boranium?source->minerals.boranium:source->minerals.germanium;
        if(stock<=0)return "Programme confirmed; last received source stock empty (cycles continue)";
    }
    return source && !source->owner?"Programme confirmed; mining cache is checked physically on arrival":
        "Repeating programme confirmed; normal fuel, service and combat apply";
}
}
