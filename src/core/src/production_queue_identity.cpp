#include "suns/production_queue_identity.hpp"
#include <algorithm>

namespace suns::detail {
ProductionQueueIdentity::ProductionQueueIdentity(const GameState& state,const GameState* received) {
    for(const auto& p:state.planets) {
        auto& rows=rows_[p.id];rows.items=p.productionQueue;
        for(std::size_t i=0;i<rows.items.size();++i)rows.origins.push_back({true,received?1u:0u,i});
        if(!received)continue;
        const auto* known=find_planet(*received,p.id);if(!known)continue;
        const auto& initial=known->productionQueue;
        const auto* player=find_player(*received,known->owner);if(!player)continue;
        const auto report=std::find_if(player->surveyKnowledge.begin(),player->surveyKnowledge.end(),
            [&](const auto& entry){return entry.star==known->star;});
        // Row values (even an exact, unique match) cannot prove continuity
        // across production years: completed work may have an identical successor.
        // Without persistent row IDs, bind original rows only to the current
        // final economic snapshot and an unchanged complete queue. New rows in
        // this submission still use their explicit command origins below.
        if(report==player->surveyKnowledge.end() || report->observedOwner!=known->owner
            || report->observedTurn!=received->turn || !report->planetSnapshot
            || !report->planetSnapshot->habitat
            || report->planetSnapshot->habitat->stationEvidenceTime!=3*received->turn+2
            || !report->planetSnapshot->surfaceMinerals
            || !report->planetSnapshot->surfaceMinerals->finalSurface
            || report->planetSnapshot->surfaceMinerals->observedTurn!=received->turn
            || initial!=rows.items)continue;
        for(std::size_t i=0;i<initial.size();++i)rows.origins[i]={true,0,i};
    }
}
void ProductionQueueIdentity::synchronize(const GameState& state,std::size_t previousOrder) {
    for(const auto& p:state.planets) {
        auto& rows=rows_[p.id];if(rows.items==p.productionQueue)continue;
        std::vector<QueueRowOrigin> origins;std::size_t cursor=0,added=0;
        for(const auto& item:p.productionQueue) {
            auto found=std::find(rows.items.begin()+cursor,rows.items.end(),item);
            if(found==rows.items.end())origins.push_back({false,previousOrder,added++});
            else {const auto index=std::size_t(found-rows.items.begin());origins.push_back(rows.origins[index]);cursor=index+1;}
        }
        rows.items=p.productionQueue;rows.origins=std::move(origins);
    }
}
QueueEditTarget ProductionQueueIdentity::target(const Order& order) const {
    QueueEditTarget result;
    const auto get=[&](PlanetId id,std::size_t index)->std::optional<QueueRowOrigin>{
        const auto rows=rows_.find(id);if(rows==rows_.end() || index>=rows->second.origins.size())return {};
        return rows->second.origins[index];
    };
    if(const auto* cancel=std::get_if<CancelProductionOrder>(&order)){result.colony=cancel->colony;result.source=get(cancel->colony,cancel->index);}
    if(const auto* move=std::get_if<ReorderProductionQueueOrder>(&order)){result.colony=move->colony;result.source=get(move->colony,move->fromIndex);result.destination=get(move->colony,move->toIndex);result.after=move->fromIndex<move->toIndex;}
    return result;
}
bool ProductionQueueIdentity::bind(Order& order,const QueueEditTarget& target) const {
    if(!std::holds_alternative<CancelProductionOrder>(order) && !std::holds_alternative<ReorderProductionQueueOrder>(order))return true;
    const auto rows=rows_.find(target.colony);if(rows==rows_.end() || !target.source)return false;
    const auto& origins=rows->second.origins;
    const auto from=std::find(origins.begin(),origins.end(),*target.source);if(from==origins.end())return false;
    const auto source=std::size_t(from-origins.begin());
    if(auto* cancel=std::get_if<CancelProductionOrder>(&order)){cancel->index=source;return true;}
    if(!target.destination)return false;
    const auto to=std::find(origins.begin(),origins.end(),*target.destination);
    if(to==origins.end() || to==from)return false;
    const auto destination=std::size_t(to-origins.begin());
    auto& move=std::get<ReorderProductionQueueOrder>(order);move.fromIndex=source;
    move.toIndex=destination-(source<destination?1:0)+(target.after?1:0);return true;
}
void ProductionQueueIdentity::erased(PlanetId id,std::size_t index,const std::vector<ProductionItem>& after) {
    auto& rows=rows_.at(id);rows.origins.erase(rows.origins.begin()+index);rows.items=after;
}
void ProductionQueueIdentity::moved(PlanetId id,std::size_t from,std::size_t to,const std::vector<ProductionItem>& after) {
    auto& rows=rows_.at(id);const auto origin=rows.origins[from];rows.origins.erase(rows.origins.begin()+from);
    rows.origins.insert(rows.origins.begin()+to,origin);rows.items=after;
}
void ProductionQueueIdentity::template_applied(PlanetId id,std::size_t order,std::size_t inserted,const std::vector<ProductionItem>& after) {
    auto& rows=rows_.at(id);std::vector<QueueRowOrigin> origins;
    const auto retained=after.size()-inserted;std::size_t cursor=0;
    for(std::size_t i=0;i<retained;++i) {
        auto found=std::find(rows.items.begin()+cursor,rows.items.end(),after[i]);
        if(found==rows.items.end())origins.push_back({false,order,inserted+i});
        else {const auto index=std::size_t(found-rows.items.begin());origins.push_back(rows.origins[index]);cursor=index+1;}
    }
    // Every template row is newly inserted, including an identical annual rule.
    // Its ordinal must not depend on which earlier rows survived on the host.
    for(std::size_t i=0;i<inserted;++i)origins.push_back({false,order,i});
    rows.items=after;rows.origins=std::move(origins);
}
}
