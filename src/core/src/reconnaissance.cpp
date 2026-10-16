#include "suns/reconnaissance.hpp"
#include "suns/strategic_operations.hpp"
#include "suns/communications.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace suns {
namespace {
struct Receiver { Position position; FleetId fleet{}; PlanetId colony{}; double range{}, tachyon{}, interference{}; bool passive{}; double nominal{}; };
bool newer(const ReconContact& a,const ReconContact& b)
{
    return a.observedTurn>b.observedTurn || (a.observedTurn==b.observedTurn
        && (a.lost>b.lost || (a.lost==b.lost && (a.quality>b.quality || (a.quality==b.quality && a.uncertainty<b.uncertainty)))));
}
std::vector<Receiver> receivers(const GameState& s,PlayerId owner,std::uint64_t turn)
{
    std::vector<Receiver> result;
    for(const auto& p:s.planets) if(p.owner==owner && p.population && p.radar!=PlanetaryRadarLevel::None)
        if(const auto* star=find_star(s,p.star)) {
            const double jam=sensor_interference(s,owner,star->position,turn);
            result.push_back({star->position,0,p.id,planetary_radar_range(p.radar)*(1-jam),0,jam,false,planetary_radar_range(p.radar)});
        }
    for(const auto& f:s.fleets) if(f.owner==owner && fleet_has_scanner(s,f)) {
        const bool passive=!fleet_active_sensors(f,turn);
        const double jam=sensor_interference(s,owner,f.position,turn);
        result.push_back({f.position,f.id,0,fleet_sensor_range(s,f)*(1-jam),fleet_tachyon_fraction(s,f),jam,passive,fleet_sensor_range(s,f)});
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tuple{bool(a.fleet),a.colony,a.fleet}<std::tuple{bool(b.fleet),b.colony,b.fleet};});
    return result;
}
bool live_receiver_allowed(const GameState& s,PlayerId owner,const Receiver& r)
{
    if(!r.fleet) return true;
    if(std::any_of(s.wormholeTransits.begin(),s.wormholeTransits.end(),[&](const auto& t){return t.lastContact.id==r.fleet
        && t.lastContact.owner==owner && t.status!=WormholeTransitStatus::EmergenceConfirmed && t.status!=WormholeTransitStatus::PresumedLost;})) return false;
    const auto* player=find_player(s,owner);
    return player && std::none_of(player->pendingPlayerReports.begin(),player->pendingPlayerReports.end(),[&](const auto& report){return report.battle && report.deliveryTurn>s.turn
        && std::any_of(report.battle->units.begin(),report.battle->units.end(),[&](const auto& u){return u.owner==owner && u.fleet==r.fleet;});});
}
SensorStatus sensor_status(const GameState& s,PlayerId owner,const Receiver& r,std::uint64_t turn)
{
    SensorStatus status; status.fleet=r.fleet; status.colony=r.colony; status.position=r.position;
    status.observedTurn=turn; status.deliveryTurn=fleet_report_delivery_turn(s,owner,r.position,turn,r.fleet);
    status.transmissionFleet=held_report_source(s,owner,r.position,turn,r.fleet);
    status.interferencePercent=std::uint8_t(std::lround(r.interference*100));
    status.nominalRange=r.nominal; status.activeRange=r.passive?0:r.range;
    status.passiveRange=r.range*0.25; status.emissionRange=r.range*2;
    return status;
}
std::optional<ReconContact> observe(const GameState& s,PlayerId owner,const Fleet& target,const Receiver& receiver,
    std::uint64_t turn,bool tracked,ContactQuality held=ContactQuality::Probable)
{
    // A player export's anonymous fleet placeholders are evidence, never truth to rescan.
    if(target.id==receiver.fleet || target.owner==0 || (!target.design && target.ships.empty())) return {};
    const double range=fleet_detection_range(s,target,receiver.range,receiver.tachyon,receiver.passive);
    const double distance=distance_between(receiver.position,target.position);
    if(range<=0 || distance>range*(tracked?1.10:1.0)+1e-6) return {};
    const auto* player=find_player(s,owner); if(!player) return {};
    const auto electronics=player->technology.levels[std::size_t(ResearchField::Electronics)];
    ContactQuality quality=ContactQuality::Probable;
    const double ratio=distance/range;
    if(!receiver.passive && ratio<=0.70) quality=ContactQuality::Confirmed;
    if(!receiver.passive && ratio<=0.25 && electronics>=3 && receiver.interference<=0.25) quality=ContactQuality::Identified;
    if(receiver.passive && ratio<=0.25 && electronics>=6 && receiver.tachyon>=0.5) quality=ContactQuality::Confirmed;
    // Hysteresis also covers classification boundaries, without random flicker.
    if(tracked && held>=ContactQuality::Confirmed && !receiver.passive && ratio<=0.77 && quality==ContactQuality::Probable) quality=ContactQuality::Confirmed;
    if(tracked && held==ContactQuality::Identified && !receiver.passive && ratio<=0.275 && electronics>=3 && receiver.interference<=0.25) quality=ContactQuality::Identified;
    ReconContact c; c.fleet=target.id; c.owner=quality==ContactQuality::Probable?0:target.owner;
    c.quality=quality; c.sensor=receiver.passive?ContactSensor::Passive:ContactSensor::Active;
    c.observedTurn=c.lastSeenTurn=turn; c.sourceFleet=receiver.fleet; c.sourceColony=receiver.colony;
    c.jammed=receiver.interference>0;
    c.position=target.position;
    const double grid=quality==ContactQuality::Probable?10:quality==ContactQuality::Confirmed && ratio>0.25?2:0;
    if(grid) { c.position={std::floor(target.position.x/grid)*grid+grid/2,std::floor(target.position.y/grid)*grid+grid/2}; c.uncertainty=grid*0.75; }
    if(quality!=ContactQuality::Probable) for(const auto& star:s.stars) if(same_position(star.position,target.position)) {
        c.star=star.id; c.position=star.position; break;
    }
    const double mass=fleet_gross_mass(s,target), error=quality==ContactQuality::Probable?0.5:0.25;
    c.massMinimum=std::max(0.0,std::floor(mass*(1-error)/10)*10);
    c.massMaximum=std::ceil(mass*(1+error)/10)*10;
    if(quality==ContactQuality::Identified) {
        c.shipCount=fleet_ship_count(target);
        for(const auto& stack:fleet_ship_stacks(target)) if(const auto* design=find_ship_design(s,stack.design)) c.hulls.push_back(design->hull);
        std::sort(c.hulls.begin(),c.hulls.end()); c.hulls.erase(std::unique(c.hulls.begin(),c.hulls.end()),c.hulls.end());
    }
    c.deliveryTurn=fleet_report_delivery_turn(s,owner,receiver.position,turn,receiver.fleet);
    c.transmissionFleet=held_report_source(s,owner,receiver.position,turn,receiver.fleet);
    return c;
}
void stage(Player& player,ReconContact c,std::optional<PlayerReportKind> event)
{
    // Keep different observation years if an older snapshot will arrive earlier.
    const auto same=[&](const auto& old){return old.fleet==c.fleet && old.observedTurn==c.observedTurn;};
    auto old=std::find_if(player.pendingEnemyContacts.begin(),player.pendingEnemyContacts.end(),same);
    if(old==player.pendingEnemyContacts.end()) player.pendingEnemyContacts.push_back(c);
    else if(c.deliveryTurn<old->deliveryTurn || (c.deliveryTurn==old->deliveryTurn && newer(c,*old))) *old=c;
    if(!event) return;
    if(std::any_of(player.pendingPlayerReports.begin(),player.pendingPlayerReports.end(),[&](const auto& r){return r.kind==*event && r.fleet==c.fleet && r.observedTurn==c.observedTurn;})) return;
    PendingPlayerReport report; report.kind=*event; report.observedTurn=c.observedTurn; report.deliveryTurn=c.deliveryTurn;
    report.fleet=c.fleet; report.star=c.star; report.contactPosition=c.position; report.contactOwner=c.owner; report.contact=c;
    report.transmissionFleet=c.transmissionFleet;
    player.pendingPlayerReports.push_back(std::move(report));
}
}
const char* contact_quality_name(ContactQuality q)
{ return q==ContactQuality::Identified?"Identified":q==ContactQuality::Confirmed?"Confirmed":"Probable"; }
bool recon_contact_valid(const ReconContact& c)
{
    if(!c.fleet || c.quality>ContactQuality::Identified || c.sensor>ContactSensor::Passive
        || !c.observedTurn || !c.lastSeenTurn || c.lastSeenTurn>c.observedTurn || c.deliveryTurn<c.observedTurn
        || (!c.sourceFleet && !c.sourceColony) || (c.sourceFleet && c.sourceColony)
        || !std::isfinite(c.position.x) || !std::isfinite(c.position.y)
        || !std::isfinite(c.uncertainty) || c.uncertainty<0 || c.uncertainty>10000
        || !std::isfinite(c.massMinimum) || !std::isfinite(c.massMaximum) || c.massMinimum<0 || c.massMaximum<c.massMinimum
        || c.massMaximum>1e15 || (c.quality==ContactQuality::Probable && (c.owner || c.star))
        || (c.quality!=ContactQuality::Probable && !c.owner)
        || (c.quality!=ContactQuality::Identified && (c.shipCount || !c.hulls.empty()))
        || (c.quality==ContactQuality::Identified && (!c.shipCount || c.hulls.empty() || c.uncertainty))) return false;
    return c.hulls.size()<=40 && std::is_sorted(c.hulls.begin(),c.hulls.end())
        && std::adjacent_find(c.hulls.begin(),c.hulls.end())==c.hulls.end()
        && std::all_of(c.hulls.begin(),c.hulls.end(),[](auto h){return h>=ShipHullType::Scout && h<=ShipHullType::MetaMorph;});
}
Fleet recon_contact_fleet(const ReconContact& c,std::uint64_t currentTurn)
{
    Fleet f; f.id=c.fleet; f.owner=c.owner; f.design=0; f.fuel=0; f.warp=0;
    f.name=std::string(contact_quality_name(c.quality))+" contact "+std::to_string(c.fleet);
    if(c.lastSeenTurn<currentTurn) f.name+=" [age "+std::to_string(currentTurn-c.lastSeenTurn)+"]";
    f.position=c.position; f.telemetry.observedTurn=c.lastSeenTurn; f.telemetry.position=c.position; f.telemetry.warp=0;
    return f;
}

std::vector<ReconContact> known_enemy_contacts(const GameState& s,PlayerId owner)
{
    const auto* player=find_player(s,owner); if(!player) return {};
    auto result=player->enemyContacts;
    // Connected measurements are available immediately, including the first planning boundary.
    const auto sensors=receivers(s,owner,s.turn);
    for(const auto& target:s.fleets) for(const auto& receiver:sensors) {
        if(target.owner==owner) continue;
        if(!live_receiver_allowed(s,owner,receiver)) continue;
        const auto prior=std::find_if(player->observedEnemyFleets.begin(),player->observedEnemyFleets.end(),[&](const auto& x){return x.fleet==target.id && x.reportingFleet==receiver.fleet && x.reportingColony==receiver.colony;});
        auto c=observe(s,owner,target,receiver,s.turn,prior!=player->observedEnemyFleets.end(),prior!=player->observedEnemyFleets.end()?prior->observation.quality:ContactQuality::Probable);
        if(!c || c->deliveryTurn!=s.turn) continue;
        auto old=std::find_if(result.begin(),result.end(),[&](const auto& x){return x.fleet==c->fleet;});
        if(old==result.end()) result.push_back(*c); else if(newer(*c,*old)) *old=*c;
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.fleet<b.fleet;});
    return result;
}
bool sensor_status_valid(const SensorStatus& x)
{
    return bool(x.fleet)!=bool(x.colony) && x.observedTurn && x.deliveryTurn>=x.observedTurn && x.interferencePercent<=75
        && std::isfinite(x.position.x) && std::isfinite(x.position.y)
        && std::isfinite(x.nominalRange) && x.nominalRange>=0 && x.nominalRange<=10000
        && std::isfinite(x.activeRange) && x.activeRange>=0 && x.activeRange<=x.nominalRange
        && std::isfinite(x.passiveRange) && x.passiveRange>=0 && x.passiveRange<=x.nominalRange*0.25
        && std::isfinite(x.emissionRange) && x.emissionRange>=0 && x.emissionRange<=x.nominalRange*2;
}
std::vector<SensorStatus> known_sensor_status(const GameState& s,PlayerId owner)
{
    const auto* player=find_player(s,owner); if(!player) return {};
    auto result=player->sensorStatus;
    for(const auto& receiver:receivers(s,owner,s.turn)) {
        if(!live_receiver_allowed(s,owner,receiver)) continue;
        const auto status=sensor_status(s,owner,receiver,s.turn);
        if(status.deliveryTurn!=s.turn) continue;
        auto old=std::find_if(result.begin(),result.end(),[&](const auto& x){return x.fleet==status.fleet && x.colony==status.colony;});
        if(old==result.end()) result.push_back(status); else if(status.observedTurn>=old->observedTurn) *old=status;
    }
    return result;
}

void observe_reconnaissance(GameState& s,std::uint64_t turn)
{
    for(auto& player:s.players) {
        std::vector<Player::EnemyContact> visible;
        const auto sensors=receivers(s,player.id,turn);
        std::vector<SensorStatus> statuses;
        for(const auto& receiver:sensors) {
            const auto status=sensor_status(s,player.id,receiver,turn); statuses.push_back(status);
            if(status.deliveryTurn!=kAwaitingTransmission) record_report_transmission(s,player.id,receiver.position,turn,receiver.fleet);
            auto duplicate=std::find_if(player.pendingSensorStatus.begin(),player.pendingSensorStatus.end(),[&](const auto& x){return x.fleet==status.fleet && x.colony==status.colony && x.observedTurn==turn;});
            if(duplicate==player.pendingSensorStatus.end()) player.pendingSensorStatus.push_back(status);
            const auto old=std::find_if(player.observedSensorStatus.begin(),player.observedSensorStatus.end(),[&](const auto& x){return x.fleet==status.fleet && x.colony==status.colony;});
            if((old==player.observedSensorStatus.end()?0:old->interferencePercent)!=status.interferencePercent) {
                PendingPlayerReport report; report.kind=PlayerReportKind::SensorInterferenceChanged;
                report.observedTurn=turn; report.deliveryTurn=status.deliveryTurn; report.fleet=status.fleet;
                report.planet=status.colony; report.position=status.position; report.quantity=status.interferencePercent;
                report.sensorStatus=status; player.pendingPlayerReports.push_back(report);
                player.pendingPlayerReports.back().transmissionFleet=status.transmissionFleet;
            }
        }
        player.observedSensorStatus=std::move(statuses);
        for(const auto& target:s.fleets) {
            auto previous=std::find_if(player.observedEnemyFleets.begin(),player.observedEnemyFleets.end(),[&](const auto& c){return c.fleet==target.id;});
            std::optional<ReconContact> best;
            for(const auto& receiver:sensors) {
                const bool tracked=previous!=player.observedEnemyFleets.end() && previous->reportingFleet==receiver.fleet && previous->reportingColony==receiver.colony;
                if(auto c=observe(s,player.id,target,receiver,turn,tracked,tracked?previous->observation.quality:ContactQuality::Probable))
                    if(!best || std::tuple{c->deliveryTurn,-int(c->quality),c->uncertainty,bool(c->sourceFleet),c->sourceColony,c->sourceFleet}
                        <std::tuple{best->deliveryTurn,-int(best->quality),best->uncertainty,bool(best->sourceFleet),best->sourceColony,best->sourceFleet}) best=*c;
            }
            if(!best) continue;
            if(target.owner==player.id) {
                auto& pending=player.pendingFriendlyFleetContacts;
                auto duplicate=std::find_if(pending.begin(),pending.end(),[&](const auto& c){return c.fleet==target.id && c.observedTurn==turn;});
                if(duplicate==pending.end()) pending.push_back(*best);
                else if(best->deliveryTurn<duplicate->deliveryTurn
                    || (best->deliveryTurn==duplicate->deliveryTurn && newer(*best,*duplicate))) *duplicate=*best;
                continue;
            }
            const auto event=previous==player.observedEnemyFleets.end()?std::optional{PlayerReportKind::EnemyFleetDetected}
                : previous->observation.quality!=best->quality || previous->observation.jammed!=best->jammed
                    ?std::optional{PlayerReportKind::EnemyFleetReclassified}:std::nullopt;
            stage(player,*best,event);
            visible.push_back({target.id,target.owner,best->position,best->sourceFleet,best->sourceColony,*best});
        }
        for(const auto& old:player.observedEnemyFleets) {
            if(std::any_of(visible.begin(),visible.end(),[&](const auto& c){return c.fleet==old.fleet;})) continue;
            const auto source=std::find_if(sensors.begin(),sensors.end(),[&](const auto& r){return r.fleet==old.reportingFleet && r.colony==old.reportingColony;});
            if(source==sensors.end() || !old.observation.fleet) continue; // No surviving observer; age remains unknown.
            auto lost=old.observation; lost.lost=true; lost.observedTurn=turn;
            lost.deliveryTurn=fleet_report_delivery_turn(s,player.id,source->position,turn,source->fleet);
            lost.transmissionFleet=held_report_source(s,player.id,source->position,turn,source->fleet);
            stage(player,lost,PlayerReportKind::EnemyFleetLost);
        }
        std::sort(visible.begin(),visible.end(),[](const auto& a,const auto& b){return a.fleet<b.fleet;});
        player.observedEnemyFleets=std::move(visible);
    }
}
void deliver_reconnaissance(GameState& s)
{
    for(auto& player:s.players) {
        std::sort(player.pendingSensorStatus.begin(),player.pendingSensorStatus.end(),[](const auto& a,const auto& b){return std::tuple{a.deliveryTurn,a.observedTurn,a.fleet,a.colony}<std::tuple{b.deliveryTurn,b.observedTurn,b.fleet,b.colony};});
        for(const auto& status:player.pendingSensorStatus) if(status.deliveryTurn<=s.turn) {
            auto old=std::find_if(player.sensorStatus.begin(),player.sensorStatus.end(),[&](const auto& x){return x.fleet==status.fleet && x.colony==status.colony;});
            if(old==player.sensorStatus.end()) player.sensorStatus.push_back(status); else if(status.observedTurn>old->observedTurn) *old=status;
        }
        std::erase_if(player.pendingSensorStatus,[&](const auto& x){return x.deliveryTurn<=s.turn;});
        std::sort(player.pendingEnemyContacts.begin(),player.pendingEnemyContacts.end(),[](const auto& a,const auto& b){return std::tuple{a.deliveryTurn,a.observedTurn,a.fleet,int(a.quality),a.lost}<std::tuple{b.deliveryTurn,b.observedTurn,b.fleet,int(b.quality),b.lost};});
        for(const auto& c:player.pendingEnemyContacts) if(c.deliveryTurn<=s.turn) {
            auto old=std::find_if(player.enemyContacts.begin(),player.enemyContacts.end(),[&](const auto& x){return x.fleet==c.fleet;});
            if(old==player.enemyContacts.end()) player.enemyContacts.push_back(c); else if(newer(c,*old)) *old=c;
        }
        std::erase_if(player.pendingEnemyContacts,[&](const auto& c){return c.deliveryTurn<=s.turn;});
        std::sort(player.enemyContacts.begin(),player.enemyContacts.end(),[](const auto& a,const auto& b){return a.fleet<b.fleet;});
        auto& pending=player.pendingFriendlyFleetContacts;
        std::sort(pending.begin(),pending.end(),[](const auto& a,const auto& b){return std::tuple{a.deliveryTurn,a.observedTurn,a.fleet}<std::tuple{b.deliveryTurn,b.observedTurn,b.fleet};});
        for(const auto& c:pending) if(c.deliveryTurn<=s.turn) {
            auto& contacts=player.friendlyFleetContacts;
            auto old=std::find_if(contacts.begin(),contacts.end(),[&](const auto& x){return x.fleet==c.fleet;});
            if(old==contacts.end()) contacts.push_back(c); else if(newer(c,*old)) *old=c;
        }
        std::erase_if(pending,[&](const auto& c){return c.deliveryTurn<=s.turn;});
        std::sort(player.friendlyFleetContacts.begin(),player.friendlyFleetContacts.end(),[](const auto& a,const auto& b){return a.fleet<b.fleet;});
    }
}
}
